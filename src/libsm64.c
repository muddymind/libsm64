#ifndef SM64_LIB_EXPORT
    #define SM64_LIB_EXPORT
#endif

#include "libsm64.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>
#include <pthread.h>

#include <PR/os_cont.h>
#include "decomp/engine/math_util.h"
#include <sm64.h>
#include <mario_animation_ids.h>
#include <mario_geo_switch_case_ids.h>
#include <seq_ids.h>
#include <object_fields.h>
#include "decomp/shim.h"
#include "decomp/memory.h"
#include "decomp/global_state.h"
#include "decomp/game/mario.h"
#include "decomp/game/object_stuff.h"
#include "decomp/engine/surface_collision.h"
#include "decomp/engine/graph_node.h"
#include "decomp/engine/geo_layout.h"
#include "decomp/game/rendering_graph_node.h"
#include "decomp/mario/geo.inc.h"
#include "decomp/game/platform_displacement.h"

#include "debug_print.h"
#include "load_surfaces.h"
#include "gfx_adapter.h"
#include "load_anim_data.h"
#include "load_tex_data.h"
#include "obj_pool.h"
#include "fake_interaction.h"
#include "decomp/pc/audio/audio_null.h"
#include "decomp/pc/audio/audio_wasapi.h"
#include "decomp/pc/audio/audio_pulse.h"
#include "decomp/pc/audio/audio_alsa.h"
#include "decomp/audio/external.h"
#include "decomp/audio/load_dat.h"
#include "decomp/tools/convTypes.h"
#include "decomp/tools/convUtils.h"
#include "decomp/mario/geo.inc.h"

static struct AllocOnlyPool *s_mario_geo_pool = NULL;
static struct GraphNode *s_mario_graph_node = NULL;
static struct AudioAPI *audio_api;

static bool s_init_global = false;
static bool s_init_one_mario = false;

struct MarioInstance
{
    struct GlobalState *globalState;
};
struct ObjPool s_mario_instance_pool = { 0, 0 };


struct GlobalState *set_global_mario_state(int marioId)
{
	struct GlobalState *state = ((struct MarioInstance *)s_mario_instance_pool.objects[ marioId ])->globalState;
	global_state_bind( state );
	level_set_active_mario(marioId);
	return state;
}


static void update_button( bool on, u16 button )
{
    gController.buttonPressed &= ~button;

    if( on )
    {
        if(( gController.buttonDown & button ) == 0 )
            gController.buttonPressed |= button;

        gController.buttonDown |= button;
    }
    else 
    {
        gController.buttonDown &= ~button;
    }
}

static struct Area *allocate_area( void )
{
    struct Area *result = malloc( sizeof( struct Area ));
    memset( result, 0, sizeof( struct Area ));

    result->flags = 1;
    result->camera = malloc( sizeof( struct Camera ));
    memset( result->camera, 0, sizeof( struct Camera ));

    return result;
}

static void free_area( struct Area *area )
{
    free( area->camera );
    free( area );
}

pthread_t gSoundThread;
SM64_LIB_FN void sm64_global_init( uint8_t *rom, uint8_t *outTexture, SM64DebugPrintFunctionPtr debugPrintFunction )
{
	g_debug_print_func = debugPrintFunction;
	
	uint8_t* rom2 = malloc(0x800000);
	memcpy(rom2, rom, 0x800000);
	rom = rom2;
	gSoundDataADSR = parse_seqfile(rom+0x57B720); //ctl
	gSoundDataRaw = parse_seqfile(rom+0x593560); //tbl
	gMusicData = parse_seqfile(rom+0x7B0860);
	gBankSetsData = rom+0x7CC621;
	memmove(gBankSetsData+0x45,gBankSetsData+0x45-1,0x5B);
	gBankSetsData[0x45]=0x00;
	ptrs_to_offsets(gSoundDataADSR);
	
	DEBUG_PRINT("ADSR: %p, raw: %p, bs: %p, seq: %p", gSoundDataADSR, gSoundDataRaw, gBankSetsData, gMusicData);

    if( s_init_global )
        sm64_global_terminate();

    s_init_global = true;

    load_mario_textures_from_rom( rom, outTexture );
    load_mario_anims_from_rom( rom );

    memory_init();
	
	#if HAVE_WASAPI
	if (audio_api == NULL && audio_wasapi.init()) {
		audio_api = &audio_wasapi;
		DEBUG_PRINT("Audio API: WASAPI");
	}
	#endif
	#if HAVE_PULSE_AUDIO
	if (audio_api == NULL && audio_pulse.init()) {
		audio_api = &audio_pulse;
		DEBUG_PRINT("Audio API: PulseAudio");
	}
	#endif
	#if HAVE_ALSA
	if (audio_api == NULL && audio_alsa.init()) {
		audio_api = &audio_alsa;
		DEBUG_PRINT("Audio API: Alsa");
	}
	#endif
	#ifdef TARGET_WEB
	if (audio_api == NULL && audio_sdl.init()) {
		audio_api = &audio_sdl;
		DEBUG_PRINT("Audio API: SDL");
	}
	#endif
	if (audio_api == NULL) {
		audio_api = &audio_null;
		DEBUG_PRINT("Audio API: Null");
	}
	
	audio_init();
	sound_init();
	sound_reset(0);
	pthread_create(&gSoundThread, NULL, audio_thread, &s_init_global);
}

