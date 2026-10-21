#pragma once

// =============================================================================
// VfxGlow.h - glow (bloom) for effects, plus the red hurt flash.
//
// Glow is light, not paint. While an effect draws itself normally, it also
// records a bright copy of itself here (Glow().AddSprite / AddHalo / AddCircle /
// AddRing / AddLine, in virtual-canvas pixels). Once per frame the engine calls
// Glow().Composite(canvas), which:
//   1. draws every recorded copy into a half-size light buffer, additively, so
//      overlapping lights get brighter;
//   2. blurs it with a two-pass Gaussian shader, then shrinks and blurs it
//      twice more (quarter and eighth size) for wider, softer light;
//   3. adds all three onto the canvas through a soft-knee shader, so piled-up
//      light brightens smoothly instead of clipping to flat white.
// Recording is presentation output, like a draw call: it never changes game
// state, so const Draw functions may record. Tuning lives in Balance::Vfx.
// With Balance::Vfx::kGlowEnabled off, or if a shader fails to compile, halos
// fall back to the old flat additive disc and nothing else glows.
//
// At night the same recording also lights the ground (ForEachSource, read by
// VfxNight.h), so anything that glows carries its own light.
//
// Also here: HeatRamp (particle colour over its life: white-hot, own colour,
// dark ember) and the silhouette shader, which redraws one-colour effect art
// (Hit03) in any colour so it can follow the same ramp.
// =============================================================================

