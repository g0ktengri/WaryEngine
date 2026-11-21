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
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "view"), 1, 0, vm);

    // Draw Infinite Grid
    glUseProgram(gridProgram);
    glUniformMatrix4fv(glGetUniformLocation(gridProgram, "view"), 1, 0, vm);
    float gridTransform[16];
    // Scale quad to cover view. Standard quad is 1x1.
    CreateTransform(gridTransform, cam.x, cam.y, 0, 1000.0f / cam.zoom, 1000.0f / cam.zoom);
    glUniformMatrix4fv(glGetUniformLocation(gridProgram, "transform"), 1, 0, gridTransform);

    glBindVertexArray(gridVAO);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

    // Draw Entities
    glUseProgram(shaderProgram);
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
}
