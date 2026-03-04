#include "global.h"
#include "nuzlocke.h"
#include "event_data.h"
#include "pokemon.h"
#include "constants/flags.h"
#include "constants/vars.h"
#include "constants/region_map_sections.h"
#include "pokemon_storage_system.h"
#include "sprite.h"
#include "battle.h"
#include "battle_interface.h"
#include "overworld.h"
#include "config/battle.h"
#include "save.h"

// Check if Nuzlocke mode is active
bool8 IsNuzlockeActive(void)
{
    return FlagGet(FLAG_NUZLOCKE);
}

// Location conversion for Nuzlocke tracking
static u8 GetNuzlockeLocationId(u8 currLocation)
{
    switch (currLocation)
    {
        
    // Cities and towns that have encounters are treated as separate areas
   case MAPSEC_PALLET_TOWN:
   case MAPSEC_VIRIDIAN_CITY:
   case MAPSEC_PEWTER_CITY:
   case MAPSEC_CERULEAN_CITY:
   case MAPSEC_LAVENDER_TOWN:
   case MAPSEC_VERMILION_CITY:
   case MAPSEC_CELADON_CITY:
   case MAPSEC_FUCHSIA_CITY:
   case MAPSEC_CINNABAR_ISLAND:
   case MAPSEC_INDIGO_PLATEAU:
   case MAPSEC_SAFFRON_CITY:
   case MAPSEC_ROUTE_4_POKECENTER:
   case MAPSEC_ROUTE_10_POKECENTER:
        return currLocation;
        
    // Routes are separate areas
   case MAPSEC_ROUTE_1:
   case MAPSEC_ROUTE_2:
   case MAPSEC_ROUTE_3:
   case MAPSEC_ROUTE_4:
   case MAPSEC_ROUTE_5:
   case MAPSEC_ROUTE_6:
   case MAPSEC_ROUTE_7:
   case MAPSEC_ROUTE_8:
   case MAPSEC_ROUTE_9:
   case MAPSEC_ROUTE_10:
   case MAPSEC_ROUTE_11:
   case MAPSEC_ROUTE_12:
   case MAPSEC_ROUTE_13:
   case MAPSEC_ROUTE_14:
   case MAPSEC_ROUTE_15:
   case MAPSEC_ROUTE_16:
   case MAPSEC_ROUTE_17:
   case MAPSEC_ROUTE_18:
   case MAPSEC_ROUTE_19:
   case MAPSEC_ROUTE_20:
   case MAPSEC_ROUTE_21:
   case MAPSEC_ROUTE_22:
   case MAPSEC_ROUTE_23:
   case MAPSEC_ROUTE_24:
   case MAPSEC_ROUTE_25:
        return currLocation;
        
    // Caves and special areas
   case MAPSEC_VIRIDIAN_FOREST:
   case MAPSEC_MT_MOON:
   case MAPSEC_S_S_ANNE:
   case MAPSEC_UNDERGROUND_PATH:
   case MAPSEC_UNDERGROUND_PATH_2:
   case MAPSEC_DIGLETTS_CAVE:
   case MAPSEC_KANTO_VICTORY_ROAD:
   case MAPSEC_ROCKET_HIDEOUT:
   case MAPSEC_SILPH_CO:
   case MAPSEC_POKEMON_MANSION:
   case MAPSEC_KANTO_SAFARI_ZONE:
   case MAPSEC_POKEMON_LEAGUE:
   case MAPSEC_ROCK_TUNNEL:
   case MAPSEC_SEAFOAM_ISLANDS:
   case MAPSEC_POKEMON_TOWER:
   case MAPSEC_CERULEAN_CAVE:
   case MAPSEC_POWER_PLANT:
   case MAPSEC_ONE_ISLAND:
   case MAPSEC_TWO_ISLAND:
   case MAPSEC_THREE_ISLAND:
   case MAPSEC_FOUR_ISLAND:
   case MAPSEC_FIVE_ISLAND:
   case MAPSEC_SEVEN_ISLAND:
   case MAPSEC_SIX_ISLAND:
   case MAPSEC_KINDLE_ROAD:
   case MAPSEC_TREASURE_BEACH:
   case MAPSEC_CAPE_BRINK:
   case MAPSEC_BOND_BRIDGE:
   case MAPSEC_THREE_ISLE_PORT:
   case MAPSEC_RESORT_GORGEOUS:
   case MAPSEC_WATER_LABYRINTH:
   case MAPSEC_FIVE_ISLE_MEADOW:
   case MAPSEC_MEMORIAL_PILLAR:
   case MAPSEC_OUTCAST_ISLAND:
   case MAPSEC_GREEN_PATH:
   case MAPSEC_WATER_PATH:
   case MAPSEC_RUIN_VALLEY:
   case MAPSEC_TRAINER_TOWER:
   case MAPSEC_CANYON_ENTRANCE:
   case MAPSEC_SEVAULT_CANYON:
   case MAPSEC_TANOBY_RUINS:
   case MAPSEC_SEVII_ISLE_22:
   case MAPSEC_SEVII_ISLE_23:
   case MAPSEC_SEVII_ISLE_24:
   case MAPSEC_NAVEL_ROCK_FRLG:
   case MAPSEC_MT_EMBER:
   case MAPSEC_BERRY_FOREST:
   case MAPSEC_ICEFALL_CAVE:
   case MAPSEC_ROCKET_WAREHOUSE:
   case MAPSEC_TRAINER_TOWER_2:
   case MAPSEC_DOTTED_HOLE:
   case MAPSEC_LOST_CAVE:
   case MAPSEC_PATTERN_BUSH:
   case MAPSEC_ALTERING_CAVE_FRLG:
   case MAPSEC_TANOBY_CHAMBERS:
   case MAPSEC_THREE_ISLE_PATH:
   case MAPSEC_TANOBY_KEY:
   case MAPSEC_BIRTH_ISLAND_FRLG:
   case MAPSEC_MONEAN_CHAMBER:
   case MAPSEC_LIPTOO_CHAMBER:
   case MAPSEC_WEEPTH_CHAMBER:
   case MAPSEC_DILFORD_CHAMBER:
   case MAPSEC_SCUFIB_CHAMBER:
   case MAPSEC_RIXY_CHAMBER:
   case MAPSEC_VIAPOIS_CHAMBER:
   case MAPSEC_EMBER_SPA:
        return currLocation;
        
    // Default to the location itself if not handled above
    default:
        return currLocation;
    }
}

