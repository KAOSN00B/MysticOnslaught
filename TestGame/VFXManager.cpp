#include "VFXManager.h"
#include "GameBalance.h"
#include "VfxGlow.h"
#include "VfxPixel.h"
#include "VirtualCanvas.h"
#include "AnimationUtils.h"
#include "VirtualCanvas.h"
#include "raymath.h"
#include "VirtualCanvas.h"

#include <algorithm>
#include <cmath>

// Frames in Hit03's one-colour star burst (32px cells; the sixth cell is empty).
static constexpr int kHit03FrameCount = 5;

// ── Setup ─────────────────────────────────────────────────────────────────────

void VFXManager::Init(Texture2D* fireballCastTex,  Texture2D* fireballHitTex,
                      Texture2D* genericHitTex,    Texture2D* iceHitTex,
                      Texture2D* lightningCastTex, Texture2D* thunderHitTex,
                      Texture2D* healEffectTex)
{
    _fireballCastTex  = fireballCastTex;
    _fireballHitTex   = fireballHitTex;
    _genericHitTex    = genericHitTex;
    _iceHitTex        = iceHitTex;
    _lightningCastTex = lightningCastTex;
    _thunderHitTex    = thunderHitTex;
    _healEffectTex    = healEffectTex;
}

// ── Per-frame ─────────────────────────────────────────────────────────────────

void VFXManager::Update(float dt)
{
    for (auto& accent : _contactAccents) accent.age += std::max(0.f, dt);
    _contactAccents.erase(std::remove_if(_contactAccents.begin(), _contactAccents.end(),
        [](const ContactAccent& accent) { return accent.age >= accent.lifetime; }), _contactAccents.end());
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
            {
                // Lingering decals loop; one-shot effects finish and are culled.
                if (effect.looping) effect.frame = 0;
                else                effect.active = false;
            }
        }

        // Lingering hazard decals expire on their own timer regardless of frame.
        if (effect.looping)
        {
            effect.lifeRemaining -= dt;
            if (effect.lifeRemaining <= 0.f)
                effect.active = false;
        }
    }

    _effects.erase(
        std::remove_if(_effects.begin(), _effects.end(),
            [](const AnimatedEffect& e) { return !e.active; }),
        _effects.end());

    // Death payoff: shockwaves expand; gore arcs, lands and settles; splats age.
    for (auto& w : _shockwaves) w.age += dt;
    _shockwaves.erase(std::remove_if(_shockwaves.begin(), _shockwaves.end(),
        [](const Shockwave& w) { return w.age >= w.life; }), _shockwaves.end());
    for (auto& g : _gore)
    {
        g.age += dt;
        if (g.lift > 0.f || g.liftVel > 0.f)
        {
            g.pos = Vector2Add(g.pos, Vector2Scale(g.vel, dt));
            g.lift += g.liftVel * dt;
            g.liftVel -= 1500.f * dt;                    // gravity
            g.spin += 540.f * dt;
            if (g.lift <= 0.f)
            {
                g.lift = 0.f;
                if (g.liftVel < -260.f)                  // one small bounce, then rest
                {
                    g.liftVel *= -0.28f;
                    g.vel = Vector2Scale(g.vel, 0.45f);
                }
                else
                {
                    g.liftVel = 0.f;
                    g.vel = Vector2Zero();
                }
            }
        }
    }
    _gore.erase(std::remove_if(_gore.begin(), _gore.end(),
        [](const GoreChunk& g) { return g.age >= g.life; }), _gore.end());

    // Impact sparks — fly out, slow down, fade.
    for (auto& s : _sparks)
    {
        s.timer += dt;
        s.pos = Vector2Add(s.pos, Vector2Scale(s.vel, dt));
        s.vel = Vector2Scale(s.vel, 1.f - 6.f * dt);   // drag
    }
    _sparks.erase(
        std::remove_if(_sparks.begin(), _sparks.end(),
            [](const Spark& s) { return s.timer >= s.life; }),
        _sparks.end());
}

