#ifndef ENGINE_H
#define ENGINE_H

#include <SDL.h>
#include <vector>
#include "Entity.h"
#include "Renderer.h"

class Engine {
public:
    Engine();
    ~Engine();

    void Init();
    void Run();

private:
    SDL_Window* window;
    Renderer renderer;
    bool running;
    
    std::vector<Entity> entities;
    std::vector<Particle> particles;
    Camera cam;
    int selectedEntity;

    void Update();
    void SaveScene();
    void LoadScene();
    void Undo(); // Ctrl+Z
    bool CheckCollision(const Entity &a, const Entity &b);

    std::vector<std::vector<Entity>> undoStack;
};

#endif
