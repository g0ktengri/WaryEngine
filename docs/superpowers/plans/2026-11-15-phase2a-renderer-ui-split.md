# Wary Engine — Faz 2a: Renderer / Editor UI Split Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Split `Renderer` (currently OpenGL drawing + all ImGui editor UI + picking + undo-capture, all in one `Render()` function) into a `Renderer` that only does OpenGL setup/drawing and a new `EditorUI` class that owns ImGui lifecycle, the Hierarchy/Inspector/Tips panels, mouse picking, and undo-capture — with zero behavior change.

**Architecture:** `Engine` creates the SDL GL context once and initializes both `Renderer` and `EditorUI` with it. Each frame, `Engine::Run` calls `editorUI.Draw(...)` (builds all ImGui panels, does picking, returns the computed scene viewport rect), then `renderer.Render(...)` (pure GL: clear, grid, entities, particles), then finishes the frame itself (`ImGui::Render()`, `RenderDrawData`, `SDL_GL_SwapWindow`).

**Tech Stack:** C++20, SDL2, OpenGL 3.3 (glad), Dear ImGui, CMake/MinGW — unchanged.

**Spec:** [docs/specs/2026-11-15-phase2a-renderer-ui-split-design.md](../specs/2026-11-15-phase2a-renderer-ui-split-design.md), which implements the "Renderer/UI ayrımı" slice of [docs/specs/2026-11-15-wary-engine-levelup-design.md](../specs/2026-11-15-wary-engine-levelup-design.md)'s Faz 2.

## Global Constraints

- No new dependency; existing CMake `file(GLOB_RECURSE ...)` picks up new `.cpp`/`.h` files automatically — no CMakeLists.txt edit needed.
- **Zero behavior change.** Every click, drag, shortcut, and visual result must match pre-refactor behavior exactly. This is a pure refactor, not a feature change.
- `bool& saveRequested` and `std::vector<std::vector<Entity>>& undoStack` keep their names and meaning (Faz 1 interfaces).
- No automated test framework (same as Faz 1) — verification is manual: build, then run the real exe and check against an explicit behavior checklist.

## Review Focus

1. **Mouse picking must still select the right entity.** Picking math (screen → NDC → world) and the returned `ViewportRect` are now computed in `EditorUI::Draw`, while `Renderer::Render` independently uses that same `ViewportRect` for its own aspect-ratio math. If the two ever disagree, clicks would select the wrong entity or render would look skewed relative to where clicks land. → Task 2's verification clicks entities at different screen positions and confirms selection matches the visual position.
2. **Undo (Ctrl+Z) must still work** after `wasActive`/`preEditState` move from function-local `static` variables in `Renderer::Render` to instance members of `EditorUI` (same single-instance lifetime, but worth confirming nothing resets between frames). → Task 2's verification edits an Inspector field, then Ctrl+Z, confirms revert.
3. **Save / "+ New Entity" / "DELETE ENTITY" must still mutate the right state** now that their button handlers live in `EditorUI::Draw` instead of `Renderer::Render`, operating on references passed in from `Engine::Run`. → Task 2's verification exercises all three and checks `scene.wary` / the Hierarchy list.
4. **Frame composition order**: `ImGui::Render()` / `RenderDrawData` / `SDL_GL_SwapWindow` move from inside `Renderer::Render` to `Engine::Run`, after both `editorUI.Draw` and `renderer.Render` are called. Wrong order would make panels disappear, flicker, or render behind the scene. → Task 2's verification confirms panels are visible and correctly on top of the 3D scene, exactly as before.
5. **Camera pan/zoom and WASD movement** (computed in `Engine::Update`, unrelated to this refactor) must still visibly work once `cam` and `selectedEntity` flow through two separate objects (`EditorUI` and `Renderer`) instead of one. → Task 2's verification pans/zooms the camera and moves the selected entity with WASD.

---

### Task 1: `EditorUI` class skeleton — move ImGui lifecycle only

