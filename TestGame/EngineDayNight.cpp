// =============================================================================
// EngineDayNight.cpp - the time of day in play (DayNight.h, VfxNight.h).
//
// The clock runs whenever you play, in the village and in the dungeon, indoors
// or out, and stops with the game (menus, pause, card picks). It is saved in
// the profile (village.clock) and never shows on screen; the debug juice panel
// (J in debug mode) can set the hour and fast-forward it. The look of the hour
// is drawn only outdoors: the village and the outdoor biomes.
// =============================================================================

#include "Engine.h"
#include "DayNight.h"
#include "VfxNight.h"

void Engine::UpdateDayNightClock(float dt)
{
    if (_gameState != GameState::DungeonRun && _gameState != GameState::Village) return;
    _clockHours = DayNight::AdvanceClock(_clockHours, dt, _dayClockSpeed);
    _meta.SetClockHours(_clockHours);   // written with the next calm save, never on its own
}

void Engine::ApplyTimeOfDay(bool outdoors, const Vector2* playerScreen) const
{
    if (!outdoors) return;
    if (playerScreen)
    {
        using namespace Balance::DayNight;
        Vfx::Night().AddLight(*playerScreen, kPlayerLightRadius,
                              Color{ kPlayerLightColor.r, kPlayerLightColor.g, kPlayerLightColor.b,
                                     (unsigned char)(kPlayerLightStrength * 255.f) });
    }
    Vfx::Night().Composite(_virtualCanvas, DayNight::LookAt(_clockHours));
}
