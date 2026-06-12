#pragma once
#include "raylib.h"
#include "WebGamepad.h"

// Remappable gamepad button assignments.
// Defaults match the standard Xbox / PlayStation layout requested by the user.
// Left stick is always move and is not remappable.
struct GamepadBindings
{
    GamepadButton attack     = GAMEPAD_BUTTON_RIGHT_FACE_LEFT;  // X / Square
    GamepadButton dash       = GAMEPAD_BUTTON_RIGHT_FACE_DOWN;  // A / Cross
    GamepadButton ability[4] = {
        GAMEPAD_BUTTON_LEFT_TRIGGER_1,   // LB / L1
        GAMEPAD_BUTTON_RIGHT_TRIGGER_1,  // RB / R1
        GAMEPAD_BUTTON_LEFT_TRIGGER_2,   // LT / L2
        GAMEPAD_BUTTON_RIGHT_TRIGGER_2,  // RT / R2
    };
    GamepadButton pause = GAMEPAD_BUTTON_MIDDLE_RIGHT;          // Start / Options
};

// Human-readable label shown in the keybindings UI
inline const char* GetGamepadButtonName(GamepadButton btn)
{
    switch (btn)
    {
    case GAMEPAD_BUTTON_RIGHT_FACE_DOWN:  return "A / Cross";
    case GAMEPAD_BUTTON_RIGHT_FACE_RIGHT: return "B / Circle";
    case GAMEPAD_BUTTON_RIGHT_FACE_LEFT:  return "X / Square";
    case GAMEPAD_BUTTON_RIGHT_FACE_UP:    return "Y / Tri";
    case GAMEPAD_BUTTON_LEFT_TRIGGER_1:   return "LB / L1";
    case GAMEPAD_BUTTON_RIGHT_TRIGGER_1:  return "RB / R1";
    case GAMEPAD_BUTTON_LEFT_TRIGGER_2:   return "LT / L2";
    case GAMEPAD_BUTTON_RIGHT_TRIGGER_2:  return "RT / R2";
    case GAMEPAD_BUTTON_LEFT_FACE_UP:     return "D-Up";
    case GAMEPAD_BUTTON_LEFT_FACE_DOWN:   return "D-Down";
    case GAMEPAD_BUTTON_LEFT_FACE_LEFT:   return "D-Left";
    case GAMEPAD_BUTTON_LEFT_FACE_RIGHT:  return "D-Right";
    case GAMEPAD_BUTTON_MIDDLE_LEFT:      return "Select";
    case GAMEPAD_BUTTON_MIDDLE:           return "Home";
    case GAMEPAD_BUTTON_MIDDLE_RIGHT:     return "Start";
    case GAMEPAD_BUTTON_LEFT_THUMB:       return "L3";
    case GAMEPAD_BUTTON_RIGHT_THUMB:      return "R3";
    default:                              return "---";
    }
}

// Polls gamepad 0 each frame and maps buttons to in-game actions.
// Works identically for Xbox and PlayStation controllers on Windows and web
// because browsers normalise all controllers to the HTML5 standard layout,
// which matches Raylib's button enum order exactly.
struct GamepadInput
{
    // Per-frame outputs
    Vector2 moveDir{};
    bool    attackPressed    = false;
    bool    dashPressed      = false;
    bool    abilityPressed[4]{};
    bool    pausePressed        = false;
    bool    backPressed         = false;
    bool    menuConfirmPressed  = false;  // always A / Cross — not remappable
    bool    isActive            = false;

    static constexpr float kDeadZone = 0.2f;
    static constexpr int   kGamepad  = 0;

    void Update(const GamepadBindings& bindings);
};
