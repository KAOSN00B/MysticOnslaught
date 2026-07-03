#pragma once

#include "raylib.h"
#include <string>

// ─────────────────────────────────────────────────────────────────────────────
// CharacterTuning — per-character overrides authored in the Character Animator
// (main menu dev tool). Each tunable enemy loads its file once and applies it
// at the end of ResetForSpawn, so edits survive restarts without recompiling.
//
// File: charactertuning_<Name>.txt (key=value), saved next to the executable —
// same pattern as the tilemapper_<biome>.txt files.
// ─────────────────────────────────────────────────────────────────────────────
struct CharacterTuning
{
    static constexpr int kMaxAnims = 10;

    bool  hasScale = false;
    float scale    = 4.f;

    bool      hasCollision = false;
    Rectangle collisionRel{};        // solid hitbox relative to the enemy worldPos

    bool    hasCapsule = false;
    float   capsuleRadius     = 0.f;
    float   capsuleHalfHeight = 0.f;
    Vector2 capsuleOffset{};

    bool  hasAttackBox = false;
    float attackBoxWidth   = 0.f;
    float attackBoxHeight  = 0.f;
    float attackBoxOffsetX = 0.f;
    float attackBoxOffsetY = 0.f;

    // Per-animation frame time overrides; <= 0 keeps the class default.
    float animFrameTime[kMaxAnims] = {};

    // ── Per-animation hitboxes (authored facing right, relative to worldPos) ──
    struct AnimBody   { bool set = false; float x = 0.f, y = 0.f, radius = 0.f; };
    struct AnimMelee  { bool set = false; Rectangle rect{}; };
    struct AnimDraw   { bool set = false; float x = 0.f, y = 0.f; };
    AnimBody  animBody[kMaxAnims];    // body circle: solid capsule + hurt box
    AnimMelee animMelee[kMaxAnims];   // melee swing box
    AnimDraw  animDraw[kMaxAnims];    // sprite-only draw offset
};

namespace CharacterTuningStore
{
    // Returns nullptr when no tuning file exists for this character.
    // Results are cached; Reload() drops the cache entry after a save/delete.
    const CharacterTuning* Get(const std::string& characterName);

    void Save(const std::string& characterName, const CharacterTuning& tuning);
    void Reload(const std::string& characterName);
    bool Delete(const std::string& characterName);
}
