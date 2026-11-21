#include "Engine.h"
#include "Input.h"
#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl3.h"
#include <fstream>
#include <iostream>
#include <iomanip>

Engine::Engine() : window(nullptr), running(false) {}

Engine::~Engine() {
    SDL_DestroyWindow(window);
    SDL_Quit();
}

void Engine::Init() {
    SDL_Init(SDL_INIT_VIDEO);
    window = SDL_CreateWindow("Wary Engine", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    SDL_GLContext ctx = SDL_GL_CreateContext(window);
    renderer.Init();
    editorUI.Init(window, ctx);

    // Initial Entity
    EntityID player = registry.Create("Player");
    registry.transforms.Add(player, {0, 0, 0, 0.4f, 0.4f});
    registry.sprites.Add(player, {{1, 1, 1}, "default"});
    registry.physics.Add(player, {false, false, 0});
    selectedEntity = player;

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
            if (ev.type == SDL_MOUSEWHEEL && ev.wheel.y != 0 && editorUI.IsSceneHovered()) {
                cam.zoom *= (ev.wheel.y > 0 ? 1.1f : 0.9f);
            }
        }

        Update();
        bool saveRequested = false;
        ViewportRect viewport = editorUI.Draw(window, registry, cam, selectedEntity, undoStack, saveRequested, renderer.GetTextureList());
        int winW, winH;
        SDL_GetWindowSize(window, &winW, &winH);
        renderer.Render(registry, particles, cam, viewport, winH);
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(window);
        if (saveRequested) SaveScene();
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
    if (selectedEntity != EditorUI::kNoSelection) {
        Transform& t = registry.transforms.Get(selectedEntity);
        float s = 0.02f / cam.zoom;
        float pdx = 0, pdy = 0;

        if (Input::IsKeyDown(SDL_SCANCODE_W)) pdy += s;
        if (Input::IsKeyDown(SDL_SCANCODE_S)) pdy -= s;
        if (Input::IsKeyDown(SDL_SCANCODE_A)) pdx -= s;
        if (Input::IsKeyDown(SDL_SCANCODE_D)) pdx += s;

        // Try X Movement
        t.x += pdx;
        bool collX = false;
        for (EntityID otherId : registry.Entities()) {
            if (otherId == selectedEntity || !registry.Active(otherId) || !registry.physics.Get(otherId).isStatic) continue;
            if (CheckCollision(t, registry.transforms.Get(otherId))) {
                collX = true;
                break;
            }
        }
        if (collX) t.x -= pdx; // Revert X

        // Try Y Movement
        t.y += pdy;
        bool collY = false;
        for (EntityID otherId : registry.Entities()) {
            if (otherId == selectedEntity || !registry.Active(otherId) || !registry.physics.Get(otherId).isStatic) continue;
            if (CheckCollision(t, registry.transforms.Get(otherId))) {
                collY = true;
                break;
            }
        }
        if (collY) t.y -= pdy; // Revert Y

        if (Input::IsKeyDown(SDL_SCANCODE_SPACE)) {
            // Spawn Particles
             for (int k = 0; k < 2; k++)
                particles.push_back({registry.transforms.Get(selectedEntity).x, registry.transforms.Get(selectedEntity).y,
                                     (float)(rand() % 100 - 50) / 1000, (float)(rand() % 100 - 50) / 1000,
                                     1.0f, {1, 0.9f, 0.1f}});
        }
    }

    // Physics
    for (EntityID id : registry.Entities()) {
        Physics& p = registry.physics.Get(id);
        if (p.hasGravity && !p.isStatic) {
            Transform& t = registry.transforms.Get(id);
            p.vy -= 0.001f;
            t.y += p.vy;

            // Ground Collision
            for (EntityID otherId : registry.Entities()) {
                if (otherId == id || !registry.Active(otherId) || !registry.physics.Get(otherId).isStatic) continue;
                if (CheckCollision(t, registry.transforms.Get(otherId))) {
                    // Simple response: move back and stop
                    t.y -= p.vy;
                    p.vy = 0;
                }
            }

            if (t.y < -0.8f) { // Floor default
                t.y = -0.8f;
                p.vy *= -0.5f;
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
    if (!f.is_open()) {
        std::cerr << "SaveScene: could not open scene.wary for writing\n";
        return;
    }
    for (EntityID id : registry.Entities()) {
        const Transform& t = registry.transforms.Get(id);
        const Sprite& s = registry.sprites.Get(id);
        const Physics& p = registry.physics.Get(id);
        f << std::quoted(registry.Name(id)) << " " << t.x << " " << t.y << " " << t.rotation << " "
          << t.sx << " " << t.sy << " " << s.color[0] << " " << s.color[1] << " " << s.color[2]
          << " " << p.hasGravity << " " << p.isStatic << " " << std::quoted(s.textureName) << "\n";
    }
}

void Engine::LoadScene() {
    std::ifstream f("scene.wary");
    if (!f.is_open()) return;
    registry.Restore({}); // empty snapshot == clear the registry
    std::string name;
    float x, y, r, sx, sy, c0, c1, c2;
    int g, st;
    std::string tName;
    while (f >> std::quoted(name) >> x >> y >> r >> sx >> sy >> c0 >> c1 >> c2 >> g >> st >> std::quoted(tName)) {
        EntityID id = registry.Create(name);
        registry.transforms.Add(id, {x, y, r, sx, sy});
        registry.sprites.Add(id, {{c0, c1, c2}, tName});
        registry.physics.Add(id, {(bool)g, (bool)st, 0});
    }
    // Every EntityID was just reassigned: a structural change clears selection (spec 1a).
    selectedEntity = EditorUI::kNoSelection;
}

bool Engine::CheckCollision(const Transform &a, const Transform &b) {
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
        registry.Restore(undoStack.back());
        undoStack.pop_back();
        selectedEntity = EditorUI::kNoSelection;
    }
}
