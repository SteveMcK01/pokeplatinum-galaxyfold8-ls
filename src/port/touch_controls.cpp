#include <nitro.h>
#include <cmath>
#include <cstdio>
#include <simulator/sim.h>
#include <simulator/sim_carts.h>
#include <simulator/sim_gui.hpp>
#include <simulator/sim_settings.hpp>
#include <simulator/sim_ui.hpp>
#include <simulator/imgui/imgui.hpp>
#include <simulator/simvariables.h>

#include "port/sim_config_prj.h"
#include "port/touch_controls.h"

namespace {

enum VirtualBtn {
    VBTN_A = 0,
    VBTN_B,
    VBTN_X,
    VBTN_Y,
    VBTN_L,
    VBTN_R,
    VBTN_START,
    VBTN_SELECT,
    VBTN_UP,
    VBTN_DOWN,
    VBTN_LEFT,
    VBTN_RIGHT,
    VBTN_COUNT
};

static u16 sPrevPadMask = 0;
static u16 sPrevXYMask = 0;

static bool PointInCircle(float px, float py, float cx, float cy, float r) {
    const float dx = px - cx;
    const float dy = py - cy;
    return (dx * dx + dy * dy) <= (r * r);
}

static bool PointInRect(float px, float py, float x0, float y0, float x1, float y1) {
    return px >= x0 && px <= x1 && py >= y0 && py <= y1;
}

static void DrawCircleButton(ImDrawList *dl, float cx, float cy, float r, const char *label,
                             bool pressed, float alpha, ImU32 accentCol) {
    const ImU32 bgCol = pressed
        ? IM_COL32((accentCol >> 0) & 0xFF, (accentCol >> 8) & 0xFF, (accentCol >> 16) & 0xFF, (int)(220.0f * alpha))
        : IM_COL32(24, 28, 36, (int)(155.0f * alpha));
    const ImU32 borderCol = pressed
        ? IM_COL32(255, 255, 255, (int)(245.0f * alpha))
        : IM_COL32(220, 228, 240, (int)(140.0f * alpha));
    const ImU32 textCol = IM_COL32(255, 255, 255, (int)((pressed ? 255.0f : 220.0f) * alpha));

    dl->AddCircleFilled(ImVec2(cx, cy), r, bgCol, 32);
    dl->AddCircle(ImVec2(cx, cy), r, borderCol, 32, pressed ? 2.5f : 1.75f);

    ImFont *font = SIM::UI::BoldFont();
    const float fontSize = r * 0.85f;
    ImVec2 ts = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, label);
    dl->AddText(font, fontSize, ImVec2(cx - ts.x * 0.5f, cy - ts.y * 0.5f), textCol, label);
}

static void DrawPillButton(ImDrawList *dl, float x0, float y0, float x1, float y1, const char *label,
                           bool pressed, float alpha) {
    const float rounding = (y1 - y0) * 0.5f;
    const ImU32 bgCol = pressed
        ? IM_COL32(95, 165, 255, (int)(215.0f * alpha))
        : IM_COL32(24, 28, 36, (int)(155.0f * alpha));
    const ImU32 borderCol = pressed
        ? IM_COL32(255, 255, 255, (int)(240.0f * alpha))
        : IM_COL32(220, 228, 240, (int)(140.0f * alpha));
    const ImU32 textCol = IM_COL32(255, 255, 255, (int)((pressed ? 255.0f : 215.0f) * alpha));

    dl->AddRectFilled(ImVec2(x0, y0), ImVec2(x1, y1), bgCol, rounding);
    dl->AddRect(ImVec2(x0, y0), ImVec2(x1, y1), borderCol, rounding, 0, pressed ? 2.2f : 1.5f);

    ImFont *font = SIM::UI::BoldFont();
    const float fontSize = (y1 - y0) * 0.48f;
    ImVec2 ts = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, label);
    dl->AddText(font, fontSize, ImVec2((x0 + x1 - ts.x) * 0.5f, (y0 + y1 - ts.y) * 0.5f), textCol, label);
}

