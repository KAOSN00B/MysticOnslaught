#pragma once
#include "raylib.h"

// Fixed logical resolution — all game drawing targets this canvas size.
// The Engine letterboxes it onto whatever window or screen the player has.
static constexpr int kVirtualWidth  = 1920;
static constexpr int kVirtualHeight = 1080;

struct LetterboxTransform
{
    float scale;
    float offsetX;
    float offsetY;
};

// Scale + offsets to fit kVirtualWidth x kVirtualHeight into the current window
// while preserving aspect ratio (black bars on wider/taller windows).
inline LetterboxTransform GetLetterboxTransform()
{
    float scaleX = (float)GetScreenWidth()  / (float)kVirtualWidth;
    float scaleY = (float)GetScreenHeight() / (float)kVirtualHeight;
    float scale  = (scaleX < scaleY) ? scaleX : scaleY;
    return {
        scale,
        ((float)GetScreenWidth()  - (float)kVirtualWidth  * scale) * 0.5f,
        ((float)GetScreenHeight() - (float)kVirtualHeight * scale) * 0.5f
    };
}

// Remap a raw screen-space position to virtual 1920x1080 canvas space.
// Always pass input through this before comparing against UI element positions.
inline Vector2 RemapToVirtual(Vector2 screenPos)
{
    LetterboxTransform lb = GetLetterboxTransform();
    return {
        (screenPos.x - lb.offsetX) / lb.scale,
        (screenPos.y - lb.offsetY) / lb.scale
    };
}

// Drop-in replacements for Raylib input — use these everywhere in the game
// so coordinates are always in virtual 1920x1080 space regardless of window size.
inline Vector2 GetVirtualMousePos()
{
    return RemapToVirtual(GetMousePosition());
}

inline Vector2 GetVirtualTouchPos(int index = 0)
{
    return RemapToVirtual(GetTouchPosition(index));
}
