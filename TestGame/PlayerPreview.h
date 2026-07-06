#pragma once

#include "Enemy.h"

// ─────────────────────────────────────────────────────────────────────────────
// PlayerPreview — editor-only stand-in for the player in the Character Animator.
// It shows the hero Idle / Walk / Swing / Stab sheets and reuses Enemy's
// per-animation hitbox tooling, so you can author the player's HIT (melee) and
// HURT (body) colliders visually. It saves to charactertuning_Player.txt, which
// the live Character loads and applies at run start.
//
// It is never spawned in the actual game — only instantiated by CharacterAnimator.
// ─────────────────────────────────────────────────────────────────────────────
class PlayerPreview : public Enemy
{
public:
    explicit PlayerPreview(Vector2 pos = Vector2{ 0.f, 0.f });

    void Init();

    const char* GetTuningName() const override { return "Player"; }

    // Editor animation set: 0 Idle, 1 Walk, 2 Swing (attack), 3 Stab.
    int         GetEditorAnimCount() const override { return 4; }
    const char* GetEditorAnimName(int index) const override;
    void        PlayEditorAnim(int index) override;

private:
    static void EnsureLoaded();
    static Texture2D _sIdle, _sWalk, _sSwing, _sStab, _sHurt, _sDeath;
    static bool      _loaded;
};
