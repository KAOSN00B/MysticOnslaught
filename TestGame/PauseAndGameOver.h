#pragma once

#include "raylib.h"
#include "InputPrompts.h"

class PauseAndGameOver
{
public:

    ~PauseAndGameOver();

    void Init();
    void Unload();

    // Input only (call from Update). Returns the chosen action:
    // 0=nothing  1=resume  2=howtoplay  3=quit  4=keybindings  5=mainmenu  6=settings  7=grimoire
    // 8=character sheet (also Q / E / Tab or a shoulder button, like switching tabs)
    int  UpdatePause();
    // Render only; never reads clicks or changes menu state. During a resumable
    // run the exits read "Save & ..." (leaving is not a death).
    void DrawPause(InputPromptMode promptMode, bool runInProgress) const;

private:
    // Geometry shared by UpdatePause (hit tests) and DrawPause (rendering).
    struct PauseLayout
    {
        float sw, sh;
        float btnW, btnH, btnGap, padV, titleH;
        float panelX, panelY, panelW, panelH;
        Rectangle borderRect;
        float naturalBtnY;
    };
    PauseLayout ComputePauseLayout() const;
    float       ActivePauseButtonY(const PauseLayout& layout) const;
    static Rectangle PauseButtonRect(const PauseLayout& layout, float activeBtnY, int index);

    Texture2D _borderTex{};
    Texture2D _btnTex{};
    Texture2D _htpBtnTex{};

    // Gamepad navigation state
    int   _gpPauseSelected        = 0;
    float _gpPauseStickCooldown   = 0.f;

    // ── Border panel editor ───────────────────────────────────────────────────
public:
    void ToggleBorderEditor() { _editorActive = !_editorActive; _editorInited = false; }
private:
    bool      _editorActive = false;
    bool      _editorInited = false;
    Rectangle _edRect       = {};
    int       _edHandle     = -1;
    Vector2   _edDragStart  = {};
    Rectangle _edRectStart  = {};

    // Button group drag (Y only)
    float _btnEdY         = 0.f;
    bool  _btnInited      = false;
    bool  _btnDragging    = false;
    float _btnDragStartMY = 0.f;
    float _btnDragStartY  = 0.f;
};
