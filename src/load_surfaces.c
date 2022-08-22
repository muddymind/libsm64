#include "load_surfaces.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "decomp/include/types.h"
#include "decomp/include/surface_terrains.h"
#include "decomp/engine/math_util.h"
#include "decomp/shim.h"

#include "debug_print.h"

#define BIG_HACK_FLOOR_HEIGHT 100000
#define BIG_HACK_FLOOR_DIMENSIONS 1000
#define MAX_MARIO_PLAYERS 10


static uint32_t s_level_rooms_count = 0;

static struct Room **s_level_rooms=NULL;

static struct MarioLoadedRooms s_mario_loaded_rooms[MAX_MARIO_PLAYERS];
static struct MarioLoadedRooms *s_current_loaded_rooms;

static struct DynamicObjects *s_dynamic_objects = NULL;

static struct Room *s_big_floor_hack = NULL;

static bool s_level_loaded = false;


#define CONVERT_ANGLE( x ) ((s16)( -(x) / 180.0f * 32768.0f ))

#pragma region Auxiliary Funcitons

static void init_transform( struct SurfaceObjectTransform *out, const struct SM64ObjectTransform *in )
{
    out->aVelX = 0.0f;
    out->aVelY = 0.0f;
    out->aVelZ = 0.0f;
    out->aPosX = in->position[0];
    out->aPosY = in->position[1];
    out->aPosZ = in->position[2];

    out->aAngleVelPitch = 0.0f;
    out->aAngleVelYaw   = 0.0f;
    out->aAngleVelRoll  = 0.0f;
    out->aFaceAnglePitch = CONVERT_ANGLE(in->eulerRotation[0]);
    out->aFaceAngleYaw   = CONVERT_ANGLE(in->eulerRotation[1]);
    out->aFaceAngleRoll  = CONVERT_ANGLE(in->eulerRotation[2]);
}

static void update_transform( struct SurfaceObjectTransform *out, const struct SM64ObjectTransform *in )
{
    out->aVelX = in->position[0] - out->aPosX;
    out->aVelY = in->position[1] - out->aPosY;
    out->aVelZ = in->position[2] - out->aPosZ;
    out->aPosX = in->position[0];
    out->aPosY = in->position[1];
    out->aPosZ = in->position[2];

    s16 inX = CONVERT_ANGLE(in->eulerRotation[0]);
    s16 inY = CONVERT_ANGLE(in->eulerRotation[1]);
    s16 inZ = CONVERT_ANGLE(in->eulerRotation[2]);

    out->aAngleVelPitch = inX - out->aFaceAnglePitch;
    out->aAngleVelYaw   = inY - out->aFaceAngleYaw;
    out->aAngleVelRoll  = inZ - out->aFaceAngleRoll;
    out->aFaceAnglePitch = inX;
    out->aFaceAngleYaw   = inY;
    out->aFaceAngleRoll  = inZ;
}

/**
 * Returns whether a surface has exertion/moves Mario
 * based on the surface type.
 */
static s32 surface_has_force(s16 surfaceType) {
    s32 hasForce = FALSE;

    switch (surfaceType) {
        case SURFACE_0004: // Unused
        case SURFACE_FLOWING_WATER:
        case SURFACE_DEEP_MOVING_QUICKSAND:
        case SURFACE_SHALLOW_MOVING_QUICKSAND:
        case SURFACE_MOVING_QUICKSAND:
        case SURFACE_HORIZONTAL_WIND:
        case SURFACE_INSTANT_MOVING_QUICKSAND:
            hasForce = TRUE;
            break;

        default:
            break;
    }
    return hasForce;
}

