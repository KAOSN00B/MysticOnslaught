#include "Warchief.h"
#include "VirtualCanvas.h"
#include "AssetPaths.h"
#include "raymath.h"
#include <cmath>

Texture2D Warchief::_sharedIdleAnim[Warchief::kVariantCount]{};
Texture2D Warchief::_sharedWalkAnim[Warchief::kVariantCount]{};
Texture2D Warchief::_sharedAttackAnim[Warchief::kVariantCount]{};
Texture2D Warchief::_sharedHurtAnim[Warchief::kVariantCount]{};
Texture2D Warchief::_sharedDeathAnim[Warchief::kVariantCount]{};
Sound     Warchief::_sharedAttackSound{};
Sound     Warchief::_sharedHurtSound{};
Sound     Warchief::_sharedDeathSound{};
bool      Warchief::_sharedResourcesLoaded = false;

namespace
{
    constexpr float kWarchiefFrameWidth = 48.f;
    // Tier ladder: brown -> black -> red (pack letters A, C, D).
    const char* kWarchiefVariantSuffixes[3] = { "", "_C", "_D" };
}

Warchief::Warchief(Vector2 pos)
    : Enemy(pos)
{
}

Warchief::~Warchief() {}

void Warchief::Init()
{
    EnsureSharedResourcesLoaded();

    _attackSound = _sharedAttackSound;
    _hurtSound   = _sharedHurtSound;
    _deathSound  = _sharedDeathSound;
    SetVariantTier(_variantTier);

    ResetForSpawn(_worldPos);
}

void Warchief::SetVariantTier(int tier)
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

void Warchief::ResetForSpawn(Vector2 pos)
{
    Enemy::ResetForSpawn(pos);

    _width  = kWarchiefFrameWidth;
    _scale  = 4.8f;
    _speed  = 175.f;
    _health = 6.f;
    _maxHealth = 6.f;
    _attackPower = 1.f;
    _expValue = 6;   // priority target — rewarded accordingly

    _capsuleRadius     = 30.f;
    _capsuleHalfHeight = 12.f;
    _capsuleOffset     = { 0.f, 6.f };
    _attackRange       = 110.f;
    _attackBoxWidth    = 54.f;
    _attackBoxHeight   = 74.f;
    _attackBoxOffsetX  = 54.f;

    _texture   = _idleAnim;
    _height    = _texture.height;
    _maxFrames = (int)(_texture.width / _width);
    _frame     = GetRandomValue(0, _maxFrames - 1);

    ApplyStoredTuning();
}

void Warchief::Update(float dt, Vector2 heroWorldPos, Vector2 navigationTarget, bool hasNavigationTarget,
    const std::vector<std::unique_ptr<Enemy>>& enemies,
    const std::vector<Vector2>& propCenters)
{
    // Full base bruiser behaviour first…
    Enemy::Update(dt, heroWorldPos, navigationTarget, hasNavigationTarget, enemies, propCenters);

    if (!_isActive || _dying)
        return;

    // …then refresh the war banner on every ally inside the ring. The half
    // second duration means the buff fades quickly once the chief dies.
    for (const auto& ally : enemies)
    {
        if (ally.get() == this || !ally->IsActive() || ally->IsDying())
            continue;
        if (ally->AsWarchief() != nullptr)
            continue;   // banners don't stack chiefs
        if (Vector2Distance(ally->GetWorldPos(), _worldPos) < kAuraRadius)
            ally->GrantWarAura(0.5f);
    }
}

void Warchief::DrawEnemy(Vector2 heroWorldPos)
{
    // Banner ring drawn under the sprite so buffed packs are readable.
    if (_isActive && !_dying)
    {
        Vector2 screenPos = Vector2{ _worldPos.x - heroWorldPos.x + kVirtualWidth / 2.f,
                                     _worldPos.y - heroWorldPos.y + kVirtualHeight / 2.f };
        float pulse = sinf((float)GetTime() * 5.f) * 0.5f + 0.5f;
        DrawCircleLines((int)screenPos.x, (int)screenPos.y, kAuraRadius,
            Fade(Color{ 255, 120, 60, 255 }, 0.16f + 0.10f * pulse));
        DrawCircleLines((int)screenPos.x, (int)screenPos.y, kAuraRadius - 6.f,
            Fade(Color{ 255, 170, 90, 255 }, 0.10f + 0.06f * pulse));
    }

    Enemy::DrawEnemy(heroWorldPos);
}

Rectangle Warchief::GetCollisionRec() const
{
    Rectangle animBodyRect;
    if (GetAnimBodyRectWorld(animBodyRect))
        return animBodyRect;
    if (_hasTunedCollision)
        return GetTunedCollisionRec();

    return Rectangle{ _worldPos.x - 40.f, _worldPos.y - 48.f + 10.f, 80.f, 96.f };
}

void Warchief::SetWaveScale(int wave)
{
    (void)wave;
    _expValue    = 6;
    _health      = 6.f;
    _maxHealth   = 6.f;
    _speed       = 175.f;
    _attackPower = 1.f;
}

void Warchief::PlayAttackSound()
{
    float pitch = GetRandomValue(70, 95) / 100.f;
    SetSoundPitch(_attackSound, pitch);
    SetSoundVolume(_attackSound, 0.7f);
    PlaySound(_attackSound);
}

void Warchief::EnsureSharedResourcesLoaded()
{
    if (_sharedResourcesLoaded)
        return;

    for (int variant = 0; variant < kVariantCount; variant++)
    {
        const char* suffix = kWarchiefVariantSuffixes[variant];
        _sharedIdleAnim[variant]   = LoadTexture(AssetPath(TextFormat("Enemy/WarchiefIdle%s.png",   suffix)).c_str());
        _sharedWalkAnim[variant]   = LoadTexture(AssetPath(TextFormat("Enemy/WarchiefWalk%s.png",   suffix)).c_str());
        _sharedAttackAnim[variant] = LoadTexture(AssetPath(TextFormat("Enemy/WarchiefAttack%s.png", suffix)).c_str());
        _sharedHurtAnim[variant]   = LoadTexture(AssetPath(TextFormat("Enemy/WarchiefHurt%s.png",   suffix)).c_str());
        _sharedDeathAnim[variant]  = LoadTexture(AssetPath(TextFormat("Enemy/WarchiefDeath%s.png",  suffix)).c_str());
    }
    _sharedAttackSound = LoadSound(AssetPath("Sounds/SwordSwipe2.ogg").c_str());
    _sharedHurtSound   = LoadSound(AssetPath("Sounds/SmallMonsterDamage.ogg").c_str());
    _sharedDeathSound  = LoadSound(AssetPath("Sounds/MonsterDeath.ogg").c_str());
    _sharedResourcesLoaded = true;
}

void Warchief::UnloadSharedResources()
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
