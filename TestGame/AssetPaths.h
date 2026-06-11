#pragma once

#include "raylib.h"

#include <string>

// AssetPath converts the project's asset references into a form that works for
// both native builds and Emscripten builds.
//
// Native:
// - tries a few relative roots so the game can run from the project folder,
//   the Visual Studio output folder, or the workspace root without hardcoded
//   machine-specific absolute paths.
//
// Web:
// - returns a normalized virtual filesystem path that matches the
//   --preload-file mounts used by the web build script.
inline std::string AssetPath(const char* relativePath)
{
    std::string normalized = relativePath != nullptr ? relativePath : "";
    for (char& ch : normalized)
    {
        if (ch == '\\')
            ch = '/';
    }

#ifdef __EMSCRIPTEN__
    return normalized;
#else
    static const char* prefixes[] =
    {
        "",
        "../",
        "../../",
        "../../../"
    };

    for (const char* prefix : prefixes)
    {
        std::string candidate = std::string(prefix) + normalized;
        if (FileExists(candidate.c_str()))
            return candidate;
    }

    return normalized;
#endif
}

// AssetFolderPath resolves a relative folder path the same way AssetPath does,
// but checks for directory existence instead of file existence.
inline std::string AssetFolderPath(const char* relativePath)
{
    std::string normalized = relativePath != nullptr ? relativePath : "";
    for (char& ch : normalized)
    {
        if (ch == '\\')
            ch = '/';
    }

#ifdef __EMSCRIPTEN__
    return normalized;
#else
    static const char* prefixes[] =
    {
        "",
        "../",
        "../../",
        "../../../"
    };

    for (const char* prefix : prefixes)
    {
        std::string candidate = std::string(prefix) + normalized;
        if (DirectoryExists(candidate.c_str()))
            return candidate;
    }

    return normalized;
#endif
}
