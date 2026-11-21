#include "EditorUI.h"
#include "imgui.h"
#include "imgui_internal.h" // DockBuilder* API: not yet promoted to the public imgui.h surface on this docking-branch version.
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl3.h"
#include <filesystem>
#include <cstring>

// Single named accent color for the whole editor theme: a deliberate, unambiguous red (not
// orange, not a desaturated purple/magenta) used for every "this is active/on/selected"
// highlight -- checkboxes, the Gizmo toolbar's active-mode button, slider grabs, and dock tabs.
// kAccentColorDark is the same hue at reduced lightness/saturation, used for the handful of
// spots (SliderGrabActive, tab-active states) that previously used a correspondingly darker
// shade of the old orange -- same light/dark relationship, just re-hued to red.
static const ImVec4 kAccentColor     = ImVec4(0.82f, 0.18f, 0.16f, 1.00f);
static const ImVec4 kAccentColorDark = ImVec4(0.74f, 0.16f, 0.14f, 1.00f);

// GizmoToolbar-only "floating glass chip" treatment. Composition call (confirmed live with the
// user against two alternative mockups): Hierarchy/Inspector/Scene should read as flush,
// edge-to-edge sidebars/world -- ONE cohesive window, not a stack of separate rounded cards -- so
// the heavier rounding + translucency + sheen from an earlier pass was pulled back off of them and
// concentrated here instead, on the one element that is genuinely a floating overlay drawn on top
// of the scene rather than a docked panel. Scoped to GizmoToolbar via PushStyleVar/PushStyleColor
// around its Begin/End rather than folding into the global style, so it doesn't leak onto
// Hierarchy/Inspector/Scene.
static const float kToolbarWindowRounding = 10.0f;
static const float kToolbarFrameRounding  = 8.0f;
static const ImVec4 kToolbarGlassBg       = ImVec4(0.26f, 0.26f, 0.27f, 0.55f);

// Builds the default dockspace layout (Hierarchy left ~20%, Inspector right ~25%, central node for
// "Scene") from scratch. Assumes dockspaceId has no existing node (or that the caller just removed
// one), same precondition the original first-run-only block had. Factored out so it can be called
// both on first run (no saved layout yet) and from View > Reset Layout (rebuild on demand, same
// frame) without duplicating the DockBuilder* sequence.
static ImGuiID BuildDefaultDockLayout(ImGuiID dockspaceId, int winW, int winH) {
    ImGui::DockBuilderAddNode(dockspaceId, (ImGuiDockNodeFlags)((int)ImGuiDockNodeFlags_DockSpace | (int)ImGuiDockNodeFlags_PassthruCentralNode));
    ImGui::DockBuilderSetNodeSize(dockspaceId, ImVec2((float)winW, (float)winH));

    ImGuiID centerId = dockspaceId;
    ImGuiID leftId = ImGui::DockBuilderSplitNode(centerId, ImGuiDir_Left, 0.20f, nullptr, &centerId);
    // centerId is now the remaining 80%; split off 0.3125 of THAT so the right panel is
    // 25% of the *total* width (0.3125 * 0.80 == 0.25), leaving a true 55% central node.
    ImGuiID rightId = ImGui::DockBuilderSplitNode(centerId, ImGuiDir_Right, 0.3125f, nullptr, &centerId);

    ImGui::DockBuilderDockWindow("Hierarchy", leftId);
    ImGui::DockBuilderDockWindow("Inspector", rightId);
    // Explicitly (re-)dock "Scene" into the central node too, not just Hierarchy/Inspector. On a true
    // first-ever run this is redundant with (and harmless alongside) the FindWindowSettingsByID-gated
    // fallback below Draw()'s DockSpaceOverViewport() call, which only fires when Scene has never been
    // seen before. But View > Reset Layout calls this function again later in an already-running
    // session, after Scene already has saved window settings -- without this line, removing the dock
    // node tree (DockBuilderRemoveNode) would leave Scene's dock assignment severed and it would NOT
    // get re-homed into the newly rebuilt central node (that fallback's gate would skip it, since
    // Scene's settings already exist), defeating "Reset Layout" for exactly the window that matters
    // most. Docking it here makes the reset unconditional, matching the brief's "regardless of
    // whatever's currently saved."
    ImGui::DockBuilderDockWindow("Scene", centerId);
    ImGui::DockBuilderFinish(dockspaceId);
    return centerId;
}

