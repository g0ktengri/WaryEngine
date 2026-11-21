#ifndef ENTITY_H
#define ENTITY_H

#include <string>
#include <vector>

struct Particle {
    float x, y, vx, vy, life;
    float color[3];
};

struct Camera {
    float x = 0, y = 0, zoom = 1.0f;
};

struct ViewportRect {
    float x, y, width, height;
};

#endif
