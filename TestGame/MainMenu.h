#pragma once

#include "raylib.h"
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
    // A saved profile exists: Continue leads the menu and New Game asks for
    // confirmation before StartPressed() fires (the engine then erases it).
    void SetHasProfile(bool hasProfile);
    // Shown under Continue while a run is suspended ("" = none); New Game then
    // also warns that the suspended run will be lost.
    void SetRunCaption(const std::string& caption);
    bool IsConfirmingNewGame() const { return _confirmNewGame; }
    // Credits overlay (Main Menu > Credits); the engine holds its own menu
    // input (ascension arrows, dev keys) while it is open.
    bool IsShowingCredits() const { return _showCredits; }
    void OpenCredits() { _showCredits = true; _creditsScroll = 0.f; _creditsIdle = 0.f; }
    void CloseCredits() { _showCredits = false; }
    void ScrollCredits(float px) { _creditsScroll += px; _creditsIdle = 0.f; }   // clamped on the next Update

private:

    struct Button
    {
        std::string text;
        Rectangle   bounds;
        bool        hovered  = false;
        bool        selected = false; // true when navigated to by gamepad
    };
    std::vector<Button> _buttons;

    // Gamepad navigation (panel buttons only; order depends on the profile)
    int   _gpSelected      = -1;
    int   PanelCount() const;
    static bool IsPanelLabel(const std::string& text);
    Rectangle PanelBorderRect() const;
    bool  _hasProfile      = false;
    std::string _runCaption;
    bool  _confirmNewGame  = false;   // "erase progress?" modal open
    int   _confirmCursor   = 1;       // 0 = erase and start, 1 = cancel (safe default)
    void  UpdateNewGameConfirm();
    void  DrawNewGameConfirm() const;
    bool  _showCredits     = false;
    float _creditsScroll   = 0.f;    // px scrolled into the credits list
    float _creditsIdle     = 0.f;    // seconds since the last manual scroll (auto-scroll after a beat)
    void  UpdateCredits();
    void  DrawCredits() const;
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
