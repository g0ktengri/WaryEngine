#include "EditorUI.h"
#include "imgui.h"
#include "imgui_internal.h" // DockBuilder* API: not yet promoted to the public imgui.h surface on this docking-branch version.
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl3.h"
#include <filesystem>
#include <cstring>

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

    int winW, winH;
    SDL_GetWindowSize(window, &winW, &winH);

    // --- DockSpace host + default first-run layout ---
    // Stable ID computed independent of any window's ID stack (ImGui::GetID() needs a
    // current window, which doesn't exist yet this early in the frame) so it's identical
    // every frame and can be used both to query DockBuilder state and to feed DockSpaceOverViewport().
    ImGuiID dockspaceId = ImHashStr("WaryEditorDockSpace");

    if (ImGui::DockBuilderGetNode(dockspaceId) == nullptr) {
        // First run (no saved layout for this dockspace yet): lay out Hierarchy left (~20%),
        // Inspector right (~25%), everything else left as the implicit central node (the scene).
        ImGui::DockBuilderRemoveNode(dockspaceId);
        ImGui::DockBuilderAddNode(dockspaceId, (ImGuiDockNodeFlags)((int)ImGuiDockNodeFlags_DockSpace | (int)ImGuiDockNodeFlags_PassthruCentralNode));
        ImGui::DockBuilderSetNodeSize(dockspaceId, ImVec2((float)winW, (float)winH));

        ImGuiID centerId = dockspaceId;
        ImGuiID leftId = ImGui::DockBuilderSplitNode(centerId, ImGuiDir_Left, 0.20f, nullptr, &centerId);
        // centerId is now the remaining 80%; split off 0.3125 of THAT so the right panel is
        // 25% of the *total* width (0.3125 * 0.80 == 0.25), leaving a true 55% central node.
        ImGuiID rightId = ImGui::DockBuilderSplitNode(centerId, ImGuiDir_Right, 0.3125f, nullptr, &centerId);

        ImGui::DockBuilderDockWindow("Hierarchy", leftId);
        ImGui::DockBuilderDockWindow("Inspector", rightId);
        ImGui::DockBuilderFinish(dockspaceId);
    }

    ImGui::DockSpaceOverViewport(dockspaceId, nullptr, ImGuiDockNodeFlags_PassthruCentralNode);

    // Central dock node's live on-screen Pos/Size *is* the scene viewport. Queried fresh every
    // frame (not just on first-run) so it tracks drags/resizes/re-docks as the user does them.
    ImGuiDockNode* centralNode = ImGui::DockBuilderGetCentralNode(dockspaceId);
    ViewportRect viewport;
    if (centralNode) {
        viewport = ViewportRect{centralNode->Pos.x, centralNode->Pos.y, centralNode->Size.x, centralNode->Size.y};
    } else {
        // Shouldn't happen given the layout above always keeps an implicit central node, but
        // fall back to the full window rather than an uninitialized/garbage viewport.
        viewport = ViewportRect{0.0f, 0.0f, (float)winW, (float)winH};
    }

    int mx, my;
    SDL_GetMouseState(&mx, &my);

    // --- Gizmo (claims its own clicks before entity-picking runs) ---
    bool gizmoActive = false;
    if (selected >= 0 && selected < (int)entities.size()) {
        gizmo.Draw(ImGui::GetForegroundDrawList(), entities[selected], cam, viewport);
        gizmoActive = gizmo.Update(entities[selected], cam, viewport,
                                    ImGui::IsMouseDown(ImGuiMouseButton_Left),
                                    ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGui::GetIO().WantCaptureMouse,
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
    if (!gizmoActive && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGui::GetIO().WantCaptureMouse) {
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
        undoStack.push_back(entities); // Save before adding
        entities.push_back({"Prop", cam.x, cam.y, 0, 0.3f, 0.3f, {1,1,1}, "default", true, false, 0});
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

    // Gizmo mode toolbar — overlaid at the top of the scene viewport, only while something is selected.
    if (selected >= 0 && selected < (int)entities.size()) {
        ImGui::SetNextWindowPos(ImVec2(viewport.x + 10, viewport.y + 10));
        ImGui::Begin("GizmoToolbar", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove);

        bool isMove = (gizmo.mode == GizmoMode::Move);
        if (isMove) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.94f, 0.50f, 0.09f, 1.00f));
        if (ImGui::Button("Move")) gizmo.mode = GizmoMode::Move;
        if (isMove) ImGui::PopStyleColor();

        ImGui::SameLine();

        bool isRotate = (gizmo.mode == GizmoMode::Rotate);
        if (isRotate) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.94f, 0.50f, 0.09f, 1.00f));
        if (ImGui::Button("Rotate")) gizmo.mode = GizmoMode::Rotate;
        if (isRotate) ImGui::PopStyleColor();

        ImGui::SameLine();

        bool isScale = (gizmo.mode == GizmoMode::Scale);
        if (isScale) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.94f, 0.50f, 0.09f, 1.00f));
        if (ImGui::Button("Scale")) gizmo.mode = GizmoMode::Scale;
        if (isScale) ImGui::PopStyleColor();

        ImGui::End();
    }

    // Tips
    ImGui::SetNextWindowPos(ImVec2(viewport.x + 10, viewport.y + viewport.height - 60));
    ImGui::Begin("Tips", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoBackground);
    ImGui::TextColored(ImVec4(1,1,1,0.5f), "WASD-Move | Space-Particle | Middle-Pan | Scroll-Zoom");
    ImGui::End();

    return viewport;
}