void VFXManager::Draw(Vector2 worldOffset, Vector2 playerWorldPos, Vector2 playerCastOrigin)
{
    const Vector2 centre{ kVirtualWidth * .5f, kVirtualHeight * .5f };
    // Death shockwaves: a white flash disc for the first few frames, then a
    // fast-expanding ring in the creature's colour.
    for (const auto& w : _shockwaves)
    {
        const float t = std::clamp(w.age / w.life, 0.f, 1.f);
        const Vector2 pos = Vector2Add(Vector2Add(w.pos, worldOffset), centre);
        if (w.age < 0.07f)
        {
            const float flashRadius = w.radius * 0.45f * (1.f - w.age / 0.07f * 0.4f);
            const float flashAlpha = 0.85f * (1.f - w.age / 0.07f);
            Px::Disc(pos, flashRadius, Fade(WHITE, flashAlpha), Balance::Vfx::kGlowShockwaveStrength);
        }
        const float eased = 1.f - (1.f - t) * (1.f - t) * (1.f - t);
        const float r = 12.f + w.radius * eased;
        const float thickness = 7.f * (1.f - t) + 1.f;
        const float ringAlpha = 0.75f * (1.f - t);
        Px::Ring(pos, r - thickness, r, Fade(w.color, ringAlpha), Balance::Vfx::kGlowShockwaveStrength);
        Px::Circle(pos, r * 0.62f, Fade(WHITE, 0.45f * (1.f - t)));
    }
    // Gore chunks in flight (landed ones are drawn with the ground decals):
    // grid-snapped pixel chunks, a whole number of grid cells across.
    for (const auto& g : _gore)
    {
        if (g.lift <= 0.f && g.liftVel == 0.f) continue;
        const Vector2 pos = Vector2Add(Vector2Add(g.pos, worldOffset), centre);
        DrawEllipse((int)pos.x, (int)pos.y, g.size * 0.6f, g.size * 0.25f, Fade(BLACK, 0.25f));
        const int size = std::max(Px::Grid(), Px::Snap(g.size));
        DrawRectangle(Px::Snap(pos.x - g.size * 0.5f), Px::Snap(pos.y - g.lift - g.size * 0.5f), size, size, g.color);
    }

    for (const auto& accent : _contactAccents)
    {
        const float t = std::clamp(accent.age / accent.lifetime, 0.f, 1.f);
        const float fade = (1.f - t) * (1.f - t);
        const Vector2 pos = Vector2Add(Vector2Add(accent.pos, worldOffset),
                                      { kVirtualWidth * .5f, kVirtualHeight * .5f });
        const float radius = (accent.heavy ? 18.f : 10.f) + t * (accent.heavy ? 38.f : 20.f);
        if (accent.heavy)
            Px::Circle(pos, radius + 2.f, Fade(accent.color, fade * .65f), Balance::Vfx::kGlowSlashStrength);
        const Vector2 tangent{ -accent.direction.y, accent.direction.x };
        const Vector2 along = Vector2Scale(tangent, radius * (1.f - .5f * t));
        const float slashThickness = (accent.heavy ? 4.f : 2.f) * (1.f - t);
        Px::Line(Vector2Subtract(pos, along), Vector2Add(pos, along), slashThickness, Fade(RAYWHITE, fade));
        Vfx::Glow().AddLine(Vector2Subtract(pos, along), Vector2Add(pos, along), slashThickness,
                            Fade(accent.color, fade * Balance::Vfx::kGlowSlashStrength));
        const Vector2 cross = Vector2Scale(accent.direction, radius * .4f);
        Px::Line(Vector2Subtract(pos, cross), Vector2Add(pos, cross), 2.f * (1.f - t), Fade(accent.color, fade));
    }
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
        screenPos.x += kVirtualWidth  / 2.f;
        screenPos.y += kVirtualHeight / 2.f;

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
        // Lingering decals fade out over their final second so they don't pop off.
        Color tint = effect.tint;
        if (effect.looping && effect.lifeRemaining < 1.f)
        {
            float k = (effect.lifeRemaining > 0.f) ? effect.lifeRemaining : 0.f;
            tint = Fade(effect.tint, (effect.tint.a / 255.f) * k);
        }
        const Vector2 origin{ dest.width * 0.5f, dest.height * 0.5f };
        if (effect.silhouette)
        {
            // Colour follows the heat ramp across the animation.
            const float progress = std::clamp((effect.frame + effect.runningTime / effect.frameTime) / effect.frameCount,
                                              0.f, 1.f);
            tint = Vfx::HeatRamp(effect.tint, progress);
            Vfx::Silhouette().Begin();
            DrawTexturePro(*effect.texture, source, dest, origin, rotation, tint);
            Vfx::Silhouette().End();
        }
        else
            DrawTexturePro(*effect.texture, source, dest, origin, rotation, tint);
        if (effect.glowStrength > 0.f)
            Vfx::Glow().AddSprite(*effect.texture, source, dest, origin, rotation,
                                  Fade(tint, (tint.a / 255.f) * effect.glowStrength), effect.silhouette);
    }
}

