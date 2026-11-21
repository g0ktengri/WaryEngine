#include "Renderer.h"
#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_sdl2.h"
#include <iostream>
#include <filesystem>
#include <cmath>
#include <fstream>
#include <algorithm>
#include <cstddef>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "Input.h"

namespace fs = std::filesystem;

// The project's hand-trimmed glad header omits this core GL 1.5 enum; standard value.
#ifndef GL_DYNAMIC_DRAW
#define GL_DYNAMIC_DRAW 0x88E8
#endif

// Shader functionality
const char *vSrc = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoord;
layout (location = 2) in vec3 aColor;
layout (location = 3) in float aAlpha;
out vec2 TexCoord;
flat out vec3 Color;
flat out float Alpha;
uniform mat4 view;
void main() {
    // aPos is already world-space (sprite transform applied on the CPU when batching)
    gl_Position = view * vec4(aPos, 1.0);
    TexCoord = aTexCoord;
    Color = aColor;
    Alpha = aAlpha;
}
)";

// Color/Alpha are per-sprite constants, passed 'flat' so they reach the fragment stage
// bit-exact (no interpolation rounding) -- same math as the old tint/alpha uniforms.
const char *fSrc = R"(
#version 330 core
in vec2 TexCoord;
flat in vec3 Color;
flat in float Alpha;
out vec4 FragColor;
uniform sampler2D tex;
void main() {
    vec4 c = texture(tex, TexCoord);
    FragColor = vec4(c.rgb * Color, c.a * Alpha);
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
}

void Renderer::Init() {
    gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    InitShader();
    InitBuffers();
    InitGridBuffers();
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
    viewLoc = glGetUniformLocation(shaderProgram, "view");
}

void Renderer::InitBuffers() {
    static_assert(sizeof(SpriteVertex) == 9 * sizeof(float), "SpriteVertex must be tightly packed");

    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &spriteVBO);
    glGenBuffers(1, &spriteEBO);
    glBindVertexArray(VAO);

    // Dynamic vertex buffer, empty until the first DrawBatch re-specifies it with a group's vertices.
    glBindBuffer(GL_ARRAY_BUFFER, spriteVBO);
    glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_DYNAMIC_DRAW);

    // Index buffer starts empty; EnsureIndexCapacity fills it on demand. Bound here so it is
    // recorded as VAO's element buffer.
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, spriteEBO);
    spriteIndexCapacityQuads = 0;

    const GLsizei stride = sizeof(SpriteVertex);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void *)offsetof(SpriteVertex, x));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, (void *)offsetof(SpriteVertex, u));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, stride, (void *)offsetof(SpriteVertex, r));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, stride, (void *)offsetof(SpriteVertex, a));
    glEnableVertexAttribArray(3);
}

void Renderer::EnsureIndexCapacity(size_t quadCount) {
    if (quadCount <= spriteIndexCapacityQuads)
        return;

    std::vector<unsigned int> indices(quadCount * 6);
    for (size_t q = 0; q < quadCount; q++) {
        unsigned int base = (unsigned int)(q * 4);
        indices[q * 6 + 0] = base + 0;
        indices[q * 6 + 1] = base + 1;
        indices[q * 6 + 2] = base + 2;
        indices[q * 6 + 3] = base + 2;
        indices[q * 6 + 4] = base + 3;
        indices[q * 6 + 5] = base + 0;
    }
    // The element-array binding is VAO state: bind the sprite VAO first so this can never
    // rebind some other VAO's (e.g. gridVAO's) element buffer.
    glBindVertexArray(VAO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, spriteEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);
    spriteIndexCapacityQuads = quadCount;
}

// Column-major 4x4 (CreateTransform's layout, as uploaded with glUniformMatrix4fv transpose=GL_FALSE)
// times (lx, ly, 0, 1) -- the same result the old vertex shader's `transform * vec4(aPos, 1.0)` gave.
// z stays 0 and w stays 1 for every matrix CreateTransform builds. Verified by the Task 4 scratchpad proof.
void Renderer::TransformPoint(const float *m, float lx, float ly, float &ox, float &oy) {
    ox = m[0] * lx + m[4] * ly + m[12];
    oy = m[1] * lx + m[5] * ly + m[13];
}

// Appends one sprite's 4 world-space corners, in the old static quad's corner/UV order.
void Renderer::AppendQuad(std::vector<SpriteVertex> &out, const float *m, const float *color, float alpha) {
    static const float corners[4][4] = {
        // local x, local y, u, v
        {-0.5f, -0.5f, 0, 0},
        { 0.5f, -0.5f, 1, 0},
        { 0.5f,  0.5f, 1, 1},
        {-0.5f,  0.5f, 0, 1},
    };
    for (const auto &c : corners) {
        SpriteVertex sv;
        TransformPoint(m, c[0], c[1], sv.x, sv.y);
        sv.z = 0;
        sv.u = c[2];
        sv.v = c[3];
        sv.r = color[0];
        sv.g = color[1];
        sv.b = color[2];
        sv.a = alpha;
        out.push_back(sv);
    }
}

