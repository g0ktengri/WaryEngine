#include "Renderer.h"
#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_sdl2.h"
#include <iostream>
#include <filesystem>
#include <cmath>
#include <fstream>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "Input.h"

namespace fs = std::filesystem;

// Shader functionality
const char *vSrc = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoord;
out vec2 TexCoord;
uniform mat4 transform;
uniform mat4 view;
void main() {
    gl_Position = view * transform * vec4(aPos, 1.0);
    TexCoord = aTexCoord;
}
)";

const char *fSrc = R"(
#version 330 core
in vec2 TexCoord;
out vec4 FragColor;
uniform sampler2D tex;
uniform vec3 tint;
uniform float alpha;
void main() {
    vec4 c = texture(tex, TexCoord);
    FragColor = vec4(c.rgb * tint, c.a * alpha);
}
)";

const char *gridVSrc = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
uniform mat4 view;
uniform mat4 transform;
out vec3 WorldPos;
void main() {
    vec4 world = transform * vec4(aPos, 1.0);
    WorldPos = world.xyz;
    gl_Position = view * world;
}
)";

const char *gridFSrc = R"(
#version 330 core
in vec3 WorldPos;
out vec4 FragColor;

// Grid calculation
float grid(vec2 coord, float spacing) {
    vec2 size = vec2(spacing);
    vec2 derivative = fwidth(coord);
    vec2 grid = abs(fract(coord / spacing - 0.5) - 0.5) / derivative * spacing;
    float line = min(grid.x, grid.y);
    float pixelSize = length(derivative);
    
    // Fade out when lines become too dense (aliasing)
    float fade = 1.0 - smoothstep(0.2 * spacing, 1.0 * spacing, pixelSize);
    return (1.0 - min(line, 1.0)) * fade;
}

void main() {
    float g1 = grid(WorldPos.xy, 1.0);   // 1 unit grid
    float g10 = grid(WorldPos.xy, 10.0); // 10 unit grid
    
    // Colors
    vec4 col1 = vec4(0.3, 0.3, 0.3, 0.3); // Faint
    vec4 col10 = vec4(0.4, 0.4, 0.4, 0.6); // Stronger
    
    vec4 outCol = vec4(0);
    
    // Composite
    // Max blending for lines
    float alpha = max(g1 * col1.a, g10 * col10.a);
    vec3 rgb = mix(col1.rgb, col10.rgb, g10 / (g1 + g10 + 0.0001));
    
    outCol = vec4(rgb, alpha);
    
    // Axis (Thick and Colored)
    vec2 derivative = fwidth(WorldPos.xy);
    if (abs(WorldPos.x) < derivative.x * 2.0) outCol = vec4(0.2, 0.8, 0.2, 0.9); // Y Axis (Green)
    if (abs(WorldPos.y) < derivative.y * 2.0) outCol = vec4(0.8, 0.2, 0.2, 0.9); // X Axis (Red)

    if (outCol.a < 0.05) discard;
    FragColor = outCol;
}
)";

Renderer::Renderer() {
}

Renderer::~Renderer() {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
}

void Renderer::Init(SDL_Window* window) {
    SDL_GLContext ctx = SDL_GL_CreateContext(window);
    gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress);
    
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    // Load Segoe UI font
    if (std::filesystem::exists("C:/Windows/Fonts/segoeui.ttf")) {
        io.Fonts->AddFontFromFileTTF("C:/Windows/Fonts/segoeui.ttf", 20.0f);
    }

    SetupUIStyle();
    ImGui_ImplSDL2_InitForOpenGL(window, ctx);
    ImGui_ImplOpenGL3_Init("#version 330");

    InitShader();
    InitBuffers();
    InitInfiniteGrid();
    RefreshTextures();
}

void Renderer::InitShader() {
    unsigned int vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &vSrc, 0);
    glCompileShader(vs);
    unsigned int fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &fSrc, 0);
    glCompileShader(fs);
    shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vs);
    glAttachShader(shaderProgram, fs);
    glLinkProgram(shaderProgram);
}

