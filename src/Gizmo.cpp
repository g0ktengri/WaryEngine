#include "Gizmo.h"

ImVec2 WorldToScreen(float worldX, float worldY, const Camera& cam, const ViewportRect& viewport) {
    float aspect = viewport.width / viewport.height;
    float ndcX = (worldX - cam.x) * cam.zoom / aspect;
    float ndcY = (worldY - cam.y) * cam.zoom;
    float screenX = (ndcX + 1.0f) / 2.0f * viewport.width + viewport.x;
    float screenY = (1.0f - ndcY) / 2.0f * viewport.height;
    return ImVec2(screenX, screenY);
}

void ScreenToWorld(float screenX, float screenY, const Camera& cam, const ViewportRect& viewport,
                   float& outWorldX, float& outWorldY) {
    float aspect = viewport.width / viewport.height;
    float localX = screenX - viewport.x;
    float localY = screenY;
    float ndcX = (localX / viewport.width) * 2.0f - 1.0f;
    float ndcY = 1.0f - (localY / viewport.height) * 2.0f;
    outWorldX = ndcX * aspect / cam.zoom + cam.x;
    outWorldY = ndcY / cam.zoom + cam.y;
}

void Gizmo::Draw(ImDrawList*, const Entity&, const Camera&, const ViewportRect&) {
}

bool Gizmo::Update(Entity&, const Camera&, const ViewportRect&,
                   bool, bool, bool, float, float) {
    return false;
}