SM64_LIB_FN void sm64_global_terminate( void )
{
    if( !s_init_global ) return;

	audio_api = NULL;
	pthread_cancel(gSoundThread);

    global_state_bind( NULL );
    
    if( s_init_one_mario )
    {
        for( int i = 0; i < s_mario_instance_pool.size; ++i )
            if( s_mario_instance_pool.objects[i] != NULL )
                sm64_mario_delete( i );

        obj_pool_free_all( &s_mario_instance_pool );
    }

    s_init_global = false;
    s_init_one_mario = false;
	   
	ctl_free();
    alloc_only_pool_free( s_mario_geo_pool );
    //surfaces_unload_all();
	level_unload();
    unload_mario_anims();
    memory_terminate();
}

// SM64_LIB_FN void sm64_static_surfaces_load( const struct SM64Surface *surfaceArray, uint32_t numSurfaces )
// {
//     surfaces_load_static( surfaceArray, numSurfaces );
// }

SM64_LIB_FN int32_t sm64_mario_create( float x, float y, float z, int16_t rx, int16_t ry, int16_t rz, uint8_t fake, int *loadedRooms, int loadedCount, float animationSale)
{
    int32_t marioIndex = obj_pool_alloc_index( &s_mario_instance_pool, sizeof( struct MarioInstance ));
	level_load_player_loaded_rooms(marioIndex);
	level_update_player_loaded_Rooms(marioIndex, loadedRooms, loadedCount);
    struct MarioInstance *newInstance = s_mario_instance_pool.objects[marioIndex];

    newInstance->globalState = global_state_create();
    global_state_bind( newInstance->globalState );

    if( !s_init_one_mario )
    {
        s_init_one_mario = true;
        s_mario_geo_pool = alloc_only_pool_init();
        s_mario_graph_node = process_geo_layout( s_mario_geo_pool, mario_geo_ptr );
    }

    gCurrSaveFileNum = 1;
    gMarioObject = hack_allocate_mario();
    gCurrentArea = allocate_area();
    gCurrentObject = gMarioObject;

    gMarioSpawnInfoVal.startPos[0] = x;
    gMarioSpawnInfoVal.startPos[1] = y;
    gMarioSpawnInfoVal.startPos[2] = z;

    gMarioSpawnInfoVal.startAngle[0] = rx;
    gMarioSpawnInfoVal.startAngle[1] = ry;
    gMarioSpawnInfoVal.startAngle[2] = rz;

    gMarioSpawnInfoVal.areaIndex = 0;
    gMarioSpawnInfoVal.activeAreaIndex = 0;
    gMarioSpawnInfoVal.behaviorArg = 0;
    gMarioSpawnInfoVal.behaviorScript = NULL;
    gMarioSpawnInfoVal.unk18 = NULL;
    gMarioSpawnInfoVal.next = NULL;

    init_mario_from_save_file();
	
	int initResult = init_mario();
	if(fake == 0)
	{
		if( initResult < 0 )
		{
			sm64_mario_delete( marioIndex );
			return -1;
		}

		set_mario_action( gMarioState, ACT_SPAWN_SPIN_AIRBORNE, 0);
		find_floor( x, y, z, &gMarioState->floor );
		gMarioState->tankMode=false;
		gMarioState->tankLeftCount=0;
		gMarioState->tankRightCount=0;
	}

	gMarioState->partsAnimationCount=0;

	gMarioState->partsAnimationMatrix = (float***) malloc(sizeof(float**)*MAX_ANIMATION_PARTS);
	for(int i=0; i<MAX_ANIMATION_PARTS; i++)
	{
		gMarioState->partsAnimationMatrix[i]=(float**) malloc(sizeof(float*)*4);
		for(int j=0; j<4; j++)
		{
			gMarioState->partsAnimationMatrix[i][j]=(float*) malloc(sizeof(float)*4);
		}
	}

	gMarioState->animationScaling=animationSale;

    return marioIndex;
}

