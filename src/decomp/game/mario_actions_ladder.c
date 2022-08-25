#include <math.h>

#include "../include/PR/ultratypes.h"
#include "../shim.h"

#include "../include/sm64.h"
#include "mario.h"
#include "../audio/external.h"
#include "../engine/math_util.h"
#include "../engine/surface_collision.h"
#include "mario_step.h"
#include "area.h"
#include "interaction.h"
#include "mario_actions_ladder.h"
//#include "memory.h"
//#include "behavior_data.h"
//#include "thread6.h"
#include "../include/mario_animation_ids.h"
#include "../include/object_fields.h"
#include "../include/mario_geo_switch_case_ids.h"

#define LADDER_MOVEMENT_VELOCITY 4.0f
#define LADDER_SIDE_MOVEMENT_LIMIT 30.0f
#define LADDER_SIDE_MOVEMENT_ANIM_SPEED 0x55000
#define LADDER_IDLE_ANIM_SPEED 0x10000


bool testValidWall(struct MarioState *m, struct Surface *wall)
{
    if(wall==NULL || wall->normal.y!=0.0f)
    {
        return false;
    }

    if((m->climbDirection[0][0] && wall->normal.x>0 && wall->normal.z==0.0f) //-X wall
    || (m->climbDirection[0][1] && wall->normal.x<0 && wall->normal.z==0.0f) //+X wall
    || (m->climbDirection[1][0] && wall->normal.z>0 && wall->normal.x==0.0f) //-Z wall
    || (m->climbDirection[1][1] && wall->normal.z<0 && wall->normal.x==0.0f) //+Z wall
    )
    {
        s16 wallAngle = atan2s(wall->normal.z, wall->normal.x)+0x8000;
        if(abs(m->faceAngle[1]-wallAngle)>0x2800)
        { 
            return false;
        }
        return true;
    }

    return false;
}

bool mario_check_viable_ladder_action(struct MarioState *m)
{
    struct Surface* wall = m->wall;
    if(wall==NULL)
    {
        Vec3f nextPos;

        nextPos[0] = m->pos[0] + sins(m->faceAngle[1])*60.0f;
        nextPos[2] = m->pos[2] + coss(m->faceAngle[1])*60.0f;
        nextPos[1] = m->pos[1];

        printf("cos: %.3f, sin: %.3f\n", coss(m->faceAngle[1]), sins(m->faceAngle[1]));

        printf("current: %.0f, after: %.0f\n", m->pos[2], nextPos[2]);
        wall = resolve_and_return_wall_collisions(nextPos, 65.0f, 80.0f);
    }

    // if there's no wall or if the wall is not 100% vertical then there's no ladder
    if(wall!=NULL && testValidWall(m, wall))
    {
         m->wall = wall;
        return true;
    }

    return false;
}



struct Surface *getViableLadderNextWall(struct MarioState *m, Vec3f nextPos)
{
    struct Surface* wall = NULL;
    struct WallCollisionData collisionData;

    resolve_and_return_multiple_wall_collisions(&collisionData, nextPos, 1.0f, 10.0f);

    for(int i=0; i<collisionData.numWalls; i++)
    {
        if(testValidWall(m, collisionData.walls[i]))
        {
            return collisionData.walls[i];
        }
    }    
}

bool stub(struct MarioState *m)
{
    return FALSE;
}

s32 let_go_of_ladder(struct MarioState *m) {
    f32 floorHeight;
    struct Surface *floor;

    m->vel[1] = 0.0f;
    m->forwardVel = -2.0f; // previously -8.0f
    m->pos[0] -= 5.0f * sins(m->faceAngle[1]);
    m->pos[2] -= 5.0f * coss(m->faceAngle[1]);

    floorHeight = find_floor(m->pos[0], m->pos[1], m->pos[2], &floor);
    if (floorHeight < m->pos[1] - 100.0f) {
        m->pos[1] -= 100.0f;
    } else {
        m->pos[1] = floorHeight;
    }

    return set_mario_action(m, ACT_SOFT_BONK, 0);
}

