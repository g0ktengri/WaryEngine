#define SDL_MAIN_HANDLED
#include "Input.h"
#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_sdl2.h"
#include <SDL.h>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <glad/glad.h>
#include <map>
#include <string>
#include <vector>


#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

namespace fs = std::filesystem;

struct Entity {
  std::string name;
  float x, y;
  float rotation;
  float scale;
  float color[3];
  std::string textureName; // Store name instead of ID for serialization
  bool active;
  bool hasGravity;
  float vy;
};

struct Particle {
  float x, y, vx, vy, life;
  float color[3];
};

struct Camera {
  float x = 0, y = 0, zoom = 1.0f;
};

// Texture Manager
std::map<std::string, unsigned int> textures;
std::vector<std::string> textureList;

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

unsigned int LoadTexture(const char *p) {
  unsigned int id;
  glGenTextures(1, &id);
  int w, h, n;
  stbi_set_flip_vertically_on_load(true);
  unsigned char *d = stbi_load(p, &w, &h, &n, 0);
  if (d) {
    GLenum f = (n == 4) ? GL_RGBA : GL_RGB;
    glBindTexture(GL_TEXTURE_2D, id);
    glTexImage2D(GL_TEXTURE_2D, 0, f, w, h, 0, f, GL_UNSIGNED_BYTE, d);
    glTexParameteri(GL_TEXTURE_2D, 0x2802, 0x812F);
    glTexParameteri(GL_TEXTURE_2D, 0x2803, 0x812F);
    glTexParameteri(GL_TEXTURE_2D, 0x2801, 0x2601);
    glTexParameteri(GL_TEXTURE_2D, 0x2800, 0x2601);
    stbi_image_free(d);
  }
  return id;
}

void RefreshTextures() {
  textures.clear();
  textureList.clear();
  if (!fs::exists("assets"))
    fs::create_directory("assets");
  for (const auto &entry : fs::directory_iterator("assets")) {
    std::string path = entry.path().string();
    std::string filename = entry.path().filename().string();
    // Simple check for image extensions
    if (path.find(".png") != std::string::npos ||
        path.find(".jpg") != std::string::npos) {
      unsigned int id = LoadTexture(path.c_str());
      textures[filename] = id;
      textureList.push_back(filename);
    }
  }
  if (textures.empty()) {
    // Create a default white texture if no assets found
    unsigned int id;
    glGenTextures(1, &id);
    unsigned char white[] = {255, 255, 255, 255};
    glBindTexture(GL_TEXTURE_2D, id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 white);
    textures["default"] = id;
    textureList.push_back("default");
  }
}

void CreateTransform(float *m, float x, float y, float r, float s) {
  float c = cos(r), sn = sin(r);
  m[0] = c * s;
  m[4] = -sn * s;
  m[8] = 0;
  m[12] = x;
  m[1] = sn * s;
  m[5] = c * s;
  m[9] = 0;
  m[13] = y;
  m[2] = 0;
  m[6] = 0;
  m[10] = 1;
  m[14] = 0;
  m[3] = 0;
  m[7] = 0;
  m[11] = 0;
  m[15] = 1;
}

void CreateView(float *m, Camera c) {
  m[0] = c.zoom;
  m[4] = 0;
  m[8] = 0;
  m[12] = -c.x * c.zoom;
  m[1] = 0;
  m[5] = c.zoom;
  m[9] = 0;
  m[13] = -c.y * c.zoom;
  m[2] = 0;
  m[6] = 0;
  m[10] = 1;
  m[14] = 0;
  m[3] = 0;
  m[7] = 0;
  m[11] = 0;
  m[15] = 1;
}

bool CheckPointInside(Entity &e, float px, float py) {
  float half = e.scale / 2.0f;
  return (px > e.x - half && px < e.x + half && py > e.y - half &&
          py < e.y + half);
}

void SaveScene(const std::vector<Entity> &entities) {
  std::ofstream f("scene.wary");
  for (auto &e : entities) {
    f << e.name << " " << e.x << " " << e.y << " " << e.rotation << " "
      << e.scale << " " << e.color[0] << " " << e.color[1] << " " << e.color[2]
      << " " << e.hasGravity << " " << e.textureName << "\n";
  }
}

void LoadScene(std::vector<Entity> &entities) {
  std::ifstream f("scene.wary");
  if (!f.is_open())
    return;
  entities.clear();
  char name[64];
  float x, y, r, s, c0, c1, c2;
  int g;
  std::string tName;
  while (f >> name >> x >> y >> r >> s >> c0 >> c1 >> c2 >> g >> tName) {
    entities.push_back(
        {name, x, y, r, s, {c0, c1, c2}, tName, true, (bool)g, 0});
  }
}

