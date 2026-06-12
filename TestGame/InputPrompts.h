#pragma once

#include <string>

// Tracks whichever input family the player used most recently.
enum class InputPromptMode
{
    KeyboardMouse,
    Gamepad,
    Touch
};

inline const char* PromptSelect(InputPromptMode mode)
{
    switch (mode)
    {
    case InputPromptMode::Gamepad: return "A: Select";
    case InputPromptMode::Touch:   return "Tap: Select";
    default:                       return "Click: Select";
    }
}

inline const char* PromptBack(InputPromptMode mode)
{
    switch (mode)
    {
    case InputPromptMode::Gamepad: return "B: Back";
    case InputPromptMode::Touch:   return "Tap Back";
    default:                       return "Esc: Back";
    }
}

inline const char* PromptContinue(InputPromptMode mode)
{
    switch (mode)
    {
    case InputPromptMode::Gamepad: return "A: Continue";
    case InputPromptMode::Touch:   return "Tap to Continue";
    default:                       return "Space / Enter to Continue";
    }
}

inline const char* PromptSkip(InputPromptMode mode)
{
    switch (mode)
    {
    case InputPromptMode::Gamepad: return "A: Skip";
    case InputPromptMode::Touch:   return "Tap to Skip";
    default:                       return "Space / Enter to Skip";
    }
}

inline const char* PromptMove(InputPromptMode mode)
{
    switch (mode)
    {
    case InputPromptMode::Gamepad: return "Left Stick / D-Pad";
    case InputPromptMode::Touch:   return "Left Joystick";
    default:                       return "W / A / S / D";
    }
}

inline const char* PromptDash(InputPromptMode mode)
{
    switch (mode)
    {
    case InputPromptMode::Gamepad: return "A";
    case InputPromptMode::Touch:   return "DASH";
    default:                       return "SPACE";
    }
}

inline const char* PromptAttack(InputPromptMode mode)
{
    switch (mode)
    {
    case InputPromptMode::Gamepad: return "X";
    case InputPromptMode::Touch:   return "ATK";
    default:                       return "Left Click";
    }
}

inline const char* PromptPause(InputPromptMode mode)
{
    switch (mode)
    {
    case InputPromptMode::Gamepad: return "Start";
    case InputPromptMode::Touch:   return "Pause Button";
    default:                       return "ESC";
    }
}

inline const char* PromptAbilitySlot(InputPromptMode mode, int slot)
{
    static const char* keyboard[] = { "1", "2", "3", "4" };
    static const char* gamepad[]  = { "LB", "RB", "LT", "RT" };
    static const char* touch[]    = { "Tap", "Tap", "Tap", "Tap" };
    if (slot < 0 || slot >= 4) slot = 0;
    switch (mode)
    {
    case InputPromptMode::Gamepad: return gamepad[slot];
    case InputPromptMode::Touch:   return touch[slot];
    default:                       return keyboard[slot];
    }
}

inline std::string PromptAbilitySlots(InputPromptMode mode)
{
    switch (mode)
    {
    case InputPromptMode::Gamepad: return "LB  RB  LT  RT";
    case InputPromptMode::Touch:   return "Tap Ability Icons";
    default:                       return "1  2  3  4";
    }
}

inline std::string PromptAbilityAction(InputPromptMode mode)
{
    switch (mode)
    {
    case InputPromptMode::Gamepad: return "Press LB/RB/LT/RT to cast a learned ability.";
    case InputPromptMode::Touch:   return "Tap an ability icon to cast a learned ability.";
    default:                       return "Press 1-4 to cast a learned ability.";
    }
}

inline const char* PromptWorldMapHint(InputPromptMode mode)
{
    switch (mode)
    {
    case InputPromptMode::Gamepad: return "Left Stick / D-Pad: Choose   A: Select";
    case InputPromptMode::Touch:   return "Tap a highlighted biome to travel there";
    default:                       return "Click a highlighted biome to travel there";
    }
}

inline const char* PromptWorldMapYes(InputPromptMode mode)
{
    switch (mode)
    {
    case InputPromptMode::Gamepad: return "A: YEAH, LET'S GO";
    case InputPromptMode::Touch:   return "YEAH, LET'S GO";
    default:                       return "YES, LET'S GO";
    }
}

inline const char* PromptWorldMapNo(InputPromptMode mode)
{
    switch (mode)
    {
    case InputPromptMode::Gamepad: return "B: NO";
    case InputPromptMode::Touch:   return "NO";
    default:                       return "NOT YET";
    }
}

inline const char* PromptShopNpc(InputPromptMode mode)
{
    switch (mode)
    {
    case InputPromptMode::Gamepad: return "A: Shop";
    case InputPromptMode::Touch:   return "Enter Shop";
    default:                       return "[E] Shop";
    }
}

inline const char* PromptDialogueContinue(InputPromptMode mode)
{
    switch (mode)
    {
    case InputPromptMode::Gamepad: return "[ A ] Continue";
    case InputPromptMode::Touch:   return "Tap to Continue";
    default:                       return "[ E ] Continue";
    }
}