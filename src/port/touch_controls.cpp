#include <nitro.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <mutex>
#include <vector>
#include <simulator/sim.h>
#include <simulator/sim_carts.h>
#include <simulator/sim_companion.h>
#include <simulator/sim_gui.hpp>
#include <simulator/sim_settings.hpp>
#include <simulator/sim_ui.hpp>
#include <simulator/imgui/imgui.hpp>
#include <simulator/simvariables.h>

#ifdef __ANDROID__
#include <jni.h>
#endif

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
    VBTN_SWITCH,
    VBTN_FF,
    VBTN_COUNT
};

struct TouchPt {
    float x;
    float y;
};

struct BottomLayout {
    float dpadCx, dpadCy, dpadR;
    float faceCx, faceCy, faceBtnR, diamondSpread;
    float aCx, aCy;
    float bCx, bCy;
    float xCx, xCy;
    float yCx, yCy;
};

static std::mutex sInputMutex;
static u16 sTopPadMask = 0;
static u16 sBottomPadMask = 0;
static u16 sBottomXYMask = 0;
static u16 sAppliedPadMask = 0;
static u16 sAppliedXYMask = 0;

static bool sPrevSwitchPressed = false;
static bool sPrevFFPressed = false;

static std::vector<TouchPt> sSecondaryTouches;
static float sSecondaryViewW = 0.0f;
static float sSecondaryViewH = 0.0f;

static float GetOverlayScale(const SIM_Config_prj_type *cfg) {
    return (cfg->touchOverlayScale > 0.4f && cfg->touchOverlayScale < 2.2f)
               ? cfg->touchOverlayScale
               : 1.0f;
}

static float GetOverlayAlpha(const SIM_Config_prj_type *cfg) {
    return (cfg->touchOverlayOpacity > 0.05f && cfg->touchOverlayOpacity <= 1.0f)
               ? cfg->touchOverlayOpacity
               : 0.65f;
}

static BottomLayout ComputeBottomLayout(float width, float height, float scale) {
    BottomLayout L = {};
    const float minDim = width < height ? width : height;
    const float unit = minDim * 0.145f * scale;
    const float marginX = width * 0.032f;
    const float midY = height * 0.52f;

    // Large D-Pad on the left side of the bottom screen
    L.dpadR = unit * 1.48f;
    L.dpadCx = marginX + L.dpadR * 1.12f;
    L.dpadCy = midY;

    // Large A/B/X/Y diamond on the right side of the bottom screen
    L.faceBtnR = unit * 0.62f;
    L.diamondSpread = unit * 1.22f;
    L.faceCx = width - marginX - L.diamondSpread - L.faceBtnR;
    L.faceCy = midY;

    L.aCx = L.faceCx + L.diamondSpread;
    L.aCy = L.faceCy;
    L.bCx = L.faceCx;
    L.bCy = L.faceCy + L.diamondSpread;
    L.xCx = L.faceCx;
    L.xCy = L.faceCy - L.diamondSpread;
    L.yCx = L.faceCx - L.diamondSpread;
    L.yCy = L.faceCy;

    return L;
}

static bool PointInCircle(float px, float py, float cx, float cy, float r) {
    const float dx = px - cx;
    const float dy = py - cy;
    return (dx * dx + dy * dy) <= (r * r);
}

static bool PointInRect(float px, float py, float x0, float y0, float x1, float y1) {
    return px >= x0 && px <= x1 && py >= y0 && py <= y1;
}

static void EvalBottomPoint(float fx, float fy, const BottomLayout &L, bool pressed[VBTN_COUNT]) {
    // D-Pad 8-way hit zone
    const float dx = fx - L.dpadCx;
    const float dy = fy - L.dpadCy;
    const float distSq = dx * dx + dy * dy;
    const float maxDpadHit = L.dpadR * 1.38f;
    const float deadzone = L.dpadR * 0.20f;
    if (distSq >= (deadzone * deadzone) && distSq <= (maxDpadHit * maxDpadHit)) {
        const float angle = std::atan2(dy, dx);
        if (angle > -2.55f && angle < -0.59f) pressed[VBTN_UP] = true;
        if (angle > 0.59f && angle < 2.55f) pressed[VBTN_DOWN] = true;
        if (angle > 2.16f || angle < -2.16f) pressed[VBTN_LEFT] = true;
        if (angle > -0.98f && angle < 0.98f) pressed[VBTN_RIGHT] = true;
    }

    // Face buttons
    const float hitR = L.faceBtnR * 1.22f;
    if (PointInCircle(fx, fy, L.aCx, L.aCy, hitR)) pressed[VBTN_A] = true;
    if (PointInCircle(fx, fy, L.bCx, L.bCy, hitR)) pressed[VBTN_B] = true;
    if (PointInCircle(fx, fy, L.xCx, L.xCy, hitR)) pressed[VBTN_X] = true;
    if (PointInCircle(fx, fy, L.yCx, L.yCy, hitR)) pressed[VBTN_Y] = true;
}

