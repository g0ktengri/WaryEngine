#ifndef EDITOR_UI_H
#define EDITOR_UI_H

#include <SDL.h>
#include <vector>
#include <string>
#include "Registry.h"
#include "Gizmo.h"

class EditorUI {
public:
    EditorUI();
    ~EditorUI();

    void Init(SDL_Window* window, SDL_GLContext ctx);
    ViewportRect Draw(SDL_Window* window, Registry& registry, const Camera& cam,
                       EntityID& selected, std::vector<std::vector<EntitySnapshot>>& undoStack,
                       bool& saveRequested, const std::vector<std::string>& textureList);

    bool IsSceneHovered() const { return lastSceneHovered; }

private:
    void SetupUIStyle();
    bool CheckPointInside(const Transform& t, float px, float py);

    bool wasActive = false;
    std::vector<EntitySnapshot> preEditState;
    Gizmo gizmo;
    Uint32 lastFrameTicks = 0;
    float fpsSmoothed = 0.0f;
    bool lastSceneHovered = false;
};

#endif
