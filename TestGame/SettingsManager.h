#pragma once

class AudioManager;
struct KeyBindings;
struct GamepadBindings;

// All persistent player preferences.
struct GameSettings
{
    enum class WindowMode { Fullscreen, Borderless, Windowed };

    WindowMode windowMode     = WindowMode::Windowed;
    int        windowedWidth  = 1920;
    int        windowedHeight = 1080;
    bool       vsync          = true;

    float masterVolume = 1.0f;  // 0.0 - 1.0, applied via Raylib SetMasterVolume()
    float musicVolume  = 1.0f;  // 0.0 - 1.0, scales all music stream volumes
    float sfxVolume    = 1.0f;  // 0.0 - 1.0, applied to Sound objects before play

    // Comfort (Settings > Comfort). The engine applies these where the effect
    // is triggered, so every shake, flash and freeze goes through them.
    float screenShake  = 1.0f;  // 0.0 - 1.0, scales every camera shake
    int   flashLevel   = 0;     // full-screen flashes: 0 = full, 1 = reduced, 2 = off
    int   hitStopLevel = 0;     // impact freeze frames: 0 = full, 1 = reduced, 2 = off

    float FlashScale()   const { return flashLevel   == 0 ? 1.f : flashLevel   == 1 ? 0.3f : 0.f; }
    float HitStopScale() const { return hitStopLevel == 0 ? 1.f : hitStopLevel == 1 ? 0.5f : 0.f; }
};

// Saves/settings.json and Saves/keybindings.json (SaveFiles.h): plain JSON the
// player may edit. A field of the wrong type rejects the file (the backup is
// used, or the defaults); a number out of range is clamped as the menus would;
// a missing field keeps its default.
class SettingsManager
{
public:
    void Load();
    void Save() const;

    // Keybindings live beside the settings. Load returns false (and leaves
    // both untouched) when there is no usable file; a key code out of range
    // keeps that binding's default.
    static bool LoadKeybindings(KeyBindings& keys, GamepadBindings& pad);
    static bool SaveKeybindings(const KeyBindings& keys, const GamepadBindings& pad);

    // Apply window mode + VSync to the OS window.
    void ApplyWindow() const;
    // Push master/music/sfx volumes to the audio system.
    void ApplyVolumes(AudioManager& audio) const;

    GameSettings&       Get()       { return _settings; }
    const GameSettings& Get() const { return _settings; }

private:
    GameSettings _settings;

    // Cache of the window-relevant settings from the last time ApplyWindow()
    // actually touched the OS window. Window recreation (SetWindowSize /
    // ToggleFullscreen / window-state flags) stalls for a second or more on some
    // drivers, so ApplyWindow early-outs when none of these changed — that's why
    // exiting Settings or toggling an unrelated option no longer freezes. These
    // are mutable because ApplyWindow() is const.
    mutable bool                     _windowApplied     = false;
    mutable GameSettings::WindowMode _appliedWindowMode = GameSettings::WindowMode::Windowed;
    mutable int                      _appliedWidth      = 0;
    mutable int                      _appliedHeight     = 0;
    mutable bool                     _appliedVsync      = true;
};
