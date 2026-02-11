#ifndef ENTITY_H
#define ENTITY_H

#include <string>
#include <vector>

struct Entity {
    std::string name;
    float x, y;
    float rotation;
    float sx, sy;
    float color[3];
    std::string textureName;
    bool active;
    bool hasGravity;
    bool isStatic;
    float vy;
};

struct Particle {
    float x, y, vx, vy, life;
    float color[3];
};

struct Camera {
    float x = 0, y = 0, zoom = 1.0f;
};

#endif
