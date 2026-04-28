#include "RunStateController.h"

void RunStateController::OpenHowToPlay(GameState from, bool touchMode)
{
    _howToPlayFrom = from;
    _howToPlayTab = touchMode ? 3 : 0;
    _howToPlaySlideOffset = 0.f;
    _state = GameState::HowToPlay;
}

void RunStateController::ReturnFromHowToPlay()
{
    _state = _howToPlayFrom;
}
