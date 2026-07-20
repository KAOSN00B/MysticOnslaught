#include "LavaBallProjectile.h"
#include "VirtualCanvas.h"
#include "AssetPaths.h"
#include "AttackTuning.h"
#include "VirtualCanvas.h"

#include "raymath.h"
#include "VirtualCanvas.h"

#include <algorithm>
#include <cmath>

Texture2D LavaBallProjectile::_sharedLavaBallTex{};
Texture2D LavaBallProjectile::_sharedLavaBallHitTex{};
bool LavaBallProjectile::_sharedResourcesLoaded = false;

namespace
{
    Rectangle GetSheetFrame(Texture2D texture, int columns, int rows, int frameIndex)
    {
        float frameWidth = texture.width / (float)columns;
        float frameHeight = texture.height / (float)rows;
        int column = frameIndex % columns;
        int row = frameIndex / columns;

        return Rectangle{
            column * frameWidth,
            row * frameHeight,
            frameWidth,
            frameHeight
        };
    }
}

void LavaBallProjectile::Init(Vector2 spawnPos, Vector2 direction, const char* tuningKey)
{
    EnsureSharedResourcesLoaded();

    _worldPos = spawnPos;
    _direction = (Vector2Length(direction) > 0.01f)
        ? Vector2Normalize(direction)
        : Vector2{ 1.f, 0.f };
    _tuningKey = tuningKey ? tuningKey : "";
    _lifeTimer = _maxLife;
    _runningTime = 0.f;
    _frame = 0;
    _state = State::Flying;
    _isActive = true;
    _playerHit = false;
}

void LavaBallProjectile::Update(float dt)
{
    if (!_isActive)
        return;

    if (_state == State::Flying)
    {
        _lifeTimer -= dt;
        if (_lifeTimer <= 0.f)
        {
            Destroy();
            return;
        }

        _worldPos = Vector2Add(_worldPos, Vector2Scale(_direction, _speed * dt));
        AdvanceAnimation(dt, _flyingFrameCount, _frameTime, true);
        return;
    }

    AdvanceAnimation(dt, _hitFrameCount, _hitFrameTime, false);
}

void LavaBallProjectile::Draw(Vector2 worldOffset) const
{
    if (!_isActive)
        return;

    float rotation = atan2f(_direction.y, _direction.x) * RAD2DEG;

    if (_state == State::Flying)
    {
        DrawFlyingSheet(worldOffset, rotation);
        return;
    }

    DrawHitSheet(worldOffset);
}

void LavaBallProjectile::BeginHit()
{
    if (!_isActive)
        return;

    _state = State::Exploding;
    _runningTime = 0.f;
    _frame = 0;
}

void LavaBallProjectile::Destroy()
{
    _isActive = false;
}

Rectangle LavaBallProjectile::GetCollisionRec() const
{
    float size = 64.f;
    float offsetX = 0.f;
    float offsetY = 0.f;

    if (!_tuningKey.empty())
    {
        if (const AttackTuning* tuning = AttackTuningStore::Get(_tuningKey))
        {
            if (tuning->hasBox)
            {
                size = std::max(8.f, tuning->w);
                offsetX = tuning->x;
                offsetY = tuning->y;
            }
        }
    }

    Vector2 right = _direction;
    Vector2 up{ -right.y, right.x };
    Vector2 center = Vector2Add(_worldPos, Vector2Add(Vector2Scale(right, offsetX), Vector2Scale(up, offsetY)));
    float halfSize = size * 0.5f;
    return Rectangle{ center.x - halfSize, center.y - halfSize, size, size };
}

void LavaBallProjectile::UnloadSharedResources()
{
    if (!_sharedResourcesLoaded)
        return;

    UnloadTexture(_sharedLavaBallTex);
    UnloadTexture(_sharedLavaBallHitTex);
    _sharedLavaBallTex = Texture2D{};
    _sharedLavaBallHitTex = Texture2D{};
    _sharedResourcesLoaded = false;
}

void LavaBallProjectile::EnsureSharedResourcesLoaded()
{
    if (_sharedResourcesLoaded)
        return;

    _sharedLavaBallTex = LoadTexture(AssetPath("Bosses/Lavaball.png").c_str());
    _sharedLavaBallHitTex = LoadTexture(AssetPath("Bosses/Lavaball_Hit.png").c_str());
    _sharedResourcesLoaded = true;
}

void LavaBallProjectile::AdvanceAnimation(float dt, int frameCount, float frameTime, bool loop)
{
    _runningTime += dt;
    if (_runningTime < frameTime)
        return;

    _runningTime = 0.f;
    ++_frame;

    if (_frame < frameCount)
        return;

    if (loop)
    {
        _frame = 0;
        return;
    }

    Destroy();
}

void LavaBallProjectile::DrawFlyingSheet(Vector2 worldOffset, float rotation) const
{
    Rectangle source = GetSheetFrame(_sharedLavaBallTex, _flyingColumns, _flyingRows, _frame);

    Vector2 screenPos{
        _worldPos.x + worldOffset.x + kVirtualWidth * 0.5f,
        _worldPos.y + worldOffset.y + kVirtualHeight * 0.5f
    };

    Rectangle dest{
        screenPos.x,
        screenPos.y,
        source.width * _drawScale,
        source.height * _drawScale
    };

    DrawTexturePro(_sharedLavaBallTex, source, dest, Vector2{ dest.width * 0.5f, dest.height * 0.5f }, rotation, _tint);
}

void LavaBallProjectile::DrawHitSheet(Vector2 worldOffset) const
{
    Rectangle source = GetSheetFrame(_sharedLavaBallHitTex, _hitColumns, _hitRows, _frame);

    Vector2 screenPos{
        _worldPos.x + worldOffset.x + kVirtualWidth * 0.5f,
        _worldPos.y + worldOffset.y + kVirtualHeight * 0.5f
    };

    Rectangle dest{
        screenPos.x,
        screenPos.y,
        source.width * _drawScale,
        source.height * _drawScale
    };

    DrawTexturePro(_sharedLavaBallHitTex, source, dest, Vector2{ dest.width * 0.5f, dest.height * 0.5f }, 0.f, _tint);
}
