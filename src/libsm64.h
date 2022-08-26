#ifndef LIB_SM64_H
#define LIB_SM64_H
#define _XOPEN_SOURCE 500

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "decomp/include/external_types.h"

#ifdef _WIN32
    #ifdef SM64_LIB_EXPORT
        #define SM64_LIB_FN __declspec(dllexport)
    #else
        #define SM64_LIB_FN __declspec(dllimport)
    #endif
#else
    #define SM64_LIB_FN
#endif

struct SM64MarioInputs
{
    float camLookX, camLookZ;
    float stickX, stickY;
    uint8_t buttonA, buttonB, buttonZ;
};

struct SM64MarioState
{
    float position[3];
    float velocity[3];
	float angleVel[3];
    float faceAngle;
    float pitchAngle;
    int16_t health;
	uint32_t action;
	uint32_t flags;
	uint32_t particleFlags;
	int16_t invincTimer;
	int16_t burnTimer;
	uint8_t fallDamage;
};

struct SM64MarioGeometryBuffers
{
    float *position;
    float *normal;
    float *color;
    float *uv;
    uint16_t numTrianglesUsed;
};

struct SM64MarioColorGroup
{
    uint8_t shade[3];
    uint8_t color[3];
};

struct SM64MarioModelColors
{
	struct SM64MarioColorGroup blue;
	struct SM64MarioColorGroup red;
	struct SM64MarioColorGroup white;
	struct SM64MarioColorGroup brown1;
	struct SM64MarioColorGroup beige;
	struct SM64MarioColorGroup brown2;
};

struct LibSM64Animation
{
	int16_t flags;
	int16_t animYTransDivisor;
	int16_t startFrame;
	int16_t loopStart;
	int16_t loopEnd;
	int16_t unusedBoneCount;
	int16_t* values;
	uint16_t* index;
	uint32_t length;
};

struct SM64AnimInfo
{
	int16_t animID;
	int16_t animYTrans;
	struct LibSM64Animation *curAnim;
	int16_t animFrame;
	uint16_t animTimer;
	int32_t animFrameAccelAssist;
	int32_t animAccel;
};

typedef void (*SM64DebugPrintFunctionPtr)( const char * );

enum
{
    SM64_TEXTURE_WIDTH = 64 * 11,
    SM64_TEXTURE_HEIGHT = 64,
    SM64_GEO_MAX_TRIANGLES = 1024,
};

extern SM64_LIB_FN void sm64_global_init( uint8_t *rom, uint8_t *outTexture, SM64DebugPrintFunctionPtr debugPrintFunction );
extern SM64_LIB_FN void sm64_global_terminate( void );

extern SM64_LIB_FN int32_t sm64_mario_create( float x, float y, float z, int16_t rx, int16_t ry, int16_t rz, uint8_t fake, int *loadedRooms, int loadedCount);
extern SM64_LIB_FN void sm64_mario_tick(int32_t marioId, const struct SM64MarioInputs *inputs, struct SM64MarioState *outState, struct SM64MarioGeometryBuffers *outBuffers );
extern SM64_LIB_FN struct SM64AnimInfo* sm64_mario_get_anim_info( int32_t marioId, int16_t rot[3] );
extern SM64_LIB_FN void sm64_mario_anim_tick( int32_t marioId, uint32_t stateFlags, struct SM64AnimInfo* animInfo, struct SM64MarioGeometryBuffers *outBuffers, int16_t rot[3] );
extern SM64_LIB_FN void sm64_mario_delete( int32_t marioId );

