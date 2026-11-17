# Editor Viewport Gizmos Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Give the editor on-screen Move/Rotate/Scale gizmos for the selected entity, switchable via a small viewport toolbar, fully integrated with the existing Undo stack.

**Architecture:** A new `Gizmo` class (owned by `EditorUI`) draws handles via ImGui's `ImDrawList` and hit-tests them in screen space, reusing the same world↔screen math the existing mouse-picking code already uses. `EditorUI::Draw` calls into it each frame, lets it consume clicks on its own handles before falling through to normal entity-picking, and folds its drag state into the existing undo-capture condition.

**Tech Stack:** C++20, Dear ImGui (`ImDrawList`), existing `Entity`/`Camera`/`ViewportRect` types. No new dependencies.

**Spec:** `docs/superpowers/specs/2026-11-17-editor-gizmos-design.md`

## Global Constraints

- No test framework exists. Verification is: an isolated scratchpad C++ repro for pure math (no GUI needed), plus manual GUI click-throughs with the user for anything interactive — per standing project convention.
- Single-entity selection only (`int& selected` index into `std::vector<Entity>`) — no multi-select gizmo.
- Must not regress existing click-to-select picking, Inspector editing, or Undo behavior.
- No new third-party dependency; no GLM.
- Gizmo move-arrows and scale-handles stay world-axis-aligned regardless of the entity's own `rotation`.
- Scale drags clamp `sx`/`sy` to `[0.01, 10.0]`, matching the Inspector's existing `ImGuiSliderFlags_AlwaysClamp` fields.
- Gizmo drawing and hit-testing are clipped to the scene `ViewportRect` (same rule the `Tips` overlay already follows).
- Toolbar: plain text buttons (`Move`/`Rotate`/`Scale`), no icons, no keyboard shortcuts for mode switching in this pass.
- New source file added (`src/Gizmo.cpp`) means `cmake ..` must be re-run inside `build/` before `cmake --build build` picks it up (same `GLOB_RECURSE` reconfigure rule hit in Phase 2a).

## Review Focus

- An entity near the edge of the camera view (or at extreme zoom) must not get a gizmo handle that paints over or becomes draggable on top of the Hierarchy/Inspector panels — owned by Task 2.
- A fast/large Scale drag must not shrink an entity toward zero size (which would also shrink its own click-hitbox) or grow it unbounded — owned by Task 4.
- Releasing the mouse outside the window/viewport mid-drag must still end the drag cleanly next frame, not leave the gizmo stuck "dragging" forever — owned by Task 2.
- Clicking a different entity (or empty space) while a gizmo is shown must move/hide the gizmo onto the new selection, not keep acting on the stale one — owned by Task 2.
- Deleting the currently-selected entity (existing `DELETE ENTITY` button, which resets `selected` to `0`) must not leave the gizmo referencing a dangling entity — owned by Task 2.

---

### Task 1: Coordinate math foundation + `Gizmo` skeleton

**Files:**
- Create: `include/Gizmo.h`
- Create: `src/Gizmo.cpp`

