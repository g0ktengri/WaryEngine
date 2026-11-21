# ECS Migration Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the monolithic `Entity` struct with a hand-rolled sparse-set ECS (`EntityID` + per-component `SparseSet<T>` storage behind a `Registry`), migrating every consumer (`Renderer`, `EditorUI`, `Gizmo`, `Engine`) to it with zero behavior change except two small, deliberate, documented corrections to today's accidental selection-after-mutation quirks.

**Architecture:** Three new files (`Components.h`, `SparseSet.h`, `Registry.h`/`.cpp`) introduce the storage. `Entity` is deleted from `Entity.h` (which keeps `Particle`/`Camera`/`ViewportRect`, untouched). `Renderer::Render`, `EditorUI::Draw`, `Gizmo::Draw`/`Update`, and all of `Engine` are rewritten against `Registry`/`EntityID` in one combined task, because their call sites are mutually coupled (`Engine::Run()` calls both `Renderer::Render` and `EditorUI::Draw` with the same entity storage; `EditorUI::Draw` calls `Gizmo::Draw`/`Update`) — there is no buildable intermediate state that updates one without the others.

**Tech Stack:** C++20, SDL2, OpenGL 3.3 core (glad), Dear ImGui (docking branch).

**Spec:** `docs/superpowers/specs/2026-11-21-ecs-migration-design.md`

## Global Constraints

- No automated test framework (standing project convention). Verification per task is build success, scratchpad RED→GREEN proofs for non-obvious data-structure logic, and live manual GUI comparison (same scene, same actions, before/after).
- Working directly on `main`, no worktree, push by default.
- `Particle`/`Camera`/`ViewportRect` in `Entity.h` are untouched — only the `Entity` struct itself is removed from that file. `Particle` is explicitly **not** migrated into the ECS (Non-Goal).
- `EntityID = uint32_t`, a plain reusable integer — no generational handle.
- `name`/`active` are plain per-entity fields on `Registry` (parallel-indexed by `EntityID`), **not** components.
- `SparseSet<T>::Remove` must be order-preserving (erase+shift, like `std::vector::erase`) — never swap-and-pop — so Hierarchy order, click-picking's "topmost = last in iteration" semantics, and render stacking order all stay exactly as they are today.
- Selection (`EntityID selectedEntity`, with a sentinel for "nothing selected") is reset to the sentinel after delete, after either undo path (Ctrl+Z and Edit > Undo), and after `LoadScene` — a deliberate, documented small improvement over today's fragile index-reinterpretation (see spec §1a). Startup still selects the initial Player.
- The undo stack's current, *inconsistent* trimming must be preserved exactly: only the edit-gesture-completion push (`EditorUI.cpp`'s `wasActive`→false transition) trims to 50 entries; the spawn and delete pushes stay uncapped, as they are today.
- `EntitySnapshot` holds plain (non-`optional`) `Transform`/`Sprite`/`Physics` values — no defensive handling anywhere for a component combination nothing in this codebase produces.
- `CMakeLists.txt` globs `src/*.cpp` — adding `src/Registry.cpp` needs a fresh `cmake` configure (not just a build) to be picked up the first time.

## Review Focus

