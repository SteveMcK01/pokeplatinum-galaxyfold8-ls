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
    VBTN_PIP,
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
static u16 sTopXYMask = 0;
static u16 sBottomPadMask = 0;
static u16 sBottomXYMask = 0;
static u16 sAppliedPadMask = 0;
static u16 sAppliedXYMask = 0;

static bool sPrevSwitchPressed = false;
static bool sPrevPipPressed = false;
static bool sPrevCompanionSwitchPressed = false;
static bool sPrevFFPressed = false;
static int sLastAndroidLayoutMode = -1;
static float sLastAndroidOverlayScale = -1.0f;

static std::vector<TouchPt> sSecondaryTouches;
static float sSecondaryViewW = 0.0f;
static float sSecondaryViewH = 0.0f;

struct PillRect {
    float x0, y0, x1, y1;
};

static PillRect ComputeCompanionSwitchRect(float width, float height, float scale) {
    const float pillH = std::max(34.0f, std::min(height * 0.052f * scale, height * 0.09f));
    const float pillW = std::max(108.0f, std::min(width * 0.16f * scale, width * 0.34f));
    const float cx = width * 0.5f;
    const float y1 = height - height * 0.015f;
    return {cx - pillW * 0.5f, y1 - pillH, cx + pillW * 0.5f, y1};
}

static void SyncAndroidLayoutMode(int mode) {
#ifdef __ANDROID__
    if (sLastAndroidLayoutMode == mode) {
        return;
    }
    JNIEnv *env = static_cast<JNIEnv *>(SDL_AndroidGetJNIEnv());
    if (!env) {
        return;
    }
    jclass cls = env->FindClass("org/pokeplatinum/port/PlatinumActivity");
    if (!cls) {
        env->ExceptionClear();
        return;
    }
    jmethodID mid = env->GetStaticMethodID(cls, "setInlineLayoutMode", "(I)V");
    if (mid) {
        env->CallStaticVoidMethod(cls, mid, static_cast<jint>(mode));
        sLastAndroidLayoutMode = mode;
    }
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
    }
    env->DeleteLocalRef(cls);
#else
    (void)mode;
#endif
}

static void SyncAndroidOverlayScale(float widthFraction) {
#ifdef __ANDROID__
    if (std::fabs(sLastAndroidOverlayScale - widthFraction) < 0.005f) {
        return;
    }
    JNIEnv *env = static_cast<JNIEnv *>(SDL_AndroidGetJNIEnv());
    if (!env) {
        return;
    }
    jclass cls = env->FindClass("org/pokeplatinum/port/PlatinumActivity");
    if (!cls) {
        env->ExceptionClear();
        return;
    }
    jmethodID mid = env->GetStaticMethodID(cls, "setInlineOverlayWidthFraction", "(F)V");
    if (mid) {
        env->CallStaticVoidMethod(cls, mid, static_cast<jfloat>(widthFraction));
        sLastAndroidOverlayScale = widthFraction;
    }
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
    }
    env->DeleteLocalRef(cls);
#else
    (void)widthFraction;
#endif
}

static float GetOverlayScale(const SIM_Config_prj_type *cfg) {
    return (cfg->touchOverlayScale > 0.4f && cfg->touchOverlayScale < 2.2f)
               ? cfg->touchOverlayScale
               : 1.0f;
}

static float GetTopLeftWidthFraction(const SIM_Config_prj_type *cfg) {
    return (cfg->topRightOverlayScale >= 0.22f && cfg->topRightOverlayScale <= 0.55f)
               ? cfg->topRightOverlayScale
               : 0.36f;
}

static float GetOverlayAlpha(const SIM_Config_prj_type *cfg) {
    return (cfg->touchOverlayOpacity > 0.05f && cfg->touchOverlayOpacity <= 1.0f)
               ? cfg->touchOverlayOpacity
               : 0.65f;
}

static BottomLayout ComputeBottomLayout(float width, float height, float scale, float centerYRatio = 0.52f) {
    BottomLayout L = {};
    const float minDim = width < height ? width : height;
    const float unit = minDim * 0.145f * scale;
    const float marginX = width * 0.032f;
    const float midY = height * centerYRatio;

    L.dpadR = unit * 1.48f;
    L.dpadCx = marginX + L.dpadR * 1.12f;
    L.dpadCy = midY;

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

    const float hitR = L.faceBtnR * 1.22f;
    if (PointInCircle(fx, fy, L.aCx, L.aCy, hitR)) pressed[VBTN_A] = true;
    if (PointInCircle(fx, fy, L.bCx, L.bCy, hitR)) pressed[VBTN_B] = true;
    if (PointInCircle(fx, fy, L.xCx, L.xCy, hitR)) pressed[VBTN_X] = true;
    if (PointInCircle(fx, fy, L.yCx, L.yCy, hitR)) pressed[VBTN_Y] = true;
}

