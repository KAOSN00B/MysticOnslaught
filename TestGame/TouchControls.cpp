#include "TouchControls.h"
#include "raymath.h"
#include <cmath>
#include <algorithm>

// ─── helpers ─────────────────────────────────────────────────────────────────

bool TouchControls::IsTouchIdAlive(int id, int touchCount)
{
    for (int i = 0; i < touchCount; i++)
        if (GetTouchPointId(i) == id) return true;
    return false;
}

int TouchControls::FindTouchIndex(int id, int touchCount)
{
    for (int i = 0; i < touchCount; i++)
        if (GetTouchPointId(i) == id) return i;
    return -1;
}

// ─── Update ──────────────────────────────────────────────────────────────────

void TouchControls::Update(int screenW, int screenH)
{
    attackPressed = false;
    dashPressed   = false;

    const int tc = GetTouchPointCount();

    // Button positions (shared by real-touch and mouse-simulation paths)
    const Vector2 atkCenter  = { (float)screenW - kBtnRightPad,                  (float)screenH - kBtnBotPad };
    const Vector2 dashCenter = { (float)screenW - kBtnRightPad - kDashBtnOffset, (float)screenH - kBtnBotPad };

    // ── Mouse simulation (desktop testing of touch mode) ──────────────────────
    // Uses fake touch ID 9999 so the rest of the logic stays uniform.
    if (tc == 0)
    {
        static constexpr int kMouseFakeId = 9999;
        const bool mouseHeld    = IsMouseButtonDown(MOUSE_LEFT_BUTTON);
        const bool mousePressed = IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
        const Vector2 mousePos  = GetMousePosition();

        // Release everything when mouse button lifts
        if (!mouseHeld)
        {
            _joyTouchId  = -1;
            _atkTouchId  = -1;
            _dashTouchId = -1;
            joystickDir     = Vector2Zero();
            joystickVisible = false;
            return;
        }

        // Assign zone on new press
        if (mousePressed && _joyTouchId < 0 && _atkTouchId < 0 && _dashTouchId < 0)
        {
            if (Vector2Distance(mousePos, atkCenter) <= kBtnRadius * 1.3f)
            {
                _atkTouchId   = kMouseFakeId;
                attackPressed = true;
            }
            else if (Vector2Distance(mousePos, dashCenter) <= kDashBtnRadius * 1.3f)
            {
                _dashTouchId = kMouseFakeId;
                dashPressed  = true;
            }
            else if (mousePos.x < screenW * 0.5f)
            {
                _joyTouchId    = kMouseFakeId;
                joystickAnchor = mousePos;
                joystickStick  = mousePos;
                joystickVisible = true;
                joystickDir    = Vector2Zero();
            }
        }

        // Update joystick direction while held
        if (_joyTouchId == kMouseFakeId)
        {
            Vector2 delta  = Vector2Subtract(mousePos, joystickAnchor);
            float   dist   = Vector2Length(delta);
            float   clamp  = std::min(dist, kJoyRadius);
            joystickStick  = dist > 0.001f
                ? Vector2Add(joystickAnchor, Vector2Scale(Vector2Normalize(delta), clamp))
                : joystickAnchor;
            joystickDir = dist > kJoyDeadZone
                ? Vector2Scale(Vector2Normalize(delta), clamp / kJoyRadius)
                : Vector2Zero();
        }
        return;
    }

    // ── Real touch path below ─────────────────────────────────────────────────

    // ── Drop lifted touches ───────────────────────────────────────────────────
    if (_joyTouchId  >= 0 && !IsTouchIdAlive(_joyTouchId,  tc))
    {
        _joyTouchId     = -1;
        joystickDir     = Vector2Zero();
        joystickVisible = false;
    }
    if (_atkTouchId  >= 0 && !IsTouchIdAlive(_atkTouchId,  tc)) _atkTouchId  = -1;
    if (_dashTouchId >= 0 && !IsTouchIdAlive(_dashTouchId, tc)) _dashTouchId = -1;

    // ── Update joystick direction ─────────────────────────────────────────────
    if (_joyTouchId >= 0)
    {
        int idx = FindTouchIndex(_joyTouchId, tc);
        if (idx >= 0)
        {
            Vector2 pos   = GetTouchPosition(idx);
            Vector2 delta = Vector2Subtract(pos, joystickAnchor);
            float   dist  = Vector2Length(delta);
            float   clamped = std::min(dist, kJoyRadius);

            joystickStick = (dist > 0.001f)
                ? Vector2Add(joystickAnchor, Vector2Scale(Vector2Normalize(delta), clamped))
                : joystickAnchor;

            if (dist > kJoyDeadZone)
                joystickDir = Vector2Scale(Vector2Normalize(delta), clamped / kJoyRadius);
            else
                joystickDir = Vector2Zero();
        }
    }

    // ── Assign new touches ────────────────────────────────────────────────────
    for (int i = 0; i < tc; i++)
    {
        int id = GetTouchPointId(i);
        if (id == _joyTouchId || id == _atkTouchId || id == _dashTouchId) continue;

        Vector2 pos = GetTouchPosition(i);

        // ATK button (1.25× radius for a generous touch target)
        if (Vector2Distance(pos, atkCenter) <= kBtnRadius * 1.25f)
        {
            _atkTouchId   = id;
            attackPressed = true;
            continue;
        }

        // DASH button
        if (Vector2Distance(pos, dashCenter) <= kDashBtnRadius * 1.25f)
        {
            _dashTouchId = id;
            dashPressed  = true;
            continue;
        }

        // Left half = floating joystick (right half reserved for action buttons)
        if (pos.x < screenW * 0.5f && _joyTouchId < 0)
        {
            _joyTouchId     = id;
            joystickAnchor  = pos;
            joystickStick   = pos;
            joystickVisible = true;
            joystickDir     = Vector2Zero();
        }
    }
}

