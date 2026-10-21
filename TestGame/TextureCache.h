#pragma once

#include "raylib.h"

#include <string>
#include <filesystem>
#include <unordered_map>

// ── TextureCache ──────────────────────────────────────────────────────────────
// Reference-counted GPU texture sharing for tile renderers. The active room
// renderer, the incoming scroll renderer and every room-asset catalog load all
// reference the same sheets, so crossing a door or reloading a catalog must not
// re-upload images that are already resident.
//
// Entries are keyed by path AND file modification time: an image edited on disk
// (room editor workflows) loads as a fresh entry on the next Acquire, while any
// renderer still holding the previous texture keeps it alive until Release.
// Main (GL) thread only.
// ─────────────────────────────────────────────────────────────────────────────
class TextureCache
{
public:
    static TextureCache& Get()
    {
        static TextureCache cache;
        return cache;
    }

    // Returns a texture with id 0 when the file cannot be loaded; such a texture
    // is not cached and Release ignores it.
    Texture2D Acquire(const std::string& path)
    {
        // Resolve lexical aliases against the current working directory so the
        // runtime, room catalog and editor share the same resident resource.
        std::error_code error;
        auto normalized = std::filesystem::absolute(std::filesystem::path(path), error);
        if (error) normalized = std::filesystem::path(path);
        const std::string normalizedPath = normalized.lexically_normal().generic_string();
        const std::string key = normalizedPath + '|' + std::to_string(GetFileModTime(path.c_str()));
        auto it = _entries.find(key);
        if (it != _entries.end())
        {
            ++it->second.refs;
            return it->second.texture;
        }
        Texture2D texture = LoadTexture(path.c_str());
        if (texture.id == 0) return texture;
        ++_loads;
        _entries.emplace(key, Entry{ texture, 1 });
        return texture;
    }

    void Release(Texture2D texture)
    {
        if (texture.id == 0) return;
        for (auto it = _entries.begin(); it != _entries.end(); ++it)
        {
            if (it->second.texture.id != texture.id) continue;
            if (--it->second.refs <= 0)
            {
                UnloadTexture(it->second.texture);
                _entries.erase(it);
            }
            return;
        }
    }

    int ResidentCount() const { return (int)_entries.size(); }
    int LoadCount() const { return _loads; }   // total uploads, for diagnostics/tests

private:
    struct Entry
    {
        Texture2D texture{};
        int refs = 0;
    };
    std::unordered_map<std::string, Entry> _entries;
    int _loads = 0;
};
