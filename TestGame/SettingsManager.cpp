#include "SettingsManager.h"
#include "AudioManager.h"
#include "GamepadInput.h"
#include "KeyBindings.h"
#include "SaveFiles.h"
#include "SaveStore.h"
#include "VirtualCanvas.h"
#include "raylib.h"

#include <algorithm>
#include <cstring>

namespace
{
    // Window modes by name, in GameSettings::WindowMode order.
    constexpr const char* kWindowModeNames[] = { "fullscreen", "borderless", "windowed" };
    constexpr int    kMinWindowSize   = 320;
    constexpr int    kMaxWindowSize   = 16384;
    constexpr int    kMaxComfortLevel = 2;     // full / reduced / off
    constexpr double kAnyNumber       = 1.0e9; // read wide, then clamp to the real range

    SaveFile SettingsFile()    { return SaveFile(SaveFiles::SettingsPath(), SaveKind::Settings, SaveFiles::kMaxSettingsBytes); }
    SaveFile KeybindingsFile() { return SaveFile(SaveFiles::KeybindingsPath(), SaveKind::Keybindings, SaveFiles::kMaxSettingsBytes); }

    float ReadClamped(const JsonReader& section, const char* key, float fallback, float min, float max)
    {
        return std::clamp(section.FloatOr(key, fallback, -(float)kAnyNumber, (float)kAnyNumber), min, max);
    }

    int ReadClamped(const JsonReader& section, const char* key, int fallback, int min, int max)
    {
        return (int)std::clamp(section.IntOr(key, fallback, -(long long)kAnyNumber, (long long)kAnyNumber),
                               (long long)min, (long long)max);
    }

    JsonValue SettingsToJson(const GameSettings& s)
    {
        JsonValue doc = JsonValue::MakeObject();
        doc.Set("schema", JsonValue::MakeInt(SaveFiles::kSettingsSchema));
        JsonValue& display = doc.Set("display", JsonValue::MakeObject());
        display.Set("windowMode", JsonValue::MakeString(kWindowModeNames[(int)s.windowMode]));
        display.Set("windowedWidth", JsonValue::MakeInt(s.windowedWidth));
        display.Set("windowedHeight", JsonValue::MakeInt(s.windowedHeight));
        display.Set("vsync", JsonValue::MakeBool(s.vsync));
        JsonValue& audio = doc.Set("audio", JsonValue::MakeObject());
        audio.Set("master", JsonValue::MakeFloat(s.masterVolume));
        audio.Set("music", JsonValue::MakeFloat(s.musicVolume));
        audio.Set("sfx", JsonValue::MakeFloat(s.sfxVolume));
        JsonValue& comfort = doc.Set("comfort", JsonValue::MakeObject());
        comfort.Set("screenShake", JsonValue::MakeFloat(s.screenShake));
        comfort.Set("screenFlashes", JsonValue::MakeInt(s.flashLevel));
        comfort.Set("hitStop", JsonValue::MakeInt(s.hitStopLevel));
        return doc;
    }

