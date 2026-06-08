#include "SpreadProjectile.h"
#include "VirtualCanvas.h"
#include "AssetPaths.h"
#include "VirtualCanvas.h"
#include "AnimationUtils.h"
#include "VirtualCanvas.h"
#include "raymath.h"
#include "VirtualCanvas.h"

Texture2D SpreadProjectile::_fireTex{};
Texture2D SpreadProjectile::_iceTex{};
Texture2D SpreadProjectile::_electricTex{};
Texture2D SpreadProjectile::_fireUltTex{};
Texture2D SpreadProjectile::_iceUltTex{};
Texture2D SpreadProjectile::_electricUltTex{};
bool      SpreadProjectile::_texturesLoaded = false;

void SpreadProjectile::Init(Vector2 spawnPos, Vector2 direction, AbilityType element)
{
    EnsureTexturesLoaded();
    _worldPos   = spawnPos;
    _direction  = Vector2Normalize(direction);
    _element    = element;
    _lifeTimer  = 5.f;
    _runningTime = 0.f;
    _frame      = 0;
    _isActive   = true;
}

void SpreadProjectile::Update(float dt)
{
    if (!_isActive)
        return;

    _lifeTimer -= dt;
    if (_lifeTimer <= 0.f)
    {
        Destroy();
        return;
    }

    _runningTime += dt;
    if (_runningTime >= _updateTime)
    {
        _runningTime = 0.f;
        _frame = (_frame + 1) % _frameCount;
    }

    _worldPos = Vector2Add(_worldPos, Vector2Scale(_direction, _speed * dt));
}

void SpreadProjectile::Draw(Vector2 worldOffset) const
{
    if (!_isActive)
        return;

    Vector2 screenPos = Vector2Add(_worldPos, worldOffset);
    screenPos.x += kVirtualWidth  / 2.f;
    screenPos.y += kVirtualHeight / 2.f;

    const Texture2D* tex = &_fireTex;
    switch (_element)
    {
    case AbilityType::IceSpread:
    case AbilityType::IceBolt:        tex = &_iceTex;      break;
    case AbilityType::ElectricSpread:
    case AbilityType::ElectricBolt:   tex = &_electricTex; break;
    default:                          tex = &_fireTex;     break;
    }

    float rotation = atan2f(_direction.y, _direction.x) * RAD2DEG;
    Rectangle source = GetAnimationFrameRect(*tex, _frameWidth, _frameHeight, _frame);
    float scale = 7.2f;
    Rectangle dest{
        screenPos.x,
        screenPos.y,
        _frameWidth  * scale,
        _frameHeight * scale
    };

    DrawTexturePro(*tex, source, dest,
        Vector2{ dest.width * 0.5f, dest.height * 0.5f }, rotation, WHITE);
}

void SpreadProjectile::Destroy()
{
    _isActive = false;
}

void SpreadProjectile::UnloadSharedResources()
{
    if (!_texturesLoaded)
        return;

    UnloadTexture(_fireTex);
    UnloadTexture(_iceTex);
    UnloadTexture(_electricTex);
    UnloadTexture(_fireUltTex);
    UnloadTexture(_iceUltTex);
    UnloadTexture(_electricUltTex);
    _fireTex        = Texture2D{};
    _iceTex         = Texture2D{};
    _electricTex    = Texture2D{};
    _fireUltTex     = Texture2D{};
    _iceUltTex      = Texture2D{};
    _electricUltTex = Texture2D{};
    _texturesLoaded = false;
}

bool SpreadProjectile::IsActive() const
{
    return _isActive;
}

Rectangle SpreadProjectile::GetCollisionRec() const
{
    return Rectangle{
        _worldPos.x - _radius,
        _worldPos.y - _radius,
        _radius * 2.f,
        _radius * 2.f
    };
}

Vector2 SpreadProjectile::GetWorldPos() const   { return _worldPos; }
Vector2 SpreadProjectile::GetDirection() const  { return _direction; }
AbilityType SpreadProjectile::GetElement() const { return _element; }

const Texture2D& SpreadProjectile::GetAnimTexture(AbilityType element)
{
    EnsureTexturesLoaded();
    switch (element)
    {
    case AbilityType::FireUltimate:     return _fireUltTex;
    case AbilityType::IceUltimate:      return _iceUltTex;
    case AbilityType::ElectricUltimate: return _electricUltTex;
    case AbilityType::IceSpread:
    case AbilityType::IceBolt:          return _iceTex;
    case AbilityType::ElectricSpread:
    case AbilityType::ElectricBolt:     return _electricTex;
    default:                            return _fireTex;
    }
}

int SpreadProjectile::GetFrameWFor(AbilityType element)
{
    switch (element)
    {
    case AbilityType::FireUltimate:
    case AbilityType::IceUltimate:
    case AbilityType::ElectricUltimate: return 64;
    default:                            return _frameWidth;
    }
}

int SpreadProjectile::GetFrameHFor(AbilityType element)
{
    switch (element)
    {
    case AbilityType::FireUltimate:
    case AbilityType::IceUltimate:
    case AbilityType::ElectricUltimate: return 64;
    default:                            return _frameHeight;
    }
}

int SpreadProjectile::GetFrameCountFor(AbilityType element)
{
    switch (element)
    {
    case AbilityType::FireUltimate:     return 13;
    case AbilityType::IceUltimate:      return 11;
    case AbilityType::ElectricUltimate: return 11;
    default:                            return _frameCount;
    }
}

void SpreadProjectile::EnsureTexturesLoaded()
{
    if (_texturesLoaded)
        return;

    _fireTex        = LoadTexture(AssetPath("PowerUps/Fireball.png").c_str());
    _iceTex         = LoadTexture(AssetPath("PowerUps/Ice_Shard.png").c_str());
    _electricTex    = LoadTexture(AssetPath("PowerUps/Lightning_Bolt01_Y.png").c_str());
    _fireUltTex     = LoadTexture(AssetPath("PowerUps/Flame_Explosion.png").c_str());
    _iceUltTex      = LoadTexture(AssetPath("PowerUps/Frozen_Explosion.png").c_str());
    _electricUltTex = LoadTexture(AssetPath("PowerUps/Thunder_Blast.png").c_str());
    _texturesLoaded = true;
}
