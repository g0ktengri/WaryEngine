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

    // Fixed padding beyond the entity's on-screen half-diagonal, per the brief.
    const float kRotateRingPadding = 15.0f;
    // Hit-band half-width around the ring radius — generous like kArrowHitHalfWidth,
    // so clicking anywhere near the visible ring (not just pixel-perfect on it) grabs it.
    const float kRotateRingHitHalfWidth = 10.0f;

    // Scale handle square half-size (drawn) and a more generous hit-box half-size around it,
    // same "generous hit-testing" feel as Move's kCenterHitHalfSize.
    const float kScaleHandleHalfSize = 5.0f;
    const float kScaleHandleHitHalfSize = 12.0f;
    const float kScaleMin = 0.01f;
    const float kScaleMax = 10.0f;

    // Keeps Draw() and Update() agreeing on where the rotate ring is.
    float ComputeRotateRingRadius(const Entity& entity, const Camera& cam, const ViewportRect& viewport,
                                   ImVec2& outCenter) {
        outCenter = WorldToScreen(entity.x, entity.y, cam, viewport);
        ImVec2 corner = WorldToScreen(entity.x + entity.sx * 0.5f, entity.y + entity.sy * 0.5f, cam, viewport);
        float dx = corner.x - outCenter.x, dy = corner.y - outCenter.y;
        return std::sqrt(dx * dx + dy * dy) + kRotateRingPadding;
    }

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

    // Keeps Draw() and Update() agreeing on where Scale's 8 handles are.
    // Order: TL, TR, BL, BR, TopMid, BottomMid, LeftMid, RightMid — matching
    // activeHandle ids 10-17 (10+index into this array).
    void ComputeScaleHandlePositions(const Entity& entity, const Camera& cam, const ViewportRect& viewport,
                                      ImVec2 outHandles[8]) {
        float hx = entity.sx * 0.5f;
        float hy = entity.sy * 0.5f;
        outHandles[0] = WorldToScreen(entity.x - hx, entity.y + hy, cam, viewport); // TL corner
        outHandles[1] = WorldToScreen(entity.x + hx, entity.y + hy, cam, viewport); // TR corner
        outHandles[2] = WorldToScreen(entity.x - hx, entity.y - hy, cam, viewport); // BL corner
        outHandles[3] = WorldToScreen(entity.x + hx, entity.y - hy, cam, viewport); // BR corner
        outHandles[4] = WorldToScreen(entity.x, entity.y + hy, cam, viewport);      // top edge mid
        outHandles[5] = WorldToScreen(entity.x, entity.y - hy, cam, viewport);      // bottom edge mid
        outHandles[6] = WorldToScreen(entity.x - hx, entity.y, cam, viewport);      // left edge mid
        outHandles[7] = WorldToScreen(entity.x + hx, entity.y, cam, viewport);      // right edge mid
    }
}

