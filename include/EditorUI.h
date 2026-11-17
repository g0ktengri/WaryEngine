#ifndef EDITOR_UI_H
#define EDITOR_UI_H

#include <SDL.h>
#include <vector>
#include <string>
#include "Entity.h"
#include "Gizmo.h"

class EditorUI {
public:
    EditorUI();
    ~EditorUI();

    void Init(SDL_Window* window, SDL_GLContext ctx);
    ViewportRect Draw(SDL_Window* window, std::vector<Entity>& entities, const Camera& cam,
                       int& selected, std::vector<std::vector<Entity>>& undoStack,
                       bool& saveRequested, const std::vector<std::string>& textureList);

private:
    void SetupUIStyle();
    bool CheckPointInside(Entity& e, float px, float py);

    bool wasActive = false;
    std::vector<Entity> preEditState;
    Gizmo gizmo;
};

#endif
