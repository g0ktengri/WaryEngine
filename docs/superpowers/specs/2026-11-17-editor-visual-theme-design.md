# Editor Visual/Theme Overhaul — Design

## Intent & Why

The editor currently looks like a functional prototype: a fixed, non-resizable 3-panel layout (Hierarchy left, Inspector right, scene viewport filling whatever's left), flat text buttons, and a dark theme with an orange accent color. The user wants the editor to read as "professional" the way a real engine editor does — panels you can drag, resize, and dock into new arrangements — with its own visual identity rather than a copy of Unity/Unreal/Godot's look, and explicitly wants to avoid the generic "AI-generated UI" tell: purple/violet gradient accents. This follows directly on from the gizmo feature (Move/Rotate/Scale), which the user picked first because it was the most demo-able; this pass is the visual half of "ikisi de olsun" (both function and look), and matters for the same Instagram before/after content goal as the gizmos did — a visually distinctive, real-feeling editor is also better demo footage.

## Goals

- Real docking: panels can be dragged, resized, and docked/merged/tabbed, using Dear ImGui's own official docking system (not a custom one).
- A default dock layout on first run that looks deliberately arranged (Hierarchy left, Inspector right, scene viewport center) rather than a jumble the user has to assemble themselves.
- A redesigned color theme: dark base (kept, since the current near-black background is not itself an "AI slop" tell), **red accent color replacing the current orange**, flat colors only — no gradients anywhere.
- The look reads as deliberately designed, not a default/generic theme — distinct from Unity's blue-gray, Unreal's flat dark-blue, and from "AI-generated SaaS" purple-gradient styling.

## Non-Goals

- No menu bar (File/Edit/View) — not asked for; a natural follow-up, not this pass.
- No icon set — buttons stay text-labeled for now; a later, separate task once an icon approach (font icons vs. image atlas) is chosen deliberately, not bolted on here.
- No multi-viewport (dragging a docked panel out into its own OS-level window) — Dear ImGui's docking branch supports this as a related but separate feature requiring platform-backend changes; not requested, bigger lift, skip it.
- No changes to gizmo logic, Renderer's OpenGL rendering, or any non-visual editor behavior.

## Anti-Goals

- No gradients anywhere in the UI (buttons, headers, backgrounds, accents) — flat colors only. This is a hard constraint, not a preference.
- No purple/violet hues anywhere in the palette — the explicit "AI slop" tell the user named.
- Doesn't touch `Renderer.cpp`/`Renderer.h`'s rendering *algorithms* — shaders, draw calls, batching, the grid/entity/particle drawing logic all stay exactly as they are. (This is narrower than originally stated — see "Viewport region can move vertically too" below, which does require one small change to how `Renderer::Render` sets the GL viewport, but not to anything it draws or how.)

## Constraints

- The vendored Dear ImGui in this repo (`imgui/`, version 1.91.7) is the **master** branch, which has no docking support at all (confirmed: no `ImGuiConfigFlags_DockingEnable`, no `DockSpace` API in the vendored `imgui.h`). Docking requires replacing the vendored ImGui core files with Dear ImGui's official **`docking` branch** (https://github.com/ocornut/imgui/tree/docking) — the standard, actively-maintained way essentially every custom engine editor gets docking from Dear ImGui. This is a vendored-source upgrade, not a new third-party dependency.
- The scene viewport's render region (`ViewportRect`, already the shared type `Gizmo`/`Renderer`/picking all consume) is currently computed as a fixed percentage of the window (`leftPanelWidth = winW*0.2f`, `rightPanelWidth = winW*0.25f`, whatever's left is the viewport). Once Hierarchy/Inspector become freely resizable/dockable, that formula no longer holds — the viewport's actual on-screen region must come from wherever the user has arranged the dock layout, not a fixed split. `ViewportRect`'s shape doesn't change (still `{x, width, height}`), only how it gets computed each frame.
- No automated test framework (standing project convention) — verification is manual GUI click-throughs with the user, same as the gizmo work.
- Working directly on `main`, no worktree, push by default — same standing convention as every prior phase.

