#include "SlimeEnemy.h"
#include "VirtualCanvas.h"
#include "AssetPaths.h"

#include <algorithm>

// ---- Static member definitions ----------------------------------------------
Texture2D SlimeEnemy::_sharedBigIdleAnim{};
Texture2D SlimeEnemy::_sharedBigWalkAnim{};
Texture2D SlimeEnemy::_sharedBigAttackAnim{};
Texture2D SlimeEnemy::_sharedBigHurtAnim{};
Texture2D SlimeEnemy::_sharedBigDeathAnim{};
Texture2D SlimeEnemy::_sharedSmallIdleAnim{};
Texture2D SlimeEnemy::_sharedSmallWalkAnim{};
Texture2D SlimeEnemy::_sharedSmallAttackAnim{};
Texture2D SlimeEnemy::_sharedSmallHurtAnim{};
Texture2D SlimeEnemy::_sharedSmallDeathAnim{};
Sound     SlimeEnemy::_sharedAttackSound{};
Sound     SlimeEnemy::_sharedHurtSound{};
Sound     SlimeEnemy::_sharedDeathSound{};
bool      SlimeEnemy::_sharedResourcesLoaded = false;

namespace
{
    // Both slime strips are stitched 6-frame rows: Big = 48x48, Small = 32x32.
    constexpr float kBigSlimeFrameWidth   = 48.f;
    constexpr float kSmallSlimeFrameWidth = 32.f;
}

// =============================================================================
SlimeEnemy::SlimeEnemy(Vector2 pos, SlimeSize size)
    : Enemy(pos)
    , _size(size)
{
}

SlimeEnemy::~SlimeEnemy() {}

// =============================================================================
void SlimeEnemy::Init()
{
    EnsureSharedResourcesLoaded();

    if (_size == SlimeSize::Big)
    {
        _idleAnim       = _sharedBigIdleAnim;
        _walkAnim       = _sharedBigWalkAnim;
        _attackAnim     = _sharedBigAttackAnim;
        _takeDamageAnim = _sharedBigHurtAnim;
        _deathAnim      = _sharedBigDeathAnim;
    }
    else
    {
        _idleAnim       = _sharedSmallIdleAnim;
        _walkAnim       = _sharedSmallWalkAnim;
        _attackAnim     = _sharedSmallAttackAnim;
        _takeDamageAnim = _sharedSmallHurtAnim;
        _deathAnim      = _sharedSmallDeathAnim;
    }
    _attackSound = _sharedAttackSound;
    _hurtSound   = _sharedHurtSound;
    _deathSound  = _sharedDeathSound;

    ResetForSpawn(_worldPos);
}

// =============================================================================
void SlimeEnemy::ResetForSpawn(Vector2 pos)
{
    // Base reset gives us all the shared grunt behaviour state; then the slime
    // overrides the sprite metrics and stats for its size class.
    Enemy::ResetForSpawn(pos);

    if (_size == SlimeSize::Big)
    {
        _width  = kBigSlimeFrameWidth;
        _scale  = 5.2f;
        _speed  = 135.f;
        _health = 5.f;
        _maxHealth = 5.f;
        _attackPower = 1.f;
        _expValue = 3;
        // Big slime body is wide — larger capsule so it feels solid.
        _capsuleRadius     = 34.f;
        _capsuleHalfHeight = 0.f;
        _capsuleOffset     = { 0.f, 10.f };
    }
    else
    {
        _width  = kSmallSlimeFrameWidth;
        _scale  = 4.6f;
        _speed  = 265.f;
        _health = 1.f;
        _maxHealth = 1.f;
        _attackPower = 1.f;
        _expValue = 1;
        _capsuleRadius     = 20.f;
        _capsuleHalfHeight = 0.f;
        _capsuleOffset     = { 0.f, 6.f };
        // Small slimes bite at close range with a small hit box.
        _attackRange       = 80.f;
        _attackBoxWidth    = 36.f;
        _attackBoxHeight   = 50.f;
        _attackBoxOffsetX  = 36.f;
    }

    _texture   = _idleAnim;
    _height    = _texture.height;
    _maxFrames = (int)(_texture.width / _width);
    _frame     = GetRandomValue(0, _maxFrames - 1);
}

// =============================================================================
Rectangle SlimeEnemy::GetCollisionRec() const
{
    // Solid rect centred on the body, sitting slightly low like the sprite.
    float rectWidth  = (_size == SlimeSize::Big) ? 110.f : 64.f;
    float rectHeight = (_size == SlimeSize::Big) ?  78.f : 48.f;
    float yOffset    = (_size == SlimeSize::Big) ?  14.f : 10.f;

    return Rectangle{
        _worldPos.x - rectWidth  * 0.5f,
        _worldPos.y - rectHeight * 0.5f + yOffset,
        rectWidth,
        rectHeight
    };
}

