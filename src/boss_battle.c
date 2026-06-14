#include "global.h"
#include "battle.h"
#include "battle_gfx_sfx_util.h"
#include "berry.h"
#include "caps.h"
#include "data.h"
#include "daycare.h"
#include "decompress.h"
#include "event_data.h"
#include "international_string_util.h"
#include "item.h"
#include "link.h"
#include "link_rfu.h"
#include "main.h"
#include "menu.h"
#include "overworld.h"
#include "ow_synchronize.h"
#include "palette.h"
#include "party_menu.h"
#include "pokedex.h"
#include "pokemon.h"
#include "pokemon_storage_system.h"
#include "random.h"
#include "script.h"
#include "sprite.h"
#include "string_util.h"
#include "tv.h"
#include "wild_encounter.h"
#include "constants/abilities.h"
#include "constants/items.h"
#include "constants/battle_frontier.h"
#include "randomizer.h"
#include "boss_battle.h"



// Mirrors the pattern of sIsScriptedWildDouble in scripted_wild_battle.c
static bool8 sIsBossBattle = FALSE;

// -----------------------------------------------------------------------
// CreateScriptedBossMon
//   Creates a single boss mon in the enemy party, identical in structure
//   to CreateScriptedWildMon, but tags the battle type with BATTLE_TYPE_BOSS
//   so downstream systems (stat boost, catch block) can react.
// -----------------------------------------------------------------------
void CreateScriptedBossMon(u16 species, u8 level, enum Item item)
{
    u8 heldItem[2];

    ZeroEnemyPartyMons();

    u32 personality = GetMonPersonality(
        species,
        GetSynchronizedGender(STATIC_WILDMON_ORIGIN, species),
        GetSynchronizedNature(STATIC_WILDMON_ORIGIN, species),
        RANDOM_UNOWN_LETTER
    );

    CreateMonWithIVs(&gEnemyParty[0], species, level, personality,
                     OTID_STRUCT_PLAYER_ID, USE_RANDOM_IVS);
    GiveMonInitialMoveset(&gEnemyParty[0]);

    if (item)
    {
        heldItem[0] = item;
        heldItem[1] = item >> 8;
        SetMonData(&gEnemyParty[0], MON_DATA_HELD_ITEM, heldItem);
    }

    // Queue DEF +2 and SPDEF +2 via the totem boost system.
    // In a wild battle the opponent is always battler B_POSITION_OPPONENT_LEFT (1).
    // The statChanges array is 0-indexed from ATK (i.e. index = STAT_X - 1).
    gQueuedStatBoosts[B_POSITION_OPPONENT_LEFT].statChanges[STAT_DEF - 1]   = 2;
    gQueuedStatBoosts[B_POSITION_OPPONENT_LEFT].statChanges[STAT_SPDEF - 1] = 2;
    gQueuedStatBoosts[B_POSITION_OPPONENT_LEFT].stats |= (1 << (STAT_DEF - 1));
    gQueuedStatBoosts[B_POSITION_OPPONENT_LEFT].stats |= (1 << (STAT_SPDEF - 1));
    gQueuedStatBoosts[B_POSITION_OPPONENT_LEFT].stats |= 0x80; // triggers "flared to life" anim

    // Tag this battle for catch prevention
    gBattleTypeFlags |= BATTLE_TYPE_BOSS;
    sIsBossBattle = TRUE;
}

// -----------------------------------------------------------------------
// IsBossBattleActive
//   Convenience helper used by the catch-prevention check and the
//   battle-start stat boost. Checks both the flag and the local bool
//   so it is safe to call before gBattleTypeFlags is fully initialized.
// -----------------------------------------------------------------------
bool8 IsBossBattleActive(void)
{
    return sIsBossBattle || (gBattleTypeFlags & BATTLE_TYPE_BOSS);
}