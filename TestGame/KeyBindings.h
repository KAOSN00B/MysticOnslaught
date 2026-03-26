#pragma once
#include "raylib.h"

// All player-configurable key bindings in one place.
// Saved to / loaded from "keybindings.cfg" by Engine.
struct KeyBindings
{
    // Movement
    KeyboardKey moveUp    = KEY_W;
    KeyboardKey moveDown  = KEY_S;
    KeyboardKey moveLeft  = KEY_A;
    KeyboardKey moveRight = KEY_D;

    // Actions
    KeyboardKey dash      = KEY_SPACE;
    KeyboardKey attack    = KEY_NULL;   // optional; LMB always works as melee

    // Abilities (up to 4 slots; slot cap can be raised via upgrade)
    KeyboardKey ability[4] = { KEY_ONE, KEY_TWO, KEY_THREE, KEY_FOUR };
};

// Human-readable label for a KeyboardKey (used by ability bar and keybindings screen)
inline const char* GetKeyName(KeyboardKey k)
{
    switch (k)
    {
        case KEY_NULL:         return "---";
        case KEY_SPACE:        return "SPC";
        case KEY_ENTER:        return "ENT";
        case KEY_TAB:          return "TAB";
        case KEY_BACKSPACE:    return "BSP";
        case KEY_LEFT_SHIFT:   return "LSH";
        case KEY_RIGHT_SHIFT:  return "RSH";
        case KEY_LEFT_CONTROL: return "LCT";
        case KEY_RIGHT_CONTROL:return "RCT";
        case KEY_LEFT_ALT:     return "LAL";
        case KEY_RIGHT_ALT:    return "RAL";
        case KEY_CAPS_LOCK:    return "CAP";
        case KEY_UP:           return "UP";
        case KEY_DOWN:         return "DWN";
        case KEY_LEFT:         return "LFT";
        case KEY_RIGHT:        return "RGT";
        case KEY_F1:  return "F1";  case KEY_F2:  return "F2";  case KEY_F3:  return "F3";
        case KEY_F4:  return "F4";  case KEY_F5:  return "F5";  case KEY_F6:  return "F6";
        case KEY_F7:  return "F7";  case KEY_F8:  return "F8";  case KEY_F9:  return "F9";
        case KEY_F10: return "F10"; case KEY_F11: return "F11"; case KEY_F12: return "F12";
        case KEY_ZERO:  return "0"; case KEY_ONE:   return "1"; case KEY_TWO:   return "2";
        case KEY_THREE: return "3"; case KEY_FOUR:  return "4"; case KEY_FIVE:  return "5";
        case KEY_SIX:   return "6"; case KEY_SEVEN: return "7"; case KEY_EIGHT: return "8";
        case KEY_NINE:  return "9";
        case KEY_A: return "A"; case KEY_B: return "B"; case KEY_C: return "C";
        case KEY_D: return "D"; case KEY_E: return "E"; case KEY_F: return "F";
        case KEY_G: return "G"; case KEY_H: return "H"; case KEY_I: return "I";
        case KEY_J: return "J"; case KEY_K: return "K"; case KEY_L: return "L";
        case KEY_M: return "M"; case KEY_N: return "N"; case KEY_O: return "O";
        case KEY_P: return "P"; case KEY_Q: return "Q"; case KEY_R: return "R";
        case KEY_S: return "S"; case KEY_T: return "T"; case KEY_U: return "U";
        case KEY_V: return "V"; case KEY_W: return "W"; case KEY_X: return "X";
        case KEY_Y: return "Y"; case KEY_Z: return "Z";
        default:    return "?";
    }
}
