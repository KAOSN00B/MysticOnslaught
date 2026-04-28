#pragma once

#include "raylib.h"

class FireballProjectile
{
public:
    FireballProjectile() = default;

    void Init(Vector2 spawnPos, Vector2 direction);
    void Update(float dt);
    void Draw(Vector2 worldOffset) const;
    void Destroy();
    static void UnloadSharedResources();

    bool IsActive() const;
    Rectangle GetCollisionRec() const;
    Vector2 GetWorldPos() const;
    Vector2 GetDirection() const;

private:
    static void EnsureTextureLoaded();

    Vector2 _worldPos{};
    Vector2 _direction{};
    float _speed  = 650.f;
    float _radius = 56.f;
    float _lifeTimer = 5.f;
    float _runningTime = 0.f;
    float _updateTime = 1.f / 16.f;
    int _frame = 0;
    bool _isActive = false;

    static constexpr int _frameWidth = 32;
    static constexpr int _frameHeight = 32;
    static constexpr int _frameCount = 8;
    static Texture2D _sharedTexture;
    static bool _textureLoaded;
};