SM64_LIB_FN struct SM64AnimInfo* sm64_mario_get_anim_info( int32_t marioId, int16_t rot[3] )
{
	if( marioId >= s_mario_instance_pool.size || s_mario_instance_pool.objects[marioId] == NULL )
    {
        DEBUG_PRINT("Tried to tick non-existant Mario with ID: %u", marioId);
        return NULL;
    }

	set_global_mario_state(marioId);
	
	rot[0] = gMarioState->marioObj->header.gfx.angle[0];
	rot[1] = gMarioState->marioObj->header.gfx.angle[1];
	rot[2] = gMarioState->marioObj->header.gfx.angle[2];
	
	return &gMarioState->marioObj->header.gfx.animInfo;
}

SM64_LIB_FN void sm64_mario_anim_tick( int32_t marioId, uint32_t stateFlags, struct SM64AnimInfo* animInfo, struct SM64MarioGeometryBuffers *outBuffers, int16_t rot[3] )
{
	if( marioId >= s_mario_instance_pool.size || s_mario_instance_pool.objects[marioId] == NULL )
    {
        DEBUG_PRINT("Tried to tick non-existant Mario with ID: %u", marioId);
        return;
    }
	
	set_global_mario_state(marioId);
	
	gMarioState->marioObj->header.gfx.angle[0] = rot[0];
	gMarioState->marioObj->header.gfx.angle[1] = rot[1];
	gMarioState->marioObj->header.gfx.angle[2] = rot[2];
	
	gMarioState->flags = stateFlags;
	mario_update_hitbox_and_cap_model( gMarioState );
	if (gMarioState->marioObj->header.gfx.animInfo.animFrame != animInfo->animID && animInfo->animID != -1)
        set_mario_anim_with_accel( gMarioState, animInfo->animID, animInfo->animAccel );
    gMarioState->marioObj->header.gfx.animInfo.animAccel = animInfo->animAccel;

    gfx_adapter_bind_output_buffers( outBuffers );
    geo_process_root_hack_single_node( s_mario_graph_node );
    gAreaUpdateCounter++;
}


SM64_LIB_FN void sm64_mario_tick(int32_t marioId, const struct SM64MarioInputs *inputs, struct SM64MarioState *outState, struct SM64MarioGeometryBuffers *outBuffers )
{
    if( marioId >= s_mario_instance_pool.size || s_mario_instance_pool.objects[marioId] == NULL )
    {
        DEBUG_PRINT("Tried to tick non-existant Mario with ID: %u", marioId);
        return;
    }

	set_global_mario_state(marioId);

    gMarioState->fallDamage = 0;

    update_button( inputs->buttonA, A_BUTTON );
    update_button( inputs->buttonB, B_BUTTON );
    update_button( inputs->buttonZ, Z_TRIG );

	gMarioState->marioObj->header.gfx.cameraToObject[0] = 0; 
	gMarioState->marioObj->header.gfx.cameraToObject[1] = 0;
	gMarioState->marioObj->header.gfx.cameraToObject[2] = 0;

    gMarioState->area->camera->yaw = atan2s( inputs->camLookZ, inputs->camLookX );

    gController.stickX = -64.0f * inputs->stickX;
    gController.stickY = 64.0f * inputs->stickY;
    gController.stickMag = sqrtf( gController.stickX*gController.stickX + gController.stickY*gController.stickY );

	apply_mario_platform_displacement();
    bhv_mario_update();
    update_mario_platform(); // TODO platform grabbed here and used next tick could be a use-after-free

    gfx_adapter_bind_output_buffers( outBuffers );

    geo_process_root_hack_single_node( s_mario_graph_node );

    gAreaUpdateCounter++;

	int i;
    outState->health = gMarioState->health;
    vec3f_copy( outState->position, gMarioState->pos );
    vec3f_copy( outState->velocity, gMarioState->vel );
    for (i=0; i<3; i++) outState->angleVel[i] = (float)gMarioState->angleVel[i] / 32768.0f * 3.14159f;
    outState->faceAngle = (float)gMarioState->faceAngle[1] / 32768.0f * 3.14159f;
    outState->pitchAngle = (float)gMarioState->faceAngle[0] / 32768.0f * 3.14159f;
	outState->action = gMarioState->action;
	outState->flags = gMarioState->flags;
	outState->particleFlags = gMarioState->particleFlags;
	outState->invincTimer = gMarioState->invincTimer;
	outState->burnTimer = 160 - gMarioState->marioObj->oMarioBurnTimer;
	outState->fallDamage = gMarioState->fallDamage;
}

