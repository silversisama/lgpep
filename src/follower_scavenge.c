#include "global.h"
#include "event_data.h"
#include "event_object_movement.h"
#include "data.h"
#include "follower_helper.h"
#include "random.h"
#include "script.h"
#include "script_movement.h"
#include "string_util.h"
#include "constants/items.h"
#include "constants/maps.h"
#include "constants/event_objects.h"
#include "constants/species.h"
#include "field_player_avatar.h"
#include "follower_scavenge.h"


//check the include/follower_scavenge.h file as well for more info


// Compile-time safety check: item IDs must fit in 10 bits.
// Theoretically, if we go beyond 1024 items, may need to adjust this
// If this fails, increase SCAVENGE_ITEM_BITS and adjust packing below.
STATIC_ASSERT(ITEMS_COUNT <= 0x3FF, ScavengeItemIdFitsIn10Bits);


// Packed var encoding  (all fits in one u16 / VAR_FOLLOWER_SCAVENGE)
//   bits  [9:0]  = item ID         (10 bits, 0-1023)
//   bits [13:10] = movement index  ( 4 bits, 0-15, ScavengeMovement enum)
//   bits [15:14] = message category( 2 bits, 0-3, ScavengeCategory enum)


//I do not fully understand this but the internet assures me it works
// By doing things this way we use one var for multiple info pieces and save data woo
#define SCAVENGE_ITEM_BITS      10
#define SCAVENGE_MOVEMENT_BITS  4
#define SCAVENGE_CATEGORY_BITS  2

#define SCAVENGE_ITEM_MASK      ((1 << SCAVENGE_ITEM_BITS) - 1)
#define SCAVENGE_MOVEMENT_MASK  ((1 << SCAVENGE_MOVEMENT_BITS) - 1)
#define SCAVENGE_CATEGORY_MASK  ((1 << SCAVENGE_CATEGORY_BITS) - 1)

#define SCAVENGE_MOVEMENT_SHIFT SCAVENGE_ITEM_BITS
#define SCAVENGE_CATEGORY_SHIFT (SCAVENGE_ITEM_BITS + SCAVENGE_MOVEMENT_BITS)

static inline u16 PackScavengeVar(u16 itemId, u8 movement, u8 category)
{
    return itemId
         | ((u16)movement << SCAVENGE_MOVEMENT_SHIFT)
         | ((u16)category << SCAVENGE_CATEGORY_SHIFT);
}

static inline u16 ScavengeVar_ItemId(u16 packed)   { return packed & SCAVENGE_ITEM_MASK; }
static inline u8  ScavengeVar_Movement(u16 packed)  { return (packed >> SCAVENGE_MOVEMENT_SHIFT) & SCAVENGE_MOVEMENT_MASK; }
static inline u8  ScavengeVar_Category(u16 packed)  { return (packed >> SCAVENGE_CATEGORY_SHIFT) & SCAVENGE_CATEGORY_MASK; }


// Message category enum (internal - resolved at find time against party state)

enum ScavengeCategory
{
    SCAVENGE_CATEGORY_DEFAULT    = 0, // weight-based random message pool
    SCAVENGE_CATEGORY_FOUND,          // item->foundMessage
    SCAVENGE_CATEGORY_SELF,           // follower IS in associatedPokemon
    SCAVENGE_CATEGORY_ASSOCIATED,     // a non-follower party member is in associatedPokemon
};


// Assembly movement scripts - defined in data/scripts/follower.inc (:: labels)

extern const u8 FollowerDance[];
extern const u8 FollowerHopFast[];
extern const u8 FollowerSkippingMovement[];
extern const u8 FollowerPokeGround[];
extern const u8 FollowerStartled[];
extern const u8 FollowerNostalgiaMovement[];
extern const u8 FollowerShiverHorizontalMovement[];
// Directional variants - dispatched at runtime by follower position relative to player
extern const u8 FollowerJumpOnPlayerNorth[];
extern const u8 FollowerJumpOnPlayerSouth[];
extern const u8 FollowerJumpOnPlayerEast[];
extern const u8 FollowerJumpOnPlayerWest[];
extern const u8 FollowerCuddlingNorth[];
extern const u8 FollowerCuddlingSouth[];
extern const u8 FollowerCuddlingEast[];
extern const u8 FollowerCuddlingWest[];

