#include "SettingsManager.h"
#include "GamepadInput.h"
#include "KeyBindings.h"
#include "SaveFiles.h"

#include <cassert>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

namespace
{
    void WriteText(const std::string& path, const std::string& text)
    {
        const std::filesystem::path parent = std::filesystem::path(path).parent_path();
        if (!parent.empty()) std::filesystem::create_directories(parent);
        std::ofstream(path, std::ios::binary | std::ios::trunc) << text;
    }

    std::string ReadText(const std::string& path)
    {
        std::ifstream in(path, std::ios::binary);
        return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    }
}

int main()
{
    GameSettings settings{};

    // Comfort defaults: the full intended feel until the player turns it down.
    assert(settings.screenShake == 1.0f);
    assert(settings.flashLevel == 0 && settings.FlashScale() == 1.f);
    assert(settings.hitStopLevel == 0 && settings.HitStopScale() == 1.f);
    settings.flashLevel = 1;   assert(settings.FlashScale() > 0.f && settings.FlashScale() < 1.f);
    settings.flashLevel = 2;   assert(settings.FlashScale() == 0.f);
    settings.hitStopLevel = 1; assert(settings.HitStopScale() > 0.f && settings.HitStopScale() < 1.f);
    settings.hitStopLevel = 2; assert(settings.HitStopScale() == 0.f);

    // Clean slate: the old settings.cfg is not read.
    WriteText("settings.cfg", "screen_shake=0.1\nsfx_volume=0.2\n");
    {
        SettingsManager fresh;
        fresh.Load();
        assert(fresh.Get().screenShake == 1.f && fresh.Get().sfxVolume == 1.f);
    }

    // Comfort choices survive a save and reload (runs in the suite's sandbox),
    // as readable JSON in the save folder.
    {
        SettingsManager saved;
        saved.Get().screenShake  = 0.35f;
        saved.Get().flashLevel   = 2;
        saved.Get().hitStopLevel = 1;
        saved.Get().sfxVolume    = 0.5f;
        saved.Get().windowMode   = GameSettings::WindowMode::Borderless;
        saved.Save();
        SettingsManager loaded;
        loaded.Load();
        assert(loaded.Get().screenShake == 0.35f);
        assert(loaded.Get().flashLevel == 2);
        assert(loaded.Get().hitStopLevel == 1);
        assert(loaded.Get().sfxVolume == 0.5f);
        assert(loaded.Get().windowMode == GameSettings::WindowMode::Borderless);
        assert(ReadText(SaveFiles::SettingsPath()).find("\"windowMode\": \"borderless\"") != std::string::npos);
    }
    // Out-of-range values from a hand-edited file are clamped, not trusted;
    // missing ones keep their defaults.
    {
        WriteText(SaveFiles::SettingsPath(),
                  "{ \"schema\": 1, \"comfort\": { \"screenShake\": 7, \"screenFlashes\": 9, \"hitStop\": -3 },"
                  "  \"display\": { \"windowedWidth\": 5 } }");
        SettingsManager loaded;
        loaded.Load();
        assert(loaded.Get().screenShake == 1.f);
        assert(loaded.Get().flashLevel == 2);
        assert(loaded.Get().hitStopLevel == 0);
        assert(loaded.Get().windowedWidth >= 320 && loaded.Get().sfxVolume == 1.f);
    }
    // A hand edit that breaks the file falls back to the backup.
    {
        SettingsManager first;
        first.Get().musicVolume = 0.25f;
        first.Save();
        SettingsManager second;
        second.Get().musicVolume = 0.75f;
        second.Save();                                               // backup holds 0.25
        WriteText(SaveFiles::SettingsPath(), "{ \"schema\": 1, \"audio\": { \"music\": \"loud\" } }");
        SettingsManager loaded;
        loaded.Load();
        assert(loaded.Get().musicVolume == 0.25f);
    }

    // Keybindings: round trip, and a code out of range keeps its default.
    {
        KeyBindings keys;
        GamepadBindings pad;
        keys.dash = KEY_LEFT_SHIFT;
        keys.ability[2] = KEY_E;
        pad.pause = GAMEPAD_BUTTON_MIDDLE_LEFT;
        assert(SettingsManager::SaveKeybindings(keys, pad));
        KeyBindings loadedKeys;
        GamepadBindings loadedPad;
        assert(SettingsManager::LoadKeybindings(loadedKeys, loadedPad));
        assert(loadedKeys.dash == KEY_LEFT_SHIFT && loadedKeys.ability[2] == KEY_E && loadedKeys.ability[0] == KEY_ONE);
        assert(loadedPad.pause == GAMEPAD_BUTTON_MIDDLE_LEFT);

        WriteText(SaveFiles::KeybindingsPath(),
                  "{ \"schema\": 1, \"keyboard\": { \"dash\": 99999, \"moveUp\": 265 }, \"gamepad\": { \"dash\": -4 } }");
        KeyBindings edited;
        GamepadBindings editedPad;
        assert(SettingsManager::LoadKeybindings(edited, editedPad));
        assert(edited.dash == KeyBindings().dash && edited.moveUp == KEY_UP && editedPad.dash == GamepadBindings().dash);
    }
    std::puts("Settings default tests passed: defaults, old file ignored, JSON round trip, clamped hand edits, "
              "broken file -> backup, keybindings round trip and range checks");
    return 0;
}