static void engine_surface_from_lib_surface( struct Surface *surface, const struct SM64Surface *libSurf, struct SurfaceObjectTransform *transform, enum SM64ExternalSurfaceTypes externalType )
{
    int16_t type = libSurf->type;
    int16_t force = libSurf->force;
    s32 x1 = libSurf->vertices[0][0];
    s32 y1 = libSurf->vertices[0][1];
    s32 z1 = libSurf->vertices[0][2];
    s32 x2 = libSurf->vertices[1][0];
    s32 y2 = libSurf->vertices[1][1];
    s32 z2 = libSurf->vertices[1][2];
    s32 x3 = libSurf->vertices[2][0];
    s32 y3 = libSurf->vertices[2][1];
    s32 z3 = libSurf->vertices[2][2];

    s32 maxY, minY;
    f32 nx, ny, nz;
    f32 mag;

    if( transform != NULL )
    {
        Mat4 m;
        Vec3s rotation = { transform->aFaceAnglePitch, transform->aFaceAngleYaw, transform->aFaceAngleRoll };
        Vec3f position = { transform->aPosX, transform->aPosY, transform->aPosZ };
        mtxf_rotate_zxy_and_translate(m, position, rotation);

        Vec3f v1 = { x1, y1, z1 };
        Vec3f v2 = { x2, y2, z2 };
        Vec3f v3 = { x3, y3, z3 };

        mtxf_mul_vec3f( m, v1 );
        mtxf_mul_vec3f( m, v2 );
        mtxf_mul_vec3f( m, v3 );

        x1 = v1[0]; y1 = v1[1]; z1 = v1[2];
        x2 = v2[0]; y2 = v2[1]; z2 = v2[2];
        x3 = v3[0]; y3 = v3[1]; z3 = v3[2];

        surface->transform = transform;
    }
    else
    {
        surface->transform = NULL;
    }

    // (v2 - v1) x (v3 - v2)
    nx = (y2 - y1) * (z3 - z2) - (z2 - z1) * (y3 - y2);
    ny = (z2 - z1) * (x3 - x2) - (x2 - x1) * (z3 - z2);
    nz = (x2 - x1) * (y3 - y2) - (y2 - y1) * (x3 - x2);
    mag = sqrtf(nx * nx + ny * ny + nz * nz);

    // Could have used min_3 and max_3 for this...
    minY = y1;
    if (y2 < minY) {
        minY = y2;
    }
    if (y3 < minY) {
        minY = y3;
    }

    maxY = y1;
    if (y2 > maxY) {
        maxY = y2;
    }
    if (y3 > maxY) {
        maxY = y3;
    }

    if (mag < 0.0001) {
        //DEBUG_PRINT("ERROR: normal magnitude is very close to zero:");
        //DEBUG_PRINT("v1 %i %i %i", x1, y1, z1 );
        //DEBUG_PRINT("v2 %i %i %i", x2, y2, z2 );
        //DEBUG_PRINT("v3 %i %i %i", x3, y3, z3 );
        surface->isValid = 0;
        return;
    }

    mag = (f32)(1.0 / mag);
    nx *= mag;
    ny *= mag;
    nz *= mag;

    surface->vertex1[0] = x1;
    surface->vertex2[0] = x2;
    surface->vertex3[0] = x3;

    surface->vertex1[1] = y1;
    surface->vertex2[1] = y2;
    surface->vertex3[1] = y3;

    surface->vertex1[2] = z1;
    surface->vertex2[2] = z2;
    surface->vertex3[2] = z3;

    surface->normal.x = nx;
    surface->normal.y = ny;
    surface->normal.z = nz;

    surface->originOffset = -(nx * x1 + ny * y1 + nz * z1);

    surface->lowerY = minY - 5;
    surface->upperY = maxY + 5;

    s16 hasForce = surface_has_force(type);
    s16 flags = 0; // surf_has_no_cam_collision(type);

    surface->room = 0;
    surface->type = type;
    surface->flags = (s8) flags;
    surface->terrain = libSurf->terrain;

    if (hasForce) {
        surface->force = force;
    } else {
        surface->force = 0;
    }

    surface->isValid = 1;
    surface->eSurfaceType = externalType;
    surface->externalRoom = libSurf->roomId;
    surface->externalFace = libSurf->faceId;
}

