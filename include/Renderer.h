#ifndef RENDERER_H
#define RENDERER_H

#include <glad/glad.h>
#include <SDL.h>
#include <vector>
#include <map>
#include <string>
#include "Entity.h"
#include "EditorUI.h"

// Simple Renderer class to handle OpenGL calls
class Renderer {
public:
    Renderer();
    ~Renderer();

    void Init();
    void Render(std::vector<Entity>& entities, std::vector<Particle>& particles, const Camera& cam, const ViewportRect& viewport);
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
};

#endif
