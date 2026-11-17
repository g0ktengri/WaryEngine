# Editor Viewport Gizmos — Design

## Intent & Why

Wary Engine's editor currently has no way to move, rotate, or scale an entity except by typing numbers into the Inspector panel's drag-fields. That's slow and doesn't read as a "real" game-engine editor. The user wants the UI/UX side of the engine to visibly level up next (after the Phase 2a Renderer/UI split), and picked viewport gizmos — the classic move/rotate/scale handles drawn directly over the selected entity — as the first piece of that, because it's both a genuine usability upgrade and the single most demo-able feature for the Instagram "before/after" content (dragging a handle on screen reads far better on video than typing into a field).

This is scoped as its own project: gizmos only. A second pass on pure visual/theme polish (icons, docking, menu bar) is intentionally deferred to a follow-up, per the user's own ordering ("ikisi de olsun, önce gizmo'larla başlayalım").

## Goals

- A selected entity shows an on-screen gizmo for the active mode: **Move**, **Rotate**, or **Scale**.
- Dragging a gizmo handle edits the entity's `x`/`y`, `rotation`, or `sx`/`sy` live, matching what the Inspector fields already control.
- Mode switching via a small toolbar (text buttons: Move / Rotate / Scale), with the active mode visually highlighted.
- Gizmo drags integrate with the existing Undo stack exactly like Inspector edits do (one undo step per drag, not per frame).
- Inspector fields keep working unchanged as a secondary, precise-value input method.

## Non-Goals (this pass)

- No multi-entity selection or multi-entity gizmo.
- No grid/angle snapping (e.g. hold-Shift-to-snap).
- No gizmo-space rotation: move arrows and scale handles stay world-axis-aligned even when the entity itself is rotated (the Rotate gizmo itself still works on any rotation value).
- No toolbar icons/images — plain text buttons for now (icon pass is a separate, later UI task).
- No keyboard shortcuts for mode switching in this pass (toolbar-only) — WASD is already taken by the play-test movement controls, and picking new bindings is a separate decision not worth blocking gizmos on.

## Anti-Goals

- Doesn't touch `Renderer.cpp`/`Renderer.h` at all — gizmos are editor chrome, not scene content, and Phase 2a deliberately separated those concerns. Rendering them through raw OpenGL would undo that split for no visual benefit in a 2D editor.
- Doesn't introduce a new third-party dependency (rules out ImGuizmo) or revisit the shelved GLM decision.

## Constraints

- No test framework; verification is manual GUI click-throughs (per standing project convention) plus, where feasible, an isolated scratchpad math check for the coordinate-conversion formulas (pure functions, no GUI needed).
- Must keep working on the existing single-entity selection model (`int& selected` index into `std::vector<Entity>`).
- Must not regress existing picking (click-to-select), Inspector editing, or Undo behavior.

## Approach (decided)

**ImGui `ImDrawList`-drawn 2D gizmo, hit-tested in screen space, living entirely in `EditorUI`.** Considered and rejected:
- *Real OpenGL geometry through `Renderer`* — would re-couple UI interaction into the rendering class Phase 2a just split out, for a 2D editor where flat ImGui-drawn handles look identical to "real" geometry anyway.
- *ImGuizmo library* — built around 3D/GLM-style matrices; GLM is explicitly shelved, and adapting its 3D-flavored rotation rings/visuals to this flat 2D style would cost more than writing a small purpose-built gizmo.

This fits the current architecture directly: `EditorUI` already owns all interactive editor chrome (panels, picking, undo-capture) and already has the mouse→world conversion math (in the picking block); the gizmo reuses and formalizes that math instead of duplicating it.

## Decided Design

### New file: `include/Gizmo.h` / `src/Gizmo.cpp`

A new `Gizmo` class, owned by `EditorUI` (one member, like `wasActive`/`preEditState` already are), with its own `.h`/`.cpp` rather than growing `EditorUI.cpp` further — gizmo math/drawing is a distinct enough responsibility to earn its own file, consistent with how Phase 2a split `Renderer`/`EditorUI` apart.

```cpp
enum class GizmoMode { Move, Rotate, Scale };

class Gizmo {
public:
    GizmoMode mode = GizmoMode::Move;

    // Draws the handles for the given entity and processes any active drag.
    // Returns true while a drag is in progress (so EditorUI can drive undo-capture).
    bool Update(ImDrawList* drawList, Entity& entity, const Camera& cam,
                const ViewportRect& viewport, bool mouseDown, bool mouseClicked, bool mouseReleased,
                float mouseScreenX, float mouseScreenY);

private:
    // drag state: which handle, drag-start mouse world pos, drag-start entity values
    ...
    ImVec2 WorldToScreen(float wx, float wy, const Camera& cam, const ViewportRect& viewport) const;
    glm_free_vec2 ScreenToWorld(float sx, float sy, const Camera& cam, const ViewportRect& viewport) const;
};
```

(No GLM — "vec2" here just means a small internal `struct { float x, y; }`, or two floats; the builder picks whichever is less code.)

### Coordinate math (pin down exactly, since picking already uses it)

