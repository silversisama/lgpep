#ifndef GUARD_CONFIG_FOLLOWER_SCAVENGE_H
#define GUARD_CONFIG_FOLLOWER_SCAVENGE_H

// Set FALSE to disable the system
// If SCAVENGE_ENABLED is nonzero, FLAG_FOLLOWER_HAS_ITEM and VAR_FOLLOWER_SCAVENGE must point to unused flag/var, respectively.
// In theory if you wanted to gate this by area like for regions, or to make this a quest reward, could point this to flag
#define SCAVENGE_ENABLED         TRUE

#define SCAVENGE_STEP_INTERVAL       256    // steps between rolls- more steps -> less frequent rolls
#define SCAVENGE_FIND_CHANCE          25    // percent chance per roll (0-100) to discover an item.

// Tier selection: roll 0-99. Below COMMON = common, below UNCOMMON = uncommon, else rare.
#define SCAVENGE_COMMON_THRESHOLD     60   // 60% common
#define SCAVENGE_UNCOMMON_THRESHOLD   90    // 90-60 =30% uncommon, 10% rare

// Point this flag and var at unused ones in your hack. You should probably change these.
#define FLAG_FOLLOWER_HAS_ITEM   FLAG_FOLLOWER_ITEM   // used to track if pokemon is holding an item
#define VAR_FOLLOWER_SCAVENGE    VAR_FOLLOWER_IS_SCAVENGE    // packed: itemId[9:0] | movement[12:10] | category[14:13]

#endif // GUARD_CONFIG_FOLLOWER_SCAVENGE_H
