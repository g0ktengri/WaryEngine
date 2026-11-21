# ECS Migration — Design Doc

## Intent & Why

Faz 2 of the Wary Engine "level up" roadmap is architecture: ECS is its last
piece (2a Renderer/UI split — done, 2b batching — done, 2c ECS — this doc).
The motivation is the same one driving the whole roadmap: demonstrate real
engine-architecture depth to engine/graphics-minded reviewers of this
portfolio project. Today, `Entity` (`include/Entity.h`) is one monolithic
struct carrying transform, rendering, and physics data together, and four
unrelated systems (`Renderer`, `EditorUI`, `Gizmo`, `Engine`) all reach into
its fields directly. This migration decomposes `Entity` into independent
components stored in per-type sparse-set containers, addressed by a
lightweight `EntityID`, and rewires every consumer to go through a
`Registry` instead of a `std::vector<Entity>&`.

## Goals

- Replace `Entity` as a monolithic struct with a sparse-set ECS: per-component-type
  packed storage, `EntityID`-based lookup, `Registry::View<...>()`-style
  iteration for systems that need several component types at once.
- Preserve every current behavior exactly: entity creation (`+ New Entity`,
  stress-spawn), selection, click-to-pick, gizmo editing, Inspector editing,
  deletion, undo/redo, save/load (`scene.wary`), physics (gravity, AABB
  collision), and rendering (including Faz 2b's texture-batched draw path).
- Keep entity iteration/removal order-preserving (matches today's
  `std::vector::erase`-based deletion) so picking's "topmost = last in
  order" semantics and render stacking order don't silently change — unlike
  Faz 2b's accepted cross-texture-group reordering, this migration has no
  reason to introduce a new reordering, since nothing about moving to
  component storage requires one.

## Non-Goals

- Multithreading, job systems, or any parallel system execution.
- A general event/messaging bus between systems.
- Serialization changes — `scene.wary`'s save/load format and `SaveScene`/`LoadScene` stay exactly as they are (they reconstruct full entities field-by-field; under the new architecture they'll do so via the Registry's create+add-component calls instead of `vector::push_back`, but the file format and the fields it reads/writes are unchanged).
- Migrating `Particle` into the ECS. Particles are transient, bulk-spawned, never selected/inspected/picked, and already render as their own batched pass (Faz 2b). Folding them into component storage would add generality this project never uses for them. `Engine`'s `std::vector<Particle> particles` stays exactly as-is.
- Demonstrating entity heterogeneity for its own sake. Every entity this engine creates today gets all of Transform+Sprite+Physics+Meta; the architecture doesn't forbid partial component sets at the storage layer (Add/Remove/Has are real, per-component `SparseSet` operations, not a monolithic bundle), but nothing in this migration manufactures a scenario that uses that flexibility just to show it off.
- Defensive handling, anywhere in `Renderer`/`EditorUI`/`Gizmo`/`Engine`, for an entity missing a component no code path ever produces. `EntitySnapshot` holds plain (non-optional) `Transform`/`Sprite`/`Physics` values; the Inspector always shows all three sections; `SaveScene`'s fixed field layout is untouched. If a future change actually needs partial entities, that change updates these consumers then — this migration doesn't pay for it now. (`Engine`'s physics loop already filters by `Has<Physics>` today in spirit — `!other.isStatic` — so that one check stays, but as the natural shape of a physics loop, not as new defensive-programming scope.)

## Anti-Goals

(What would make this a failure even if it technically works)

- A "components" layer that's just `Entity`'s same four field-groups behind
  getters, with the *storage* (`Registry`/`SparseSet`) itself still wired as
  if every entity always has every component — e.g. an API that can't add a
  `Transform` without also forcing a `Sprite`. That's a renaming exercise,
  not an ECS, and wouldn't read as one to a reviewer who knows the pattern.
  (This is about the storage layer's own API being honestly general — see
  the Non-Goals note on heterogeneity for what this does *not* require of
  consumers.)
- Breaking undo/redo's exact current granularity (one undo entry per
  discrete action: spawn, delete, or one completed drag/edit gesture — never
  one entry per intermediate frame of a drag) or its current, inconsistent
  50-cap scope (see Constraints — only trim where today's code trims).
- Silently changing render or pick stacking order as a side effect of how
  component storage happens to iterate (see the Goals note above — this is
  avoidable here, unlike Faz 2b's texture-grouping case, so it would be a
  regression, not an accepted tradeoff).

## Constraints

- No automated test framework (standing project convention). Verification
  is build success, scratchpad proofs for any non-obvious data-structure
  logic (sparse-set add/remove/lookup correctness, matching the
  `superpowers:test-driven-development` RED→GREEN pattern used for Faz 2b's
  corner-transform math), and live manual GUI comparison (same scene, same
  actions, before/after).
- Working directly on `main`, no worktree, push by default — same standing
  convention as every prior phase.
- Unlike Faz 2b (which explicitly preserved `Renderer::Render`'s and
  `EditorUI::Draw`'s public signatures), **this migration changes both
  signatures** — `std::vector<Entity>&`/`int selected`/`std::vector<std::vector<Entity>>& undoStack`
  become `Registry&`/`EntityID selected` (or an equivalent "nothing
  selected" sentinel)/an undo stack of Registry snapshots. This is expected
  and necessary; every one of `Entity`'s current consumers (`Engine.cpp`,
  `EditorUI.cpp`, `Gizmo.cpp`, `Renderer.cpp`) is touched by this plan.
- `Gizmo::Draw`/`Gizmo::Update` and `EditorUI::CheckPointInside` only ever
  read/write position+rotation+scale — i.e. only the `Transform` component.
  They must not need `Sprite` or `Physics` at all once migrated (confirms
  the component boundary below is the right cut).
- Entity deletion (`DELETE ENTITY` button) is the one place order-preserving
  removal has a real (if rare) cost: it's an O(n) shift today
  (`std::vector::erase`) and must stay O(n)-shift (not O(1) swap-and-pop) in
  the new sparse-set storage, trading the sparse-set pattern's usual
  constant-time removal for exact behavioral parity. Acceptable: deletion is
  a manual, one-click action, never a hot path, and this project's entity
  counts (tested to 5000 in Faz 2b) make an O(n) shift irrelevant at the
  scales this engine runs at.
- The undo snapshot's current behavior — one entry pushed per spawn
  (`EditorUI.cpp:67`), per delete (`EditorUI.cpp:424`), and per completed
  edit gesture (drag start→end or any ImGui widget's active→inactive
  transition, `EditorUI.cpp:338-339`) — must be preserved exactly,
  including its current *inconsistency*: only the edit-gesture push trims
  the stack to 50 entries; the spawn and delete pushes are uncapped today,
  and stay uncapped. This migration fixes none of that — it only changes
  what a pushed entry's *contents* are (a `vector<EntitySnapshot>` instead
  of a `vector<Entity>`), never when entries are pushed or trimmed. A
  snapshot becomes "all
  currently-live entities' full component data, as plain value structs,"
  gathered by querying the Registry at snapshot time; restoring means
  clearing the Registry and recreating every entity from the snapshot's
  values. This keeps undo's UX identical while letting the Registry's
  internal storage be whatever it needs to be.
- Debug > "Spawn 1000 Test Entities" still does not push an undo snapshot
  (standing ruling from Faz 2b, reconfirmed by the user after experiencing
  it live — unchanged by this migration).

## Approaches Considered

1. **Sparse-set component storage** (chosen) — each component type gets its
   own packed array plus a sparse `EntityID → packed index` lookup array.
   Iteration over "all entities with component X" is a direct scan of X's
   packed array; iteration over "all entities with X and Y" intersects the
   smaller packed array against the other's sparse lookup. Cache-friendly,
   the pattern real ECS libraries (EnTT, etc.) use, and the most
   recognizable "real ECS" shape to this project's intended audience.
2. **Bitmask / fixed-slot arrays** ("Artemis"-style) — every component type
   gets an array sized to the max entity count, with a per-entity bitmask
   marking which slots are populated. Simpler to implement correctly (no
   packing/compaction logic at all), but iteration walks through unused
   slots for any entity lacking a given component, and it reads as a
   slightly more dated/less "real" pattern than sparse-set to a reviewer
   who knows the space.
3. **Per-entity component maps** — `std::unordered_map<std::type_index, std::unique_ptr<void>>`
   per entity. Most flexible (zero static wiring for new component types),
   worst cache-locality of the three, and closer to a generic "component
   bag" than to what most people mean by "ECS."

Decided in conversation: sparse-set (1), explicitly chosen over adopting an
existing library (EnTT) because this project has hand-built everything so
far (including its math, with GLM deferred) and a hand-built ECS is a
stronger architecture-credibility story for its stated purpose than wiring
in a well-known third-party one.

## Decided Design

**1. Entity identity.** `using EntityID = uint32_t;` — a plain, reusable
integer, not a generational handle. A generational handle would guard
against a class of dangling-reference bug this codebase's simple,
single-selection model doesn't expose — not worth the added complexity here.

**1a. Selection, corrected from an inaccurate first draft.** Today,
`selected` is a plain `int` *array position*, re-interpreted fresh against
whatever `entities` currently is after every mutation — it is not an
"entity identity" at all. Concretely, today: startup selects index 0
(Player, since it's always created first); the DELETE button sets
`selected = 0`, which actually *selects whatever entity now happens to sit
first* in the vector (not "nothing" — an earlier draft of this doc
mischaracterized this); Ctrl+Z/Edit > Undo/`LoadScene` never touch
`selected` at all, so after any of them it still holds its old numeric
value, now silently reinterpreted against a different vector (sometimes
landing on an unrelated entity, sometimes out-of-range and therefore
harmless only because every consumer already bounds-checks it).
None of that position-reinterpretation trick survives a move to
`EntityID`s that get reassigned on every restore — and it was never a
deliberate feature, just what falls out of using a raw index. Decided
replacement, a deliberate small improvement rather than bit-exact parity:
`EntityID selectedEntity`, with a sentinel (see "What's Left to the
Builder") for "nothing selected." Startup selects the initial Player's ID
(same visible outcome as today). Delete, undo (either path), and
`LoadScene` all set selection to the sentinel — a clean, easily-explained
rule ("a structural change to the entity list clears selection") in place
of today's accidental index reinterpretation. This is a user-visible
behavior change in an edge case nobody is likely to have relied on; flagged
to the human partner for visibility, not blocking on it.

**2. Components** (plain data structs, no methods):
```cpp
struct Transform { float x, y, rotation, sx, sy; };
struct Sprite     { float color[3]; std::string textureName; };
struct Physics    { bool hasGravity, isStatic; float vy; };
```
`name` (`std::string`) and `active` (`bool`) are not components — every
entity this engine creates has exactly one of each, nothing ever adds or
removes them independently of the entity's own lifetime, and nothing in the
current code ever toggles `active` at all (it's always `true` — there's no
UI for it). They live as plain per-entity fields inside the `Registry`
itself, parallel-indexed by `EntityID` the same way the components are,
rather than as a fourth sparse-set component type.

**3. `SparseSet<T>`** (one instantiation per component type):
```cpp
template <typename T>
class SparseSet {
    std::vector<int> sparse;        // EntityID -> index into dense, or -1
    std::vector<EntityID> denseIds; // dense[i] belongs to denseIds[i]
    std::vector<T> dense;           // packed component data
public:
    void Add(EntityID e, T value);
    void Remove(EntityID e);   // order-preserving: erase+shift, not swap-and-pop
    bool Has(EntityID e) const;
    T& Get(EntityID e);
    const std::vector<EntityID>& Ids() const { return denseIds; }
    std::vector<T>& Dense() { return dense; }
};
```

**4. `Registry`** owns one `SparseSet` per component type, the live
`EntityID` list (insertion-ordered — this is what makes Hierarchy listing,
click-picking's "last match wins," and render order all stay consistent
with today), and the parallel `name`/`active` arrays:
```cpp
class Registry {
public:
    EntityID Create(const std::string& name);
    void Destroy(EntityID e);           // removes from every component set + live list
    const std::vector<EntityID>& Entities() const; // insertion order, for Hierarchy/picking/render

    std::string& Name(EntityID e);
    bool& Active(EntityID e);

    SparseSet<Transform> transforms;
    SparseSet<Sprite> sprites;
    SparseSet<Physics> physics;
};
```
No generic templated `View<A, B>()` is required by anything this migration
actually does — every system that needs multiple components (Renderer's
batch-building needs Transform+Sprite; Engine's physics loop needs
Transform+Physics) can iterate `Entities()` once and `Has()`/`Get()` per
component directly, which is simpler than building a real multi-component
view iterator and costs nothing in practice at this engine's scale. Building
a generic view mechanism nobody needs yet is exactly the kind of
over-engineering Non-Goals rules out.

**5. Undo.** A snapshot is `std::vector<EntitySnapshot>` where
`EntitySnapshot` is a plain value struct holding one entity's `name`,
`active`, and copies of its `Transform`/`Sprite`/`Physics` (today, always
all three — see Non-Goals on heterogeneity). `undoStack` becomes
`std::vector<std::vector<EntitySnapshot>>`. Taking a snapshot walks
`Registry::Entities()` once, reading each entity's components into a
`EntitySnapshot`. Restoring clears the `Registry` and recreates every
entity from its snapshot in order. This is a direct, mechanical translation
of what `entities = undoStack.back()` already does today.

**6. Consumer migration.** This is a storage swap, not a file-responsibility
reshuffle — corrected from an earlier draft that incorrectly proposed
moving spawn/undo logic into `Engine.cpp`. Every one of today's call sites
keeps living exactly where it lives today; each just talks to a `Registry&`
and `EntityID`s instead of a `std::vector<Entity>&` and `int`s:
- `Engine.cpp`: owns the `Registry` (replacing `std::vector<Entity> entities`),
  creates the initial Player, runs the physics/collision loop (iterate
  `Entities()`, check `Has<Physics>`), calls `Registry`-based restore on
  Ctrl+Z, and drives `SaveScene`/`LoadScene`.
- `EditorUI.cpp`: keeps owning `SpawnNewEntity`, the Debug stress-spawn menu
  item, the DELETE button's pre-delete snapshot, the drag/edit-gesture
  capture (`preEditState`/`wasActive`), and its own inline Edit > Undo
  restore — all unchanged in *where* they live, just rewritten against
  `Registry`/`EntityID`.
- `Renderer.cpp`: `Render()`'s signature takes a `Registry&` instead of
  `std::vector<Entity>&`. Faz 2b's texture-grouped batch-building loop
  changes its source of truth from iterating a `vector<Entity>` to iterating
  `registry.Entities()` and reading `Transform`+`Sprite` per entity —
  the grouping-by-resolved-texture-ID logic itself is unchanged.
- `EditorUI.cpp`: `Draw()`'s signature takes a `Registry&` and an
  `EntityID selected` (with a sentinel, e.g. `UINT32_MAX`, for "nothing
  selected," replacing `selected == -1`/out-of-range-int checks).
  Hierarchy iterates `Entities()`; Inspector reads/writes the selected
  entity's components directly; `CheckPointInside` takes a `Transform`
  (not an `Entity`).
- `Gizmo.cpp`: `Draw()`/`Update()` take a `Transform&` instead of an
  `Entity&` — confirmed by this doc's Constraints section that Gizmo never
  touches Sprite or Physics.

## What's Left to the Builder

- Exact `SparseSet<T>::Remove`'s shift implementation (a straightforward
  `std::vector::erase`-equivalent across the three parallel arrays) — no
  design judgment left here, just correct bookkeeping of `sparse[]` indices
  for every entity after the removed one.
- A scratchpad RED→GREEN proof for `SparseSet<T>` itself (add/remove/lookup
  sequences, confirming order-preservation and correct `sparse[]` upkeep
  after removal) before it's trusted by every other task — same role Faz
  2b's corner-transform proof played.
- Exact task breakdown and sequencing is the next step
  (`superpowers:writing-plans`), not this doc. A reasonable shape — core
  primitives first (SparseSet + Registry + components, no behavior change
  yet since nothing uses them), then `Engine.cpp` (creation/physics/undo),
  then `Renderer.cpp`, then `EditorUI.cpp` + `Gizmo.cpp` together (they
  share the `Transform`-only contract) — is likely more tasks than Faz 2b's
  four, given every consumer of `Entity` is touched; the plan author should
  size tasks so each still ends in something buildable and independently
  reviewable, per `superpowers:writing-plans`' Task Right-Sizing.
- Whether `EntityID`'s "nothing selected" sentinel is `UINT32_MAX`, `0`
  treated specially, or an `std::optional<EntityID>` — any of the three
  works; pick whichever reads cleanest against the final `Registry::Create`
  numbering scheme once it's written.