void VFXManager::DrawGroundDecals(Vector2 worldOffset) const
{
    const Vector2 centre{ kVirtualWidth * .5f, kVirtualHeight * .5f };
    // Gore that has come to rest stays on the floor briefly, then fades.
    for (const auto& g : _gore)
    {
        if (g.lift > 0.f || g.liftVel != 0.f) continue;
        const float fade = std::clamp((g.life - g.age) / 0.6f, 0.f, 1.f);
        const Vector2 pos = Vector2Add(Vector2Add(g.pos, worldOffset), centre);
        const int width = std::max(Px::Grid(), Px::Snap(g.size));
        const int height = std::max(Px::Grid(), Px::Snap(g.size * 0.6f));   // flattened on the floor
        DrawRectangle(Px::Snap(pos.x - g.size * 0.5f), Px::Snap(pos.y - g.size * 0.3f), width, height,
                      Fade(g.color, fade));
    }
}

void VFXManager::DrawSparks(Vector2 worldOffset) const
{
    const Vector2 centre{ kVirtualWidth * .5f, kVirtualHeight * .5f };
    const int   px  = Px::Grid();
    const float pxf = (float)px;
    for (const auto& s : _sparks)
    {
        const float age = std::clamp(s.timer / s.life, 0.f, 1.f);
        const Vector2 head = Vector2Add(Vector2Add(s.pos, worldOffset), centre);
        if (s.smoke)
        {
            // Puff: a square that grows a couple of pixels and fades. No glow.
            const int grow = (int)(Balance::Vfx::kSmokeGrowPixels * age + 0.5f);
            const int size = px * (1 + grow);
            DrawRectangle(Px::Snap(head.x) - px * (grow / 2), Px::Snap(head.y) - px * (grow / 2),
                          size, size, Fade(s.color, (s.color.a / 255.f) * (1.f - age)));
            continue;
        }

        // Hot streak: a few square pixels trailing behind the head along the
        // flight path, shortening as drag slows the spark. The head is the
        // hottest; each tail pixel looks a little older (cooler) on HeatRamp.
        const float speed = Vector2Length(s.vel);
        const Vector2 back = speed > 0.001f ? Vector2Scale(s.vel, -1.f / speed) : Vector2{ 0.f, 0.f };
        const float length = std::min(speed * Balance::Vfx::kSparkStreakSeconds, Balance::Vfx::kSparkMaxLength);
        const int pixels = std::max(1, (int)(length / pxf));
        const Color headColor = Vfx::HeatRamp(s.color, age);
        for (int i = 0; i < pixels; ++i)
        {
            const float pixelAge = std::min(1.f, age + Balance::Vfx::kSparkTailAgeLead * i / pixels);
            const Vector2 p = Vector2Add(head, Vector2Scale(back, i * pxf));
            DrawRectangle(Px::Snap(p.x), Px::Snap(p.y), px, px,
                          i == 0 ? headColor : Vfx::HeatRamp(s.color, pixelAge));
        }

        const Color glow = Fade(headColor, (headColor.a / 255.f) * Balance::Vfx::kGlowSparkStrength * (1.f - age));
        const float glowWidth = pxf * Balance::Vfx::kSparkGlowWidth;
        if (pixels > 1)
            Vfx::Glow().AddLine(head, Vector2Add(head, Vector2Scale(back, (pixels - 1) * pxf)), glowWidth, glow);
        else
            Vfx::Glow().AddCircle(head, glowWidth * 0.5f, glow);
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

    const float sw2 = kVirtualWidth  / 2.f;
    const float sh2 = kVirtualHeight / 2.f;

    for (const auto& ft : _floatingTexts)
    {
        float t      = (now - ft.spawnTime) / FloatingText::kLifetime;
        float yOff   = -55.f * t;
        float screenX = ft.worldPos.x + worldOffset.x + sw2;
        float screenY = ft.worldPos.y + worldOffset.y + sh2 + yOff;

        // Spawn "pop": briefly overshoot the font size, then settle.
        float pop  = (t < 0.18f) ? (1.f + (0.18f - t) / 0.18f * 0.6f) : 1.f;
        int   fs   = (int)(22.f * ft.scale * pop);
        const char* txt = ft.label.c_str();
        int   tw    = MeasureText(txt, fs);
        float alpha = 1.f - t;
        // Dark outline so big numbers read over any background.
        DrawText(txt, (int)(screenX - tw / 2.f) + 2, (int)screenY + 2, fs, Fade(BLACK, alpha * 0.6f));
        DrawText(txt, (int)(screenX - tw / 2.f),     (int)screenY,     fs, Fade(ft.color, alpha));
    }
}

void VFXManager::SpawnDeathBurst(Vector2 worldPos, Vector2 direction, Color gore, bool heavy)
{
    constexpr size_t kMaxShockwaves = 24, kMaxGore = 220;
    if (_shockwaves.size() < kMaxShockwaves)
    {
        Shockwave w;
        w.pos = worldPos;
        w.color = gore;
        w.radius = heavy ? 150.f : 100.f;
        w.life = heavy ? 0.36f : 0.28f;
        _shockwaves.push_back(w);
    }
    const float len = Vector2Length(direction);
    const Vector2 dir = len > 0.001f ? Vector2Scale(direction, 1.f / len) : Vector2{ 0.f, -1.f };
    const float baseAngle = atan2f(dir.y, dir.x);
    const int count = heavy ? 16 : 10;
    for (int i = 0; i < count && _gore.size() < kMaxGore; ++i)
    {
        GoreChunk g;
        g.pos = worldPos;
        // Most chunks follow the blow; a few scatter for volume.
        const bool follow = i < count * 2 / 3;
        const float spread = follow ? 0.9f : 3.14159f;
        const float angle = baseAngle + spread * ((float)GetRandomValue(-100, 100) / 100.f);
        const float speed = (float)GetRandomValue(follow ? 140 : 70, follow ? 300 : 180) * (heavy ? 1.2f : 1.f);
        g.vel = Vector2{ cosf(angle) * speed, sinf(angle) * speed * 0.6f };
        g.lift = 30.f + (float)GetRandomValue(0, 40);
        g.liftVel = (float)GetRandomValue(220, 380);
        g.size = (float)GetRandomValue(5, heavy ? 12 : 9);
        g.spin = (float)GetRandomValue(0, 359);
        g.life = 1.0f + (float)GetRandomValue(0, 50) / 100.f;
        const float shade = (float)GetRandomValue(70, 115) / 100.f;
        g.color = Color{ (unsigned char)std::min(255.f, gore.r * shade), (unsigned char)std::min(255.f, gore.g * shade),
                         (unsigned char)std::min(255.f, gore.b * shade), 255 };
        _gore.push_back(g);
    }
}

void VFXManager::Clear()
{
    _shockwaves.clear();
    _gore.clear();
    _dashDustActive = false;
    _contactAccents.clear();
    _sparks.clear();
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

    effect.glowStrength = Balance::Vfx::kGlowSpriteStrength;
    if (effect.active)
        _effects.push_back(effect);
}

void VFXManager::SpawnHitEffect(Character::CastType castType,
                                 Vector2 worldPos, Vector2 direction,
                                 float scaleMultiplier)
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
        // Melee: Hit03's one-colour star burst, redrawn by the silhouette shader
        // white-hot -> warm yellow -> ember. Its own red read as blood, so without
        // the shader it is skipped.
        effect.texture    = _genericHitTex;
        effect.frameCount = kHit03FrameCount;
        effect.frameTime  = Balance::Vfx::kMeleeImpactFrameTime;
        effect.scale      = Balance::Vfx::kMeleeImpactScale;
        effect.tint       = Vfx::kHitSparkColor;
        effect.silhouette = true;
        effect.active     = Vfx::Silhouette().Ready() && _genericHitTex != nullptr && _genericHitTex->id != 0;
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
        // Real lightning burst (Thunder_Blast: 64px cells, 11 frames — same
        // layout the electric ultimate draws). Falls back to the generic hit
        // if the asset is missing so electric never draws nothing.
        if (_thunderHitTex && _thunderHitTex->id != 0)
        {
            effect.texture     = _thunderHitTex;
            effect.frameWidth  = 64;
            effect.frameHeight = 64;
            effect.frameCount  = 11;
            effect.scale       = 2.5f;
            effect.tint        = WHITE;
        }
        else
        {
            effect.texture    = _genericHitTex;
            effect.frameCount = 5;
            effect.scale      = 4.f;
            effect.tint       = Color{ 255, 230, 120, 255 };   // at least tint it electric
        }
        break;
    default:
        effect.active = false;
        break;
    }

    if (effect.active)
    {
        effect.scale *= std::max(0.1f, scaleMultiplier);
        effect.glowStrength = Balance::Vfx::kGlowSpriteStrength;
        _effects.push_back(effect);
    }
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
    effect.glowStrength       = Balance::Vfx::kGlowSpriteStrength;
    _effects.push_back(effect);
}

