#include "Gizmo.h"
#include <cmath>
#include <algorithm>

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
    // Perpendicular hit-band half-width along each arrow's full line segment (not just its tip) —
    // this is what makes the whole visible arrow clickable, not a tiny circle at one end of it.
    const float kArrowHitHalfWidth = 8.0f;
    const float kCenterHalfSize = 6.0f;
    const float kCenterHitHalfSize = 14.0f;

    // Keeps Draw() and Update() agreeing on where the handles are.
    void ComputeHandlePositions(const Entity& entity, const Camera& cam, const ViewportRect& viewport,
                                 ImVec2& outCenter, ImVec2& outXTip, ImVec2& outYTip) {
        outCenter = WorldToScreen(entity.x, entity.y, cam, viewport);
        outXTip = ImVec2(outCenter.x + kArrowLength, outCenter.y);
        outYTip = ImVec2(outCenter.x, outCenter.y - kArrowLength);
    }

    // Shortest distance from point p to the line segment [a, b] (clamped to the segment, not
    // the infinite line), used to make an entire drawn arrow clickable along its length.
    float DistancePointToSegment(const ImVec2& p, const ImVec2& a, const ImVec2& b) {
        float abx = b.x - a.x, aby = b.y - a.y;
        float apx = p.x - a.x, apy = p.y - a.y;
        float lenSq = abx * abx + aby * aby;
        float t = (lenSq > 0.0001f) ? (apx * abx + apy * aby) / lenSq : 0.0f;
        t = std::max(0.0f, std::min(1.0f, t));
        float closestX = a.x + t * abx;
        float closestY = a.y + t * aby;
        float dx = p.x - closestX, dy = p.y - closestY;
        return std::sqrt(dx * dx + dy * dy);
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

    // Hit-testing must stay inside the scene viewport, mirroring the clip rect used in Draw():
    // a handle that's clipped out of view (e.g. panned behind the Hierarchy/Inspector panels)
    // must not still be grabbable there.
    bool insideViewport = (mouseScreenX >= viewport.x && mouseScreenX <= viewport.x + viewport.width &&
                            mouseScreenY >= 0.0f && mouseScreenY <= viewport.height);

    if (mouseClicked && !dragging && insideViewport) {
        ImVec2 mousePos(mouseScreenX, mouseScreenY);
        bool hitCenter = (mouseScreenX >= center.x - kCenterHitHalfSize && mouseScreenX <= center.x + kCenterHitHalfSize &&
                           mouseScreenY >= center.y - kCenterHitHalfSize && mouseScreenY <= center.y + kCenterHitHalfSize);
        bool hitX = DistancePointToSegment(mousePos, center, xTip) <= kArrowHitHalfWidth;
        bool hitY = DistancePointToSegment(mousePos, center, yTip) <= kArrowHitHalfWidth;

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
