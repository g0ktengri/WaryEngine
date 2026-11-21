#ifndef COMPONENTS_H
#define COMPONENTS_H
#include <string>

struct Transform { float x = 0, y = 0, rotation = 0, sx = 1, sy = 1; };
struct Sprite     { float color[3] = {1,1,1}; std::string textureName = "default"; };
struct Physics    { bool hasGravity = false, isStatic = false; float vy = 0; };

#endif