// Shorthand for inline NULL-terminated tier arrays inside pool struct definitions.
// Usage: .common = TIER(&sItem_OranBerry, &sItem_Potion)
#define TIER(...) ((const struct ScavengeItem * const []){ __VA_ARGS__, NULL })

// Table for non-directional movements. JUMP_ON and CUDDLE are handled in GetMovementScript().
static const u8 *const sScavengeMovements[] =
{
    [SCAVENGE_MOVEMENT_DEFAULT]     = FollowerHopFast,
    [SCAVENGE_MOVEMENT_DANCE]       = FollowerDance,
    [SCAVENGE_MOVEMENT_HOP_FAST]    = FollowerHopFast,
    [SCAVENGE_MOVEMENT_SKIP]        = FollowerSkippingMovement,
    [SCAVENGE_MOVEMENT_POKE_GROUND] = FollowerPokeGround,
    [SCAVENGE_MOVEMENT_STARTLED]    = FollowerStartled,
    [SCAVENGE_MOVEMENT_NOSTALGIA]   = FollowerNostalgiaMovement,
    [SCAVENGE_MOVEMENT_SHIVER_H]    = FollowerShiverHorizontalMovement,
};


// Discovery prefix - always shown first as a separate message box.
// STR_VAR_1 = follower nickname, STR_VAR_2 = item name (buffered by script).
// Exported so EventScript_FollowerFoundItem can reference it directly.

const u8 gText_ScavengeDiscovery[] = _("Oh? {STR_VAR_1} found\n{STR_VAR_2}!$");


// Default flavor message pools - added a few so you don't see one common message over and over adn over again
// Index 0 = common, 1 = uncommon, 2 = rare.

static const u8 *const sDefaultMessages[3][4] =
{
    // Common
    {
        (const u8[])_("{STR_VAR_1} seems pleased with itself.$"),
        (const u8[])_("Did it find it on the ground?$"),
        (const u8[])_("{STR_VAR_1} has a good nose!$"),
        (const u8[])_("Where did it find this?$"),
    },
    // Uncommon
    {
        (const u8[])_("What a discovery!$"),
        (const u8[])_("{STR_VAR_1} seems proud of itself.$"),
        (const u8[])_("Not something you see every day.$"),
        (const u8[])_("{STR_VAR_1} must have been\nsearching carefully.$"),
    },
    // Rare
    {
        //I like this one so it's here twice
        (const u8[])_("{STR_VAR_1} is practically\nglowing with pride!$"),
        (const u8[])_("{STR_VAR_1} is practically\nglowing with pride!$"),
        (const u8[])_("Good job, {STR_VAR_1}!$"),
        (const u8[])_("{STR_VAR_1} is dancing with pride!$"),
    },
};


// Items shared across pools

static const struct ScavengeItem sItem_Potion    = { .itemId = ITEM_POTION };
static const struct ScavengeItem sItem_Antidote  = { .itemId = ITEM_ANTIDOTE };
static const struct ScavengeItem sItem_OranBerry = { .itemId = ITEM_ORAN_BERRY };
static const struct ScavengeItem sItem_TinyMushroom = { .itemId = ITEM_TINY_MUSHROOM };
static const struct ScavengeItem sItem_PechaBerry   = { .itemId = ITEM_PECHA_BERRY };
static const struct ScavengeItem sItem_CheriBerry   = { .itemId = ITEM_CHERI_BERRY };
static const struct ScavengeItem sItem_SitrusBerry  = { .itemId = ITEM_SITRUS_BERRY };
static const struct ScavengeItem sItem_IapapaBerry  = { .itemId = ITEM_IAPAPA_BERRY };
static const struct ScavengeItem sItem_HeartScale = { .itemId = ITEM_HEART_SCALE };
static const struct ScavengeItem sItem_Pearl      = { .itemId = ITEM_PEARL };
static const struct ScavengeItem sItem_Stardust   = { .itemId = ITEM_STARDUST };
static const struct ScavengeItem sItem_Repel = { .itemId = ITEM_REPEL };


