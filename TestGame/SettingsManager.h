#pragma once

class AudioManager;

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
    bool  abilityAimToggle = true;  // false: hold/release, true: press/press
};

class SettingsManager
{
public:
    // Load from settings.cfg (no-op on web build).
    void Load();
    // Save to settings.cfg (no-op on web build).
    void Save() const;

    // Apply window mode + VSync to the OS window.
    void ApplyWindow() const;
    // Push master/music/sfx volumes to the audio system.
    void ApplyVolumes(AudioManager& audio) const;

    GameSettings&       Get()       { return _settings; }
    const GameSettings& Get() const { return _settings; }

private:
    GameSettings _settings;
};
