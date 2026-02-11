#include "Engine.h"
#include "Input.h"
#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include <fstream>
#include <iostream>

Engine::Engine() : window(nullptr), running(false), selectedEntity(0) {}

Engine::~Engine() {
    SDL_DestroyWindow(window);
    SDL_Quit();
}

void Engine::Init() {
    SDL_Init(SDL_INIT_VIDEO);
    window = SDL_CreateWindow("Wary Engine", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    renderer.Init(window);

    // Initial Entity
    entities.push_back({"Player", 0, 0, 0, 0.4f, 0.4f, {1, 1, 1}, "default", true, false, 0});
    
    // Attempt to load scene if exists
    LoadScene();
}

void Engine::Run() {
    running = true;
    SDL_Event ev;

    while (running) {
        // Event Loop
        while (SDL_PollEvent(&ev)) {
            ImGui_ImplSDL2_ProcessEvent(&ev);
            if (ev.type == SDL_QUIT) running = false;
            Input::Update(ev);

            // Zoom
            if (ImGui::GetIO().MouseWheel != 0) {
                cam.zoom *= (ImGui::GetIO().MouseWheel > 0 ? 1.1f : 0.9f);
            }
        }

        Update();
        renderer.Render(window, entities, particles, cam, selectedEntity, undoStack);
    }
}

void Engine::Update() {
    // Camera Pan
    if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {
        ImVec2 delta = ImGui::GetIO().MouseDelta;
        cam.x -= delta.x / 500.0f / cam.zoom;
        cam.y += delta.y / 500.0f / cam.zoom;
    }

    // Undo
    static bool zPressed = false;
    if (Input::IsKeyDown(SDL_SCANCODE_LCTRL) && Input::IsKeyDown(SDL_SCANCODE_Z)) {
        if (!zPressed) {
            Undo();
            zPressed = true;
        }
    } else {
        zPressed = false;
    }

    // Entity Control
    if (selectedEntity >= 0 && selectedEntity < (int)entities.size()) {
        Entity &e = entities[selectedEntity];
        float s = 0.02f / cam.zoom;
        float pdx = 0, pdy = 0;

        if (Input::IsKeyDown(SDL_SCANCODE_W)) pdy += s;
        if (Input::IsKeyDown(SDL_SCANCODE_S)) pdy -= s;
        if (Input::IsKeyDown(SDL_SCANCODE_A)) pdx -= s;
        if (Input::IsKeyDown(SDL_SCANCODE_D)) pdx += s;

        // Try X Movement
        e.x += pdx;
        bool collX = false;
        for (const auto &other : entities) {
            if (&e == &other || !other.active || !other.isStatic) continue;
            if (CheckCollision(e, other)) {
                collX = true;
                break;
            }
        }
        if (collX) e.x -= pdx; // Revert X

        // Try Y Movement
        e.y += pdy;
        bool collY = false;
        for (const auto &other : entities) {
            if (&e == &other || !other.active || !other.isStatic) continue;
            if (CheckCollision(e, other)) {
                collY = true;
                break;
            }
        }
        if (collY) e.y -= pdy; // Revert Y
        
        if (Input::IsKeyDown(SDL_SCANCODE_SPACE)) {
            // Spawn Particles
             for (int k = 0; k < 2; k++)
                particles.push_back({entities[selectedEntity].x, entities[selectedEntity].y, 
                                     (float)(rand() % 100 - 50) / 1000, (float)(rand() % 100 - 50) / 1000, 
                                     1.0f, {1, 0.9f, 0.1f}});
        }
    }

    // Physics
    for (auto &e : entities) {
        if (e.hasGravity && !e.isStatic) {
            e.vy -= 0.001f;
            e.y += e.vy;
            
            // Ground Collision
            for (const auto &other : entities) {
                if (&e == &other || !other.active || !other.isStatic) continue;
                if (CheckCollision(e, other)) {
                    // Simple response: move back and stop
                    e.y -= e.vy;
                    e.vy = 0;
                }
            }

            if (e.y < -0.8f) { // Floor default
                e.y = -0.8f;
                e.vy *= -0.5f;
            }
        }
    }

    // Particles
    for (auto it = particles.begin(); it != particles.end();) {
        it->x += it->vx;
        it->y += it->vy;
        it->life -= 0.015f;
        if (it->life <= 0) it = particles.erase(it);
        else ++it;
    }
}

void Engine::SaveScene() {
    std::ofstream f("scene.wary");
    for (auto &e : entities) {
        f << e.name << " " << e.x << " " << e.y << " " << e.rotation << " "
          << e.sx << " " << e.sy << " " << e.color[0] << " " << e.color[1] << " " << e.color[2]
          << " " << e.hasGravity << " " << e.isStatic << " " << e.textureName << "\n";
    }
}

void Engine::LoadScene() {
    std::ifstream f("scene.wary");
    if (!f.is_open()) return;
    entities.clear();
    char name[64];
    float x, y, r, sx, sy, c0, c1, c2;
    int g, st;
    std::string tName;
    while (f >> name >> x >> y >> r >> sx >> sy >> c0 >> c1 >> c2 >> g >> st >> tName) {
        entities.push_back({name, x, y, r, sx, sy, {c0, c1, c2}, tName, true, (bool)g, (bool)st, 0});
    }
}

bool Engine::CheckCollision(const Entity &a, const Entity &b) {
    // Simple AABB
    float halfAX = a.sx / 2.0f;
    float halfAY = a.sy / 2.0f;
    float halfBX = b.sx / 2.0f;
    float halfBY = b.sy / 2.0f;
    bool xOverlap = (a.x - halfAX < b.x + halfBX) && (a.x + halfAX > b.x - halfBX);
    bool yOverlap = (a.y - halfAY < b.y + halfBY) && (a.y + halfAY > b.y - halfBY);
    return xOverlap && yOverlap;
}

void Engine::Undo() {
    if (!undoStack.empty()) {
        entities = undoStack.back();
        undoStack.pop_back();
    }
}
