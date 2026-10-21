#pragma once

// =============================================================================
// VfxNight.h - draws the time-of-day look (DayNight.h): tint, haze and, from
// evening to morning, light.
//
// No night art: the look recolours the art as drawn. Once per frame, after an
// outdoor world is drawn and before the glow and the HUD, the engine calls
// Night().Composite(canvas, look), which:
//   1. draws this frame's lights into a small light buffer with one texel per
//      effect pixel (Balance::Vfx::kFxPixelSize), so light falls on the same
//      square grid as the effects. Every glow recorded so far (VfxGlow.h)
//      becomes a light where it is right now, in its own colour, plus any light
//      added with AddLight (the player's own);
//   2. redraws the scene through a shader that tints, greys and hazes it by
//      the look, while light brings the art back toward its daytime colours
//      (never brighter), with a cast of the light's colour, stepped into a few
//      flat bands so it reads as pixel art.
// A plain look (full day) skips all of it. If a buffer or the shader fails to
// load, the world simply stays as drawn. Tuning lives in Balance::DayNight.
// =============================================================================

#include "raylib.h"
#include "rlgl.h"
#include "DayNight.h"
#include "GameBalance.h"
#include "VfxGlow.h"

#include <algorithm>
#include <vector>

namespace Vfx
{
    namespace NightShaders
    {
        // texture0 is the scene; lightMap holds this frame's light (added up).
        inline constexpr const char* kGrade = R"(#version 330
in vec2 fragTexCoord;
in vec4 fragColor;
uniform sampler2D texture0;
uniform sampler2D lightMap;
uniform vec3 tint;
uniform float saturation;
uniform vec3 haze;
uniform float hazeAmount;
uniform float lightGain;
uniform float lightBands;
uniform float lightTint;
out vec4 finalColor;
void main()
{
    vec4 scene = texture(texture0, fragTexCoord);
    // How lit this spot is: a soft knee, so piled-up light approaches full
    // gradually, then a few flat steps so it reads as pixel art. The light's
    // hue is kept apart, so the steps never split its colour.
    vec3 light = vec3(1.0) - exp(-texture(lightMap, fragTexCoord).rgb * lightGain);
    float amount = max(light.r, max(light.g, light.b));
    vec3 hue = amount > 0.0 ? light / amount : vec3(1.0);
    if (lightBands > 0.0) amount = floor(amount * lightBands + 0.5) / lightBands;
    float grey = dot(scene.rgb, vec3(0.299, 0.587, 0.114));
    vec3 graded = mix(vec3(grey), scene.rgb, saturation) * tint;
    // Light brings the art back toward its daytime colours, never past them,
    // with a cast of the light's own colour.
    vec3 litScene = scene.rgb * mix(vec3(1.0), hue, lightTint);
    vec3 lit = mix(graded, litScene, amount);
    finalColor = vec4(mix(lit, haze, hazeAmount), scene.a);
}
)";
    }

    class NightLayer
    {
    public:
        // A round light at `centre` (virtual-canvas pixels) for this frame;
        // colour alpha is its strength.
        void AddLight(Vector2 centre, float radius, Color color)
        {
            if (radius <= 0.f || color.a == 0) return;
            _lights.push_back(Light{ centre, radius, color });
        }

        // Creates the buffers and compiles the shader up front, so the first
        // evening does not stall a frame. Call once after the window opens,
        // outside any render texture.
        void Prepare(int canvasWidth, int canvasHeight) { EnsureResources(canvasWidth, canvasHeight); }

        // Grades `target` by `look`, then clears this frame's lights. Call while
        // `target` is the active render texture and no BeginMode2D, scissor,
        // rlPushMatrix, blend or shader mode is open: the passes switch render
        // targets and re-enter `target` afterwards. Call before
        // Glow().Composite, since it reads the glows recorded so far.
        void Composite(const RenderTexture2D& target, const DayNight::Look& look)
        {
            if (look.IsPlain())
            {
                _lights.clear();
                return;
            }

            // Leave `target` first: this flushes what it has drawn so far
            // (creating a buffer while it had unflushed draws would lose them).
            EndTextureMode();
            if (!EnsureResources(target.texture.width, target.texture.height))
            {
                _lights.clear();
                BeginTextureMode(target);
                return;
            }

            // 1. Light buffer.
            BeginTextureMode(_lightBuffer);
            ClearBackground(BLACK);
            if (look.lights > 0.f)
            {
                using namespace Balance::DayNight;
                rlPushMatrix();
                const float shrink = 1.f / (float)Balance::Vfx::kFxPixelSize;
                rlScalef(shrink, shrink, 1.f);
                BeginBlendMode(BLEND_ADDITIVE);
                Glow().ForEachSource(kLineLightSpacing, kMaxLightsPerLine,
                    [&](Vector2 centre, float radius, Color color)
                    {
                        const float reach = std::clamp(radius * kLightSizeFromGlow, kMinLightRadius, kMaxLightRadius);
                        DrawLight(centre, reach, LightColorForGlow(color),
                                  color.a / 255.f * kGlowLightStrength * look.lights);
                    });
                for (const Light& light : _lights)
                    DrawLight(light.centre, light.radius, light.color, light.color.a / 255.f * look.lights);
                EndBlendMode();
                rlPopMatrix();
            }
            EndTextureMode();
            _lights.clear();

            // 2. Copy the scene, so the shader can read it while writing back.
            //    Both copies replace (no blending), so the canvas keeps its alpha.
            const Rectangle full{ 0.f, 0.f, (float)target.texture.width, (float)target.texture.height };
            rlSetBlendFactors(RL_ONE, RL_ZERO, RL_FUNC_ADD);
            BeginTextureMode(_sceneCopy);
            BeginBlendMode(BLEND_CUSTOM);
            DrawTexturePro(target.texture, Flipped(target), full, Vector2{ 0.f, 0.f }, 0.f, WHITE);
            EndBlendMode();
            EndTextureMode();

            // 3. Grade it back into the scene.
            const Vector3 tint{ look.tint[0], look.tint[1], look.tint[2] };
            const Vector3 haze{ look.haze[0], look.haze[1], look.haze[2] };
            const float gain = Balance::DayNight::kLightGain;
            const float bands = (float)Balance::DayNight::kLightBands;
            const float lightTint = Balance::DayNight::kLightTint;
            BeginTextureMode(target);
            BeginBlendMode(BLEND_CUSTOM);
            BeginShaderMode(_grade);
            SetShaderValue(_grade, _tintLoc, &tint, SHADER_UNIFORM_VEC3);
            SetShaderValue(_grade, _saturationLoc, &look.saturation, SHADER_UNIFORM_FLOAT);
            SetShaderValue(_grade, _hazeLoc, &haze, SHADER_UNIFORM_VEC3);
            SetShaderValue(_grade, _hazeAmountLoc, &look.hazeAmount, SHADER_UNIFORM_FLOAT);
            SetShaderValue(_grade, _gainLoc, &gain, SHADER_UNIFORM_FLOAT);
            SetShaderValue(_grade, _bandsLoc, &bands, SHADER_UNIFORM_FLOAT);
            SetShaderValue(_grade, _lightTintLoc, &lightTint, SHADER_UNIFORM_FLOAT);
            SetShaderValueTexture(_grade, _lightMapLoc, _lightBuffer.texture);
            DrawTexturePro(_sceneCopy.texture, Flipped(_sceneCopy), full, Vector2{ 0.f, 0.f }, 0.f, WHITE);
            EndShaderMode();
            EndBlendMode();
        }

        // Releases the GPU buffers and shader. Call before the window closes.
        void Unload()
        {
            if (!_loaded) return;
            UnloadRenderTexture(_lightBuffer);
            UnloadRenderTexture(_sceneCopy);
            UnloadShader(_grade);
            _lightBuffer = RenderTexture2D{};
            _sceneCopy = RenderTexture2D{};
            _grade = Shader{};
            _loaded = false;
        }

    private:
        struct Light
        {
            Vector2 centre;
            float   radius;
            Color   color;
        };

        // Glows drawn in white carry a sprite's own colours; they light warm.
        static Color LightColorForGlow(Color glow)
        {
            const unsigned char white = Balance::DayNight::kWhiteLightMin;
            if (glow.r >= white && glow.g >= white && glow.b >= white)
            {
                const Balance::DayNight::Rgb warm = Balance::DayNight::kWhiteLightColor;
                return Color{ warm.r, warm.g, warm.b, 255 };
            }
            return Color{ glow.r, glow.g, glow.b, 255 };
        }

        static void DrawLight(Vector2 centre, float radius, Color color, float strength)
        {
            if (strength <= 0.f) return;
            const Texture2D dot = GetSoftDotTexture();
            color.a = (unsigned char)(std::clamp(strength, 0.f, 1.f) * 255.f);
            DrawTexturePro(dot, Rectangle{ 0.f, 0.f, (float)dot.width, (float)dot.height },
                           Rectangle{ centre.x, centre.y, radius * 2.f, radius * 2.f },
                           Vector2{ radius, radius }, 0.f, color);
        }

        // Render textures are stored upside down; a negative source height
        // reads them the right way up.
        static Rectangle Flipped(const RenderTexture2D& rt)
        {
            return Rectangle{ 0.f, 0.f, (float)rt.texture.width, -(float)rt.texture.height };
        }

        // Creates the buffers and shader on first use (needs the GL context).
        // Call with no render texture active (see Composite).
        bool EnsureResources(int targetWidth, int targetHeight)
        {
            if (_loaded) return true;
            if (_failed) return false;
            GetSoftDotTexture();   // created here, outside any light-buffer draw
            const int pixel = Balance::Vfx::kFxPixelSize;
            _lightBuffer = LoadRenderTexture(targetWidth / pixel, targetHeight / pixel);
            _sceneCopy = LoadRenderTexture(targetWidth, targetHeight);
            SetTextureFilter(_lightBuffer.texture, TEXTURE_FILTER_POINT);   // square light pixels
            _grade = LoadShaderFromMemory(nullptr, NightShaders::kGrade);
            _loaded = true;

            if (!ShaderCompiled(_grade) || !IsRenderTextureValid(_lightBuffer) || !IsRenderTextureValid(_sceneCopy))
            {
                TraceLog(LOG_WARNING, "NIGHT: buffers or shader failed to load; the time of day is not shown");
                Unload();
                _failed = true;
                return false;
            }
            _tintLoc       = GetShaderLocation(_grade, "tint");
            _saturationLoc = GetShaderLocation(_grade, "saturation");
            _hazeLoc       = GetShaderLocation(_grade, "haze");
            _hazeAmountLoc = GetShaderLocation(_grade, "hazeAmount");
            _gainLoc       = GetShaderLocation(_grade, "lightGain");
            _bandsLoc      = GetShaderLocation(_grade, "lightBands");
            _lightTintLoc  = GetShaderLocation(_grade, "lightTint");
            _lightMapLoc   = GetShaderLocation(_grade, "lightMap");
            return true;
        }

        std::vector<Light> _lights;
        RenderTexture2D _lightBuffer{};
        RenderTexture2D _sceneCopy{};
        Shader _grade{};
        int  _tintLoc = -1;
        int  _saturationLoc = -1;
        int  _hazeLoc = -1;
        int  _hazeAmountLoc = -1;
        int  _gainLoc = -1;
        int  _bandsLoc = -1;
        int  _lightTintLoc = -1;
        int  _lightMapLoc = -1;
        bool _loaded = false;
        bool _failed = false;
    };

    // The one time-of-day layer the game draws through.
    inline NightLayer& Night()
    {
        static NightLayer layer;
        return layer;
    }
}