void VFXManager::SpawnFloatingLabel(Vector2 worldPos, const char* text, Color color, float scale)
{
    if (text == nullptr || text[0] == '\0')
        return;
    FloatingText ft;
    ft.worldPos  = worldPos;
    ft.label     = text;
    ft.color     = color;
    ft.scale     = scale;
    ft.spawnTime = (float)GetTime();
    _floatingTexts.push_back(ft);
}

void VFXManager::SpawnSpriteFx(Texture2D* strip, Vector2 worldPos, int frameCount,
                               float scale, float frameTime, Color tint, Vector2 direction)
{
    if (strip == nullptr || strip->id == 0 || frameCount <= 0)
        return;
    AnimatedEffect effect{};
    effect.texture     = strip;
    effect.worldPos    = worldPos;
    effect.direction   = direction;
    effect.tint        = tint;
    effect.scale       = scale;
    effect.frameTime   = frameTime;
    effect.frameWidth  = 64;   // owned FX strips are 64px cells
    effect.frameHeight = 64;
    effect.frameCount  = frameCount;
    effect.active      = true;
    _effects.push_back(effect);
}

void VFXManager::SpawnHazardDecal(Texture2D* strip, Vector2 worldPos, int frameCount,
                                  float scale, float duration, Color tint, float frameTime)
{
    if (strip == nullptr || strip->id == 0 || frameCount <= 0 || duration <= 0.f)
        return;
    AnimatedEffect effect{};
    effect.texture       = strip;
    effect.worldPos      = worldPos;
    effect.direction     = { 1.f, 0.f };   // ground decal, no rotation
    effect.tint          = tint;
    effect.scale         = scale;
    effect.frameTime     = frameTime;
    effect.frameWidth    = 64;             // owned FX strips are 64px cells
    effect.frameHeight   = 64;
    effect.frameCount    = frameCount;
    effect.looping       = true;
    effect.lifeRemaining = duration;
    effect.glowStrength  = Balance::Vfx::kGlowHazardStrength;   // acid/poison/lava pools glow faintly
    effect.active        = true;
    _effects.push_back(effect);
}

