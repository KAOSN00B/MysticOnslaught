#pragma once
#include "DevTools.h"
#include <cstdlib>
#include <filesystem>
#include <string>

// std::getenv is fine here (read-only, copied immediately); keep MSVC's SDL
// deprecation (C4996) from turning every includer into a build error.
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 4996)
#endif

inline std::string ResolveUserDataPath(const char* filename, const std::string& base, bool installedGame)
{
    if (!installedGame || base.empty()) return filename;
    return (std::filesystem::path(base) / "MysticOnslaught" / filename).string();
}

// Full native builds write outside the install folder. Authoring builds keep
// existing local profiles.
inline std::string UserDataPath(const char* filename)
{
    // Sandbox override for automated tests and isolated playtests: every save
    // (profile, village, settings, bindings) goes into this one folder.
    if (const char* sandbox = std::getenv("MO_PROFILE_DIR"); sandbox != nullptr && *sandbox != 0)
    {
        std::error_code error;
        std::filesystem::create_directories(sandbox, error);
        return (std::filesystem::path(sandbox) / filename).string();
    }
#if defined(_WIN32) && !MO_DEV_TOOLS && !MO_ITCH_DEMO
    const char* local = std::getenv("LOCALAPPDATA");
    if (!local || !*local) local = std::getenv("APPDATA");
    const std::string path = ResolveUserDataPath(filename, local ? local : "", true);
    const auto parent = std::filesystem::path(path).parent_path();
    if (!parent.empty())
    {
        std::error_code error;
        std::filesystem::create_directories(parent, error);
        // Keep the requested path on failure so the save layer reports it;
        // never silently switch the player to a different profile.
    }
    return path;
#else
    return filename;
#endif
}

#if defined(_MSC_VER)
#pragma warning(pop)
#endif
