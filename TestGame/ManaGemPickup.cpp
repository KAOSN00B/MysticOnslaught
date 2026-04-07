#include "ManaGemPickup.h"
#include "AssetPaths.h"
#include "Character.h"
#include "raymath.h"
#include <cmath>

Texture2D ManaGemPickup::_sharedTexture{};
bool      ManaGemPickup::_textureLoaded = false;

ManaGemPickup::ManaGemPickup()
{
    _isActive = true;
}

void ManaGemPickup::Init(Vector2 spawnPos)
{
    EnsureTextureLoaded();
    _worldPos = spawnPos;
    _isActive = true;
    _bob      = 0.f;
}

void ManaGemPickup::Draw(Vector2 worldOffset)
{
    if (!_isActive)
        return;

    EnsureTextureLoaded();

    _bob += GetFrameTime() * 3.f;
    float bobOffset = sinf(_bob) * 5.f;

    Vector2 screenPos = Vector2Add(_worldPos, worldOffset);
    screenPos.x += GetScreenWidth()  / 2.f;
    screenPos.y += GetScreenHeight() / 2.f + bobOffset;

    float scale = 3.0f;
    float w = _sharedTexture.width  * scale;
    float h = _sharedTexture.height * scale;

    DrawTextureEx(_sharedTexture,
        { screenPos.x - w * 0.5f, screenPos.y - h * 0.5f },
        0.f, scale, WHITE);
}

void ManaGemPickup::OnCollect(Character& player)
{
    player.RestoreMana(20);
    _isActive = false;
}

Rectangle ManaGemPickup::GetCollisionRec() const
{
    return Rectangle{ _worldPos.x - 20.f, _worldPos.y - 20.f, 40.f, 40.f };
}

void ManaGemPickup::UnloadSharedResources()
{
    if (_textureLoaded)
    {
        UnloadTexture(_sharedTexture);
        _sharedTexture = Texture2D{};
        _textureLoaded = false;
    }
}

void ManaGemPickup::EnsureTextureLoaded()
{
    if (_textureLoaded)
        return;

    _sharedTexture = LoadTexture(AssetPath("PowerUps/ManaGem.png").c_str());
    _textureLoaded = true;
}
