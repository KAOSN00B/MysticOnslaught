#pragma once

#include "raylib.h"
#include "KeyBindings.h"
#include "Leaderboard.h"
#include <vector>
#include <string>

class PauseAndGameOver
{
public:

    ~PauseAndGameOver();

    void Init();
    void Unload();

    int  DrawPause();      // 0=nothing  1=resume  2=howtoplay  3=quit  4=keybindings  5=mainmenu
    // Returns true when Back is pressed. Modifies bindings in place as rebinds happen.
    bool DrawKeybindings(KeyBindings& bindings);
    int  DrawGameOver(int wave, int kills, const std::vector<LeaderboardEntry>& scores);  // 0=nothing  1=play again  2=main menu  3=quit
    bool DrawLeaderboardScreen(const std::vector<LeaderboardEntry>& scores);  // returns true when Back is clicked

    // Returns "" while the player is still typing; returns the confirmed name when Enter is pressed
    std::string DrawNameEntry(int wave, int kills);
    void        ResetNameEntry();

private:

    Texture2D   _borderTex{};
    Texture2D   _btnTex{};
    Texture2D   _htpBtnTex{};

    std::string _nameBuffer;
    float       _cursorBlink  = 0.f;
    int         _rebindingSlot = -1;   // -1 = not rebinding
};