int main(int argc, char **argv) {
  SDL_Init(SDL_INIT_VIDEO);
  SDL_Window *win =
      SDL_CreateWindow("Wary Engine", SDL_WINDOWPOS_CENTERED,
                       SDL_WINDOWPOS_CENTERED, 1280, 720, SDL_WINDOW_OPENGL);
  SDL_GLContext ctx = SDL_GL_CreateContext(win);
  gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGui_ImplSDL2_InitForOpenGL(win, ctx);
  ImGui_ImplOpenGL3_Init("#version 330");

  unsigned int vs = glCreateShader(0x8B31);
  glShaderSource(vs, 1, &vSrc, 0);
  glCompileShader(vs);
  unsigned int fs = glCreateShader(0x8B30);
  glShaderSource(fs, 1, &fSrc, 0);
  glCompileShader(fs);
  unsigned int prog = glCreateProgram();
  glAttachShader(prog, vs);
  glAttachShader(prog, fs);
  glLinkProgram(prog);

  float v[] = {-0.5f, -0.5f, 0, 0, 0, 0.5f,  -0.5f, 0, 1, 0,
               0.5f,  0.5f,  0, 1, 1, -0.5f, 0.5f,  0, 0, 1};
  unsigned int i[] = {0, 1, 2, 2, 3, 0};
  unsigned int VBO, VAO, EBO;
  glGenVertexArrays(1, &VAO);
  glGenBuffers(1, &VBO);
  glGenBuffers(1, &EBO);
  glBindVertexArray(VAO);
  glBindBuffer(0x8892, VBO);
  glBufferData(0x8892, sizeof(v), v, 0x88E4);
  glBindBuffer(0x8893, EBO);
  glBufferData(0x8893, sizeof(i), i, 0x88E4);
  glVertexAttribPointer(0, 3, 0x1406, 0, 5 * 4, 0);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(1, 2, 0x1406, 0, 5 * 4, (void *)12);
  glEnableVertexAttribArray(1);

  RefreshTextures();
  std::string defaultTex = textureList.empty() ? "" : textureList[0];

  std::vector<Entity> entities;
  entities.push_back(
      {"Player", 0, 0, 0, 0.4f, {1, 1, 1}, defaultTex, true, false, 0});

  std::vector<Particle> particles;
  Camera cam;
  int selected = 0;
  bool running = true;
  SDL_Event ev;

  while (running) {
    while (SDL_PollEvent(&ev)) {
      ImGui_ImplSDL2_ProcessEvent(&ev);
      if (ev.type == SDL_QUIT)
        running = false;
      Input::Update(ev);
      if (ev.type == SDL_MOUSEWHEEL)
        cam.zoom *= (ev.wheel.y > 0 ? 1.1f : 0.9f);
      if (ev.type == SDL_MOUSEBUTTONDOWN &&
          ev.button.button == SDL_BUTTON_LEFT) {
        if (!ImGui::GetIO().WantCaptureMouse) {
          int mx, my;
          SDL_GetMouseState(&mx, &my);
          float wx = ((float)mx / 1280.0f * 2.0f - 1.0f) / cam.zoom + cam.x;
          float wy = (1.0f - (float)my / 720.0f * 2.0f) / cam.zoom + cam.y;
          for (int k = (int)entities.size() - 1; k >= 0; k--) {
            if (CheckPointInside(entities[k], wx, wy)) {
              selected = k;
              break;
            }
          }
        }
      }
    }

    if (SDL_GetMouseState(NULL, NULL) & SDL_BUTTON(SDL_BUTTON_MIDDLE)) {
      int mx, my;
      SDL_GetRelativeMouseState(&mx, &my);
      cam.x -= (float)mx / 500 / cam.zoom;
      cam.y += (float)my / 500 / cam.zoom;
    }
    if (selected >= (int)entities.size())
      selected = (int)entities.size() - 1;

    if (selected >= 0 && selected < (int)entities.size()) {
      float s = 0.02f / cam.zoom;
      if (Input::IsKeyDown(SDL_SCANCODE_W))
        entities[selected].y += s;
      if (Input::IsKeyDown(SDL_SCANCODE_S))
        entities[selected].y -= s;
      if (Input::IsKeyDown(SDL_SCANCODE_A))
        entities[selected].x -= s;
      if (Input::IsKeyDown(SDL_SCANCODE_D))
        entities[selected].x += s;
      if (Input::IsKeyDown(SDL_SCANCODE_SPACE)) {
        for (int k = 0; k < 2; k++)
          particles.push_back({entities[selected].x,
                               entities[selected].y,
                               (float)(rand() % 100 - 50) / 1000,
                               (float)(rand() % 100 - 50) / 1000,
                               1.0f,
                               {1, 0.9f, 0.1f}});
      }
    }

    for (auto &e : entities) {
      if (e.hasGravity) {
        e.vy -= 0.001f;
        e.y += e.vy;
        if (e.y < -0.8f) {
          e.y = -0.8f;
          e.vy *= -0.5f;
        }
      }
    }
    for (auto it = particles.begin(); it != particles.end();) {
      it->x += it->vx;
      it->y += it->vy;
      it->life -= 0.015f;
      if (it->life <= 0)
        it = particles.erase(it);
      else
        ++it;
    }

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();

    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(250, 400), ImGuiCond_FirstUseEver);
    ImGui::Begin("Hierarchy / Scene");
    if (ImGui::Button("Save"))
      SaveScene(entities);
    ImGui::SameLine();
    if (ImGui::Button("Load")) {
      LoadScene(entities);
      selected = 0;
    }
    ImGui::SameLine();
    if (ImGui::Button("Refresh Assets"))
      RefreshTextures();
    ImGui::Separator();
    for (int i = 0; i < (int)entities.size(); i++) {
      if (ImGui::Selectable(
              (entities[i].name + "##" + std::to_string(i)).c_str(),
              i == selected))
        selected = i;
    }
    if (ImGui::Button("+ New Entity"))
      entities.push_back({"Prop",
                          cam.x,
                          cam.y,
                          0,
                          0.3f,
                          {1, 1, 1},
                          defaultTex,
                          true,
                          false,
                          0});
    ImGui::End();

    ImGui::SetNextWindowPos(ImVec2(1020, 10), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(250, 400), ImGuiCond_FirstUseEver);
    ImGui::Begin("Inspector");
    if (selected >= 0 && selected < (int)entities.size()) {
      Entity &e = entities[selected];
      char nBuf[64];
      strcpy(nBuf, e.name.c_str());
      if (ImGui::InputText("Name", nBuf, 64))
        e.name = nBuf;
      ImGui::Separator();

      // Texture Selector
      if (ImGui::BeginCombo("Texture", e.textureName.c_str())) {
        for (auto &texName : textureList) {
          bool is_selected = (e.textureName == texName);
          if (ImGui::Selectable(texName.c_str(), is_selected))
            e.textureName = texName;
          if (is_selected)
            ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
      }

      ImGui::DragFloat("Pos X", &e.x, 0.01f);
      ImGui::DragFloat("Pos Y", &e.y, 0.01f);
      ImGui::SliderFloat("Rot", &e.rotation, -3.14f, 3.14f);
      ImGui::DragFloat("Scale", &e.scale, 0.01f, 0.01f, 5.0f);
      ImGui::ColorEdit3("Color", e.color);
      ImGui::Checkbox("Gravity", &e.hasGravity);
      if (ImGui::Button("Delete Entity")) {
        entities.erase(entities.begin() + selected);
        selected = 0;
      }
    }
    ImGui::End();

    ImGui::Begin("Help");
    ImGui::Text("Drop .png/.jpg files into 'assets' folder.");
    ImGui::Text("Then click 'Refresh Assets'.");
    ImGui::End();

    glClearColor(0.12f, 0.12f, 0.15f, 1.0f);
    glClear(0x00004000);
    glUseProgram(prog);
    float vm[16];
    CreateView(vm, cam);
    glUniformMatrix4fv(glGetUniformLocation(prog, "view"), 1, 0, vm);
    glBindVertexArray(VAO);
    for (auto &e : entities) {
      float tm[16];
      CreateTransform(tm, e.x, e.y, e.rotation, e.scale);
      glUniformMatrix4fv(glGetUniformLocation(prog, "transform"), 1, 0, tm);
      glUniform3f(glGetUniformLocation(prog, "tint"), e.color[0], e.color[1],
                  e.color[2]);
      glUniform1f(glGetUniformLocation(prog, "alpha"), 1.0f);

      // Bind correct texture
      unsigned int tid = textures.count(e.textureName) ? textures[e.textureName]
                                                       : textures["default"];
      glBindTexture(0x0DE1, tid);
      glDrawElements(4, 6, 0x1405, 0);
    }
    for (auto &p : particles) {
      float tm[16];
      CreateTransform(tm, p.x, p.y, 0, 0.04f);
      glUniformMatrix4fv(glGetUniformLocation(prog, "transform"), 1, 0, tm);
      glUniform3f(glGetUniformLocation(prog, "tint"), p.color[0], p.color[1],
                  p.color[2]);
      glUniform1f(glGetUniformLocation(prog, "alpha"), p.life);
      glDrawElements(4, 6, 0x1405, 0);
    }
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    SDL_GL_SwapWindow(win);
  }
  return 0;
}