SM64_LIB_FN void sm64_mario_delete( int32_t marioId )
{
    if( marioId >= s_mario_instance_pool.size || s_mario_instance_pool.objects[marioId] == NULL )
    {
        DEBUG_PRINT("Tried to delete non-existant Mario with ID: %u", marioId);
        return;
    }

    struct GlobalState *globalState = set_global_mario_state(marioId);

	stop_sound(SOUND_MARIO_SNORING3, gMarioState->marioObj->header.gfx.cameraToObject);

	for(int i=0; i<MAX_ANIMATION_PARTS; i++)
	{
		for(int j=0; j<4; j++)
		{
			free(gMarioState->partsAnimationMatrix[i][j]);
		}
		free(gMarioState->partsAnimationMatrix[i]);
	}
	free(gMarioState->partsAnimationMatrix);

    free( gMarioObject );
    free_area( gCurrentArea );

    global_state_delete( globalState );
    obj_pool_free_index( &s_mario_instance_pool, marioId );
}

SM64_LIB_FN void sm64_set_mario_position(int32_t marioId, float x, float y, float z)
{
	set_global_mario_state(marioId);
	
	gMarioState->pos[0] = x;
	gMarioState->pos[1] = y;
	gMarioState->pos[2] = z;
	vec3f_copy(gMarioState->marioObj->header.gfx.pos, gMarioState->pos);
}

SM64_LIB_FN void sm64_add_mario_position(int32_t marioId, float x, float y, float z)
{
	set_global_mario_state(marioId);
	
	gMarioState->pos[0] += x;
	gMarioState->pos[1] += y;
	gMarioState->pos[2] += z;
	vec3f_copy(gMarioState->marioObj->header.gfx.pos, gMarioState->pos);
}

SM64_LIB_FN void sm64_set_mario_angle(int32_t marioId, int16_t x, int16_t y, int16_t z)
{
	set_global_mario_state(marioId);
	
	vec3s_set(gMarioState->faceAngle, x, y, z);
	vec3s_set(gMarioState->marioObj->header.gfx.angle, 0, gMarioState->faceAngle[1], 0);
}

SM64_LIB_FN void sm64_set_mario_faceangle(int32_t marioId, int16_t y)
{
	set_global_mario_state(marioId);
	
	gMarioState->faceAngle[1] = y;
	vec3s_set(gMarioState->marioObj->header.gfx.angle, 0, gMarioState->faceAngle[1], 0);
}

SM64_LIB_FN void sm64_set_mario_velocity(int32_t marioId, float x, float y, float z)
{
	set_global_mario_state(marioId);
	
	gMarioState->vel[0] = x;
	gMarioState->vel[1] = y;
	gMarioState->vel[2] = z;
}

SM64_LIB_FN void sm64_set_mario_forward_velocity(int32_t marioId, float vel)
{
	set_global_mario_state(marioId);
	
	gMarioState->forwardVel = vel;
}

SM64_LIB_FN void sm64_set_mario_action(int32_t marioId, uint32_t action)
{
	set_global_mario_state(marioId);
	
	set_mario_action( gMarioState, action, 0);
}

SM64_LIB_FN void sm64_set_mario_action_arg(int32_t marioId, uint32_t action, uint32_t actionArg)
{
	set_global_mario_state(marioId);
	
	set_mario_action( gMarioState, action, actionArg);
}

