#ifndef GIZMO_H
#define GIZMO_H

#include "imgui.h"
#include "Entity.h"

enum class GizmoMode { Move, Rotate, Scale };

// Free coordinate helpers (usable by EditorUI picking as well as the gizmo)
ImVec2 WorldToScreen(float worldX, float worldY, const Camera& cam, const ViewportRect& viewport);
void ScreenToWorld(float screenX, float screenY, const Camera& cam, const ViewportRect& viewport,
                   float& outWorldX, float& outWorldY);

class Gizmo {
public:
    GizmoMode mode = GizmoMode::Move;

    void Draw(ImDrawList* drawList, const Entity& entity, const Camera& cam, const ViewportRect& viewport);
    // Returns true while a drag is in progress.
    bool Update(Entity& entity, const Camera& cam, const ViewportRect& viewport,
                bool mouseDown, bool mouseClicked, bool mouseReleased,
                float mouseScreenX, float mouseScreenY);

private:
    bool dragging = false;
    int activeHandle = -1;
    float dragStartMouseX = 0.0f;
    float dragStartMouseY = 0.0f;
    float dragStartEntityX = 0.0f;
    float dragStartEntityY = 0.0f;
    float dragStartRotation = 0.0f;
    float dragStartSx = 0.0f;
    float dragStartSy = 0.0f;
    float dragStartAngle = 0.0f;
};

#endif
