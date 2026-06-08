#pragma warning(disable: 4996)  // fopen/fscanf/fprintf are safe here; paths are internal constants

#include "SettingsManager.h"
#include "AudioManager.h"
#include "VirtualCanvas.h"
#include "raylib.h"

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <algorithm>

static const char* kSettingsPath = "settings.cfg";

// ── Load ──────────────────────────────────────────────────────────────────────
void SettingsManager::Load()
{
#ifdef PLATFORM_WEB
    return;  // no persistent file system on web; use defaults every session
#else
    FILE* f = fopen(kSettingsPath, "r");
    if (!f) return;

    char key[64], val[64];
    while (fscanf(f, " %63[^=]=%63s", key, val) == 2)
    {
        if      (strcmp(key, "window_mode") == 0)
        {
            if      (strcmp(val, "fullscreen") == 0) _settings.windowMode = GameSettings::WindowMode::Fullscreen;
            else if (strcmp(val, "borderless") == 0) _settings.windowMode = GameSettings::WindowMode::Borderless;
            else                                     _settings.windowMode = GameSettings::WindowMode::Windowed;
        }
        else if (strcmp(key, "windowed_width")  == 0) _settings.windowedWidth  = atoi(val);
        else if (strcmp(key, "windowed_height") == 0) _settings.windowedHeight = atoi(val);
        else if (strcmp(key, "vsync")           == 0) _settings.vsync          = (atoi(val) != 0);
        else if (strcmp(key, "master_volume")   == 0) _settings.masterVolume   = (float)atof(val);
        else if (strcmp(key, "music_volume")    == 0) _settings.musicVolume    = (float)atof(val);
        else if (strcmp(key, "sfx_volume")      == 0) _settings.sfxVolume      = (float)atof(val);
    }
    fclose(f);

    // Clamp volumes to valid range after loading
    auto clamp01 = [](float v) { return v < 0.f ? 0.f : v > 1.f ? 1.f : v; };
    _settings.masterVolume = clamp01(_settings.masterVolume);
    _settings.musicVolume  = clamp01(_settings.musicVolume);
    _settings.sfxVolume    = clamp01(_settings.sfxVolume);
#endif
}

// ── Save ──────────────────────────────────────────────────────────────────────
void SettingsManager::Save() const
{
#ifdef PLATFORM_WEB
    return;
#else
    FILE* f = fopen(kSettingsPath, "w");
    if (!f) return;

    const char* modeStr = "windowed";
    if      (_settings.windowMode == GameSettings::WindowMode::Fullscreen) modeStr = "fullscreen";
    else if (_settings.windowMode == GameSettings::WindowMode::Borderless)  modeStr = "borderless";

    fprintf(f, "window_mode=%s\n",     modeStr);
    fprintf(f, "windowed_width=%d\n",  _settings.windowedWidth);
    fprintf(f, "windowed_height=%d\n", _settings.windowedHeight);
    fprintf(f, "vsync=%d\n",           _settings.vsync ? 1 : 0);
    fprintf(f, "master_volume=%.3f\n", _settings.masterVolume);
    fprintf(f, "music_volume=%.3f\n",  _settings.musicVolume);
    fprintf(f, "sfx_volume=%.3f\n",    _settings.sfxVolume);
    fclose(f);
#endif
}

// ── ApplyWindow ───────────────────────────────────────────────────────────────
void SettingsManager::ApplyWindow() const
{
    // VSync
    if (_settings.vsync)
        SetWindowState(FLAG_VSYNC_HINT);
    else
        ClearWindowState(FLAG_VSYNC_HINT);

    switch (_settings.windowMode)
    {
    case GameSettings::WindowMode::Fullscreen:
        if (IsWindowState(FLAG_WINDOW_UNDECORATED))
            ClearWindowState(FLAG_WINDOW_UNDECORATED);
        if (!IsWindowFullscreen())
            ToggleFullscreen();
        break;

    case GameSettings::WindowMode::Borderless:
        if (IsWindowFullscreen())
            ToggleFullscreen();
        SetWindowState(FLAG_WINDOW_UNDECORATED);
        MaximizeWindow();
        break;

    case GameSettings::WindowMode::Windowed:
        if (IsWindowFullscreen())
            ToggleFullscreen();
        ClearWindowState(FLAG_WINDOW_UNDECORATED);
        SetWindowSize(_settings.windowedWidth, _settings.windowedHeight);
        {
            int monitor = GetCurrentMonitor();
            int cx = (GetMonitorWidth(monitor)  - _settings.windowedWidth)  / 2;
            int cy = (GetMonitorHeight(monitor) - _settings.windowedHeight) / 2;
            if (cx < 0) cx = 0;
            if (cy < 0) cy = 0;
            SetWindowPosition(cx, cy);
        }
        break;
    }
}

// ── ApplyVolumes ──────────────────────────────────────────────────────────────
void SettingsManager::ApplyVolumes(AudioManager& audio) const
{
    SetMasterVolume(_settings.masterVolume);
    audio.SetMusicVolumeScale(_settings.musicVolume);
    audio.SetSfxVolumeScale(_settings.sfxVolume);
}
