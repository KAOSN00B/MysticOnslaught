#include "GamepadInput.h"
#include "raymath.h"
#include <cmath>

void GamepadInput::Update(const GamepadBindings& bindings)
{
    for (int i = 0; i < 4; i++) abilityPressed[i] = false;
    attackPressed = false;
    dashPressed   = false;
    pausePressed  = false;
    moveDir       = Vector2Zero();

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

    attackPressed = IsGamepadButtonPressed(kGamepad, bindings.attack);
    dashPressed   = IsGamepadButtonPressed(kGamepad, bindings.dash);
    for (int i = 0; i < 4; i++)
        abilityPressed[i] = IsGamepadButtonPressed(kGamepad, bindings.ability[i]);
    pausePressed = IsGamepadButtonPressed(kGamepad, bindings.pause);
}