// =============================================================================
void SlimeEnemy::SetWaveScale(int wave)
{
    // Fixed base profile per size; global growth comes from ApplyEnemyPowerLevel.
    (void)wave;
    if (_size == SlimeSize::Big)
    {
        _expValue    = 3;
        _health      = 5.f;
        _maxHealth   = 5.f;
        _speed       = 135.f;
        _attackPower = 1.f;
    }
    else
    {
        _expValue    = 1;
        _health      = 1.f;
        _maxHealth   = 1.f;
        _speed       = 265.f;
        _attackPower = 1.f;
    }
}

// =============================================================================
void SlimeEnemy::PlayAttackSound()
{
    float pitch = GetRandomValue(_size == SlimeSize::Big ? 70 : 130,
                                 _size == SlimeSize::Big ? 95 : 170) / 100.f;
    SetSoundPitch(_attackSound, pitch);
    SetSoundVolume(_attackSound, 0.6f);
    PlaySound(_attackSound);
}

// =============================================================================
void SlimeEnemy::EnsureSharedResourcesLoaded()
{
    if (_sharedResourcesLoaded)
        return;

    _sharedBigIdleAnim     = LoadTexture(AssetPath("Enemy/SlimeBigIdle.png").c_str());
    _sharedBigWalkAnim     = LoadTexture(AssetPath("Enemy/SlimeBigWalk.png").c_str());
    _sharedBigAttackAnim   = LoadTexture(AssetPath("Enemy/SlimeBigAttack.png").c_str());
    _sharedBigHurtAnim     = LoadTexture(AssetPath("Enemy/SlimeBigHurt.png").c_str());
    _sharedBigDeathAnim    = LoadTexture(AssetPath("Enemy/SlimeBigDeath.png").c_str());
    _sharedSmallIdleAnim   = LoadTexture(AssetPath("Enemy/SlimeSmallIdle.png").c_str());
    _sharedSmallWalkAnim   = LoadTexture(AssetPath("Enemy/SlimeSmallWalk.png").c_str());
    _sharedSmallAttackAnim = LoadTexture(AssetPath("Enemy/SlimeSmallAttack.png").c_str());
    _sharedSmallHurtAnim   = LoadTexture(AssetPath("Enemy/SlimeSmallHurt.png").c_str());
    _sharedSmallDeathAnim  = LoadTexture(AssetPath("Enemy/SlimeSmallDeath.png").c_str());
    _sharedAttackSound     = LoadSound(AssetPath("Sounds/SwordSwipe2.ogg").c_str());
    _sharedHurtSound       = LoadSound(AssetPath("Sounds/SmallMonsterDamage.ogg").c_str());
    _sharedDeathSound      = LoadSound(AssetPath("Sounds/PlayerDeath.ogg").c_str());
    _sharedResourcesLoaded = true;
}

void SlimeEnemy::UnloadSharedResources()
{
    if (!_sharedResourcesLoaded)
        return;

    UnloadTexture(_sharedBigIdleAnim);
    UnloadTexture(_sharedBigWalkAnim);
    UnloadTexture(_sharedBigAttackAnim);
    UnloadTexture(_sharedBigHurtAnim);
    UnloadTexture(_sharedBigDeathAnim);
    UnloadTexture(_sharedSmallIdleAnim);
    UnloadTexture(_sharedSmallWalkAnim);
    UnloadTexture(_sharedSmallAttackAnim);
    UnloadTexture(_sharedSmallHurtAnim);
    UnloadTexture(_sharedSmallDeathAnim);
    UnloadSound(_sharedAttackSound);
    UnloadSound(_sharedHurtSound);
    UnloadSound(_sharedDeathSound);

    _sharedBigIdleAnim     = Texture2D{};
    _sharedBigWalkAnim     = Texture2D{};
    _sharedBigAttackAnim   = Texture2D{};
    _sharedBigHurtAnim     = Texture2D{};
    _sharedBigDeathAnim    = Texture2D{};
    _sharedSmallIdleAnim   = Texture2D{};
    _sharedSmallWalkAnim   = Texture2D{};
    _sharedSmallAttackAnim = Texture2D{};
    _sharedSmallHurtAnim   = Texture2D{};
    _sharedSmallDeathAnim  = Texture2D{};
    _sharedAttackSound     = Sound{};
    _sharedHurtSound       = Sound{};
    _sharedDeathSound      = Sound{};
    _sharedResourcesLoaded = false;
}
