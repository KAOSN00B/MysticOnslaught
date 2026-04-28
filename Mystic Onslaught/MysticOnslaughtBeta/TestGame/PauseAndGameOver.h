#pragma once

#include "raylib.h"
#include "KeyBindings.h"

class PauseAndGameOver
{
public:

    ~PauseAndGameOver();

    void Init();
    void Unload();

    int  DrawPause();      // 0=nothing  1=resume  2=howtoplay  3=quit  4=keybindings  5=mainmenu
    bool DrawKeybindings(KeyBindings& bindings);
    int  DrawGameOver();   // 0=nothing  1=retry  2=main menu  3=quit

private:

    Texture2D _borderTex{};
    Texture2D _btnTex{};
    Texture2D _htpBtnTex{};

    int _rebindingSlot = -1;   // -1 = not rebinding
};