//Special Items
//As you can see, you can add more or less detail for more specialized items
static const struct ScavengeItem sItem_IceStone     = { .itemId = ITEM_ICE_STONE, .movement = SCAVENGE_MOVEMENT_JUMP_ON,
                                                        .foundMessage = (const u8[])_("It's so cold, it stings\nyour hand!") };

static const struct ScavengeItem sItem_DuskStone =
{
    .itemId            = ITEM_DUSK_STONE,
    .emote             = SCAVENGE_EMOTE(FOLLOWER_EMOTION_LOVE),
    .movement          = SCAVENGE_MOVEMENT_CUDDLE,
    .foundMessage      = (const u8[])_("{STR_VAR_1} was looking into\nthe dark depths of the {STR_VAR_2}...$"),
    .associatedPokemon = (const u16[]){ SPECIES_MISDREAVUS, SPECIES_MURKROW, SPECIES_LAMPENT, SPECIES_DOUBLADE, 0xFFFF },
    .selfMessage       = (const u8[])_("Does {STR_VAR_1} want to evolve?$"),
    .associatedMessage = (const u8[])_("Has {STR_VAR_1} been carrying\nit thinking of {STR_VAR_3}?$"),
};
static const struct ScavengeItem sItem_StickyBarb =
{
    .itemId            = ITEM_STICKY_BARB,
    .emote             = SCAVENGE_EMOTE(FOLLOWER_EMOTION_SURPRISE),
    .movement          = SCAVENGE_MOVEMENT_SHIVER_H,
    .foundMessage      = (const u8[])_("{STR_VAR_1} seems grateful you\n took the {STR_VAR_2} away.$"),
};

static const struct ScavengePool sPool_Forest =
{
    .common   = TIER(&sItem_TinyMushroom, &sItem_Antidote, &sItem_PechaBerry, &sItem_CheriBerry, &sItem_OranBerry),
    .uncommon = TIER(&sItem_SitrusBerry, &sItem_IapapaBerry),
    .rare     = TIER(&sItem_StickyBarb, &sItem_DuskStone),
};


// Beach pool

static const struct ScavengePool sPool_Beach =
{
    .common = TIER(&sItem_HeartScale, &sItem_Pearl),
    .rare   = TIER(&sItem_Stardust),
};

// Cave pool
static const struct ScavengeItem sItem_HardStone = { .itemId = ITEM_HARD_STONE };
static const struct ScavengeItem sItem_Iron      = { .itemId = ITEM_IRON };
static const struct ScavengeItem sItem_Revive    = { .itemId = ITEM_REVIVE };

static const struct ScavengePool sPool_Cave =
{
    .common   = TIER(&sItem_HardStone, &sItem_Iron),
    .uncommon = TIER(&sItem_Revive),
    //if no rare tier is defined, it should in theory always roll a common or uncommon
};


// Plains pool


static const struct ScavengePool sPool_Plains =
{
    .common = TIER(&sItem_Potion, &sItem_Antidote, &sItem_Repel, &sItem_OranBerry),
};


// Generic fallback pool

static const struct ScavengePool sPool_Generic =
{
    .common = TIER(&sItem_Potion, &sItem_Antidote, &sItem_OranBerry),
};


// Per-location scavenge table
// One row per map. The sentinel row (0xFF, 0xFF) must be last; it provides
// sPool_Generic as the global fallback. Set pool = NULL to disable a map.

