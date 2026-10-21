#pragma once

// =============================================================================
// DayNight.h - the time of day and how the world looks at each hour.
//
// The clock always runs while the game is played (village and dungeon, indoors
// or out) and never shows on screen. Outdoor places, the village and the
// outdoor biomes, take the look of the hour: a soft gold morning, the art as
// drawn by day, an orange evening and a blue night where fire, spells and the
// player light the ground. Indoor biomes keep their own art. Vfx::Night()
// (VfxNight.h) draws the look; the hours and colours are in Balance::DayNight.
// This file is plain logic (no drawing), so the tests can check it directly.
// =============================================================================

#include "GameTypes.h"   // Biome

namespace DayNight
{
    // How the world looks at one moment. Colours are 0-1.
    struct Look
    {
        float tint[3]    = { 1.f, 1.f, 1.f };   // multiplies the scene
        float saturation = 1.f;                 // 1 = the art's colours, 0 = grey
        float lights     = 0.f;                 // how strongly lights show (0 by day)
        float haze[3]    = { 0.f, 0.f, 0.f };   // colour laid over everything
        float hazeAmount = 0.f;

        // True when the look leaves the art exactly as drawn, so the whole pass
        // can be skipped.
        bool IsPlain() const;
    };

    // Moves the clock on by `seconds` of play at `speed` x real time and wraps
    // it into [0, 24).
    float AdvanceClock(float hours, float seconds, float speed = 1.f);

    // The look at `hours`, blended smoothly between the two nearest keyframes
    // of Balance::DayNight::kLooks (wrapping past midnight).
    Look LookAt(float hours);

    // Outdoor biomes take the time of day; the rest keep their own lighting.
    bool IsOutdoorBiome(Biome biome);
}
