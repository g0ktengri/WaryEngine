#ifndef INPUT_H
#define INPUT_H

#include <SDL.h>

class Input {
public:
  static bool keys[256];
  static int mouseX, mouseY;
  static bool mouseLeft, mouseRight;

  static void Update(SDL_Event &e) {
    if (e.type == SDL_KEYDOWN) {
      keys[e.key.keysym.scancode] = true;
    }
    if (e.type == SDL_KEYUP) {
      keys[e.key.keysym.scancode] = false;
    }
    if (e.type == SDL_MOUSEMOTION) {
      mouseX = e.motion.x;
      mouseY = e.motion.y;
    }
    if (e.type == SDL_MOUSEBUTTONDOWN) {
      if (e.button.button == SDL_BUTTON_LEFT)
        mouseLeft = true;
      if (e.button.button == SDL_BUTTON_RIGHT)
        mouseRight = true;
    }
    if (e.type == SDL_MOUSEBUTTONUP) {
      if (e.button.button == SDL_BUTTON_LEFT)
        mouseLeft = false;
      if (e.button.button == SDL_BUTTON_RIGHT)
        mouseRight = false;
    }
  }

  static bool IsKeyDown(SDL_Scancode key) { return keys[key]; }
};



#endif