World→screen is the exact inverse of the existing picking code in `EditorUI::Draw`:

```
aspect = viewport.width / viewport.height
ndcX = (worldX - cam.x) * cam.zoom / aspect
ndcY = (worldY - cam.y) * cam.zoom
screenX = (ndcX + 1) / 2 * viewport.width + viewport.x
screenY = (1 - ndcY) / 2 * viewport.height
```

Screen→world is the existing picking formula, already in the codebase (`EditorUI::Draw`'s picking block), unchanged. The builder should factor both directions into `Gizmo`'s private helpers (or a shared free function both `Gizmo` and the picking block call) rather than leaving two copies — builder's call which.

### Visuals

- **Move**: a red arrow along world +X, a green arrow along world +Y (matching the existing grid axis colors), both from the entity's center outward a fixed screen-space length (e.g. 60px, not world-space, so the gizmo stays a usable size at any zoom); plus a small white square at dead-center for free 2-axis drag.
- **Rotate**: a circle outline around the entity (radius = entity's on-screen half-diagonal + a fixed padding). Dragging anywhere on the ring rotates the entity.
- **Scale**: a small square handle at each of the 4 corners of the entity's (world-axis-aligned) bounding box for 2-axis scale, plus a small square at the midpoint of each edge for single-axis (width-only or height-only) scale — mirroring the Inspector's existing separate Width/Height controls.
- All handles drawn via `drawList->AddLine`/`AddCircle`/`AddRectFilled` with a highlight color when hovered.
- Both drawing and hit-testing are clipped to the scene viewport rectangle (the same `ViewportRect` already passed to `Renderer::Render`) — same "stay inside the scene area" rule the `Tips` overlay already follows — so a handle for an entity near the camera edge never paints over or becomes draggable on top of the Hierarchy/Inspector panels.

### Interaction

- Each frame, `Gizmo::Update` hit-tests the mouse against its own handles *before* `EditorUI`'s existing entity-picking block runs its check for that same click. If a handle is hit on mouse-down, the gizmo captures the drag and the normal click-to-select logic is skipped for that click (an `if (gizmo consumed the click) { /* skip picking */ }` guard in `EditorUI::Draw`).
- **Move drag**: `delta = mouseWorldNow - mouseWorldAtDragStart`; X-arrow applies `delta.x` only, Y-arrow `delta.y` only, center square both; new value = entity's value at drag-start + delta (not accumulated frame-to-frame, to avoid drift).
- **Rotate drag**: store `dragStartAngle = atan2(mouseWorldAtDragStart - entityCenter)` and `dragStartRotation = e.rotation`; each frame `e.rotation = dragStartRotation + (atan2(mouseWorldNow - entityCenter) - dragStartAngle)`.
- **Scale drag**: for a corner handle, new half-extent on each axis = `abs(mouseWorldNow.axis - entityCenter.axis)`, so `e.sx`/`e.sy` = `2 * that`; an edge-midpoint handle only updates the one axis it sits on. Clamp the result to the same `[0.01, 10.0]` range the Inspector's own Width/Height drag-fields already enforce (`EditorUI.cpp`'s `ImGuiSliderFlags_AlwaysClamp` fields) — without this, a fast drag can shrink an entity's size toward zero, which also shrinks its own click-hitbox (`CheckPointInside` uses `sx/2`/`sy/2`) and makes it hard to re-select in the viewport afterward.

### Toolbar

A small ImGui window overlaid at the top of the scene viewport (same "overlay, don't shrink the GL viewport" pattern already used by the `Tips` window at the bottom), containing three buttons — `Move`, `Rotate`, `Scale` — that set `gizmo.mode`. The active mode's button is visually highlighted (e.g. via `ImGui::PushStyleColor(ImGuiCol_Button, ...)` when `mode == that button's mode`, matching the existing dark theme's accent color already used for `CheckMark`/`SliderGrabActive`). Only drawn/active when an entity is selected.

### Undo integration

`EditorUI::Draw`'s existing undo-capture block keys off `ImGui::IsAnyItemActive()` alone. Change the condition to `bool editing = ImGui::IsAnyItemActive() || gizmo.IsDragging();` (add an `IsDragging()` query to `Gizmo`, or have `Update`'s return value feed the same flag) — everything else about the undo-capture block (capture `preEditState` on the active→true edge, push to `undoStack` on the true→false edge) stays exactly as-is, so gizmo drags produce one undo step per drag just like one undo step per Inspector-field edit.

## What's Left to the Builder

- Exact handle sizes/colors/hover-highlight tuning (pick something reasonable and visible against the dark background; not pixel-perfect-critical).
- Whether `WorldToScreen`/`ScreenToWorld` live as `Gizmo` private methods or as free functions shared with `EditorUI`'s picking block (DRY either way works; pick one).
- Exact ImGui styling calls for the toolbar's active-mode highlight.
- Whether `Gizmo::Update`'s mouse-state parameters are passed in from `EditorUI` (which already reads `ImGui::IsMouseClicked`/`SDL_GetMouseState` for picking) or re-queried inside `Gizmo` itself — builder's call, either is fine since both run in the same frame/thread.
