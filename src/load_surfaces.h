#pragma once

#include "decomp/include/types.h"
#include "decomp/include/external_types.h"
#include "libsm64.h"


struct LoadedSurfaceObject
{
    struct SurfaceObjectTransform *transform;
    uint32_t surfaceCount;
    struct SM64Surface *libSurfaces;
    struct Surface *engineSurfaces;    
};

struct Room
{
    struct Surface *surfaces;
    uint32_t count;
};

struct MarioLoadedRooms
{
    int32_t marioId;
    struct Room **rooms;
    uint32_t count;
};

struct DynamicObjects
{
    struct LoadedSurfaceObject *objects;
    uint32_t objectsCount;

    struct Surface **cached_surfaces;
    uint32_t cached_count;
};

extern bool level_init(uint32_t roomsCount);
extern void level_unload();
extern void level_set_active_mario(int marioId);

extern void level_load_room(uint32_t roomId, const struct SM64Surface *staticSurfaces, uint32_t numSurfaces, const struct SM64SurfaceObject *staticObjects, uint32_t staticObjectsCount);
extern void level_rooms_switch(int switchedRooms[][2], int switchedRoomsCount);

extern void level_load_player_loaded_rooms(int marioId);
extern void level_unload_player_loaded_rooms(int marioId);
extern void level_update_player_loaded_Rooms(int marioId, int *newloadedRooms, int loadedCount);

extern uint32_t level_load_dynamic_object( const struct SM64SurfaceObject *surfaceObject );
extern void level_unload_dynamic_object( uint32_t objId, bool update_cache );
extern void level_update_dynamic_object_transform( uint32_t objId, const struct SM64ObjectTransform *newTransform );
extern struct SurfaceObjectTransform *level_get_dynamic_object_transform( uint32_t objId );

/**
 * @brief Gets the number of activated rooms for the current Mario +1 for the dynamic objects.
 * 
 * @return uint32_t
 */
extern uint32_t level_get_room_count(void);
/**
 * @brief Gets the number of surfaces contained by the given selected activated room.
 * If the roomIndex is the last one it will correspond to the dynamic objects surfaces.
 * 
 * @param roomIndex activated room index from which to get the number of surfaces.
 * @return uint32_t
 */
extern uint32_t level_get_room_surfaces_count(uint32_t roomIndex);
/**
 * @brief Gets a surface from the given selected activated room.
 * If the roomIndex is the last one it will correspond to the dynamic objects surfaces.
 * 
 * @param roomIndex activated room index from which to get the surface.
 * @param surfaceIndex surface index to get from the given activated room.
 * @return struct Surface*
 */
extern struct Surface *level_get_room_surface(uint32_t roomIndex, uint32_t surfaceIndex);

extern struct Surface **level_get_all_loaded_surfaces(int *resultCount);