#include "raylib.h"
#include "rlgl.h"
#include "GameBalance.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace Vfx
{
    // Colour the hurt sprite is multiplied by on its first frame. The hurt art
    // opens on a white frame, so a red tint turns that frame red.
    inline constexpr Color kHurtFlashTint{ 255, 70, 70, 255 };

    // Glow colours, one per effect family.
    inline constexpr Color kSpitGlow  { 120, 255,  60, 255 };   // toxic glob
    inline constexpr Color kFireGlow  { 255, 140,  40, 255 };   // fire bolt
    inline constexpr Color kArrowGlow { 255, 235, 170, 255 };   // arrow
    inline constexpr Color kGoldGlow  { 255, 210,  60, 255 };   // gold coin
    inline constexpr Color kHealGlow  { 120, 255, 150, 255 };   // heal orb
    inline constexpr Color kCellGlow  { 190, 130, 255, 255 };   // magic cell
    inline constexpr Color kDeathGlow { 255, 240, 220, 255 };   // kill burst

    // Hit sparks and the melee impact burst. Warm, never red: red read as blood.
    inline constexpr Color kHitSparkColor  { 255, 214, 110, 255 };
    inline constexpr Color kCritSparkColor { 255, 160,  40, 255 };
    // Spell bolt cast sparks (fire uses kFireGlow).
    inline constexpr Color kIceGlow        { 130, 210, 255, 255 };
    inline constexpr Color kShockGlow      { 255, 240, 120, 255 };
    // Dash dust puffs: a light, neutral dust that reads on every floor.
    inline constexpr Color kDashDustColor  { 214, 204, 184, 190 };

    // Colour of a particle at `age` (0 = born, 1 = gone): white-hot first, then
    // its own colour, cooling to a dark ember. Alpha is kept.
    inline Color HeatRamp(Color base, float age)
    {
        const float hot = Balance::Vfx::kHeatWhiteFraction;
        const Color ember{ (unsigned char)(base.r * Balance::Vfx::kHeatEmberDarken),
                           (unsigned char)(base.g * Balance::Vfx::kHeatEmberDarken),
                           (unsigned char)(base.b * Balance::Vfx::kHeatEmberDarken), base.a };
        Color c = age < hot ? ColorLerp(WHITE, base, age / hot)
                            : ColorLerp(base, ember, (age - hot) / (1.f - hot));
        c.a = base.a;
        return c;
    }

    // Linear radial gradient for the fallback halo (glow disabled). Generated
    // once and kept for the life of the process; raylib releases it on close.
    inline Texture2D GetHaloTexture()
    {
        static const Texture2D halo = [] {
            Image gradient = GenImageGradientRadial(Balance::Vfx::kHaloTextureSize,
                                                    Balance::Vfx::kHaloTextureSize,
                                                    0.f, WHITE, BLACK);
            Texture2D texture = LoadTextureFromImage(gradient);
            UnloadImage(gradient);
            return texture;
        }();
        return halo;
    }

    // White dot whose alpha falls off as (1 - d^2)^2: full at the centre, zero
    // at the edge with no visible rim. The light buffer's round light source.
    inline Texture2D GetSoftDotTexture()
    {
        static const Texture2D dot = [] {
            const int size = Balance::Vfx::kHaloTextureSize;
            Image image = GenImageColor(size, size, BLANK);
            Color* pixels = (Color*)image.data;
            const float half = size * 0.5f;
            for (int y = 0; y < size; ++y)
                for (int x = 0; x < size; ++x)
                {
                    const float dx = (x + 0.5f - half) / half;
                    const float dy = (y + 0.5f - half) / half;
                    const float d2 = dx * dx + dy * dy;
                    const float falloff = d2 < 1.f ? (1.f - d2) * (1.f - d2) : 0.f;
                    pixels[y * size + x] = Color{ 255, 255, 255, (unsigned char)(falloff * 255.f) };
                }
            Texture2D texture = LoadTextureFromImage(image);
            UnloadImage(image);
            SetTextureFilter(texture, TEXTURE_FILTER_BILINEAR);
            return texture;
        }();
        return dot;
    }

    namespace GlowShaders
    {
        // Separable Gaussian blur, 9 taps from 5 reads (linear filtering blends
        // each tap pair). texelStep is one source texel along the blur axis.
        inline constexpr const char* kBlur = R"(#version 330
in vec2 fragTexCoord;
in vec4 fragColor;
uniform sampler2D texture0;
uniform vec2 texelStep;
out vec4 finalColor;
const float kOffsets[3] = float[](0.0, 1.3846153846, 3.2307692308);
const float kWeights[3] = float[](0.2270270270, 0.3162162162, 0.0702702703);
void main()
{
    vec3 sum = texture(texture0, fragTexCoord).rgb * kWeights[0];
    for (int i = 1; i < 3; ++i)
    {
        sum += texture(texture0, fragTexCoord + texelStep * kOffsets[i]).rgb * kWeights[i];
        sum += texture(texture0, fragTexCoord - texelStep * kOffsets[i]).rgb * kWeights[i];
    }
    finalColor = vec4(sum, 1.0);
}
)";

        // Sums the three blurred levels and adds them to the scene (the draw
        // uses additive blending). 1 - exp(-x) is the soft knee: faint light
        // passes almost unchanged, stacked light approaches white gradually.
        inline constexpr const char* kComposite = R"(#version 330
in vec2 fragTexCoord;
in vec4 fragColor;
uniform sampler2D texture0;
uniform sampler2D glowMid;
uniform sampler2D glowWide;
uniform vec3 levelWeights;
uniform float intensity;
out vec4 finalColor;
void main()
{
    vec3 light = texture(texture0, fragTexCoord).rgb * levelWeights.x
               + texture(glowMid,  fragTexCoord).rgb * levelWeights.y
               + texture(glowWide, fragTexCoord).rgb * levelWeights.z;
    finalColor = vec4(vec3(1.0) - exp(-light * intensity), 1.0);
}
)";

        // Draws a sprite as a flat silhouette in the draw's tint colour, keeping
        // only the texture's alpha.
        inline constexpr const char* kSilhouette = R"(#version 330
