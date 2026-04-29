#pragma once
#include "raylib.h"
#include <vector>

// ─────────────────────────────────────────────────────────────────────────────
// TouchControls — virtual joystick + action buttons for touchscreen play.
//
// Handles ONLY the joystick, ATK, and DASH buttons.
// Ability arc tapping is handled by Engine (needs icon textures + player data).
//
// Per-frame engine usage:
//   _touch.Update(screenW, screenH);
//   _player.SetTouchDirection(_touch.joystickDir);
//   if (_touch.attackPressed) _player.SetTouchAttack();
//   if (_touch.dashPressed)   _player.SetTouchDash();
//
// Expose tracked touch IDs so Engine can skip them when scanning ability taps.
// ─────────────────────────────────────────────────────────────────────────────
struct TouchControls
{
    // ── Per-frame outputs ─────────────────────────────────────────────────────
    Vector2 joystickDir{};          // normalised [-1,1] move direction
    bool    attackPressed  = false; // true for exactly one frame on tap
    bool    dashPressed    = false; // true for exactly one frame on tap

    // ── Joystick visual state ─────────────────────────────────────────────────
    bool    joystickVisible = false;
    Vector2 joystickAnchor{};
    Vector2 joystickStick{};

    // ── Layout constants ──────────────────────────────────────────────────────
    static constexpr float kJoyRadius      = 90.f;   // outer ring radius
    static constexpr float kJoyDeadZone    = 14.f;
    static constexpr int   kJoyDirections  = 8;
    static constexpr float kBtnRadius      = 81.f;   // ATK hit + visual radius
    static constexpr float kDashBtnRadius  = 65.f;   // DASH hit + visual radius
    static constexpr float kBtnRightPad    = 140.f;  // ATK centre x from right edge
    static constexpr float kBtnBotPad      = 160.f;  // ATK centre y from bottom edge
    static constexpr float kDashBtnOffset  = 185.f;  // DASH is this far left of ATK

    // ── API ───────────────────────────────────────────────────────────────────
    void Update(int screenW, int screenH);
    void Draw(int screenW, int screenH) const;

    // Returns the touch IDs currently owned by joy/ATK/DASH (-1 = none).
    // Engine uses these to skip those touches when scanning for ability taps.
    int GetJoyTouchId()  const { return _joyTouchId;  }
    int GetAtkTouchId()  const { return _atkTouchId;  }
    int GetDashTouchId() const { return _dashTouchId; }

    // Whether any touch is currently being tracked
    bool IsActive() const { return _joyTouchId >= 0 || _atkTouchId >= 0 || _dashTouchId >= 0; }

private:
    int _joyTouchId  = -1;
    int _atkTouchId  = -1;
    int _dashTouchId = -1;
    int _joySector   = -1; // sticky 8-way sector for gameplay movement only

    static bool IsTouchIdAlive(int id, int touchCount);
    static int  FindTouchIndex(int id, int touchCount);
    Vector2 QuantizeJoystickDir(Vector2 rawDir, float magnitude);
};
