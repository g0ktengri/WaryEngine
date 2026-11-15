#ifndef EDITOR_UI_H
#define EDITOR_UI_H

#include <SDL.h>

class EditorUI {
public:
    EditorUI();
    ~EditorUI();

    void Init(SDL_Window* window, SDL_GLContext ctx);

private:
    void SetupUIStyle();
};

#endif