## Approaches (considered)

1. **Upgrade vendored ImGui to the official `docking` branch, build a real `DockSpace` host, rework the theme (chosen).** Gives genuine drag/resize/dock/tab behavior identical to what professional engine editors actually have, because it's the same underlying library feature they use. The theme rework (color-only, no structural risk) layers on top independently.
2. **Keep master ImGui, just remove `NoMove`/`NoResize` from the existing fixed windows.** Gives floating, resizable windows but never true docking — panels can't merge into tabs or snap into a shared layout. Rejected: the user explicitly asked to include "docking," and free-floating windows are a different, lesser feature wearing the same name.
3. **Hand-roll a custom docking system.** Reinventing a problem Dear ImGui's own branch already solves well, for a worse result at far higher cost. Rejected.

## Decided Design

### 1. ImGui docking-branch upgrade

Replace the vendored core ImGui files (`imgui/imgui.h`, `imgui/imgui.cpp`, `imgui/imgui_internal.h`, `imgui/imgui_tables.cpp`, `imgui/imgui_widgets.cpp`, `imgui/imgui_draw.cpp`, `imgui/imgui_demo.cpp`, and the two backends already in use — `imgui/backends/imgui_impl_sdl2.cpp`/`.h`, `imgui/backends/imgui_impl_opengl3.cpp`/`.h`/`.._loader.h`) with their counterparts from the `docking` branch of `ocornut/imgui`, at a version as close as possible to the current `1.91.7` to minimize unrelated drift. Leave the `imstb_*.h` single-header dependencies and `misc/`/`examples/`/`.github/` vendored-but-unused clutter alone unless the docking branch requires a change there too.

In `EditorUI::Init`, add `io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;` after context creation.

### 2. DockSpace host + default layout

In `EditorUI::Draw`, before drawing the Hierarchy/Inspector windows, host a full-window `ImGui::DockSpaceOverViewport()` (or an explicitly sized `ImGui::DockSpace()` call over a borderless host window spanning the whole client area — builder's call on which of ImGui's two standard patterns fits this codebase better). Change the Hierarchy and Inspector `ImGui::Begin(...)` calls to drop `ImGuiWindowFlags_NoMove`/`NoResize` (they can keep `NoCollapse` if that still reads as intentional) and drop their per-frame `SetNextWindowPos`/`SetNextWindowSize` calls — docked windows are positioned by the dock layout, not hardcoded coordinates.

On first run (no existing dock layout saved in `imgui.ini` for these dock IDs), use `ImGui::DockBuilder*` to construct a default split: Hierarchy docked left (~20% width), Inspector docked right (~25% width), and the remaining central dock node implicitly becomes "the rest of the screen," which is where the scene viewport renders (the viewport itself is not an ImGui window — it's the raw OpenGL draw region behind/between the docked panels, same as today). After a user has moved panels around once, `imgui.ini` (already present in this repo, currently storing plain window positions) will persist their arrangement across launches, same as any Dear ImGui app with docking enabled — no new save/load code needed.

### 3. Deriving the viewport region from the dock layout — including Y

Replace the fixed-percentage `leftPanelWidth`/`rightPanelWidth` calculation in `EditorUI::Draw` with one that reads the actual current screen position/size of the *central* dock node each frame (whichever of the docking branch's APIs exposes that most directly — builder's call), so the OpenGL viewport and all the `ViewportRect`-consuming code keep working off real numbers instead of an assumed split.

**Viewport region can move vertically too (correction from an earlier draft of this doc):** real docking lets the user drop a panel above or below the central region, not just left/right — so the central node's on-screen Y origin and height can both change, not just X and width. `ViewportRect` (`include/Entity.h`) needs a `y` field added (`{x, y, width, height}`). Every consumer that currently assumes "the viewport always starts at screen Y=0 and is always full window height" needs that assumption removed:
- `Renderer::Render`'s `glViewport((int)viewport.x, 0, (int)viewport.width, (int)viewport.height)` hardcodes the GL viewport's Y to 0. OpenGL's viewport Y is bottom-left-origin, while `ViewportRect.y` (like the rest of this codebase's screen-space math) is top-left-origin, so the GL call needs `glViewport((int)viewport.x, (int)(windowHeight - viewport.y - viewport.height), (int)viewport.width, (int)viewport.height)` — a Y-flip, not a direct copy. This is the one line in `Renderer.cpp` this task touches; no drawing logic changes.
- `Gizmo.cpp`'s `WorldToScreen`/`ScreenToWorld` currently assume `screenY` is measured from the window top (`viewport.height` spans the full window) — both need to measure from `viewport.y` instead of `0`.
- `EditorUI::Draw`'s mouse-picking block (`localMy = my`) has the same assumption and needs the same fix.