// Official Nuzlocke location tracking functions - much more robust than our map-based approach
bool8 HasWildPokemonBeenSeenInLocation(u8 location, bool8 setEncounteredIfFirst)
{
    u8 varToCheck, bitToCheck;
    u16 varValue;
    const u16 pokemonSeenVars[] = {
        VAR_NUZLOCKE_ENCOUNTERS_1,
        VAR_NUZLOCKE_ENCOUNTERS_2,
        VAR_NUZLOCKE_ENCOUNTERS_3,
        VAR_NUZLOCKE_ENCOUNTERS_4,
    };
    
    location = GetNuzlockeLocationId(location);
    
    if (!FlagGet(FLAG_NUZLOCKE) || !FlagGet(FLAG_SYS_POKEDEX_GET))
    {
        // Clear all encounter tracking if Nuzlocke not active
        VarSet(VAR_NUZLOCKE_ENCOUNTERS_1, 0);
        VarSet(VAR_NUZLOCKE_ENCOUNTERS_2, 0);
        VarSet(VAR_NUZLOCKE_ENCOUNTERS_3, 0);
        VarSet(VAR_NUZLOCKE_ENCOUNTERS_4, 0);
        VarSet(VAR_NUZLOCKE_CATCHES_1, 0);
        VarSet(VAR_NUZLOCKE_CATCHES_2, 0);
        VarSet(VAR_NUZLOCKE_CATCHES_3, 0);
        VarSet(VAR_NUZLOCKE_CATCHES_4, 0);
        return 0;
    }
    
    // Map location to variable index and bit position
    varToCheck = location / 16;  // Each variable stores 16 locations (16 bits)
    bitToCheck = location % 16;
    
    if (varToCheck >= 4)  // Safety check - we only have 4 variables
        return 0;
    
    varValue = VarGet(pokemonSeenVars[varToCheck]);
    
    if ((varValue & (1 << bitToCheck)) != 0)
    {
        // Already encountered in this location
        return 1;
    }
    else if (setEncounteredIfFirst)
    {
        // First encounter - mark it
        VarSet(pokemonSeenVars[varToCheck], varValue | (1 << bitToCheck));
    }
    
    return 0;
}

