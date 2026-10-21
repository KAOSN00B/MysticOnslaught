#include "Shieldbearer.h"
#include "SoundVariation.h"
#include "VirtualCanvas.h"
#include "AssetPaths.h"
#include <algorithm>
#include <cmath>

Texture2D Shieldbearer::_sharedIdleAnim[Shieldbearer::kVariantCount]{};
Texture2D Shieldbearer::_sharedWalkAnim[Shieldbearer::kVariantCount]{};
Texture2D Shieldbearer::_sharedAttackAnim[Shieldbearer::kVariantCount]{};
Texture2D Shieldbearer::_sharedHurtAnim[Shieldbearer::kVariantCount]{};
Texture2D Shieldbearer::_sharedDeathAnim[Shieldbearer::kVariantCount]{};
Sound     Shieldbearer::_sharedAttackSound{};
Sound     Shieldbearer::_sharedHurtSound{};
Sound     Shieldbearer::_sharedDeathSound{};
Sound     Shieldbearer::_sharedBlockSound{};
bool      Shieldbearer::_sharedResourcesLoaded = false;

namespace
{
    constexpr float kShieldbearerFrameWidth = 48.f;
    // Tier ladder: brown -> black -> red (pack letters A, C, D).
    const char* kShieldbearerVariantSuffixes[3] = { "", "_C", "_D" };
}

Shieldbearer::Shieldbearer(Vector2 pos)
    : Enemy(pos)
{
}

Shieldbearer::~Shieldbearer() {}

void Shieldbearer::Init()
{
    EnsureSharedResourcesLoaded();

    _attackSound = _sharedAttackSound;
    _hurtSound   = _sharedHurtSound;
    _deathSound  = _sharedDeathSound;
    SetVariantTier(_variantTier);

    ResetForSpawn(_worldPos);
}

void Shieldbearer::SetVariantTier(int tier)
{
    EnsureSharedResourcesLoaded();

    _variantTier = (tier < 0) ? 0 : (tier >= kVariantCount) ? kVariantCount - 1 : tier;

    _idleAnim       = _sharedIdleAnim[_variantTier];
    _walkAnim       = _sharedWalkAnim[_variantTier];
    _attackAnim     = _sharedAttackAnim[_variantTier];
    _takeDamageAnim = _sharedHurtAnim[_variantTier];
    _deathAnim      = _sharedDeathAnim[_variantTier];

    _texture = _idleAnim;
    _height  = _texture.height;
    if (_width > 0.f)
        _maxFrames = (int)(_texture.width / _width);
}

void Shieldbearer::ResetForSpawn(Vector2 pos)
{
    // Full base grunt behaviour; only sprite metrics and stats change.
    Enemy::ResetForSpawn(pos);

    _width  = kShieldbearerFrameWidth;
    _scale  = 4.6f;
    _speed  = 150.f;   // slow, inevitable advance
    _health = Balance::Fodder::kShieldbearerHealth;
    _maxHealth = Balance::Fodder::kShieldbearerHealth;
    _attackPower = 10.f;
    _expValue = 4;     // blocking makes it worth more than a plain grunt

    _capsuleRadius     = 28.f;
    _capsuleHalfHeight = 12.f;
    _capsuleOffset     = { 0.f, 6.f };
    _attackRange       = 105.f;
    _attackBoxWidth    = 50.f;
    _attackBoxHeight   = 70.f;
    _attackBoxOffsetX  = 50.f;

    _blockFlashStart   = kNoBlockFlash;
    _guard             = 0.f;
    _guardRecoverDelay = 0.f;
    _guardBreakTimer   = 0.f;
    _shieldRaiseTimer  = 0.f;
    // Heavy shield unit: pivots slowly (see Balance::Facing), so circling behind
    // the shield is a real tactic instead of a frame-perfect race.
    _turnCommitInterval = Balance::Facing::kHeavyTurnCommitInterval;

    _texture   = _idleAnim;
    _height    = _texture.height;
    _maxFrames = (int)(_texture.width / _width);
    _frame     = GetRandomValue(0, _maxFrames - 1);

    ApplyStoredTuning();
}

void Shieldbearer::Update(float dt, Vector2 heroWorldPos, Vector2 navigationTarget, bool hasNavigationTarget,
    const std::vector<std::unique_ptr<Enemy>>& enemies, const std::vector<Vector2>& propCenters)
{
    if (_isActive && !_dying)
    {
        if (_guardBreakTimer > 0.f)
            _guardBreakTimer = std::max(0.f, _guardBreakTimer - dt);

        // The shield is down for the whole swing and comes back up a moment after.
        if (_attacking)
            _shieldRaiseTimer = Balance::ShieldGuard::kRaiseAfterSwingSeconds;
        else if (_shieldRaiseTimer > 0.f)
            _shieldRaiseTimer = std::max(0.f, _shieldRaiseTimer - dt);

        // After a spell without blocks, the guard meter drains.
        if (_guardRecoverDelay > 0.f)
            _guardRecoverDelay = std::max(0.f, _guardRecoverDelay - dt);
        else if (_guard > 0.f)
            _guard = std::max(0.f, _guard - GuardCapacity() * Balance::ShieldGuard::kRecoverPerSecond * dt);
    }

    Enemy::Update(dt, heroWorldPos, navigationTarget, hasNavigationTarget, enemies, propCenters);
}