static bool IsPointOnBottomControls(float fx, float fy, const BottomLayout &L) {
    bool tmp[VBTN_COUNT] = {};
    EvalBottomPoint(fx, fy, L, tmp);
    // Also claim the center hub of the D-Pad so thumb resting doesn't tap the DS screen
    if (PointInCircle(fx, fy, L.dpadCx, L.dpadCy, L.dpadR * 1.25f)) {
        return true;
    }
    return tmp[VBTN_UP] || tmp[VBTN_DOWN] || tmp[VBTN_LEFT] || tmp[VBTN_RIGHT] ||
           tmp[VBTN_A] || tmp[VBTN_B] || tmp[VBTN_X] || tmp[VBTN_Y];
}

static void FlushCombinedHardwareMasksLocked() {
    const u16 combinedPad = sTopPadMask | sBottomPadMask;
    const u16 combinedXY = sBottomXYMask;

    const u16 releasedPad = sAppliedPadMask & ~combinedPad;
    const u16 releasedXY = sAppliedXYMask & ~combinedXY;

    s_reg_PAD_KEYINPUT = (s_reg_PAD_KEYINPUT | releasedPad) & ~combinedPad;
    *(vu16 *)HW_BUTTON_XY_BUF = (*(vu16 *)HW_BUTTON_XY_BUF | releasedXY) & ~combinedXY;

    sAppliedPadMask = combinedPad;
    sAppliedXYMask = combinedXY;
}

static void DrawCircleButton(ImDrawList *dl, ImFont *font, float cx, float cy, float r,
                             const char *label, bool pressed, float alpha, ImU32 accentCol) {
    const ImU32 bgCol = pressed
        ? IM_COL32((accentCol >> 0) & 0xFF, (accentCol >> 8) & 0xFF, (accentCol >> 16) & 0xFF, (int)(225.0f * alpha))
        : IM_COL32(24, 28, 36, (int)(170.0f * alpha));
    const ImU32 borderCol = pressed
        ? IM_COL32(255, 255, 255, (int)(245.0f * alpha))
        : IM_COL32(220, 228, 240, (int)(155.0f * alpha));
    const ImU32 textCol = IM_COL32(255, 255, 255, (int)((pressed ? 255.0f : 230.0f) * alpha));

    dl->AddCircleFilled(ImVec2(cx, cy), r, bgCol, 36);
    dl->AddCircle(ImVec2(cx, cy), r, borderCol, 36, pressed ? 2.8f : 2.0f);

    if (font) {
        const float fontSize = r * 0.86f;
        ImVec2 ts = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, label);
        dl->AddText(font, fontSize, ImVec2(cx - ts.x * 0.5f, cy - ts.y * 0.5f), textCol, label);
    }
}

static void DrawPillButton(ImDrawList *dl, ImFont *font, float x0, float y0, float x1, float y1,
                           const char *label, bool active, float alpha,
                           ImU32 activeAccent = IM_COL32(95, 165, 255, 255)) {
    const float rounding = (y1 - y0) * 0.5f;
    const ImU32 bgCol = active
        ? IM_COL32((activeAccent >> 0) & 0xFF, (activeAccent >> 8) & 0xFF, (activeAccent >> 16) & 0xFF, (int)(225.0f * alpha))
        : IM_COL32(24, 28, 36, (int)(170.0f * alpha));
    const ImU32 borderCol = active
        ? IM_COL32(255, 255, 255, (int)(245.0f * alpha))
        : IM_COL32(220, 228, 240, (int)(155.0f * alpha));
    const ImU32 textCol = IM_COL32(255, 255, 255, (int)((active ? 255.0f : 225.0f) * alpha));

    dl->AddRectFilled(ImVec2(x0, y0), ImVec2(x1, y1), bgCol, rounding);
    dl->AddRect(ImVec2(x0, y0), ImVec2(x1, y1), borderCol, rounding, 0, active ? 2.4f : 1.6f);

    if (font) {
        const float fontSize = (y1 - y0) * 0.45f;
        ImVec2 ts = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, label);
        dl->AddText(font, fontSize, ImVec2((x0 + x1 - ts.x) * 0.5f, (y0 + y1 - ts.y) * 0.5f), textCol, label);
    }
}

