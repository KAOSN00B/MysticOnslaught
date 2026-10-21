#pragma once

#include "raylib.h"
#include "TuningFiles.h"
#include <string>
#include <vector>

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
    // Set by the animator's Size control: the game draws exactly `scale`.
    // Without it, standard enemies are sized to the player (Balance::Visual)
    // and `scale` only says what size the hitboxes were authored at.
    bool  sizeSet  = false;

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

    // ── Hit windows (Unreal "Anim Notify State") ──────────────────────────────
    // The frames an animation's melee box is live, first to last (0-based).
    // Unset = the class's own frames.
    struct AnimHit { bool set = false; int start = 0; int end = 0; };
    AnimHit animHit[kMaxAnims];

    // ── Animation events (Unity "Animation Events" / Unreal "Anim Notifies") ──
    // When an animation reaches `frame`, the game plays an FX sheet (FX_<fx>.png
    // at x,y, authored facing right, drawn at `size`), the character's attack
    // sound, or a screen shake of `size` strength.
    enum class EventKind { Fx, Sound, Shake };
    struct AnimEvent
    {
        EventKind   kind  = EventKind::Fx;
        int         frame = 0;
        float       x = 0.f, y = 0.f;
        float       size  = 0.f;
        std::string fx;
    };
    static constexpr int kMaxEventsPerAnim = 8;
    std::vector<AnimEvent> animEvents[kMaxAnims];

    // How far the hurtbox (what the player's attacks hit) reaches past the body
    // square, each side. Unset = Enemy's default padding.
    bool  hasHurtPad = false;
    float hurtPadX = 0.f, hurtPadY = 0.f;
};

// An animation event that just fired, placed in the world (Engine plays it).
struct FiredAnimEvent
{
    CharacterTuning::EventKind kind = CharacterTuning::EventKind::Fx;
    Vector2     worldPos{};
    float       size = 0.f;
    std::string fx;
};

namespace AnimEvents
{
    // Whether an event on `eventFrame` fires as an animation moves from
    // `lastFrame` to `frame`. A new animation, or the same one starting over,
    // fires from its first frame; otherwise every frame passed fires, so a
    // long frame step never skips one.
    inline bool Reached(int eventFrame, bool sameAnimation, int lastFrame, int frame)
    {
        const bool fromStart = !sameAnimation || frame < lastFrame;
        return fromStart ? eventFrame <= frame : (eventFrame > lastFrame && eventFrame <= frame);
    }
}

namespace CharacterTuningStore
{
    // Returns nullptr when no tuning file exists for this character.
    // Results are cached; Reload() drops the cache entry after a save/delete.
    const CharacterTuning* Get(const std::string& characterName);

    void Save(const std::string& characterName, const CharacterTuning& tuning);
    void Reload(const std::string& characterName);
    void ReloadAll();   // after TuningFiles::Directory() changes
    bool Delete(const std::string& characterName);

    // The file text Save writes, and the reader for it. Two tunings are the
    // same edit when their text matches (the animator's unsaved/undo checks).
    std::string Serialize(const CharacterTuning& tuning);
    void        Parse(const std::string& text, CharacterTuning& outTuning);
}
