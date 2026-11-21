# Rendering Batching (Faz 2b) — Design

## Intent & Why

The roadmap's Faz 2b. Today `Renderer::Render` issues one OpenGL draw call per entity and one per particle, each preceded by its own `glUniformMatrix4fv`/`glUniform3f`/`glUniform1f` calls (with `glGetUniformLocation` re-looked-up every single entity, every single frame) and its own `glBindTexture`. This is the standard "unbatched" 2D renderer shape, and it's the next credibility/architecture milestone on the user's roadmap after the Renderer/UI split (Faz 2a) and the editor UI/UX work (gizmos, docking, theme) — all already shipped. It also feeds the same ongoing Instagram "before/after AI assistance" content series: a provable draw-call reduction with an on-screen FPS counter and a stress-test of many sprites is good, legible demo material.

The user deferred the one open question from brainstorming (what sprite-count scale to target) twice with "devam et" — explicitly handing the call to the builder. Decision made below, with reasoning, as the user's stated preference throughout this project ("just guess"/deferring = propose something concrete and move).

## Goals

- Reduce per-frame draw calls for entities+particles from O(entity count + particle count) to O(distinct textures in use that frame) — typically 1-3 draw calls total instead of dozens/hundreds/thousands.
- Fix the already-present uniform-location lookup happening every entity every frame (cache locations once, at shader link time) — a free correctness/performance fix riding along with the same code.
- Add a visible on-screen FPS / frame-time readout, so the improvement is provable live and on camera, not just asserted.
- Add a lightweight stress-test control (spawn a large number of test entities on demand) so the before/after comparison has something to show at a scale where the difference is visually and numerically obvious.
- Preserve exact current visual output — same sprite positions, rotations, scales, colors, alpha blending, draw order semantics. Batching is a backend change, not a visual one.

## Non-Goals

- No ECS — that's Faz 2c, a separate, later roadmap item. This task still operates on `std::vector<Entity>`/`std::vector<Particle>` exactly as today.
- No texture atlas or texture array. Sprites using different textures still fall into different draw-call groups (one draw call per distinct texture in use that frame, not truly one call regardless of texture). Atlasing is a bigger, separate lift and not required to hit this task's target scale.
- No GPU instancing. The CPU-side dynamic-vertex-buffer approach (below) is sufficient for the target scale and is the simpler, more standard technique to implement and to demo/explain. Instancing is a plausible future upgrade on top of this, not bundled here.
- Does not touch the ImGui/editor rendering path (docking, gizmos, theme) — that's a separate pipeline (`imgui_impl_opengl3`) with its own batching already.
- Does not change the infinite grid's rendering (already a single full-screen quad draw call; nothing to batch).

## Anti-Goals

- Batching must not visibly change blending/draw order in a way that looks different from today — particles are alpha-blended, and grouping by texture must not introduce visible popping, z-fighting, or incorrect over/under-draw versus today's strict per-entity-then-per-particle draw order. (Within a texture group, draw order among same-texture sprites is not visually meaningful today since there's no inter-sprite depth testing — only the entities-before-particles ordering matters, and that's preserved by keeping entities and particles as separate batch passes.)
- Don't regress the "no automated test framework" project convention by secretly requiring one — verification stays manual (live FPS-counter comparison + visual GUI check), consistent with every prior phase.

## Constraints