// Shared by the Hierarchy panel's "+ New Entity" button and the File menu's "+ New Entity" item,
// so the two can't drift apart over time.
static void SpawnNewEntity(std::vector<Entity>& entities, const Camera& cam,
                            std::vector<std::vector<Entity>>& undoStack) {
    undoStack.push_back(entities); // Save before adding
    entities.push_back({"Prop", cam.x, cam.y, 0, 0.3f, 0.3f, {1,1,1}, "default", true, false, 0});
}

// Draws the theme's one deliberately-allowed gradient: a faint light sheen across the top edge of
// a floating glass surface, simulating light catching the top of real glass. Fades from a
// low-alpha white at the top edge to fully transparent by heightFraction down. Must be called
// right after Begin() (and before any other widget calls) so it lands behind the window's own
// content in its draw list, not on top of it.
//
// Composition note: this used to also run on the docked Hierarchy/Inspector sidebars, but that
// made each one read as its own separate floating rounded card instead of a flush, cohesive part
// of the window (the exact "no figure-ground unity" problem live user feedback flagged) -- flush
// sidebars in an Xcode/Finder-style composition don't get a sheen. It now runs on GizmoToolbar
// only, which genuinely is a small floating glass chip drawn on top of the scene, not a docked
// panel, so the effect is both visually and structurally honest there. Kept as a helper (even
// though it's down to one call site for now) in case a future floating/overlay panel wants the
// same effect without re-deriving the corner-inset math below.
//
// The quad is inset by the window's own rounding on the left/right (and half that on top) so its
// flat corners stay within the window's straight edge instead of visibly squaring off past the
// already-rounded corner the window background itself draws.
static void DrawGlassSheen(float heightFraction = 0.28f, float topAlpha = 0.10f) {
    ImGuiStyle& style = ImGui::GetStyle();
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 winPos = ImGui::GetWindowPos();
    ImVec2 winSize = ImGui::GetWindowSize();
    float inset = style.WindowRounding;

    ImVec2 topLeft(winPos.x + inset, winPos.y + inset * 0.5f);
    ImVec2 bottomRight(winPos.x + winSize.x - inset, winPos.y + winSize.y * heightFraction);
    ImU32 sheenColor = ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, topAlpha));
    ImU32 fadeColor  = ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, 0.0f));
    drawList->AddRectFilledMultiColor(topLeft, bottomRight, sheenColor, sheenColor, fadeColor, fadeColor);
}

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
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    if (std::filesystem::exists("C:/Windows/Fonts/segoeui.ttf")) {
        io.Fonts->AddFontFromFileTTF("C:/Windows/Fonts/segoeui.ttf", 20.0f);
    }
    SetupUIStyle();
    ImGui_ImplSDL2_InitForOpenGL(window, ctx);
    ImGui_ImplOpenGL3_Init("#version 330");
}

bool EditorUI::CheckPointInside(Entity &e, float px, float py) {
    float halfX = e.sx / 2.0f;
    float halfY = e.sy / 2.0f;
    return (px > e.x - halfX && px < e.x + halfX && py > e.y - halfY && py < e.y + halfY);
}

