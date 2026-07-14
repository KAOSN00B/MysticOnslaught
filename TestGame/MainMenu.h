#pragma once

#include "raylib.h"
#include "WebGamepad.h"
#include <vector>
#include <string>

class MainMenu
{
public:

    ~MainMenu();

    void Init();
    void Update();
    void Draw();

    bool StartPressed()          const;
    bool QuitPressed()           const;
    bool ContinuePressed()       const;
    bool DebugPressed()          const;
    bool DungeonRunPressed()     const;
    bool TileMapperPressed()     const;
    bool NineSliceEditorPressed() const;
    bool CharacterAnimatorPressed() const;
    bool SettingsPressed()        const;
    void SetDebugUnlocked(bool unlocked);

private:

    struct Button
    {
        std::string text;
        Rectangle   bounds;
        bool        hovered  = false;
        bool        selected = false; // true when navigated to by gamepad
    };
    std::vector<Button> _buttons;

    // Gamepad navigation (panel buttons 0-3 only)
    int   _gpSelected      = -1;
    float _gpStickCooldown = 0.f;

    Texture2D _borderTex{};
    Texture2D _bannerTex{};
    Texture2D _playBtnTex{};
    Texture2D _continueBtnTex{};

    bool _startPressed         = false;
    bool _quitPressed          = false;
    bool _continuePressed      = false;
    bool _debugPressed         = false;
    bool _dungeonRunPressed      = false;
    bool _tileMapperPressed      = false;
    bool _nineSliceEditorPressed = false;
    bool _charAnimatorPressed    = false;
    bool _settingsPressed        = false;
    bool _debugUnlocked          = false;
    bool _devToolsVisible        = false; // toggled by \ key

    // ── Border panel editor ───────────────────────────────────────────────────
public:
    void ToggleBorderEditor() { _editorActive = !_editorActive; }
private:
    bool      _editorActive = false;
    Rectangle _edRect       = {};
    int       _edHandle     = -1;
    Vector2   _edDragStart  = {};
    Rectangle _edRectStart  = {};

    // Banner drag (Y only)
    float _bannerEdY         = 0.f;
    bool  _bannerDragging    = false;
    float _bannerDragStartMY = 0.f;
    float _bannerDragStartY  = 0.f;

    // Button group drag (Y only)
    float _btnEdFirstY      = 0.f;
    bool  _btnDragging      = false;
    float _btnDragStartMY   = 0.f;
    float _btnDragStartFY   = 0.f;
};