void VFXManager::SpawnImpactBurst(Vector2 worldPos, Color color, int count, float speed)
{
    // Radial burst (kept for blocked-hit / non-directional callers).
    SpawnImpactBurst(worldPos, color, count, speed, Vector2{ 0.f, 0.f }, 0.f);
}

void VFXManager::SpawnImpactBurst(Vector2 worldPos, Color color, int count, float speed,
                                  Vector2 direction, float spreadRadians)
{
    const float dirLen = Vector2Length(direction);
    const bool  directional = dirLen > 0.001f;
    const float baseAng = directional ? atan2f(direction.y, direction.x) : 0.f;

    for (int i = 0; i < count; i++)
    {
        float ang;
        float spd;
        if (directional)
        {
            // Bias into a cone around the blow's travel — tighter, faster spray so
            // it reads as a cut kicking debris off the hit.
            ang = baseAng + ((float)GetRandomValue(-100, 100) / 100.f) * spreadRadians;
            spd = speed * (0.6f + (float)GetRandomValue(0, 100) / 100.f * 0.7f);
        }
        else
        {
            ang = (float)GetRandomValue(0, 628) / 100.f;
            spd = speed * (0.4f + (float)GetRandomValue(0, 100) / 100.f * 0.6f);
        }
        Spark s;
        s.pos   = worldPos;
        s.vel   = Vector2{ cosf(ang) * spd, sinf(ang) * spd };
        s.color = color;
        s.timer = 0.f;
        s.life  = 0.22f + (float)GetRandomValue(0, 100) / 100.f * 0.18f;
        _sparks.push_back(s);
    }
}

