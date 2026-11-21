#ifndef ENGINE_H
#define ENGINE_H

#include <SDL.h>
#include <vector>
#include "Entity.h"
#include "Registry.h"
#include "Renderer.h"
#include "EditorUI.h"

class Engine {
public:
    Engine();
    ~Engine();

    void Init();
    void Run();

private:
    SDL_Window* window;
    Renderer renderer;
    EditorUI editorUI;
    bool running;
    
    Registry registry;
    std::vector<Particle> particles;
    Camera cam;
    EntityID selectedEntity = EditorUI::kNoSelection; // set to the Player in Init()

    void Update();
    void SaveScene();
    void LoadScene();
    void Undo(); // Ctrl+Z
    bool CheckCollision(const Transform &a, const Transform &b);

    std::vector<std::vector<EntitySnapshot>> undoStack;
};

#endif
