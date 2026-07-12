#include "GamepadInput.h"
#include "raymath.h"
#include <cmath>

void GamepadInput::Update(const GamepadBindings& bindings)
{
    for (int i = 0; i < 4; i++)
    {
        abilityPressed[i] = false;
        abilityDown[i] = false;
        abilityReleased[i] = false;
    }
    attackPressed       = false;
    dashPressed         = false;
    pausePressed        = false;
    backPressed         = false;
    menuConfirmPressed  = false;
    moveDir             = Vector2Zero();
    aimDir              = Vector2Zero();

    isActive = IsGamepadAvailable(kGamepad);
    if (!isActive) return;

    // Left stick with per-axis dead zone
    float axisX = GetGamepadAxisMovement(kGamepad, GAMEPAD_AXIS_LEFT_X);
    float axisY = GetGamepadAxisMovement(kGamepad, GAMEPAD_AXIS_LEFT_Y);
    if (std::abs(axisX) < kDeadZone) axisX = 0.f;
    if (std::abs(axisY) < kDeadZone) axisY = 0.f;
    moveDir = { axisX, axisY };
    if (Vector2Length(moveDir) > 1.f)
        moveDir = Vector2Normalize(moveDir);

    float aimX = GetGamepadAxisMovement(kGamepad, GAMEPAD_AXIS_RIGHT_X);
    float aimY = GetGamepadAxisMovement(kGamepad, GAMEPAD_AXIS_RIGHT_Y);
    if (std::abs(aimX) < kDeadZone) aimX = 0.f;
    if (std::abs(aimY) < kDeadZone) aimY = 0.f;
    aimDir = { aimX, aimY };
    if (Vector2Length(aimDir) > 1.f)
        aimDir = Vector2Normalize(aimDir);

    attackPressed = IsGamepadButtonPressed(kGamepad, bindings.attack);
    dashPressed   = IsGamepadButtonPressed(kGamepad, bindings.dash);
    for (int i = 0; i < 4; i++)
    {
        abilityPressed[i] = IsGamepadButtonPressed(kGamepad, bindings.ability[i]);
        abilityDown[i] = IsGamepadButtonDown(kGamepad, bindings.ability[i]);
        abilityReleased[i] = IsGamepadButtonReleased(kGamepad, bindings.ability[i]);
    }
    pausePressed       = IsGamepadButtonPressed(kGamepad, bindings.pause);
    backPressed        = IsGamepadButtonPressed(kGamepad, GAMEPAD_BUTTON_RIGHT_FACE_RIGHT);
    menuConfirmPressed = IsGamepadButtonPressed(kGamepad, GAMEPAD_BUTTON_RIGHT_FACE_DOWN);
}