- **Selection after a structural change.** Delete the currently-selected entity, undo (both Ctrl+Z and Edit > Undo), and reload a saved scene — each must leave `selectedEntity` at the "nothing selected" sentinel (Inspector/GizmoToolbar hidden), not pointing at an arbitrary other entity by stale position, matching the deliberate behavior change in the spec.
- **Order preservation through add/remove churn.** A `SparseSet<T>` that's had entities added, removed from the middle, and re-added must still iterate in insertion order for the entities that remain — this is what keeps Hierarchy listing, click-picking priority, and render stacking order unchanged; a swap-and-pop slip here would silently reorder all three at once.
- **Collision/physics against a non-selected entity.** The physics loop's gravity+ground-collision and the WASD movement's collision-blocking both check the *other* entities' `Physics.isStatic`/`active` — this must still work when "self" and "other" live in different sparse-set slots after migration, including a case with 3+ entities where the colliding pair isn't adjacent in creation order.
- **Save/load round-trip.** Save a scene with several non-default entities (varied position/rotation/scale/color/texture/gravity/static), reload it (relaunch or re-trigger `LoadScene`), and confirm every field matches — `scene.wary`'s format is unchanged, but it's now read into/written from `Registry` component data instead of a `vector<Entity>`.
- **Undo granularity after the migration.** One full gizmo drag (press→move→release) must still produce exactly one undo entry, not one per frame; one Debug-menu stress-spawn click must still push zero undo entries (standing ruling, unaffected by this migration, but worth re-confirming since the snapshot's shape changed from `vector<Entity>` to `vector<EntitySnapshot>`).

---

### Task 1: `SparseSet<T>` primitive

**Files:**
- Create: `include/SparseSet.h`

**Interfaces:**
- Consumes: nothing.
- Produces: `template <typename T> class SparseSet` with `void Add(EntityID, T)`, `void Remove(EntityID)`, `bool Has(EntityID) const`, `T& Get(EntityID)`, `const std::vector<EntityID>& Ids() const`, `std::vector<T>& Dense()`. `EntityID` is `uint32_t` — define it here (Task 2's `Components.h` and `Registry.h` both depend on this definition, so this is its one source).

- [ ] **Step 1: Write the `SparseSet<T>` class**

Header-only template, per the design doc §3:
```cpp
#ifndef SPARSE_SET_H
#define SPARSE_SET_H
#include <vector>
#include <cstdint>

using EntityID = uint32_t;

template <typename T>
class SparseSet {
public:
    void Add(EntityID e, T value) {
        if (Has(e)) { dense[(size_t)sparse[e]] = std::move(value); return; }
        if (e >= sparse.size()) sparse.resize(e + 1, -1);
        sparse[e] = (int)dense.size();
        denseIds.push_back(e);
        dense.push_back(std::move(value));
    }
    void Remove(EntityID e) {
        if (!Has(e)) return;
        size_t idx = (size_t)sparse[e];
        denseIds.erase(denseIds.begin() + idx);
        dense.erase(dense.begin() + idx);
        sparse[e] = -1;
        for (size_t i = idx; i < denseIds.size(); i++) sparse[denseIds[i]] = (int)i;
    }
    bool Has(EntityID e) const { return e < sparse.size() && sparse[e] != -1; }
    T& Get(EntityID e) { return dense[(size_t)sparse[e]]; }
    const std::vector<EntityID>& Ids() const { return denseIds; }
    std::vector<T>& Dense() { return dense; }
private:
    std::vector<int> sparse;
    std::vector<EntityID> denseIds;
    std::vector<T> dense;
};
#endif
```
(This is a complete reference implementation — the design left no judgment calls here per its "What's Left to the Builder" note; transcribe and verify it rather than redesigning it.)

- [ ] **Step 2: Scratchpad RED→GREEN proof — order preservation through churn**

In the session's scratchpad directory (not the project), write a standalone `.cpp` that:
1. Creates a `SparseSet<int>`, adds entities 0..4 (values 100..104) in order.
2. Asserts `Ids()` is exactly `{0,1,2,3,4}` and `Dense()` is exactly `{100,101,102,103,104}`.
3. Removes entity 2 (the middle one). Asserts `Ids()` is exactly `{0,1,3,4}` (order-preserving — not `{0,1,4,3}`, which is what swap-and-pop would produce) and `Dense()` is exactly `{100,101,103,104}`.
4. Asserts `Has(2)` is false and `Has(3)`/`Has(4)` are still true, with `Get(3)==103`/`Get(4)==104` (confirms `sparse[]` was correctly re-indexed after the shift, not just the dense arrays).
5. Re-adds a new entity 2 with value 200. Asserts it lands at the *end* (`Ids() == {0,1,3,4,2}`), not back in its old slot (a sparse set has no notion of "slots," only current membership — confirms the implementation doesn't accidentally special-case this).
6. Write this with a deliberately-wrong `Remove` first (e.g. a swap-and-pop version, or one that forgets to re-index `sparse[]` after the shift) to confirm the assertions actually fail (RED) before switching to the real implementation above (GREEN). This is the one piece of this task's logic subtle enough to warrant the proof — not a formality.

- [ ] **Step 3: Run the proof, confirm RED then GREEN**

Compile and run against the deliberately-wrong version first (expect an assertion failure — note which one and why it's expected), then against the real implementation (expect every assertion to pass, clean exit).

- [ ] **Step 4: Build the main project**

Run: `cmake --build build --config Debug`
Expected: builds with no new errors/warnings (nothing includes `SparseSet.h` yet, so this just confirms the existing build is unaffected).

- [ ] **Step 5: Commit**

```bash
git add include/SparseSet.h
git commit -m "feat: add SparseSet<T>, the storage primitive for the ECS migration"
```

---

### Task 2: Components + `Registry`

**Files:**
- Create: `include/Components.h`
- Create: `include/Registry.h`
- Create: `src/Registry.cpp`

**Interfaces:**
- Consumes: `EntityID` from `SparseSet.h` (Task 1).
- Produces: `Transform`/`Sprite`/`Physics` structs; `Registry` class with `EntityID Create(const std::string& name)`, `void Destroy(EntityID)`, `const std::vector<EntityID>& Entities() const`, `std::string& Name(EntityID)`, `bool& Active(EntityID)`, public members `SparseSet<Transform> transforms`, `SparseSet<Sprite> sprites`, `SparseSet<Physics> physics`; `EntitySnapshot` struct (`name`, `active`, `Transform`, `Sprite`, `Physics`, all by value) plus `std::vector<EntitySnapshot> Registry::Snapshot() const` and `void Registry::Restore(const std::vector<EntitySnapshot>&)`. These five names/signatures are what every later task calls — treat them as fixed.

- [ ] **Step 1: Define the components**

In `include/Components.h`:
```cpp
#ifndef COMPONENTS_H
#define COMPONENTS_H
#include <string>

struct Transform { float x = 0, y = 0, rotation = 0, sx = 1, sy = 1; };
struct Sprite     { float color[3] = {1,1,1}; std::string textureName = "default"; };
struct Physics    { bool hasGravity = false, isStatic = false; float vy = 0; };

#endif
```

- [ ] **Step 2: Declare `Registry` and `EntitySnapshot`**

In `include/Registry.h`:
```cpp
#ifndef REGISTRY_H
#define REGISTRY_H
#include <string>
#include <vector>
#include "SparseSet.h"
#include "Components.h"

struct EntitySnapshot {
    std::string name;
    bool active;
    Transform transform;
    Sprite sprite;
    Physics physics;
};

class Registry {
public:
    EntityID Create(const std::string& name);
    void Destroy(EntityID e);
    const std::vector<EntityID>& Entities() const { return liveEntities; }

    std::string& Name(EntityID e) { return names[e]; }
    bool& Active(EntityID e) { return actives[e]; }

    std::vector<EntitySnapshot> Snapshot() const;
    void Restore(const std::vector<EntitySnapshot>& snapshot);

    SparseSet<Transform> transforms;
    SparseSet<Sprite> sprites;
    SparseSet<Physics> physics;

private:
    std::vector<EntityID> liveEntities; // insertion order
    std::vector<std::string> names;     // parallel-indexed by EntityID, grown as needed
    std::vector<bool> actives;          // parallel-indexed by EntityID, grown as needed
    EntityID nextId = 0;
};
#endif
```

- [ ] **Step 3: Implement `Create`/`Destroy`**

In `src/Registry.cpp`: `Create` allocates `EntityID id = nextId++`, grows `names`/`actives` to fit (resize with a default), sets `names[id] = name; actives[id] = true;`, appends to `liveEntities`, returns `id`. `Destroy` erases `id` from `liveEntities` (order-preserving — `std::find` + `erase`, mirroring today's `entities.erase(entities.begin() + selected)`), and calls `transforms.Remove(id)`/`sprites.Remove(id)`/`physics.Remove(id)` (each a no-op via `Has()` if that entity never had the component — not relevant today since every entity always gets all three, but keeps `Destroy` correct regardless).

- [ ] **Step 4: Implement `Snapshot`/`Restore`**

`Snapshot()`: walk `liveEntities`, for each `id` read `names[id]`, `actives[id]`, `transforms.Get(id)`, `sprites.Get(id)`, `physics.Get(id)` into an `EntitySnapshot`, collect into the returned vector (iteration order == `liveEntities` order, i.e. insertion order — this is what keeps a restored scene's Hierarchy/pick/render order identical to what was snapshotted).

`Restore(snapshot)`: clear every component `SparseSet` and `liveEntities`/`names`/`actives` back to empty (do **not** reset `nextId` — freshly-created entities after a restore must not collide with any `EntityID` a live reference, e.g. a stale `selectedEntity`, might still hold. Since selection is reset to the sentinel on every undo/restore per Global Constraints, this is mostly precautionary, but it's the correct invariant regardless and costs nothing to keep). Then for each `EntitySnapshot` in order, call `Create(name)`, set `Active(id) = active`, and `Add` its `Transform`/`Sprite`/`Physics`.

- [ ] **Step 5: Scratchpad proof — snapshot/restore round-trip**

In the scratchpad directory: create a `Registry`, create 3 entities with distinct component values, destroy the middle one, take a `Snapshot()`, assert its size is 2 and its contents match the 2 remaining entities in insertion order, then `Restore()` a hand-built 2-entity snapshot into a *different* `Registry` instance and assert `Entities()`/`Name()`/component values all match what was restored. Watch this fail first against a deliberately-broken `Restore` (e.g. one that forgets to re-add a component), then pass against the real implementation.

- [ ] **Step 6: Re-run `cmake` configure, then build**

Run: `cmake -S . -B build` (configure step — required because `src/Registry.cpp` is new and `CMakeLists.txt`'s `file(GLOB_RECURSE SOURCES "src/*.cpp")` only re-scans on configure, not on every build), then `cmake --build build --config Debug`.
Expected: configure succeeds, build succeeds with no new errors/warnings (still nothing in the existing engine includes `Registry.h` yet).

- [ ] **Step 7: Commit**

```bash
git add include/Components.h include/Registry.h src/Registry.cpp
git commit -m "feat: add Components and Registry — the ECS's entity/component storage"
```

---

### Task 3: Migrate `Gizmo`, `EditorUI`, `Renderer`, and `Engine` to the `Registry`

**Files:**
- Modify: `include/Entity.h` (remove the `Entity` struct; keep `Particle`/`Camera`/`ViewportRect`)
- Modify: `include/Gizmo.h`, `src/Gizmo.cpp`
- Modify: `include/EditorUI.h`, `src/EditorUI.cpp`
- Modify: `include/Renderer.h`, `src/Renderer.cpp`
- Modify: `include/Engine.h`, `src/Engine.cpp`

**Interfaces:**
- Consumes: `Registry`/`EntitySnapshot`/`Components.h` (Tasks 1-2).
- Produces: nothing further downstream — this is the plan's last task. `Renderer::Render` and `EditorUI::Draw`'s public signatures both change (unlike Faz 2b, which explicitly preserved them — see Global Constraints); nothing outside this codebase calls either.

This task is large because these four files' call sites are mutually coupled (`Engine::Run()` calls both `Renderer::Render(entities, ...)` and `editorUI.Draw(entities, ..., selectedEntity, undoStack, ...)` against the *same* entity storage every frame; `EditorUI::Draw` calls `gizmo.Draw`/`Update(entities[selected], ...)`) — there is no buildable intermediate state that migrates one without the others. Work the steps in the order given; each sub-step names the exact replacement for one piece of today's code.

- [ ] **Step 1: Remove `Entity` from `include/Entity.h`**

Delete the `struct Entity { ... };` block. Leave `Particle`, `Camera`, `ViewportRect` exactly as they are.

- [ ] **Step 2: Migrate `Gizmo`**

`include/Gizmo.h`: replace `#include "Entity.h"` with `#include "Components.h"`. Change `Draw(ImDrawList*, const Entity& entity, ...)` to `Draw(ImDrawList*, const Transform& transform, ...)` and `Update(Entity& entity, ...)` to `Update(Transform& transform, ...)`. `Camera`/`ViewportRect` parameters are unchanged (still from `Entity.h`, which `Gizmo.h` must now include alongside `Components.h`, or get transitively — check what already declares them and include accordingly).

`src/Gizmo.cpp`: every `entity.x`/`entity.y`/`entity.rotation`/`entity.sx`/`entity.sy` read/write becomes `transform.x`/`.y`/`.rotation`/`.sx`/`.sy` (rename the parameter, the field accesses are identical — `Transform` has the same field names as `Entity`'s transform fields). No logic changes.

- [ ] **Step 3: Migrate `EditorUI`'s declaration**

`include/EditorUI.h`: replace `#include "Entity.h"` with `#include "Registry.h"`. Change:
```cpp
ViewportRect Draw(SDL_Window* window, Registry& registry, const Camera& cam,
                   EntityID& selected, std::vector<std::vector<EntitySnapshot>>& undoStack,
                   bool& saveRequested, const std::vector<std::string>& textureList);
```
Change `CheckPointInside(Entity& e, float px, float py)` to `CheckPointInside(const Transform& t, float px, float py)`. Change `std::vector<Entity> preEditState;` to `std::vector<EntitySnapshot> preEditState;`. Add a sentinel constant, e.g. `static constexpr EntityID kNoSelection = UINT32_MAX;` (public, so `Engine.h`/`.cpp` can use the same value — see Step 6).

- [ ] **Step 4: Migrate `EditorUI`'s implementation — entity creation and the stress-spawn menu**

`SpawnNewEntity` (currently a free function taking `std::vector<Entity>&`): change its signature to take `Registry& registry` and `std::vector<std::vector<EntitySnapshot>>& undoStack`. Body becomes: `undoStack.push_back(registry.Snapshot());` then `EntityID id = registry.Create("Prop"); registry.transforms.Add(id, {cam.x, cam.y, 0, 0.3f, 0.3f}); registry.sprites.Add(id, {{1,1,1}, "default"}); registry.physics.Add(id, {false, false, 0});`. Both call sites (File menu's "+ New Entity" and Hierarchy's own button) pass `registry`/`undoStack` instead of `entities`/`undoStack`.

Debug > "Spawn 1000 Test Entities": same translation — each loop iteration becomes `EntityID id = registry.Create("Test"); registry.transforms.Add(id, {px, py, 0, 0.1f, 0.1f}); registry.sprites.Add(id, {{r,g,b}, "default"}); registry.physics.Add(id, {false, false, 0});`, still no undo push (standing ruling, unchanged).

- [ ] **Step 5: Migrate `EditorUI`'s implementation — gizmo, edit-gesture undo capture, and picking**

Gizmo block: `gizmo.Draw(ImGui::GetForegroundDrawList(), registry.transforms.Get(selected), cam, viewport)` and `gizmo.Update(registry.transforms.Get(selected), cam, viewport, ...)`, guarded by `selected != EditorUI::kNoSelection` instead of today's `selected >= 0 && selected < (int)entities.size()`.

Edit-gesture undo capture (`wasActive`/`preEditState`): `preEditState = registry.Snapshot();` on the false→true transition; `undoStack.push_back(preEditState); if (undoStack.size() > 50) undoStack.erase(undoStack.begin());` on the true→false transition — unchanged logic, just `EntitySnapshot` instead of `Entity` as the stored value type.

Picking: iterate `registry.Entities()` in reverse (same `for (int k = (int)entities.size()-1; ...)` intent, now over `const auto& ids = registry.Entities(); for (int k = (int)ids.size()-1; k >= 0; k--) { EntityID id = ids[k]; if (CheckPointInside(registry.transforms.Get(id), wx, wy)) { selected = id; break; } }`).

- [ ] **Step 6: Migrate `EditorUI`'s implementation — Hierarchy, Inspector, delete**

Hierarchy: `for (EntityID id : registry.Entities()) { if (ImGui::Selectable((registry.Name(id) + "##" + std::to_string(id)).c_str(), id == selected)) selected = id; }` (the `##`-disambiguator now uses the stable `EntityID` instead of a position index — strictly better, since today's position-based suffix could in principle collide after a mid-list deletion shifts indices; not a behavior anyone could have observed as a bug, just a byproduct of the identity now being real).

Inspector: guard on `selected != EditorUI::kNoSelection` (not a range check). Replace `Entity &e = entities[selected];` with direct references: `Transform& t = registry.transforms.Get(selected); Sprite& s = registry.sprites.Get(selected); Physics& p = registry.physics.Get(selected); std::string& name = registry.Name(selected);`. Every field access below it (`e.name`→`name`, `e.textureName`→`s.textureName`, `e.x`/`e.y`/`e.rotation`→`t.x`/`t.y`/`t.rotation`, `e.sx`/`e.sy`→`t.sx`/`t.sy`, `e.color`→`s.color`, `e.hasGravity`/`e.isStatic`→`p.hasGravity`/`p.isStatic`) translates one-to-one — no widget logic changes.

DELETE ENTITY button: `undoStack.push_back(registry.Snapshot()); registry.Destroy(selected); selected = EditorUI::kNoSelection;` (the sentinel reset is the one deliberate behavior change from spec §1a — today's `selected = 0` is replaced, not translated).

Edit > Undo menu item's inline restore: `if (!undoStack.empty()) { registry.Restore(undoStack.back()); undoStack.pop_back(); selected = EditorUI::kNoSelection; }` (same sentinel-reset reasoning).

- [ ] **Step 7: Migrate `Renderer`**

`include/Renderer.h`: replace `#include "Entity.h"` with `#include "Registry.h"`; change `Render(std::vector<Entity>& entities, std::vector<Particle>& particles, ...)` to `Render(Registry& registry, std::vector<Particle>& particles, ...)`.

`src/Renderer.cpp`'s entity-batching loop (`for (auto &e : entities) { ... }`): becomes `for (EntityID id : registry.Entities()) { const Transform& t = registry.transforms.Get(id); const Sprite& s = registry.sprites.Get(id); auto it = textures.find(s.textureName); unsigned int tid = (it != textures.end()) ? it->second : defaultTid; float tm[16]; CreateTransform(tm, t.x, t.y, t.rotation, t.sx, t.sy); AppendQuad(entityBatches[tid], tm, s.color, 1.0f); lastEntityTid = tid; }` — same grouping-by-resolved-texture-id logic, same alpha-1.0 convention (entities still have no alpha field), reading from components instead of one struct. The particle pass is entirely unchanged (particles were never part of this migration).

- [ ] **Step 8: Migrate `Engine`**

`include/Engine.h`: keep `#include "Entity.h"` (for `Particle`/`Camera`/`ViewportRect`) but add `#include "Registry.h"`. Change `std::vector<Entity> entities;` to `Registry registry;`. Change `int selectedEntity;` to `EntityID selectedEntity;`. Change `std::vector<std::vector<Entity>> undoStack;` to `std::vector<std::vector<EntitySnapshot>> undoStack;`. Change `bool CheckCollision(const Entity &a, const Entity &b);` to `bool CheckCollision(const Transform &a, const Transform &b);`.

`src/Engine.cpp`:
- Constructor: drop `selectedEntity(0)` from the initializer list (it's set explicitly in `Init()` instead, once the Player exists — see below); default-construct `registry`/`undoStack` as usual.
- `Init()`: replace `entities.push_back({"Player", 0, 0, 0, 0.4f, 0.4f, {1, 1, 1}, "default", true, false, 0});` with `EntityID player = registry.Create("Player"); registry.transforms.Add(player, {0, 0, 0, 0.4f, 0.4f}); registry.sprites.Add(player, {{1,1,1}, "default"}); registry.physics.Add(player, {false, false, 0}); selectedEntity = player;` (this is what makes "startup selects Player" hold under the new identity scheme — explicit, not incidental).
- `Run()`: `editorUI.Draw(window, registry, cam, selectedEntity, undoStack, saveRequested, renderer.GetTextureList())` and `renderer.Render(registry, particles, cam, viewport, winH)`.
- `Update()`'s Undo call site is unchanged (`Undo()` still means Ctrl+Z); its entity-control block: guard on `selectedEntity != EditorUI::kNoSelection` instead of the range check, replace `Entity &e = entities[selectedEntity];` with `Transform& t = registry.transforms.Get(selectedEntity);` (movement writes `t.x`/`t.y`), and the two collision-check loops over `entities` become loops over `registry.Entities()` reading `registry.physics.Get(otherId).isStatic`/`registry.Active(otherId)` and `registry.transforms.Get(otherId)`. Today's self-skip is pointer identity (`&e == &other`), which no longer applies once there's no single `Entity&` to compare — replace it with an `EntityID` comparison (`otherId == selectedEntity`), which is the more correct translation anyway now that a real stable identity exists: `if (otherId == selectedEntity || !registry.Active(otherId) || !registry.physics.Get(otherId).isStatic) continue;`, same `CheckCollision(t, registry.transforms.Get(otherId))` call shape. Particle spawn on SPACE reads `registry.transforms.Get(selectedEntity).x/.y` in place of `entities[selectedEntity].x/.y`.
- The standalone Physics loop (gravity + ground collision): iterate `registry.Entities()` as `id`, skip unless `registry.physics.Get(id).hasGravity && !registry.physics.Get(id).isStatic`, mutate `registry.transforms.Get(id).y` and `registry.physics.Get(id).vy`, with the inner "other" loop over `registry.Entities()` as `otherId`, same `EntityID`-comparison self-skip as above (`otherId == id`, not pointer identity) reading `registry.Active(otherId)`/`registry.physics.Get(otherId).isStatic`/`registry.transforms.Get(otherId)` exactly as today's inner loop does against `entities`.
- `SaveScene()`: `for (EntityID id : registry.Entities()) { const Transform& t = ...; const Sprite& s = ...; const Physics& p = ...; f << std::quoted(registry.Name(id)) << " " << t.x << " " << t.y << " " << t.rotation << " " << t.sx << " " << t.sy << " " << s.color[0] << " " << s.color[1] << " " << s.color[2] << " " << p.hasGravity << " " << p.isStatic << " " << std::quoted(s.textureName) << "\n"; }` — identical field order and format to today, just read from components.
- `LoadScene()`: clear the registry first (there's no existing "clear" method — `Registry::Restore({})` with an empty snapshot does exactly this, reusing Step 4's logic rather than adding a second clearing path), then for each parsed line, `EntityID id = registry.Create(name); registry.transforms.Add(id, {x, y, r, sx, sy}); registry.sprites.Add(id, {{c0,c1,c2}, tName}); registry.physics.Add(id, {(bool)g, (bool)st, 0});`, and finally `selectedEntity = EditorUI::kNoSelection;` (deliberate — today's code never explicitly sets selection after `LoadScene`, leaving the constructor's stale `0`; see spec §1a for why this migration makes that explicit instead of accidental).
- `CheckCollision`: parameter types change from `const Entity&` to `const Transform&`; body (`a.x`/`a.y`/`a.sx`/`a.sy` vs `b.x`/`b.y`/`b.sx`/`b.sy`) is unchanged.
- `Undo()`: `if (!undoStack.empty()) { registry.Restore(undoStack.back()); undoStack.pop_back(); selectedEntity = EditorUI::kNoSelection; }`.

- [ ] **Step 9: Build**

Run: `cmake --build build --config Debug`
Expected: builds with no new errors/warnings. If the engine process from a previous run is locked, close it first — do not attempt to kill it yourself.

- [ ] **Step 10: Live verification — the full Review Focus list plus a general regression pass**

Run the engine and walk through, in order:
1. **Startup:** Player is selected (Inspector/GizmoToolbar visible immediately).
2. **Spawn + pick:** "+ New Entity" a couple of times, click to select different entities (including ones created earlier vs. later — confirms topmost-last-created pick priority is unchanged), confirm Hierarchy highlights match.
3. **Gizmo, all 3 modes:** Move, Rotate, Scale a selected entity each at least once; confirm one Ctrl+Z after each undoes exactly that one action (not more, not less) — this is the "Undo granularity" Review Focus item.
4. **Inspector:** edit name, texture (needs a non-default PNG in `assets/`), position, rotation, both scale modes (locked and unlocked), color, Gravity, Is Static — confirm each reflects live in the viewport.
5. **Delete:** delete the selected entity; confirm selection becomes "nothing" (Inspector/GizmoToolbar disappear) — this is the deliberate behavior change from spec §1a, confirm it reads as sensible, not broken.
6. **Undo both paths:** Ctrl+Z and Edit > Undo each at least once, confirming both still work and both reset selection to "nothing" afterward.
7. **Stress-spawn + undo-cap check:** Debug > Spawn 1000 Test Entities a few times; confirm no undo entry was pushed for it (Ctrl+Z right after should undo whatever your *last real edit* was, not un-spawn the 1000).
8. **Physics + collision:** select an entity with Gravity on, drop it onto a static platform entity; confirm it lands and stops (not falls through, not jitters) — exercises the "collision against a non-selected entity" Review Focus item.
9. **Particles:** hold SPACE on a selected entity; confirm particles still spawn and render (unaffected by this migration, confirm it stays that way).
10. **Save/load round-trip:** build a small scene with 3+ varied entities, Save, then relaunch the engine (or otherwise re-trigger `LoadScene`) and confirm every entity's every field matches — the "Save/load round-trip" Review Focus item.
11. **Scale/batching still fine:** Debug > Spawn 1000 Test Entities up to ~5000 total and confirm the FPS overlay still shows a healthy number and nothing crashes — confirms Faz 2b's batching integrates cleanly with Registry-sourced entities (it only ever needed `Transform`+`Sprite` per entity, which it now reads from components instead of an `Entity`).
12. **Grid:** confirm the infinite grid still renders normally (unaffected by this migration, confirm it stays that way).

- [ ] **Step 11: Commit**

```bash
git add include/Entity.h include/Gizmo.h src/Gizmo.cpp include/EditorUI.h src/EditorUI.cpp include/Renderer.h src/Renderer.cpp include/Engine.h src/Engine.cpp
git commit -m "refactor: migrate Entity to a sparse-set ECS (Registry + Transform/Sprite/Physics)"
```
