# Wary Engine — Phase 1: Credibility Fixes Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make the Save button actually save, close the small data-corruption hole that wiring it up exposes, and make README.md stop claiming things the code doesn't do (ECS, GLM) — so the repo is honest before any architecture work starts.

**Architecture:** No structural change. `Renderer::Render` gains an output parameter so the UI layer can signal "user clicked Save" back to `Engine`, which already owns `SaveScene()`. `SaveScene`/`LoadScene` get a one-line space-safe encoding for entity names. README.md is edited to match the code that exists today.

**Tech Stack:** C++20, SDL2, OpenGL 3.3 (glad), Dear ImGui, CMake/MinGW — unchanged.

**Spec:** [docs/specs/2026-11-15-wary-engine-levelup-design.md](../specs/2026-11-15-wary-engine-levelup-design.md) — this plan implements only that spec's Faz 1 (Güvenilirlik düzeltmeleri). Faz 2–4 get their own plans once this one lands and is reviewed, per the spec's per-phase gate.

## Global Constraints

- C++20 standard, existing CMake + MinGW + vendored SDL2/glad/imgui/stb_image toolchain — no new dependency.
- No GLM integration in this phase (shelved per explicit decision — see spec's "Kararlaştırılan Yapı").
- Repo already has a public remote: `https://github.com/g0ktengri/WaryEngine` (branch `main`). Phase ends with a push to it.
- Don't touch the scene file *format* beyond the one-line name-encoding fix below — the full `scene.wary` format redesign is explicitly Faz 2 scope.

## Review Focus

1. **Entity name containing a space silently corrupts the saved scene.** `LoadScene` reads fields with `operator>>`, which splits on whitespace — a name like "My Player" desyncs every field after it on the next load. The Save button was previously a no-op, so nobody could hit this; wiring it up makes it live. → Task 3's test.
2. **Save gives no feedback, success or failure look identical to the user**, and `scene.wary` is opened without checking `is_open()` on write. A demo recording where Save silently fails would look broken. → Task 1's verification confirms the write actually lands at the path the running exe uses; a known no-feedback-UX limitation is explicitly deferred, not silently shipped unnoted.
3. **Saving with zero entities** (user deleted everything) must produce a valid empty `scene.wary` that `LoadScene` then reads back as an empty scene, not a stale leftover file or a crash. → Task 1's verification.
4. **Working-directory mismatch**: `scene.wary` is read/written relative to CWD, and the exe is built to `build/bin/`. If Save and Load are exercised from different CWDs the file won't be where expected. → Task 1's verification runs the exe from the same place it'll be run for the eventual demo recording.
5. **README edits must not leave a stray ECS/GLM mention elsewhere** (there are two ECS mentions and one GLM mention in the current file, in different sections) — a partial edit would leave the credibility problem half-fixed. → Task 2's grep self-check.

---

### Task 1: Wire the Save button to `Engine::SaveScene()`

**Files:**
- Modify: `include/Renderer.h:18` (Render signature)
- Modify: `src/Renderer.cpp:248` (Render signature), `src/Renderer.cpp:306` (Save button body)
- Modify: `src/Engine.cpp:27-47` (`Engine::Run`)

**Interfaces:**
- Consumes: `Engine::SaveScene()` — already exists (`src/Engine.cpp:145`), private, called from within `Engine::Run`.
- Produces: `Renderer::Render(..., bool& saveRequested)` — later tasks/phases that touch `Render`'s call site must keep passing this parameter.

- [ ] **Step 1: Change `Renderer::Render`'s signature** in `include/Renderer.h:18` and `src/Renderer.cpp:248` to add a trailing `bool& saveRequested` parameter:
  `void Render(SDL_Window* window, std::vector<Entity>& entities, std::vector<Particle>& particles, const Camera& cam, int& selectedEntityIndex, std::vector<std::vector<Entity>>& undoStack, bool& saveRequested);`

- [ ] **Step 2: Make the Save button set the flag** — in `src/Renderer.cpp:306`, replace
  `if (ImGui::Button("Save", ImVec2(-1, 0))) { /* Save logic */ }`
  with
  `if (ImGui::Button("Save", ImVec2(-1, 0))) { saveRequested = true; }`

- [ ] **Step 3: Call it from `Engine::Run`** in `src/Engine.cpp` — declare `bool saveRequested = false;` immediately before the `renderer.Render(...)` call (so it's reset every frame), pass it as the new last argument, then immediately after the call add:
  ```cpp
  if (saveRequested) SaveScene();
  ```

- [ ] **Step 4: Build**

  Run: `cmake --build build`
  Expected: builds with no errors.

- [ ] **Step 5: Manual verification — happy path, from the real run location**

  Run the exe from the directory it's meant to run from: `cd build/bin && ./WaryEngine.exe` (this is also where `assets/` already lives, confirming CWD matches Load's expectations).
  In the running app: move the selected entity (WASD) to change its position, click **Save** in the Hierarchy panel.
  Expected: `build/bin/scene.wary` is created/updated (check with `ls -la build/bin/scene.wary` in another terminal — timestamp changes, and `cat` shows the moved position). Close and relaunch the exe from the same directory; the entity reappears at the saved position (confirms Load reads what Save wrote, addresses Review Focus #4).

- [ ] **Step 6: Manual verification — empty scene**

  In the running app, select the one entity and click **DELETE ENTITY** until the hierarchy is empty, click **Save**.
  Expected: `scene.wary` is truncated to an empty file (or absent of entity lines); relaunching starts with zero entities and doesn't crash (Review Focus #3).

- [ ] **Step 7: Commit**

  ```bash
  git add include/Renderer.h src/Renderer.cpp src/Engine.cpp
  git commit -m "fix: wire Save button to actually save the scene"
  ```

---

### Task 2: Correct README.md to match the real architecture

**Files:**
- Modify: `README.md:5`, `README.md:10`, `README.md:38`

**Interfaces:**
- Consumes: nothing from Task 1.
- Produces: nothing other tasks depend on (documentation only).

- [ ] **Step 1: Fix the intro paragraph** — in `README.md:5`, replace
  `This project serves as a foundational exploration into game engine architecture, focusing on ECS (Entity Component System), custom rendering pipelines, and editor tool development.`
  with
  `This project serves as a foundational exploration into game engine architecture, focusing on entity-based scene management, custom rendering pipelines, and editor tool development.`

- [ ] **Step 2: Fix the Core Systems bullet** — in `README.md:10`, replace
  `-   **Entity Component System (ECS)**: Flexible management of game objects with properties like Transform, Physics, and Rendering.`
  with
  `-   **Entity Management**: Structured handling of game objects with properties like Transform, Physics, and Rendering. (A true ECS architecture is in progress.)`

- [ ] **Step 3: Remove the false GLM dependency claim** — in `README.md:38`, delete the line
  `-   **GLM** (Mathematics - Optional/Integrated)`
  entirely (transform/view matrices are hand-rolled in `Renderer.cpp`; no math library is linked).

- [ ] **Step 4: Self-check — no stray ECS/GLM mentions remain**

  Run: `grep -ni "ecs\|glm" README.md`
  Expected: no output (or only the new, accurate "ECS architecture is in progress" line from Step 2 — nothing implying it already exists, and zero GLM hits). Addresses Review Focus #5.

- [ ] **Step 5: Commit**

  ```bash
  git add README.md
  git commit -m "docs: correct README claims to match current architecture"
  ```

---

### Task 3: Make entity-name serialization space-safe

**Files:**
- Modify: `src/Engine.cpp:145-152` (`SaveScene`), `src/Engine.cpp:154-165` (`LoadScene`)

**Interfaces:**
- Consumes: nothing new.
- Produces: nothing other tasks depend on — Faz 2's scene-format redesign replaces this encoding entirely, so it doesn't need to be load-bearing beyond this phase.

- [ ] **Step 1: Encode on save** — in `Engine::SaveScene` (`src/Engine.cpp:145`), before writing `e.name`, replace spaces with underscores so the whitespace-delimited format can't desync:
  ```cpp
  std::string safeName = e.name;
  std::replace(safeName.begin(), safeName.end(), ' ', '_');
  ```
  and write `safeName` instead of `e.name` in the output line. (Add `#include <algorithm>` if not already present.)

- [ ] **Step 2: Decode on load** — in `Engine::LoadScene` (`src/Engine.cpp:154`), after reading `name` into the entity, replace underscores back to spaces before constructing the `Entity`:
  ```cpp
  std::string decodedName = name;
  std::replace(decodedName.begin(), decodedName.end(), '_', ' ');
  ```
  and use `decodedName` as the entity's name instead of the raw `name` buffer.

- [ ] **Step 3: Build**

  Run: `cmake --build build`
  Expected: builds with no errors.

- [ ] **Step 4: Manual verification — round-trip a spaced name**

  Run `build/bin/WaryEngine.exe` from `build/bin`. In the Inspector, rename the selected entity to `My Player` (with a space), click **Save**.
  Expected: `cat build/bin/scene.wary` shows `My_Player` (underscore) in the name field, with all other fields on the correct position. Relaunch the exe; the entity's name in the Hierarchy panel reads back as `My Player` (space restored), and its position/rotation/scale are unchanged from what was saved (confirms no field desync — Review Focus #1).

- [ ] **Step 5: Commit**

  ```bash
  git add src/Engine.cpp
  git commit -m "fix: make entity-name serialization space-safe"
  ```

---

### Task 4: Push Phase 1 to the public repo

**Files:** none (process step).

- [ ] **Step 1: Confirm local history is clean and ahead of origin**

  Run: `git log --oneline -5 && git status`
  Expected: the three commits from Tasks 1–3 show up, nothing uncommitted except intentionally-untracked docs (see Step 2).

- [ ] **Step 2: Commit the design doc and this plan**

  ```bash
  git add docs/specs/2026-11-15-wary-engine-levelup-design.md docs/superpowers/plans/2026-11-15-phase1-credibility-fixes.md
  git commit -m "docs: add Phase 1 design doc and implementation plan"
  ```

- [ ] **Step 3: Push**

  Run: `git push origin main`
  Expected: push succeeds (no rejection — `git pull` already fast-forwarded earlier in this session, so this should be a clean linear push).

- [ ] **Step 4: Verify on GitHub**

  Run: `curl -s https://api.github.com/repos/g0ktengri/WaryEngine/commits/main | grep '"message"' | head -1`
  Expected: shows the most recent commit message from Task 4 Step 2 or 3, confirming the push landed.