// ─── Draw ─────────────────────────────────────────────────────────────────────

void TouchControls::Draw(int screenW, int screenH) const
{
    const Vector2 atkCenter  = { (float)screenW - kBtnRightPad,                  (float)screenH - kBtnBotPad };
    const Vector2 dashCenter = { (float)screenW - kBtnRightPad - kDashBtnOffset, (float)screenH - kBtnBotPad };

    // ── ATK button — large red circle ────────────────────────────────────────
    {
        bool held = (_atkTouchId >= 0);
        DrawCircleV(atkCenter, kBtnRadius, held ? Fade(RED, 0.65f) : Fade(RED, 0.28f));
        DrawCircleLinesV(atkCenter, kBtnRadius, held ? Fade(WHITE, 0.85f) : Fade(WHITE, 0.45f));
        const char* label = "ATTACK";
        int w = MeasureText(label, 20);
        DrawText(label, (int)(atkCenter.x - w * 0.5f), (int)(atkCenter.y - 10), 20, RAYWHITE);
    }

    // ── DASH button — medium blue circle ─────────────────────────────────────
    {
        bool held = (_dashTouchId >= 0);
        DrawCircleV(dashCenter, kDashBtnRadius, held ? Fade(SKYBLUE, 0.65f) : Fade(SKYBLUE, 0.28f));
        DrawCircleLinesV(dashCenter, kDashBtnRadius, held ? Fade(WHITE, 0.85f) : Fade(WHITE, 0.45f));
        const char* label = "DASH";
        int w = MeasureText(label, 17);
        DrawText(label, (int)(dashCenter.x - w * 0.5f), (int)(dashCenter.y - 9), 17, RAYWHITE);
    }

    // ── Virtual joystick (floating, appears where thumb first lands) ──────────
    if (joystickVisible)
    {
        DrawCircleV(joystickAnchor, kJoyRadius,        Fade(WHITE, 0.08f));
        DrawCircleLinesV(joystickAnchor, kJoyRadius,   Fade(WHITE, 0.30f));
        DrawCircleV(joystickStick, kJoyRadius * 0.36f, Fade(WHITE, 0.48f));
    }
}