static void DrawDpad(ImDrawList *dl, float cx, float cy, float outerR,
                     bool up, bool down, bool left, bool right, float alpha) {
    const float armHalf = outerR * 0.34f;
    const float round = outerR * 0.14f;
    const ImU32 baseBg = IM_COL32(24, 28, 36, (int)(155.0f * alpha));
    const ImU32 borderCol = IM_COL32(220, 228, 240, (int)(140.0f * alpha));
    const ImU32 activeBg = IM_COL32(95, 165, 255, (int)(220.0f * alpha));

    // Subtle circular backing pad for thumb grip guidance
    dl->AddCircleFilled(ImVec2(cx, cy), outerR * 1.12f, IM_COL32(15, 18, 24, (int)(85.0f * alpha)), 36);
    dl->AddCircle(ImVec2(cx, cy), outerR * 1.12f, IM_COL32(220, 228, 240, (int)(65.0f * alpha)), 36, 1.25f);

    // Cross arms
    dl->AddRectFilled(ImVec2(cx - armHalf, cy - outerR), ImVec2(cx + armHalf, cy + outerR), baseBg, round);
    dl->AddRectFilled(ImVec2(cx - outerR, cy - armHalf), ImVec2(cx + outerR, cy + armHalf), baseBg, round);
    dl->AddRect(ImVec2(cx - armHalf, cy - outerR), ImVec2(cx + armHalf, cy + outerR), borderCol, round, 0, 1.5f);
    dl->AddRect(ImVec2(cx - outerR, cy - armHalf), ImVec2(cx + outerR, cy + armHalf), borderCol, round, 0, 1.5f);

    // Highlight active directions
    if (up) {
        dl->AddRectFilled(ImVec2(cx - armHalf + 2.0f, cy - outerR + 2.0f),
                          ImVec2(cx + armHalf - 2.0f, cy - armHalf), activeBg, round);
    }
    if (down) {
        dl->AddRectFilled(ImVec2(cx - armHalf + 2.0f, cy + armHalf),
                          ImVec2(cx + armHalf - 2.0f, cy + outerR - 2.0f), activeBg, round);
    }
    if (left) {
        dl->AddRectFilled(ImVec2(cx - outerR + 2.0f, cy - armHalf + 2.0f),
                          ImVec2(cx - armHalf, cy + armHalf - 2.0f), activeBg, round);
    }
    if (right) {
        dl->AddRectFilled(ImVec2(cx + armHalf, cy - armHalf + 2.0f),
                          ImVec2(cx + outerR - 2.0f, cy + armHalf - 2.0f), activeBg, round);
    }
}

static void ApplyButtonMasks(u16 padMask, u16 xyMask) {
    // Release only bits that were previously held by the touch overlay
    const u16 releasedPad = sPrevPadMask & ~padMask;
    const u16 releasedXY = sPrevXYMask & ~xyMask;

    s_reg_PAD_KEYINPUT = (s_reg_PAD_KEYINPUT | releasedPad) & ~padMask;
    *(vu16 *)HW_BUTTON_XY_BUF = (*(vu16 *)HW_BUTTON_XY_BUF | releasedXY) & ~xyMask;

    sPrevPadMask = padMask;
    sPrevXYMask = xyMask;
}

} // namespace