**Files:**
- Create: `include/EditorUI.h`, `src/EditorUI.cpp`
- Modify: `include/Renderer.h:17` (`Init` signature), `src/Renderer.cpp:103-132` (`~Renderer`, `Init`)
- Modify: `include/Engine.h:7,19` (add `#include "EditorUI.h"`, add `EditorUI editorUI;` member)
- Modify: `src/Engine.cpp:16-26` (`Engine::Init`)

**Interfaces:**
- Consumes: nothing from other Faz 2a tasks.
- Produces: `EditorUI::Init(SDL_Window* window, SDL_GLContext ctx)` and `EditorUI::~EditorUI()` — Task 2 adds `EditorUI::Draw(...)` to this same class, so the class must already exist with its constructor/destructor doing the right thing.
- Produces: `Renderer::Init()` (no parameters) — Task 2's Engine orchestration changes call this.

- [ ] **Step 1: Create `include/EditorUI.h`** with:
  ```cpp
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
  ```
  (Task 2 adds `Draw(...)`, the `ViewportRect` struct, and more private members/methods to this same file — don't build those yet.)

- [ ] **Step 2: Create `src/EditorUI.cpp`** with the constructor/destructor/Init body moved verbatim from `Renderer`'s current `Renderer()`, `~Renderer()` (just the ImGui shutdown lines), and `Init` (just the ImGui setup lines):
  ```cpp
  #include "EditorUI.h"
  #include "imgui.h"
  #include "imgui_impl_sdl2.h"
  #include "imgui_impl_opengl3.h"
  #include <filesystem>

  EditorUI::EditorUI() {}

  EditorUI::~EditorUI() {
      ImGui_ImplOpenGL3_Shutdown();
      ImGui_ImplSDL2_Shutdown();
      ImGui::DestroyContext();
  }

  void EditorUI::Init(SDL_Window* window, SDL_GLContext ctx) {
      IMGUI_CHECKVERSION();
      ImGui::CreateContext();
      ImGuiIO& io = ImGui::GetIO(); (void)io;
      if (std::filesystem::exists("C:/Windows/Fonts/segoeui.ttf")) {
          io.Fonts->AddFontFromFileTTF("C:/Windows/Fonts/segoeui.ttf", 20.0f);
      }
      SetupUIStyle();
      ImGui_ImplSDL2_InitForOpenGL(window, ctx);
      ImGui_ImplOpenGL3_Init("#version 330");
  }
  ```
  Then copy `Renderer::SetupUIStyle`'s full body (`src/Renderer.cpp:437-501`) into `EditorUI::SetupUIStyle` in this file, unchanged.

- [ ] **Step 3: Remove the moved code from `Renderer`** — in `src/Renderer.cpp`:
  - `Renderer::~Renderer()` (currently lines 103-107): delete the three ImGui shutdown lines, leave an empty body `Renderer::~Renderer() {}`.
  - `Renderer::Init` (currently lines 109-132): delete `ctx = SDL_GL_CreateContext(window)` — the context now comes in as a parameter (next step) — and delete the entire ImGui setup block (`IMGUI_CHECKVERSION()` through `ImGui_ImplOpenGL3_Init(...)`), keeping `gladLoadGLLoader`, `glEnable(GL_BLEND)`, `glBlendFunc(...)`, `InitShader()`, `InitBuffers()`, `InitInfiniteGrid()`, `RefreshTextures()`.
  - Change the signature to `void Renderer::Init() {` (no parameters — the GL context is already current by the time `Engine::Init` calls this; see Step 5) and call `gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress);` as the first line (SDL_GL_GetProcAddress doesn't need the context object itself).
  - Delete `Renderer::SetupUIStyle`'s body from `Renderer.cpp` entirely (it's now in `EditorUI.cpp`); remove its declaration from `include/Renderer.h`.

- [ ] **Step 4: Update `include/Renderer.h:17`** to `void Init();` (drop the `SDL_Window* window` parameter).

- [ ] **Step 5: Wire `Engine::Init`** (`src/Engine.cpp:16-26`) to create the context itself and initialize both objects:
  ```cpp
  void Engine::Init() {
      SDL_Init(SDL_INIT_VIDEO);
      window = SDL_CreateWindow("Wary Engine", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
      SDL_GLContext ctx = SDL_GL_CreateContext(window);
      renderer.Init();
      editorUI.Init(window, ctx);

      entities.push_back({"Player", 0, 0, 0, 0.4f, 0.4f, {1, 1, 1}, "default", true, false, 0});
      LoadScene();
  }
  ```

- [ ] **Step 6: Add the `EditorUI` member to `Engine`** — in `include/Engine.h`, add `#include "EditorUI.h"` near the existing includes and add `EditorUI editorUI;` as a private member alongside `Renderer renderer;` (`include/Engine.h:19`).

- [ ] **Step 7: Build**

  Run: `cmake --build build`
  Expected: builds with no errors. (The app's behavior is unchanged by this task — `Renderer::Render` still does everything it did before; only *initialization* moved.)

- [ ] **Step 8: Manual verification — app still starts and looks identical**

  Launch `build/bin/WaryEngine.exe` (from `build/bin`). Confirm: the window opens, the grid and any saved entities render, the Hierarchy/Inspector/Tips panels appear and look the same as before this task (same fonts, same colors/style). This proves ImGui's context/style/font/backend init still happens correctly even though it now runs inside `EditorUI::Init` instead of `Renderer::Init`.

- [ ] **Step 9: Commit**

  ```bash
  git add include/EditorUI.h src/EditorUI.cpp include/Renderer.h src/Renderer.cpp include/Engine.h src/Engine.cpp
  git commit -m "refactor: extract EditorUI class, move ImGui lifecycle out of Renderer"
  ```

---

### Task 2: Move panels, picking, and undo-capture into `EditorUI::Draw`; slim `Renderer::Render` to scene-only

**Files:**
- Modify: `include/EditorUI.h` (add `ViewportRect` struct, `Draw(...)` declaration, private members/methods)
- Modify: `src/EditorUI.cpp` (add `Draw(...)` body, `CheckPointInside`)
- Modify: `include/Renderer.h:18,46` (`Render` signature, remove `CheckPointInside` declaration)
- Modify: `src/Renderer.cpp:244-433` (remove `CheckPointInside`, slim `Render`)
- Modify: `src/Engine.cpp:28-50` (`Engine::Run` orchestration)

**Interfaces:**
- Consumes: `EditorUI::Init`/`~EditorUI` from Task 1 (unchanged); `Renderer::Init()` from Task 1 (unchanged).
- Produces: `ViewportRect { float x, width, height; }` (defined in `include/EditorUI.h`) — consumed by `Renderer::Render`, so `Renderer.h` must `#include "EditorUI.h"`.
- Produces: `EditorUI::Draw(SDL_Window* window, std::vector<Entity>& entities, const Camera& cam, int& selected, std::vector<std::vector<Entity>>& undoStack, bool& saveRequested, const std::vector<std::string>& textureList) -> ViewportRect`.
- Produces: `Renderer::Render(std::vector<Entity>& entities, std::vector<Particle>& particles, const Camera& cam, const ViewportRect& viewport)` — no `window`, no `selected`/`undoStack`/`saveRequested` (those are `EditorUI`'s now).

- [ ] **Step 1: Add to `include/EditorUI.h`** — a `ViewportRect` struct above the class, and extend the class:
  ```cpp
  #include <vector>
  #include <string>
  #include "Entity.h"

  struct ViewportRect {
      float x, width, height;
  };

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
  };
  ```

- [ ] **Step 2: Add `EditorUI::CheckPointInside`** to `src/EditorUI.cpp`, moved verbatim from `Renderer::CheckPointInside` (`src/Renderer.cpp:244-248`):
  ```cpp
  bool EditorUI::CheckPointInside(Entity &e, float px, float py) {
      float halfX = e.sx / 2.0f;
      float halfY = e.sy / 2.0f;
      return (px > e.x - halfX && px < e.x + halfX && py > e.y - halfY && py < e.y + halfY);
  }
  ```

- [ ] **Step 3: Add `EditorUI::Draw`** to `src/EditorUI.cpp`, moved from `Renderer::Render` (`src/Renderer.cpp:250-379`, i.e. everything from the start of the function through the end of the Tips panel's `ImGui::End()`), with these changes:
  - Drop the `ImGui_ImplOpenGL3_NewFrame()`/`ImGui_ImplSDL2_NewFrame()`/`ImGui::NewFrame()` calls at the top — keep them (they still belong here, at the start of the UI-building phase of the frame).
  - Replace `static bool wasActive = false;` / `static std::vector<Entity> preEditState;` with reads/writes of the `wasActive/preEditState` member variables (drop `static`, drop the local declarations — they're member fields now).
  - Use `this->CheckPointInside(...)` (or just `CheckPointInside(...)`, now resolving to the member) in place of the old call.
  - At the end, instead of falling through into scene rendering, `return ViewportRect{sceneX, sceneW, sceneH};`.
  - Everything else (Hierarchy panel, Inspector panel including the Save/`+ New Entity`/`DELETE ENTITY` button bodies, Tips panel, the picking block) is copied unchanged — same ImGui calls, same field names, same button bodies.

- [ ] **Step 4: Slim `Renderer::Render`** in `src/Renderer.cpp` — replace the whole function (currently lines 250-433) with just the "--- Scene Render ---" section (currently lines 381-429), adjusted:
  ```cpp
  void Renderer::Render(std::vector<Entity>& entities, std::vector<Particle>& particles, const Camera& cam, const ViewportRect& viewport) {
      glClearColor(0.12f, 0.12f, 0.15f, 1.0f);
      glClear(GL_COLOR_BUFFER_BIT);

      glViewport((int)viewport.x, 0, (int)viewport.width, (int)viewport.height);
      glUseProgram(shaderProgram);

      float vm[16];
      float aspect = viewport.width / viewport.height;
      CreateView(vm, cam, aspect);
      glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "view"), 1, 0, vm);

      // Draw Infinite Grid
      glUseProgram(gridProgram);
      glUniformMatrix4fv(glGetUniformLocation(gridProgram, "view"), 1, 0, vm);
      float gridTransform[16];
      CreateTransform(gridTransform, cam.x, cam.y, 0, 1000.0f / cam.zoom, 1000.0f / cam.zoom);
      glUniformMatrix4fv(glGetUniformLocation(gridProgram, "transform"), 1, 0, gridTransform);

      glBindVertexArray(VAO);
      glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

      // Draw Entities
      glUseProgram(shaderProgram);
      glBindVertexArray(VAO);
      for (auto &e : entities) {
          float tm[16];
          CreateTransform(tm, e.x, e.y, e.rotation, e.sx, e.sy);
          glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "transform"), 1, 0, tm);
          glUniform3f(glGetUniformLocation(shaderProgram, "tint"), e.color[0], e.color[1], e.color[2]);
          glUniform1f(glGetUniformLocation(shaderProgram, "alpha"), 1.0f);

          unsigned int tid = textures.count(e.textureName) ? textures[e.textureName] : textures["default"];
          glBindTexture(GL_TEXTURE_2D, tid);
          glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
      }

      // Draw Particles
      for (auto &p : particles) {
          float tm[16];
          CreateTransform(tm, p.x, p.y, 0, 0.04f, 0.04f);
          glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "transform"), 1, 0, tm);
          glUniform3f(glGetUniformLocation(shaderProgram, "tint"), p.color[0], p.color[1], p.color[2]);
          glUniform1f(glGetUniformLocation(shaderProgram, "alpha"), p.life);
          glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
      }
  }
  ```
  Note what's dropped versus the old function: no `window` parameter, no `ImGui::Render()`/`RenderDrawData`/`SDL_GL_SwapWindow` (moved to `Engine::Run`, Step 6), no picking, no panels.

- [ ] **Step 5: Update `include/Renderer.h`** — change the `Render` declaration (line 18) to
  `void Render(std::vector<Entity>& entities, std::vector<Particle>& particles, const Camera& cam, const ViewportRect& viewport);`,
  remove the `bool CheckPointInside(Entity& e, float px, float py);` declaration (line 46, now dead — it's `EditorUI`'s), and add `#include "EditorUI.h"` near the top (for `ViewportRect`).

- [ ] **Step 6: Rewrite `Engine::Run`'s render call** (`src/Engine.cpp:28-50`) to orchestrate both objects and finish the frame itself:
  ```cpp
  void Engine::Run() {
      running = true;
      SDL_Event ev;

      while (running) {
          while (SDL_PollEvent(&ev)) {
              ImGui_ImplSDL2_ProcessEvent(&ev);
              if (ev.type == SDL_QUIT) running = false;
              Input::Update(ev);

              if (ImGui::GetIO().MouseWheel != 0) {
                  cam.zoom *= (ImGui::GetIO().MouseWheel > 0 ? 1.1f : 0.9f);
              }
          }

          Update();
          bool saveRequested = false;
          ViewportRect viewport = editorUI.Draw(window, entities, cam, selectedEntity, undoStack, saveRequested, renderer.GetTextureList());
          renderer.Render(entities, particles, cam, viewport);
          ImGui::Render();
          ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
          SDL_GL_SwapWindow(window);
          if (saveRequested) SaveScene();
      }
  }
  ```
  This requires `#include "imgui_impl_opengl3.h"` in `src/Engine.cpp` (for `ImGui_ImplOpenGL3_RenderDrawData`) — add it alongside the existing `imgui_impl_sdl2.h` include.

- [ ] **Step 7: Build**

  Run: `cmake --build build`
  Expected: builds with no errors.

- [ ] **Step 8: Manual verification — full behavior checklist (zero change expected)**

  Launch `build/bin/WaryEngine.exe` from `build/bin` and confirm every one of these still works exactly as before this task:
  - Grid and entities render (Review Focus #4 — panels and scene both visible, correctly composited).
  - Left-click an entity directly, and left-click a different entity — Hierarchy selection follows the click each time (Review Focus #1).
  - Select an entity, drag its Inspector "Pos X" field, release — press Ctrl+Z — position reverts (Review Focus #2).
  - Click **Save** — `scene.wary` is written (check timestamp/content) (Review Focus #3).
  - Click **+ New Entity** — a new "Prop" appears in Hierarchy at the camera's position (Review Focus #3).
  - Select an entity, click **DELETE ENTITY** — it disappears from Hierarchy (Review Focus #3).
  - Middle-mouse-drag pans the camera; scroll wheel zooms (Review Focus #5).
  - With an entity selected, WASD moves it (Review Focus #5); Space spawns particles.

- [ ] **Step 9: Commit**

  ```bash
  git add include/EditorUI.h src/EditorUI.cpp include/Renderer.h src/Renderer.cpp src/Engine.cpp
  git commit -m "refactor: move panels, picking, and undo-capture into EditorUI; slim Renderer::Render to scene-only draw"
  ```

---

### Task 3: Push Faz 2a to the public repo

**Files:** none (process step).

- [ ] **Step 1: Confirm local history is clean and ahead of origin**

  Run: `git log --oneline -5 && git status`
  Expected: the two commits from Tasks 1-2 show up, nothing uncommitted except this plan/design doc (see Step 2).

- [ ] **Step 2: Commit the design doc and this plan**

  ```bash
  git add docs/specs/2026-11-15-phase2a-renderer-ui-split-design.md docs/superpowers/plans/2026-11-15-phase2a-renderer-ui-split.md
  git commit -m "docs: add Phase 2a design doc and implementation plan"
  ```

- [ ] **Step 3: Push**

  Run: `git push origin main`
  Expected: push succeeds.

- [ ] **Step 4: Verify on GitHub**

  Run: `curl -s https://api.github.com/repos/g0ktengri/WaryEngine/commits/main | grep '"message"' | head -1`
  Expected: shows the most recent commit message from Step 2 or 3, confirming the push landed.