bool8 HasWildPokemonBeenCaughtInLocation(u8 location, bool8 setCaughtIfCaught)
{
    u8 varToCheck, bitToCheck;
    u16 varValue;
    const u16 pokemonCaughtVars[] = {
        VAR_NUZLOCKE_CATCHES_1,
        VAR_NUZLOCKE_CATCHES_2,
        VAR_NUZLOCKE_CATCHES_3,
        VAR_NUZLOCKE_CATCHES_4,
    };
    
    location = GetNuzlockeLocationId(location);
    
    if (!FlagGet(FLAG_NUZLOCKE) || !FlagGet(FLAG_SYS_POKEDEX_GET))
        return 0;
    
    // Map location to variable index and bit position
    varToCheck = location / 16;  // Each variable stores 16 locations (16 bits)
    bitToCheck = location % 16;
    
    if (varToCheck >= 4)  // Safety check - we only have 4 variables
        return 0;
    
    varValue = VarGet(pokemonCaughtVars[varToCheck]);
    
    if ((varValue & (1 << bitToCheck)) != 0)
    {
        // Already caught in this location
        return 1;
    }
    else if (setCaughtIfCaught)
    {
        // Mark as caught
        VarSet(pokemonCaughtVars[varToCheck], varValue | (1 << bitToCheck));
    }
    
    return 0;
}

// Legacy compatibility functions (internal use and wild_encounter.c)
static bool8 HasEncounteredInArea(u16 mapGroup, u16 mapNum)
{
    // For legacy compatibility, use current location
    if (!IsNuzlockeActive())
        return FALSE;
    
    u8 currentLocation = GetCurrentRegionMapSectionId();
    return HasWildPokemonBeenSeenInLocation(currentLocation, FALSE);
}

bool8 IsFirstEncounterInArea(u16 mapGroup, u16 mapNum)
{
    return IsNuzlockeActive() && !HasEncounteredInArea(mapGroup, mapNum);
}

// Dead Pokemon functions
bool8 IsMonDead(struct Pokemon *mon)
{
    if (!IsNuzlockeActive())
        return FALSE;
    
    return GetMonData(mon, MON_DATA_IS_DEAD);
}

bool8 IsBoxMonDead(struct BoxPokemon *boxMon)
{
    if (!IsNuzlockeActive())
        return FALSE;
    
    return GetBoxMonData(boxMon, MON_DATA_IS_DEAD);
}

void SetMonDead(struct Pokemon *mon, bool8 isDead)
{
    if (!IsNuzlockeActive())
        return;
    
    u32 deadFlag = isDead ? 1 : 0;
    SetMonData(mon, MON_DATA_IS_DEAD, &deadFlag);
}

void SetBoxMonDead(struct BoxPokemon *boxMon, bool8 isDead)
{
    if (!IsNuzlockeActive())
        return;
    
    u32 deadFlag = isDead ? 1 : 0;
    SetBoxMonData(boxMon, MON_DATA_IS_DEAD, &deadFlag);
}

// Species checking for duplicate clause
bool8 PlayerOwnsSpecies(u16 species)
{
    int i, j;
    
    if (!IsNuzlockeActive())
        return FALSE;
    
    // Check party
    for (i = 0; i < PARTY_SIZE; i++)
    {
        u16 partySpecies = GetMonData(&gPlayerParty[i], MON_DATA_SPECIES);
        if (partySpecies != SPECIES_NONE && partySpecies == species)
            return TRUE;
    }
    
    // Check PC boxes
    for (i = 0; i < TOTAL_BOXES_COUNT; i++)
    {
        for (j = 0; j < IN_BOX_COUNT; j++)
        {
            struct BoxPokemon *boxMon = GetBoxedMonPtr(i, j);
            u16 boxSpecies = GetBoxMonData(boxMon, MON_DATA_SPECIES);
            
            // Early exit if empty slot
            if (boxSpecies == SPECIES_NONE)
                continue;
            
            if (boxSpecies == species)
                return TRUE;
        }
    }
    
    return FALSE;
}

// Handle Pokemon fainting (mark as dead in Nuzlocke mode)
void NuzlockeHandleFaint(struct Pokemon *mon)
{
    if (!IsNuzlockeActive())
        return;
    
    // Don't mark as dead until player receives Pokédex
    if (!FlagGet(FLAG_SYS_POKEDEX_GET))
        return;
    
    // Check if the Pokemon's HP is 0 (just fainted)
    if (GetMonData(mon, MON_DATA_HP) == 0)
    {
        SetMonDead(mon, TRUE);
    }
}

void NuzlockeHandleWhiteout(void)
{
    if (!IsNuzlockeActive())
        return;
    
    // Don't mark as dead until player receives Pokédex
    if (!FlagGet(FLAG_SYS_POKEDEX_GET))
        return;
    
    int i;
    
    // Mark all party Pokemon as dead
    for (i = 0; i < PARTY_SIZE; i++)
    {
        struct Pokemon *mon = &gPlayerParty[i];
        if (GetMonData(mon, MON_DATA_SPECIES) != SPECIES_NONE && 
            !GetMonData(mon, MON_DATA_SANITY_IS_EGG))
        {
            SetMonDead(mon, TRUE);
        }
    }
}

