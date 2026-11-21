# Rendering Batching (Faz 2b) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Cut per-frame sprite draw calls from one-per-entity-plus-one-per-particle down to one-per-distinct-texture, add a visible FPS counter and a stress-test spawn control so the improvement is provable live, with zero visible change to today's rendered output.

**Architecture:** Entities and particles are grouped by resolved texture each frame; each group's quads are pre-transformed on the CPU into world-space vertices (position+uv+color+alpha) and uploaded to one dynamic VBO, drawn with one `glDrawElements` per group against a shared, grow-only index buffer. The infinite grid is first decoupled onto its own small static buffer so repurposing the old shared VBO for batching can't silently break it. `Renderer::Render`'s public signature does not change — this is entirely internal to `Renderer.cpp`/`Renderer.h`.

**Tech Stack:** C++20, OpenGL 3.3 core profile (via glad), SDL2, Dear ImGui (docking branch, already vendored).

**Spec:** [docs/superpowers/specs/2026-11-21-rendering-batching-design.md](../specs/2026-11-21-rendering-batching-design.md)

## Global Constraints

- Current vertex format is position+UV only; per-entity tint/alpha are shader uniforms today, not per-vertex attributes — batching requires moving both (and the transform) to per-vertex CPU-computed data.
- The existing shader's `transform` uniform must be dropped from the entity/particle vertex shader — positions arriving at the GPU are already world-space once batching lands.
- No automated test framework (standing project convention) — verification is build success, a scratchpad math proof where the logic doesn't need the GUI, and live manual GUI comparison (same scene, same stress-test count, before/after).
- Working directly on `main`, no worktree, push by default — same standing convention as every prior phase.
- The infinite grid currently reuses the entity/particle `VAO`/`VBO`/`EBO` (`// Re-use standard quad`) — this must be decoupled onto its own buffer before the shared buffer is repurposed for batching, or the grid silently breaks.
- `Renderer::Render`'s public signature (`Render(std::vector<Entity>&, std::vector<Particle>&, const Camera&, const ViewportRect&, int windowHeight)`) does not change — no caller (`Engine.cpp`) needs touching.

## Review Focus

- A scene with entities/particles split across more than one distinct texture (e.g. some use `"default"`, others a custom PNG) at once — each texture's sprites must end up in exactly one draw call for that texture, with no sprite dropped, duplicated, or drawn with the wrong texture.
- Entity/particle count exceeding the shared index buffer's current capacity mid-session (expected the first time the Task 3 stress-spawn control is used) — the grow-only index buffer must actually grow and stay correct, not leave stale or undersized index data.
- Zero entities or zero particles in a given frame (deleting all entities; particles naturally emptying between SPACE presses) — batch-building and drawing must not attempt a zero-length `glBufferData`/`glDrawElements` call, divide by zero while grouping, or skip the grid.
- An entity whose `textureName` doesn't match any loaded texture (today's code already falls back to `textures["default"]` for this case) — batching must group it with other default-textured sprites by its *resolved GL texture ID*, not accidentally key grouping by the raw (unmatched) string and spawn a spurious extra group/draw call.
- Stacking the Task 3 stress-spawn control many times in one session, combined with the existing O(n) click-to-select picking loop and the 50-entry undo stack — large entity counts must not hang or crash the app during a live demo (not required to stay fast, just not broken).

---

### Task 1: Decouple the infinite grid onto its own static buffer

**Files:**
- Modify: `src/Renderer.cpp` (new `InitGridBuffers()`, called from `Init()`; grid-drawing block in `Render()` binds the new buffer instead of `VAO`)
- Modify: `include/Renderer.h` (add `gridEBO` member alongside the existing, currently-unused `gridVAO`/`gridVBO`)

