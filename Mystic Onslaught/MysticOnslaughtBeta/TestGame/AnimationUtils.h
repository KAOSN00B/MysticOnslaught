#pragma once

#include "raylib.h"

inline Rectangle GetAnimationFrameRect(const Texture2D& texture, int frameWidth, int frameHeight, int frameIndex)
{
    int columns = frameWidth > 0 ? texture.width / frameWidth : 1;
    if (columns <= 0)
        columns = 1;

    int x = (frameIndex % columns) * frameWidth;
    int y = (frameIndex / columns) * frameHeight;

    return Rectangle{
        (float)x,
        (float)y,
        (float)frameWidth,
        (float)frameHeight
    };
}
