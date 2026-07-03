#pragma once

#include "Enemy.h"
#include <vector>
#include <memory>

// =============================================================================
// SlimeEnemy — classic splitting slime in two sizes.
// Big: slow, tanky, lunges like a regular grunt. When it dies it splits into
//      two Small slimes (spawned by CombatDirector via the death context).
// Small: fast, fragile, swarms the player.
// Movement, attacks, and pathfinding all reuse the base Enemy logic — only the
// sprites, stats, and the split-on-death flag differ.
// =============================================================================
enum class SlimeSize { Big, Small };

class SlimeEnemy : public Enemy
{
public:
    SlimeEnemy(Vector2 pos, SlimeSize size);
    ~SlimeEnemy() override;

    void Init();
    void ResetForSpawn(Vector2 pos) override;
    static void UnloadSharedResources();

    void SetWaveScale(int wave) override;

    // The base Enemy collision math assumes 32px grunt frames; the big slime
    // uses 48px frames, so both sizes compute their own centred rect here.
    Rectangle GetCollisionRec() const override;

    SlimeEnemy* AsSlime() override { return this; }
    SlimeSize   GetSize() const { return _size; }
    bool        IsBig()   const { return _size == SlimeSize::Big; }

    void PlayAttackSound() override;

private:
    static void EnsureSharedResourcesLoaded();

    SlimeSize _size = SlimeSize::Big;

    // Big and small variants use separate stitched sprite strips.
    static Texture2D _sharedBigIdleAnim;
    static Texture2D _sharedBigWalkAnim;
    static Texture2D _sharedBigAttackAnim;
    static Texture2D _sharedBigHurtAnim;
    static Texture2D _sharedBigDeathAnim;
    static Texture2D _sharedSmallIdleAnim;
    static Texture2D _sharedSmallWalkAnim;
    static Texture2D _sharedSmallAttackAnim;
    static Texture2D _sharedSmallHurtAnim;
    static Texture2D _sharedSmallDeathAnim;
    static Sound     _sharedAttackSound;
    static Sound     _sharedHurtSound;
    static Sound     _sharedDeathSound;
    static bool      _sharedResourcesLoaded;
};