s32 act_ladder_start_grab(struct MarioState *m)
{
    m->pos[1] +=120;

    if(m->wall->normal.x != 0.0f)
    {
        m->pos[0] = m->wall->vertex1[0];
    }
    else
    {
        m->pos[2] = m->wall->vertex1[2];
    }
    
    s16 wallAngle = atan2s(m->wall->normal.z, m->wall->normal.x)+0x8000;
    m->faceAngle[1] = wallAngle;

    vec3f_copy(m->marioObj->header.gfx.pos, m->pos);
    vec3s_set(m->marioObj->header.gfx.angle, 0, m->faceAngle[1], 0);
    
    set_mario_action(m, ACT_LADDER_IDLE, 0);
    return FALSE;
}

s32 act_ladder_moving_vertical(struct MarioState *m)
{
    if(m->wall == NULL || m->input & INPUT_B_PRESSED)
    {
        let_go_of_ladder(m);
        return FALSE;
    }

    float floorHeight = find_floor_height(m->pos[0], m->pos[1], m->pos[2]);

    Vec3f nextPos;
    vec3f_copy(&nextPos, &(m->pos));

    struct Surface *newWall=NULL;

    for(int i=LADDER_MOVEMENT_VELOCITY; i>0; i--)
    {
        if (m->input & INPUT_NONZERO_ANALOG)
        {
            //printf("yaw %s%x\n", m->rawYaw<0 ? "-" : "", m->rawYaw<0 ? -(unsigned)m->rawYaw :m->rawYaw);
            if(m->rawYaw >= -0x3000 && m->rawYaw <= 0x3000)
            {   
                // moving up
                nextPos[1]+=i;
            }
            else if(m->rawYaw <= -0x5000 || m->rawYaw >= 0x5000)
            {
                // moving down
                nextPos[1]-=i;

                if( nextPos[1] > floorHeight && nextPos[1]-120<floorHeight)
                {
                    set_mario_action(m, ACT_LADDER_IDLE, 0);
                    return FALSE;
                }
            }
            else
            {
                set_mario_action(m, ACT_LADDER_IDLE, 0);
                return FALSE;
            }
        }
        else
        {
            return set_mario_action(m, ACT_LADDER_IDLE, 0);
        }
        newWall = getViableLadderNextWall(m, nextPos);
        if(newWall!=NULL) break;
    }

    if(newWall==NULL)
    {
        set_mario_action(m, ACT_LADDER_IDLE, 0);
        return FALSE;
    }

    m->wall = newWall;

    vec3f_copy(&(m->pos), &nextPos);
    vec3f_copy(m->marioObj->header.gfx.pos, m->pos);
    vec3s_set(m->marioObj->header.gfx.angle, 0, m->faceAngle[1], 0);
    set_mario_anim_with_accel(m, MARIO_ANIM_IDLE_ON_LEDGE, LADDER_SIDE_MOVEMENT_ANIM_SPEED);

    return FALSE;
}

