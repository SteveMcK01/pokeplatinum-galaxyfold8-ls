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
#ifdef GEN5_BATTLES
#include "port/gen5_battle.h"
#endif

#include "generated/pokemon_types.h"
#include "constants/battle.h"
#include "constants/moves.h"
#include "generated/abilities.h"
#include "port/gear_data.h"
#include "port/gear_sprites.h"

struct BattleOverlayBallEntry {
    u16 item;
    u16 count;
    char name[32];
};

extern "C" {
struct BattleSystem *Gear_ActiveBattleSystem(void);
BOOL GearSprites_ItemIcon(u16 item, u32 *rgba);
int Gear_BattleWeather(void);
int Gear_GetBattlePokeBalls(BattleOverlayBallEntry *out, int maxOut, BOOL *canThrowOut);
BOOL Gear_UseBattlePokeBall(u16 item);
}

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
static int sLastAndroidBattleMode = -1;
static int sLastAndroidPipAutoFade = -1;

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

static bool IsBattleScreenActive(void) {
    if (Gear_ActiveBattleSystem() != NULL) {
        return true;
    }
#ifdef GEN5_BATTLES
    if (Gen5Battle_IsShown() != FALSE) {
        return true;
    }
#endif
    return false;
}

static void SyncAndroidBattleMode(bool inBattle) {
#ifdef __ANDROID__
    const int val = inBattle ? 1 : 0;
    if (sLastAndroidBattleMode == val) {
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
    jmethodID mid = env->GetStaticMethodID(cls, "setInlineBattleMode", "(Z)V");
    if (mid) {
        env->CallStaticVoidMethod(cls, mid, inBattle ? JNI_TRUE : JNI_FALSE);
        sLastAndroidBattleMode = val;
    }
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
    }
    env->DeleteLocalRef(cls);
#else
    (void)inBattle;
#endif
}