static bool IsPointOnBottomControls(float fx, float fy, const BottomLayout &L) {
    bool tmp[VBTN_COUNT] = {};
    EvalBottomPoint(fx, fy, L, tmp);
    if (PointInCircle(fx, fy, L.dpadCx, L.dpadCy, L.dpadR * 1.25f)) {
        return true;
    }
    return tmp[VBTN_UP] || tmp[VBTN_DOWN] || tmp[VBTN_LEFT] || tmp[VBTN_RIGHT] ||
           tmp[VBTN_A] || tmp[VBTN_B] || tmp[VBTN_X] || tmp[VBTN_Y];
}

static void FlushCombinedHardwareMasksLocked() {
    const u16 combinedPad = sTopPadMask | sBottomPadMask;
    const u16 combinedXY = sTopXYMask | sBottomXYMask;

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
        const float fontSize = (y1 - y0) * 0.44f;
        ImVec2 ts = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, label);
        dl->AddText(font, fontSize, ImVec2((x0 + x1 - ts.x) * 0.5f, (y0 + y1 - ts.y) * 0.5f), textCol, label);
    }
}

static void DrawTogglePillButton(ImDrawList *dl, ImFont *font, float x0, float y0, float x1, float y1,
                                 const char *label, bool toggledOn, float alpha, ImU32 activeAccent) {
    const float h = y1 - y0;
    const float rounding = h * 0.5f;
    const float effAlpha = toggledOn ? std::max(alpha, 0.88f) : alpha;

    if (toggledOn) {
        const ImU32 glowCol = IM_COL32(
            (activeAccent >> 0) & 0xFF,
            (activeAccent >> 8) & 0xFF,
            (activeAccent >> 16) & 0xFF,
            (int)(110.0f * effAlpha));
        dl->AddRect(ImVec2(x0 - 2.5f, y0 - 2.5f), ImVec2(x1 + 2.5f, y1 + 2.5f), glowCol, rounding + 2.5f, 0, 3.0f);
    }

    const ImU32 bgCol = toggledOn
        ? IM_COL32((activeAccent >> 0) & 0xFF, (activeAccent >> 8) & 0xFF, (activeAccent >> 16) & 0xFF, (int)(235.0f * effAlpha))
        : IM_COL32(24, 28, 36, (int)(170.0f * effAlpha));
    const ImU32 borderCol = toggledOn
        ? IM_COL32(255, 255, 255, (int)(250.0f * effAlpha))
        : IM_COL32(220, 228, 240, (int)(155.0f * effAlpha));
    const ImU32 textCol = toggledOn
        ? IM_COL32(20, 22, 28, (int)(255.0f * effAlpha))
        : IM_COL32(235, 240, 248, (int)(225.0f * effAlpha));

    dl->AddRectFilled(ImVec2(x0, y0), ImVec2(x1, y1), bgCol, rounding);
    dl->AddRect(ImVec2(x0, y0), ImVec2(x1, y1), borderCol, rounding, 0, toggledOn ? 2.5f : 1.6f);

    const float dotR = std::max(3.0f, h * 0.13f);
    const float dotCx = x0 + rounding * 0.72f + dotR * 0.5f;
    const float dotCy = (y0 + y1) * 0.5f;
    if (toggledOn) {
        dl->AddCircleFilled(ImVec2(dotCx, dotCy), dotR, IM_COL32(20, 22, 28, (int)(245.0f * effAlpha)), 16);
    } else {
        dl->AddCircle(ImVec2(dotCx, dotCy), dotR, IM_COL32(200, 210, 225, (int)(160.0f * effAlpha)), 16, 1.5f);
    }

    if (font) {
        const float fontSize = h * 0.43f;
        ImVec2 ts = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, label);
        const float textAreaLeft = dotCx + dotR + 4.0f;
        const float textAreaRight = x1 - rounding * 0.45f;
        const float tx = textAreaLeft + std::max(0.0f, (textAreaRight - textAreaLeft - ts.x) * 0.5f);
        dl->AddText(font, fontSize, ImVec2(tx, (y0 + y1 - ts.y) * 0.5f), textCol, label);
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

BOOL TouchControls_IsEnabled(void) {
    SIM_Config_prj_type *cfg = SIM_Config_prj_GetConfig();
    return (cfg->touchOverlay && !SIM::Settings::IsOpen() && !SIM_Carts_UIActive()) ? TRUE : FALSE;
}

void TouchControls_DrawOverlay(void) {
    SIM_Config_prj_type *cfg = SIM_Config_prj_GetConfig();
    const bool allowFullExpand = (cfg->fullScreenCompanion != FALSE);
    const bool primaryFull = allowFullExpand && (SIM::Settings::IsOpen() || SIM_Carts_UIActive());
    const bool companionFull = allowFullExpand && !primaryFull && (SIM_Companion_IsFullScreen() != FALSE);
    const bool pipOverlayMode = !primaryFull && !companionFull && (cfg->topRightOverlay != FALSE);
    const bool pipMinimized = pipOverlayMode && (cfg->topRightOverlayMinimized != FALSE);

    SyncAndroidOverlayScale(GetTopLeftWidthFraction(cfg));
    SyncAndroidLayoutMode(primaryFull ? 2 : (companionFull ? 1 : (pipOverlayMode ? (pipMinimized ? 4 : 3) : 0)));

    if (!cfg->touchOverlay || primaryFull || companionFull) {
        std::lock_guard<std::mutex> lock(sInputMutex);
        if (sTopPadMask != 0 || sTopXYMask != 0) {
            sTopPadMask = 0;
            sTopXYMask = 0;
            FlushCombinedHardwareMasksLocked();
        }
        sPrevSwitchPressed = false;
        sPrevPipPressed = false;
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
    const float gap = disp.x * 0.006f;
    const float pillH = std::max(28.0f, std::min(disp.y * (pipOverlayMode ? 0.048f : 0.072f) * scale, disp.y * 0.14f));
    const float rowY1 = disp.y - disp.y * 0.018f;
    const float rowY0 = rowY1 - pillH;

    float lX0, lX1, pipX0 = 0.0f, pipX1 = 0.0f, swX0, swX1, selX0, selX1;
    float staX0, staX1, ffX0, ffX1, rX0, rX1;

    if (pipOverlayMode) {
        const float maxCenterSpan = disp.x * 0.52f;
        const float totalMult = 7.98f; // L(1.0) + HIDE(1.25) + SWITCH(1.28) + SELECT(1.25) + START(1.20) + FF(1.0) + R(1.0)
        const float baseW = std::min(disp.x * 0.044f * scale, (maxCenterSpan - 6.0f * gap) / totalMult);
        const float totalBarW = baseW * totalMult + gap * 6.0f;
        const float startX = (disp.x - totalBarW) * 0.5f;
        lX0 = startX;
        lX1 = lX0 + baseW;
        pipX0 = lX1 + gap;
        pipX1 = pipX0 + baseW * 1.25f;
        swX0 = pipX1 + gap;
        swX1 = swX0 + baseW * 1.28f;
        selX0 = swX1 + gap;
        selX1 = selX0 + baseW * 1.25f;
        staX0 = selX1 + gap;
        staX1 = staX0 + baseW * 1.20f;
        ffX0 = staX1 + gap;
        ffX1 = ffX0 + baseW;
        rX0 = ffX1 + gap;
        rX1 = rX0 + baseW;
    } else {
        const float maxGroupW = disp.x * 0.43f;
        const float baseW = std::min(disp.x * 0.056f * scale, (maxGroupW - 2.0f * gap) / 3.55f);

        lX0 = marginX;
        lX1 = lX0 + baseW;
        swX0 = lX1 + gap;
        swX1 = swX0 + baseW * 1.28f;
        selX0 = swX1 + gap;
        selX1 = selX0 + baseW * 1.25f;

        rX1 = disp.x - marginX;
        rX0 = rX1 - baseW;
        ffX1 = rX0 - gap;
        ffX0 = ffX1 - baseW;
        staX1 = ffX0 - gap;
        staX0 = staX1 - baseW * 1.20f;
    }

    const BottomLayout L = ComputeBottomLayout(disp.x, disp.y, scale * 0.88f, 0.69f);

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
            if (pipOverlayMode && PointInRect(fx, fy, pipX0, rowY0, pipX1, rowY1)) pressed[VBTN_PIP] = true;
            if (PointInRect(fx, fy, swX0, rowY0, swX1, rowY1)) pressed[VBTN_SWITCH] = true;
            if (PointInRect(fx, fy, selX0, rowY0, selX1, rowY1)) pressed[VBTN_SELECT] = true;
            if (PointInRect(fx, fy, staX0, rowY0, staX1, rowY1)) pressed[VBTN_START] = true;
            if (PointInRect(fx, fy, ffX0, rowY0, ffX1, rowY1)) pressed[VBTN_FF] = true;
            if (PointInRect(fx, fy, rX0, rowY0, rX1, rowY1)) pressed[VBTN_R] = true;

            if (pipOverlayMode) {
                EvalBottomPoint(fx, fy, L, pressed);
            }
        }
    }

    if (pipOverlayMode && pressed[VBTN_PIP] && !sPrevPipPressed) {
        cfg->topRightOverlayMinimized = (cfg->topRightOverlayMinimized != FALSE) ? FALSE : TRUE;
        SIM_Config_prj_SaveConfigFile(cfg);
    }
    sPrevPipPressed = pipOverlayMode && pressed[VBTN_PIP];

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
    u16 topXY = 0;
    if (pressed[VBTN_SELECT]) topPad |= (1u << 2);
    if (pressed[VBTN_START])  topPad |= (1u << 3);
    if (pressed[VBTN_R])      topPad |= (1u << 8);
    if (pressed[VBTN_L])      topPad |= (1u << 9);
    if (pipOverlayMode) {
        if (pressed[VBTN_A])     topPad |= (1u << 0);
        if (pressed[VBTN_B])     topPad |= (1u << 1);
        if (pressed[VBTN_RIGHT]) topPad |= (1u << 4);
        if (pressed[VBTN_LEFT])  topPad |= (1u << 5);
        if (pressed[VBTN_UP])    topPad |= (1u << 6);
        if (pressed[VBTN_DOWN])  topPad |= (1u << 7);
        if (pressed[VBTN_X])     topXY  |= (1u << 10);
        if (pressed[VBTN_Y])     topXY  |= (1u << 11);
    }

    {
        std::lock_guard<std::mutex> lock(sInputMutex);
        sTopPadMask = topPad;
        sTopXYMask = topXY;
        FlushCombinedHardwareMasksLocked();
    }

    ImDrawList *dl = ImGui::GetForegroundDrawList();
    ImFont *boldFont = SIM::UI::BoldFont();

    const bool screenHidden = pipOverlayMode && (cfg->topRightOverlayMinimized != FALSE);
    const bool switchActive = pressed[VBTN_SWITCH] || (SIM_Companion_IsFullScreen() != FALSE);
    const bool ffActive = pressed[VBTN_FF] || (SIM_IsFastForwarding() != FALSE);

    DrawPillButton(dl, boldFont, lX0, rowY0, lX1, rowY1, "L", pressed[VBTN_L], alpha);
    if (pipOverlayMode) {
        DrawTogglePillButton(dl, boldFont, pipX0, rowY0, pipX1, rowY1, "HIDE", screenHidden, alpha, IM_COL32(245, 185, 55, 255));
    }
    DrawPillButton(dl, boldFont, swX0, rowY0, swX1, rowY1, "SWITCH", switchActive, alpha, IM_COL32(70, 195, 235, 255));
    DrawPillButton(dl, boldFont, selX0, rowY0, selX1, rowY1, "SELECT", pressed[VBTN_SELECT], alpha);

    DrawPillButton(dl, boldFont, staX0, rowY0, staX1, rowY1, "START", pressed[VBTN_START], alpha);
    DrawPillButton(dl, boldFont, ffX0, rowY0, ffX1, rowY1, "FF", ffActive, alpha, IM_COL32(245, 155, 50, 255));
    DrawPillButton(dl, boldFont, rX0, rowY0, rX1, rowY1, "R", pressed[VBTN_R], alpha);

    if (pipOverlayMode) {
        ImFont *font = ImGui::GetFont();
        DrawDpad(dl, L.dpadCx, L.dpadCy, L.dpadR,
                 pressed[VBTN_UP], pressed[VBTN_DOWN], pressed[VBTN_LEFT], pressed[VBTN_RIGHT], alpha);
        DrawCircleButton(dl, font, L.aCx, L.aCy, L.faceBtnR, "A", pressed[VBTN_A], alpha, IM_COL32(235, 75, 85, 255));
        DrawCircleButton(dl, font, L.bCx, L.bCy, L.faceBtnR, "B", pressed[VBTN_B], alpha, IM_COL32(235, 185, 55, 255));
        DrawCircleButton(dl, font, L.xCx, L.xCy, L.faceBtnR, "X", pressed[VBTN_X], alpha, IM_COL32(75, 145, 240, 255));
        DrawCircleButton(dl, font, L.yCx, L.yCy, L.faceBtnR, "Y", pressed[VBTN_Y], alpha, IM_COL32(70, 200, 120, 255));
    }
}

