#pragma once

#include "GameTypes.h"

class RunStateController
{
public:
    GameState& StateRef() { return _state; }
    GameState& HowToPlayFromRef() { return _howToPlayFrom; }
    int& HowToPlayTabRef() { return _howToPlayTab; }
    float& HowToPlaySlideOffsetRef() { return _howToPlaySlideOffset; }

    GameState GetState() const { return _state; }
    void SetState(GameState state) { _state = state; }
    GameState GetHowToPlayFrom() const { return _howToPlayFrom; }

    void OpenHowToPlay(GameState from, bool touchMode);
    void ReturnFromHowToPlay();

private:
    GameState _state = GameState::Menu;
    GameState _howToPlayFrom = GameState::Menu;
    int _howToPlayTab = 0;
    float _howToPlaySlideOffset = 0.f;
};