static const struct ScavengeEntry sScavengeTable[] =
{
    // --- Forest / wooded routes ---
    { MAP_GROUP(MAP_ROUTE101), MAP_NUM(MAP_ROUTE101), &sPool_Forest },
    { MAP_GROUP(MAP_ROUTE102), MAP_NUM(MAP_ROUTE102), &sPool_Forest },
    { MAP_GROUP(MAP_ROUTE104), MAP_NUM(MAP_ROUTE104), &sPool_Forest },
    { MAP_GROUP(MAP_ROUTE119), MAP_NUM(MAP_ROUTE119), &sPool_Forest },
    { MAP_GROUP(MAP_ROUTE121), MAP_NUM(MAP_ROUTE121), &sPool_Forest },

    // --- Beach / coastal routes ---
    { MAP_GROUP(MAP_ROUTE103), MAP_NUM(MAP_ROUTE103), &sPool_Beach },
    { MAP_GROUP(MAP_ROUTE105), MAP_NUM(MAP_ROUTE105), &sPool_Beach },
    { MAP_GROUP(MAP_ROUTE106), MAP_NUM(MAP_ROUTE106), &sPool_Beach },
    { MAP_GROUP(MAP_ROUTE107), MAP_NUM(MAP_ROUTE107), &sPool_Beach },
    { MAP_GROUP(MAP_ROUTE108), MAP_NUM(MAP_ROUTE108), &sPool_Beach },
    { MAP_GROUP(MAP_ROUTE109), MAP_NUM(MAP_ROUTE109), &sPool_Beach },

    // --- Cave / underground ---
    { MAP_GROUP(MAP_GRANITE_CAVE_1F),  MAP_NUM(MAP_GRANITE_CAVE_1F),  &sPool_Cave },
    { MAP_GROUP(MAP_GRANITE_CAVE_B1F), MAP_NUM(MAP_GRANITE_CAVE_B1F), &sPool_Cave },
    { MAP_GROUP(MAP_GRANITE_CAVE_B2F), MAP_NUM(MAP_GRANITE_CAVE_B2F), &sPool_Cave },
    { MAP_GROUP(MAP_VICTORY_ROAD_1F),  MAP_NUM(MAP_VICTORY_ROAD_1F),  &sPool_Cave },
    { MAP_GROUP(MAP_VICTORY_ROAD_B1F), MAP_NUM(MAP_VICTORY_ROAD_B1F), &sPool_Cave },
    { MAP_GROUP(MAP_VICTORY_ROAD_B2F), MAP_NUM(MAP_VICTORY_ROAD_B2F), &sPool_Cave },

    // --- Plains / open routes ---
    { MAP_GROUP(MAP_ROUTE110), MAP_NUM(MAP_ROUTE110), &sPool_Plains },
    { MAP_GROUP(MAP_ROUTE111), MAP_NUM(MAP_ROUTE111), &sPool_Plains },
    { MAP_GROUP(MAP_ROUTE113), MAP_NUM(MAP_ROUTE113), &sPool_Plains },
    { MAP_GROUP(MAP_ROUTE117), MAP_NUM(MAP_ROUTE117), &sPool_Plains },

    // Sentinel / fallback
    { 0xFF, 0xFF, &sPool_Generic },
};


// Helpers


static const struct ScavengeEntry *GetEntryForCurrentMap(void)
{
    u8 group = gSaveBlock1Ptr->location.mapGroup;
    u8 num   = gSaveBlock1Ptr->location.mapNum;
    u32 i;
    for (i = 0; sScavengeTable[i].mapGroup != 0xFF; i++)
    {
        if (sScavengeTable[i].mapGroup == group && sScavengeTable[i].mapNum == num)
            return &sScavengeTable[i];
    }
    return &sScavengeTable[i]; // sentinel row = generic fallback
}

// Pick a uniform-random item from a NULL-terminated pointer array. Returns NULL if empty.
static const struct ScavengeItem *PickFromTier(const struct ScavengeItem * const *tier)
{
    u32 count = 0;
    if (tier == NULL)
        return NULL;
    while (tier[count] != NULL)
        count++;
    if (count == 0)
        return NULL;
    return tier[Random() % count];
}

