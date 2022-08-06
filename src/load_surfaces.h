#pragma once

#include "decomp/include/types.h"
#include "libsm64.h"

// extern uint32_t loaded_surface_iter_group_count( void );
// extern uint32_t loaded_surface_iter_group_size( uint32_t groupIndex );
// extern struct Surface *loaded_surface_iter_get_at_index( uint32_t groupIndex, uint32_t surfaceIndex );

// extern void surfaces_load_static( const struct SM64Surface *surfaceArray, uint32_t numSurfaces );
// extern uint32_t surfaces_load_object( const struct SM64SurfaceObject *surfaceObject );
// extern void surface_object_update_transform( uint32_t objId, const struct SM64ObjectTransform *newTransform );
// extern struct SurfaceObjectTransform *surfaces_object_get_transform_ptr( uint32_t objId );
// extern void surfaces_unload_object( uint32_t objId );
// extern void surfaces_unload_all( void );

extern void level_init(uint32_t roomsCount);
extern void level_unload();
extern void level_load_room(uint32_t roomId, const struct SM64Surface *staticSurfaces, uint32_t numSurfaces);
extern void level_unload_room(uint32_t roomId);

extern uint32_t level_get_all_surface_group_count(void);
extern uint32_t level_get_all_surface_group_size(uint32_t groupIndex);
extern struct Surface *level_get_surface_index(uint32_t groupIndex, uint32_t surfaceIndex);