void Renderer::InitBuffers() {
    float v[] = {-0.5f, -0.5f, 0, 0, 0, 0.5f,  -0.5f, 0, 1, 0,
                 0.5f,  0.5f,  0, 1, 1, -0.5f, 0.5f,  0, 0, 1};
    unsigned int i[] = {0, 1, 2, 2, 3, 0};
    
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(v), v, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(i), i, GL_STATIC_DRAW);
    
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), 0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void *)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
}

void Renderer::InitInfiniteGrid() {
    unsigned int vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &gridVSrc, 0);
    glCompileShader(vs);
    unsigned int fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &gridFSrc, 0);
    glCompileShader(fs);
    gridProgram = glCreateProgram();
    glAttachShader(gridProgram, vs);
    glAttachShader(gridProgram, fs);
    glLinkProgram(gridProgram);
    glDeleteShader(vs);
    glDeleteShader(fs);
}

void Renderer::RefreshTextures() {
    textures.clear();
    textureList.clear();
    if (!fs::exists("assets"))
        fs::create_directory("assets");
    
    for (const auto &entry : fs::directory_iterator("assets")) {
        std::string path = entry.path().string();
        std::string filename = entry.path().filename().string();
        if (path.find(".png") != std::string::npos || path.find(".jpg") != std::string::npos) {
            unsigned int id = LoadTexture(path.c_str());
            textures[filename] = id;
            textureList.push_back(filename);
        }
    }
    
    // Default white texture
    if (textures.find("default") == textures.end()) {
        unsigned int id;
        glGenTextures(1, &id);
        unsigned char white[] = {255, 255, 255, 255};
        glBindTexture(GL_TEXTURE_2D, id);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, white);
        textures["default"] = id;
        textureList.push_back("default");
    }
}

unsigned int Renderer::LoadTexture(const char* p) {
    unsigned int id;
    glGenTextures(1, &id);
    int w, h, n;
    stbi_set_flip_vertically_on_load(true);
    unsigned char *d = stbi_load(p, &w, &h, &n, 0);
    if (d) {
        GLenum f = (n == 4) ? GL_RGBA : GL_RGB;
        glBindTexture(GL_TEXTURE_2D, id);
        glTexImage2D(GL_TEXTURE_2D, 0, f, w, h, 0, f, GL_UNSIGNED_BYTE, d);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        stbi_image_free(d);
    }
    return id;
}

void Renderer::CreateTransform(float *m, float x, float y, float r, float sx, float sy) {
    float c = cos(r), sn = sin(r);
    m[0] = c * sx;   m[4] = -sn * sy; m[8] = 0;  m[12] = x;
    m[1] = sn * sx;  m[5] = c * sy;   m[9] = 0;  m[13] = y;
    m[2] = 0;       m[6] = 0;       m[10] = 1; m[14] = 0;
    m[3] = 0;       m[7] = 0;       m[11] = 0; m[15] = 1;
}

void Renderer::CreateView(float *m, Camera c, float aspect) {
    m[0] = c.zoom / aspect; m[4] = 0;      m[8] = 0;  m[12] = -c.x * c.zoom / aspect;
    m[1] = 0;               m[5] = c.zoom; m[9] = 0;  m[13] = -c.y * c.zoom;
    m[2] = 0;               m[6] = 0;      m[10] = 1; m[14] = 0;
    m[3] = 0;               m[7] = 0;      m[11] = 0; m[15] = 1;
}

bool Renderer::CheckPointInside(Entity &e, float px, float py) {
    float halfX = e.sx / 2.0f;
    float halfY = e.sy / 2.0f;
    return (px > e.x - halfX && px < e.x + halfX && py > e.y - halfY && py < e.y + halfY);
}

