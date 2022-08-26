#ifndef MARIO_ACTIONS_LADDER
#define MARIO_ACTIONS_LADDER

#include "../include/PR/ultratypes.h"

#include "../include/types.h"

s32 mario_execute_ladder_action(struct MarioState *m);
bool mario_check_viable_ladder_action(struct MarioState *m, uint32_t currentState);

#endif