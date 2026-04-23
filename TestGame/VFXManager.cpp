#include "VFXManager.h"
#include "AnimationUtils.h"
#include "raymath.h"

#include <algorithm>
#include <cmath>

// ── Setup ─────────────────────────────────────────────────────────────────────

void VFXManager::Init(Texture2D* fireballCastTex,  Texture2D* fireballHitTex,
                      Texture2D* genericHitTex,    Texture2D* iceHitTex,
                      Texture2D* lightningCastTex, Texture2D* healEffectTex)
{
    _fireballCastTex  = fireballCastTex;
    _fireballHitTex   = fireballHitTex;
    _genericHitTex    = genericHitTex;
    _iceHitTex        = iceHitTex;
    _lightningCastTex = lightningCastTex;
    _healEffectTex    = healEffectTex;
}

// ── Per-frame ─────────────────────────────────────────────────────────────────

void VFXManager::Update(float dt)
{
    for (auto& effect : _effects)
    {
        if (!effect.active)
            continue;

        effect.runningTime += dt;
        if (effect.runningTime >= effect.frameTime)
        {
            effect.runningTime = 0.f;
            effect.frame++;
            if (effect.frame >= effect.frameCount)
                effect.active = false;
        }
    }

    _effects.erase(
        std::remove_if(_effects.begin(), _effects.end(),
            [](const AnimatedEffect& e) { return !e.active; }),
        _effects.end());
}

void VFXManager::Draw(Vector2 worldOffset, Vector2 playerWorldPos, Vector2 playerCastOrigin)
{
    for (const auto& effect : _effects)
    {
        if (!effect.active || effect.texture == nullptr || effect.texture->id == 0)
            continue;

        Vector2 worldPos = effect.followPlayer
            ? (effect.followPlayerCenter
                ? Vector2Add(playerWorldPos,  effect.offset)
                : Vector2Add(playerCastOrigin, effect.offset))
            : effect.worldPos;

        Vector2 screenPos = Vector2Add(worldPos, worldOffset);
        screenPos.x += GetScreenWidth()  / 2.f;
        screenPos.y += GetScreenHeight() / 2.f;

        float rotation = atan2f(effect.direction.y, effect.direction.x) * RAD2DEG;
        Rectangle source = GetAnimationFrameRect(*effect.texture,
                                                  effect.frameWidth, effect.frameHeight,
                                                  effect.frame);
        Rectangle dest{
            screenPos.x,
            screenPos.y,
            effect.frameWidth  * effect.scale,
            effect.frameHeight * effect.scale
        };
        DrawTexturePro(*effect.texture, source, dest,
            Vector2{ dest.width * 0.5f, dest.height * 0.5f }, rotation, effect.tint);
    }
}

void VFXManager::DrawFloatingTexts(Vector2 worldOffset)
{
    float now = (float)GetTime();
    _floatingTexts.erase(
        std::remove_if(_floatingTexts.begin(), _floatingTexts.end(),
            [now](const FloatingText& ft)
            { return now - ft.spawnTime >= FloatingText::kLifetime; }),
        _floatingTexts.end());

    const float sw2 = GetScreenWidth()  / 2.f;
    const float sh2 = GetScreenHeight() / 2.f;
    for (const auto& ft : _floatingTexts)
    {
        float t      = (now - ft.spawnTime) / FloatingText::kLifetime;
        float yOff   = -55.f * t;
        float screenX = ft.worldPos.x + worldOffset.x + sw2;
        float screenY = ft.worldPos.y + worldOffset.y + sh2 + yOff;
        const char* txt = TextFormat("%d", ft.value);
        int   tw    = MeasureText(txt, 22);
        float alpha = 1.f - t;
        DrawText(txt, (int)(screenX - tw / 2.f), (int)screenY, 22, Fade(ft.color, alpha));
    }
}

void VFXManager::Clear()
{
    _effects.clear();
    _floatingTexts.clear();
}

// ── Spawn helpers ─────────────────────────────────────────────────────────────

void VFXManager::SpawnCastEffect(Character::CastType castType,
                                  Vector2 castOrigin, Vector2 facingDir)
{
    AnimatedEffect effect{};
    effect.followPlayer = true;
    effect.worldPos     = castOrigin;
    effect.direction    = facingDir;
    effect.active       = true;

    switch (castType)
    {
    case Character::CastType::FireSpread:
        effect.texture    = _fireballCastTex;
        effect.frameCount = 8;
        effect.scale      = 4.f;
        break;
    case Character::CastType::IceSpread:
        effect.texture    = _iceHitTex;
        effect.frameCount = 8;
        effect.scale      = 4.f;
        effect.tint       = Color{ 100, 200, 255, 255 };
        break;
    case Character::CastType::ElectricSpread:
        effect.texture    = _lightningCastTex;
        effect.frameCount = 8;
        effect.scale      = 4.f;
        break;
    default:
        effect.active = false;
        break;
    }

    if (effect.active)
        _effects.push_back(effect);
}

void VFXManager::SpawnHitEffect(Character::CastType castType,
                                 Vector2 worldPos, Vector2 direction)
{
    AnimatedEffect effect{};
    effect.followPlayer = false;
    effect.worldPos     = worldPos;
    effect.direction    = direction;
    effect.active       = true;
    effect.frameTime    = 1.f / 20.f;

    switch (castType)
    {
    case Character::CastType::None:
        effect.texture    = _genericHitTex;
        effect.frameCount = 5;
        effect.scale      = 3.5f;
        effect.tint       = Color{ 255, 150, 150, 255 };
        break;
    case Character::CastType::FireSpread:
        effect.texture    = _fireballHitTex;
        effect.frameCount = 6;
        effect.scale      = 4.f;
        effect.tint       = WHITE;
        break;
    case Character::CastType::IceSpread:
        effect.texture    = _iceHitTex;
        effect.frameCount = 5;
        effect.scale      = 4.f;
        effect.tint       = Color{ 100, 200, 255, 255 };
        break;
    case Character::CastType::ElectricSpread:
        effect.texture    = _genericHitTex;
        effect.frameCount = 5;
        effect.scale      = 4.f;
        effect.tint       = WHITE;
        break;
    default:
        effect.active = false;
        break;
    }

    if (effect.active)
        _effects.push_back(effect);
}

void VFXManager::SpawnHealEffect()
{
    if (_healEffectTex == nullptr || _healEffectTex->id == 0)
        return;

    AnimatedEffect effect{};
    effect.texture            = _healEffectTex;
    effect.followPlayer       = true;
    effect.followPlayerCenter = true;
    effect.offset             = Vector2{ 0.f, -20.f };
    effect.direction          = Vector2{ 1.f,   0.f };
    effect.tint               = WHITE;
    effect.frameCount         = 13;
    effect.frameTime          = 1.f / 16.f;
    effect.scale              = 4.5f;
    effect.active             = true;
    _effects.push_back(effect);
}

void VFXManager::SpawnFloatingText(Vector2 worldPos, int value, Color color)
{
    FloatingText ft;
    ft.worldPos  = worldPos;
    ft.value     = value;
    ft.color     = color;
    ft.spawnTime = (float)GetTime();
    _floatingTexts.push_back(ft);
}
