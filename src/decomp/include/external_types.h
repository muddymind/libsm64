#ifndef EXTERNAL_TYPES_H
#define EXTERNAL_TYPES_H

#include <stdint.h>

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

#endif