**Interfaces:**
- Produces: `enum class GizmoMode { Move, Rotate, Scale };` and `class Gizmo` with public `GizmoMode mode = GizmoMode::Move;` and two methods later tasks will call: `void Draw(ImDrawList* drawList, const Entity& entity, const Camera& cam, const ViewportRect& viewport);` and `bool Update(Entity& entity, const Camera& cam, const ViewportRect& viewport, bool mouseDown, bool mouseClicked, bool mouseReleased, float mouseScreenX, float mouseScreenY);` (`Update` returns true while a drag is in progress). Also produces two free functions (not members, so Task 2's `EditorUI` picking-block refactor can call them too): `ImVec2 WorldToScreen(float worldX, float worldY, const Camera& cam, const ViewportRect& viewport);` and a matching `void ScreenToWorld(float screenX, float screenY, const Camera& cam, const ViewportRect& viewport, float& outWorldX, float& outWorldY);`.
- Consumes: existing `Entity`, `Camera`, `ViewportRect` from `Entity.h`/`EditorUI.h`.

- [ ] **Step 1: Write the failing scratchpad test**

In the session scratchpad (not the repo), write a standalone `.cpp` with declarations only (no bodies) for `WorldToScreen`/`ScreenToWorld` matching the signatures in Interfaces above, and a `main()` that asserts, using independently hand-calculated expected values:
```cpp
// Given cam = {x:2.0f, y:-1.0f, zoom:1.5f}, viewport = {x:100, width:800, height:600}
// WorldToScreen(2.0f, -1.0f, cam, viewport) should equal screen center: (100 + 400, 300) = (500, 300)
// ScreenToWorld(500, 300, cam, viewport) should round-trip back to (2.0f, -1.0f)
// A second point: WorldToScreen(3.0f, -1.0f, cam, viewport) then ScreenToWorld of that result should round-trip to (3.0f, -1.0f)
```
Assert with a small epsilon (`1e-4f`). This proves the two formulas are true inverses of each other before they're wired into any UI.

- [ ] **Step 2: Run the scratchpad test to verify it fails**

It won't link (declared but undefined) — confirm the error is "undefined reference" / "unresolved external", not a typo in the test itself.

- [ ] **Step 3: Implement `WorldToScreen`/`ScreenToWorld` in `src/Gizmo.cpp`, declare in `include/Gizmo.h`**

Use the exact formulas from the design doc:
```
aspect = viewport.width / viewport.height
ndcX = (worldX - cam.x) * cam.zoom / aspect
ndcY = (worldY - cam.y) * cam.zoom
screenX = (ndcX + 1) / 2 * viewport.width + viewport.x
screenY = (1 - ndcY) / 2 * viewport.height
```
`ScreenToWorld` is the algebraic inverse (already present as the picking-block math in `EditorUI.cpp` lines 70-74 — port it, don't re-derive it).

- [ ] **Step 4: Paste the just-written implementation bodies into the scratchpad test file (replacing the empty declarations) and run it**

Expected: all assertions pass (round-trip and known-point checks) — this is the GREEN step, proving the real implementation (not a re-typed copy) satisfies the hand-calculated oracles.

- [ ] **Step 5: Add the `Gizmo` class skeleton to `include/Gizmo.h`**

`GizmoMode` enum, `Gizmo` class with the `mode` field and the `Draw`/`Update` signatures from Interfaces above declared (bodies can be empty/no-op — Tasks 2-4 fill them in per mode). Private members: whatever drag-state fields `Update` needs (e.g. `bool dragging`, `int activeHandle`, drag-start snapshot fields) — builder's call on exact names.

- [ ] **Step 6: Reconfigure and build**

Run: `cmake ..` inside `build/`, then `cmake --build build`
Expected: builds clean (new file compiles; `Gizmo::Draw`/`Update` aren't called from anywhere yet, so this just proves the new file compiles standalone).

- [ ] **Step 7: Commit**

```bash
git add include/Gizmo.h src/Gizmo.cpp
git commit -m "feat: add Gizmo class skeleton and world/screen coordinate math"
```

---

### Task 2: Move gizmo, wired into `EditorUI`, undo + selection-lifecycle integration

**Files:**
- Modify: `include/Gizmo.h`, `src/Gizmo.cpp`
- Modify: `include/EditorUI.h`
- Modify: `src/EditorUI.cpp`

**Interfaces:**
- Consumes: `Gizmo::Draw`/`Update` signatures and `WorldToScreen`/`ScreenToWorld` from Task 1.
- Produces: `EditorUI` gains a `Gizmo gizmo;` private member, used by Task 3 and Task 4 (they extend the same instance's `Rotate`/`Scale` handling, not new instances).

- [ ] **Step 1: Implement Move-mode drawing in `Gizmo::Draw`**

When `mode == GizmoMode::Move` and the viewport's clip rect contains the entity's on-screen position (use `ImDrawList::PushClipRect(ImVec2(viewport.x, 0), ImVec2(viewport.x + viewport.width, viewport.height), true)` before drawing, `PopClipRect` after — this is what satisfies the Review Focus viewport-clipping item for every mode, not just Move): draw a red line for the +X arrow, a green line for the +Y arrow (both a fixed 60px screen-space length from the entity's `WorldToScreen` position), and a small white filled square at the center.

- [ ] **Step 2: Implement Move-mode hit-testing and drag in `Gizmo::Update`**

On `mouseClicked`, hit-test the mouse screen position against the three handle regions (X-arrow tip area, Y-arrow tip area, center square) in screen space; if hit, store `dragging = true`, which handle, and the drag-start world position (`ScreenToWorld` of the click) and the entity's `x`/`y` at that moment. While `mouseDown` and `dragging`, compute `delta = ScreenToWorld(current mouse) - dragStartWorld`; apply `delta.x` to `entity.x` (X-arrow), `delta.y` to `entity.y` (Y-arrow), or both (center square), each as `dragStartEntityX/Y + delta`, not accumulated. On `mouseReleased` (or if `mouseDown` becomes false for any reason, including the mouse leaving the window), set `dragging = false` unconditionally — this satisfies the Review Focus "release outside the window" item. Return `dragging`.

- [ ] **Step 3: Wire `Gizmo` into `EditorUI`**

Add `#include "Gizmo.h"` and `Gizmo gizmo;` to `include/EditorUI.h`. In `src/EditorUI.cpp`'s `Draw`, when `selected` is a valid index: call `gizmo.Draw(ImGui::GetForegroundDrawList(), entities[selected], cam, viewport)` and `bool gizmoActive = gizmo.Update(entities[selected], cam, viewport, ImGui::IsMouseDown(ImGuiMouseButton_Left), ImGui::IsMouseClicked(ImGuiMouseButton_Left), ImGui::IsMouseReleased(ImGuiMouseButton_Left), mx, my)` (reuse the `mx`/`my` already read for picking). When `selected` is invalid, skip both calls (satisfies the Review Focus "hide on no selection" item) and treat `gizmoActive` as `false`.

- [ ] **Step 4: Make the gizmo claim its clicks before entity-picking runs**

In the existing picking block (`EditorUI.cpp` lines ~64-83), only run the picking loop `if (!gizmoActive_this_click)` — i.e. skip re-selecting/deselecting when the click that just happened was consumed by a gizmo handle. (`gizmoActive` from Step 3, read before the picking block runs in the same frame, satisfies the "clicking a different entity" Review Focus item by falling through to normal picking whenever the gizmo did *not* consume the click.)

- [ ] **Step 5: Fold gizmo dragging into the undo-capture condition**

Change `EditorUI.cpp`'s undo block's condition from `ImGui::IsAnyItemActive()` to `bool editing = ImGui::IsAnyItemActive() || gizmoActive;` and use `editing` in place of the old direct call — the rest of the capture/push logic (lines ~42-53) is unchanged.

- [ ] **Step 6: Build**

Run: `cmake --build build` (no new files this task, no reconfigure needed)
Expected: builds clean.

- [ ] **Step 7: Manual verification — full Move gizmo behavior checklist**

Launch `build/bin/WaryEngine.exe` from `build/bin` and confirm:
- Selecting an entity shows a red X-arrow, green Y-arrow, and white center square at its position.
- Dragging the X-arrow moves the entity only horizontally; the Y-arrow only vertically; the center square moves it freely.
- After a drag, `Ctrl+Z` undoes the entire drag as one step (entity returns to its pre-drag position, not just the last frame's delta).
- Clicking a different entity while a gizmo is shown moves the gizmo to the new selection (Review Focus item).
- Selecting an entity, deleting it via `DELETE ENTITY`, confirms no crash and the gizmo reflects the new `selected` (now `0`) next frame (Review Focus item).
- Starting a drag, moving the mouse outside the application window, and releasing there: clicking normally afterward still works (drag ended cleanly — Review Focus item).
- Panning/zooming the camera so the entity sits near the left/right edge of the scene viewport: the gizmo's arrows are clipped and never draw over the Hierarchy or Inspector panels (Review Focus item).
- Clicking on empty space (no entity) still deselects as before Phase 1/2a; the gizmo disappears.

- [ ] **Step 8: Commit**

```bash
git add include/Gizmo.h src/Gizmo.cpp include/EditorUI.h src/EditorUI.cpp
git commit -m "feat: add Move gizmo with drag, undo, and selection-lifecycle handling"
```

---

### Task 3: Toolbar + Rotate gizmo

**Files:**
- Modify: `include/Gizmo.h`, `src/Gizmo.cpp`
- Modify: `src/EditorUI.cpp`

**Interfaces:**
- Consumes: `Gizmo` instance and `mode` field from Tasks 1-2.
- Produces: nothing new consumed by later tasks beyond the existing `Gizmo::mode` field, which Task 4 also switches.

- [ ] **Step 1: Implement Rotate-mode drawing in `Gizmo::Draw`**

When `mode == GizmoMode::Rotate`: draw a circle outline (`drawList->AddCircle`) centered on the entity's screen position, radius = the entity's on-screen half-diagonal (computed from its `sx`/`sy` scaled by `cam.zoom`/aspect via `WorldToScreen` of a corner) plus a fixed ~15px padding. Still inside the same clip-rect push/pop as Move.

- [ ] **Step 2: Implement Rotate-mode hit-testing and drag in `Gizmo::Update`**

On `mouseClicked`, hit-test the mouse against the ring (distance from entity's screen center within a small band around the ring radius); if hit, store `dragging = true`, `dragStartAngle = atan2(dragStartWorld.y - entity.y, dragStartWorld.x - entity.x)`, and `dragStartRotation = entity.rotation`. While dragging, each frame: `currentAngle = atan2(currentWorld.y - entity.y, currentWorld.x - entity.x)`; `entity.rotation = dragStartRotation + (currentAngle - dragStartAngle)`. Same `mouseReleased`/drag-end handling as Move.

- [ ] **Step 3: Add the toolbar to `EditorUI::Draw`**

A new ImGui window overlaid at the top of the scene viewport (same pattern as the existing `Tips` window: `ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove`, positioned at `(sceneX + 10, 10)`), shown only when `selected` is valid. Two buttons so far, `"Move"` and `"Rotate"`, each setting `gizmo.mode` on click; highlight whichever matches the current `gizmo.mode` (e.g. `ImGui::PushStyleColor(ImGuiCol_Button, <accent color>)` around that one button before drawing it, matching the existing theme's orange accent already used for `CheckMark`/`SliderGrabActive` in `SetupUIStyle`).

- [ ] **Step 4: Build**

Run: `cmake --build build`
Expected: builds clean.

- [ ] **Step 5: Manual verification**

Launch the exe. Confirm: toolbar appears top-left of the scene viewport only when an entity is selected; clicking "Rotate" switches the gizmo to a ring around the entity and highlights the Rotate button; dragging the ring rotates the entity continuously (no snapping/jumping back to the drag-start angle); `Ctrl+Z` undoes a full rotate-drag as one step; switching back to "Move" shows the arrows again on the same entity.

- [ ] **Step 6: Commit**

```bash
git add include/Gizmo.h src/Gizmo.cpp src/EditorUI.cpp
git commit -m "feat: add Rotate gizmo and viewport mode toolbar"
```

---

### Task 4: Scale gizmo (with clamp)

**Files:**
- Modify: `include/Gizmo.h`, `src/Gizmo.cpp`
- Modify: `src/EditorUI.cpp`

**Interfaces:**
- Consumes: `Gizmo` instance/toolbar pattern from Task 3.
- Produces: nothing further consumed by later tasks (this is the last gizmo mode in this plan).

- [ ] **Step 1: Implement Scale-mode drawing in `Gizmo::Draw`**

When `mode == GizmoMode::Scale`: draw a small filled square at each of the 4 corners and 4 edge-midpoints of the entity's world-axis-aligned bounding box (`entity.x ± sx/2`, `entity.y ± sy/2`, each converted via `WorldToScreen`). Same clip-rect rule as the other modes.

- [ ] **Step 2: Implement Scale-mode hit-testing and drag, with the clamp**

On `mouseClicked`, hit-test the mouse against the 8 handle squares; if hit, store `dragging = true`, which handle, and the entity's center (`entity.x`, `entity.y`) at drag-start (center doesn't move during a scale drag). While dragging: for a corner handle, `newHalfX = abs(currentWorld.x - entity.x)`, `newHalfY = abs(currentWorld.y - entity.y)`, then `entity.sx = clamp(2 * newHalfX, 0.01f, 10.0f)` and `entity.sy = clamp(2 * newHalfY, 0.01f, 10.0f)` (this satisfies the Review Focus scale-clamp item); for an edge-midpoint handle, only update the one axis it sits on, same clamp. Same `mouseReleased` drag-end handling as the other modes.

- [ ] **Step 3: Add the third toolbar button**

In `EditorUI.cpp`'s toolbar block from Task 3, add a `"Scale"` button alongside `"Move"`/`"Rotate"`, same highlight-when-active treatment.

- [ ] **Step 4: Build**

Run: `cmake --build build`
Expected: builds clean.

- [ ] **Step 5: Manual verification — full checklist (all three modes together)**

Launch the exe and confirm, in one pass:
- "Scale" button appears and switches to corner/edge-midpoint handles.
- Dragging a corner handle resizes both Width and Height; dragging an edge-midpoint handle resizes only the one axis it sits on.
- Dragging a scale handle far past the entity's center (fast/extreme drag) stops growing/shrinking at `10.0`/`0.01` rather than going further or flipping sign (Review Focus item) — confirm by checking the Inspector's Width/Height fields read exactly `10.000`/`0.010` at the extremes, not beyond.
- After a scale drag, `Ctrl+Z` undoes it as one step.
- All three toolbar buttons switch modes correctly and the active one stays highlighted; Inspector fields for Pos/Rot/Width/Height still reflect every gizmo edit live (no regression from Task 2/3's changes).

- [ ] **Step 6: Commit**

```bash
git add include/Gizmo.h src/Gizmo.cpp src/EditorUI.cpp
git commit -m "feat: add Scale gizmo with size clamp and third toolbar mode"
```

---

### Task 5: Push to origin/main

**Files:**
- Create: none — this task only commits the already-written spec/plan docs and pushes.

- [ ] **Step 1: Commit the spec and plan docs**

```bash
git add docs/superpowers/specs/2026-11-17-editor-gizmos-design.md docs/superpowers/plans/2026-11-17-editor-gizmos.md
git commit -m "docs: add editor gizmos design doc and implementation plan"
```

- [ ] **Step 2: Push**

```bash
git push origin main
```

- [ ] **Step 3: Verify**

Run: `git log origin/main -1 --oneline` (or a `gh`/GitHub API check)
Expected: the pushed commit matches local `HEAD`.