bool Shieldbearer::IsShieldRaised() const
{
    return !_dying && IsAlive() && _guardBreakTimer <= 0.f && !_attacking && _shieldRaiseTimer <= 0.f;
}

float Shieldbearer::GuardCapacity() const
{
    return _maxHealth * Balance::ShieldGuard::kBreakAtMaxHpFraction;
}

float Shieldbearer::GetGuardFraction() const
{
    const float capacity = GuardCapacity();
    return (capacity > 0.f) ? std::min(1.f, _guard / capacity) : 0.f;
}

void Shieldbearer::TakeDamage(int damage, Vector2 attackerPos)
{
    // The raised shield blocks hits arriving inside the frontal cone
    // (dot-product check — see Balance::Facing), judged from where the hit
    // arrives: a shot from the way it travelled, a blast from its centre.
    // Hits from behind or the exposed sides, or while the shield is down,
    // get through in full.
    if (IsShieldRaised() && IsPositionInFront(HitArrivalPos(attackerPos), Balance::Facing::kFrontConeDot))
    {
        _guard += (float)std::max(0, damage);
        _guardRecoverDelay = Balance::ShieldGuard::kRecoverDelaySeconds;
        if (_guard < GuardCapacity())
        {
            _blockFlashStart = GetTime();
            float pitch = GetRandomValue(90, 120) / 100.f;
            SetSoundPitch(_sharedBlockSound, pitch);
            SetSoundVolume(_sharedBlockSound, 0.5f);
            PlaySound(_sharedBlockSound);
            // Tell the hit code the shield ate this hit so it shows a "BLOCKED"
            // callout (attack from behind) instead of nothing.
            _hitBlock = HitBlockReason::Blocked;
            return;   // fully blocked
        }

        // The meter is full: the guard breaks and this hit lands. The arrival
        // hold keeps it from moving or attacking while it is staggered.
        _guard             = 0.f;
        _guardRecoverDelay = 0.f;
        _guardBreakTimer   = Balance::ShieldGuard::kBreakStaggerSeconds;
        BeginArrivalDelay(Balance::ShieldGuard::kBreakStaggerSeconds);
        SfxBank::Get().Play(SfxId::ArmorHeavy, kGuardBreakVolume, true);
    }

    Enemy::TakeDamage(damage, attackerPos);
}

void Shieldbearer::DrawEnemy(Vector2 heroWorldPos)
{
    Enemy::DrawEnemy(heroWorldPos);
    if (!_isActive)
        return;

    Vector2 screenPos = Vector2{ _worldPos.x - heroWorldPos.x + kVirtualWidth / 2.f,
                                 _worldPos.y - heroWorldPos.y + kVirtualHeight / 2.f };
    DrawGuardBar(screenPos);

    // Shield spark: bright arc on the facing side so blocks read instantly.
    // It fades by the time since the block, so drawing never changes the enemy.
    const float sinceBlock = (float)(GetTime() - _blockFlashStart);
    if (sinceBlock >= kBlockFlashSeconds)
        return;

    float alpha = 1.f - sinceBlock / kBlockFlashSeconds;
    float sparkX = screenPos.x + _rightLeft * 52.f;
    DrawCircleV(Vector2{ sparkX, screenPos.y }, 22.f, Fade(Color{ 255, 240, 150, 255 }, 0.55f * alpha));
    DrawCircleV(Vector2{ sparkX, screenPos.y }, 10.f, Fade(WHITE, 0.75f * alpha));
}

// A thin bar under the health bar: steel while the guard meter fills, orange
// (the stagger time left) while the guard is broken, with a short GUARD BREAK
// callout above it. Reads the timers only.
void Shieldbearer::DrawGuardBar(Vector2 screenPos) const
{
    if (_dying)
        return;
    const bool broken = IsGuardBroken();
    const float fill = broken ? _guardBreakTimer / Balance::ShieldGuard::kBreakStaggerSeconds
                              : GetGuardFraction();
    if (fill <= 0.f)
        return;

    // Same anchor as Enemy::DrawHealthBar (just above the body), one bar lower.
    const Capsule2D cap = GetCapsule();
    const float bodyTopRelY = (cap.center.y - _worldPos.y) - cap.halfHeight - cap.radius;
    const float barWidth = _width * _scale * kGuardBarWidthShare;
    const float barX = screenPos.x - barWidth * 0.5f;
    const float barY = screenPos.y + bodyTopRelY - _healthBarYOffset + _healthBarHeight + kGuardBarGap;
    DrawRectangle((int)barX, (int)barY, (int)barWidth, (int)kGuardBarHeight, kGuardBackColor);
    DrawRectangle((int)barX, (int)barY, (int)(barWidth * fill), (int)kGuardBarHeight,
                  broken ? kGuardBrokenColor : kGuardFillColor);

    const float sinceBreak = Balance::ShieldGuard::kBreakStaggerSeconds - _guardBreakTimer;
    if (!broken || sinceBreak >= kGuardLabelSeconds)
        return;
    static const Font font = GetFontDefault();
    const char* text = "GUARD BREAK";
    const Vector2 size = MeasureTextEx(font, text, kGuardLabelFontSize, kGuardLabelSpacing);
    const Vector2 labelPos{ screenPos.x - size.x * 0.5f, barY - _healthBarHeight - kGuardLabelRise };
    const float alpha = 1.f - sinceBreak / kGuardLabelSeconds;
    DrawTextEx(font, text, { labelPos.x + kGuardLabelShadowPx, labelPos.y + kGuardLabelShadowPx },
               kGuardLabelFontSize, kGuardLabelSpacing, Fade(BLACK, kGuardLabelShadowAlpha * alpha));
    DrawTextEx(font, text, labelPos, kGuardLabelFontSize, kGuardLabelSpacing, Fade(kGuardBrokenColor, alpha));
}