// Select a tier by roll, then pick from it. outTier receives 0/1/2 (common/uncommon/rare).
static const struct ScavengeItem *PickItem(const struct ScavengePool *pool, u8 *outTier)
{
    u8 roll = Random() % 100;
    if (roll < SCAVENGE_COMMON_THRESHOLD)
    {
        *outTier = 0;
        return PickFromTier(pool->common);
    }
    if (roll < SCAVENGE_UNCOMMON_THRESHOLD)
    {
        *outTier = 1;
        return PickFromTier(pool->uncommon);
    }
    *outTier = 2;
    return PickFromTier(pool->rare);
}

static const struct ScavengeItem *SearchTier(const struct ScavengeItem * const *tier, u16 itemId)
{
    u32 i;
    if (tier == NULL)
        return NULL;
    for (i = 0; tier[i] != NULL; i++)
        if (tier[i]->itemId == itemId)
            return tier[i];
    return NULL;
}

// Search the current map's pool for itemId. Returns the item and its tier (0/1/2), or NULL.
static const struct ScavengeItem *FindScavengeItemById(u16 itemId, u8 *outTier)
{
    const struct ScavengeEntry *entry = GetEntryForCurrentMap();
    const struct ScavengePool *pool;
    const struct ScavengeItem *found;

    *outTier = 0;
    if (entry == NULL)
        return NULL;
    pool = entry->pool;
    if (pool == NULL)
        return NULL;

    if ((found = SearchTier(pool->common,   itemId))) { *outTier = 0; return found; }
    if ((found = SearchTier(pool->uncommon, itemId))) { *outTier = 1; return found; }
    if ((found = SearchTier(pool->rare,     itemId))) { *outTier = 2; return found; }
    return NULL;
}

// Returns the appropriate directional movement script based on the follower's
// position relative to the player.
static const u8 *GetDirectionalMovement(u8 movementType)
{
    struct ObjectEvent *followerObj = GetFollowerObject();
    struct ObjectEvent *playerObj;
    s16 dx, dy;

    if (followerObj == NULL)
        return FollowerHopFast;

    playerObj = &gObjectEvents[gPlayerAvatar.objectEventId];
    dx = (s16)followerObj->currentCoords.x - (s16)playerObj->currentCoords.x;
    dy = (s16)followerObj->currentCoords.y - (s16)playerObj->currentCoords.y;

    // Use whichever axis has the larger offset (follower is always one tile away).
    // Animation names describe the direction the follower MOVES (toward the player),
    // which is opposite to the follower's position relative to the player.
    // e.g. follower is NORTH of player → follower must move SOUTH → use South animations.
    if (dy <= -1 && (dy <= dx || dy <= -dx))
    {
        // follower is north of player → move south toward player
        return movementType == SCAVENGE_MOVEMENT_JUMP_ON ? FollowerJumpOnPlayerSouth : FollowerCuddlingSouth;
    }
    else if (dy >= 1 && (dy >= dx || dy >= -dx))
    {
        // follower is south of player → move north toward player
        return movementType == SCAVENGE_MOVEMENT_JUMP_ON ? FollowerJumpOnPlayerNorth : FollowerCuddlingNorth;
    }
    else if (dx >= 1)
    {
        // follower is east of player → move west toward player
        return movementType == SCAVENGE_MOVEMENT_JUMP_ON ? FollowerJumpOnPlayerWest : FollowerCuddlingWest;
    }
    else
    {
        // follower is west of player → move east toward player
        return movementType == SCAVENGE_MOVEMENT_JUMP_ON ? FollowerJumpOnPlayerEast : FollowerCuddlingEast;
    }
}

//pokemon get more excited as they find cooler stuff
static u8 EmoteForTier(u8 tier)
{
    if (tier == 0) return FOLLOWER_EMOTION_CURIOUS;
    if (tier == 1) return FOLLOWER_EMOTION_HAPPY;
    return FOLLOWER_EMOTION_SURPRISE;
}

static u8 UnpackEmote(u8 packed)
{
    return packed - 1; // only called when packed != 0
}