- Current vertex format (`VBO` in `Renderer::InitBuffers`) is position+UV only; per-entity tint and alpha are passed as shader *uniforms* (`tint`, `alpha`), not per-vertex attributes. Uniforms are set once per draw call for the *whole* draw — to batch multiple differently-colored/differently-transparent entities into a single draw call, tint and alpha (and the transform, since each sprite has its own position/rotation/scale) must move from uniforms to **per-vertex data**, computed on the CPU per sprite per frame, not per-draw.
- The existing shader (`vSrc`/`fSrc` in `Renderer.cpp`) takes a `transform` uniform matrix applied in the vertex shader. Batching requires transforms to be pre-applied to vertex positions on the CPU (since a batch's one draw call can't carry N different transform uniforms), so the vertex shader simplifies to just applying `view` to already-world-space positions.
- No automated test framework (standing project convention) — verification is the new FPS counter plus live manual GUI comparison (same sprite scene before/after, same stress-test entity count, compare draw-call count conceptually and FPS numerically), same spirit as every prior phase.
- Working directly on `main`, no worktree, push by default — same standing convention as every prior phase.
- **The infinite grid currently reuses the same `VAO`/`VBO`/`EBO` as entity/particle sprites** (`Renderer::Render`'s grid-drawing block does `glBindVertexArray(VAO); // Re-use standard quad`, binding the exact same static unit-quad buffer `InitBuffers` sets up). This buffer is exactly the one this design turns into a per-frame, per-texture-group *dynamic* buffer with a different vertex layout (position+uv+color+alpha instead of position+uv) — so the grid can no longer share it without breaking (wrong vertex format, overwritten every frame by whichever sprite batch uploaded last). This was missed in the first draft of this doc despite the explicit non-goal "does not change the grid's rendering" — caught by builder-check. Resolved below as the builder's call, not a user decision: the fix is small and the design's own non-goal already states the intent (grid must keep rendering exactly as today).

## Approaches (considered)

1. **CPU-side dynamic vertex-buffer batching, grouped by texture (chosen).** Each frame, for each distinct texture in use, the CPU builds one growable vertex buffer containing every sprite using that texture (4 vertices each: position pre-transformed by that sprite's world transform, UV, and per-vertex color+alpha), uploads it with one `glBufferSubData`/`glBufferData` call, and issues one `glDrawElements` call covering all of that group's quads (using a shared, pre-generated index buffer sized to the largest batch seen so far, 6 indices per quad, grown as needed). Draw calls per frame become roughly "number of distinct textures in use," typically 1 (the default white texture) to a handful. Standard, well-understood 2D batching technique; doesn't require touching the texture/asset system at all; scales comfortably to many thousands of sprites on any texture-call-bound hardware, since the bottleneck this removes is CPU-GPU driver overhead per draw call, not vertex throughput.
2. **GPU instancing (one `glDrawElementsInstanced` call per texture, per-instance attributes for transform/color/alpha).** Removes the CPU-side vertex-building cost (no per-sprite CPU vertex-position math), which matters at much larger scales (tens-hundreds of thousands of sprites) where CPU-side batch construction itself becomes the bottleneck. Still one draw call per distinct texture, same as approach 1, unless combined with atlasing. More shader/vertex-attribute-divisor machinery to set up correctly for a modest gain at this project's realistic scale. Rejected for this pass — a reasonable future upgrade once/if the engine needs tens of thousands of sprites, which isn't the stated goal.
3. **Texture atlas + single draw call regardless of texture.** The "ultimate" version — genuinely one draw call for the whole scene no matter how many different source images are in play. Requires a runtime or offline atlas packer, UV remapping for every sprite, and handling atlas overflow/multiple pages. Much bigger scope for a win this project doesn't currently need (texture count per scene is small; the draw-call-per-texture-group count from approach 1 is already effectively "a few, not hundreds"). Rejected for this pass; could layer on top of either approach 1 or 2 later if ever needed.

## Decided scope: target scale

The user deferred this choice to the builder twice ("devam et"). Decision: **target hundreds to a few thousand sprites** (entities + particles combined) as the scale the stress-test and FPS counter should comfortably demonstrate — e.g., a stress-test control that spawns 1,000-2,000 test entities on demand, showing FPS before batching (one draw call per sprite) crumbling well below 60fps at that count, and FPS after batching staying smooth. This is the natural "this clearly proves the point, on camera, without needing an atlas or instancing" scale for a 2D portfolio engine, matches approach 1's sweet spot exactly, and keeps this task's scope matched to approach 1 (no instancing, no atlas needed to hit it). If the user later wants a "tens of thousands of sprites" flex for content purposes, that's approach 2 layered on top — a separate, later task, not blocked by this one.

## Decided Design

### 1. Vertex format change

Replace the current position+UV-only vertex layout with a batching-friendly per-vertex layout: `position (vec3) + uv (vec2) + color (vec3) + alpha (float)`. The shared unit quad (`-0.5..0.5`) stops being a single static buffer reused via a `transform` uniform; instead, for every sprite drawn this frame, the CPU computes that sprite's 4 world-space corner positions (via the existing `CreateTransform` math, applied on the CPU to the 4 local corners) and writes them, plus that sprite's UV, color, and alpha, into a per-texture-group CPU-side vertex array.

### 1b. Grid gets its own dedicated static buffer

The infinite grid must keep its own small, static unit-quad `VAO`/`VBO`/`EBO` — built once (in `InitBuffers` or a new `InitGridBuffers`, builder's call), using the grid's existing position-only vertex layout and `gridProgram`/`gridVSrc`/`gridFSrc` exactly as today, completely independent of the new dynamic per-texture-group sprite-batch buffers. Today's code accidentally reuses the entity/particle `VAO`/`VBO`/`EBO` for the grid (`// Re-use standard quad`); once that buffer becomes a per-frame dynamic batch buffer with a different vertex layout, the grid can no longer share it without breaking. This keeps the grid rendering byte-for-byte as it does today, honoring this doc's own non-goal, and is a pure bugfix-before-it-happens, not a design change — caught by builder-check, settled here as the builder's call rather than a user decision since the intended behavior (grid unchanged) was already stated.

### 2. Shader simplification

`vSrc`/`fSrc` update: the vertex shader drops the `transform` uniform (positions arriving are already world-space) and gains a per-vertex `color`+`alpha` input, passed through to the fragment shader and multiplied with the sampled texture color exactly as the current uniform-based `tint`/`alpha` are today — pixel output must be identical to today's for the same sprite. `view` uniform stays exactly as-is (still applied once per draw call in the vertex shader, same as today).

### 3. Batch building and draw, in `Renderer::Render`

Replace the current per-entity and per-particle draw loops with: group entities (and then particles) by `textures[e.textureName]` (resolved texture id, same resolution logic as today), build each group's vertex+index data into a reusable, growable CPU-side buffer (a `std::vector<float>` or small vertex struct vector, sized/reserved to avoid per-frame reallocation churn where easy), upload via one dynamic VBO (`GL_DYNAMIC_DRAW`, `glBufferData` or `glBufferSubData` sized to the current frame's vertex count) per group, and issue one `glDrawElements` per group using a shared, pre-sized index buffer (grown, never shrunk, whenever a group's quad count exceeds the current index buffer's capacity — standard "grow-only" pattern). Entities are batched as one pass, particles as a second pass immediately after (preserves today's "entities fully drawn, then particles drawn on top" ordering).

### 4. Uniform location caching

Cache `glGetUniformLocation` results for `view` (and any remaining uniforms) once, right after each shader program links in `InitShader`/`InitInfiniteGrid`, as member fields, instead of calling `glGetUniformLocation` inside the per-frame draw loop. This is a strict subset of the same code already being rewritten for batching, so it rides along for free.

### 5. FPS / frame-time counter

A small always-visible ImGui text overlay (builder's call on exact placement — e.g., a corner of the Scene viewport, in the same style/layer as the existing "Tips" overlay text) showing current FPS (and/or frame time in ms), computed from `SDL_GetTicks()`/a simple rolling average, updated every frame. This is the thing that makes "batching helped" provable on screen and on camera, not just asserted in a commit message.

### 6. Stress-test spawn control

A menu item (File or a new "Debug" menu — builder's call, consistent with the existing menu bar's conventions) that spawns a fixed, round-number batch of additional test entities (e.g. 1,000) at randomized positions with the default texture/a random color, so the user can repeatedly trigger "add 1,000 more sprites" live and watch the FPS counter respond. Exact N and UI placement are the builder's call; the important property is that it's trivially repeatable live, for both engine-verification and Instagram-demo purposes.

## What's Left to the Builder

- Exact vertex struct layout and attribute pointer offsets/strides for the new position+uv+color+alpha format.
- Exact index-buffer growth strategy (initial capacity, growth factor) — any reasonable grow-only scheme is fine.
- Exact FPS counter placement, update/smoothing method (instant vs. rolling average), and text styling — consistent with the existing overlay conventions in `EditorUI.cpp`.
- Exact stress-test menu placement, spawned-entity count (suggested 1,000, but builder may pick a cleaner demo number), and whether repeated triggers stack (recommended: yes, stack, so the user can escalate live: 1,000 → 2,000 → 3,000... while watching FPS drop, which is itself useful demo content) or reset-and-respawn.
- Whether particles and entities that happen to share the same resolved texture get merged into one combined batch group or always issued as two passes (entities-pass, then particles-pass) even when textures match — either is fine as long as draw order (entities fully before particles) is preserved; builder's call based on whichever is simpler to implement correctly.
