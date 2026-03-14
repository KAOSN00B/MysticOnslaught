#include "FireBallPickup.h"
#include "Character.h"
#include "raymath.h"

Texture2D FireBallPickup::_sharedTexture{};
bool      FireBallPickup::_textureLoaded = false;

FireBallPickup::FireBallPickup()
{
    _isActive = true;
}

void FireBallPickup::Init(Vector2 spawnPos)
{
    Init(spawnPos, 1);
}

void FireBallPickup::Init(Vector2 spawnPos, int ammoValue)
{
    EnsureTextureLoaded();
    _worldPos   = spawnPos;
    _ammoValue  = ammoValue;
    _isActive   = true;
}

void FireBallPickup::Draw(Vector2 worldOffset)
{
    if (!_isActive)
        return;

    EnsureTextureLoaded();

    Vector2 screenPos = Vector2Add(_worldPos, worldOffset);
    screenPos.x += GetScreenWidth()  / 2.f;
    screenPos.y += GetScreenHeight() / 2.f;

    Vector2 drawPos{
        screenPos.x - (_sharedTexture.width  * _scale) * 0.5f,
        screenPos.y - (_sharedTexture.height * _scale) * 0.5f
    };

    DrawTextureEx(_sharedTexture, drawPos, 0.f, _scale, WHITE);
}

void FireBallPickup::OnCollect(Character& player)
{
    player.AddFireballAmmo(_ammoValue);
    _isActive = false;
}

Rectangle FireBallPickup::GetCollisionRec() const
{
    if (!_textureLoaded)
        return Rectangle{ _worldPos.x - 32.f, _worldPos.y - 32.f, 64.f, 64.f };

    float w = _sharedTexture.width  * _scale;
    float h = _sharedTexture.height * _scale;

    return Rectangle{ _worldPos.x - w * 0.5f, _worldPos.y - h * 0.5f, w, h };
}

int FireBallPickup::GetAmmoValue() const
{
    return _ammoValue;
}

Texture2D FireBallPickup::GetSharedTexture()
{
    EnsureTextureLoaded();
    return _sharedTexture;
}

void FireBallPickup::UnloadSharedResources()
{
    if (_textureLoaded)
    {
        UnloadTexture(_sharedTexture);
        _sharedTexture  = Texture2D{};
        _textureLoaded  = false;
    }
}

void FireBallPickup::EnsureTextureLoaded()
{
    if (_textureLoaded)
        return;

    _sharedTexture = LoadTexture("C:\\Users\\rober\\Desktop\\Lasalle\\Semester 4\\2DGamesProgramming\\ClassNotes\\TestGame\\PowerUps\\FireBallPickup.png");
    _textureLoaded = true;
}