#pragma endregion

#pragma region Dynamic objects management

void level_init_dynamic_objects()
{
    s_dynamic_objects = (struct DynamicObjects*) malloc(sizeof(struct DynamicObjects));

    s_dynamic_objects->objects = NULL;
    s_dynamic_objects->objectsCount = 0;
    s_dynamic_objects->cached_surfaces = NULL;
    s_dynamic_objects->cached_count = 0;
}

uint32_t level_load_dynamic_object( const struct SM64SurfaceObject *surfaceObject )
{
    bool pickedOldIndex = false;
    uint32_t idx = s_dynamic_objects->objectsCount;

    for( int i = 0; i < s_dynamic_objects->objectsCount; ++i )
    {
        if( s_dynamic_objects->objects[i].surfaceCount == 0 )
        {
            pickedOldIndex = true;
            idx = i;
            break;
        }
    }

    if( !pickedOldIndex )
    {
        idx = s_dynamic_objects->objectsCount;
        s_dynamic_objects->objectsCount++;
        s_dynamic_objects->objects = realloc( s_dynamic_objects->objects, s_dynamic_objects->objectsCount * sizeof( struct LoadedSurfaceObject ));
    }

    struct LoadedSurfaceObject *obj = &s_dynamic_objects->objects[idx];

    obj->surfaceCount = surfaceObject->surfaceCount;

    obj->transform = malloc( sizeof( struct SurfaceObjectTransform ));
    init_transform( obj->transform, &surfaceObject->transform );

    obj->libSurfaces = malloc( obj->surfaceCount * sizeof( struct SM64Surface ));
    memcpy( obj->libSurfaces, surfaceObject->surfaces, obj->surfaceCount * sizeof( struct SM64Surface ));

    obj->engineSurfaces = malloc( obj->surfaceCount * sizeof( struct Surface ));
    for( int i = 0; i < obj->surfaceCount; ++i )
    {
        engine_surface_from_lib_surface( &obj->engineSurfaces[i], &obj->libSurfaces[i], obj->transform, EXTERNAL_SURFACE_TYPE_DYNAMIC_OBJECT);
    }

    level_update_cached_object_surface_list();

    #ifdef DEBUG_LEVEL_ROOMS
        printf("Added Collider %d\n", idx);
    #endif

    return idx;
}

void level_unload_dynamic_object( uint32_t objId, bool update_cache )
{
    if( objId >= s_dynamic_objects->objectsCount || s_dynamic_objects->objects[objId].surfaceCount == 0 )
    {
        DEBUG_PRINT("Tried to unload non-existant surface object with ID: %u", objId);
        return;
    }

    free( s_dynamic_objects->objects[objId].transform );
    free( s_dynamic_objects->objects[objId].libSurfaces );
    free( s_dynamic_objects->objects[objId].engineSurfaces );

    s_dynamic_objects->objects[objId].surfaceCount = 0;
    s_dynamic_objects->objects[objId].transform = NULL;
    s_dynamic_objects->objects[objId].libSurfaces = NULL;
    s_dynamic_objects->objects[objId].engineSurfaces = NULL;

    #ifdef DEBUG_LEVEL_ROOMS
        printf("SM64: Removed Collider %d\n", objId);
    #endif

    if(update_cache)
    {
        level_update_cached_object_surface_list();
    }
}

void level_unload_all_dynamic_objects()
{
    if(s_dynamic_objects==NULL)
    {
        return;
    }
    if(s_dynamic_objects->objects!=NULL)
    {
        for( int i = 0; i < s_dynamic_objects->objectsCount; ++i )
        {
            level_unload_dynamic_object(i, false);
        }
        free( s_dynamic_objects->objects );
        s_dynamic_objects->objects = NULL;
        s_dynamic_objects->objectsCount = 0;
    }

    if(s_dynamic_objects->cached_surfaces != NULL)
    {
        free(s_dynamic_objects->cached_surfaces);
        s_dynamic_objects->cached_surfaces = NULL;
        s_dynamic_objects->cached_count = 0;
    }

    free(s_dynamic_objects);
    s_dynamic_objects = NULL;
}

