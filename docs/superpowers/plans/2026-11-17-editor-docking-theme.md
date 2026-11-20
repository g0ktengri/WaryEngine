# Editor Docking + Theme Overhaul Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Give the editor real Dear ImGui docking (drag/resize/tab panels, persisted layout) and a redesigned dark-with-red-accent theme, with no gradients and no purple, replacing the current fixed 3-panel layout and orange accent.

**Architecture:** Upgrade the vendored Dear ImGui core + SDL2/OpenGL3 backend files to the official `docking` branch, enable docking, host a `DockSpace` that Hierarchy/Inspector become real dockable windows inside (with a constructed default layout on first run), make the scene viewport's region (now including a `y` origin, since docking lets panels move vertically too) and everything that consumes it (`Renderer`, `Gizmo`, picking) read from the actual dock layout each frame instead of a fixed left/right percentage split, then restyle the theme.

**Tech Stack:** C++20, Dear ImGui `docking` branch (replacing the vendored `master` branch copy), existing SDL2/OpenGL3 backends.

**Spec:** `docs/superpowers/specs/2026-11-17-editor-visual-theme-design.md`

## Global Constraints

- Vendored Dear ImGui (currently 1.91.7, `master`) must be upgraded to the official `docking` branch of `ocornut/imgui`, at the commit/tag closest to 1.91.7 available on that branch.
- No gradients anywhere in the UI. No purple/violet hues anywhere in the palette.
- `Renderer.cpp`'s only permitted change in this plan is the `glViewport` Y-flip described below — no other rendering-logic changes.
- `ViewportRect` (`include/Entity.h`) gains a `y` field: `{x, y, width, height}`. Every consumer that currently assumes the viewport always starts at screen Y=0 and spans the full window height must be updated to use `viewport.y` instead of that assumption.
- Non-goals (do not build): icon set, multi-viewport (OS-window-level docking). (A menu bar was originally a non-goal too, but the user asked for one mid-plan — see Task 4, added after Task 3 shipped — scoped to File/Edit/View only.)
- No automated test framework exists; verification is an isolated scratchpad check for pure coordinate math where feasible (same pattern as the gizmo plan's Task 1), plus manual GUI click-throughs with the user for interactive/visual behavior.
- Working directly on `main`, no worktree, push by default — standing project convention.
- `imgui.ini` (currently tracked, holding stale pre-docking window positions) must be deleted from git and added to `.gitignore`.

## Review Focus

- Dragging the Hierarchy or Inspector panel to dock above or below the scene viewport (not just left/right) must still render the scene at the correct position and size, not stretched, offset, or flipped — owned by Task 2 (coordinate math) and re-verified live in Task 3 (docking enabled).
- The very first launch after this upgrade (no prior dock-layout data) must show the intended default arranged layout (Hierarchy left, Inspector right, scene center), not an empty/undocked jumble or stale floating windows at old coordinates — owned by Task 3.
- Gizmo handles and click-to-select picking must stay pixel-aligned with the rendered scene once the viewport can move in Y, not just X — a direct regression risk from changing shared coordinate math — owned by Task 2, re-verified live in Task 3.
- The new red accent must actually appear everywhere the old orange did (toolbar active-button highlight, checkbox checkmarks, slider grab-active) — a missed spot looks half-themed — owned by Task 5.
- Docking two panels into the same space as tabs (dragging Inspector onto Hierarchy, the first thing a real user tries with docking) must not crash and must use readable, on-theme tab colors, not Dear ImGui's unstyled defaults — owned by Task 5 (styling) and Task 3 (the underlying mechanism).

---

### Task 1: Upgrade vendored Dear ImGui to the `docking` branch

**Files:**
- Modify (replace wholesale): `imgui/imgui.h`, `imgui/imgui.cpp`, `imgui/imgui_internal.h`, `imgui/imgui_tables.cpp`, `imgui/imgui_widgets.cpp`, `imgui/imgui_draw.cpp`, `imgui/imgui_demo.cpp`
- Modify (replace wholesale): `imgui/backends/imgui_impl_sdl2.cpp`, `imgui/backends/imgui_impl_sdl2.h`, `imgui/backends/imgui_impl_opengl3.cpp`, `imgui/backends/imgui_impl_opengl3.h`, `imgui/backends/imgui_impl_opengl3_loader.h`

**Interfaces:**
- Produces: a vendored Dear ImGui that has `ImGuiConfigFlags_DockingEnable`, `ImGui::DockSpace`/`DockSpaceOverViewport`, and the `ImGui::DockBuilder*` family available (not yet enabled or used by any of this project's own code — that's Task 3).
- Consumes: nothing from this project.

- [ ] **Step 1: Fetch the replacement files**

From the official `ocornut/imgui` GitHub repository, `docking` branch, fetch the files listed above at the commit/tag closest to version `1.91.7` (check the branch's tags/history for the nearest match; if none matches exactly, use the closest later commit still tagged in the 1.91.x series, and note the exact commit SHA you used in your task report). Do not fetch or modify anything under `imgui/examples/`, `imgui/misc/`, `imgui/.github/`, or the `imstb_*.h` headers — those are unused vendored clutter in this project and out of scope.

- [ ] **Step 2: Replace the files in place**

Overwrite each file listed in **Files** with its `docking`-branch counterpart, preserving the existing file paths exactly (don't introduce a new directory structure).

- [ ] **Step 3: Reconfigure and build**

Run: `cmake ..` inside `build/`, then `cmake --build build`
Expected: builds clean. (A `docking`-branch Dear ImGui is designed to be behavior-compatible with `master` when `ImGuiConfigFlags_DockingEnable` is not set — which it isn't yet, since Task 3 adds that — so no existing code in this project should need to change for this build to succeed.)

- [ ] **Step 4: Manual verification — no behavior change yet**

Launch `build/bin/WaryEngine.exe` from `build/bin`. Confirm the editor looks and behaves pixel-identical to before this task: same fixed Hierarchy/Inspector panel positions, same inability to move/resize them, same colors. This proves the upgrade itself introduced no regression before Task 3 turns docking on.

- [ ] **Step 5: Commit**

```bash
git add imgui/imgui.h imgui/imgui.cpp imgui/imgui_internal.h imgui/imgui_tables.cpp imgui/imgui_widgets.cpp imgui/imgui_draw.cpp imgui/imgui_demo.cpp imgui/backends/imgui_impl_sdl2.cpp imgui/backends/imgui_impl_sdl2.h imgui/backends/imgui_impl_opengl3.cpp imgui/backends/imgui_impl_opengl3.h imgui/backends/imgui_impl_opengl3_loader.h
git commit -m "chore: upgrade vendored Dear ImGui to the docking branch"
```

---

### Task 2: `ViewportRect` gains a Y origin; Renderer/Gizmo/picking math generalized

**Files:**
- Modify: `include/Entity.h`
- Modify: `src/Renderer.cpp`
- Modify: `src/Gizmo.cpp`
- Modify: `src/EditorUI.cpp` (picking block only)

**Interfaces:**
- Consumes: the current `struct ViewportRect { float x, width, height; };` in `include/Entity.h`.
- Produces: `struct ViewportRect { float x, y, width, height; };` — every later task (Task 3's dock-layout-driven viewport computation) constructs and passes this 4-field version.

- [ ] **Step 1: Write the failing scratchpad test for the Y-flip formula**

In the session scratchpad, write a standalone `.cpp` with a declaration-only (no body) function matching this signature: `int GLViewportY(int windowHeight, float viewportY, float viewportHeight);` and assert, using independently hand-calculated values:
```cpp
// windowHeight=720, viewportY=0, viewportHeight=720 (today's always-full-height case) -> GL Y = 0
// windowHeight=720, viewportY=100, viewportHeight=500 (a panel docked below, pushing the bottom edge up) -> GL Y = 720-100-500 = 120
// windowHeight=720, viewportY=50, viewportHeight=670 (a thin panel docked above) -> GL Y = 720-50-670 = 0
```
Assert exact integer equality (this formula has no floating-point rounding concern at integer precision, but compute in float then cast to int, matching how `Renderer::Render` will call it).

- [ ] **Step 2: Run the scratchpad test to verify it fails**

It won't link (declared but undefined) — confirm the error is "undefined reference" / "unresolved external," not a typo in the test.

- [ ] **Step 3: Add `y` to `ViewportRect` in `include/Entity.h`**

`struct ViewportRect { float x, y, width, height; };`

- [ ] **Step 4: Fix `Renderer::Render`'s `glViewport` call in `src/Renderer.cpp`**

Current: `glViewport((int)viewport.x, 0, (int)viewport.width, (int)viewport.height);`
New: compute the GL Y via the flip formula (`windowHeight - viewport.y - viewport.height`) and pass it as the second argument instead of the literal `0`. `Renderer::Render` doesn't currently receive the window height as a parameter — add whatever minimal plumbing gets it there (an extra parameter, or read it via existing means if `Renderer` already has access — check the current signature and call site in `Engine.cpp` before deciding; builder's call on the exact mechanism, but don't restructure `Renderer::Render`'s existing parameter list beyond what's needed for this one value).

- [ ] **Step 5: Paste the implementation into the scratchpad and verify it passes**

Expected: all three assertions pass.

- [ ] **Step 6: Fix `Gizmo.cpp`'s `WorldToScreen`/`ScreenToWorld`**

Both currently assume `screenY` is measured from `0` (the window top) over a span of `viewport.height`. Change both to measure from `viewport.y` instead: in `WorldToScreen`, the final line becomes `screenY = viewport.y + (1.0f - ndcY) / 2.0f * viewport.height;` (was `(1.0f - ndcY) / 2.0f * viewport.height;`); in `ScreenToWorld`, `localY = screenY - viewport.y;` (was `screenY`).

- [ ] **Step 7: Fix `EditorUI::Draw`'s picking block in `src/EditorUI.cpp`**

The same assumption (`localMy = my;`) needs the same fix: `localMy = my - viewport.y;` (or the equivalent local variable name at the current call site — read the live file for the exact current variable names before editing).

- [ ] **Step 8: Build**

Run: `cmake --build build` (no new source files, no reconfigure needed)
Expected: builds clean.

- [ ] **Step 9: Manual verification — no behavior change yet**

Launch the exe. Since nothing yet constructs a `ViewportRect` with a non-zero `y` (Task 3 is what makes that possible), confirm the editor still looks and behaves exactly as before: same panel positions, same gizmo/picking alignment on the scene (click an entity, confirm it still selects correctly; show a gizmo, confirm the handles still land exactly on the entity).

- [ ] **Step 10: Commit**

```bash
git add include/Entity.h src/Renderer.cpp src/Gizmo.cpp src/EditorUI.cpp
git commit -m "refactor: generalize ViewportRect and its consumers to support a non-zero Y origin"
```

---

### Task 3: Enable docking, host a DockSpace, derive the viewport region from the dock layout

**Files:**
- Modify: `src/EditorUI.cpp`
- Modify: `.gitignore`
- Delete: `imgui.ini` (from git tracking; the file itself regenerates at runtime)

**Interfaces:**
- Consumes: `ViewportRect{x, y, width, height}` from Task 2; the now-available `ImGuiConfigFlags_DockingEnable`/`DockSpace`/`DockBuilder` APIs from Task 1.
- Produces: nothing new consumed by later tasks (Task 4 only touches colors, not structure).

- [ ] **Step 1: Enable docking**

In `EditorUI::Init`, after `ImGui::CreateContext()`/`ImGuiIO& io = ImGui::GetIO();`, add `io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;`.

- [ ] **Step 2: Host a DockSpace and build the first-run default layout**

In `EditorUI::Draw`, before the Hierarchy/Inspector `ImGui::Begin` calls, host a dockspace covering the full window (via `ImGui::DockSpaceOverViewport()` or an explicit borderless full-window host + `ImGui::DockSpace()` — builder's choice of whichever fits this codebase's existing per-frame window setup better). On first run (check via `ImGui::DockBuilderGetNode` returning null/no existing layout for this dockspace's ID, or an equivalent docking-branch idiom), use `ImGui::DockBuilder*` calls to split the dockspace into three nodes — Hierarchy docked left (~20% width), Inspector docked right (~25% width), remaining central node implicit — and dock the "Hierarchy"/"Inspector" windows into their respective nodes.

- [ ] **Step 3: Make Hierarchy/Inspector real dockable windows**

Remove `ImGuiWindowFlags_NoMove`/`NoResize` from both windows' flags (keep `NoCollapse` if it still reads as intentional — builder's call). Remove their per-frame `ImGui::SetNextWindowPos`/`SetNextWindowSize` calls — a docked window's position/size comes from the dock layout, not hardcoded coordinates.

- [ ] **Step 4: Compute the scene `ViewportRect` from the central dock node**

Replace the fixed-percentage `leftPanelWidth`/`rightPanelWidth`/`sceneX`/`sceneW`/`sceneH` calculation with one that reads the central dock node's actual on-screen position and size each frame (via whichever docking-branch API exposes a dock node's `Pos`/`Size` most directly — builder's call), producing a `ViewportRect{x, y, width, height}` that reflects wherever the user has actually arranged the layout. Keep the `Tips` and `GizmoToolbar` overlay windows exactly as they are today (not part of the dockspace, still positioned relative to the scene region as they are now — just fed the new 4-field `ViewportRect`'s `x`/`y` instead of the old 3-field one).

- [ ] **Step 5: Reset stale layout state**

Delete the tracked `imgui.ini` from the repo (`git rm imgui.ini`) and add a `imgui.ini` line to `.gitignore`.

- [ ] **Step 6: Build**

Run: `cmake --build build`
Expected: builds clean.

- [ ] **Step 7: Manual verification — full docking behavior checklist**

Launch the exe fresh (confirm no `imgui.ini` exists first, to genuinely test the first-run path — delete the regenerated one from `build/bin/` if present from a prior run before this specific check). Confirm:
- First launch shows the intended default layout: Hierarchy left, Inspector right, scene filling the center — not an empty/jumbled arrangement.
- Hierarchy and Inspector can be dragged by their title bars and resized by their edges.
- Dragging Inspector onto Hierarchy merges them into a tabbed panel (even with default/unstyled tab colors for now — Task 4 styles them) without crashing.
- Dragging a panel to dock **above or below** the scene viewport (not just left/right) — confirm the scene still renders at the correct position and size afterward (Review Focus item), and that clicking an entity still selects it correctly, and a gizmo's handles still land exactly on the selected entity, in this new layout.
- Closing the app and relaunching preserves whatever arrangement was left (via the regenerated `imgui.ini`).

- [ ] **Step 8: Commit**

```bash
git add src/EditorUI.cpp .gitignore
git rm --cached imgui.ini
git commit -m "feat: enable Dear ImGui docking with a default layout and dock-driven viewport region"
```

---

### Task 4: Menu bar + Reset Layout (added mid-plan, user request)

> Added after Task 3 shipped, per the user's explicit request: a top menu bar "like other engines have," plus a way to recover the default dock layout without deleting `imgui.ini` by hand. This revises the plan's original "Non-goals: menu bar" line in Global Constraints — superseded for this specific, scoped addition (File/Edit/View only, no icons, no additional menus) by the user's direct instruction.

**Files:**
- Modify: `src/EditorUI.cpp` (the first-run dock-building block, extracted into a reusable function; a new main menu bar)

**Interfaces:**
- Consumes: the existing `dockspaceId`, the first-run `DockBuilder*` split logic from Task 3 (to be refactored into a callable function, not duplicated).
- Produces: nothing new consumed by later tasks.

- [ ] **Step 1: Extract the default-layout construction into a reusable function**

Task 3's first-run block (`DockBuilderRemoveNode`/`AddNode`/`SetNodeSize`/`SplitNode`×2/`DockWindow`×3/`Finish`) currently only runs inline inside the `if (ImGui::DockBuilderGetNode(dockspaceId) == nullptr)` check. Factor that block's body into a small local function (e.g. a static free function in this file, or a lambda captured by the `Draw` call — builder's choice) taking `dockspaceId`/`winW`/`winH`, so it can be called from two places: the existing first-run check, and the new "Reset Layout" menu item below.

- [ ] **Step 2: Add the main menu bar**

Call `ImGui::BeginMainMenuBar()` / `EndMainMenuBar()` **before** the `DockSpaceOverViewport()` call in the same frame (ImGui's main-viewport work-area shrink from a main menu bar needs to happen before `DockSpaceOverViewport(dockspaceId, nullptr, ...)` reads the main viewport's work rect that same frame — reversing this order risks the exact class of one-frame-stale-geometry bug this plan's Task 3 spent multiple rounds fixing; call out this ordering explicitly in a comment so it isn't "fixed" backwards later). Menus and items:
  - **File**: `+ New Entity` (same push_back logic already in the Hierarchy panel — call a shared helper, don't duplicate the entity-construction literal), `Save` (sets `saveRequested = true`, same as the existing Hierarchy Save button).
  - **Edit**: `Undo` (calls whatever the existing Ctrl+Z path calls in `Engine.cpp` — expose it or replicate its exact one-step pop-from-`undoStack` logic; your call on the cleanest wiring given `Draw`'s current parameters, which already include `undoStack` by reference).
  - **View**: `Reset Layout` — calls the Step 1 function to rebuild the default split immediately (not just on next launch), regardless of whatever's currently saved.

- [ ] **Step 3: Build**

Run: `cmake --build build`
Expected: builds clean.

- [ ] **Step 4: Manual verification**

Launch the exe. Confirm: a menu bar appears at the very top of the window, above all docked panels, spanning the full width. File > Save actually saves (check `scene.wary`'s contents change). File > + New Entity adds an entity, same as the Hierarchy button. Edit > Undo undoes the last change. Drag panels into a messy arrangement, then View > Reset Layout restores the original default split immediately, in the same running session (not requiring a relaunch) — this is the main new-risk item to verify carefully, since it exercises the same `DockBuilder` machinery Task 3's bugs lived in. Also re-confirm the menu bar itself doesn't overlap or get overlapped by the Hierarchy/Scene/Inspector panels at the top edge.

- [ ] **Step 5: Commit**

```bash
git add src/EditorUI.cpp
git commit -m "feat: add menu bar (File/Edit/View) and a Reset Layout command"
```

---

### Task 5: Theme rework — red accent, no gradients, no purple, styled dock tabs

**Files:**
- Modify: `src/EditorUI.cpp` (`SetupUIStyle`, and the Gizmo toolbar's active-button highlight)

**Interfaces:**
- Consumes: nothing structural from Tasks 1-4; purely a styling pass over the already-working docking/editor and menu bar.
- Produces: nothing consumed by later tasks (Task 6 is docs-only).

- [ ] **Step 1: Define the accent color once**

Add a single named constant (e.g. a `static const ImVec4 kAccentColor = ImVec4(...);` near the top of `EditorUI.cpp`, or wherever `SetupUIStyle` already lives) for the new red accent — builder's choice of exact value within "red, not orange, not neon/warning-red, no purple." Use it everywhere the old orange literal (`ImVec4(0.94f, 0.50f, 0.09f, 1.00f)`) currently appears: `SetupUIStyle`'s `CheckMark`/`SliderGrabActive`, and the Gizmo toolbar's active-mode button highlight (three known call sites from the gizmo work — find any others by searching for the literal).

- [ ] **Step 2: Style the dock tab colors**

Set `ImGuiCol_Tab`, `ImGuiCol_TabHovered`, `ImGuiCol_TabActive`, `ImGuiCol_TabUnfocused`, `ImGuiCol_TabUnfocusedActive` in `SetupUIStyle` to values in the same dark/red palette (not Dear ImGui's default blue-ish tab colors) — these are newly visible now that Task 3 makes tabbed panels reachable.

- [ ] **Step 3: Audit the rest of the palette**

Read through every color `SetupUIStyle` sets. Confirm: no purple/violet hue anywhere, no color that reads as part of a gradient pair (since this is a flat-color API, this means no two adjacent/related colors that look like they're meant to blend — e.g. don't make `FrameBg`/`FrameBgHovered`/`FrameBgActive` a purple-to-red ramp). Adjust any that don't fit the "professional, not AI-generic" goal from the spec.

- [ ] **Step 4: Build**

Run: `cmake --build build`
Expected: builds clean.

- [ ] **Step 5: Manual verification**

Launch the exe. Confirm: checkboxes, active sliders, and the Gizmo toolbar's active-mode button all show the new red (Review Focus item — check every spot the old orange appeared). Dock two panels into tabs and confirm the tabs are styled in the new palette, not Dear ImGui's default blue (Review Focus item). Confirm nothing in the UI looks purple or gradiented anywhere.

- [ ] **Step 6: Commit**

```bash
git add src/EditorUI.cpp
git commit -m "style: rework editor theme to a red accent, remove orange, style dock tabs"
```

---

### Task 6: Push to origin/main

**Files:**
- Create: none — this task only commits the already-written spec/plan docs and pushes.

- [ ] **Step 1: Commit the spec and plan docs**

```bash
git add docs/superpowers/specs/2026-11-17-editor-visual-theme-design.md docs/superpowers/plans/2026-11-17-editor-docking-theme.md
git commit -m "docs: add editor docking/theme design doc and implementation plan"
```

- [ ] **Step 2: Push**

```bash
git push origin main
```

- [ ] **Step 3: Verify**

Run: `git log origin/main -1 --oneline`
Expected: the pushed commit matches local `HEAD`.
