#pragma once

#include "raylib.h"
#include "Enemy.h"
#include "CharacterTuning.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

// ─────────────────────────────────────────────────────────────────────────────
// CharacterAnimator — dev tool (main menu corner button, like the Tile Editor).
// Previews a live instance of each enemy/boss, plays every animation, and lets
// you drag-edit PER-ANIMATION hitboxes directly over the sprite:
//
//   Body circle (cyan) — the character's body for this animation. Drives both
//     the solid capsule AND the hurt box (its bounding square), so one circle
//     per animation is all you tune.
//   Melee box (red)    — where the melee swing lands, shown on the attack
//     animation of characters that have one (slimes + the three bosses).
//
//   Screen 1 — character list.
//   Screen 2 — editor:
//     Q / E        previous / next animation     SPACE  pause    , .  step
//     TAB          toggle Body / Melee           F      flip facing
//     Left-drag    move / resize via the gold handles (clicking any overlay's
//                  handle selects it automatically)
//     Right-drag   nudge the sprite draw offset for this animation
//     C            copy this animation's body circle + draw offset to ALL anims
//     Scale / FPS  drag the side panel rows
//     S save file  DEL delete file  ESC back
// ─────────────────────────────────────────────────────────────────────────────
class CharacterAnimator
{
public:
    void Init();
    void Update();
    void Draw();
    void Unload();
    bool WantsToExit() const { return _wantsToExit; }

private:
    enum class Screen { Select, Edit };
    enum class EditTarget { Body, Melee };

    struct CharacterEntry
    {
        std::string displayName;                  // shown in the list; also the tuning name
        int meleeSlot = -1;                       // anim slot with a melee box; -1 = ranged
        std::function<Enemy*()> createInstance;
    };

    void OpenCharacter(int index);
    void CloseCharacter();
    void SaveCurrentTuning();
    void DeleteCurrentTuning();
    void UpdateEditInput();
    void UpdateHandleDrag(Vector2 mouse);
    void DrawEditScreen();
    void DrawSelectScreen();
    void DrawOverlays(Vector2 screenCenter);
    void DrawSidePanel();
    void DrawHandle(Vector2 screenPos, bool hot, Color color) const;
    bool HandleHit(Vector2 mouse, Vector2 screenPos) const;

    // Effective body circle for the current anim (falls back to the class
    // capsule when the anim has no explicit circle yet).
    void GetEffectiveBodyCircle(Vector2& outOffset, float& outRadius) const;
    bool MeleeEditableNow() const;                // current anim == entry melee slot
    Rectangle GetEffectiveMeleeRel() const;       // facing right, relative to worldPos

    std::vector<CharacterEntry> _entries;
    Screen     _screen       = Screen::Select;
    int        _selectedIdx  = -1;
    bool       _wantsToExit  = false;

    std::unique_ptr<Enemy> _enemy;                // live preview instance
    int   _animIndex   = 0;
    bool  _animPaused  = false;
    EditTarget _target = EditTarget::Body;

    // Drag state: which handle is captured (see kHandle* ids in the .cpp)
    int     _dragHandle = -1;
    Vector2 _dragAnchor{};                        // fixed opposite corner while resizing

    // Right-mouse sprite-offset drag
    bool    _spriteDragActive = false;
    Vector2 _spriteDragStartMouse{};
    Vector2 _spriteDragStartOffset{};

    // Value-row drag state (Scale / FPS rows in the side panel)
    int   _rowDrag       = -1;                    // 0 = scale, 1 = fps
    float _rowDragStartX = 0.f;
    float _rowDragStartValue = 0.f;

    float _statusTimer = 0.f;                     // "Saved!" style toast
    std::string _statusText;
};
