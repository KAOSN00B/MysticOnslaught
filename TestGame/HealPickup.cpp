#include "HealPickup.h"

#include "AnimationUtils.h"
#include "Character.h"
#include "raymath.h"

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
    _runningTime = 0.f;
    _frame = 0;
}

void HealPickup::Draw(Vector2 worldOffset)
{
    if (!_isActive)
        return;

    EnsureTextureLoaded();

    _runningTime += GetFrameTime();
    if (_runningTime >= _updateTime)
    {
        _runningTime = 0.f;
        _frame = (_frame + 1) % _frameCount;
    }

    Vector2 screenPos = Vector2Add(_worldPos, worldOffset);
    screenPos.x += GetScreenWidth() / 2.f;
    screenPos.y += GetScreenHeight() / 2.f;

    Rectangle source = GetAnimationFrameRect(_sharedTexture, _frameWidth, _frameHeight, _frame);
    float scale = 3.2f;
    Rectangle dest{
        screenPos.x,
        screenPos.y,
        _frameWidth * scale,
        _frameHeight * scale
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

    _sharedTexture = LoadTexture("C:\\Users\\rober\\Desktop\\Lasalle\\Semester 4\\2DGamesProgramming\\ClassNotes\\TestGame\\PowerUps\\Health_Up.png");
    _textureLoaded = true;
}