bool8 NuzlockeCanCatchPokemon(u16 species, u32 personality, u32 otId)
{
    u8 currentLocation;
    
    if (!IsNuzlockeActive())
    {
        return TRUE; // Not in Nuzlocke mode - always allow catching
    }
    
    currentLocation = GetCurrentRegionMapSectionId();
    
    // Shiny clause: always allow shiny Pokemon regardless of other rules
    u32 shinyValue = ((personality >> 16) ^ (personality & 0xFFFF)) ^ ((otId >> 16) ^ (otId & 0xFFFF));
    bool8 isShiny = (shinyValue < 8);
    
    if (isShiny)
    {
        return TRUE;
    }
    
    // Check if we've already had our "real" first encounter in this location
    // (an encounter of a species we don't own, which counts as the area's encounter)
    if (HasWildPokemonBeenSeenInLocation(currentLocation, FALSE))
    {
        return FALSE;
    }
    
    // This is a potential first encounter - check duplicate clause
    if (PlayerOwnsSpecies(species))
    {
        return FALSE; // Don't catch, but this encounter doesn't count - keep hunting
    }
    
    // We don't own this species - this is our "real" first encounter for this area
    return TRUE;
}

void NuzlockeOnBattleEnd(void)
{
    // Mark the current location as encountered when certain wild battles end
    if (IsNuzlockeActive() && !(gBattleTypeFlags & BATTLE_TYPE_TRAINER))
    {
        u8 currentLocation = GetCurrentRegionMapSectionId();
        
        // Check if this area has already been marked as encountered
        bool8 alreadyEncountered = HasWildPokemonBeenSeenInLocation(currentLocation, FALSE);
        
        if (!alreadyEncountered)
        {
            // Get wild Pokemon info
            u16 wildSpecies = GetMonData(&gEnemyParty[0], MON_DATA_SPECIES);
            u32 wildPersonality = GetMonData(&gEnemyParty[0], MON_DATA_PERSONALITY);
            u32 wildOtId = GetMonData(&gEnemyParty[0], MON_DATA_OT_ID);
            
            // Check if it's a shiny - shiny clause means it doesn't consume the encounter
            u32 shinyValue = ((wildPersonality >> 16) ^ (wildPersonality & 0xFFFF)) ^ ((wildOtId >> 16) ^ (wildOtId & 0xFFFF));
            bool8 isShiny = (shinyValue < 8);
            
            if (isShiny)
            {
                // Shiny clause: don't mark area as used, even if caught/defeated/ran
                return;
            }
            
            // Check if it's a duplicate species - duplicate clause means can keep trying
            if (PlayerOwnsSpecies(wildSpecies))
            {
                // Duplicate clause: don't mark area as used, player can keep trying
                return;
            }
            
            // This was a catchable (non-duplicate, non-shiny) encounter
            // Mark the area as used regardless of outcome (caught, defeated, ran, teleported)
            HasWildPokemonBeenSeenInLocation(currentLocation, TRUE);
        }
    }
}

u8 GetNuzlockeEncounterStatus(u16 species, u32 personality, u32 otId)
{
    u8 currentLocation;
    
    if (!IsNuzlockeActive())
    {
        return NUZLOCKE_ENCOUNTER_NORMAL;
    }
    
    currentLocation = GetCurrentRegionMapSectionId();
    
    // Check shiny clause first (highest priority)
    u32 shinyValue = ((personality >> 16) ^ (personality & 0xFFFF)) ^ ((otId >> 16) ^ (otId & 0xFFFF));
    bool8 isShiny = (shinyValue < 8);
    
    if (isShiny)
    {
        return NUZLOCKE_ENCOUNTER_SHINY;
    }
    
    // Check if location already used
    if (HasWildPokemonBeenSeenInLocation(currentLocation, FALSE))
    {
        return NUZLOCKE_ENCOUNTER_NORMAL;
    }
    
    // Check duplicate clause
    if (PlayerOwnsSpecies(species))
    {
        return NUZLOCKE_ENCOUNTER_DUPLICATE;
    }
    
    // New species, first encounter - catchable!
    return NUZLOCKE_ENCOUNTER_CATCHABLE;
}


// Silent save for Nuzlocke mode (no confirmation prompt)
// Uses SAVE_LINK for faster saving (skips PC storage)
void NuzlockeSilentSave(void)
{
    TrySavingData(SAVE_LINK);
}

