#include "Sporeling.h"
#include "VirtualCanvas.h"
#include "AssetPaths.h"

Texture2D Sporeling::_sharedIdleAnim[Sporeling::kVariantCount]{};
Texture2D Sporeling::_sharedWalkAnim[Sporeling::kVariantCount]{};
Texture2D Sporeling::_sharedAttackAnim[Sporeling::kVariantCount]{};
Texture2D Sporeling::_sharedHurtAnim[Sporeling::kVariantCount]{};
Texture2D Sporeling::_sharedDeathAnim[Sporeling::kVariantCount]{};
Sound     Sporeling::_sharedAttackSound{};
Sound     Sporeling::_sharedHurtSound{};
Sound     Sporeling::_sharedDeathSound{};
bool      Sporeling::_sharedResourcesLoaded = false;

namespace
{
    constexpr float kSporelingFrameWidth = 48.f;
    // Tier ladder: red cap -> green cap -> purple cap (pack letters A, D, C).
    const char* kSporelingVariantSuffixes[3] = { "", "_D", "_C" };
}

Sporeling::Sporeling(Vector2 pos)
    : Enemy(pos)
{
}

Sporeling::~Sporeling() {}

void Sporeling::Init()
{
    EnsureSharedResourcesLoaded();

    _attackSound = _sharedAttackSound;
    _hurtSound   = _sharedHurtSound;
    _deathSound  = _sharedDeathSound;
    SetVariantTier(_variantTier);

    ResetForSpawn(_worldPos);
}

void Sporeling::SetVariantTier(int tier)
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

void Sporeling::ResetForSpawn(Vector2 pos)
{
    // Full base grunt behaviour; only the sprite metrics and stats change.
    Enemy::ResetForSpawn(pos);

    _width  = kSporelingFrameWidth;
    _scale  = 4.4f;
    _speed  = 115.f;   // slow — the threat is the death cloud, not the chase
    _health = 4.f;
    _maxHealth = 4.f;
    _attackPower = 1.f;
    _expValue = 3;

    _capsuleRadius     = 26.f;
    _capsuleHalfHeight = 8.f;
    _capsuleOffset     = { 0.f, 8.f };
    _attackRange       = 95.f;
    _attackBoxWidth    = 44.f;
    _attackBoxHeight   = 60.f;
    _attackBoxOffsetX  = 44.f;

    _texture   = _idleAnim;
    _height    = _texture.height;
    _maxFrames = (int)(_texture.width / _width);
    _frame     = GetRandomValue(0, _maxFrames - 1);

    ApplyStoredTuning();
}

Rectangle Sporeling::GetCollisionRec() const
{
    Rectangle animBodyRect;
    if (GetAnimBodyRectWorld(animBodyRect))
        return animBodyRect;
    if (_hasTunedCollision)
        return GetTunedCollisionRec();

    return Rectangle{ _worldPos.x - 36.f, _worldPos.y - 40.f + 12.f, 72.f, 80.f };
}

void Sporeling::SetWaveScale(int wave)
{
    (void)wave;
    _expValue    = 3;
    _health      = 4.f;
    _maxHealth   = 4.f;
    _speed       = 115.f;
    _attackPower = 1.f;
}

void Sporeling::PlayAttackSound()
{
    float pitch = GetRandomValue(80, 110) / 100.f;
    SetSoundPitch(_attackSound, pitch);
    SetSoundVolume(_attackSound, 0.55f);
    PlaySound(_attackSound);
}

void Sporeling::EnsureSharedResourcesLoaded()
{
    if (_sharedResourcesLoaded)
        return;

    for (int variant = 0; variant < kVariantCount; variant++)
    {
        const char* suffix = kSporelingVariantSuffixes[variant];
        _sharedIdleAnim[variant]   = LoadTexture(AssetPath(TextFormat("Enemy/SporelingIdle%s.png",   suffix)).c_str());
        _sharedWalkAnim[variant]   = LoadTexture(AssetPath(TextFormat("Enemy/SporelingWalk%s.png",   suffix)).c_str());
        _sharedAttackAnim[variant] = LoadTexture(AssetPath(TextFormat("Enemy/SporelingAttack%s.png", suffix)).c_str());
        _sharedHurtAnim[variant]   = LoadTexture(AssetPath(TextFormat("Enemy/SporelingHurt%s.png",   suffix)).c_str());
        _sharedDeathAnim[variant]  = LoadTexture(AssetPath(TextFormat("Enemy/SporelingDeath%s.png",  suffix)).c_str());
    }
    _sharedAttackSound = LoadSound(AssetPath("Sounds/SwordSwipe2.ogg").c_str());
    _sharedHurtSound   = LoadSound(AssetPath("Sounds/SmallMonsterDamage.ogg").c_str());
    _sharedDeathSound  = LoadSound(AssetPath("Sounds/PlayerDeath.ogg").c_str());
    _sharedResourcesLoaded = true;
}

void Sporeling::UnloadSharedResources()
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
    _sharedAttackSound = Sound{};
    _sharedHurtSound   = Sound{};
    _sharedDeathSound  = Sound{};
    _sharedResourcesLoaded = false;
}