// Returns TRUE if speciesId is in a 0xFFFF-terminated species list.
//used to check if a pokemon is associated.
static bool8 SpeciesInList(u16 speciesId, const u16 *list)
{
    if (list == NULL)
        return FALSE;
    for (; *list != 0xFFFF; list++)
        if (*list == speciesId)
            return TRUE;
    return FALSE;
}

// Determine the message category given the ScavengeItem and current follower/party.
static u8 ResolveCategory(const struct ScavengeItem *item)
{
    u32 i;
    u16 followerSpecies = SPECIES_NONE;
    struct Pokemon *followerMon;

    if (item == NULL || item->associatedPokemon == NULL)
        return item && item->foundMessage ? SCAVENGE_CATEGORY_FOUND : SCAVENGE_CATEGORY_DEFAULT;

    followerMon = GetFirstLiveMon();
    if (followerMon != NULL)
    {
        followerSpecies = GetMonData(followerMon, MON_DATA_SPECIES);
        if (SpeciesInList(followerSpecies, item->associatedPokemon))
            return SCAVENGE_CATEGORY_SELF;
    }

    for (i = 0; i < PARTY_SIZE; i++)
    {
        u16 partySpecies = GetMonData(&gPlayerParty[i], MON_DATA_SPECIES);
        if (partySpecies == SPECIES_NONE || partySpecies == followerSpecies)
            continue;
        if (SpeciesInList(partySpecies, item->associatedPokemon))
            return SCAVENGE_CATEGORY_ASSOCIATED;
    }

    return item->foundMessage ? SCAVENGE_CATEGORY_FOUND : SCAVENGE_CATEGORY_DEFAULT;
}

// Buffer the first matching (non-follower) party member's nickname into STR_VAR_3.
// Called at presentation time so renames between find and pickup are reflected.
static void BufferAssociatedNickname(const struct ScavengeItem *item)
{
    u32 i;
    u16 followerSpecies = SPECIES_NONE;
    struct Pokemon *followerMon = GetFirstLiveMon();

    if (followerMon != NULL)
        followerSpecies = GetMonData(followerMon, MON_DATA_SPECIES);

    for (i = 0; i < PARTY_SIZE; i++)
    {
        u16 partySpecies = GetMonData(&gPlayerParty[i], MON_DATA_SPECIES);
        if (partySpecies == SPECIES_NONE || partySpecies == followerSpecies)
            continue;
        if (SpeciesInList(partySpecies, item->associatedPokemon))
        {
            GetMonData(&gPlayerParty[i], MON_DATA_NICKNAME, gStringVar3);
            return;
        }
    }
}


// Public API


void TryFollowerScavenge(void)
{
    static u16 sStepCounter = 0;
    struct ObjectEvent *followerObj;
    const struct ScavengeEntry *entry;
    const struct ScavengeItem *item;
    u8 category, emote, tier;

    if (!SCAVENGE_ENABLED)
        return;

    followerObj = GetFollowerObject();
    if (followerObj == NULL)
        return;

    if (!TestPlayerAvatarFlags(PLAYER_AVATAR_FLAG_ON_FOOT))
        return;

    sStepCounter++;
    if (sStepCounter < SCAVENGE_STEP_INTERVAL)
        return;
    sStepCounter = 0;

    if (Random() % 100 >= SCAVENGE_FIND_CHANCE)
        return;

    entry = GetEntryForCurrentMap();
    if (entry->pool == NULL)
        return;

    item = PickItem(entry->pool, &tier);
    if (item == NULL)
        return;

    category = ResolveCategory(item);
    emote    = item->emote ? UnpackEmote(item->emote) : EmoteForTier(tier);


    // if you are getting errors here you may have failed to define the flag/var
    VarSet(VAR_FOLLOWER_SCAVENGE, PackScavengeVar(item->itemId, item->movement, category));
    FlagSet(FLAG_FOLLOWER_ITEM);
    FollowerPlayEmote(followerObj, emote);
}

