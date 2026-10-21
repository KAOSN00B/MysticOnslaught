#pragma once

#include "raylib.h"

// Footstep-like pitch variation for repeated creature sounds (hurt, attack).
// Every play lands within +/-kPitchSpread of the clip's base pitch, so a swarm
// never repeats the exact same sample while each creature keeps its character
// (its base pitch). Subtle on purpose: wide random ranges sound like a
// different creature, not a variation of the same one.
namespace SoundVariation
{
    constexpr float kPitchSpread = 0.10f;

    inline float VariedPitch(float basePitch = 1.f)
    {
        return basePitch * (1.f + kPitchSpread * (float)GetRandomValue(-100, 100) / 100.f);
    }

    // Plays at a varied pitch without touching the clip's volume.
    inline void PlayPitchVaried(Sound& sound, float basePitch = 1.f)
    {
        SetSoundPitch(sound, VariedPitch(basePitch));
        PlaySound(sound);
    }
}
