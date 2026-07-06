#include "PlayerPreview.h"
#include "AssetPaths.h"

Texture2D PlayerPreview::_sIdle{};
Texture2D PlayerPreview::_sWalk{};
Texture2D PlayerPreview::_sSwing{};
Texture2D PlayerPreview::_sStab{};
Texture2D PlayerPreview::_sHurt{};
Texture2D PlayerPreview::_sDeath{};
bool      PlayerPreview::_loaded = false;

namespace { constexpr const char* kPreviewHero = "Hero03"; }   // default look

PlayerPreview::PlayerPreview(Vector2 pos)
    : Enemy(pos)
{
}

void PlayerPreview::EnsureLoaded()
{
    if (_loaded) return;
    _sIdle  = LoadTexture(AssetPath(TextFormat("Hero/%s_Idle.png",   kPreviewHero)).c_str());
    _sWalk  = LoadTexture(AssetPath(TextFormat("Hero/%s_Walk.png",   kPreviewHero)).c_str());
    _sSwing = LoadTexture(AssetPath(TextFormat("Hero/%s_Attack.png", kPreviewHero)).c_str());
    _sStab  = LoadTexture(AssetPath(TextFormat("Hero/%s_Stab.png",   kPreviewHero)).c_str());
    _sHurt  = LoadTexture(AssetPath(TextFormat("Hero/%s_Hurt.png",   kPreviewHero)).c_str());
    _sDeath = LoadTexture(AssetPath(TextFormat("Hero/%s_Death.png",  kPreviewHero)).c_str());
    _loaded = true;
}

void PlayerPreview::Init()
{
    EnsureLoaded();

    // Map the hero sheets onto the base-character animation members so the shared
    // editor drawing / hitbox tooling works exactly like it does for enemies.
    _idleAnim       = _sIdle;
    _walkAnim       = _sWalk;
    _attackAnim     = _sSwing;
    _takeDamageAnim = _sHurt;
    _deathAnim      = _sDeath;

    _width  = 32.f;                       // hero frames are 32x32 (6-frame strips)
    _scale  = 6.f;                        // matches the live player's draw scale
    _texture = _idleAnim;
    _height  = _texture.height;
    if (_width > 0.f)
        _maxFrames = (int)(_texture.width / _width);
    if (_maxFrames < 1) _maxFrames = 1;

    // Reasonable starting body capsule so an un-tuned preview still shows a grabbable circle.
    _capsuleRadius     = 22.f;
    _capsuleHalfHeight = 20.f;
    _capsuleOffset     = { 0.f, 20.f };

    ApplyStoredTuning();                  // load any existing charactertuning_Player.txt
}

const char* PlayerPreview::GetEditorAnimName(int index) const
{
    static const char* kNames[4] = { "Idle", "Walk", "Swing", "Stab" };
    return (index >= 0 && index < 4) ? kNames[index] : "";
}

void PlayerPreview::PlayEditorAnim(int index)
{
    const Texture2D* sheets[4] = { &_idleAnim, &_walkAnim, &_attackAnim, &_sStab };
    if (index < 0 || index > 3)
        return;

    _texture = *sheets[index];
    if (_width > 0.f)
        _maxFrames = (int)(_texture.width / _width);
    if (_maxFrames < 1)
        _maxFrames = 1;
    _frame       = 0;
    _runningTime = 0.f;

    float frameTimeOverride = GetEditorAnimFrameTime(index);
    if (frameTimeOverride > 0.f)
        _updateTime = frameTimeOverride;
}