    bool SettingsFromJson(const JsonValue& document, GameSettings& out, std::string& error)
    {
        JsonReadErrors errors;
        const JsonReader root(document, errors);
        root.Int("schema", SaveFiles::kSettingsSchema, SaveFiles::kSettingsSchema);
        GameSettings s;   // anything missing keeps its default
        JsonReader section;
        if (root.OptionalObject("display", section))
        {
            const std::string mode = section.StringOr("windowMode", kWindowModeNames[(int)s.windowMode], SaveFiles::kMaxIdLength);
            for (int i = 0; i < (int)std::size(kWindowModeNames); ++i)
                if (mode == kWindowModeNames[i]) s.windowMode = (GameSettings::WindowMode)i;
            s.windowedWidth = ReadClamped(section, "windowedWidth", s.windowedWidth, kMinWindowSize, kMaxWindowSize);
            s.windowedHeight = ReadClamped(section, "windowedHeight", s.windowedHeight, kMinWindowSize, kMaxWindowSize);
            s.vsync = section.BoolOr("vsync", s.vsync);
        }
        if (root.OptionalObject("audio", section))
        {
            s.masterVolume = ReadClamped(section, "master", s.masterVolume, 0.f, 1.f);
            s.musicVolume = ReadClamped(section, "music", s.musicVolume, 0.f, 1.f);
            s.sfxVolume = ReadClamped(section, "sfx", s.sfxVolume, 0.f, 1.f);
        }
        if (root.OptionalObject("comfort", section))
        {
            s.screenShake = ReadClamped(section, "screenShake", s.screenShake, 0.f, 1.f);
            s.flashLevel = ReadClamped(section, "screenFlashes", s.flashLevel, 0, kMaxComfortLevel);
            s.hitStopLevel = ReadClamped(section, "hitStop", s.hitStopLevel, 0, kMaxComfortLevel);
        }
        if (!errors.Ok()) { error = errors.first; return false; }
        out = s;
        return true;
    }

    // Raylib key and button codes; anything outside keeps the default.
    void ReadKey(const JsonReader& section, const char* key, KeyboardKey& value)
    {
        const long long code = section.IntOr(key, value, -(long long)kAnyNumber, (long long)kAnyNumber);
        if (code >= KEY_NULL && code <= KEY_KB_MENU) value = (KeyboardKey)code;
    }

    void ReadButton(const JsonReader& section, const char* key, GamepadButton& value)
    {
        const long long code = section.IntOr(key, value, -(long long)kAnyNumber, (long long)kAnyNumber);
        if (code >= GAMEPAD_BUTTON_UNKNOWN && code <= GAMEPAD_BUTTON_RIGHT_THUMB) value = (GamepadButton)code;
    }

    template <typename Code, std::size_t N, typename Read>
    void ReadCodeList(const JsonReader& section, const char* key, Code (&values)[N], Read read)
    {
        JsonReader list;
        if (!section.OptionalArray(key, N, list)) return;
        for (std::size_t i = 0; i < list.Count(); ++i)
        {
            const long long code = list.Item(i).AsInt(-(long long)kAnyNumber, (long long)kAnyNumber);
            read(code, values[i]);
        }
    }

    template <typename Code, std::size_t N>
    JsonValue CodeList(const Code (&values)[N])
    {
        JsonValue list = JsonValue::MakeArray();
        for (Code value : values) list.Push(JsonValue::MakeInt((int)value));
        return list;
    }
}

// ── Load ──────────────────────────────────────────────────────────────────────
void SettingsManager::Load()
{
    SettingsFile().Load([this](const JsonValue& document, std::string& error) {
        return SettingsFromJson(document, _settings, error);
    });
}

// ── Save ──────────────────────────────────────────────────────────────────────
void SettingsManager::Save() const
{
    SettingsFile().Save(SettingsToJson(_settings), [](const JsonValue& document, std::string& error) {
        GameSettings check;
        return SettingsFromJson(document, check, error);
    });
}

// ── Keybindings ───────────────────────────────────────────────────────────────
bool SettingsManager::SaveKeybindings(const KeyBindings& keys, const GamepadBindings& pad)
{
    JsonValue doc = JsonValue::MakeObject();
    doc.Set("schema", JsonValue::MakeInt(SaveFiles::kKeybindingsSchema));
    JsonValue& keyboard = doc.Set("keyboard", JsonValue::MakeObject());
    keyboard.Set("moveUp", JsonValue::MakeInt(keys.moveUp));
    keyboard.Set("moveDown", JsonValue::MakeInt(keys.moveDown));
    keyboard.Set("moveLeft", JsonValue::MakeInt(keys.moveLeft));
    keyboard.Set("moveRight", JsonValue::MakeInt(keys.moveRight));
    keyboard.Set("dash", JsonValue::MakeInt(keys.dash));
    keyboard.Set("attack", JsonValue::MakeInt(keys.attack));
    keyboard.Set("ability", CodeList(keys.ability));
    JsonValue& gamepad = doc.Set("gamepad", JsonValue::MakeObject());
    gamepad.Set("attack", JsonValue::MakeInt(pad.attack));
    gamepad.Set("dash", JsonValue::MakeInt(pad.dash));
    gamepad.Set("ability", CodeList(pad.ability));
    gamepad.Set("pause", JsonValue::MakeInt(pad.pause));
    return KeybindingsFile().Save(doc);
}

