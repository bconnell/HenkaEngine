#ifndef HENKA_CORE_H
#define HENKA_CORE_H

#define HENKA_VERSION_MAJOR 0
#define HENKA_VERSION_MINOR 1
#define HENKA_VERSION_PATCH 0

#define HENKA_PI 3.14159265358979323846f
#define HENKA_DEG_TO_RAD (HENKA_PI / 180.0f)
#define HENKA_RAD_TO_DEG (180.0f / HENKA_PI)

typedef struct henka_viewport
{
    int x;
    int y;
    int width;
    int height;
} henka_viewport;

#endif
