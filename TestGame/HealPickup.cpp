#include "HealPickup.h"
#include "AssetPaths.h"

#include "Character.h"

Texture2D HealPickup::_sharedTexture{};
bool HealPickup::_textureLoaded = false;

HealPickup::HealPickup()
{
    _isActive = true;
}

void HealPickup::Init(Vector2 spawnPos)
{
    EnsureTextureLoaded();
    _worldPos = spawnPos;
    _isActive = true;
}

void HealPickup::Draw(Vector2 worldOffset)
{
    if (!_isActive)
        return;

    EnsureTextureLoaded();
    Vector2 screenPos{ _worldPos.x + worldOffset.x, _worldPos.y + worldOffset.y };
    screenPos.x += GetScreenWidth() / 2.f;
    screenPos.y += GetScreenHeight() / 2.f;

    Rectangle source{ 0.f, 0.f, (float)_sharedTexture.width, (float)_sharedTexture.height };
    float scale = 6.0f;
    Rectangle dest{
        screenPos.x,
        screenPos.y,
        _sharedTexture.width * scale,
        _sharedTexture.height * scale
    };

    DrawTexturePro(_sharedTexture, source, dest, Vector2{ dest.width * 0.5f, dest.height * 0.5f }, 0.f, WHITE);
}

void HealPickup::OnCollect(Character& player)
{
    player.Heal(1);
    _isActive = false;
}

Rectangle HealPickup::GetCollisionRec() const
{
    return Rectangle{ _worldPos.x - 18.f, _worldPos.y - 18.f, 36.f, 36.f };
}

void HealPickup::UnloadSharedResources()
{
    if (_textureLoaded)
    {
        UnloadTexture(_sharedTexture);
        _sharedTexture = Texture2D{};
        _textureLoaded = false;
    }
}

void HealPickup::EnsureTextureLoaded()
{
    if (_textureLoaded)
        return;

    _sharedTexture = LoadTexture(AssetPath("PowerUps/FoodPickup.png").c_str());
    _textureLoaded = true;
}
