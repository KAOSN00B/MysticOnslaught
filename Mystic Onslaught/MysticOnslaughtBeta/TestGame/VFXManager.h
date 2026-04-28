#pragma once
#include "raylib.h"
#include "Character.h"
#include <vector>

// ── VFXManager ────────────────────────────────────────────────────────────────
// Owns all animated sprite effects and floating damage/heal numbers.
// Engine owns the textures; VFXManager holds non-owning pointers to them.
// Engine calls:
//   _vfx.Init(...)                 once after textures are loaded
//   _vfx.Update(dt)                every frame from UpdateGamePlay
//   _vfx.Draw(offset, pos, cast)   every frame from DrawWorld
//   _vfx.DrawFloatingTexts(offset) every frame from DrawWorld (after Draw)
//   _vfx.Clear()                   on run reset / room change
// ─────────────────────────────────────────────────────────────────────────────
class VFXManager
{
public:
    // Call once after all VFX textures are loaded by Engine.
    void Init(Texture2D* fireballCastTex,  Texture2D* fireballHitTex,
              Texture2D* genericHitTex,    Texture2D* iceHitTex,
              Texture2D* lightningCastTex, Texture2D* healEffectTex);

    // Advance frame counters; remove finished effects.
    void Update(float dt);

    // Render animated sprite effects.
    // playerWorldPos / playerCastOrigin are the live positions for follow-player effects.
    void Draw(Vector2 worldOffset, Vector2 playerWorldPos, Vector2 playerCastOrigin);

    // Cull expired floating numbers then render remaining.
    void DrawFloatingTexts(Vector2 worldOffset);

    // Discard all active effects and floating texts (room reset / run reset).
    void Clear();

    // ── Spawn helpers ────────────────────────────────────────────────────────
    // Cast animation that follows the player's cast origin.
    void SpawnCastEffect(Character::CastType castType, Vector2 castOrigin, Vector2 facingDir);

    // Impact animation at a fixed world position.
    void SpawnHitEffect(Character::CastType castType, Vector2 worldPos, Vector2 direction);

    // Heal animation that follows the player's center.
    void SpawnHealEffect();

    // Floating damage / heal number that rises and fades.
    void SpawnFloatingText(Vector2 worldPos, int value, Color color);

private:
    struct AnimatedEffect
    {
        Texture2D* texture        = nullptr;
        Vector2    worldPos       = {};
        Vector2    offset         = {};
        Vector2    direction      = { 1.f, 0.f };
        Color      tint           = WHITE;
        float      scale          = 4.f;
        float      frameTime      = 1.f / 18.f;
        float      runningTime    = 0.f;
        int        frameWidth     = 32;
        int        frameHeight    = 32;
        int        frameCount     = 1;
        int        frame          = 0;
        bool       followPlayer      = false;
        bool       followPlayerCenter = false;
        bool       active         = false;
    };

    struct FloatingText
    {
        Vector2 worldPos  = {};
        int     value     = 0;
        Color   color     = WHITE;
        float   spawnTime = 0.f;
        static constexpr float kLifetime = 0.75f;
    };

    std::vector<AnimatedEffect> _effects;
    std::vector<FloatingText>   _floatingTexts;

    // Non-owning pointers — Engine is responsible for load/unload.
    Texture2D* _fireballCastTex  = nullptr;
    Texture2D* _fireballHitTex   = nullptr;
    Texture2D* _genericHitTex    = nullptr;
    Texture2D* _iceHitTex        = nullptr;
    Texture2D* _lightningCastTex = nullptr;
    Texture2D* _healEffectTex    = nullptr;
};