SM64_LIB_FN void sm64_set_mario_animation(int32_t marioId, int32_t animID)
{
	set_global_mario_state(marioId);
	
	set_mario_animation(gMarioState, animID);
}

SM64_LIB_FN void sm64_set_mario_anim_frame(int32_t marioId, int16_t animFrame)
{
	set_global_mario_state(marioId);
	
	gMarioState->marioObj->header.gfx.animInfo.animFrame = animFrame;
	
}

SM64_LIB_FN void sm64_set_mario_state(int32_t marioId, uint32_t flags)
{
	set_global_mario_state(marioId);
	
	gMarioState->flags = flags;
}

SM64_LIB_FN void sm64_set_mario_water_level(int32_t marioId, signed int level)
{
	set_global_mario_state(marioId);
	
	gMarioState->waterLevel = level;
}

SM64_LIB_FN signed int sm64_get_mario_water_level(int32_t marioId)
{
	set_global_mario_state(marioId);
	
	return gMarioState->waterLevel;
}

SM64_LIB_FN void sm64_set_mario_floor_override(int32_t marioId, uint16_t terrain, int16_t floorType)
{
	set_global_mario_state(marioId);
	
	gMarioState->overrideTerrain = terrain;
	gMarioState->overrideFloorType = floorType;
}

SM64_LIB_FN void sm64_mario_take_damage(int32_t marioId, uint32_t damage, uint32_t subtype, float x, float y, float z)
{
	set_global_mario_state(marioId);
	
	fake_damage_knock_back(gMarioState, damage, subtype, x, y, z);
}

SM64_LIB_FN void sm64_mario_heal(int32_t marioId, uint8_t healCounter)
{
	set_global_mario_state(marioId);
	
	gMarioState->healCounter += healCounter;
}

SM64_LIB_FN void sm64_mario_set_health(int32_t marioId, uint16_t health)
{
	set_global_mario_state(marioId);
	
	gMarioState->health = health;
}

SM64_LIB_FN uint16_t sm64_mario_get_health(int32_t marioId)
{
	set_global_mario_state(marioId);
	
	return gMarioState->health;
}

SM64_LIB_FN void sm64_mario_kill(int32_t marioId)
{
	set_global_mario_state(marioId);
	
	gMarioState->health = 0xFF;
}

SM64_LIB_FN void sm64_mario_interact_cap(int32_t marioId, uint32_t capFlag, uint16_t capTime, uint8_t playMusic)
{
	set_global_mario_state(marioId);
	
	uint16_t capMusic = 0;
	if(gMarioState->action != ACT_GETTING_BLOWN && capFlag != 0)
	{
		gMarioState->flags &= ~MARIO_CAP_ON_HEAD & ~MARIO_CAP_IN_HAND;
		gMarioState->flags |= capFlag;
		
		switch(capFlag)
		{
			case MARIO_VANISH_CAP:
				if(capTime == 0) capTime = 600;
				capMusic = SEQUENCE_ARGS(4, SEQ_EVENT_POWERUP);
				break;
			case MARIO_METAL_CAP:
				if(capTime == 0) capTime = 600;
				capMusic = SEQUENCE_ARGS(4, SEQ_EVENT_METAL_CAP);
				break;
			case MARIO_WING_CAP:
				if(capTime == 0) capTime = 1800;
				capMusic = SEQUENCE_ARGS(4, SEQ_EVENT_POWERUP);
				break;
		}
		
		if (capTime > gMarioState->capTimer) {
			gMarioState->capTimer = capTime;
		}
		
		if ((gMarioState->action & ACT_FLAG_IDLE) || gMarioState->action == ACT_WALKING) {
			gMarioState->flags |= MARIO_CAP_IN_HAND;
			set_mario_action(gMarioState, ACT_PUTTING_ON_CAP, 0);
		} else {
			gMarioState->flags |= MARIO_CAP_ON_HEAD;
		}

		play_sound(SOUND_MENU_STAR_SOUND, gMarioState->marioObj->header.gfx.cameraToObject);
		play_sound(SOUND_MARIO_HERE_WE_GO, gMarioState->marioObj->header.gfx.cameraToObject);

		if (playMusic != 0 && capMusic != 0) {
			play_cap_music(capMusic);
		}
	}
}