s32 act_ladder_moving_horizontal(struct MarioState *m)
{
    if(m->wall == NULL || m->input & INPUT_B_PRESSED)
    {
        let_go_of_ladder(m);
        return FALSE;
    }

    Vec3f nextPos;
    Vec3f exageratedNextPos;
    vec3f_copy(&nextPos, &(m->pos));
    vec3f_copy(&exageratedNextPos, &(m->pos));

    
    if (m->input & INPUT_NONZERO_ANALOG)
    {
        if( m->rawYaw <= -0x3000 && m->rawYaw >= -0x5000 ) 
        {
            // moving left!
            nextPos[0] += coss(m->faceAngle[1]) * -(LADDER_MOVEMENT_VELOCITY);
            nextPos[2] += sins(m->faceAngle[1]) * (LADDER_MOVEMENT_VELOCITY);       
            exageratedNextPos[0] += coss(m->faceAngle[1]) * -(LADDER_MOVEMENT_VELOCITY+LADDER_SIDE_MOVEMENT_LIMIT);
            exageratedNextPos[2] += sins(m->faceAngle[1]) * (LADDER_MOVEMENT_VELOCITY+LADDER_SIDE_MOVEMENT_LIMIT); 
        }
        else if(m->rawYaw <= 0x5000 && m->rawYaw >= 0x3000)
        {
            // moving right!
            nextPos[0] += coss(m->faceAngle[1]) * (LADDER_MOVEMENT_VELOCITY);
            nextPos[2] += sins(m->faceAngle[1]) * -(LADDER_MOVEMENT_VELOCITY);
            exageratedNextPos[0] += coss(m->faceAngle[1]) * (LADDER_MOVEMENT_VELOCITY+LADDER_SIDE_MOVEMENT_LIMIT);
            exageratedNextPos[2] += sins(m->faceAngle[1]) * -(LADDER_MOVEMENT_VELOCITY+LADDER_SIDE_MOVEMENT_LIMIT);
        }
        else
        {
            set_mario_action(m, ACT_LADDER_IDLE, 0);
            return FALSE;
        }
    }
    else
    {
        return set_mario_action(m, ACT_LADDER_IDLE, 0);
    }    
    struct Surface *newWall = getViableLadderNextWall(m, exageratedNextPos);

    if(newWall==NULL)
    {
        set_mario_action(m, ACT_LADDER_IDLE, 0);
        return FALSE;
    }

    m->wall = newWall;

    vec3f_copy(&(m->pos), &nextPos);

    vec3f_copy(m->marioObj->header.gfx.pos, m->pos);
    vec3s_set(m->marioObj->header.gfx.angle, 0, m->faceAngle[1], 0);
    set_mario_anim_with_accel(m, MARIO_ANIM_IDLE_ON_LEDGE, LADDER_SIDE_MOVEMENT_ANIM_SPEED);

    return FALSE;

}

s32 act_ladder_idle(struct MarioState *m)
{
    //printf("########## Grabbing idle! ###########\n");
    if(m->wall == NULL || m->input & INPUT_B_PRESSED)
    {
        let_go_of_ladder(m);
        return FALSE;
    }

    if (m->input & INPUT_NONZERO_ANALOG)
    {
        if((m->rawYaw <= -0x3000 && m->rawYaw >= -0x5000) || (m->rawYaw <= 0x5000 && m->rawYaw >= 0x3000))
        {
            return set_mario_action(m, ACT_LADDER_MOVING_HORIZONTAL, 0);
        }
        else
        {
            return set_mario_action(m, ACT_LADDER_MOVING_VERTICAL, 0);
        }
    }

    set_mario_anim_with_accel(m, MARIO_ANIM_IDLE_ON_LEDGE, LADDER_IDLE_ANIM_SPEED);
    return FALSE;
}


s32 check_common_ladder_cancels(struct MarioState *m) {
    if (m->pos[1] < m->waterLevel - 100) {
        return set_water_plunge_action(m);
    }

    return FALSE;
}

s32 mario_execute_ladder_action(struct MarioState *m) {
    s32 cancel;

    if (check_common_ladder_cancels(m)) {
        return TRUE;
    }

    /* clang-format off */
    switch (m->action) {
        case ACT_LADDER_START_GRAB:        cancel = act_ladder_start_grab(m);        break;
        case ACT_LADDER_IDLE:              cancel = act_ladder_idle(m);              break;
        case ACT_LADDER_MOVING_VERTICAL:   cancel = act_ladder_moving_vertical(m);   break;
        case ACT_LADDER_MOVING_HORIZONTAL: cancel = act_ladder_moving_horizontal(m); break;
    }
    /* clang-format on */

    if (!cancel && (m->input & INPUT_IN_WATER)) {
        m->particleFlags |= PARTICLE_WAVE_TRAIL;
        m->particleFlags &= ~PARTICLE_DUST;
    }

    return cancel;
}