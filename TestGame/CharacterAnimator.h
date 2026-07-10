#pragma once

#include "raylib.h"
#include "Enemy.h"
#include "CharacterTuning.h"
#include "AttackTuning.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

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
    enum class EditTarget { Body, Melee, Attack };

    struct CharacterEntry
    {
        std::string displayName;
        int meleeSlot = -1;
        std::function<Enemy*()> createInstance;
    };

    void OpenCharacter(int index);
    void CloseCharacter();
    void SaveCurrentTuning();
    void SaveCurrentAttackTuning();
    void DeleteCurrentTuning();
    void BuildAttackList();
    void SelectAttack(int index);
    void LoadCurrentAttackTuning();
    std::string CurrentAttackKey() const;
    const char* CurrentAttackLabel() const;
    void DrawAttackOverlay(Vector2 screenCenter);
    bool PlayerAttackEditorAvailable() const;
    void UpdateEditInput();
    void UpdateHandleDrag(Vector2 mouse);
    void DrawEditScreen();
    void DrawSelectScreen();
    void DrawOverlays(Vector2 screenCenter);
    void DrawSidePanel();
    void DrawHandle(Vector2 screenPos, bool hot, Color color) const;
    bool HandleHit(Vector2 mouse, Vector2 screenPos) const;

    void GetEffectiveBodyCircle(Vector2& outOffset, float& outRadius) const;
    bool MeleeEditableNow() const;
    Rectangle GetEffectiveMeleeRel() const;

    std::vector<CharacterEntry> _entries;
    Screen _screen = Screen::Select;
    int _selectedIdx = -1;
    bool _wantsToExit = false;

    std::unique_ptr<Enemy> _enemy;
    int _animIndex = 0;
    bool _animPaused = false;
    EditTarget _target = EditTarget::Body;

    struct AttackOption
    {
        std::string label;
        std::string key;
        bool basic = false;
    };
    std::vector<AttackOption> _attackOptions;
    int _attackIndex = 0;
    AttackTuning _attackTuning{};
    bool _attackTuningDirty = false;
    bool _attackPreviewActive = false;
    float _attackPreviewTimer = 0.f;
    float _attackPreviewNextShot = 0.f;
    std::vector<float> _attackPreviewShots;

    int _dragHandle = -1;
    Vector2 _dragAnchor{};

    bool _spriteDragActive = false;
    Vector2 _spriteDragStartMouse{};
    Vector2 _spriteDragStartOffset{};

    int _rowDrag = -1;
    float _rowDragStartX = 0.f;
    float _rowDragStartValue = 0.f;

    float _statusTimer = 0.f;
    std::string _statusText;
};