void level_update_cached_object_surface_list()
{
    if(s_dynamic_objects->cached_surfaces!=NULL)
    {
        free(s_dynamic_objects->cached_surfaces);
        s_dynamic_objects->cached_surfaces=NULL;
        s_dynamic_objects->cached_count = 0;
    }

    s_dynamic_objects->cached_count=0;

    for(int i=0; i<s_dynamic_objects->objectsCount; i++)
    {
        s_dynamic_objects->cached_count+=s_dynamic_objects->objects[i].surfaceCount;
    }

    //make space for big floor surfaces
    if(s_big_floor_hack!=NULL)
        s_dynamic_objects->cached_count+=s_big_floor_hack->count;
    
    s_dynamic_objects->cached_surfaces = (struct Surface**)malloc(sizeof(struct Surface*)*s_dynamic_objects->cached_count);

    int currentIdx=0;
    for(int i=0; i<s_dynamic_objects->objectsCount; i++)
    {
        for(int j=0; j<s_dynamic_objects->objects[i].surfaceCount; j++)
        {
            s_dynamic_objects->cached_surfaces[currentIdx++] = &(s_dynamic_objects->objects[i].engineSurfaces[j]);
        }
    }

    if(s_big_floor_hack!=NULL)
    {
        for(int i=0; i< s_big_floor_hack->count; i++)
        {
            s_dynamic_objects->cached_surfaces[currentIdx++]=&(s_big_floor_hack->surfaces[i]);
        }
    }
}

void level_update_dynamic_object_transform( uint32_t objId, const struct SM64ObjectTransform *newTransform )
{
    if( objId >= s_dynamic_objects->objectsCount || s_dynamic_objects->objects[objId].surfaceCount == 0 )
    {
        DEBUG_PRINT("Tried to update non-existant surface object with ID: %u", objId);
        return;
    }

    update_transform( s_dynamic_objects->objects[objId].transform, newTransform );
    for( int i = 0; i < s_dynamic_objects->objects[objId].surfaceCount; ++i )
    {
        struct LoadedSurfaceObject *obj = &s_dynamic_objects->objects[objId];
        engine_surface_from_lib_surface( &obj->engineSurfaces[i], &obj->libSurfaces[i], obj->transform, EXTERNAL_SURFACE_TYPE_DYNAMIC_OBJECT );
    }
}

struct SurfaceObjectTransform *level_get_dynamic_object_transform( uint32_t objId )
{
    if( objId < s_dynamic_objects->objectsCount && s_dynamic_objects->objects[objId].surfaceCount != 0 )
        return s_dynamic_objects->objects[objId].transform;
    
    return NULL;
}

#pragma endregion

#pragma region Room management

void level_init_rooms(int roomsCount)
{
    s_level_rooms_count = roomsCount;
    s_level_rooms = (struct Room**)malloc(sizeof(struct Room*)*roomsCount);
    for(uint32_t i=0; i<roomsCount; i++)
    {
        s_level_rooms[i]=NULL;
    }
}

