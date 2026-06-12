#pragma once

#include "raylib.h"
#include <cmath>

#if defined(PLATFORM_WEB)
#include <emscripten/emscripten.h>

struct WebGamepadCache
{
    double lastSampleTime = -1.0;
    bool down[18] = {};
    bool pressed[18] = {};
};

inline WebGamepadCache gWebGamepadCache;

static inline WebGamepadCache& GetWebGamepadCache()
{
    return gWebGamepadCache;
}

static inline int RaylibButtonToBrowserButton(int button)
{
    switch (button)
    {
    case GAMEPAD_BUTTON_LEFT_FACE_UP:       return 12;
    case GAMEPAD_BUTTON_LEFT_FACE_RIGHT:    return 15;
    case GAMEPAD_BUTTON_LEFT_FACE_DOWN:     return 13;
    case GAMEPAD_BUTTON_LEFT_FACE_LEFT:     return 14;
    case GAMEPAD_BUTTON_RIGHT_FACE_UP:      return 3;
    case GAMEPAD_BUTTON_RIGHT_FACE_RIGHT:   return 1;
    case GAMEPAD_BUTTON_RIGHT_FACE_DOWN:    return 0;
    case GAMEPAD_BUTTON_RIGHT_FACE_LEFT:    return 2;
    case GAMEPAD_BUTTON_LEFT_TRIGGER_1:     return 4;
    case GAMEPAD_BUTTON_LEFT_TRIGGER_2:     return 6;
    case GAMEPAD_BUTTON_RIGHT_TRIGGER_1:    return 5;
    case GAMEPAD_BUTTON_RIGHT_TRIGGER_2:    return 7;
    case GAMEPAD_BUTTON_MIDDLE_LEFT:        return 8;
    case GAMEPAD_BUTTON_MIDDLE:             return 16;
    case GAMEPAD_BUTTON_MIDDLE_RIGHT:       return 9;
    case GAMEPAD_BUTTON_LEFT_THUMB:         return 10;
    case GAMEPAD_BUTTON_RIGHT_THUMB:        return 11;
    default:                                return -1;
    }
}

static inline bool WebGamepadApiAvailable()
{
    return EM_ASM_INT({
        var pads = (navigator.getGamepads && navigator.getGamepads()) || [];
        for (var i = 0; i < pads.length; i++) {
            if (pads[i] && pads[i].connected) return 1;
        }
        return 0;
    }) != 0;
}

static inline double WebGamepadButtonValue(int browserButton)
{
    if (browserButton < 0) return 0.0;
    return EM_ASM_DOUBLE({
        var buttonIndex = $0;
        var pads = (navigator.getGamepads && navigator.getGamepads()) || [];
        for (var i = 0; i < pads.length; i++) {
            var pad = pads[i];
            if (!pad || !pad.connected || !pad.buttons || buttonIndex >= pad.buttons.length) continue;
            var b = pad.buttons[buttonIndex];
            return b ? (b.value || (b.pressed ? 1.0 : 0.0)) : 0.0;
        }
        return 0.0;
    }, browserButton);
}

static inline double WebGamepadAxisValue(int axis)
{
    return EM_ASM_DOUBLE({
        var axisIndex = $0;
        var pads = (navigator.getGamepads && navigator.getGamepads()) || [];
        for (var i = 0; i < pads.length; i++) {
            var pad = pads[i];
            if (!pad || !pad.connected || !pad.axes || axisIndex >= pad.axes.length) continue;
            return pad.axes[axisIndex] || 0.0;
        }
        return 0.0;
    }, axis);
}

static inline void RefreshWebGamepadCache()
{
    WebGamepadCache& cache = GetWebGamepadCache();
    double now = GetTime();
    if (std::fabs(now - cache.lastSampleTime) < 0.004) return;

    for (int button = 0; button < 18; button++)
    {
        bool isDown = WebGamepadButtonValue(RaylibButtonToBrowserButton(button)) > 0.45;
        cache.pressed[button] = isDown && !cache.down[button];
        cache.down[button] = isDown;
    }
    cache.lastSampleTime = now;
}

static inline bool WebIsGamepadAvailable(int gamepad)
{
    (void)gamepad;
    return WebGamepadApiAvailable();
}

static inline bool WebIsGamepadButtonDown(int gamepad, GamepadButton button)
{
    (void)gamepad;
    RefreshWebGamepadCache();
    int idx = (int)button;
    return idx >= 0 && idx < 18 && GetWebGamepadCache().down[idx];
}

static inline bool WebIsGamepadButtonPressed(int gamepad, GamepadButton button)
{
    (void)gamepad;
    RefreshWebGamepadCache();
    int idx = (int)button;
    return idx >= 0 && idx < 18 && GetWebGamepadCache().pressed[idx];
}

static inline float WebGetGamepadAxisMovement(int gamepad, GamepadAxis axis)
{
    (void)gamepad;
    if (axis == GAMEPAD_AXIS_LEFT_TRIGGER)
        return (float)(WebGamepadButtonValue(6) * 2.0 - 1.0);
    if (axis == GAMEPAD_AXIS_RIGHT_TRIGGER)
        return (float)(WebGamepadButtonValue(7) * 2.0 - 1.0);
    return (float)WebGamepadAxisValue((int)axis);
}
#define IsGamepadAvailable WebIsGamepadAvailable
#define IsGamepadButtonDown WebIsGamepadButtonDown
#define IsGamepadButtonPressed WebIsGamepadButtonPressed
#define GetGamepadAxisMovement WebGetGamepadAxisMovement
#endif