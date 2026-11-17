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

namespace {
    const float kArrowLength = 60.0f;
    const float kHandleHitRadius = 10.0f;
    const float kCenterHalfSize = 6.0f;
    const float kCenterHitHalfSize = 10.0f;

    // Keeps Draw() and Update() agreeing on where the handles are.
    void ComputeHandlePositions(const Entity& entity, const Camera& cam, const ViewportRect& viewport,
                                 ImVec2& outCenter, ImVec2& outXTip, ImVec2& outYTip) {
        outCenter = WorldToScreen(entity.x, entity.y, cam, viewport);
        outXTip = ImVec2(outCenter.x + kArrowLength, outCenter.y);
        outYTip = ImVec2(outCenter.x, outCenter.y - kArrowLength);
    }
}

void Gizmo::Draw(ImDrawList* drawList, const Entity& entity, const Camera& cam, const ViewportRect& viewport) {
    if (mode != GizmoMode::Move) return;

    ImVec2 center, xTip, yTip;
    ComputeHandlePositions(entity, cam, viewport, center, xTip, yTip);

    drawList->PushClipRect(ImVec2(viewport.x, 0.0f), ImVec2(viewport.x + viewport.width, viewport.height), true);

    drawList->AddLine(center, xTip, IM_COL32(255, 0, 0, 255), 2.0f);
    drawList->AddLine(center, yTip, IM_COL32(0, 255, 0, 255), 2.0f);
    drawList->AddRectFilled(ImVec2(center.x - kCenterHalfSize, center.y - kCenterHalfSize),
                             ImVec2(center.x + kCenterHalfSize, center.y + kCenterHalfSize),
                             IM_COL32(255, 255, 255, 255));

    drawList->PopClipRect();
}

bool Gizmo::Update(Entity& entity, const Camera& cam, const ViewportRect& viewport,
                   bool mouseDown, bool mouseClicked, bool mouseReleased,
                   float mouseScreenX, float mouseScreenY) {
    if (mode != GizmoMode::Move) {
        dragging = false;
        activeHandle = -1;
        return false;
    }

    ImVec2 center, xTip, yTip;
    ComputeHandlePositions(entity, cam, viewport, center, xTip, yTip);

    if (mouseClicked && !dragging) {
        bool hitCenter = (mouseScreenX >= center.x - kCenterHitHalfSize && mouseScreenX <= center.x + kCenterHitHalfSize &&
                           mouseScreenY >= center.y - kCenterHitHalfSize && mouseScreenY <= center.y + kCenterHitHalfSize);
        float dxX = mouseScreenX - xTip.x, dyX = mouseScreenY - xTip.y;
        bool hitX = (dxX * dxX + dyX * dyX) <= (kHandleHitRadius * kHandleHitRadius);
        float dxY = mouseScreenX - yTip.x, dyY = mouseScreenY - yTip.y;
        bool hitY = (dxY * dxY + dyY * dyY) <= (kHandleHitRadius * kHandleHitRadius);

        int handle = -1;
        if (hitCenter) handle = 2;
        else if (hitX) handle = 0;
        else if (hitY) handle = 1;

        if (handle != -1) {
            dragging = true;
            activeHandle = handle;
            ScreenToWorld(mouseScreenX, mouseScreenY, cam, viewport, dragStartMouseX, dragStartMouseY);
            dragStartEntityX = entity.x;
            dragStartEntityY = entity.y;
        }
    }

    if (dragging && mouseDown) {
        float curWorldX, curWorldY;
        ScreenToWorld(mouseScreenX, mouseScreenY, cam, viewport, curWorldX, curWorldY);
        float deltaX = curWorldX - dragStartMouseX;
        float deltaY = curWorldY - dragStartMouseY;

        if (activeHandle == 0) {
            entity.x = dragStartEntityX + deltaX;
        } else if (activeHandle == 1) {
            entity.y = dragStartEntityY + deltaY;
        } else if (activeHandle == 2) {
            entity.x = dragStartEntityX + deltaX;
            entity.y = dragStartEntityY + deltaY;
        }
    }

    if (!mouseDown || mouseReleased) {
        dragging = false;
    }

    return dragging;
}