void level_load_room(uint32_t roomId, const struct SM64Surface *staticSurfaces, uint32_t numSurfaces, const struct SM64SurfaceObject *staticObjects, uint32_t staticObjectsCount) 
{
    if( !s_level_loaded || s_level_rooms == NULL )
    {
        #ifdef DEBUG_LEVEL_ROOMS
            printf("SM64: tried to load room %d into non-loaded level.\n", roomId);
        #endif
        return;
    }

    if(roomId >= s_level_rooms_count )
    {
        #ifdef DEBUG_LEVEL_ROOMS
            printf("SM64: tried to load room %d when there's only space for %d rooms.\n", roomId, s_level_rooms_count);
        #endif
        return;
    }

    if(s_level_rooms[roomId] != NULL)
    {
        #ifdef DEBUG_LEVEL_ROOMS
            printf("SM64: tried to reload room %d that was already loaded.\n", roomId);
        #endif
        return;
    }

    #ifdef DEBUG_LEVEL_ROOMS
		printf("SM64: loading room %d\n", roomId);
    #endif

    struct Room *room = (struct Room*)malloc(sizeof(struct Room));
    s_level_rooms[roomId] = room;

    room->count = numSurfaces;
    for(int i=0; i<staticObjectsCount; i++)
    {
        room->count += staticObjects[i].surfaceCount;
    }
    room->surfaces = malloc( sizeof( struct Surface ) * room->count );

    for( uint32_t i = 0; i < numSurfaces; ++i )
    {
        engine_surface_from_lib_surface( &room->surfaces[i], &staticSurfaces[i], NULL, EXTERNAL_SURFACE_TYPE_STATIC_SURFACE );
    }

    uint32_t cIdx=numSurfaces;
    for(int i=0; i<staticObjectsCount; i++)
    {
        struct SurfaceObjectTransform *transform = (struct SurfaceObjectTransform*)malloc(sizeof(struct SurfaceObjectTransform));
        init_transform( transform, &(staticObjects[i].transform) );
        for(int j=0; j<staticObjects[i].surfaceCount;j++)
        {
            engine_surface_from_lib_surface( &room->surfaces[cIdx++], &staticObjects[i].surfaces[j], transform, EXTERNAL_SURFACE_TYPE_STATIC_MESH );
        }
    }
}

void level_unload_all_rooms()
{
    if(s_level_rooms!=NULL)
    {
        for(uint32_t i=0; i<s_level_rooms_count; i++)
        {
            level_unload_room(i);
        }

        free(s_level_rooms);
        s_level_rooms=NULL;
        s_level_rooms_count = 0;
    }
}

void level_unload_room(uint32_t roomId)
{
    #ifdef DEBUG_LEVEL_ROOMS
		printf("SM64: unloading room %d\n", roomId);
    #endif
    
    if( s_level_rooms==NULL || roomId >= s_level_rooms_count )
    {
        return;
    }

    struct Room *room = s_level_rooms[roomId];
    s_level_rooms[roomId] = NULL;

    if(room == NULL)
    {
        return;
    }

    if( room->surfaces != NULL )
    {
        struct SurfaceObjectTransform *previousTransform=NULL;
        for(int i=0; i<room->count; i++)
        {
            if(room->surfaces[i].transform!=NULL && room->surfaces[i].transform != previousTransform)
            {
                previousTransform = room->surfaces[i].transform;
                free(room->surfaces[i].transform);
            }
        }
        free(room->surfaces);
        room->surfaces = NULL;
    }

    free(room);
    s_level_rooms[roomId]=NULL;
}

void level_rooms_switch(int switchedRooms[][2], int switchedRoomsCount)
{
    for(int i=0; i<switchedRoomsCount; i++)
    {
        int src = switchedRooms[i][0];
        int dst = switchedRooms[i][1];

        #ifdef DEBUG_LEVEL_ROOMS
            printf("SM64: Switched room %d with room %d\n", src, dst);
        #endif

        // If the room we are about to unload was active in wither player we need to keep the loaded rooms updated to the new pointer
        int idx_in_loaded_room[MAX_MARIO_PLAYERS];        
        for(int i=0; i<MAX_MARIO_PLAYERS; i++)
        {
            idx_in_loaded_room[i]=-1;

            if(s_mario_loaded_rooms[i].marioId==-1){
                continue;
            }

            for(int j=0; j<s_mario_loaded_rooms[i].count; j++)
            {
                if(s_level_rooms[src] == s_mario_loaded_rooms[i].rooms[j])
                {
                    idx_in_loaded_room[i] = j;
                    break;
                }
            }
        }

        struct Room* tmp = s_level_rooms[src];
        s_level_rooms[src] = s_level_rooms[dst];

        // if the src room was activated in either player we need to switch it to the new one
        for(int i=0; i<MAX_MARIO_PLAYERS; i++)
        {
            if(idx_in_loaded_room[i]>=0)
            {
                s_mario_loaded_rooms[i].rooms[idx_in_loaded_room[i]] = s_level_rooms[src];
            }
        }

        s_level_rooms[dst] = tmp;
    }
}

