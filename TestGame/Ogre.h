#pragma once

#include "Enemy.h"

#include <vector>
#include <memory>

class Ogre : public Enemy
{
public:
    Ogre(Vector2 pos);
    ~Ogre() override;

    void Init();
    void ResetForSpawn(Vector2 pos) override;
    static void UnloadSharedResources();

    // Ogre stays in the shared enemy pool, but overrides update so it can
    // manage its charge, rush, and recovery states.
    void Update(float dt, Vector2 heroWorldPos, Vector2 navigationTarget, bool hasNavigationTarget,
                const std::vector<std::unique_ptr<Enemy>>& enemies,
                const std::vector<Vector2>& propCenters) override;

    void SetWaveScale(int wave) override;
    void DrawEnemy(Vector2 cameraRef) override;
    void TakeDamage(int damage, Vector2 attackerPos) override;
    Rectangle GetCollisionRec() const override;
    Ogre* AsOgre() override { return this; }
    void PlayHurtSound() override;
    void PlayDeathSound() override;

    bool IsRushing() const { return _rushState == RushState::Rushing; }
    void OnRushBlocked();
    bool ConsumeImpactShakeRequest();

private:
    enum class RushState
    {
        Repositioning,
        Charging,
        Rushing,
        Stunned
    };

    static void EnsureSharedResourcesLoaded();

    void SetIdleAnimation(bool resetFrame);
    void SetWalkAnimation(bool resetFrame);
    void SetRushAnimation(bool resetFrame);
    void SetHurtAnimation(bool resetFrame);
    void SetDeathAnimation(bool resetFrame);

    void HandleRepositioning(float dt, Vector2 navigationTarget, bool hasNavigationTarget);
    void HandleCharging(float dt);
    void HandleRush(float dt, const std::vector<std::unique_ptr<Enemy>>& enemies);
    void HandleAnimation(float dt);
    void DrawHealthBar(Vector2 screenPos, float w, float h);
    void ScatterEnemy(Enemy& enemy) const;
    bool HasAlreadyRushedEnemy(const Enemy* enemy) const;
    void FinishRush(bool stunnedOnImpact);
    void PlayRushHitSound() const;
    void PlayWallHitSound() const;

    RushState _rushState = RushState::Repositioning;

    float _facingTimer = 0.f;
    float _chargeTimer = 0.f;
    float _rushCooldown = 0.f;
    float _stunTimer = 0.f;
    bool _impactShakeRequested = false;
    Vector2 _rushDirection{};
    std::vector<const Enemy*> _rushedEnemies;

    // Ogre sheets are authored as six columns across the texture.
    static constexpr int   _sheetFrameCount = 6;
    static constexpr int   _playbackFrameCount = 6;
    static constexpr int   _walkPlaybackFrameCount = 4;
    static constexpr float _walkSheetFrameCrop = 1.f;

    // Even though the source frame size changes per sheet, the ogre should
    // occupy one stable visual footprint on screen. These constants describe
    // that shared draw box so the sprite does not appear to slide around when
    // moving between idle, walk, rush, hurt, and death sheets.
    static constexpr float _visualFrameWidth = 45.f;
    static constexpr float _visualFrameHeight = 25.f;
    static constexpr float _walkVisualOffsetX = -4.f;
    static constexpr int   _rushGhostCount = 3;
    static constexpr float _rushGhostSpacing = 18.f;
    static constexpr float _rushGhostAlphaFalloff = 0.18f;

    // Collision should match the ogre's lower body rather than the full sprite
    // width, otherwise the rush can clip props and the hurtbox feels too large.
    static constexpr float _collisionWidthScale = 0.42f;
    static constexpr float _collisionHeightScale = 0.38f;
    static constexpr float _collisionYOffsetScale = 0.28f;

    // Charge begins one player-length farther out than the cyclops range so
    // the ogre has time to telegraph before it launches.
    static constexpr float _chargeRange = 480.f + 192.f;
    static constexpr float _chargeDuration = 3.0f;
    static constexpr float _rushSpeed = 750.f;
    static constexpr float _chargeCooldownDuration = 6.0f;
    static constexpr float _stunDuration = 0.6f;
    static constexpr float _playerPushSpeed = 1200.f;
    static constexpr float _rushDamage = 1.f;
    static constexpr float _walkSpeed = 175.f;
    static constexpr float _scatterImpulse = 1850.f;


    static Texture2D _sharedIdleAnim;
    static Texture2D _sharedWalkAnim;
    static Texture2D _sharedAttackAnim;
    static Texture2D _sharedTakeDamageAnim;
    static Texture2D _sharedDeathAnim;
    static Sound _sharedRushHitSound;
    static Sound _sharedWallHitSound;
    static Sound _sharedOgreHurtSound;
    static Sound _sharedOgreDeathSound;
    static bool _sharedResourcesLoaded;
};
