#define SDL_MAIN_HANDLED
#include "Engine.h"

int main(int argc, char **argv) {
    Engine engine;
    engine.Init();
    engine.Run();
    return 0;
}