#pragma endregion

#pragma region Level management

bool level_init(uint32_t roomsCount)
{
    #ifdef DEBUG_LEVEL_ROOMS
        printf("SM64: init level\n");
    #endif
    if(s_level_loaded)
    {
        printf("SM64: Aborted trying to load a level already loaded.");
        return false;
    }

    level_init_rooms(roomsCount);
    level_init_player_loaded_rooms();
    level_init_dynamic_objects();
    level_init_big_floor_hack();
    level_update_cached_object_surface_list();

    s_level_loaded = true;

    return true;
}

void level_unload()
{
    #ifdef DEBUG_LEVEL_ROOMS
        printf("SM64: unload level\n");
    #endif

    s_level_loaded=false;

    level_unload_all_rooms();
    level_unload_all_player_loaded_rooms();
    level_unload_all_dynamic_objects();
    level_unload_big_floor_hack();
}

#pragma endregion

#pragma region Player Loaded Rooms

void level_init_player_loaded_rooms()
{
    for(int i=0; i<MAX_MARIO_PLAYERS; i++)
    {
        s_mario_loaded_rooms[i].marioId=-1;
        s_mario_loaded_rooms[i].count=0;
        s_mario_loaded_rooms[i].rooms=NULL;
        s_mario_loaded_rooms[i].clippersCount=0;
    }
}

void level_load_player_loaded_rooms(int marioId)
{
    for(int i=0; i<MAX_MARIO_PLAYERS; i++)
    {
        if(s_mario_loaded_rooms[i].marioId == -1)
        {
            s_mario_loaded_rooms[i].marioId = marioId;
            s_mario_loaded_rooms[i].count=0;
            s_mario_loaded_rooms[i].rooms=(struct Room**)malloc((sizeof(struct Room*) * s_level_rooms_count));
            s_current_loaded_rooms = &(s_mario_loaded_rooms[i]);
            s_mario_loaded_rooms[i].clippersCount=0;

            return;
        }
    }
}

void level_unload_player_loaded_rooms(int marioId)
{
    for(int i=0; i<MAX_MARIO_PLAYERS; i++)
    {
        if(s_mario_loaded_rooms[i].marioId == marioId)
        {
            s_mario_loaded_rooms[i].marioId=-1;
            s_mario_loaded_rooms[i].count=0;
            s_mario_loaded_rooms[i].clippersCount=0;
            if(s_mario_loaded_rooms[i].rooms!=NULL){
                free(s_mario_loaded_rooms[i].rooms);
                s_mario_loaded_rooms[i].rooms=NULL;
            }
            s_current_loaded_rooms = NULL;
        }
    }
}

void level_unload_all_player_loaded_rooms()
{
    s_current_loaded_rooms = NULL;
    for(int i=0; i<MAX_MARIO_PLAYERS; i++)
    {
        s_mario_loaded_rooms[i].marioId=-1;
        s_mario_loaded_rooms[i].count=0;
        s_mario_loaded_rooms[i].clippersCount=0;
        if(s_mario_loaded_rooms[i].rooms!=NULL){
            free(s_mario_loaded_rooms[i].rooms);
            s_mario_loaded_rooms[i].rooms=NULL;
        }
    }
}