static void DrawDpad(ImDrawList *dl, float cx, float cy, float outerR,
                     bool up, bool down, bool left, bool right, float alpha) {
    const float armHalf = outerR * 0.34f;
    const float round = outerR * 0.14f;
    const ImU32 baseBg = IM_COL32(24, 28, 36, (int)(170.0f * alpha));
    const ImU32 borderCol = IM_COL32(220, 228, 240, (int)(155.0f * alpha));
    const ImU32 activeBg = IM_COL32(95, 165, 255, (int)(225.0f * alpha));

    dl->AddCircleFilled(ImVec2(cx, cy), outerR * 1.12f, IM_COL32(15, 18, 24, (int)(100.0f * alpha)), 40);
    dl->AddCircle(ImVec2(cx, cy), outerR * 1.12f, IM_COL32(220, 228, 240, (int)(80.0f * alpha)), 40, 1.5f);

    dl->AddRectFilled(ImVec2(cx - armHalf, cy - outerR), ImVec2(cx + armHalf, cy + outerR), baseBg, round);
    dl->AddRectFilled(ImVec2(cx - outerR, cy - armHalf), ImVec2(cx + outerR, cy + armHalf), baseBg, round);
    dl->AddRect(ImVec2(cx - armHalf, cy - outerR), ImVec2(cx + armHalf, cy + outerR), borderCol, round, 0, 1.8f);
    dl->AddRect(ImVec2(cx - outerR, cy - armHalf), ImVec2(cx + outerR, cy + armHalf), borderCol, round, 0, 1.8f);

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

} // namespace