extern SM64_LIB_FN void sm64_set_mario_action(int32_t marioId, uint32_t action);
extern SM64_LIB_FN uint32_t sm64_get_mario_action(int32_t marioId);
extern SM64_LIB_FN void sm64_set_mario_action_arg(int32_t marioId, uint32_t action, uint32_t actionArg);
extern SM64_LIB_FN void sm64_set_mario_animation(int32_t marioId, int32_t animID);
extern SM64_LIB_FN void sm64_set_mario_anim_frame(int32_t marioId, int16_t animFrame);
extern SM64_LIB_FN void sm64_set_mario_state(int32_t marioId, uint32_t flags);
extern SM64_LIB_FN void sm64_set_mario_position(int32_t marioId, float x, float y, float z);
extern SM64_LIB_FN void sm64_add_mario_position(int32_t marioId, float x, float y, float z);
extern SM64_LIB_FN void sm64_set_mario_angle(int32_t marioId, int16_t x, int16_t y, int16_t z);
extern SM64_LIB_FN void sm64_set_mario_faceangle(int32_t marioId, int16_t y);
extern SM64_LIB_FN void sm64_set_mario_velocity(int32_t marioId, float x, float y, float z);
extern SM64_LIB_FN void sm64_set_mario_forward_velocity(int32_t marioId, float vel);
extern SM64_LIB_FN void sm64_set_mario_water_level(int32_t marioId, signed int level);
extern SM64_LIB_FN signed int sm64_get_mario_water_level(int32_t marioId);
extern SM64_LIB_FN void sm64_set_mario_climbing_vector(int32_t marioId, bool direction[2][2]);
extern SM64_LIB_FN void sm64_set_mario_floor_override(int32_t marioId, uint16_t terrain, int16_t floorType);
extern SM64_LIB_FN void sm64_mario_take_damage(int32_t marioId, uint32_t damage, uint32_t subtype, float x, float y, float z);
extern SM64_LIB_FN void sm64_mario_heal(int32_t marioId, uint8_t healCounter);
extern SM64_LIB_FN void sm64_mario_set_health(int32_t marioId, uint16_t health);
extern SM64_LIB_FN uint16_t sm64_mario_get_health(int32_t marioId);
extern SM64_LIB_FN void sm64_mario_kill(int32_t marioId);
extern SM64_LIB_FN void sm64_mario_interact_cap(int32_t marioId, uint32_t capFlag, uint16_t capTime, uint8_t playMusic);
extern SM64_LIB_FN bool sm64_mario_attack(int32_t marioId, float x, float y, float z, float hitboxHeight);

extern SM64_LIB_FN uint32_t sm64_surface_object_create( const struct SM64SurfaceObject *surfaceObject );
extern SM64_LIB_FN void sm64_surface_object_move( uint32_t objectId, const struct SM64ObjectTransform *transform );
extern SM64_LIB_FN void sm64_surface_object_delete( uint32_t objectId );

extern SM64_LIB_FN void sm64_seq_player_play_sequence(uint8_t player, uint8_t seqId, uint16_t arg2);
extern SM64_LIB_FN void sm64_play_music(uint8_t player, uint16_t seqArgs, uint16_t fadeTimer);
extern SM64_LIB_FN void sm64_stop_background_music(uint16_t seqId);
extern SM64_LIB_FN void sm64_fadeout_background_music(uint16_t arg0, uint16_t fadeOut);
extern SM64_LIB_FN uint16_t sm64_get_current_background_music();
extern SM64_LIB_FN void sm64_play_sound(int32_t soundBits, float *pos);
extern SM64_LIB_FN void sm64_play_sound_global(int32_t soundBits);
extern SM64_LIB_FN int sm64_get_version();

extern SM64_LIB_FN void sm64_get_collision_surfaces(int marioId, struct SM64DebugSurface *floor, struct SM64DebugSurface *ceiling, struct SM64DebugSurface *wall, struct SM64DebugSurface surfaces[]);
extern SM64_LIB_FN int sm64_get_collision_surfaces_count(int marioId);

void audio_tick();
void* audio_thread(void* param);

extern SM64_LIB_FN void sm64_level_init(uint32_t roomsCount);
extern SM64_LIB_FN void sm64_level_unload();
extern SM64_LIB_FN void sm64_level_load_room(uint32_t roomId, const struct SM64Surface *staticSurfaces, uint32_t numSurfaces, const struct SM64SurfaceObject *staticObjects, uint32_t staticObjectsCount);
extern SM64_LIB_FN void sm64_level_unload_room(uint32_t roomId);
extern SM64_LIB_FN void sm64_level_update_loaded_rooms_list(int marioId, int *loadedRooms, int loadedCount);
extern SM64_LIB_FN void sm64_level_update_player_loaded_Rooms_with_clippers(int marioId, int *newloadedRooms, int loadedCount, const struct SM64Surface clippers[MAX_CLIPPER_BLOCKS_FACES], uint32_t clippersCount);
extern SM64_LIB_FN void sm64_level_rooms_switch(int switchedRooms[][2], int switchedRoomsCount);
extern SM64_LIB_FN void level_set_active_mario(int marioId);

extern SM64_LIB_FN float* sm64_get_mario_position(int marioId);

#endif//LIB_SM64_H