void Gizmo::Draw(ImDrawList* drawList, const Entity& entity, const Camera& cam, const ViewportRect& viewport) {
    if (mode != GizmoMode::Move && mode != GizmoMode::Rotate && mode != GizmoMode::Scale) return;

    if (mode == GizmoMode::Rotate) {
        ImVec2 center;
        float ringRadius = ComputeRotateRingRadius(entity, cam, viewport, center);

        drawList->PushClipRect(ImVec2(viewport.x, 0.0f), ImVec2(viewport.x + viewport.width, viewport.height), true);
        drawList->AddCircle(center, ringRadius, IM_COL32(255, 220, 0, 255), 0, 2.0f);
        drawList->PopClipRect();
        return;
    }

    if (mode == GizmoMode::Scale) {
        ImVec2 handles[8];
        ComputeScaleHandlePositions(entity, cam, viewport, handles);

        drawList->PushClipRect(ImVec2(viewport.x, 0.0f), ImVec2(viewport.x + viewport.width, viewport.height), true);
        for (int i = 0; i < 8; i++) {
            drawList->AddRectFilled(ImVec2(handles[i].x - kScaleHandleHalfSize, handles[i].y - kScaleHandleHalfSize),
                                     ImVec2(handles[i].x + kScaleHandleHalfSize, handles[i].y + kScaleHandleHalfSize),
                                     IM_COL32(0, 200, 255, 255));
        }
        drawList->PopClipRect();
        return;
    }

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
    if (mode != GizmoMode::Move && mode != GizmoMode::Rotate && mode != GizmoMode::Scale) {
        dragging = false;
        activeHandle = -1;
        return false;
    }

    if (mode == GizmoMode::Scale) {
        ImVec2 handles[8];
        ComputeScaleHandlePositions(entity, cam, viewport, handles);

        // Hit-testing must stay inside the scene viewport, mirroring the clip rect used in Draw().
        bool insideViewport = (mouseScreenX >= viewport.x && mouseScreenX <= viewport.x + viewport.width &&
                                mouseScreenY >= 0.0f && mouseScreenY <= viewport.height);

        if (mouseClicked && !dragging && insideViewport) {
            int handle = -1;
            for (int i = 0; i < 8; i++) {
                if (mouseScreenX >= handles[i].x - kScaleHandleHitHalfSize && mouseScreenX <= handles[i].x + kScaleHandleHitHalfSize &&
                    mouseScreenY >= handles[i].y - kScaleHandleHitHalfSize && mouseScreenY <= handles[i].y + kScaleHandleHitHalfSize) {
                    handle = 10 + i; // distinct range from Move's 0/1/2 and Rotate's 3
                    break;
                }
            }

            if (handle != -1) {
                dragging = true;
                activeHandle = handle;
                // Center doesn't move during a scale drag — cache it, same pattern as Move's
                // dragStartEntityX/Y, so later computations use a stable reference even though
                // entity.x/y aren't actually mutated by this mode.
                dragStartEntityX = entity.x;
                dragStartEntityY = entity.y;
                dragStartSx = entity.sx;
                dragStartSy = entity.sy;
                // Cache the grab-time mouse world position (reusing Move's dragStart* fields,
                // unused by Scale otherwise) as the baseline distance-from-center to diff
                // against each frame, so the drag is relative rather than absolute. This is
                // what makes the grab frame itself a zero-change frame regardless of where
                // within the handle's hit-box the user actually clicked.
                ScreenToWorld(mouseScreenX, mouseScreenY, cam, viewport, dragStartMouseX, dragStartMouseY);
            }
        }

        if (dragging && mouseDown) {
            float curWorldX, curWorldY;
            ScreenToWorld(mouseScreenX, mouseScreenY, cam, viewport, curWorldX, curWorldY);
            float curHalfX = std::fabs(curWorldX - dragStartEntityX);
            float curHalfY = std::fabs(curWorldY - dragStartEntityY);
            float startHalfX = std::fabs(dragStartMouseX - dragStartEntityX);
            float startHalfY = std::fabs(dragStartMouseY - dragStartEntityY);
            float newHalfX = dragStartSx * 0.5f + (curHalfX - startHalfX);
            float newHalfY = dragStartSy * 0.5f + (curHalfY - startHalfY);

            bool isCorner = (activeHandle >= 10 && activeHandle <= 13);
            bool isTopOrBottomMid = (activeHandle == 14 || activeHandle == 15);
            bool isLeftOrRightMid = (activeHandle == 16 || activeHandle == 17);

            if (isCorner) {
                entity.sx = std::clamp(2.0f * newHalfX, kScaleMin, kScaleMax);
                entity.sy = std::clamp(2.0f * newHalfY, kScaleMin, kScaleMax);
            } else if (isTopOrBottomMid) {
                entity.sy = std::clamp(2.0f * newHalfY, kScaleMin, kScaleMax);
            } else if (isLeftOrRightMid) {
                entity.sx = std::clamp(2.0f * newHalfX, kScaleMin, kScaleMax);
            }
        }

        if (!mouseDown || mouseReleased) {
            dragging = false;
        }

        return dragging;
    }

    if (mode == GizmoMode::Rotate) {
        ImVec2 center;
        float ringRadius = ComputeRotateRingRadius(entity, cam, viewport, center);

        // Hit-testing must stay inside the scene viewport, mirroring the clip rect used in Draw().
        bool insideViewport = (mouseScreenX >= viewport.x && mouseScreenX <= viewport.x + viewport.width &&
                                mouseScreenY >= 0.0f && mouseScreenY <= viewport.height);

        if (mouseClicked && !dragging && insideViewport) {
            float mdx = mouseScreenX - center.x;
            float mdy = mouseScreenY - center.y;
            float distFromCenter = std::sqrt(mdx * mdx + mdy * mdy);
            bool hitRing = std::fabs(distFromCenter - ringRadius) <= kRotateRingHitHalfWidth;

            if (hitRing) {
                dragging = true;
                activeHandle = 3; // distinct marker; rotate doesn't use Move's 0/1/2 handle ids
                float dragStartWorldX, dragStartWorldY;
                ScreenToWorld(mouseScreenX, mouseScreenY, cam, viewport, dragStartWorldX, dragStartWorldY);
                dragStartAngle = std::atan2(dragStartWorldY - entity.y, dragStartWorldX - entity.x);
                dragStartRotation = entity.rotation;
            }
        }

        if (dragging && mouseDown) {
            float curWorldX, curWorldY;
            ScreenToWorld(mouseScreenX, mouseScreenY, cam, viewport, curWorldX, curWorldY);
            float currentAngle = std::atan2(curWorldY - entity.y, curWorldX - entity.x);
            entity.rotation = dragStartRotation + (currentAngle - dragStartAngle);
        }

        if (!mouseDown || mouseReleased) {
            dragging = false;
        }

        return dragging;
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
