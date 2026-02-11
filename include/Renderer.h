#ifndef RENDERER_H
#define RENDERER_H

#include <glad/glad.h>
#include <SDL.h>
#include <vector>
#include <map>
#include <string>
#include "Entity.h"

// Simple Renderer class to handle OpenGL calls
class Renderer {
public:
    Renderer();
    ~Renderer();

    void Init(SDL_Window* window);
    void Render(SDL_Window* window, std::vector<Entity>& entities, std::vector<Particle>& particles, const Camera& cam, int& selectedEntityIndex, std::vector<std::vector<Entity>>& undoStack);
    void RefreshTextures();

    // Helpers
    const std::vector<std::string>& GetTextureList() const { return textureList; }

private:
    unsigned int shaderProgram;
    unsigned int gridProgram;
    unsigned int VAO, VBO, EBO;
    
    // Grid
    unsigned int gridVAO, gridVBO;
    unsigned int axisVAO, axisVBO;
    std::vector<float> gridVertices;

    // Textures
    std::map<std::string, unsigned int> textures;
    std::vector<std::string> textureList;

    void InitShader();
    void InitBuffers();
    void InitInfiniteGrid();
    unsigned int LoadTexture(const char* path);
    
    // Math helpers
    void CreateTransform(float* m, float x, float y, float r, float sx, float sy);
    void CreateView(float* m, Camera c, float aspect);
    bool CheckPointInside(Entity& e, float px, float py);
    void SetupUIStyle();
};

#endif
