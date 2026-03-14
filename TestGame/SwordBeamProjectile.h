#pragma once

#include "Enemy.h"
#include "raylib.h"

#include <vector>

class SwordBeamProjectile
{
public:
    void Init(Vector2 spawnPos, Vector2 direction);
    void Update(float dt);
    void Draw(Vector2 worldOffset) const;
    void Destroy();
    static void UnloadSharedResources();

    bool IsActive() const;
    Rectangle GetCollisionRec() const;
    Vector2 GetWorldPos() const;
    Vector2 GetDirection() const;
    bool HasHitEnemy(const Enemy* enemy) const;
    void RegisterHitEnemy(const Enemy* enemy);

private:
    static void EnsureTextureLoaded();

    Vector2 _worldPos{};
    Vector2 _direction{};
    float _speed = 900.f;
    float _width = 76.f;
    float _height = 260.f;
    float _lifeTimer = 5.f;
    float _runningTime = 0.f;
    float _updateTime = 1.f / 18.f;
    int _frame = 0;
    bool _isActive = false;
    std::vector<const Enemy*> _hitEnemies;

    static constexpr int _frameWidth = 32;
    static constexpr int _frameHeight = 32;
    static constexpr int _frameCount = 12;
    static Texture2D _sharedTexture;
    static bool _textureLoaded;
};