void level_update_player_loaded_Rooms_with_clippers(int marioId, int *newloadedRooms, int loadedCount, const struct SM64Surface clippers[MAX_CLIPPER_BLOCKS_FACES], uint32_t clippersCount)
{
    for(int i=0; i<MAX_MARIO_PLAYERS; i++)
    {
        if(s_mario_loaded_rooms[i].marioId == marioId)
        {
            struct MarioLoadedRooms *loadedRooms = &(s_mario_loaded_rooms[i]);
            if(loadedRooms->rooms == NULL || loadedCount==0)
            {
                return;
            }
            loadedRooms->count=0;
            for(uint32_t i=0; i<loadedCount; i++)
            {
                loadedRooms->rooms[loadedRooms->count++]=s_level_rooms[newloadedRooms[i]];
            }
            
            loadedRooms->clippersCount=clippersCount;
            for( uint32_t i = 0; i < clippersCount; ++i )
            {
                engine_surface_from_lib_surface( &(loadedRooms->clippers[i]), &clippers[i], NULL, EXTERNAL_SURFACE_TYPE_WALL_CLIPPER );
            }
        }
    }
}

void level_update_player_loaded_Rooms(int marioId, int *newloadedRooms, int loadedCount)
{
    level_update_player_loaded_Rooms_with_clippers(marioId, newloadedRooms, loadedCount, NULL, 0);
}

void level_set_active_mario(int marioId)
{
    if(s_current_loaded_rooms!=NULL && s_current_loaded_rooms->marioId==marioId)
    {
        return;
    }
    for(int i=0; i<MAX_MARIO_PLAYERS; i++)
    {
        if(s_mario_loaded_rooms[i].marioId==marioId)
        {
            s_current_loaded_rooms = &(s_mario_loaded_rooms[i]);
        }
    }
}

#pragma endregion

#pragma region Big Floor Hack

void level_init_big_floor_hack()
{
    s_big_floor_hack = (struct Room*) malloc(sizeof(struct Room));
    
    s_big_floor_hack->count=2;

    s_big_floor_hack->surfaces = (struct Surface*) malloc(sizeof(struct Surface)*s_big_floor_hack->count);

    for(int i=0; i<s_big_floor_hack->count; i++)
    {
        level_load_big_floor_hack(&(s_big_floor_hack->surfaces[0]));
        level_load_big_floor_hack(&(s_big_floor_hack->surfaces[1]));
    }

    level_update_big_floor_hack(0.0f, 0.0f, 0.0f);
}

void level_unload_big_floor_hack()
{
    if( s_big_floor_hack == NULL )
    {
        return;
    }

    if( s_big_floor_hack->surfaces!=NULL ) 
    {
        free(s_big_floor_hack->surfaces);
        s_big_floor_hack->surfaces=NULL;
    }

    s_big_floor_hack->count=0;

    free(s_big_floor_hack);
    s_big_floor_hack = NULL;
}

void level_load_big_floor_hack(struct Surface *surf)
{
    surf->room=-1;
    surf->isValid = 1;
    surf->force=0;
    surf->transform = NULL;
    surf->type = TERRAIN_STONE;
    surf->flags = (s8) 0;

    surf->normal.x = 0.0f;
    surf->normal.y = 1.0f;
    surf->normal.z = 0.0f;
}