bool SettingsManager::LoadKeybindings(KeyBindings& keys, GamepadBindings& pad)
{
    const SaveLoadResult result = KeybindingsFile().Load([&](const JsonValue& document, std::string& error) {
        JsonReadErrors errors;
        const JsonReader root(document, errors);
        root.Int("schema", SaveFiles::kKeybindingsSchema, SaveFiles::kKeybindingsSchema);
        KeyBindings k = keys;
        GamepadBindings g = pad;
        JsonReader section;
        if (root.OptionalObject("keyboard", section))
        {
            ReadKey(section, "moveUp", k.moveUp);
            ReadKey(section, "moveDown", k.moveDown);
            ReadKey(section, "moveLeft", k.moveLeft);
            ReadKey(section, "moveRight", k.moveRight);
            ReadKey(section, "dash", k.dash);
            ReadKey(section, "attack", k.attack);
            ReadCodeList(section, "ability", k.ability, [](long long code, KeyboardKey& value) {
                if (code >= KEY_NULL && code <= KEY_KB_MENU) value = (KeyboardKey)code;
            });
        }
        if (root.OptionalObject("gamepad", section))
        {
            ReadButton(section, "attack", g.attack);
            ReadButton(section, "dash", g.dash);
            ReadButton(section, "pause", g.pause);
            ReadCodeList(section, "ability", g.ability, [](long long code, GamepadButton& value) {
                if (code >= GAMEPAD_BUTTON_UNKNOWN && code <= GAMEPAD_BUTTON_RIGHT_THUMB) value = (GamepadButton)code;
            });
        }
        if (!errors.Ok()) { error = errors.first; return false; }
        keys = k;
        pad = g;
        return true;
    });
    return result == SaveLoadResult::Loaded || result == SaveLoadResult::RestoredBackup;
}

// ── ApplyWindow ───────────────────────────────────────────────────────────────
void SettingsManager::ApplyWindow() const
{
    // The window operations below (state flags, ToggleFullscreen, SetWindowSize)
    // rebuild the OS window / GL surface and can stall for a second or more on
    // some drivers. ApplyWindow is called on every Settings exit and every
    // display toggle, so re-applying an unchanged config was freezing the game
    // for no reason. Skip entirely when nothing window-relevant changed; only a
    // genuine display change now pays that cost (a brief, expected flicker).
    if (_windowApplied
        && _appliedWindowMode == _settings.windowMode
        && _appliedWidth      == _settings.windowedWidth
        && _appliedHeight     == _settings.windowedHeight
        && _appliedVsync      == _settings.vsync)
        return;

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

    // Remember what we just applied so redundant calls become no-ops.
    _windowApplied     = true;
    _appliedWindowMode = _settings.windowMode;
    _appliedWidth      = _settings.windowedWidth;
    _appliedHeight     = _settings.windowedHeight;
    _appliedVsync      = _settings.vsync;
}

// ── ApplyVolumes ──────────────────────────────────────────────────────────────
void SettingsManager::ApplyVolumes(AudioManager& audio) const
{
    SetMasterVolume(_settings.masterVolume);
    audio.SetMusicVolumeScale(_settings.musicVolume);
    audio.SetSfxVolumeScale(_settings.sfxVolume);
}
