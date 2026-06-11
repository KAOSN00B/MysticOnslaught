#pragma once

#include "raylib.h"

class PauseAndGameOver
{
public:

    ~PauseAndGameOver();

    void Init();
    void Unload();

    int  DrawPause();      // 0=nothing  1=resume  2=howtoplay  3=quit  4=keybindings  5=mainmenu
    int  DrawGameOver();   // 0=nothing  1=retry  2=main menu  3=quit

private:

    Texture2D _borderTex{};
    Texture2D _btnTex{};
    Texture2D _htpBtnTex{};

    // Gamepad navigation state
    int   _gpPauseSelected        = 0;
    float _gpPauseStickCooldown   = 0.f;
    int   _gpGameOverSelected     = 0;
    float _gpGameOverStickCooldown = 0.f;

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
