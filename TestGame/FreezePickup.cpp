#include "FreezePickup.h"
#include "AssetPaths.h"

#include "Character.h"
#include "raymath.h"

Texture2D FreezePickup::_sharedTexture{};
bool FreezePickup::_textureLoaded = false;

FreezePickup::FreezePickup()
{
    _isActive = true;
}

void FreezePickup::Init(Vector2 spawnPos)
{
    EnsureTextureLoaded();
    _worldPos = spawnPos;
    _isActive = true;
}

void FreezePickup::Draw(Vector2 worldOffset)
{
    if (!_isActive)
        return;

    EnsureTextureLoaded();

    Vector2 screenPos = Vector2Add(_worldPos, worldOffset);
    screenPos.x += GetScreenWidth() / 2.f;
    screenPos.y += GetScreenHeight() / 2.f;

    float scale = 4.f;
    Rectangle source{ 0.f, 0.f, (float)_sharedTexture.width, (float)_sharedTexture.height };
    Rectangle dest{
        screenPos.x,
        screenPos.y,
        _sharedTexture.width * scale,
        _sharedTexture.height * scale
    };

    DrawTexturePro(_sharedTexture, source, dest, Vector2{ dest.width * 0.5f, dest.height * 0.5f }, 0.f, WHITE);
}

void FreezePickup::OnCollect(Character& player)
{
    player.AddFreezeAmmo(1);
    _isActive = false;
}

Rectangle FreezePickup::GetCollisionRec() const
{
    return Rectangle{ _worldPos.x - 18.f, _worldPos.y - 18.f, 36.f, 36.f };
}

Texture2D FreezePickup::GetSharedTexture()
{
    EnsureTextureLoaded();
    return _sharedTexture;
}

void FreezePickup::UnloadSharedResources()
{
    if (_textureLoaded)
    {
        UnloadTexture(_sharedTexture);
        _sharedTexture = Texture2D{};
        _textureLoaded = false;
    }
}

void FreezePickup::EnsureTextureLoaded()
{
    if (_textureLoaded)
        return;

    _sharedTexture = LoadTexture(AssetPath("PowerUps/IceSpellPickup.png").c_str());
    _textureLoaded = true;
}