ViewportRect EditorUI::Draw(SDL_Window* window, std::vector<Entity>& entities, const Camera& cam,
                             int& selected, std::vector<std::vector<Entity>>& undoStack,
                             bool& saveRequested, const std::vector<std::string>& textureList) {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();

    // Frame timing: wall-clock delta since the previous Draw() call, lightly smoothed so the
    // FPS readout stays legible. dt <= 0 (first frame, or a zero-length frame) is skipped.
    Uint32 nowTicks = SDL_GetTicks();
    float dt = lastFrameTicks ? (nowTicks - lastFrameTicks) / 1000.0f : 0.0f;
    lastFrameTicks = nowTicks;
    if (dt > 0.0f) {
        float instantFps = 1.0f / dt;
        fpsSmoothed = (fpsSmoothed > 0.0f) ? (fpsSmoothed * 0.9f + instantFps * 0.1f) : instantFps;
    }

    int winW, winH;
    SDL_GetWindowSize(window, &winW, &winH);

    // --- DockSpace host + default first-run layout ---
    // Stable ID computed independent of any window's ID stack (ImGui::GetID() needs a
    // current window, which doesn't exist yet this early in the frame) so it's identical
    // every frame and can be used both to query DockBuilder state and to feed DockSpaceOverViewport().
    ImGuiID dockspaceId = ImHashStr("WaryEditorDockSpace");

    ImGuiID centerId = 0;
    if (ImGui::DockBuilderGetNode(dockspaceId) == nullptr) {
        // First run (no saved layout for this dockspace yet): lay out Hierarchy left (~20%),
        // Inspector right (~25%), and dock "Scene" into the remaining central node (the scene).
        ImGui::DockBuilderRemoveNode(dockspaceId);
        centerId = BuildDefaultDockLayout(dockspaceId, winW, winH);
    }

    // --- Main menu bar ---
    // MUST run (Begin/EndMainMenuBar) before DockSpaceOverViewport() below, in this same frame.
    // DockSpaceOverViewport(dockspaceId, nullptr, ...) reads the main viewport's WorkPos/WorkSize to
    // size the dockspace; that work rect is only shrunk to account for the main menu bar's reserved
    // height once BeginMainMenuBar()/EndMainMenuBar() has run for the frame. Call it after that point
    // instead and the dockspace (and everything docked into it) will overlap the menu bar for a frame
    // of lag (or permanently, depending on how ImGui caches it) -- this is the same class of
    // one-frame-stale-geometry bug documented above for DockBuilderGetCentralNode(), so don't move
    // this block below DockSpaceOverViewport() to "simplify" the flow.
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("+ New Entity")) {
                SpawnNewEntity(entities, cam, undoStack);
            }
            if (ImGui::MenuItem("Save")) {
                saveRequested = true;
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Edit")) {
            if (ImGui::MenuItem("Undo", nullptr, false, !undoStack.empty())) {
                // Replicates Engine::Undo()'s exact 3-line logic inline: Draw() already has
                // entities/undoStack by reference, and Engine::Undo() isn't reachable from here
                // (separate class) -- exposing it across that boundary would be more invasive than
                // this small duplication is worth.
                if (!undoStack.empty()) {
                    entities = undoStack.back();
                    undoStack.pop_back();
                }
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("View")) {
            if (ImGui::MenuItem("Reset Layout")) {
                // Rebuild the default split immediately, in this running session -- not just on next
                // launch. DockBuilderRemoveNode() first because BuildDefaultDockLayout() assumes a
                // fresh/removed node going in, same precondition the first-run path above relies on.
                ImGui::DockBuilderRemoveNode(dockspaceId);
                centerId = BuildDefaultDockLayout(dockspaceId, winW, winH);
            }
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }

    // WindowBg is pushed fully transparent for the DockSpaceOverViewport() call ONLY. Reason: with
    // ImGuiDockNodeFlags_PassthruCentralNode, the dockspace root paints a WindowBg-colored fill over the
    // whole dockspace and only cuts a transparent hole for the central node while that node is EMPTY
    // (imgui.cpp DockNodeUpdate: central_node_hole requires central_node->IsEmpty(), else it does
    // AddRectFilled over the entire root rect). Since "Scene" is now a real window docked into the central
    // node, it is never empty, so that fill covered the OpenGL scene (grid/entities/particles, drawn by
    // Renderer::Render before ImGui renders) with flat gray -- confirmed by reading the framebuffer at the
    // scene's center: (186,49,50) right after Renderer::Render vs (31,31,31) == WindowBg after ImGui.
    // That fill is drawn synchronously inside this call, so a scoped push affects only it; Hierarchy/
    // Inspector's own backgrounds are drawn by their own Begin() later with the normal WindowBg.
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
    ImGui::DockSpaceOverViewport(dockspaceId, nullptr, ImGuiDockNodeFlags_PassthruCentralNode);
    ImGui::PopStyleColor();

    // "Scene" is a window added after Hierarchy/Inspector already existed in this task's history, so an
    // existing imgui.ini saved by an older build (dockspace/Hierarchy/Inspector already laid out, no
    // "Scene" entry at all) must still land Scene in the central node on upgrade, not as a stray floating
    // window. Only force its initial dock assignment if it has genuinely never been seen before in any
    // session (no saved settings for it at all) -- otherwise respect wherever the user last dragged it,
    // the same way Hierarchy/Inspector are never re-forced after their first run.
    //
    // This check (and the DockBuilderGetCentralNode() fallback it uses to find the central node's
    // *identity*, as opposed to its Pos/Size, which is what's actually unreliable -- see below) must
    // run AFTER DockSpaceOverViewport() above, not before: when the dockspace tree was loaded from an
    // existing imgui.ini rather than just built fresh this frame via DockBuilder* calls, the root's
    // cached CentralNode pointer isn't populated until DockSpaceOverViewport()'s internal DockSpace()
    // call has run its per-frame update pass at least once. Querying it before that point (confirmed by
    // reproducing this exact upgrade scenario: an existing ini with Hierarchy/Inspector but no Scene
    // entry) returned null, which made the old code fall back to the dockspace ROOT id -- docking
    // "Scene" into a non-leaf split node, which silently failed and left it as a tiny stray floating
    // window instead of filling the central region.
    if (ImGui::FindWindowSettingsByID(ImHashStr("Scene")) == nullptr) {
        if (centerId == 0) {
            ImGuiDockNode* central = ImGui::DockBuilderGetCentralNode(dockspaceId);
            centerId = central ? central->ID : dockspaceId;
        }
        ImGui::DockBuilderDockWindow("Scene", centerId);
    }

    // The scene's ViewportRect comes from a REAL docked ImGui window ("Scene"), not from peeking at
    // DockBuilder's internal node bookkeeping. An earlier approach read the central dock node's
    // Pos/Size directly via DockBuilderGetCentralNode(), but that node never has any window bound to
    // it (it's just an empty passthrough placeholder) -- its Pos/Size are only correct once BOTH of
    // its siblings (Hierarchy/Inspector) have been marked IsVisible for the frame, which only happens
    // after their own Begin() calls run later in this same frame. Confirmed by direct instrumentation
    // (logging every dock node's Pos/Size/IsVisible/Windows.Size across frames, with and without a
    // pre-existing imgui.ini): on frame 0 of *every* launch where Hierarchy/Inspector haven't bound to
    // their nodes yet this frame -- which includes every ordinary relaunch once imgui.ini already
    // exists from a prior session, not just a true first-ever run -- the central node reports the
    // FULL dockspace rect (e.g. (0,0,1280,720) instead of the correct (257,0,701,720)), and nothing
    // after that point corrects it for that frame. That's a deterministic, every-launch bug, not a
    // rare timing fluke.
    //
    // A real ImGui window's geometry does not have this hazard: Begin()/BeginDocked() synchronously
    // resolves the window's final position and size for the current frame no matter what, so reading
    // it immediately after Begin("Scene", ...) is always correct, on every frame including frame 0.
    //
    // NOTE: Scene does NOT have ImGuiWindowFlags_NoMouseInputs (an earlier draft did, and that silently
    // broke scene click-to-select/gizmo-grab -- see sceneHovered below for why and how that's handled).
    // Scene also cannot be undocked (ImGuiDockNodeFlags_NoUndocking via SetNextWindowClass): since
    // ViewportRect is always just "Scene's own window rect" now, letting it float free would shrink the
    // rendered game world into an arbitrary floating window instead of the dockspace's central region --
    // confusing, not merely cosmetic -- so undocking it is disabled outright rather than letting a user
    // get into that state.
    ImGuiWindowClass sceneWindowClass;
    sceneWindowClass.DockNodeFlagsOverrideSet = (ImGuiDockNodeFlags)((int)ImGuiDockNodeFlags_NoUndocking | (int)ImGuiDockNodeFlags_NoDockingOverMe);
    ImGui::SetNextWindowClass(&sceneWindowClass);

    ViewportRect viewport;
    ImGuiWindowFlags sceneFlags = ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar |
                                  ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoCollapse;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f)); // scene should fill edge-to-edge, no default window padding inset
    ImGui::Begin("Scene", nullptr, sceneFlags);
    ImGui::PopStyleVar();
    // Mouse inputs are intentionally left enabled on this window (no NoMouseInputs) so ImGui's own
    // hover/z-order resolution correctly yields the scene when nothing else (Hierarchy/Inspector/
    // GizmoToolbar/Tips) is on top of it at the cursor. Scene being a real hoverable window means
    // io.WantCaptureMouse is true whenever the mouse is anywhere over it too (that's just "some ImGui
    // window is hovered"), so it can no longer be used to gate scene-only clicks -- sceneHovered
    // (true only while the mouse is specifically over *this* window, not Hierarchy/Inspector/the
    // GizmoToolbar overlay drawn on top of it later this same frame) is the precise replacement, used
    // below for both the gizmo click-start gate and the entity-picking click gate.
    bool sceneHovered = ImGui::IsWindowHovered();
    ImVec2 sceneOrigin = ImGui::GetCursorScreenPos();
    ImVec2 sceneSize = ImGui::GetContentRegionAvail();
    viewport = ViewportRect{sceneOrigin.x, sceneOrigin.y, sceneSize.x, sceneSize.y};
    ImGui::End();

    int mx, my;
    SDL_GetMouseState(&mx, &my);

    // --- Gizmo (claims its own clicks before entity-picking runs) ---
    bool gizmoActive = false;
    if (selected >= 0 && selected < (int)entities.size()) {
        gizmo.Draw(ImGui::GetForegroundDrawList(), entities[selected], cam, viewport);
        gizmoActive = gizmo.Update(entities[selected], cam, viewport,
                                    ImGui::IsMouseDown(ImGuiMouseButton_Left),
                                    ImGui::IsMouseClicked(ImGuiMouseButton_Left) && sceneHovered,
                                    ImGui::IsMouseReleased(ImGuiMouseButton_Left),
                                    (float)mx, (float)my);
    }

    // Undo Logic - Capture state before edit
    bool editing = ImGui::IsAnyItemActive() || gizmoActive;
    if (editing) {
        if (!wasActive) {
            preEditState = entities;
            wasActive = true;
        }
    } else {
        if (wasActive) {
            undoStack.push_back(preEditState);
            if (undoStack.size() > 50) undoStack.erase(undoStack.begin());
            wasActive = false;
        }
    }

    // --- Input / Picking ---
    if (!gizmoActive && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && sceneHovered) {
        float localMx = mx - viewport.x;
        float localMy = my - viewport.y;
        if (localMx >= 0 && localMx <= viewport.width && localMy >= 0 && localMy <= viewport.height) {
             float aspect = viewport.width / viewport.height;
             float ndcX = (localMx / viewport.width) * 2.0f - 1.0f;
             float ndcY = 1.0f - (localMy / viewport.height) * 2.0f;
             float wx = ndcX * aspect / cam.zoom + cam.x;
             float wy = ndcY / cam.zoom + cam.y;

             for (int k = (int)entities.size() - 1; k >= 0; k--) {
                if (CheckPointInside(entities[k], wx, wy)) {
                    selected = k;
                    break;
                }
             }
        }
    }

    // --- UI Draw ---
    // Hierarchy
    ImGui::Begin("Hierarchy", nullptr, ImGuiWindowFlags_NoCollapse);
    if (ImGui::Button("Save", ImVec2(-1, 0))) { saveRequested = true; }
    ImGui::TextDisabled("Entities");
    ImGui::Separator();
    for (int i = 0; i < (int)entities.size(); i++) {
        if (ImGui::Selectable((entities[i].name + "##" + std::to_string(i)).c_str(), i == selected))
            selected = i;
    }
    if (ImGui::Button("+ New Entity", ImVec2(-1, 30))) {
        SpawnNewEntity(entities, cam, undoStack);
    }
    ImGui::End();

    // Inspector
    ImGui::Begin("Inspector", nullptr, ImGuiWindowFlags_NoCollapse);
    if (selected >= 0 && selected < (int)entities.size()) {
        Entity &e = entities[selected];
        ImGui::Text("Properties");
        ImGui::Separator();
        char nBuf[64]; strcpy(nBuf, e.name.c_str());
        if (ImGui::InputText("Name", nBuf, 64)) e.name = nBuf;

        if (ImGui::BeginCombo("Texture", e.textureName.c_str())) {
            for (auto &texName : textureList) {
                bool is_selected = (e.textureName == texName);
                if (ImGui::Selectable(texName.c_str(), is_selected)) e.textureName = texName;
                if (is_selected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        ImGui::DragFloat("Pos X", &e.x, 0.01f);
        ImGui::DragFloat("Pos Y", &e.y, 0.01f);
        ImGui::SliderFloat("Rot", &e.rotation, -3.14f, 3.14f);

        ImGui::Separator();
        ImGui::Text("Size");
        static bool uniformScale = false;
        ImGui::Checkbox("Uniform Scale", &uniformScale);

        if (uniformScale) {
            float ratio = e.sx / e.sy;
            if (ImGui::DragFloat("Width (Locked)", &e.sx, 0.01f, 0.01f, 10.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp)) {
                if (ratio > 0.0001f) {
                     e.sy = e.sx / ratio;
                }
            }
            ImGui::TextDisabled("Height is locked to Width (Ratio: %.2f)", ratio);
        } else {
            ImGui::DragFloat("Width (X)", &e.sx, 0.01f, 0.01f, 10.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
            ImGui::DragFloat("Height (Y)", &e.sy, 0.01f, 0.01f, 10.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
        }
        ImGui::Separator();

        ImGui::ColorEdit3("Color", e.color);
        ImGui::Checkbox("Gravity", &e.hasGravity);
        ImGui::Checkbox("Is Static", &e.isStatic);
        ImGui::Dummy(ImVec2(0, 20));
        if (ImGui::Button("DELETE ENTITY", ImVec2(-1, 30))) {
            undoStack.push_back(entities); // Save state before delete
            entities.erase(entities.begin() + selected);
            selected = 0;
        }
    }
    ImGui::End();

    // Gizmo mode toolbar — overlaid at the top of the scene viewport, only while something is
    // selected. The one place in this UI that's genuinely a floating object over the scene (drawn
    // via its own Begin positioned over the viewport, not docked into the layout), so it alone
    // carries the "floating glass chip" treatment: meaningful rounding, a translucent background
    // that actually blends with the rendered scene behind it, and the top-edge sheen. Style pushed
    // around Begin/End only, so none of it leaks onto Hierarchy/Inspector/Scene.
    if (selected >= 0 && selected < (int)entities.size()) {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, kToolbarWindowRounding);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, kToolbarFrameRounding);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, kToolbarGlassBg);
        ImGui::SetNextWindowPos(ImVec2(viewport.x + 10, viewport.y + 10));
        ImGui::Begin("GizmoToolbar", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDocking);
        DrawGlassSheen();

        bool isMove = (gizmo.mode == GizmoMode::Move);
        if (isMove) ImGui::PushStyleColor(ImGuiCol_Button, kAccentColor);
        if (ImGui::Button("Move")) gizmo.mode = GizmoMode::Move;
        if (isMove) ImGui::PopStyleColor();

        ImGui::SameLine();

        bool isRotate = (gizmo.mode == GizmoMode::Rotate);
        if (isRotate) ImGui::PushStyleColor(ImGuiCol_Button, kAccentColor);
        if (ImGui::Button("Rotate")) gizmo.mode = GizmoMode::Rotate;
        if (isRotate) ImGui::PopStyleColor();

        ImGui::SameLine();

        bool isScale = (gizmo.mode == GizmoMode::Scale);
        if (isScale) ImGui::PushStyleColor(ImGuiCol_Button, kAccentColor);
        if (ImGui::Button("Scale")) gizmo.mode = GizmoMode::Scale;
        if (isScale) ImGui::PopStyleColor();

        ImGui::End();
        ImGui::PopStyleColor(); // kToolbarGlassBg
        ImGui::PopStyleVar(2);  // kToolbarFrameRounding, kToolbarWindowRounding
    }

    // Tips
    ImGui::SetNextWindowPos(ImVec2(viewport.x + 10, viewport.y + viewport.height - 60));
    ImGui::Begin("Tips", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoDocking);
    ImGui::TextColored(ImVec4(1,1,1,0.5f), "WASD-Move | Space-Particle | Middle-Pan | Scroll-Zoom");
    ImGui::End();

    // FPS / frame-time overlay, top-right corner of the Scene viewport (opposite GizmoToolbar's top-left).
    if (fpsSmoothed > 0.0f) {
        ImGui::SetNextWindowPos(ImVec2(viewport.x + viewport.width - 10, viewport.y + 10), ImGuiCond_Always, ImVec2(1.0f, 0.0f));
        ImGui::Begin("FPS", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoDocking);
        ImGui::TextColored(ImVec4(1,1,1,0.7f), "FPS: %.0f (%.1fms)", fpsSmoothed, 1000.0f / fpsSmoothed);
        ImGui::End();
    }

    return viewport;
}

void EditorUI::SetupUIStyle() {
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;

    // Flush, square, Xcode/Finder-sidebar-style rounding by default -- an earlier pass bumped
    // these up for a "soft Apple-app card" look, but live user feedback was that it broke
    // figure-ground unity: Hierarchy/Inspector/Scene read as a stack of separate floating rounded
    // cards instead of one cohesive window. Reverted to flush/near-zero here; the one place that
    // still wants soft "floating glass chip" rounding (GizmoToolbar) now gets it via a scoped
    // PushStyleVar around just that window instead of a global bump (see kToolbarWindowRounding).
    style.WindowRounding = 0.0f;
    style.FrameRounding = 4.0f;
    style.PopupRounding = 4.0f;
    style.ScrollbarRounding = 4.0f;
    style.GrabRounding = 4.0f;
    style.TabRounding = 4.0f;

    style.WindowPadding = ImVec2(10, 10);
    style.FramePadding = ImVec2(6, 4);
    style.ItemSpacing = ImVec2(8, 6);
    style.ScrollbarSize = 12.0f;

    // Thin (not removed -- a subtle hairline is still part of the chosen composition, just a much
    // quieter one than the all-sides glow a previous pass gave it) and explicit, rather than
    // relying on Dear ImGui's own default, so it's guaranteed to actually render.
    style.WindowBorderSize = 1.0f;

    colors[ImGuiCol_Text]                   = ImVec4(0.95f, 0.95f, 0.95f, 1.00f);
    colors[ImGuiCol_TextDisabled]           = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
    // Nearly opaque -- a previous pass dropped this to 0.82 for a visible "glass card" look, but
    // Hierarchy/Inspector/Scene are flush sidebars/world in this composition, not floating
    // translucent cards, so they read as solid, cohesive regions of one window. (Over this app's
    // black GL clear color the two are nearly indistinguishable anyway; kept just shy of fully
    // opaque so an undocked/floating panel -- the one case something could show through -- still
    // reads as faintly glass rather than a hard cut.) The one place real translucency against a
    // colorful backdrop belongs is GizmoToolbar's own scoped override (kToolbarGlassBg).
    colors[ImGuiCol_WindowBg]               = ImVec4(0.12f, 0.12f, 0.12f, 0.97f);
    colors[ImGuiCol_ChildBg]                = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_PopupBg]                = ImVec4(0.08f, 0.08f, 0.08f, 0.94f);
    // Much quieter than a previous pass's (1,1,1,0.18): a subtle single hairline reading as where
    // a sidebar meets the scene, not a glowing outline around every side of every panel.
    colors[ImGuiCol_Border]                 = ImVec4(1.00f, 1.00f, 1.00f, 0.10f);
    colors[ImGuiCol_BorderShadow]           = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_FrameBg]                = ImVec4(0.20f, 0.21f, 0.22f, 0.54f);
    colors[ImGuiCol_FrameBgHovered]         = ImVec4(0.40f, 0.40f, 0.40f, 0.40f);
    colors[ImGuiCol_FrameBgActive]          = ImVec4(0.18f, 0.18f, 0.18f, 0.67f);
    colors[ImGuiCol_TitleBg]                = ImVec4(0.04f, 0.04f, 0.04f, 1.00f);
    colors[ImGuiCol_TitleBgActive]          = ImVec4(0.29f, 0.29f, 0.29f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed]       = ImVec4(0.00f, 0.00f, 0.00f, 0.51f);
    // Flat and solid -- the menu bar isn't part of the "glass" treatment in this composition
    // either (that's concentrated on GizmoToolbar alone); back to fully opaque.
    colors[ImGuiCol_MenuBarBg]              = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
    colors[ImGuiCol_ScrollbarBg]            = ImVec4(0.02f, 0.02f, 0.02f, 0.53f);
    colors[ImGuiCol_ScrollbarGrab]          = ImVec4(0.31f, 0.31f, 0.31f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabHovered]   = ImVec4(0.41f, 0.41f, 0.41f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabActive]    = ImVec4(0.51f, 0.51f, 0.51f, 1.00f);
    colors[ImGuiCol_CheckMark]              = kAccentColor;
    colors[ImGuiCol_SliderGrab]             = ImVec4(0.51f, 0.51f, 0.51f, 1.00f);
    colors[ImGuiCol_SliderGrabActive]       = kAccentColorDark;
    colors[ImGuiCol_Button]                 = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
    colors[ImGuiCol_ButtonHovered]          = ImVec4(0.35f, 0.35f, 0.35f, 1.00f);
    colors[ImGuiCol_ButtonActive]           = ImVec4(0.15f, 0.15f, 0.15f, 1.00f);
    colors[ImGuiCol_Header]                 = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
    colors[ImGuiCol_HeaderHovered]          = ImVec4(0.35f, 0.35f, 0.35f, 1.00f);
    colors[ImGuiCol_HeaderActive]           = ImVec4(0.15f, 0.15f, 0.15f, 1.00f);
    // Matches the new, quieter Border color (same hairline family) instead of the old blue-gray --
    // these previously shared that exact literal, so they stay in sync with it.
    colors[ImGuiCol_Separator]              = ImVec4(1.00f, 1.00f, 1.00f, 0.10f);
    colors[ImGuiCol_SeparatorHovered]       = ImVec4(1.00f, 1.00f, 1.00f, 0.10f);
    colors[ImGuiCol_SeparatorActive]        = ImVec4(1.00f, 1.00f, 1.00f, 0.10f);
    colors[ImGuiCol_ResizeGrip]             = ImVec4(0.91f, 0.91f, 0.91f, 0.25f);
    colors[ImGuiCol_ResizeGripHovered]      = ImVec4(0.81f, 0.81f, 0.81f, 0.67f);
    colors[ImGuiCol_ResizeGripActive]       = ImVec4(0.46f, 0.46f, 0.46f, 0.95f);
    // Dock tabs: dark, slightly red-tinted when inactive, rising to the full red accent on
    // hover/active -- replaces Dear ImGui's default blue-ish tab palette (now an everyday sight
    // since Tasks 3-4 make dragging panels into tab groups a normal interaction).
    colors[ImGuiCol_Tab]                    = ImVec4(0.20f, 0.10f, 0.10f, 0.86f);
    colors[ImGuiCol_TabHovered]             = ImVec4(kAccentColor.x, kAccentColor.y, kAccentColor.z, 0.80f);
    colors[ImGuiCol_TabActive]              = ImVec4(0.45f, 0.14f, 0.13f, 1.00f);
    colors[ImGuiCol_TabUnfocused]           = ImVec4(0.10f, 0.06f, 0.06f, 0.97f);
    colors[ImGuiCol_TabUnfocusedActive]     = ImVec4(0.30f, 0.11f, 0.10f, 1.00f);
    colors[ImGuiCol_TabSelectedOverline]    = kAccentColor;
    colors[ImGuiCol_TabDimmedSelectedOverline] = ImVec4(kAccentColor.x, kAccentColor.y, kAccentColor.z, 0.60f);
    colors[ImGuiCol_DockingPreview]         = ImVec4(kAccentColor.x, kAccentColor.y, kAccentColor.z, 0.60f);
    colors[ImGuiCol_PlotLines]              = ImVec4(0.61f, 0.61f, 0.61f, 1.00f);
    colors[ImGuiCol_PlotLinesHovered]       = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
    colors[ImGuiCol_PlotHistogram]          = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
    colors[ImGuiCol_PlotHistogramHovered]   = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
    colors[ImGuiCol_TextSelectedBg]         = ImVec4(0.26f, 0.59f, 0.98f, 0.35f);
    colors[ImGuiCol_DragDropTarget]         = ImVec4(1.00f, 1.00f, 0.00f, 0.90f);
    colors[ImGuiCol_NavHighlight]           = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    colors[ImGuiCol_NavWindowingHighlight]  = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
    colors[ImGuiCol_NavWindowingDimBg]      = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
    colors[ImGuiCol_ModalWindowDimBg]       = ImVec4(0.80f, 0.80f, 0.80f, 0.35f);
}