void Renderer::Render(SDL_Window* window, std::vector<Entity>& entities, std::vector<Particle>& particles, const Camera& cam, int& selected, std::vector<std::vector<Entity>>& undoStack) {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();
    
    // Undo Logic - Capture state before edit
    static bool wasActive = false;
    static std::vector<Entity> preEditState;
    if (ImGui::IsAnyItemActive()) {
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

    int winW, winH;
    
    // ... (rest of function until Inspector)
    SDL_GetWindowSize(window, &winW, &winH);
    float leftPanelWidth = winW * 0.2f;
    float rightPanelWidth = winW * 0.25f;
    float sceneX = leftPanelWidth;
    float sceneW = winW - leftPanelWidth - rightPanelWidth;
    float sceneH = (float)winH;

    // --- Input / Picking ---
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGui::GetIO().WantCaptureMouse) {
        int mx, my;
        SDL_GetMouseState(&mx, &my);
        float localMx = mx - sceneX;
        float localMy = my;
        if (localMx >= 0 && localMx <= sceneW && localMy >= 0 && localMy <= sceneH) {
             float aspect = sceneW / sceneH;
             float ndcX = (localMx / sceneW) * 2.0f - 1.0f;
             float ndcY = 1.0f - (localMy / sceneH) * 2.0f;
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
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(leftPanelWidth, (float)winH));
    ImGui::Begin("Hierarchy", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);
    if (ImGui::Button("Save", ImVec2(-1, 0))) { /* Save logic */ } 
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
    ImGui::SetNextWindowPos(ImVec2((float)winW - rightPanelWidth, 0));
    ImGui::SetNextWindowSize(ImVec2(rightPanelWidth, (float)winH));
    ImGui::Begin("Inspector", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);
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

    // Tips
    ImGui::SetNextWindowPos(ImVec2(leftPanelWidth + 10, (float)winH - 60));
    ImGui::Begin("Tips", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoBackground);
    ImGui::TextColored(ImVec4(1,1,1,0.5f), "WASD-Move | Space-Particle | Middle-Pan | Scroll-Zoom");
    ImGui::End();

    // --- Scene Render ---
    glClearColor(0.12f, 0.12f, 0.15f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    
    glViewport((int)sceneX, 0, (int)sceneW, (int)sceneH);
    glUseProgram(shaderProgram);
    
    float vm[16];
    float aspect = sceneW / sceneH;
    CreateView(vm, cam, aspect);
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "view"), 1, 0, vm);

    // Draw Grid
    // Draw Infinite Grid
    glUseProgram(gridProgram);
    glUniformMatrix4fv(glGetUniformLocation(gridProgram, "view"), 1, 0, vm);
    float gridTransform[16];
    // Scale quad to cover view. Standard quad is 1x1.
    CreateTransform(gridTransform, cam.x, cam.y, 0, 1000.0f / cam.zoom, 1000.0f / cam.zoom);
    glUniformMatrix4fv(glGetUniformLocation(gridProgram, "transform"), 1, 0, gridTransform);
    
    glBindVertexArray(VAO); // Re-use standard quad
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

    // Draw Entities
    glBindVertexArray(VAO);
    for (auto &e : entities) {
        float tm[16];
        CreateTransform(tm, e.x, e.y, e.rotation, e.sx, e.sy);
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "transform"), 1, 0, tm);
        glUniform3f(glGetUniformLocation(shaderProgram, "tint"), e.color[0], e.color[1], e.color[2]);
        glUniform1f(glGetUniformLocation(shaderProgram, "alpha"), 1.0f);
        
        unsigned int tid = textures.count(e.textureName) ? textures[e.textureName] : textures["default"];
        glBindTexture(GL_TEXTURE_2D, tid);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    }
    
    // Draw Particles
    for (auto &p : particles) {
        float tm[16];
        CreateTransform(tm, p.x, p.y, 0, 0.04f, 0.04f);
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "transform"), 1, 0, tm);
        glUniform3f(glGetUniformLocation(shaderProgram, "tint"), p.color[0], p.color[1], p.color[2]);
        glUniform1f(glGetUniformLocation(shaderProgram, "alpha"), p.life);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    }

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    SDL_GL_SwapWindow(window);
}



void Renderer::SetupUIStyle() {
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
