#pragma once

#include "decomp/include/types.h"
#include "decomp/include/external_types.h"
#include "libsm64.h"

extern void level_init(uint32_t roomsCount);
extern void level_unload();
extern void level_load_room(uint32_t roomId, const struct SM64Surface *staticSurfaces, uint32_t numSurfaces, const struct SM64SurfaceObject *staticObjects, uint32_t staticObjectsCount);
extern void level_unload_room(uint32_t roomId);
extern void level_update_loaded_rooms_list(int *loadedRooms, int loadedCount);
extern void level_rooms_switch(int switchedRooms[][2], int switchedRoomsCount);

extern uint32_t level_create_dynamic_object( const struct SM64SurfaceObject *surfaceObject );
extern void level_remove_dynamic_object( uint32_t objId );
extern struct SurfaceObjectTransform *level_get_dynamic_object_transform( uint32_t objId );
extern void level_update_dynamic_object_transform( uint32_t objId, const struct SM64ObjectTransform *newTransform );

extern uint32_t level_get_all_surface_group_count(void);
extern uint32_t level_get_all_surface_group_size(uint32_t groupIndex);
extern struct Surface *level_get_surface_index(uint32_t groupIndex, uint32_t surfaceIndex);