This is more scope than the original draft of this doc accounted for (it incorrectly assumed only the X-axis could change), but it's required for docking to actually work correctly in the general case rather than silently breaking the moment a user drags a panel above or below the scene view — which would visibly contradict the "professional, not half-built" goal this whole task exists for.

### 4. Theme rework

In `EditorUI::SetupUIStyle`, replace every use of the current orange accent (`ImVec4(0.94f, 0.50f, 0.09f, 1.00f)` — `CheckMark`, `SliderGrabActive`, the Gizmo toolbar's active-button highlight in `EditorUI.cpp`) with a single red accent color, defined once (a named constant, not a repeated literal — this also resolves the already-ledgered "orange literal typed three times" minor from the gizmo work). Keep the dark near-black base palette. Audit the rest of `SetupUIStyle`'s colors for the same discipline (flat, no gradients — Dear ImGui doesn't have a gradient-fill style property, so this is really about not introducing one via custom `ImDrawList` drawing anywhere, and about avoiding colors that read as a desaturated-purple "AI slop" cliché). Tab colors (`Tab`, `TabActive`, `TabUnfocused`, etc. — newly relevant now that docking makes tabbed panels possible) need deliberate values instead of ImGui's defaults, in the same red/dark palette.

### 5. Overlay windows stay out of the dock layout

The existing `Tips` (bottom-of-viewport hint text) and `GizmoToolbar` (top-left mode switcher) windows are deliberately free-floating overlays, not meant to become dockable panels — they use `ImGuiWindowFlags_NoDecoration`/`NoMove`/`AlwaysAutoResize` today specifically to sit on top of the scene. Keep them exactly as-is, excluded from the DockSpace (Dear ImGui's docking only docks windows that opt in / aren't explicitly excluded — builder confirms the right flag/approach during implementation).

### 6. Stale `imgui.ini` reset

The project root's `imgui.ini` is currently tracked in git and holds pre-docking plain-window `Pos`/`Size` entries for `Hierarchy`/`Inspector` (and other stale entries from earlier experiments). Dear ImGui applies those to any window not already pulled into a dock node, so left as-is they'd make the very first post-upgrade launch show floating windows at old coordinates instead of the intended first-run default dock layout. Delete the existing `imgui.ini` as part of this task (let it regenerate fresh with the new dock layout), and add `imgui.ini` to `.gitignore` going forward — it's per-user layout state, not something that should dirty `main` on every panel drag, consistent with this project's "push by default, keep history legible" convention.

## What's Left to the Builder

- Exact commit/tag of the `docking` branch to vendor (closest available to `1.91.7`; check `ocornut/imgui`'s branch history).
- Whether to use `ImGui::DockSpaceOverViewport()` or a manual host-window + `DockSpace()` call — whichever fits this codebase's existing per-frame window setup better.
- The exact red hex/RGB value and any supporting shade variants (hover/active states) — pick something that reads as deliberate and professional, not neon/bright-warning-red; the builder has full creative latitude here within "red, not orange; no purple; no gradient."
- Exact `DockBuilder` calls for the first-run default split.
- How to compute the viewport region from the dock layout each frame (central-node query vs. reading docked-panel geometry) — whichever the docking branch's API supports more directly.