extern "C" {

// Top screen overlay: single bottom row with [L] [SWITCH] [SELECT] on left and [START] [FF] [R] on right
BOOL TouchControls_IsEnabled(void) {
    SIM_Config_prj_type *cfg = SIM_Config_prj_GetConfig();
    return (cfg->touchOverlay && !SIM::Settings::IsOpen() && !SIM_Carts_UIActive()) ? TRUE : FALSE;
}

void TouchControls_DrawOverlay(void) {
    SIM_Config_prj_type *cfg = SIM_Config_prj_GetConfig();
    if (!cfg->touchOverlay || SIM::Settings::IsOpen() || SIM_Carts_UIActive()) {
        std::lock_guard<std::mutex> lock(sInputMutex);
        if (sTopPadMask != 0) {
            sTopPadMask = 0;
            FlushCombinedHardwareMasksLocked();
        }
        sPrevSwitchPressed = false;
        sPrevFFPressed = false;
        return;
    }

    const ImVec2 disp = ImGui::GetIO().DisplaySize;
    if (disp.x <= 1.0f || disp.y <= 1.0f) {
        return;
    }

    const float scale = GetOverlayScale(cfg);
    const float alpha = GetOverlayAlpha(cfg);

    const float marginX = disp.x * 0.016f;
    const float gap = disp.x * 0.008f;
    const float pillH = std::max(28.0f, std::min(disp.y * 0.072f * scale, disp.y * 0.14f));
    const float rowY1 = disp.y - disp.y * 0.018f;
    const float rowY0 = rowY1 - pillH;

    // Cap width so left and right 3-button groups never collide in the middle
    const float maxGroupW = disp.x * 0.43f;
    const float baseW = std::min(disp.x * 0.056f * scale, (maxGroupW - 2.0f * gap) / 3.55f);

    // Bottom-left row: [ L ] [ SWITCH ] [ SELECT ]
    const float lX0 = marginX;
    const float lX1 = lX0 + baseW;
    const float swX0 = lX1 + gap;
    const float swX1 = swX0 + baseW * 1.28f;
    const float selX0 = swX1 + gap;
    const float selX1 = selX0 + baseW * 1.25f;

    // Bottom-right row: [ START ] [ FF ] [ R ]
    const float rX1 = disp.x - marginX;
    const float rX0 = rX1 - baseW;
    const float ffX1 = rX0 - gap;
    const float ffX0 = ffX1 - baseW;
    const float staX1 = ffX0 - gap;
    const float staX0 = staX1 - baseW * 1.20f;

    bool pressed[VBTN_COUNT] = {};

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

            if (PointInRect(fx, fy, lX0, rowY0, lX1, rowY1)) pressed[VBTN_L] = true;
            if (PointInRect(fx, fy, swX0, rowY0, swX1, rowY1)) pressed[VBTN_SWITCH] = true;
            if (PointInRect(fx, fy, selX0, rowY0, selX1, rowY1)) pressed[VBTN_SELECT] = true;
            if (PointInRect(fx, fy, staX0, rowY0, staX1, rowY1)) pressed[VBTN_START] = true;
            if (PointInRect(fx, fy, ffX0, rowY0, ffX1, rowY1)) pressed[VBTN_FF] = true;
            if (PointInRect(fx, fy, rX0, rowY0, rX1, rowY1)) pressed[VBTN_R] = true;
        }
    }

    if (pressed[VBTN_SWITCH] && !sPrevSwitchPressed) {
        if (!cfg->gearEnabled) {
            cfg->gearEnabled = TRUE;
            SIM_Config_prj_SaveConfigFile(cfg);
            SIM_Companion_SetFullScreen(TRUE);
        } else {
            SIM_Companion_ToggleFullScreen();
        }
    }
    sPrevSwitchPressed = pressed[VBTN_SWITCH];

    if (pressed[VBTN_FF] && !sPrevFFPressed) {
        SIM_SetFastForward(SIM_IsFastForwarding() ? FALSE : TRUE);
    }
    sPrevFFPressed = pressed[VBTN_FF];

    u16 topPad = 0;
    if (pressed[VBTN_SELECT]) topPad |= (1u << 2);
    if (pressed[VBTN_START])  topPad |= (1u << 3);
    if (pressed[VBTN_R])      topPad |= (1u << 8);
    if (pressed[VBTN_L])      topPad |= (1u << 9);

    {
        std::lock_guard<std::mutex> lock(sInputMutex);
        sTopPadMask = topPad;
        FlushCombinedHardwareMasksLocked();
    }

    ImDrawList *dl = ImGui::GetForegroundDrawList();
    ImFont *font = SIM::UI::BoldFont();

    const bool switchActive = pressed[VBTN_SWITCH] || (SIM_Companion_IsFullScreen() != FALSE);
    const bool ffActive = pressed[VBTN_FF] || (SIM_IsFastForwarding() != FALSE);

    DrawPillButton(dl, font, lX0, rowY0, lX1, rowY1, "L", pressed[VBTN_L], alpha);
    DrawPillButton(dl, font, swX0, rowY0, swX1, rowY1, "SWITCH", switchActive, alpha, IM_COL32(70, 195, 235, 255));
    DrawPillButton(dl, font, selX0, rowY0, selX1, rowY1, "SELECT", pressed[VBTN_SELECT], alpha);

    DrawPillButton(dl, font, staX0, rowY0, staX1, rowY1, "START", pressed[VBTN_START], alpha);
    DrawPillButton(dl, font, ffX0, rowY0, ffX1, rowY1, "FF", ffActive, alpha, IM_COL32(245, 155, 50, 255));
    DrawPillButton(dl, font, rX0, rowY0, rX1, rowY1, "R", pressed[VBTN_R], alpha);
}