void VFXManager::SpawnSmokeBurst(Vector2 worldPos, Color color)
{
    // Upward-biased, low-speed, wide-cone puff through the existing bounded
    // spark pool — a thin semantic wrapper, no parallel particle system. The
    // new particles are flagged as smoke so they draw as puffs, not hot streaks.
    const size_t first = _sparks.size();
    SpawnImpactBurst(worldPos, color, /*count*/ 10, /*speed*/ 40.f,
                     /*direction*/ Vector2{ 0.f, -1.f }, /*spreadRadians*/ 1.1f);
    for (size_t i = first; i < _sparks.size(); ++i)
        _sparks[i].smoke = true;
}

void VFXManager::SpawnMuzzleFlash(Vector2 worldPos, Vector2 direction, Color color)
{
    if (Vfx::Silhouette().Ready() && _genericHitTex != nullptr && _genericHitTex->id != 0)
    {
        AnimatedEffect effect{};
        effect.texture      = _genericHitTex;
        effect.worldPos     = worldPos;
        effect.direction    = direction;
        effect.tint         = color;
        effect.frameCount   = kHit03FrameCount;
        effect.frameTime    = Balance::Vfx::kMuzzleFlashFrameTime;
        effect.scale        = Balance::Vfx::kMuzzleFlashScale;
        effect.silhouette   = true;
        effect.glowStrength = Balance::Vfx::kGlowSpriteStrength;
        effect.active       = true;
        _effects.push_back(effect);
    }
    SpawnImpactBurst(worldPos, color, Balance::Vfx::kMuzzleSparkCount, Balance::Vfx::kMuzzleSparkSpeed,
                     direction, Balance::Vfx::kMuzzleSparkSpread);
}

void VFXManager::SpawnDustPuffs(Vector2 worldPos, Vector2 drift, int count)
{
    const size_t first = _sparks.size();
    SpawnImpactBurst(worldPos, Vfx::kDashDustColor, count, Balance::Vfx::kDashDustSpeed);
    for (size_t i = first; i < _sparks.size(); ++i)
    {
        _sparks[i].smoke = true;
        _sparks[i].vel = Vector2Add(_sparks[i].vel, drift);
    }
}

void VFXManager::UpdateDashDust(Vector2 feetWorldPos, bool dashing, float dt)
{
    const Vector2 moved = Vector2Subtract(feetWorldPos, _dashDustLastFeet);
    _dashDustLastFeet = feetWorldPos;
    if (!dashing)
    {
        _dashDustActive = false;
        return;
    }

    // Puffs drift back against the dash (none on its first frame, before it moves).
    const float movedLength = Vector2Length(moved);
    const Vector2 drift = movedLength > 0.001f
        ? Vector2Scale(moved, -Balance::Vfx::kDashDustDrift / movedLength) : Vector2{ 0.f, 0.f };
    if (!_dashDustActive)
    {
        _dashDustActive = true;
        _dashDustTimer = 0.f;
        SpawnDustPuffs(feetWorldPos, drift, Balance::Vfx::kDashDustBurstPuffs);
        return;
    }
    _dashDustTimer += dt;
    while (_dashDustTimer >= Balance::Vfx::kDashDustTrailInterval)
    {
        _dashDustTimer -= Balance::Vfx::kDashDustTrailInterval;
        SpawnDustPuffs(feetWorldPos, drift, 1);
    }
}

void VFXManager::SpawnContactAccent(Vector2 worldPos, Vector2 direction, Color color, bool heavy)
{
    constexpr std::size_t kMaxContactAccents = 48;
    if (_contactAccents.size() >= kMaxContactAccents) _contactAccents.erase(_contactAccents.begin());
    if (Vector2LengthSqr(direction) < .001f) direction = { 1.f, 0.f };
    else direction = Vector2Normalize(direction);
    _contactAccents.push_back({ worldPos, direction, color, 0.f, heavy ? .22f : .14f, heavy });
}