SM64_LIB_FN bool sm64_mario_attack(int32_t marioId, float x, float y, float z, float hitboxHeight)
{
	set_global_mario_state(marioId);
	
	return fake_interact_bounce_top(gMarioState, x, y, z, hitboxHeight);
}

SM64_LIB_FN uint32_t sm64_surface_object_create( const struct SM64SurfaceObject *surfaceObject )
{
    uint32_t id = level_load_dynamic_object( surfaceObject );
    return id;
}

SM64_LIB_FN void sm64_surface_object_move( uint32_t objectId, const struct SM64ObjectTransform *transform )
{
    level_update_dynamic_object_transform( objectId, transform );
}

SM64_LIB_FN void sm64_surface_object_delete( uint32_t objectId )
{
    // A mario standing on the platform that is being destroyed will have a pointer to freed memory if we don't clear it.
    for( int i = 0; i < s_mario_instance_pool.size; ++i )
    {
        if( s_mario_instance_pool.objects[i] == NULL )
            continue;

        struct GlobalState *state = set_global_mario_state(i);
		if( state->mgMarioObject->platform == level_get_dynamic_object_transform( objectId ))
            state->mgMarioObject->platform = NULL;
    }

    level_unload_dynamic_object( objectId, true );

	// We need to reposition Mario's floor in case it was in the removed object.
    for( int i = 0; i < s_mario_instance_pool.size; ++i )
    {
        if( s_mario_instance_pool.objects[i] == NULL )
            continue;

		set_global_mario_state(i);
		find_floor(gMarioState->pos[0],gMarioState->pos[1],gMarioState->pos[2],&(gMarioState->floor));
    }
}

SM64_LIB_FN void sm64_seq_player_play_sequence(uint8_t player, uint8_t seqId, uint16_t arg2)
{
    seq_player_play_sequence(player,seqId,arg2);
}

SM64_LIB_FN void sm64_play_music(uint8_t player, uint16_t seqArgs, uint16_t fadeTimer)
{
    play_music(player,seqArgs,fadeTimer);
}

SM64_LIB_FN void sm64_stop_background_music(uint16_t seqId)
{
    stop_background_music(seqId);
}

SM64_LIB_FN void sm64_fadeout_background_music(uint16_t arg0, uint16_t fadeOut)
{
    fadeout_background_music(arg0,fadeOut);
}

SM64_LIB_FN uint16_t sm64_get_current_background_music()
{
    return get_current_background_music();
}

SM64_LIB_FN void sm64_play_sound(int32_t soundBits, float *pos)
{
    play_sound(soundBits,pos);
}

SM64_LIB_FN void sm64_play_sound_global(int32_t soundBits)
{
    play_sound(soundBits,gGlobalSoundSource);
}


#ifdef VERSION_EU
#define SAMPLES_HIGH 656
#define SAMPLES_LOW 640
#else
#define SAMPLES_HIGH 544
#define SAMPLES_LOW 528
#endif

void audio_tick()
{
    int samples_left = audio_api->buffered();
    u32 num_audio_samples = samples_left < audio_api->get_desired_buffered() ? SAMPLES_HIGH : SAMPLES_LOW;
    s16 audio_buffer[SAMPLES_HIGH * 2 * 2];
    for (int i = 0; i < 2; i++)
	{
        create_next_audio_buffer(audio_buffer + i * (num_audio_samples * 2), num_audio_samples);
    }
    audio_api->play((uint8_t *)audio_buffer, 2 * num_audio_samples * 4);
}

#include <sys/time.h>

long long timeInMilliseconds(void)
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	
	return(((long long)tv.tv_sec)*1000)+(tv.tv_usec/1000);
}

void* audio_thread(void* keepAlive)
{
	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE,NULL); 
    pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED,NULL);
	
    long long currentTime = timeInMilliseconds();
    long long targetTime = 0;
    while(1)
	{
		if(!*((bool*)keepAlive)) return NULL;
		audio_signal_game_loop_tick();
		audio_tick();
		targetTime = currentTime + 33;
		while (timeInMilliseconds() < targetTime)
		{
			usleep(100);
			if(!*((bool*)keepAlive)) return NULL;
		}
		currentTime = timeInMilliseconds();
    }
}

