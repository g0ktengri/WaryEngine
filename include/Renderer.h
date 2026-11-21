#ifndef RENDERER_H
#define RENDERER_H

#include <glad/glad.h>
#include <SDL.h>
#include <vector>
#include <map>
#include <string>
#include "Entity.h"   // Particle, Camera, ViewportRect
#include "Registry.h"
#include "EditorUI.h"

// Simple Renderer class to handle OpenGL calls
class Renderer {
public:
    Renderer();
    ~Renderer();

    void Init();
    void Render(Registry& registry, std::vector<Particle>& particles, const Camera& cam, const ViewportRect& viewport, int windowHeight);
    void RefreshTextures();

    // Helpers
    const std::vector<std::string>& GetTextureList() const { return textureList; }

private:
    // One batched sprite vertex: world-space position, UV, per-sprite tint and alpha.
    struct SpriteVertex { float x, y, z, u, v, r, g, b, a; };

    unsigned int shaderProgram;
    unsigned int gridProgram;

    // Sprite batching (entities + particles). VAO describes the SpriteVertex layout;
    // spriteVBO is re-filled per texture group per frame; spriteEBO holds the shared
    // {0,1,2,2,3,0}+4k quad index pattern, grown (never shrunk) to the largest group seen.
    unsigned int VAO, spriteVBO, spriteEBO;
    size_t spriteIndexCapacityQuads = 0;  // spriteEBO capacity, in quads
    std::vector<SpriteVertex> batchScratch;                      // particle pass vertices
    std::map<unsigned int, std::vector<SpriteVertex>> entityBatches; // entity pass, keyed by GL texture id

    // Cached uniform locations (looked up once, after each program links)
    int viewLoc = -1, gridViewLoc = -1, gridTransformLoc = -1;

    // Grid
    unsigned int gridVAO, gridVBO, gridEBO;
    unsigned int axisVAO, axisVBO;
    std::vector<float> gridVertices;

    // Textures
    std::map<std::string, unsigned int> textures;
    std::vector<std::string> textureList;

    void InitShader();
    void InitBuffers();
    void InitGridBuffers();
    void InitInfiniteGrid();
    unsigned int LoadTexture(const char* path);

    // Batching helpers
    void EnsureIndexCapacity(size_t quadCount);
    void DrawBatch(unsigned int textureId, const std::vector<SpriteVertex>& verts);
    static void TransformPoint(const float* m, float lx, float ly, float& ox, float& oy);
    static void AppendQuad(std::vector<SpriteVertex>& out, const float* m, const float* color, float alpha);

    // Math helpers
    void CreateTransform(float* m, float x, float y, float r, float sx, float sy);
    void CreateView(float* m, Camera c, float aspect);
};

#endif
