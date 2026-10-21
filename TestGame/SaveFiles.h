#pragma once

#include "UserDataPath.h"

#include <cstddef>
#include <string>

// ─────────────────────────────────────────────────────────────────────────────
// SAVE FILES — where each save lives and the bounds it is validated against.
//
// Every save is in one folder, <user data>/Saves (%LOCALAPPDATA%\MysticOnslaught
// \Saves in the full game, MO_PROFILE_DIR\Saves in tests), so cloud sync can
// take the whole folder. The pre-rework *.cfg files beside it are never read.
// docs/SAVE_FORMAT.md describes each file.
//
// The limits reject absurd or hostile values, not large legitimate ones: the
// stat bounds leave room for the planned x100 HP/damage rescale and beyond,
// so that work should only need to revisit this table.
// ─────────────────────────────────────────────────────────────────────────────

namespace SaveFiles
{
    inline std::string ProfilePath()       { return UserDataPath("Saves/profile.mosave"); }
    inline std::string RunCheckpointPath() { return UserDataPath("Saves/run.mosave"); }
    inline std::string RunLogPath()        { return UserDataPath("Saves/runlog.mosave"); }
    inline std::string SettingsPath()      { return UserDataPath("Saves/settings.json"); }
    inline std::string KeybindingsPath()   { return UserDataPath("Saves/keybindings.json"); }

    // The schema version each file is written with; any other value is rejected.
    constexpr int kProfileSchema       = 1;
    constexpr int kRunCheckpointSchema = 1;
    constexpr int kRunLogSchema        = 1;
    constexpr int kSettingsSchema      = 1;
    constexpr int kKeybindingsSchema   = 1;

    // Size caps, checked from the file's metadata before it is read.
    constexpr std::size_t kKiB = 1024;
    constexpr std::size_t kMiB = 1024 * kKiB;
    constexpr std::size_t kMaxProfileBytes       = kMiB;
    constexpr std::size_t kMaxRunCheckpointBytes = kMiB;
    constexpr std::size_t kMaxRunLogBytes        = 4 * kMiB;
    constexpr std::size_t kMaxSettingsBytes      = 64 * kKiB;

    // Content bounds shared by every file.
    constexpr std::size_t kMaxIdLength      = 64;     // stable ids, asset names, room layout names
    constexpr std::size_t kMaxIdListEntries = 1024;   // any list or map of ids
    constexpr std::size_t kMaxTextLength    = 256;    // labels and summary lines
}