Rectangle Shieldbearer::GetCollisionRec() const
{
    Rectangle animBodyRect;
    if (GetAnimBodyRectWorld(animBodyRect))
        return animBodyRect;
    if (_hasTunedCollision)
        return GetTunedCollisionRec();

    return Rectangle{ _worldPos.x - 38.f, _worldPos.y - 46.f + 10.f, 76.f, 92.f };
}

void Shieldbearer::SetWaveScale(int wave)
{
    (void)wave;
    _expValue    = 4;
    _health      = Balance::Fodder::kShieldbearerHealth;
    _maxHealth   = Balance::Fodder::kShieldbearerHealth;
    _speed       = 150.f;
    _attackPower = 10.f;
}

void Shieldbearer::PlayAttackSound()
{
    float pitch = SoundVariation::VariedPitch(0.875f);
    SetSoundPitch(_attackSound, pitch);
    SetSoundVolume(_attackSound, 0.65f);
    PlaySound(_attackSound);
}

void Shieldbearer::PlayDeathSound()
{
    // Armored orc roars on death — deeper than a basic beast, like a heavy guardian
    SfxBank::Get().Play(SfxId::BossRoar, 0.50f, true);
}

void Shieldbearer::EnsureSharedResourcesLoaded()
{
    if (_sharedResourcesLoaded)
        return;

    for (int variant = 0; variant < kVariantCount; variant++)
    {
        const char* suffix = kShieldbearerVariantSuffixes[variant];
        _sharedIdleAnim[variant]   = LoadTexture(AssetPath(TextFormat("Enemy/ShieldbearerIdle%s.png",   suffix)).c_str());
        _sharedWalkAnim[variant]   = LoadTexture(AssetPath(TextFormat("Enemy/ShieldbearerWalk%s.png",   suffix)).c_str());
        _sharedAttackAnim[variant] = LoadTexture(AssetPath(TextFormat("Enemy/ShieldbearerAttack%s.png", suffix)).c_str());
        _sharedHurtAnim[variant]   = LoadTexture(AssetPath(TextFormat("Enemy/ShieldbearerHurt%s.png",   suffix)).c_str());
        _sharedDeathAnim[variant]  = LoadTexture(AssetPath(TextFormat("Enemy/ShieldbearerDeath%s.png",  suffix)).c_str());
    }
    _sharedAttackSound = LoadSound(AssetPath("Sounds/SwordSwipe2.ogg").c_str());
    _sharedHurtSound   = LoadSound(AssetPath("Sounds/SmallMonsterDamage.ogg").c_str());
    _sharedDeathSound  = LoadSound(AssetPath("Sounds/PlayerDeath.ogg").c_str());
    _sharedBlockSound  = LoadSound(AssetPath("Sounds/OgreHitWall.ogg").c_str());
    _sharedResourcesLoaded = true;
}

void Shieldbearer::UnloadSharedResources()
{
    if (!_sharedResourcesLoaded)
        return;

    for (int variant = 0; variant < kVariantCount; variant++)
    {
        UnloadTexture(_sharedIdleAnim[variant]);
        UnloadTexture(_sharedWalkAnim[variant]);
        UnloadTexture(_sharedAttackAnim[variant]);
        UnloadTexture(_sharedHurtAnim[variant]);
        UnloadTexture(_sharedDeathAnim[variant]);
        _sharedIdleAnim[variant]   = Texture2D{};
        _sharedWalkAnim[variant]   = Texture2D{};
        _sharedAttackAnim[variant] = Texture2D{};
        _sharedHurtAnim[variant]   = Texture2D{};
        _sharedDeathAnim[variant]  = Texture2D{};
    }
    UnloadSound(_sharedAttackSound);
    UnloadSound(_sharedHurtSound);
    UnloadSound(_sharedDeathSound);
    UnloadSound(_sharedBlockSound);
    _sharedAttackSound = Sound{};
    _sharedHurtSound   = Sound{};
    _sharedDeathSound  = Sound{};
    _sharedBlockSound  = Sound{};
    _sharedResourcesLoaded = false;
}