// Uploads one texture group's vertices and issues its single draw call. Expects the sprite
// program + VAO bound and EnsureIndexCapacity already called for at least this group's size.
void Renderer::DrawBatch(unsigned int textureId, const std::vector<SpriteVertex> &verts) {
    if (verts.empty())
        return; // zero-quad group: no upload, no draw call

    // Full re-specification per group: the driver orphans the previous store instead of stalling
    // on the previous group's in-flight draw. (The project's trimmed glad loader has no
    // glBufferSubData, so this is also the only upload path available.)
    glBindBuffer(GL_ARRAY_BUFFER, spriteVBO);
    glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(SpriteVertex), verts.data(), GL_DYNAMIC_DRAW);

    glBindTexture(GL_TEXTURE_2D, textureId);
    glDrawElements(GL_TRIANGLES, (GLsizei)(verts.size() / 4 * 6), GL_UNSIGNED_INT, 0);
}

void Renderer::InitGridBuffers() {
    float v[] = {-0.5f, -0.5f, 0, 0, 0, 0.5f,  -0.5f, 0, 1, 0,
                 0.5f,  0.5f,  0, 1, 1, -0.5f, 0.5f,  0, 0, 1};
    unsigned int i[] = {0, 1, 2, 2, 3, 0};

    glGenVertexArrays(1, &gridVAO);
    glGenBuffers(1, &gridVBO);
    glGenBuffers(1, &gridEBO);
    glBindVertexArray(gridVAO);
    glBindBuffer(GL_ARRAY_BUFFER, gridVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(v), v, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gridEBO);
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
    gridViewLoc = glGetUniformLocation(gridProgram, "view");
    gridTransformLoc = glGetUniformLocation(gridProgram, "transform");
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
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
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

void Renderer::Render(std::vector<Entity>& entities, std::vector<Particle>& particles, const Camera& cam, const ViewportRect& viewport, int windowHeight) {
    glClearColor(0.12f, 0.12f, 0.15f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    int glY = (int)(windowHeight - viewport.y - viewport.height);
    glViewport((int)viewport.x, glY, (int)viewport.width, (int)viewport.height);
    glUseProgram(shaderProgram);

    float vm[16];
    float aspect = viewport.width / viewport.height;
    CreateView(vm, cam, aspect);
    glUniformMatrix4fv(viewLoc, 1, 0, vm);

    // Draw Infinite Grid
    glUseProgram(gridProgram);
    glUniformMatrix4fv(gridViewLoc, 1, 0, vm);
    float gridTransform[16];
    // Scale quad to cover view. Standard quad is 1x1.
    CreateTransform(gridTransform, cam.x, cam.y, 0, 1000.0f / cam.zoom, 1000.0f / cam.zoom);
    glUniformMatrix4fv(gridTransformLoc, 1, 0, gridTransform);

    glBindVertexArray(gridVAO);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

    // --- Build sprite batches (CPU) ---
    // Entities: grouped by resolved texture id (same resolution rule as the old per-entity loop).
    // Within a group, entity order is preserved.
    unsigned int defaultTid = textures["default"];
    for (auto &kv : entityBatches)
        kv.second.clear(); // keep capacity across frames
    unsigned int lastEntityTid = defaultTid;
    for (auto &e : entities) {
        auto it = textures.find(e.textureName);
        unsigned int tid = (it != textures.end()) ? it->second : defaultTid;
        float tm[16];
        CreateTransform(tm, e.x, e.y, e.rotation, e.sx, e.sy);
        AppendQuad(entityBatches[tid], tm, e.color, 1.0f);
        lastEntityTid = tid;
    }

    // Particles: one group. The old particle loop never bound a texture of its own -- it drew with
    // whatever the entity loop bound last, i.e. the last entity's texture. Reproduce that exactly
    // (falling back to the default white texture when there are no entities).
    batchScratch.clear();
    for (auto &p : particles) {
        float tm[16];
        CreateTransform(tm, p.x, p.y, 0, 0.04f, 0.04f);
        AppendQuad(batchScratch, tm, p.color, p.life);
    }

    // One index-buffer growth check per frame, sized for the largest group of either pass.
    size_t maxQuads = batchScratch.size() / 4;
    for (auto &kv : entityBatches)
        maxQuads = std::max(maxQuads, kv.second.size() / 4);
    if (maxQuads > 0)
        EnsureIndexCapacity(maxQuads);

    // --- Draw: entities pass, then particles pass (same order as before batching) ---
    glUseProgram(shaderProgram);
    glBindVertexArray(VAO);
    for (auto it = entityBatches.begin(); it != entityBatches.end();) {
        if (it->second.empty()) {
            it = entityBatches.erase(it); // texture unused this frame: no upload, no draw
            continue;
        }
        DrawBatch(it->first, it->second);
        ++it;
    }
    DrawBatch(lastEntityTid, batchScratch);
}