**Interfaces:**
- Consumes: nothing from later tasks.
- Produces: `gridVAO`/`gridVBO`/`gridEBO`, a static position-only unit quad (`-0.5..0.5`, same 4 vertices and `{0,1,2,2,3,0}` indices as today's shared buffer) built once in `Init()`, independent of whatever `VAO`/`VBO`/`EBO` become in Task 4.

- [ ] **Step 1: Add `gridEBO` member**

In `include/Renderer.h`, add `unsigned int gridEBO;` next to the existing `gridVAO, gridVBO;` declaration.

- [ ] **Step 2: Implement `Renderer::InitGridBuffers()` in `src/Renderer.cpp`**

Build `gridVAO`/`gridVBO`/`gridEBO` with the exact same vertex data, index data, and attribute layout (`glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), 0)`, matching `gridVSrc`'s single `aPos` input — note the grid shader only reads position, so the vertex data can keep the existing 5-float stride (pos+uv) for simplicity and reuse of the same literal array as today's `InitBuffers`, with the UV components simply unread by `gridVSrc`) that `InitBuffers` sets up today, using `GL_STATIC_DRAW`. Call it from `Init()` right after `InitBuffers()`.

- [ ] **Step 3: Point the grid draw at the new buffer**

In `Render()`'s "Draw Infinite Grid" block, change `glBindVertexArray(VAO); // Re-use standard quad` to `glBindVertexArray(gridVAO);`. Leave everything else in that block (shader use, uniforms, `glDrawElements` call) unchanged.

- [ ] **Step 4: Build**

Run: `cmake --build build --config Debug`
Expected: builds with no new errors/warnings.

- [ ] **Step 5: Verify grid unchanged**

Run the engine. Expected: the infinite grid (faint 1-unit lines, stronger 10-unit lines, red X-axis, green Y-axis, zoom-dependent fade) renders exactly as it did before this change — this task must be a no-op visually. Note in the task-done report whether you were able to check this live or only structurally (identical vertex/index data and attribute layout to the buffer it replaces).

- [ ] **Step 6: Commit**

```bash
git add src/Renderer.cpp include/Renderer.h
git commit -m "refactor: give the infinite grid its own static buffer, decoupled from the sprite VBO"
```

---

### Task 2: FPS / frame-time counter overlay

**Files:**
- Modify: `include/EditorUI.h` (add `Uint32 lastFrameTicks = 0;` and `float fpsSmoothed = 0.0f;` members)
- Modify: `src/EditorUI.cpp` (compute frame time each `Draw()` call; draw a small overlay)

**Interfaces:**
- Consumes: nothing.
- Produces: a small always-visible ImGui text overlay showing current FPS (and frame time in ms), with light exponential smoothing (`fpsSmoothed = fpsSmoothed * 0.9f + instantFps * 0.1f`, or equivalent) so the number doesn't flicker illegibly. No change to `Draw()`'s signature.

- [ ] **Step 1: Track frame time**

At the top of `EditorUI::Draw`, compute `Uint32 now = SDL_GetTicks(); float dt = lastFrameTicks ? (now - lastFrameTicks) / 1000.0f : 0.0f; lastFrameTicks = now;` and update `fpsSmoothed` from `dt` (guard `dt <= 0` to avoid a divide-by-zero on the very first frame or if `dt` is ever exactly 0 on a very fast machine).

- [ ] **Step 2: Draw the overlay**

Add a small `ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoDocking` overlay window (same flag family as the existing `Tips` window, including the `NoDocking` fix from the prior plan's final review) positioned at the opposite corner from `Tips` (e.g. top-right of the Scene viewport: `ImVec2(viewport.x + viewport.width - 120, viewport.y + 10)`, builder's call on exact offset so it doesn't overlap `GizmoToolbar`), showing `"FPS: %.0f (%.1fms)"` using `fpsSmoothed` and `1000.0f / fpsSmoothed`.

- [ ] **Step 3: Build**

Run: `cmake --build build --config Debug`
Expected: builds with no new errors/warnings.

- [ ] **Step 4: Verify live**

Run the engine. Expected: an FPS readout is visible over the scene, updates continuously, and shows a plausible number (not 0, not NaN, not wildly flickering frame to frame).

- [ ] **Step 5: Commit**

```bash
git add src/EditorUI.cpp include/EditorUI.h
git commit -m "feat: add FPS/frame-time overlay to the editor"
```

---

### Task 3: Stress-test entity spawn control

**Files:**
- Modify: `src/EditorUI.cpp` (new "Debug" menu with a "Spawn 1000 Test Entities" item in the main menu bar)

**Interfaces:**
- Consumes: nothing (uses the `entities` reference `Draw()` already receives).
- Produces: a menu item that, each time clicked, appends 1000 new `Entity` values to `entities` at randomized positions (e.g. `x, y` uniformly in `[-5, 5]`), small scale (e.g. `sx = sy = 0.1f`), randomized color, `textureName = "default"`, `active = true`, `hasGravity = false`, `isStatic = false`, `rotation = 0`, `vy = 0`. Repeated clicks stack additively (do not clear existing entities first).

- [ ] **Step 1: Add the Debug menu**

In the main menu bar block (after the existing File/Edit/View menus), add `if (ImGui::BeginMenu("Debug")) { if (ImGui::MenuItem("Spawn 1000 Test Entities")) { /* spawn loop */ } ImGui::EndMenu(); }`.

- [ ] **Step 2: Implement the spawn loop**

Push 1000 `Entity{}` values into `entities` with randomized position/color as specified in Interfaces, using `rand()` (already used elsewhere in this codebase, e.g. `Engine.cpp`'s particle spawn — no new RNG dependency needed).

- [ ] **Step 3: Build**

Run: `cmake --build build --config Debug`
Expected: builds with no new errors/warnings.

- [ ] **Step 4: Verify live — this is the deliberate "before" baseline**

Run the engine, open Debug > Spawn 1000 Test Entities, click it 1-3 times. Expected: entities visibly appear scattered around the scene, and — since this task ships *before* Task 4's batching — the Task 2 FPS counter should visibly drop as the count climbs (this drop is expected and correct here, not a bug; it's the documented "before" state). Note the FPS reading at 1000 and at 2000-3000 entities in the task-done report; Task 4's own verification will compare against these numbers.

- [ ] **Step 5: Commit**

```bash
git add src/EditorUI.cpp
git commit -m "feat: add a Debug menu stress-test control to spawn test entities"
```

---

### Task 4: Core sprite batching rewrite

**Files:**
- Modify: `include/Renderer.h` (new `SpriteVertex` struct; replace per-draw uniform caching with member-cached uniform locations; new dynamic sprite VBO/EBO members and capacity tracking; new private helper method signatures)
- Modify: `src/Renderer.cpp` (shader source changes; `InitBuffers` rewritten for the new vertex layout and dynamic/grow-only buffers; `Render()`'s entity and particle draw loops replaced with texture-grouped batch building and drawing)

**Interfaces:**
- Consumes: Task 1's `gridVAO`/`gridVBO`/`gridEBO` (must not be touched or rebound by this task's changes).
- Produces: `Renderer::Render`'s public signature is unchanged; visual output for any given scene is unchanged; draw call count per frame for entities+particles drops to one per distinct texture in use.

- [ ] **Step 1: Define the batching vertex format**

In `include/Renderer.h`, add (private, or in an unnamed/file-local scope in the header):
```cpp
struct SpriteVertex { float x, y, z, u, v, r, g, b, a; };
```
Add private members: a growable `std::vector<SpriteVertex> batchScratch;` (reused across frames to avoid per-frame heap churn), `unsigned int spriteVBO, spriteEBO;` (rename/repurpose the old `VBO`/`EBO`; `VAO` stays `VAO`, now describing the new layout), `size_t spriteIndexCapacityQuads = 0;`, and cached uniform locations `int viewLoc = -1, gridViewLoc = -1, gridTransformLoc = -1;`.

- [ ] **Step 2: Rewrite `vSrc`/`fSrc`**

Vertex shader: drop the `transform` uniform; add `layout (location = 2) in vec3 aColor; layout (location = 3) in float aAlpha;`; output `TexCoord`, `Color` (vec3), and `Alpha` (float) to the fragment shader; `gl_Position = view * vec4(aPos, 1.0);` (positions arrive world-space already). Fragment shader: multiply the sampled texture color by `Color`/`Alpha` exactly as the old uniform-based `tint`/`alpha` did — pixel math must be identical for the same sprite.

- [ ] **Step 3: Cache uniform locations once**

In `InitShader`, after `glLinkProgram(shaderProgram)`, set `viewLoc = glGetUniformLocation(shaderProgram, "view");`. In `InitInfiniteGrid`, after linking `gridProgram`, set `gridViewLoc`/`gridTransformLoc` similarly. Remove all per-frame `glGetUniformLocation` calls from `Render()` for these; use the cached members instead.

- [ ] **Step 4: Rewrite `InitBuffers` for the dynamic sprite layout**

Create `VAO`/`spriteVBO` with the new 9-float-per-vertex layout (`glVertexAttribPointer` for locations 0 (pos, 3 floats), 1 (uv, 2 floats), 2 (color, 3 floats), 3 (alpha, 1 float), stride `9 * sizeof(float)`), `spriteVBO` allocated with `glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_DYNAMIC_DRAW)` initially (grown per-frame as needed — builder's call whether to pre-reserve a starting capacity, e.g. enough for 64 quads, to avoid a reallocation on the very first frame). Create `spriteEBO` empty initially; capacity (`spriteIndexCapacityQuads`) starts at 0.

- [ ] **Step 5: Implement `EnsureIndexCapacity(size_t quadCount)`**

Private helper: if `quadCount <= spriteIndexCapacityQuads`, return. Otherwise build a `std::vector<unsigned int>` of `quadCount * 6` indices following the `{0,1,2,2,3,0}` pattern offset by `4 * quadIndex` per quad, upload via `glBufferData(GL_ELEMENT_ARRAY_BUFFER, ..., GL_STATIC_DRAW)` on `spriteEBO`, and set `spriteIndexCapacityQuads = quadCount`. Called once per frame with the single largest group's quad count needed that frame (not per group — the same grown index buffer is reused for every group, since indices are always `0..4N-1` relative offsets, not texture-specific).

- [ ] **Step 6: Implement batch building + a `DrawBatch` helper**

In `Render()`, replace the "Draw Entities" loop with: group `entities` by resolved texture ID (same `textures.count(e.textureName) ? textures[e.textureName] : textures["default"]` resolution as today) into one `std::map<unsigned int, std::vector<SpriteVertex>>` (or equivalent) built via `CreateTransform` applied to each entity's 4 local corners (`(-0.5,-0.5)`, `(0.5,-0.5)`, `(0.5,0.5)`, `(-0.5,0.5)`, each with the matching UV from today's static quad, i.e. unchanged mapping) using existing `CreateTransform` math (apply the 4x4 matrix to each corner point by hand — a small new helper, e.g. `TransformPoint`, is reasonable), `e.color`, and alpha `1.0f` (entities have no alpha field — matches today's hardcoded `glUniform1f(..., "alpha", 1.0f)`). For each texture group with 1+ quads: call `EnsureIndexCapacity(group.size())`, upload the group's vertices via `glBufferSubData`/`glBufferData` on `spriteVBO`, bind that texture, and `glDrawElements(GL_TRIANGLES, (GLsizei)(group.size() * 6), GL_UNSIGNED_INT, 0)`. Skip any texture group with zero quads entirely (no draw call, no buffer upload) — see Review Focus on zero-entity frames. Repeat the identical grouping/build/draw sequence for `particles` as a second, separate pass immediately after (preserves today's entities-then-particles draw order), using `p.color` and `p.life` as alpha, and the particle's own transform (`CreateTransform(tm, p.x, p.y, 0, 0.04f, 0.04f)`, unchanged from today).

- [ ] **Step 7: Scratchpad proof — CPU pre-transform matches GPU uniform-transform**

Before trusting Step 6's math live, write a small standalone scratchpad program (in the session's scratchpad directory, not the project) that: picks an arbitrary entity transform (x, y, rotation, sx, sy) and an arbitrary local corner, computes the result two ways — (a) via `CreateTransform`'s 4x4 matrix applied as a uniform would be on the GPU (matrix-vector multiply, matching the vertex shader's old `gl_Position = view * transform * vec4(aPos, 1.0)` for the `transform` stage only, i.e. just `transform * vec4(aPos,1,1)`), and (b) via whatever CPU corner-transform helper Step 6 actually implements — and asserts the two results match within floating-point tolerance for several arbitrary (non-trivial rotation + non-uniform scale) test cases. Watch it fail first if (b) isn't written correctly, then make it pass. This is the task's RED→GREEN proof for the one piece of math this whole task depends on, following the same scratchpad-proof pattern already used successfully in this project's docking plan (Task 2, the `ViewportRect` Y-flip math).

- [ ] **Step 8: Build**

Run: `cmake --build build --config Debug`
Expected: builds with no new errors/warnings.

- [ ] **Step 9: Verify visual parity, the draw-call reduction, and the Review Focus cases**

Run the engine with the same scene used throughout this plan, plus at least one entity using a non-default texture (drop any PNG into `assets/` and assign it via the Inspector's texture dropdown) alongside default-textured entities, so more than one texture group is actually exercised. Expected: every entity and particle renders in the same position, rotation, scale, color, and alpha as before this task (visual parity — the whole point of this task is zero visible change), and the non-default-textured entity renders correctly alongside the rest (Review Focus: mixed textures). Then:
- Repeat Task 3's stress-test (Debug > Spawn 1000 Test Entities, 1-3 clicks) and compare the Task 2 FPS counter's reading at the same entity counts noted in Task 3's report: FPS should now stay noticeably higher at the same counts that visibly dropped it before this task (Review Focus: index-buffer growth is exercised here too, since 1000+ quads will exceed any small initial capacity).
- Delete all entities (zero-entity frame) and confirm no crash/flicker, and that particles alone (press SPACE with no entity selected is a no-op today, so empty-particles is simply the resting state) don't break anything either (Review Focus: zero entities/particles).
- Set one entity's texture name (via `scene.wary` or by temporarily typing an unmatched name) to something not in the loaded texture set and confirm it still renders via the default texture, grouped correctly rather than causing a crash or a stray draw call (Review Focus: unresolved texture name).
- With 1000+ entities spawned, click to select a few different entities and press Ctrl+Z a few times; confirm the app stays responsive (not necessarily fast) and doesn't crash (Review Focus: picking/undo at scale).
- Confirm the grid still renders correctly (Task 1 should make this a non-issue, but this is the integration point where a regression would actually surface).

- [ ] **Step 10: Commit**

```bash
git add src/Renderer.cpp include/Renderer.h
git commit -m "perf: batch entity/particle draws by texture, cutting draw calls to one per texture"
```
