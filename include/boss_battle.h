#ifndef GUARD_BOSS_BATTLE_H
#define GUARD_BOSS_BATTLE_H

#include "global.h"
#include "pokemon.h"

// Flag stored in gBattleTypeFlags to mark a boss encounter

void CreateScriptedBossMon(u16 species, u8 level, enum Item item);
bool8 IsBossBattleActive(void);

#endif // GUARD_BOSS_BATTLE_H