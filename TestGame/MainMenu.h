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
    bool HowToPressed()          const;
    bool DebugPressed()          const;
    bool DungeonRunPressed()     const;
    bool TileMapperPressed()     const;
    bool NineSliceEditorPressed() const;
    bool SettingsPressed()        const;
    void SetDebugUnlocked(bool unlocked);

private:

    struct Button
    {
        std::string text;
        Rectangle bounds;
        bool hovered = false;
    };
    std::vector<Button> _buttons;

    Texture2D _borderTex{};
    Texture2D _bannerTex{};
    Texture2D _playBtnTex{};
    Texture2D _htpBtnTex{};

    bool _startPressed         = false;
    bool _quitPressed          = false;
    bool _howToPressed         = false;
    bool _debugPressed         = false;
    bool _dungeonRunPressed      = false;
    bool _tileMapperPressed      = false;
    bool _nineSliceEditorPressed = false;
    bool _settingsPressed        = false;
    bool _debugUnlocked          = false;

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