in vec2 fragTexCoord;
in vec4 fragColor;
uniform sampler2D texture0;
out vec4 finalColor;
void main()
{
    finalColor = vec4(fragColor.rgb, fragColor.a * texture(texture0, fragTexCoord).a);
}
)";
    }

    // True when LoadShaderFromMemory produced a working program (on failure
    // raylib returns id 0 or the default shader).
    inline bool ShaderCompiled(Shader shader)
    {
        return IsShaderValid(shader) && shader.id != rlGetShaderIdDefault();
    }

    // The silhouette shader (see GlowShaders::kSilhouette). Prepare once after the
    // window opens; until then, or if it fails, Ready() is false and callers
    // skip silhouette effects rather than draw the raw one-colour art.
    class SilhouetteShader
    {
    public:
        void Prepare()
        {
            if (_tried) return;
            _tried = true;
            _shader = LoadShaderFromMemory(nullptr, GlowShaders::kSilhouette);
            _ready = ShaderCompiled(_shader);
            if (!_ready) TraceLog(LOG_WARNING, "SILHOUETTE: shader failed to load; recoloured effects disabled");
        }
        bool Ready() const { return _ready; }
        void Begin() const { BeginShaderMode(_shader); }
        void End() const { EndShaderMode(); }
        void Unload()
        {
            if (_ready) UnloadShader(_shader);
            _shader = Shader{};
            _ready = false;
            _tried = false;
        }

    private:
        Shader _shader{};
        bool   _ready = false;
        bool   _tried = false;
    };

    inline SilhouetteShader& Silhouette()
    {
        static SilhouetteShader shader;
        return shader;
    }

    class GlowLayer
    {
    public:
        // True while glow is switched on and its shaders work.
        bool Active() const { return Balance::Vfx::kGlowEnabled && !_failed; }

        // -- Recording (virtual-canvas pixels) --------------------------------
        // A bright copy of a sprite, so the sprite's own shape and colours glow.
        // Same arguments as the DrawTexturePro call that drew it; `silhouette`
        // glows in the tint colour alone (for art drawn with the silhouette shader).
        void AddSprite(const Texture2D& texture, Rectangle source, Rectangle dest,
                       Vector2 origin, float rotation, Color tint, bool silhouette = false)
        {
            if (!Active() || texture.id == 0 || tint.a == 0) return;
            if (silhouette && !Silhouette().Ready()) return;
            Command c;
            c.shape = Shape::Sprite;
            c.texture = texture;
            c.source = source;
            c.dest = dest;
            c.origin = origin;
            c.rotation = rotation;
            c.color = tint;
            c.silhouette = silhouette;
            _commands.push_back(c);
        }
        // A soft round light with no rim; colour alpha is its strength.
        void AddHalo(Vector2 centre, float radius, Color color)
        {
            AddShape(Shape::Halo, centre, centre, radius, 0.f, color);
        }
        void AddCircle(Vector2 centre, float radius, Color color)
        {
            AddShape(Shape::Circle, centre, centre, radius, 0.f, color);
        }
        void AddRing(Vector2 centre, float innerRadius, float outerRadius, Color color)
        {
            AddShape(Shape::Ring, centre, centre, innerRadius, outerRadius, color);
        }
        void AddLine(Vector2 from, Vector2 to, float thickness, Color color)
        {
            AddShape(Shape::Line, from, to, thickness, 0.f, color);
        }

        // Drops this frame's recording without drawing it.
        void Discard() { _commands.clear(); }

        // -- Reading back (night lighting, VfxNight.h) ------------------------
        // Calls fn(centre, radius, colour) for each glow recorded so far this
        // frame, as a round source where the effect is right now: a sprite's
        // centre and half its size, a halo/circle/ring's centre and outer
        // radius, and points spaced along a line (at most maxPointsPerLine).
        template <class Fn>
        void ForEachSource(float lineSpacing, int maxPointsPerLine, Fn&& fn) const
        {
            for (const Command& c : _commands)
            {
                switch (c.shape)
                {
                case Shape::Sprite:
                {
                    // dest.x/y is where the sprite's pivot lands; its centre sits
                    // (half size - pivot) from there, turned by the rotation.
                    const float ox = c.dest.width * 0.5f - c.origin.x;
                    const float oy = c.dest.height * 0.5f - c.origin.y;
                    const float turn = c.rotation * DEG2RAD;
                    const Vector2 centre{ c.dest.x + ox * cosf(turn) - oy * sinf(turn),
                                          c.dest.y + ox * sinf(turn) + oy * cosf(turn) };
                    fn(centre, std::max(c.dest.width, c.dest.height) * 0.5f, c.color);
                    break;
                }
                case Shape::Halo:
                case Shape::Circle:
                    fn(c.from, c.size, c.color);
                    break;
                case Shape::Ring:
                    fn(c.from, c.outerRadius, c.color);
                    break;
                case Shape::Line:
                {
                    const Vector2 along{ c.to.x - c.from.x, c.to.y - c.from.y };
                    const float length = sqrtf(along.x * along.x + along.y * along.y);
                    const int points = std::clamp((int)std::ceil(length / std::max(lineSpacing, 1.f)), 1,
                                                  std::max(maxPointsPerLine, 1));
                    const float radius = std::max(c.size, length / points * 0.5f);
                    for (int i = 0; i < points; ++i)
                    {
                        const float t = (i + 0.5f) / points;
                        fn(Vector2{ c.from.x + along.x * t, c.from.y + along.y * t }, radius, c.color);
                    }
                    break;
                }
                }
            }
        }

        // Creates the buffers and compiles the shaders up front, so the first
        // glowing effect does not stall a frame. Call once after the window
        // opens, outside any render texture.
        void Prepare(int canvasWidth, int canvasHeight)
        {
            if (Active()) EnsureResources(canvasWidth, canvasHeight);
        }

        // Renders, blurs and adds this frame's light onto `target`, then clears
        // the recording. Call while `target` is the active render texture and
        // no BeginMode2D, scissor, rlPushMatrix, blend or shader mode is open:
        // the passes switch render targets and re-enter `target` afterwards.
        void Composite(const RenderTexture2D& target)
        {
            if (_commands.empty()) return;
            if (!Active())
            {
                _commands.clear();
                return;
            }

            // Leave `target` first: this flushes what it has drawn so far. Loading
            // a render texture rebinds the window's framebuffer, so creating the
            // buffers while `target` still had unflushed draws would lose them.
            EndTextureMode();
            if (!EnsureResources(target.texture.width, target.texture.height))
            {
                _commands.clear();
                BeginTextureMode(target);
                return;
            }

            // 1. Light buffer: every recorded copy, additive, scaled to its size.
            BeginTextureMode(_level[0]);
            ClearBackground(BLACK);
            rlPushMatrix();
            rlScalef(1.f / Balance::Vfx::kGlowBufferDivisor, 1.f / Balance::Vfx::kGlowBufferDivisor, 1.f);
            BeginBlendMode(BLEND_ADDITIVE);
            for (const Command& c : _commands)
                DrawCommand(c);
            EndBlendMode();
            rlPopMatrix();
            EndTextureMode();
            _commands.clear();

            // 2. Blur level 0 in place; each smaller level is a shrunk, blurred
            //    copy of the one above it, so the light spreads further.
            BlurPass(_level[0], _scratch[0], Vector2{ 1.f, 0.f });
            BlurPass(_scratch[0], _level[0], Vector2{ 0.f, 1.f });
            for (int i = 1; i < kLevelCount; ++i)
            {
                BlurPass(_level[i - 1], _scratch[i], Vector2{ 1.f, 0.f });
                BlurPass(_scratch[i], _level[i], Vector2{ 0.f, 1.f });
            }
            const int wide = kLevelCount - 1;
            for (int pass = 1; pass < Balance::Vfx::kGlowWideBlurPasses; ++pass)
            {
                BlurPass(_level[wide], _scratch[wide], Vector2{ 1.f, 0.f });
                BlurPass(_scratch[wide], _level[wide], Vector2{ 0.f, 1.f });
            }

            // 3. Add the levels onto the scene in one draw.
            BeginTextureMode(target);
            BeginBlendMode(BLEND_ADD_COLORS);
            BeginShaderMode(_composite);
            const Vector3 weights{ Balance::Vfx::kGlowTightWeight, Balance::Vfx::kGlowMidWeight,
                                   Balance::Vfx::kGlowWideWeight };
            const float intensity = Balance::Vfx::kGlowIntensity;
            SetShaderValue(_composite, _levelWeightsLoc, &weights, SHADER_UNIFORM_VEC3);
            SetShaderValue(_composite, _intensityLoc, &intensity, SHADER_UNIFORM_FLOAT);
            SetShaderValueTexture(_composite, _midLoc, _level[1].texture);
            SetShaderValueTexture(_composite, _wideLoc, _level[2].texture);
            DrawTexturePro(_level[0].texture, FlippedSource(_level[0]),
                           Rectangle{ 0.f, 0.f, (float)target.texture.width, (float)target.texture.height },
                           Vector2{ 0.f, 0.f }, 0.f, WHITE);
            EndShaderMode();
            EndBlendMode();
        }

        // Releases the GPU buffers and shaders. Call before the window closes.
        void Unload()
        {
            if (!_loaded) return;
            for (int i = 0; i < kLevelCount; ++i)
            {
                UnloadRenderTexture(_level[i]);
                UnloadRenderTexture(_scratch[i]);
                _level[i] = RenderTexture2D{};
                _scratch[i] = RenderTexture2D{};
            }
            UnloadShader(_blur);
            UnloadShader(_composite);
            _blur = Shader{};
            _composite = Shader{};
            _loaded = false;
        }

    private:
        // Half, quarter and eighth size. The composite shader samples all three.
        static constexpr int kLevelCount = 3;

        enum class Shape { Sprite, Halo, Circle, Ring, Line };
        struct Command
        {
            Shape     shape = Shape::Halo;
            Texture2D texture{};
            Rectangle source{};
            Rectangle dest{};
            Vector2   origin{};         // sprite pivot
            float     rotation = 0.f;
            Vector2   from{};           // centre of halo/circle/ring; start of a line
            Vector2   to{};             // end of a line
            float     size = 0.f;       // halo/circle radius, ring inner radius, line thickness
            float     outerRadius = 0.f;
            Color     color = WHITE;
            bool      silhouette = false;   // sprite drawn as a flat tint-coloured shape
        };

        void AddShape(Shape shape, Vector2 from, Vector2 to, float size, float outerRadius, Color color)
        {
            if (!Active() || color.a == 0 || size <= 0.f) return;
            Command c;
            c.shape = shape;
            c.from = from;
            c.to = to;
            c.size = size;
            c.outerRadius = outerRadius;
            c.color = color;
            _commands.push_back(c);
        }

        static void DrawCommand(const Command& c)
        {
            switch (c.shape)
            {
            case Shape::Sprite:
                if (c.silhouette) Silhouette().Begin();
                DrawTexturePro(c.texture, c.source, c.dest, c.origin, c.rotation, c.color);
                if (c.silhouette) Silhouette().End();
                break;
            case Shape::Halo:
            {
                const Texture2D dot = GetSoftDotTexture();
                DrawTexturePro(dot, Rectangle{ 0.f, 0.f, (float)dot.width, (float)dot.height },
                               Rectangle{ c.from.x, c.from.y, c.size * 2.f, c.size * 2.f },
                               Vector2{ c.size, c.size }, 0.f, c.color);
                break;
            }
            case Shape::Circle:
                DrawCircleV(c.from, c.size, c.color);
                break;
            case Shape::Ring:
                DrawRing(c.from, c.size, c.outerRadius, 0.f, 360.f, Balance::Vfx::kGlowRingSegments, c.color);
                break;
            case Shape::Line:
                DrawLineEx(c.from, c.to, c.size, c.color);
                break;
            }
        }

        // Render textures are stored upside down; a negative source height
        // reads them the right way up.
        static Rectangle FlippedSource(const RenderTexture2D& rt)
        {
            return Rectangle{ 0.f, 0.f, (float)rt.texture.width, -(float)rt.texture.height };
        }

        // Blurs `from` along `axis` into `to` (which may be smaller: the pass
        // then also shrinks it).
        void BlurPass(const RenderTexture2D& from, const RenderTexture2D& to, Vector2 axis) const
        {
            const Vector2 step{ axis.x / (float)from.texture.width, axis.y / (float)from.texture.height };
            BeginTextureMode(to);
            ClearBackground(BLACK);
            BeginShaderMode(_blur);
            SetShaderValue(_blur, _texelStepLoc, &step, SHADER_UNIFORM_VEC2);
            DrawTexturePro(from.texture, FlippedSource(from),
                           Rectangle{ 0.f, 0.f, (float)to.texture.width, (float)to.texture.height },
                           Vector2{ 0.f, 0.f }, 0.f, WHITE);
            EndShaderMode();
            EndTextureMode();
        }

        // Creates the buffers and shaders on first use (needs the GL context).
        // Call with no render texture active (see Composite).
        bool EnsureResources(int targetWidth, int targetHeight)
        {
            if (_loaded) return true;
            if (_failed) return false;
            GetSoftDotTexture();   // created here, outside any light-buffer draw
            int divisor = Balance::Vfx::kGlowBufferDivisor;
            for (int i = 0; i < kLevelCount; ++i, divisor *= 2)
            {
                const int w = targetWidth / divisor, h = targetHeight / divisor;
                _level[i] = LoadRenderTexture(w, h);
                _scratch[i] = LoadRenderTexture(w, h);
                SetTextureFilter(_level[i].texture, TEXTURE_FILTER_BILINEAR);
                SetTextureFilter(_scratch[i].texture, TEXTURE_FILTER_BILINEAR);
            }
            _blur = LoadShaderFromMemory(nullptr, GlowShaders::kBlur);
            _composite = LoadShaderFromMemory(nullptr, GlowShaders::kComposite);
            _loaded = true;

            bool ok = ShaderCompiled(_blur) && ShaderCompiled(_composite);
            for (int i = 0; i < kLevelCount; ++i)
                ok = ok && IsRenderTextureValid(_level[i]) && IsRenderTextureValid(_scratch[i]);
            if (!ok)
            {
                TraceLog(LOG_WARNING, "GLOW: buffers or shaders failed to load; glow disabled");
                Unload();
                _failed = true;
                return false;
            }
            _texelStepLoc    = GetShaderLocation(_blur, "texelStep");
            _levelWeightsLoc = GetShaderLocation(_composite, "levelWeights");
            _intensityLoc    = GetShaderLocation(_composite, "intensity");
            _midLoc          = GetShaderLocation(_composite, "glowMid");
            _wideLoc         = GetShaderLocation(_composite, "glowWide");
            return true;
        }

        std::vector<Command> _commands;
        RenderTexture2D _level[kLevelCount]{};
        RenderTexture2D _scratch[kLevelCount]{};
        Shader _blur{};
        Shader _composite{};
        int  _texelStepLoc = -1;
        int  _levelWeightsLoc = -1;
        int  _intensityLoc = -1;
        int  _midLoc = -1;
        int  _wideLoc = -1;
        bool _loaded = false;
        bool _failed = false;
    };

    // The one glow layer the whole game records into.
    inline GlowLayer& Glow()
    {
        static GlowLayer layer;
        return layer;
    }

    // Adds a halo centred on screenPos. With glow on it becomes a soft light in
    // the glow layer; otherwise it is drawn at once as a flat additive disc.
    // Call with the blend mode in its default (alpha) state.
    inline void DrawGlowHalo(Vector2 screenPos, float radius, Color color, float strength)
    {
        if (Glow().Active())
        {
            Glow().AddHalo(screenPos, radius * Balance::Vfx::kGlowHaloCoreScale, Fade(color, strength));
            return;
        }
        const Texture2D halo = GetHaloTexture();
        const Rectangle source{ 0.f, 0.f, (float)halo.width, (float)halo.height };
        const Rectangle dest{ screenPos.x, screenPos.y, radius * 2.f, radius * 2.f };
        BeginBlendMode(BLEND_ADDITIVE);
        DrawTexturePro(halo, source, dest, Vector2{ radius, radius }, 0.f, Fade(color, strength));
        EndBlendMode();
    }
}
