#include "SwordBeamPickup.h"
#include "AssetPaths.h"

#include "Character.h"
#include "raymath.h"

Texture2D SwordBeamPickup::_sharedTexture{};
bool SwordBeamPickup::_textureLoaded = false;

SwordBeamPickup::SwordBeamPickup()
{
    _isActive = true;
}

void SwordBeamPickup::Init(Vector2 spawnPos)
{
    Init(spawnPos, 1);
}

void SwordBeamPickup::Init(Vector2 spawnPos, int ammoValue)
{
    EnsureTextureLoaded();
    _worldPos = spawnPos;
    _ammoValue = ammoValue;
    _isActive = true;
}

void SwordBeamPickup::Draw(Vector2 worldOffset)
{
    if (!_isActive)
        return;

    EnsureTextureLoaded();

    Vector2 screenPos = Vector2Add(_worldPos, worldOffset);
    screenPos.x += GetScreenWidth() / 2.f;
    screenPos.y += GetScreenHeight() / 2.f;

    float scale = 3.8f;
    Rectangle source{ 0.f, 0.f, (float)_sharedTexture.width, (float)_sharedTexture.height };
    Rectangle dest{
        screenPos.x,
        screenPos.y,
        _sharedTexture.width * scale,
        _sharedTexture.height * scale
    };

    DrawTexturePro(_sharedTexture, source, dest, Vector2{ dest.width * 0.5f, dest.height * 0.5f }, 0.f, WHITE);
}

void SwordBeamPickup::OnCollect(Character& player)
{
    player.AddSwordBeamAmmo(_ammoValue);
    _isActive = false;
}

Rectangle SwordBeamPickup::GetCollisionRec() const
{
    return Rectangle{
        _worldPos.x - _radius,
        _worldPos.y - _radius,
        _radius * 2.f,
        _radius * 2.f
    };
}

int SwordBeamPickup::GetAmmoValue() const
{
    return _ammoValue;
}

Texture2D SwordBeamPickup::GetSharedTexture()
{
    EnsureTextureLoaded();
    return _sharedTexture;
}

void SwordBeamPickup::UnloadSharedResources()
{
    if (_textureLoaded)
    {
        UnloadTexture(_sharedTexture);
        _sharedTexture = Texture2D{};
        _textureLoaded = false;
    }
}

void SwordBeamPickup::EnsureTextureLoaded()
{
    if (_textureLoaded)
        return;

    _sharedTexture = LoadTexture(AssetPath("PowerUps/BladeBeamPickup.png").c_str());
    _textureLoaded = true;
}
