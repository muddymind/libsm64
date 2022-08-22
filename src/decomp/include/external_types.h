#ifndef EXTERNAL_TYPES_H
#define EXTERNAL_TYPES_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief The max number of clipper blocks in range. We probably won't find more than this activated at once.
 * (famous last words) 
 */
#define MAX_CLIPPER_BLOCKS 200
/**
 * @brief each clipper block is a cube made of triangles
 */
#define MAX_CLIPPER_BLOCKS_FACES 6*2*MAX_CLIPPER_BLOCKS
/**
 * @brief each block replaces 2 faces overlapping. 
 */
#define MAX_CLIPPED_FACES 2*MAX_CLIPPER_BLOCKS

struct SM64Surface
{
    int16_t type;
    int16_t force;
    uint16_t terrain;
    int32_t vertices[3][3];
    int roomId;
    int faceId;
};

struct SM64ObjectTransform
{
    float position[3];
    float eulerRotation[3];
};

struct SM64SurfaceObject
{
    struct SM64ObjectTransform transform;
    uint32_t surfaceCount;
    struct SM64Surface *surfaces;
};

enum SM64ExternalSurfaceTypes
{
    EXTERNAL_SURFACE_TYPE_STATIC_SURFACE,
    EXTERNAL_SURFACE_TYPE_STATIC_MESH,
    EXTERNAL_SURFACE_TYPE_DYNAMIC_OBJECT,
    EXTERNAL_SURFACE_TYPE_FLOOR_HACK,
    EXTERNAL_SURFACE_TYPE_WALL_CLIPPER
};

struct SM64DebugSurface
{
    float v1[3];
    float v2[3];
    float v3[3];
    enum SM64ExternalSurfaceTypes color;
    bool valid;
};



#endif