// Bottom screen overlay: large D-Pad on left and A/B/X/Y diamond on right
void TouchControls_DrawBottomOverlay(float width, float height) {
    SIM_Config_prj_type *cfg = SIM_Config_prj_GetConfig();
    if (!cfg->touchOverlay || SIM::Settings::IsOpen() || SIM_Carts_UIActive() ||
        SIM_Companion_IsFullScreen() || width <= 1.0f || height <= 1.0f) {
        std::lock_guard<std::mutex> lock(sInputMutex);
        if (sBottomPadMask != 0 || sBottomXYMask != 0) {
            sBottomPadMask = 0;
            sBottomXYMask = 0;
            FlushCombinedHardwareMasksLocked();
        }
        return;
    }

    const float scale = GetOverlayScale(cfg);
    const float alpha = GetOverlayAlpha(cfg);
    const BottomLayout L = ComputeBottomLayout(width, height, scale);

    bool pressed[VBTN_COUNT] = {};
    {
        std::lock_guard<std::mutex> lock(sInputMutex);
        const float sx = (sSecondaryViewW > 1.0f) ? (width / sSecondaryViewW) : 1.0f;
        const float sy = (sSecondaryViewH > 1.0f) ? (height / sSecondaryViewH) : 1.0f;
        for (const TouchPt &pt : sSecondaryTouches) {
            EvalBottomPoint(pt.x * sx, pt.y * sy, L, pressed);
        }

        u16 botPad = 0;
        if (pressed[VBTN_A])     botPad |= (1u << 0);
        if (pressed[VBTN_B])     botPad |= (1u << 1);
        if (pressed[VBTN_RIGHT]) botPad |= (1u << 4);
        if (pressed[VBTN_LEFT])  botPad |= (1u << 5);
        if (pressed[VBTN_UP])    botPad |= (1u << 6);
        if (pressed[VBTN_DOWN])  botPad |= (1u << 7);

        u16 botXY = 0;
        if (pressed[VBTN_X])     botXY |= (1u << 10);
        if (pressed[VBTN_Y])     botXY |= (1u << 11);

        sBottomPadMask = botPad;
        sBottomXYMask = botXY;
        FlushCombinedHardwareMasksLocked();
    }

    ImDrawList *dl = ImGui::GetForegroundDrawList();
    ImFont *font = ImGui::GetFont();

    DrawDpad(dl, L.dpadCx, L.dpadCy, L.dpadR,
             pressed[VBTN_UP], pressed[VBTN_DOWN], pressed[VBTN_LEFT], pressed[VBTN_RIGHT], alpha);

    DrawCircleButton(dl, font, L.aCx, L.aCy, L.faceBtnR, "A", pressed[VBTN_A], alpha, IM_COL32(235, 75, 85, 255));
    DrawCircleButton(dl, font, L.bCx, L.bCy, L.faceBtnR, "B", pressed[VBTN_B], alpha, IM_COL32(235, 185, 55, 255));
    DrawCircleButton(dl, font, L.xCx, L.xCy, L.faceBtnR, "X", pressed[VBTN_X], alpha, IM_COL32(75, 145, 240, 255));
    DrawCircleButton(dl, font, L.yCx, L.yCy, L.faceBtnR, "Y", pressed[VBTN_Y], alpha, IM_COL32(70, 200, 120, 255));
}

void TouchControls_DrawSettings(void) {
    SIM_Config_prj_type *cfg = SIM_Config_prj_GetConfig();
    bool enabled = cfg->touchOverlay != FALSE;
    if (ImGui::Checkbox("Enable on-screen touch controller", &enabled)) {
        cfg->touchOverlay = enabled ? TRUE : FALSE;
        SIM_Config_prj_SaveConfigFile(cfg);
    }
    ImGui::TextWrapped("Split multi-touch controller: D-Pad & A/B/X/Y on the lower screen, L/R, Start/Select, Switch & FF along the upper screen's bottom edge.");

    ImGui::Spacing();
    if (ImGui::SliderFloat("Button opacity", &cfg->touchOverlayOpacity, 0.15f, 1.0f, "%.2f")) {
        SIM_Config_prj_SaveConfigFile(cfg);
    }
    if (ImGui::SliderFloat("Button scale", &cfg->touchOverlayScale, 0.65f, 1.50f, "%.2f")) {
        SIM_Config_prj_SaveConfigFile(cfg);
    }
}

#ifdef __ANDROID__
JNIEXPORT jboolean JNICALL
Java_org_pokeplatinum_port_SecondaryScreen_nativeUpdateSecondaryTouches(
    JNIEnv *env, jclass, jint count, jfloatArray xsArr, jfloatArray ysArr,
    jfloat viewWidth, jfloat viewHeight) {
    std::lock_guard<std::mutex> lock(sInputMutex);
    sSecondaryTouches.clear();
    sSecondaryViewW = viewWidth;
    sSecondaryViewH = viewHeight;

    if (count <= 0 || xsArr == nullptr || ysArr == nullptr || viewWidth <= 1.0f || viewHeight <= 1.0f) {
        return JNI_FALSE;
    }

    SIM_Config_prj_type *cfg = SIM_Config_prj_GetConfig();
    const bool overlayActive = cfg->touchOverlay && !SIM::Settings::IsOpen() && !SIM_Carts_UIActive() && !SIM_Companion_IsFullScreen();
    const BottomLayout L = ComputeBottomLayout(viewWidth, viewHeight, GetOverlayScale(cfg));

    jfloat *xs = env->GetFloatArrayElements(xsArr, nullptr);
    jfloat *ys = env->GetFloatArrayElements(ysArr, nullptr);

    bool anyHit = false;
    for (int i = 0; i < count; ++i) {
        sSecondaryTouches.push_back({xs[i], ys[i]});
        if (overlayActive && IsPointOnBottomControls(xs[i], ys[i], L)) {
            anyHit = true;
        }
    }

    env->ReleaseFloatArrayElements(xsArr, xs, JNI_ABORT);
    env->ReleaseFloatArrayElements(ysArr, ys, JNI_ABORT);
    return anyHit ? JNI_TRUE : JNI_FALSE;
}
#endif

} // extern "C"