void copy_debug_collision_surface(struct SM64DebugSurface *dst, struct Surface *src)
{
	if( src && src->isValid ) {
		vec3i2f_copy(dst->v1, src->vertex1);
		vec3i2f_copy(dst->v2, src->vertex2);
		vec3i2f_copy(dst->v3, src->vertex3);
		dst->color = src->eSurfaceType;
		dst->valid = true;
	}
	else
	{
		vec3f_reset(dst->v1, 0);
		vec3f_reset(dst->v2, 0);
		vec3f_reset(dst->v3, 0);
		dst->color = 0;
		dst->valid = false;
	}	
}

void sm64_get_collision_surfaces(int marioId, struct SM64DebugSurface *floor, struct SM64DebugSurface *ceiling, struct SM64DebugSurface *wall, struct SM64DebugSurface surfaces[])
{
	level_set_active_mario(marioId);

	copy_debug_collision_surface(floor, gMarioState->floor);
	copy_debug_collision_surface(wall, gMarioState->wall);
	copy_debug_collision_surface(ceiling, gMarioState->ceil);
	

	int index=0;
	int roomsCount = level_get_room_count();
	for(int i=0; i<roomsCount; i++)
	{
		int surfCount = level_get_room_surfaces_count(i);
		for(int j=0; j<surfCount; j++)
		{
			copy_debug_collision_surface(&(surfaces[index++]), level_get_room_surface(i, j));
		}
	}
}

int sm64_get_collision_surfaces_count(int marioId)
{
	level_set_active_mario(marioId);

	int resultCount = 0;
	int roomsCount = level_get_room_count();
	for(int i=0; i<roomsCount; i++)
	{
		resultCount+=level_get_room_surfaces_count(i);
	}

	return resultCount;
}

void sm64_level_init(uint32_t roomsCount)
{
	level_init(roomsCount);
}

void sm64_level_unload()
{
	level_unload();
}

void sm64_level_load_room(uint32_t roomId, const struct SM64Surface *staticSurfaces, uint32_t numSurfaces, const struct SM64SurfaceObject *staticObjects, uint32_t staticObjectsCount)
{
	level_load_room(roomId, staticSurfaces, numSurfaces, staticObjects, staticObjectsCount);
}

void sm64_level_unload_room(uint32_t roomId)
{
	level_unload_room(roomId);
}

void sm64_level_update_loaded_rooms_list(int marioId, int *loadedRooms, int loadedCount)
{
	level_update_player_loaded_Rooms(marioId, loadedRooms, loadedCount);
}

void sm64_level_update_player_loaded_Rooms_with_clippers(int marioId, int *newloadedRooms, int loadedCount, const struct SM64Surface clippers[MAX_CLIPPER_BLOCKS_FACES], uint32_t clippersCount)
{
	level_update_player_loaded_Rooms_with_clippers(marioId, newloadedRooms, loadedCount, clippers, clippersCount);
}

void sm64_level_rooms_switch(int switchedRooms[][2], int switchedRoomsCount)
{
	level_rooms_switch(switchedRooms, switchedRoomsCount);
}

void sm64_level_set_active_mario(int marioId)
{
	level_set_active_mario(marioId);
}

float* sm64_get_mario_position(int marioId)
{
	if( marioId >= s_mario_instance_pool.size || s_mario_instance_pool.objects[marioId] == NULL )
    {
        return NULL;
    }
	
	set_global_mario_state(marioId);
	
	return gMarioState->pos;
}

void sm64_set_mario_tank_mode(int marioId, bool tankMode)
{
	if( marioId >= s_mario_instance_pool.size || s_mario_instance_pool.objects[marioId] == NULL )
    {
        return;
    }
	
	set_global_mario_state(marioId);

	gMarioState->tankMode=tankMode;
}

float** sm64_get_mario_part_animation_matrix(int marioId, int partId)
{
	if( marioId >= s_mario_instance_pool.size || s_mario_instance_pool.objects[marioId] == NULL )
    {
        return NULL;
    }
	
	set_global_mario_state(marioId);

	if(partId >= gMarioState->partsAnimationCount)
	{
		printf("Requested invalid part id %d for animation matrix when there's only %d\n", partId, gMarioState->partsAnimationCount);
		return NULL;
	}

	return gMarioState->partsAnimationMatrix[partId];
}