void PrepareFollowerScavengeScript(struct ScriptContext *ctx)
{
    u16 packed   = VarGet(VAR_FOLLOWER_SCAVENGE);
    u16 itemId   = ScavengeVar_ItemId(packed);
    u8  movement = ScavengeVar_Movement(packed);
    u8  category = ScavengeVar_Category(packed);
    u8  tier;

    const struct ScavengeItem *item = FindScavengeItemById(itemId, &tier);
    const u8 *message = NULL;

    if (item != NULL)
    {
        switch (category)
        {
        case SCAVENGE_CATEGORY_SELF:
            message = item->selfMessage ? item->selfMessage : item->foundMessage;
            break;
        case SCAVENGE_CATEGORY_ASSOCIATED:
            BufferAssociatedNickname(item);
            message = item->associatedMessage ? item->associatedMessage : item->foundMessage;
            break;
        case SCAVENGE_CATEGORY_FOUND:
            message = item->foundMessage;
            break;
        case SCAVENGE_CATEGORY_DEFAULT:
        default:
            break;
        }
    }

    if (message == NULL)
        message = sDefaultMessages[tier][Random() % 4];

    // Store the flavor message pointer in ctx->data[0].
    // The script shows gText_ScavengeDiscovery first (fixed ROM pointer),
    // then calls "message 0x0" which reads ctx->data[0] for the flavor box.
    ctx->data[0] = (u32)message;

    // Apply movement to the follower; the script's waitmovement will sync.
    {
        struct ObjectEvent *followerObj = GetFollowerObject();
        const u8 *movementScript;

        if (movement == SCAVENGE_MOVEMENT_JUMP_ON || movement == SCAVENGE_MOVEMENT_CUDDLE)
            movementScript = GetDirectionalMovement(movement);
        else
            movementScript = sScavengeMovements[movement < ARRAY_COUNT(sScavengeMovements) ? movement : SCAVENGE_MOVEMENT_DEFAULT];

        if (followerObj != NULL && followerObj->frozen)
        {
            ClearObjectEventMovement(followerObj, &gSprites[followerObj->spriteId]);
            gSprites[followerObj->spriteId].animCmdIndex = 0;
        }
        ScriptMovement_StartObjectMovementScript(
            OBJ_EVENT_ID_FOLLOWER,
            gSaveBlock1Ptr->location.mapNum,
            gSaveBlock1Ptr->location.mapGroup,
            movementScript);
    }

    // Overwrite var with bare item ID for the script's bufferitemname + giveitem.
    VarSet(VAR_FOLLOWER_SCAVENGE, itemId);
}


// Display helper
// This function returns items found on a given route
// I am using this for a UI feature that displays all the items and is not included in this feature branch
// Most users will never call or need this, so you could i suppose delete it if you are screaming for data
void GetScavengeDisplayItems(
    u16 *commonOut,   u8 *commonCount,
    u16 *uncommonOut, u8 *uncommonCount,
    u16 *rareOut,     u8 *rareCount,
    u8 maxCommon, u8 maxUncommon, u8 maxRare)
{
    const struct ScavengeEntry *entry = GetEntryForCurrentMap();
    const struct ScavengePool *pool;
    u32 i;

    *commonCount = *uncommonCount = *rareCount = 0;

    if (entry == NULL)
        return;
    pool = entry->pool;
    if (pool == NULL)
        return;

    if (pool->common != NULL)
        for (i = 0; pool->common[i] != NULL && *commonCount < maxCommon; i++)
            commonOut[(*commonCount)++] = pool->common[i]->itemId;

    if (pool->uncommon != NULL)
        for (i = 0; pool->uncommon[i] != NULL && *uncommonCount < maxUncommon; i++)
            uncommonOut[(*uncommonCount)++] = pool->uncommon[i]->itemId;

    if (pool->rare != NULL)
        for (i = 0; pool->rare[i] != NULL && *rareCount < maxRare; i++)
            rareOut[(*rareCount)++] = pool->rare[i]->itemId;
}
