#ifndef EXTERNAL_TYPES_H
#define EXTERNAL_TYPES_H

#include <stdint.h>
#include <stdbool.h>

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
    EXTERNAL_SURFACE_TYPE_FLOOR_HACK
};

struct SM64DebugSurface
{
    float v1[3];
    float v2[3];
    float v3[3];
    //float normaly;
    //uintptr_t surfacePointer;
    enum SM64ExternalSurfaceTypes color;
    bool valid;
};

#endif