void TouchControls_DrawBottomOverlay(float width, float height) {
    SIM_Config_prj_type *cfg = SIM_Config_prj_GetConfig();
    const bool allowFullExpand = (cfg->fullScreenCompanion != FALSE);
    const bool primaryFull = allowFullExpand && (SIM::Settings::IsOpen() || SIM_Carts_UIActive());
    const bool companionFull = allowFullExpand && !primaryFull && (SIM_Companion_IsFullScreen() != FALSE);
    const bool pipOverlayMode = !primaryFull && !companionFull && (cfg->topRightOverlay != FALSE);

    if (primaryFull || pipOverlayMode || width <= 1.0f || height <= 1.0f || (!companionFull && !cfg->touchOverlay)) {
        std::lock_guard<std::mutex> lock(sInputMutex);
        if (sBottomPadMask != 0 || sBottomXYMask != 0) {
            sBottomPadMask = 0;
            sBottomXYMask = 0;
            FlushCombinedHardwareMasksLocked();
        }
        sPrevCompanionSwitchPressed = false;
        return;
    }

    const float scale = GetOverlayScale(cfg);
    const float alpha = cfg->touchOverlay ? GetOverlayAlpha(cfg) : 0.75f;

    if (companionFull) {
        bool switchPressed = false;
        const PillRect sw = ComputeCompanionSwitchRect(width, height, scale);
        {
            std::lock_guard<std::mutex> lock(sInputMutex);
            if (sBottomPadMask != 0 || sBottomXYMask != 0) {
                sBottomPadMask = 0;
                sBottomXYMask = 0;
                FlushCombinedHardwareMasksLocked();
            }
            const float sx = (sSecondaryViewW > 1.0f) ? (width / sSecondaryViewW) : 1.0f;
            const float sy = (sSecondaryViewH > 1.0f) ? (height / sSecondaryViewH) : 1.0f;
            for (const TouchPt &pt : sSecondaryTouches) {
                if (PointInRect(pt.x * sx, pt.y * sy, sw.x0, sw.y0, sw.x1, sw.y1)) {
                    switchPressed = true;
                }
            }
        }

        if (switchPressed && !sPrevCompanionSwitchPressed) {
            SIM_Companion_ToggleFullScreen();
        }
        sPrevCompanionSwitchPressed = switchPressed;

        ImDrawList *dl = ImGui::GetForegroundDrawList();
        ImFont *font = SIM::UI::BoldFont();
        DrawPillButton(dl, font, sw.x0, sw.y0, sw.x1, sw.y1, "SWITCH", true, alpha, IM_COL32(70, 195, 235, 255));
        return;
    }
    sPrevCompanionSwitchPressed = false;

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

void TouchControls_DrawDisplaySettings(void) {
    SIM_Config_prj_type *cfg = SIM_Config_prj_GetConfig();
    SIM::UI::Section("Screen layout");

    bool topLeft = cfg->topRightOverlay != FALSE;
    if (SIM::UI::Toggle(
            "Top-left interactive lower screen overlay",
            "Show the lower DS screen as an interactive 4:3 overlay in the top-left corner while the top screen fills the display.",
            &topLeft)) {
        cfg->topRightOverlay = topLeft ? TRUE : FALSE;
        SIM_Config_prj_SaveConfigFile(cfg);
    }

    bool minimized = cfg->topRightOverlayMinimized != FALSE;
    if (SIM::UI::Toggle(
            "Hide top-left lower screen overlay",
            "Minimize the top-left lower screen overlay until toggled back on with the on-screen [ HIDE ] button.",
            &minimized, topLeft)) {
        cfg->topRightOverlayMinimized = minimized ? TRUE : FALSE;
        SIM_Config_prj_SaveConfigFile(cfg);
    }

    int widthPct = (int)(GetTopLeftWidthFraction(cfg) * 100.0f + 0.5f);
    if (widthPct < 24) widthPct = 24;
    if (widthPct > 52) widthPct = 52;
    if (SIM::UI::Stepper(
            "Top-left overlay width",
            "Width of the corner lower-screen overlay as a percentage of the display.",
            &widthPct, 24, 52, 2, "%d%%", topLeft)) {
        cfg->topRightOverlayScale = (float)widthPct / 100.0f;
        SIM_Config_prj_SaveConfigFile(cfg);
    }

    bool fullExpand = cfg->fullScreenCompanion != FALSE;
    if (SIM::UI::Toggle(
            "Full-screen Gear companion & Settings",
            "Expand the Gear companion and Settings menu to fill the whole screen with a bottom [ SWITCH ] pill.",
            &fullExpand)) {
        cfg->fullScreenCompanion = fullExpand ? TRUE : FALSE;
        SIM_Config_prj_SaveConfigFile(cfg);
    }
}

void TouchControls_DrawSettings(void) {
    SIM_Config_prj_type *cfg = SIM_Config_prj_GetConfig();
    SIM::UI::Section("On-screen controller");

    bool enabled = cfg->touchOverlay != FALSE;
    if (SIM::UI::Toggle(
            "Enable on-screen touch controller",
            "Multi-touch controller: D-Pad & A/B/X/Y plus L/R, Start/Select, Switch & FF.",
            &enabled)) {
        cfg->touchOverlay = enabled ? TRUE : FALSE;
        SIM_Config_prj_SaveConfigFile(cfg);
    }

    int opacityPct = (int)(cfg->touchOverlayOpacity * 100.0f + 0.5f);
    if (opacityPct < 15) opacityPct = 15;
    if (opacityPct > 100) opacityPct = 100;
    if (SIM::UI::Stepper(
            "Button opacity",
            "Transparency of the virtual buttons over the lower display.",
            &opacityPct, 15, 100, 5, "%d%%", enabled)) {
        cfg->touchOverlayOpacity = (float)opacityPct / 100.0f;
        SIM_Config_prj_SaveConfigFile(cfg);
    }

    int scalePct = (int)(cfg->touchOverlayScale * 100.0f + 0.5f);
    if (scalePct < 65) scalePct = 65;
    if (scalePct > 150) scalePct = 150;
    if (SIM::UI::Stepper(
            "Button scale",
            "Overall size of the D-Pad, face buttons, and shoulder triggers.",
            &scalePct, 65, 150, 5, "%d%%", enabled)) {
        cfg->touchOverlayScale = (float)scalePct / 100.0f;
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
    const bool allowFullExpand = (cfg->fullScreenCompanion != FALSE);
    const bool primaryFull = allowFullExpand && (SIM::Settings::IsOpen() || SIM_Carts_UIActive());
    const bool companionFull = allowFullExpand && !primaryFull && (SIM_Companion_IsFullScreen() != FALSE);
    const bool pipOverlayMode = !primaryFull && !companionFull && (cfg->topRightOverlay != FALSE);
    const bool bottomControlsActive = cfg->touchOverlay && !primaryFull && !companionFull && !pipOverlayMode;
    const float scale = GetOverlayScale(cfg);
    const BottomLayout L = ComputeBottomLayout(viewWidth, viewHeight, scale);
    const PillRect sw = ComputeCompanionSwitchRect(viewWidth, viewHeight, scale);

    jfloat *xs = env->GetFloatArrayElements(xsArr, nullptr);
    jfloat *ys = env->GetFloatArrayElements(ysArr, nullptr);

    bool anyHit = false;
    for (int i = 0; i < count; ++i) {
        sSecondaryTouches.push_back({xs[i], ys[i]});
        if (companionFull) {
            if (PointInRect(xs[i], ys[i], sw.x0, sw.y0, sw.x1, sw.y1)) {
                anyHit = true;
            }
        } else if (bottomControlsActive && IsPointOnBottomControls(xs[i], ys[i], L)) {
            anyHit = true;
        }
    }

    env->ReleaseFloatArrayElements(xsArr, xs, JNI_ABORT);
    env->ReleaseFloatArrayElements(ysArr, ys, JNI_ABORT);
    return anyHit ? JNI_TRUE : JNI_FALSE;
}
#endif

} // extern "C"