extern "C" {

void TouchControls_DrawOverlay(void) {
    SIM_Config_prj_type *cfg = SIM_Config_prj_GetConfig();
    if (!cfg->touchOverlay || SIM::Settings::IsOpen() || SIM_Carts_UIActive()) {
        if (sPrevPadMask != 0 || sPrevXYMask != 0) {
            ApplyButtonMasks(0, 0);
        }
        return;
    }

    const ImVec2 disp = ImGui::GetIO().DisplaySize;
    if (disp.x <= 1.0f || disp.y <= 1.0f) {
        return;
    }

    const float minDim = disp.x < disp.y ? disp.x : disp.y;
    const float scale = (cfg->touchOverlayScale > 0.4f && cfg->touchOverlayScale < 2.2f)
                            ? cfg->touchOverlayScale
                            : 1.0f;
    const float alpha = (cfg->touchOverlayOpacity > 0.05f && cfg->touchOverlayOpacity <= 1.0f)
                            ? cfg->touchOverlayOpacity
                            : 0.65f;

    // Responsive sizing tuned for Galaxy Z Fold 8 cover & inner unfolded screens
    const float unit = minDim * 0.068f * scale;
    const float marginX = unit * 0.65f;
    const float marginBottom = unit * 0.65f;

    // D-Pad geometry (bottom-left)
    const float dpadR = unit * 1.65f;
    const float dpadCx = marginX + dpadR * 1.15f;
    const float dpadCy = disp.y - marginBottom - dpadR * 1.15f;

    // Face buttons diamond geometry (bottom-right)
    const float faceBtnR = unit * 0.64f;
    const float diamondSpread = unit * 1.25f;
    const float faceCx = disp.x - marginX - diamondSpread - faceBtnR;
    const float faceCy = dpadCy;

    const float aCx = faceCx + diamondSpread;
    const float aCy = faceCy;
    const float bCx = faceCx;
    const float bCy = faceCy + diamondSpread;
    const float xCx = faceCx;
    const float xCy = faceCy - diamondSpread;
    const float yCx = faceCx - diamondSpread;
    const float yCy = faceCy;

    // L / R shoulder bumpers (above D-Pad and face buttons)
    const float lrW = unit * 1.55f;
    const float lrH = unit * 0.68f;
    const float lrY0 = dpadCy - dpadR * 1.15f - unit * 0.45f - lrH;
    const float lrY1 = lrY0 + lrH;
    const float lX0 = marginX;
    const float lX1 = lX0 + lrW;
    const float rX1 = disp.x - marginX;
    const float rX0 = rX1 - lrW;

    // SELECT / START pills (bottom center-flanking)
    const float ssW = unit * 1.35f;
    const float ssH = unit * 0.52f;
    const float ssY1 = disp.y - marginBottom * 0.75f;
    const float ssY0 = ssY1 - ssH;
    const float gap = unit * 0.45f;
    const float selX1 = disp.x * 0.5f - gap * 0.5f;
    const float selX0 = selX1 - ssW;
    const float staX0 = disp.x * 0.5f + gap * 0.5f;
    const float staX1 = staX0 + ssW;

    bool pressed[VBTN_COUNT] = {};

    // Poll all active SDL2 touch fingers simultaneously
    const int numDevices = SDL_GetNumTouchDevices();
    for (int d = 0; d < numDevices; ++d) {
        const SDL_TouchID dev = SDL_GetTouchDevice(d);
        if (dev == 0) {
            continue;
        }
        const int numFingers = SDL_GetNumTouchFingers(dev);
        for (int f = 0; f < numFingers; ++f) {
            const SDL_Finger *finger = SDL_GetTouchFinger(dev, f);
            if (!finger) {
                continue;
            }
            const float fx = finger->x * disp.x;
            const float fy = finger->y * disp.y;

            // D-Pad 8-way hit zone (generous outer hit radius, small center deadzone)
            const float dx = fx - dpadCx;
            const float dy = fy - dpadCy;
            const float distSq = dx * dx + dy * dy;
            const float maxDpadHit = dpadR * 1.45f;
            const float deadzone = dpadR * 0.22f;
            if (distSq >= (deadzone * deadzone) && distSq <= (maxDpadHit * maxDpadHit)) {
                const float angle = std::atan2(dy, dx); // -PI to PI
                // 8-way sectors (each cardinal spans 112.5 deg so diagonals overlap smoothly)
                if (angle > -2.55f && angle < -0.59f) pressed[VBTN_UP] = true;
                if (angle > 0.59f && angle < 2.55f) pressed[VBTN_DOWN] = true;
                if (angle > 2.16f || angle < -2.16f) pressed[VBTN_LEFT] = true;
                if (angle > -0.98f && angle < 0.98f) pressed[VBTN_RIGHT] = true;
            }

            // Face buttons (with generous 1.25x hit radius for thumb comfort)
            const float hitR = faceBtnR * 1.25f;
            if (PointInCircle(fx, fy, aCx, aCy, hitR)) pressed[VBTN_A] = true;
            if (PointInCircle(fx, fy, bCx, bCy, hitR)) pressed[VBTN_B] = true;
            if (PointInCircle(fx, fy, xCx, xCy, hitR)) pressed[VBTN_X] = true;
            if (PointInCircle(fx, fy, yCx, yCy, hitR)) pressed[VBTN_Y] = true;

            // Shoulders & Start/Select
            if (PointInRect(fx, fy, lX0, lrY0, lX1, lrY1)) pressed[VBTN_L] = true;
            if (PointInRect(fx, fy, rX0, lrY0, rX1, lrY1)) pressed[VBTN_R] = true;
            if (PointInRect(fx, fy, selX0, ssY0, selX1, ssY1)) pressed[VBTN_SELECT] = true;
            if (PointInRect(fx, fy, staX0, ssY0, staX1, ssY1)) pressed[VBTN_START] = true;
        }
    }

    // Build DS hardware active-low bitmasks
    u16 padMask = 0;
    if (pressed[VBTN_A])      padMask |= (1u << 0);
    if (pressed[VBTN_B])      padMask |= (1u << 1);
    if (pressed[VBTN_SELECT]) padMask |= (1u << 2);
    if (pressed[VBTN_START])  padMask |= (1u << 3);
    if (pressed[VBTN_RIGHT])  padMask |= (1u << 4);
    if (pressed[VBTN_LEFT])   padMask |= (1u << 5);
    if (pressed[VBTN_UP])     padMask |= (1u << 6);
    if (pressed[VBTN_DOWN])   padMask |= (1u << 7);
    if (pressed[VBTN_R])      padMask |= (1u << 8);
    if (pressed[VBTN_L])      padMask |= (1u << 9);

    u16 xyMask = 0;
    if (pressed[VBTN_X])      xyMask |= (1u << 10);
    if (pressed[VBTN_Y])      xyMask |= (1u << 11);

    ApplyButtonMasks(padMask, xyMask);

    // Render controls on the ImGui foreground draw list
    ImDrawList *dl = ImGui::GetForegroundDrawList();
    DrawDpad(dl, dpadCx, dpadCy, dpadR,
             pressed[VBTN_UP], pressed[VBTN_DOWN], pressed[VBTN_LEFT], pressed[VBTN_RIGHT], alpha);

    DrawCircleButton(dl, aCx, aCy, faceBtnR, "A", pressed[VBTN_A], alpha, IM_COL32(235, 75, 85, 255));
    DrawCircleButton(dl, bCx, bCy, faceBtnR, "B", pressed[VBTN_B], alpha, IM_COL32(235, 185, 55, 255));
    DrawCircleButton(dl, xCx, xCy, faceBtnR, "X", pressed[VBTN_X], alpha, IM_COL32(75, 145, 240, 255));
    DrawCircleButton(dl, yCx, yCy, faceBtnR, "Y", pressed[VBTN_Y], alpha, IM_COL32(70, 200, 120, 255));

    DrawPillButton(dl, lX0, lrY0, lX1, lrY1, "L", pressed[VBTN_L], alpha);
    DrawPillButton(dl, rX0, lrY0, rX1, lrY1, "R", pressed[VBTN_R], alpha);
    DrawPillButton(dl, selX0, ssY0, selX1, ssY1, "SELECT", pressed[VBTN_SELECT], alpha);
    DrawPillButton(dl, staX0, ssY0, staX1, ssY1, "START", pressed[VBTN_START], alpha);
}

} // namespace SIM::GUI

extern "C" void TouchControls_DrawSettings(void) {
    SIM_Config_prj_type *cfg = SIM_Config_prj_GetConfig();
    bool enabled = cfg->touchOverlay != FALSE;
    if (ImGui::Checkbox("Enable on-screen touch controller", &enabled)) {
        cfg->touchOverlay = enabled ? TRUE : FALSE;
        SIM_Config_prj_SaveConfigFile(cfg);
    }
    ImGui::TextWrapped("Multi-touch D-Pad, A/B/X/Y, L/R, and Start/Select overlay for touchscreen play.");

    ImGui::Spacing();
    if (ImGui::SliderFloat("Button opacity", &cfg->touchOverlayOpacity, 0.15f, 1.0f, "%.2f")) {
        SIM_Config_prj_SaveConfigFile(cfg);
    }
    if (ImGui::SliderFloat("Button scale", &cfg->touchOverlayScale, 0.65f, 1.50f, "%.2f")) {
        SIM_Config_prj_SaveConfigFile(cfg);
    }
}