void level_update_big_floor_hack(float x, float y, float z)
{
    if(s_big_floor_hack==NULL || s_big_floor_hack->surfaces==NULL)
    {
        return;
    }
    int height = y-BIG_HACK_FLOOR_HEIGHT;

    struct Surface *big_floor_hack1 = &(s_big_floor_hack->surfaces[0]);
    struct Surface *big_floor_hack2 = &(s_big_floor_hack->surfaces[1]);

    big_floor_hack1->vertex1[0] = x-BIG_HACK_FLOOR_DIMENSIONS;
    big_floor_hack1->vertex2[0] = x-BIG_HACK_FLOOR_DIMENSIONS;
    big_floor_hack1->vertex3[0] = x+BIG_HACK_FLOOR_DIMENSIONS;

    big_floor_hack2->vertex1[0] = x-BIG_HACK_FLOOR_DIMENSIONS;
    big_floor_hack2->vertex2[0] = x+BIG_HACK_FLOOR_DIMENSIONS;
    big_floor_hack2->vertex3[0] = x+BIG_HACK_FLOOR_DIMENSIONS;

    big_floor_hack1->vertex1[1] = height;
    big_floor_hack1->vertex2[1] = height;
    big_floor_hack1->vertex3[1] = height;

    big_floor_hack2->vertex1[1] = height;
    big_floor_hack2->vertex2[1] = height;
    big_floor_hack2->vertex3[1] = height;

    big_floor_hack1->vertex1[2] = z-BIG_HACK_FLOOR_DIMENSIONS;
    big_floor_hack1->vertex2[2] = z+BIG_HACK_FLOOR_DIMENSIONS;
    big_floor_hack1->vertex3[2] = z+BIG_HACK_FLOOR_DIMENSIONS;

    big_floor_hack2->vertex1[2] = z-BIG_HACK_FLOOR_DIMENSIONS;
    big_floor_hack2->vertex2[2] = z+BIG_HACK_FLOOR_DIMENSIONS;
    big_floor_hack2->vertex3[2] = z-BIG_HACK_FLOOR_DIMENSIONS;

    big_floor_hack1->originOffset = -height;
    big_floor_hack2->originOffset = -height;

    big_floor_hack1->lowerY = height;
    big_floor_hack2->upperY = height;
}

#pragma endregion

#pragma region Counts and Surfaces getters

uint32_t level_get_room_count(void)
{
    return s_current_loaded_rooms->count+2;
}

uint32_t level_get_room_surfaces_count(uint32_t roomIndex)
{
    if(roomIndex == s_current_loaded_rooms->count)
    {
        if(s_dynamic_objects){
            return s_dynamic_objects->cached_count;
        }
        return 0; //dynamic objects still weren't loaded
    }
    if(roomIndex>s_current_loaded_rooms->count)
    {
        return s_current_loaded_rooms->clippersCount;
    }

    return s_current_loaded_rooms->rooms[roomIndex]->count;
}

struct Surface *level_get_room_surface(uint32_t roomIndex, uint32_t surfaceIndex)
{
    if(roomIndex == s_current_loaded_rooms->count){
        return s_dynamic_objects->cached_surfaces[surfaceIndex];
    }
    if(roomIndex > s_current_loaded_rooms->count){
        return &(s_current_loaded_rooms->clippers[surfaceIndex]);
    }

    return &(s_current_loaded_rooms->rooms[roomIndex]->surfaces[surfaceIndex]);
}

struct Surface **level_get_all_loaded_surfaces(int *resultCount)
{
    *resultCount = 0;
    for(int i=0; i<s_current_loaded_rooms->count; i++)
    {
        *resultCount+=s_current_loaded_rooms->rooms[i]->count;
    }
    *resultCount+=s_dynamic_objects->cached_count;

    struct Surface **result=(struct Surface **)malloc(sizeof(struct Surface *)*(*resultCount));

    int idx=0;
    for(int i=0;i<s_current_loaded_rooms->count;i++)
    {
        for(int j=0; j<s_current_loaded_rooms->rooms[i]->count; j++)
        {
            result[idx++]=&(s_current_loaded_rooms->rooms[i]->surfaces[j]);
        }
    }

    for(int i=0; i<s_dynamic_objects->cached_count; i++)
    {
        result[idx++]=s_dynamic_objects->cached_surfaces[i];
    }
    return result;
}

#pragma endregion