static void SyncAndroidPipAutoFade(bool enabled) {
#ifdef __ANDROID__
    const int val = enabled ? 1 : 0;
    if (sLastAndroidPipAutoFade == val) {
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
    jmethodID mid = env->GetStaticMethodID(cls, "setInlinePipAutoFade", "(Z)V");
    if (mid) {
        env->CallStaticVoidMethod(cls, mid, enabled ? JNI_TRUE : JNI_FALSE);
        sLastAndroidPipAutoFade = val;
    }
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
    }
    env->DeleteLocalRef(cls);
#else
    (void)enabled;
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

static void EvalBottomPoint(float fx, float fy, const BottomLayout &L, bool pressed[VBTN_COUNT], bool includeFaceButtons = true) {
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

    if (includeFaceButtons) {
        const float hitR = L.faceBtnR * 1.22f;
        if (PointInCircle(fx, fy, L.aCx, L.aCy, hitR)) pressed[VBTN_A] = true;
        if (PointInCircle(fx, fy, L.bCx, L.bCy, hitR)) pressed[VBTN_B] = true;
        if (PointInCircle(fx, fy, L.xCx, L.xCy, hitR)) pressed[VBTN_X] = true;
        if (PointInCircle(fx, fy, L.yCx, L.yCy, hitR)) pressed[VBTN_Y] = true;
    }
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

enum class PillIcon { StartTri, SelectWin, FastFwd, SwitchArr, EyeOpen, EyeClosed };
static void DrawIconPill(ImDrawList *dl, float x0, float y0, float x1, float y1, PillIcon ic, bool act, float alpha, ImU32 acc = IM_COL32(95,165,255,255), bool darkInk = false) {
    float r = (y1 - y0) * 0.5f, cx = (x0 + x1) * 0.5f, cy = (y0 + y1) * 0.5f, h = y1 - y0;
    float s = std::min((x1 - x0) * 0.28f, h * 0.28f), st = std::max(1.8f, h * 0.065f);
    ImU32 bg = act ? IM_COL32((acc>>0)&255,(acc>>8)&255,(acc>>16)&255,(int)(225*alpha)) : IM_COL32(24,28,36,(int)(170*alpha));
    ImU32 bd = act ? IM_COL32(255,255,255,(int)(245*alpha)) : IM_COL32(220,228,240,(int)(155*alpha));
    ImU32 ink = (act && darkInk) ? IM_COL32(20,22,28,(int)(250*alpha)) : IM_COL32(255,255,255,(int)((act?255:235)*alpha));
    dl->AddRectFilled(ImVec2(x0,y0), ImVec2(x1,y1), bg, r);
    dl->AddRect(ImVec2(x0,y0), ImVec2(x1,y1), bd, r, 0, act ? 2.4f : 1.6f);
    if (ic == PillIcon::StartTri) {
        dl->AddTriangleFilled(ImVec2(cx-s*0.6f,cy-s*0.8f), ImVec2(cx+s*0.9f,cy), ImVec2(cx-s*0.6f,cy+s*0.8f), ink);
    } else if (ic == PillIcon::SelectWin) {
        dl->AddRect(ImVec2(cx-s*1.05f,cy-s*0.95f), ImVec2(cx+s*0.45f,cy+s*0.45f), ink, 2.0f, 0, st);
        dl->AddRectFilled(ImVec2(cx-s*0.45f,cy-s*0.45f), ImVec2(cx+s*1.05f,cy+s*0.95f), IM_COL32(24,28,36,(int)(225*alpha)), 2.0f);
        dl->AddRect(ImVec2(cx-s*0.45f,cy-s*0.45f), ImVec2(cx+s*1.05f,cy+s*0.95f), ink, 2.0f, 0, st);
    } else if (ic == PillIcon::FastFwd) {
        float tw = s*0.82f, th = s*0.78f;
        dl->AddTriangleFilled(ImVec2(cx-tw*1.15f,cy-th), ImVec2(cx-tw*0.05f,cy), ImVec2(cx-tw*1.15f,cy+th), ink);
        dl->AddTriangleFilled(ImVec2(cx+tw*0.05f,cy-th), ImVec2(cx+tw*1.15f,cy), ImVec2(cx+tw*0.05f,cy+th), ink);
    } else if (ic == PillIcon::SwitchArr) {
        float hw = s*1.1f, yt = cy-s*0.44f, yb = cy+s*0.44f, hd = s*0.42f;
        dl->AddLine(ImVec2(cx-hw,yt), ImVec2(cx+hw-hd*0.5f,yt), ink, st);
        dl->AddTriangleFilled(ImVec2(cx+hw,yt), ImVec2(cx+hw-hd,yt-hd*0.72f), ImVec2(cx+hw-hd,yt+hd*0.72f), ink);
        dl->AddLine(ImVec2(cx+hw,yb), ImVec2(cx-hw+hd*0.5f,yb), ink, st);
        dl->AddTriangleFilled(ImVec2(cx-hw,yb), ImVec2(cx-hw+hd,yb-hd*0.72f), ImVec2(cx-hw+hd,yb+hd*0.72f), ink);
    } else if (ic == PillIcon::EyeOpen) {
        float ew = s*1.35f, eh = s*0.82f;
        dl->AddBezierCubic(ImVec2(cx-ew,cy), ImVec2(cx-ew*0.45f,cy-eh), ImVec2(cx+ew*0.45f,cy-eh), ImVec2(cx+ew,cy), ink, st);
        dl->AddBezierCubic(ImVec2(cx-ew,cy), ImVec2(cx-ew*0.45f,cy+eh), ImVec2(cx+ew*0.45f,cy+eh), ImVec2(cx+ew,cy), ink, st);
        dl->AddCircleFilled(ImVec2(cx,cy), s*0.38f, ink, 16);
    } else if (ic == PillIcon::EyeClosed) {
        float ew = s*1.35f, eh = s*0.72f;
        dl->AddBezierCubic(ImVec2(cx-ew,cy-eh*0.15f), ImVec2(cx-ew*0.45f,cy+eh*0.75f), ImVec2(cx+ew*0.45f,cy+eh*0.75f), ImVec2(cx+ew,cy-eh*0.15f), ink, st);
        dl->AddLine(ImVec2(cx-ew*0.85f,cy+eh*0.85f), ImVec2(cx+ew*0.85f,cy-eh*0.85f), ink, st);
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


// --- Full-Screen Battle Overlay & Poke Ball Carousel ---

struct BallTexCacheEntry {
    u16 item = 0;
    unsigned int texId = 0;
    bool tried = false;
};

static std::vector<BallTexCacheEntry> sBallTexCache;
static int sSelectedBallIdx = 0;
static bool sBattleHudTouchActive = false;

static unsigned int GetBallTexture(u16 item) {
    if (item == 0) {
        return 0;
    }
    for (auto &e : sBallTexCache) {
        if (e.item == item) {
            return e.texId;
        }
    }
    BallTexCacheEntry entry;
    entry.item = item;
    entry.tried = true;
    u32 rgba[24 * 24];
    if (GearSprites_ItemIcon(item, rgba)) {
        entry.texId = SIM_UI_CreateImage(24, 24, rgba);
    }
    sBallTexCache.push_back(entry);
    return entry.texId;
}

static const char *GetTypeName(u8 type) {
    switch (type) {
    case TYPE_NORMAL:   return "NORMAL";
    case TYPE_FIGHTING: return "FIGHT";
    case TYPE_FLYING:   return "FLYING";
    case TYPE_POISON:   return "POISON";
    case TYPE_GROUND:   return "GROUND";
    case TYPE_ROCK:     return "ROCK";
    case TYPE_BUG:      return "BUG";
    case TYPE_GHOST:    return "GHOST";
    case TYPE_STEEL:    return "STEEL";
    case TYPE_FIRE:     return "FIRE";
    case TYPE_WATER:    return "WATER";
    case TYPE_GRASS:    return "GRASS";
    case TYPE_ELECTRIC: return "ELECTR";
    case TYPE_PSYCHIC:  return "PSYCHC";
    case TYPE_ICE:      return "ICE";
    case TYPE_DRAGON:   return "DRAGON";
    case TYPE_DARK:     return "DARK";
    default:            return "???";
    }
}

static ImU32 GetTypeColor(u8 type, u8 alpha = 235) {
    switch (type) {
    case TYPE_NORMAL:   return IM_COL32(150, 150, 130, alpha);
    case TYPE_FIGHTING: return IM_COL32(192,  48,  40, alpha);
    case TYPE_FLYING:   return IM_COL32(138, 124, 224, alpha);
    case TYPE_POISON:   return IM_COL32(160,  64, 160, alpha);
    case TYPE_GROUND:   return IM_COL32(204, 172,  84, alpha);
    case TYPE_ROCK:     return IM_COL32(184, 160,  56, alpha);
    case TYPE_BUG:      return IM_COL32(152, 174,  32, alpha);
    case TYPE_GHOST:    return IM_COL32(112,  88, 152, alpha);
    case TYPE_STEEL:    return IM_COL32(156, 156, 184, alpha);
    case TYPE_FIRE:     return IM_COL32(226, 114,  44, alpha);
    case TYPE_WATER:    return IM_COL32( 84, 136, 228, alpha);
    case TYPE_GRASS:    return IM_COL32( 96, 184,  68, alpha);
    case TYPE_ELECTRIC: return IM_COL32(228, 186,  38, alpha);
    case TYPE_PSYCHIC:  return IM_COL32(232,  82, 128, alpha);
    case TYPE_ICE:      return IM_COL32(118, 202, 202, alpha);
    case TYPE_DRAGON:   return IM_COL32(112,  56, 236, alpha);
    case TYPE_DARK:     return IM_COL32(112,  88,  72, alpha);
    default:            return IM_COL32(110, 118, 130, alpha);
    }
}

static void DrawShadowText(ImDrawList *dl, ImFont *font, float fontSize, ImVec2 pos, ImU32 col, const char *text) {
    if (!text || !text[0]) return;
    const ImU8 a = (col >> IM_COL32_A_SHIFT) & 0xFF;
    dl->AddText(font, fontSize, ImVec2(pos.x + 1.5f, pos.y + 1.5f), IM_COL32(12, 14, 18, (a * 210) / 255), text);
    dl->AddText(font, fontSize, pos, col, text);
}

static void DrawWeatherBadge(ImDrawList *dl, ImFont *boldFont, const ImVec2 &disp, int weather) {
    if (weather <= 0) return;
    const char *label = nullptr;
    ImU32 bgCol = IM_COL32(40, 48, 64, 215);
    ImU32 accentCol = IM_COL32(255, 210, 80, 255);
    switch (weather) {
    case 1: // Sunny
        label = "HARSH SUNLIGHT";
        bgCol = IM_COL32(145, 75, 18, 220);
        accentCol = IM_COL32(255, 205, 65, 255);
        break;
    case 2: // Rain
        label = "RAIN";
        bgCol = IM_COL32(24, 78, 145, 220);
        accentCol = IM_COL32(105, 195, 255, 255);
        break;
    case 3: // Sandstorm
        label = "SANDSTORM";
        bgCol = IM_COL32(125, 96, 46, 220);
        accentCol = IM_COL32(235, 198, 120, 255);
        break;
    case 4: // Hail
        label = "HAIL";
        bgCol = IM_COL32(45, 118, 142, 220);
        accentCol = IM_COL32(175, 240, 255, 255);
        break;
    case 5: // Fog
        label = "DEEP FOG";
        bgCol = IM_COL32(72, 82, 96, 220);
        accentCol = IM_COL32(205, 215, 228, 255);
        break;
    default:
        return;
    }

    const float fontSize = std::max(14.0f, disp.y * 0.022f);
    const ImVec2 ts = boldFont->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, label);
    const float iconR = fontSize * 0.42f;
    const float padX = fontSize * 0.75f;
    const float padY = fontSize * 0.34f;
    const float w = padX * 2.0f + iconR * 2.4f + ts.x;
    const float h = ts.y + padY * 2.0f;
    const float x0 = (disp.x - w) * 0.5f;
    const float y0 = disp.y * 0.016f;
    const float x1 = x0 + w;
    const float y1 = y0 + h;
    const float r = h * 0.5f;

    dl->AddRectFilled(ImVec2(x0, y0), ImVec2(x1, y1), bgCol, r);
    dl->AddRect(ImVec2(x0, y0), ImVec2(x1, y1), accentCol, r, 0, 1.8f);

    const ImVec2 ic(x0 + padX + iconR, (y0 + y1) * 0.5f);
    if (weather == 1) {
        dl->AddCircleFilled(ic, iconR * 0.75f, accentCol);
        for (int k = 0; k < 8; ++k) {
            float ang = k * 0.785398f;
            dl->AddLine(
                ImVec2(ic.x + std::cos(ang) * iconR * 0.9f, ic.y + std::sin(ang) * iconR * 0.9f),
                ImVec2(ic.x + std::cos(ang) * iconR * 1.25f, ic.y + std::sin(ang) * iconR * 1.25f),
                accentCol, 1.6f);
        }
    } else if (weather == 2) {
        for (int k = -1; k <= 1; ++k) {
            float ox = ic.x + k * iconR * 0.65f;
            dl->AddLine(ImVec2(ox + 2.0f, ic.y - iconR * 0.7f), ImVec2(ox - 2.0f, ic.y + iconR * 0.7f), accentCol, 2.0f);
        }
    } else if (weather == 3) {
        dl->AddCircleFilled(ImVec2(ic.x - iconR * 0.4f, ic.y - iconR * 0.3f), 2.2f, accentCol);
        dl->AddCircleFilled(ImVec2(ic.x + iconR * 0.4f, ic.y), 2.5f, accentCol);
        dl->AddCircleFilled(ImVec2(ic.x - iconR * 0.1f, ic.y + iconR * 0.45f), 2.2f, accentCol);
    } else if (weather == 4) {
        dl->AddQuadFilled(
            ImVec2(ic.x, ic.y - iconR * 0.85f),
            ImVec2(ic.x + iconR * 0.75f, ic.y),
            ImVec2(ic.x, ic.y + iconR * 0.85f),
            ImVec2(ic.x - iconR * 0.75f, ic.y),
            accentCol);
    } else {
        dl->AddLine(ImVec2(ic.x - iconR * 0.8f, ic.y - 3.0f), ImVec2(ic.x + iconR * 0.8f, ic.y - 3.0f), accentCol, 2.0f);
        dl->AddLine(ImVec2(ic.x - iconR * 0.6f, ic.y + 3.0f), ImVec2(ic.x + iconR * 0.9f, ic.y + 3.0f), accentCol, 2.0f);
    }

    DrawShadowText(dl, boldFont, fontSize, ImVec2(ic.x + iconR * 1.5f, y0 + padY), IM_COL32(255, 255, 255, 255), label);
}

static void DrawActionCard(ImDrawList *dl, ImFont *boldFont, float x0, float y0, float x1, float y1,
                           const char *label, ImU32 topCol, ImU32 botCol, bool active) {
    const float r = std::min((y1 - y0) * 0.24f, 14.0f);
    if (active) {
        x0 += 1.5f; y0 += 1.5f; x1 -= 1.5f; y1 -= 1.5f;
    }
    dl->AddRectFilled(ImVec2(x0 + 2.0f, y0 + 3.0f), ImVec2(x1 + 2.0f, y1 + 3.0f), IM_COL32(0, 0, 0, 120), r);
    dl->AddRectFilledMultiColor(ImVec2(x0, y0), ImVec2(x1, y1), topCol, topCol, botCol, botCol);
    // Subtle inner highlight
    dl->AddRect(ImVec2(x0 + 1.5f, y0 + 1.5f), ImVec2(x1 - 1.5f, y1 - 1.5f), IM_COL32(255, 255, 255, 75), r, 0, 1.2f);
    dl->AddRect(ImVec2(x0, y0), ImVec2(x1, y1), IM_COL32(24, 28, 36, 240), r, 0, 2.2f);

    const float fontSize = std::max(18.0f, std::min((y1 - y0) * 0.44f, (x1 - x0) * 0.20f));
    const ImVec2 ts = boldFont->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, label);
    DrawShadowText(dl, boldFont, fontSize,
                   ImVec2((x0 + x1 - ts.x) * 0.5f, (y0 + y1 - ts.y) * 0.5f),
                   IM_COL32(255, 255, 255, 255), label);
}

static int ComputeMoveEffectiveness(const GearMove &mv, const GearBattle &battle, int attackerIdx) {
    if (mv.id == 0 || mv.category == CLASS_STATUS) {
        return -1; // Status / non-damaging
    }
    // Find primary enemy defender
    int defIdx = -1;
    for (int i = 0; i < GEAR_BATTLERS_MAX; ++i) {
        if (battle.battlers[i].present && battle.battlers[i].enemy && battle.battlers[i].hp > 0) {
            defIdx = i;
            break;
        }
    }
    if (defIdx < 0) {
        return 10;
    }
    const GearBattler &def = battle.battlers[defIdx];
    const GearBattler *atk = (attackerIdx >= 0 && attackerIdx < GEAR_BATTLERS_MAX) ? &battle.battlers[attackerIdx] : nullptr;
    bool moldBreaker = atk && (atk->ability == ABILITY_MOLD_BREAKER);
    if (!moldBreaker && mv.type == TYPE_GROUND && def.ability == ABILITY_LEVITATE) {
        return 0;
    }
    int mult = Gear_TypeEffectiveness(mv.type, def.type1, def.type2);
    if (!moldBreaker && def.ability == ABILITY_WONDER_GUARD && mult <= 10) {
        return 0;
    }
    return mult;
}

static void DrawFullScreenBattleOverlay(ImDrawList *dl, const ImVec2 &disp, float scale,
                                        bool tapJustBegan, float tapX, float tapY) {
    ImFont *boldFont = SIM::UI::BoldFont();
    ImFont *regFont = ImGui::GetFont();

    // 1. Gen V Weather Indicator
    const int weather = Gear_BattleWeather();
    if (weather > 0) {
        DrawWeatherBadge(dl, boldFont, disp, weather);
    }

    GearBattleMenu menu;
    if (!Gear_GetBattleMenu(&menu) || !menu.ready) {
        return;
    }

    GearBattle battle;
    const bool hasBattle = Gear_GetBattle(&battle) != FALSE;

    // 2. < [Poke Ball + Qty] > Carousel on Main Action Screen
    if (menu.kind == GEAR_MENU_ACTION) {
        BattleOverlayBallEntry balls[16];
        BOOL canThrow = FALSE;
        const int ballCount = Gear_GetBattlePokeBalls(balls, 16, &canThrow);
        if (canThrow && ballCount > 0) {
            if (sSelectedBallIdx >= ballCount) sSelectedBallIdx = 0;
            if (sSelectedBallIdx < 0) sSelectedBallIdx = ballCount - 1;

            const BattleOverlayBallEntry &curBall = balls[sSelectedBallIdx];
            const float spriteSz = std::max(32.0f, disp.y * 0.056f * scale);
            const float arrowW = spriteSz * 0.72f;
            const float boxW = spriteSz + arrowW * 2.0f + 16.0f;
            const float boxH = spriteSz + 26.0f;
            // Positioned right above the player HP box (matching mockup)
            const float cx = disp.x * 0.625f;
            const float cy = disp.y * 0.505f;
            const float x0 = cx - boxW * 0.5f;
            const float y0 = cy - boxH * 0.5f;
            const float x1 = x0 + boxW;
            const float y1 = y0 + boxH;

            dl->AddRectFilled(ImVec2(x0, y0), ImVec2(x1, y1), IM_COL32(18, 22, 30, 195), 10.0f);
            dl->AddRect(ImVec2(x0, y0), ImVec2(x1, y1), IM_COL32(235, 240, 250, 180), 10.0f, 0, 1.5f);

            const float leftX0 = x0 + 4.0f;
            const float leftX1 = leftX0 + arrowW;
            const float rightX1 = x1 - 4.0f;
            const float rightX0 = rightX1 - arrowW;
            const float midX0 = leftX1 + 4.0f;
            const float midX1 = rightX0 - 4.0f;
            const float iconCy = y0 + 6.0f + spriteSz * 0.5f;

            // Left / Right Triangle Arrows
            const float triSz = arrowW * 0.34f;
            const float lMidX = (leftX0 + leftX1) * 0.5f;
            const float rMidX = (rightX0 + rightX1) * 0.5f;
            dl->AddTriangleFilled(
                ImVec2(lMidX - triSz * 0.6f, iconCy),
                ImVec2(lMidX + triSz * 0.5f, iconCy - triSz),
                ImVec2(lMidX + triSz * 0.5f, iconCy + triSz),
                IM_COL32(255, 255, 255, ballCount > 1 ? 245 : 90));
            dl->AddTriangleFilled(
                ImVec2(rMidX + triSz * 0.6f, iconCy),
                ImVec2(rMidX - triSz * 0.5f, iconCy - triSz),
                ImVec2(rMidX - triSz * 0.5f, iconCy + triSz),
                IM_COL32(255, 255, 255, ballCount > 1 ? 245 : 90));

            // 24x24 Native NARC Poke Ball Sprite
            const float imgX0 = (midX0 + midX1 - spriteSz) * 0.5f;
            const float imgY0 = y0 + 5.0f;
            unsigned int tex = GetBallTexture(curBall.item);
            if (tex != 0) {
                dl->AddImage((ImTextureID)(uintptr_t)tex, ImVec2(imgX0, imgY0), ImVec2(imgX0 + spriteSz, imgY0 + spriteSz));
            } else {
                dl->AddCircleFilled(ImVec2((midX0 + midX1) * 0.5f, iconCy), spriteSz * 0.4f, IM_COL32(225, 55, 55, 240));
            }

            // Quantity badge (xNN) + Quick Throw hint
            char qtyBuf[16];
            std::snprintf(qtyBuf, sizeof(qtyBuf), "x%u", (unsigned)curBall.count);
            const float qFont = std::max(12.0f, disp.y * 0.019f);
            const ImVec2 qSz = boldFont->CalcTextSizeA(qFont, FLT_MAX, 0.0f, qtyBuf);
            DrawShadowText(dl, boldFont, qFont, ImVec2(midX1 - qSz.x + 2.0f, y0 + 3.0f), IM_COL32(255, 235, 110, 255), qtyBuf);

            const float hFont = std::max(10.5f, disp.y * 0.015f);
            const char *hint = curBall.name[0] ? curBall.name : "TAP TO THROW";
            const ImVec2 hSz = regFont->CalcTextSizeA(hFont, FLT_MAX, 0.0f, hint);
            DrawShadowText(dl, regFont, hFont, ImVec2(cx - hSz.x * 0.5f, y1 - hSz.y - 3.0f), IM_COL32(220, 230, 245, 235), hint);

            if (tapJustBegan) {
                if (ballCount > 1 && PointInRect(tapX, tapY, leftX0, y0, leftX1, y1)) {
                    sSelectedBallIdx = (sSelectedBallIdx - 1 + ballCount) % ballCount;
                    return;
                }
                if (ballCount > 1 && PointInRect(tapX, tapY, rightX0, y0, rightX1, y1)) {
                    sSelectedBallIdx = (sSelectedBallIdx + 1) % ballCount;
                    return;
                }
                if (PointInRect(tapX, tapY, midX0, y0, midX1, y1)) {
                    Gear_UseBattlePokeBall(curBall.item);
                    return;
                }
            }
        }
    }

    // 3. Bottom-Right Command / Move Card Panel (leaves bottom-left dialogue window open!)
    const float panelX0 = disp.x * 0.435f;
    const float panelX1 = disp.x * 0.988f;
    const float panelY0 = disp.y * 0.772f;
    const float panelY1 = disp.y * 0.962f;
    const float gap = std::max(6.0f, disp.x * 0.006f);

    if (menu.kind == GEAR_MENU_ACTION || menu.kind == GEAR_MENU_FIRST || menu.kind == GEAR_MENU_PAL_PARK) {
        const float colW = (panelX1 - panelX0 - gap) * 0.5f;
        const float rowH = (panelY1 - panelY0 - gap) * 0.5f;

        const float c0x0 = panelX0, c0x1 = panelX0 + colW;
        const float c1x0 = c0x1 + gap, c1x1 = panelX1;
        const float r0y0 = panelY0, r0y1 = panelY0 + rowH;
        const float r1y0 = r0y1 + gap, r1y1 = panelY1;

        if (menu.kind == GEAR_MENU_FIRST) {
            DrawActionCard(dl, boldFont, panelX0, panelY0, panelX1, panelY1, "FIGHT",
                           IM_COL32(215, 58, 58, 235), IM_COL32(150, 28, 28, 235), false);
            if (tapJustBegan && PointInRect(tapX, tapY, panelX0, panelY0, panelX1, panelY1)) {
                Gear_PressBattleButton(0);
            }
            return;
        }

        if (menu.kind == GEAR_MENU_PAL_PARK) {
            DrawActionCard(dl, boldFont, c0x0, panelY0, c0x1, panelY1, "PARK BALL",
                           IM_COL32(62, 130, 215, 235), IM_COL32(34, 82, 155, 235), false);
            DrawActionCard(dl, boldFont, c1x0, panelY0, c1x1, panelY1, "RETIRE",
                           IM_COL32(215, 58, 58, 235), IM_COL32(150, 28, 28, 235), false);
            if (tapJustBegan) {
                if (PointInRect(tapX, tapY, c0x0, panelY0, c0x1, panelY1)) Gear_PressBattleButton(0);
                else if (PointInRect(tapX, tapY, c1x0, panelY0, c1x1, panelY1)) Gear_PressBattleButton(1);
            }
            return;
        }

        const char *lbl0 = menu.safari ? "SAFARI BALL" : "FIGHT";
        const char *lbl1 = menu.safari ? "BAIT" : "BAG";
        const char *lbl2 = menu.safari ? "ROCK" : "POKEMON";
        const char *lbl3 = menu.cancel ? "BACK" : "RUN";

        // Top-Left: FIGHT (button 0)
        DrawActionCard(dl, boldFont, c0x0, r0y0, c0x1, r0y1, lbl0,
                       IM_COL32(215, 58, 58, 235), IM_COL32(150, 28, 28, 235), false);
        // Top-Right: BAG (button 2 in sActionMenuTouchRects)
        DrawActionCard(dl, boldFont, c1x0, r0y0, c1x1, r0y1, lbl1,
                       IM_COL32(218, 152, 42, 235), IM_COL32(164, 102, 18, 235), false);
        // Bottom-Left: POKEMON (button 1 in sActionMenuTouchRects)
        DrawActionCard(dl, boldFont, c0x0, r1y0, c0x1, r1y1, lbl2,
                       IM_COL32(52, 168, 76, 235), IM_COL32(28, 118, 48, 235), false);
        // Bottom-Right: RUN / BACK (button 3 in sActionMenuTouchRects)
        DrawActionCard(dl, boldFont, c1x0, r1y0, c1x1, r1y1, lbl3,
                       IM_COL32(58, 132, 218, 235), IM_COL32(32, 86, 162, 235), false);

        if (tapJustBegan) {
            if (PointInRect(tapX, tapY, c0x0, r0y0, c0x1, r0y1)) {
                Gear_PressBattleButton(0);
            } else if (PointInRect(tapX, tapY, c1x0, r0y0, c1x1, r0y1)) {
                Gear_PressBattleButton(2);
            } else if (PointInRect(tapX, tapY, c0x0, r1y0, c0x1, r1y1)) {
                Gear_PressBattleButton(1);
            } else if (PointInRect(tapX, tapY, c1x0, r1y0, c1x1, r1y1)) {
                Gear_PressBattleButton(3);
            }
        }
        return;
    }

    if (menu.kind == GEAR_MENU_MOVES) {
        // Dedicated BACK button on the left side of the move cards (button 0)
        const float backW = std::max(64.0f, (panelX1 - panelX0) * 0.145f);
        const float backX0 = panelX0;
        const float backX1 = backX0 + backW;
        const float gridX0 = backX1 + gap;
        const float colW = (panelX1 - gridX0 - gap) * 0.5f;
        const float rowH = (panelY1 - panelY0 - gap) * 0.5f;

        DrawActionCard(dl, boldFont, backX0, panelY0, backX1, panelY1, "BACK",
                       IM_COL32(76, 84, 98, 235), IM_COL32(46, 52, 64, 235), false);
        if (tapJustBegan && PointInRect(tapX, tapY, backX0, panelY0, backX1, panelY1)) {
            Gear_PressBattleButton(0);
            return;
        }

        for (int m = 0; m < 4; ++m) {
            const int col = m % 2;
            const int row = m / 2;
            const float x0 = gridX0 + col * (colW + gap);
            const float y0 = panelY0 + row * (rowH + gap);
            const float x1 = x0 + colW;
            const float y1 = y0 + rowH;
            const float r = std::min(rowH * 0.20f, 11.0f);

            const GearMove &mv = menu.moves[m];
            const bool valid = (mv.id != 0);
            const ImU32 baseCol = valid ? GetTypeColor(mv.type, 225) : IM_COL32(42, 46, 54, 185);
            const ImU32 darkCol = valid ? GetTypeColor(mv.type, 175) : IM_COL32(28, 32, 38, 185);

            dl->AddRectFilled(ImVec2(x0 + 2.0f, y0 + 2.5f), ImVec2(x1 + 2.0f, y1 + 2.5f), IM_COL32(0, 0, 0, 115), r);
            dl->AddRectFilledMultiColor(ImVec2(x0, y0), ImVec2(x1, y1), baseCol, baseCol, darkCol, darkCol);
            dl->AddRectFilled(ImVec2(x0, y0), ImVec2(x1, y1), IM_COL32(14, 16, 22, 85), r);
            dl->AddRect(ImVec2(x0, y0), ImVec2(x1, y1), IM_COL32(240, 244, 252, valid ? 195 : 80), r, 0, 1.6f);

            if (!valid) {
                const float dashFont = std::max(15.0f, rowH * 0.34f);
                const ImVec2 dSz = boldFont->CalcTextSizeA(dashFont, FLT_MAX, 0.0f, "-");
                DrawShadowText(dl, boldFont, dashFont, ImVec2((x0 + x1 - dSz.x) * 0.5f, (y0 + y1 - dSz.y) * 0.5f),
                               IM_COL32(180, 185, 195, 160), "-");
                continue;
            }

            const float padX = std::max(7.0f, colW * 0.045f);
            const float padY = std::max(4.0f, rowH * 0.08f);
            const float nameFont = std::max(13.5f, std::min(rowH * 0.32f, colW * 0.11f));
            const float subFont = std::max(10.5f, rowH * 0.22f);

            // Top-Left: Move Name
            const char *moveName = Gear_MoveName(mv.id);
            DrawShadowText(dl, boldFont, nameFont, ImVec2(x0 + padX, y0 + padY), IM_COL32(255, 255, 255, 255), moveName ? moveName : "");

            // Top-Right: PP Counter
            char ppBuf[20];
            std::snprintf(ppBuf, sizeof(ppBuf), "PP %u/%u", (unsigned)mv.pp, (unsigned)mv.maxPp);
            const ImVec2 ppSz = boldFont->CalcTextSizeA(subFont, FLT_MAX, 0.0f, ppBuf);
            const ImU32 ppCol = (mv.pp == 0)
                ? IM_COL32(255, 95, 95, 255)
                : (mv.pp * 4 <= mv.maxPp ? IM_COL32(255, 210, 90, 255) : IM_COL32(245, 248, 255, 245));
            DrawShadowText(dl, boldFont, subFont, ImVec2(x1 - padX - ppSz.x, y0 + padY + (nameFont - subFont) * 0.4f), ppCol, ppBuf);

            // Bottom-Left: Type Pill Badge
            const char *typeStr = GetTypeName(mv.type);
            const ImVec2 tSz = boldFont->CalcTextSizeA(subFont * 0.92f, FLT_MAX, 0.0f, typeStr);
            const float pillW = tSz.x + 10.0f;
            const float pillH = tSz.y + 4.0f;
            const float pillY0 = y1 - padY - pillH;
            dl->AddRectFilled(ImVec2(x0 + padX, pillY0), ImVec2(x0 + padX + pillW, pillY0 + pillH), GetTypeColor(mv.type, 255), 5.0f);
            dl->AddRect(ImVec2(x0 + padX, pillY0), ImVec2(x0 + padX + pillW, pillY0 + pillH), IM_COL32(255, 255, 255, 190), 5.0f, 0, 1.0f);
            DrawShadowText(dl, boldFont, subFont * 0.92f, ImVec2(x0 + padX + 5.0f, pillY0 + 2.0f), IM_COL32(255, 255, 255, 255), typeStr);

            // Bottom-Right: Effectiveness Badge
            const int eff = hasBattle ? ComputeMoveEffectiveness(mv, battle, menu.battler) : -1;
            const char *effText = nullptr;
            ImU32 effCol = IM_COL32(225, 230, 238, 235);
            if (eff == 0) {
                effText = "NO EFFECT";
                effCol = IM_COL32(255, 100, 100, 255);
            } else if (eff > 0 && eff < 10) {
                effText = "NOT VERY EFFECTIVE";
                effCol = IM_COL32(255, 185, 85, 255);
            } else if (eff > 10) {
                effText = "SUPER EFFECTIVE";
                effCol = IM_COL32(155, 255, 115, 255);
            } else if (eff == 10) {
                effText = "NORMAL EFFECT";
                effCol = IM_COL32(225, 232, 242, 235);
            }
            if (effText != nullptr) {
                const float eFont = subFont * 0.88f;
                const ImVec2 eSz = boldFont->CalcTextSizeA(eFont, FLT_MAX, 0.0f, effText);
                DrawShadowText(dl, boldFont, eFont, ImVec2(x1 - padX - eSz.x, y1 - padY - eSz.y), effCol, effText);
            }

            if (tapJustBegan && PointInRect(tapX, tapY, x0, y0, x1, y1)) {
                Gear_PressBattleButton(m + 1);
                return;
            }
        }
        return;
    }

    if (menu.kind == GEAR_MENU_TARGETS) {
        const float backW = std::max(64.0f, (panelX1 - panelX0) * 0.145f);
        const float backX0 = panelX0;
        const float backX1 = backX0 + backW;
        const float gridX0 = backX1 + gap;
        const float colW = (panelX1 - gridX0 - gap) * 0.5f;
        const float rowH = (panelY1 - panelY0 - gap) * 0.5f;

        DrawActionCard(dl, boldFont, backX0, panelY0, backX1, panelY1, "BACK",
                       IM_COL32(76, 84, 98, 235), IM_COL32(46, 52, 64, 235), false);
        if (tapJustBegan && PointInRect(tapX, tapY, backX0, panelY0, backX1, panelY1)) {
            Gear_PressBattleButton(0);
            return;
        }

        for (int t = 0; t < 4; ++t) {
            const int col = t % 2;
            const int row = t / 2;
            const float x0 = gridX0 + col * (colW + gap);
            const float y0 = panelY0 + row * (rowH + gap);
            const float x1 = x0 + colW;
            const float y1 = y0 + rowH;
            const bool ok = menu.targetOk[t] != 0;
            const int bIdx = menu.targetBattler[t];
            const char *tName = (ok && hasBattle && bIdx >= 0 && bIdx < GEAR_BATTLERS_MAX && battle.battlers[bIdx].present)
                ? battle.battlers[bIdx].nickname
                : (ok ? "TARGET" : "-");
            DrawActionCard(dl, boldFont, x0, y0, x1, y1, tName,
                           ok ? IM_COL32(62, 138, 212, 235) : IM_COL32(42, 46, 54, 175),
                           ok ? IM_COL32(34, 86, 152, 235) : IM_COL32(28, 32, 38, 175), false);
            if (ok && tapJustBegan && PointInRect(tapX, tapY, x0, y0, x1, y1)) {
                Gear_PressBattleButton(t + 1);
                return;
            }
        }
    }
}

extern "C" {

BOOL TouchControls_IsEnabled(void) {
    SIM_Config_prj_type *cfg = SIM_Config_prj_GetConfig();
    return (cfg->touchOverlay && !SIM::Settings::IsOpen() && !SIM_Carts_UIActive()) ? TRUE : FALSE;
}

void TouchControls_DrawOverlay(void) {
    SIM_Config_prj_type *cfg = SIM_Config_prj_GetConfig();
    const bool allowFullExpand = (cfg->fullScreenCompanion != FALSE);
    const bool primaryFull = SIM_Carts_UIActive() || (allowFullExpand && SIM::Settings::IsOpen());
    const bool companionFull = allowFullExpand && !primaryFull && (SIM_Companion_IsFullScreen() != FALSE);
    const bool pipOverlayMode = !primaryFull && !companionFull && (cfg->topRightOverlay != FALSE);
    const bool pipMinimized = pipOverlayMode && (cfg->topRightOverlayMinimized != FALSE);
    const bool inBattle = IsBattleScreenActive();
    const bool needsDsScreen = inBattle && (Gear_BattleNeedsDsScreen() != FALSE);
    const bool battleOverlayActive = pipOverlayMode && inBattle && !needsDsScreen;
    GearBattleMenu activeBattleMenu;
    const bool battleMenuVisible = battleOverlayActive && (Gear_GetBattleMenu(&activeBattleMenu) != FALSE) && activeBattleMenu.ready;
    const bool hideFaceButtonsInBattle = pipOverlayMode && inBattle;
    SyncAndroidOverlayScale(GetTopLeftWidthFraction(cfg));
    SyncAndroidBattleMode(inBattle);
    SyncAndroidPipAutoFade(cfg->topRightOverlayAutoFade != FALSE);
    const bool hidePip = inBattle && !needsDsScreen;
    const int androidLayoutMode = primaryFull
        ? 2
        : ((companionFull || (pipOverlayMode && needsDsScreen))
            ? 1
            : (pipOverlayMode ? ((pipMinimized || hidePip) ? 4 : 3) : 0));
    SyncAndroidLayoutMode(androidLayoutMode);

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

    const BottomLayout L = ComputeBottomLayout(disp.x, disp.y, scale * 0.88f, hideFaceButtonsInBattle ? 0.63f : 0.69f);
    const float battleAR = L.faceBtnR * 0.66f;
    const float battleACx = disp.x * 0.032f + battleAR * 1.10f;
    const float battleACy = rowY1 - battleAR;

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
                EvalBottomPoint(fx, fy, L, pressed, !hideFaceButtonsInBattle);
                if (hideFaceButtonsInBattle) {
                    if (PointInCircle(fx, fy, battleACx, battleACy, battleAR * 1.25f)) {
                        pressed[VBTN_A] = true;
                    }
                }
            }
        }
    }

    bool anyTouchNow = false;
    float firstTouchX = 0.0f;
    float firstTouchY = 0.0f;
    for (int d = 0; d < numDevices && !anyTouchNow; ++d) {
        const SDL_TouchID dev = SDL_GetTouchDevice(d);
        if (dev == 0) continue;
        if (SDL_GetNumTouchFingers(dev) > 0) {
            const SDL_Finger *finger = SDL_GetTouchFinger(dev, 0);
            if (finger) {
                anyTouchNow = true;
                firstTouchX = finger->x * disp.x;
                firstTouchY = finger->y * disp.y;
            }
        }
    }
    const bool tapJustBegan = anyTouchNow && !sBattleHudTouchActive;
    sBattleHudTouchActive = anyTouchNow;

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
        DrawIconPill(dl, pipX0, rowY0, pipX1, rowY1, screenHidden ? PillIcon::EyeClosed : PillIcon::EyeOpen, screenHidden, alpha, IM_COL32(245, 185, 55, 255), true);
    }
    DrawIconPill(dl, swX0, rowY0, swX1, rowY1, PillIcon::SwitchArr, switchActive, alpha, IM_COL32(70, 195, 235, 255));
    DrawIconPill(dl, selX0, rowY0, selX1, rowY1, PillIcon::SelectWin, pressed[VBTN_SELECT], alpha);

    DrawIconPill(dl, staX0, rowY0, staX1, rowY1, PillIcon::StartTri, pressed[VBTN_START], alpha);
    DrawIconPill(dl, ffX0, rowY0, ffX1, rowY1, PillIcon::FastFwd, ffActive, alpha, IM_COL32(245, 155, 50, 255));
    DrawPillButton(dl, boldFont, rX0, rowY0, rX1, rowY1, "R", pressed[VBTN_R], alpha);


    if (pipOverlayMode) {
        ImFont *font = ImGui::GetFont();
        if (!battleOverlayActive) {
            DrawDpad(dl, L.dpadCx, L.dpadCy, L.dpadR,
                     pressed[VBTN_UP], pressed[VBTN_DOWN], pressed[VBTN_LEFT], pressed[VBTN_RIGHT], alpha);
        }
        if (!hideFaceButtonsInBattle) {
            DrawCircleButton(dl, font, L.aCx, L.aCy, L.faceBtnR, "A", pressed[VBTN_A], alpha, IM_COL32(235, 75, 85, 255));
            DrawCircleButton(dl, font, L.bCx, L.bCy, L.faceBtnR, "B", pressed[VBTN_B], alpha, IM_COL32(235, 185, 55, 255));
            DrawCircleButton(dl, font, L.xCx, L.xCy, L.faceBtnR, "X", pressed[VBTN_X], alpha, IM_COL32(75, 145, 240, 255));
            DrawCircleButton(dl, font, L.yCx, L.yCy, L.faceBtnR, "Y", pressed[VBTN_Y], alpha, IM_COL32(70, 200, 120, 255));
        } else if (!battleMenuVisible) {
            DrawCircleButton(dl, font, battleACx, battleACy, battleAR, "A", pressed[VBTN_A], alpha, IM_COL32(235, 75, 85, 255));
        }
    }

    if (battleOverlayActive) {
        DrawFullScreenBattleOverlay(dl, disp, scale, tapJustBegan, firstTouchX, firstTouchY);
    }
}

void TouchControls_DrawBottomOverlay(float width, float height) {
    SIM_Config_prj_type *cfg = SIM_Config_prj_GetConfig();
    const bool allowFullExpand = (cfg->fullScreenCompanion != FALSE);
    const bool primaryFull = SIM_Carts_UIActive() || (allowFullExpand && SIM::Settings::IsOpen());
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
        DrawIconPill(dl, sw.x0, sw.y0, sw.x1, sw.y1, PillIcon::SwitchArr, true, alpha, IM_COL32(70, 195, 235, 255));
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

    bool autoFade = cfg->topRightOverlayAutoFade != FALSE;
    if (SIM::UI::Toggle(
            "Auto-fade inactive PiP overlay",
            "Smoothly fade the corner lower-screen overlay to semi-transparent after 7 seconds of inactivity outside battle.",
            &autoFade, topLeft)) {
        cfg->topRightOverlayAutoFade = autoFade ? TRUE : FALSE;
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
    const bool primaryFull = SIM_Carts_UIActive() || (allowFullExpand && SIM::Settings::IsOpen());
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