void EditorUI::SetupUIStyle() {
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;

    style.WindowRounding = 6.0f;
    style.FrameRounding = 4.0f;
    style.PopupRounding = 4.0f;
    style.ScrollbarRounding = 4.0f;
    style.GrabRounding = 4.0f;
    style.TabRounding = 4.0f;

    style.WindowPadding = ImVec2(10, 10);
    style.FramePadding = ImVec2(6, 4);
    style.ItemSpacing = ImVec2(8, 6);
    style.ScrollbarSize = 12.0f;

    colors[ImGuiCol_Text]                   = ImVec4(0.95f, 0.95f, 0.95f, 1.00f);
    colors[ImGuiCol_TextDisabled]           = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
    colors[ImGuiCol_WindowBg]               = ImVec4(0.12f, 0.12f, 0.12f, 1.00f);
    colors[ImGuiCol_ChildBg]                = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_PopupBg]                = ImVec4(0.08f, 0.08f, 0.08f, 0.94f);
    colors[ImGuiCol_Border]                 = ImVec4(0.43f, 0.43f, 0.50f, 0.50f);
    colors[ImGuiCol_BorderShadow]           = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_FrameBg]                = ImVec4(0.20f, 0.21f, 0.22f, 0.54f);
    colors[ImGuiCol_FrameBgHovered]         = ImVec4(0.40f, 0.40f, 0.40f, 0.40f);
    colors[ImGuiCol_FrameBgActive]          = ImVec4(0.18f, 0.18f, 0.18f, 0.67f);
    colors[ImGuiCol_TitleBg]                = ImVec4(0.04f, 0.04f, 0.04f, 1.00f);
    colors[ImGuiCol_TitleBgActive]          = ImVec4(0.29f, 0.29f, 0.29f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed]       = ImVec4(0.00f, 0.00f, 0.00f, 0.51f);
    colors[ImGuiCol_MenuBarBg]              = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
    colors[ImGuiCol_ScrollbarBg]            = ImVec4(0.02f, 0.02f, 0.02f, 0.53f);
    colors[ImGuiCol_ScrollbarGrab]          = ImVec4(0.31f, 0.31f, 0.31f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabHovered]   = ImVec4(0.41f, 0.41f, 0.41f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabActive]    = ImVec4(0.51f, 0.51f, 0.51f, 1.00f);
    colors[ImGuiCol_CheckMark]              = ImVec4(0.94f, 0.50f, 0.09f, 1.00f);
    colors[ImGuiCol_SliderGrab]             = ImVec4(0.51f, 0.51f, 0.51f, 1.00f);
    colors[ImGuiCol_SliderGrabActive]       = ImVec4(0.86f, 0.45f, 0.07f, 1.00f);
    colors[ImGuiCol_Button]                 = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
    colors[ImGuiCol_ButtonHovered]          = ImVec4(0.35f, 0.35f, 0.35f, 1.00f);
    colors[ImGuiCol_ButtonActive]           = ImVec4(0.15f, 0.15f, 0.15f, 1.00f);
    colors[ImGuiCol_Header]                 = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
    colors[ImGuiCol_HeaderHovered]          = ImVec4(0.35f, 0.35f, 0.35f, 1.00f);
    colors[ImGuiCol_HeaderActive]           = ImVec4(0.15f, 0.15f, 0.15f, 1.00f);
    colors[ImGuiCol_Separator]              = ImVec4(0.43f, 0.43f, 0.50f, 0.50f);
    colors[ImGuiCol_SeparatorHovered]       = ImVec4(0.43f, 0.43f, 0.50f, 0.50f);
    colors[ImGuiCol_SeparatorActive]        = ImVec4(0.43f, 0.43f, 0.50f, 0.50f);
    colors[ImGuiCol_ResizeGrip]             = ImVec4(0.91f, 0.91f, 0.91f, 0.25f);
    colors[ImGuiCol_ResizeGripHovered]      = ImVec4(0.81f, 0.81f, 0.81f, 0.67f);
    colors[ImGuiCol_ResizeGripActive]       = ImVec4(0.46f, 0.46f, 0.46f, 0.95f);
    colors[ImGuiCol_Tab]                    = ImVec4(0.18f, 0.35f, 0.58f, 0.86f);
    colors[ImGuiCol_TabHovered]             = ImVec4(0.26f, 0.59f, 0.98f, 0.80f);
    colors[ImGuiCol_TabActive]              = ImVec4(0.20f, 0.41f, 0.68f, 1.00f);
    colors[ImGuiCol_TabUnfocused]           = ImVec4(0.07f, 0.10f, 0.15f, 0.97f);
    colors[ImGuiCol_TabUnfocusedActive]     = ImVec4(0.14f, 0.26f, 0.42f, 1.00f);
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
