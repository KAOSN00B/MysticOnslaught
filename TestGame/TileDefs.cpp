#ifdef _MSC_VER
#pragma warning(disable: 4996)
#endif

#include "TileDefs.h"
#include "RoomBlueprint.h"

#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <sstream>

static constexpr int kTileSize = 16;

Rectangle TileDefSet::Get(TileType t) const
{
    int i = (int)t;
    if (i >= 0 && i < (int)TileType::Count && assigned[i]) return rects[i];
    if (assigned[(int)TileType::Floor]) return rects[(int)TileType::Floor];
    for (int j = 0; j < (int)TileType::Count; ++j)
        if (assigned[j]) return rects[j];
    return { 0.f, 0.f, (float)kTileSize, (float)kTileSize };
}

TileRenderSource ResolveTileRenderSource(const TileDefSet& defs, TileType type)
{
    const int idx = (int)type;
    const bool assigned = idx >= 0 && idx < (int)TileType::Count && defs.assigned[idx];
    if (!assigned && (type == TileType::ChestClosed || type == TileType::ChestOpen))
    {
        const float row = type == TileType::ChestClosed ? 18.f : 19.f;
        return { { 23.f * kTileSize, row * kTileSize,
                   (float)kTileSize, (float)kTileSize }, TileRenderSheet::SharedReward };
    }
    TileRenderSheet sheet = TileRenderSheet::Biome;
    if (assigned && defs.fromGround[idx]) sheet = TileRenderSheet::Ground;
    return { defs.Get(type), sheet };
}

namespace
{
    constexpr int kMaxAnimationFrames = 1024;

    std::string LegacyId(const char* kind, std::size_t index)
    {
        return std::string(kind) + "_" + std::to_string(index);
    }

    std::string LegacyName(const char* kind, std::size_t index)
    {
        return std::string(kind) + " " + std::to_string(index + 1);
    }

    bool ReadFrames(std::istringstream& in, int count, std::vector<Rectangle>& frames)
    {
        if (count <= 0 || count > kMaxAnimationFrames) return false;
        frames.clear();
        frames.reserve((std::size_t)count);
        for (int i = 0; i < count; ++i)
        {
            Rectangle frame{};
            if (!(in >> frame.x >> frame.y >> frame.width >> frame.height)) return false;
            frames.push_back(frame);
        }
        return true;
    }

    bool ReadPlayback(int value, AnimPlaybackMode& playback)
    {
        if (value < (int)AnimPlaybackMode::Loop || value > (int)AnimPlaybackMode::PlayOnce)
            return false;
        playback = (AnimPlaybackMode)value;
        return true;
    }
}

bool TileDefSet::LoadFromFile(const char* path)
{
    std::ifstream file(path);
    if (!file) return false;
    *this = {};

    std::string line;
    while (std::getline(file, line))
    {
        if (line.empty()) continue;
        std::istringstream in(line);
        std::string tag;
        if (!(in >> tag) || tag == "BIOME") continue;

        if (tag == "TILE" || tag == "GTILE")
        {
            int col = 0, row = 0, spanCols = 0, spanRows = 0, typeIdx = -1;
            if (!(in >> col >> row >> spanCols >> spanRows >> typeIdx)) continue;
            if (typeIdx < 0 || typeIdx >= (int)TileType::Count) continue;
            rects[typeIdx] = { (float)(col * kTileSize), (float)(row * kTileSize),
                               (float)(spanCols * kTileSize), (float)(spanRows * kTileSize) };
            assigned[typeIdx] = true;
            fromGround[typeIdx] = tag == "GTILE";
        }
        else if (tag == "PROP" || tag == "PROPV2" || tag == "PROPV3")
        {
            SpriteDef def{};
            if ((tag == "PROPV2" || tag == "PROPV3") &&
                !(in >> std::quoted(def.id) >> std::quoted(def.name))) continue;
            if (tag == "PROPV3" && !(in >> std::quoted(def.sourceSheet))) continue;
            if (!(in >> def.src.x >> def.src.y >> def.src.width >> def.src.height)) continue;
            def.collision = { 0.f, 0.f, def.src.width, def.src.height };
            Rectangle authored{};
            if (in >> authored.x >> authored.y >> authored.width >> authored.height)
                def.collision = authored;
            if (tag == "PROP")
            {
                def.id = LegacyId("prop", props.size());
                def.name = LegacyName("Prop", props.size());
            }
            if (!def.id.empty()) props.push_back(std::move(def));
        }
        else if (tag == "DECOR" || tag == "DECORV2" || tag == "DECORV3")
        {
            SpriteDef def{};
            if ((tag == "DECORV2" || tag == "DECORV3") &&
                !(in >> std::quoted(def.id) >> std::quoted(def.name))) continue;
            if (tag == "DECORV3" && !(in >> std::quoted(def.sourceSheet))) continue;
            if (!(in >> def.src.x >> def.src.y >> def.src.width >> def.src.height)) continue;
            if (tag == "DECOR")
            {
                def.id = LegacyId("decor", decors.size());
                def.name = LegacyName("Decor", decors.size());
            }
            if (!def.id.empty()) decors.push_back(std::move(def));
        }
        else if (tag == "ANIMPROP" || tag == "ANIMPROPV2" || tag == "ANIMPROPV3")
        {
            AnimPropDef def{};
            int frameCount = 0;
            if (tag == "ANIMPROPV2" || tag == "ANIMPROPV3")
            {
                int playback = 0;
                if (!(in >> std::quoted(def.id) >> std::quoted(def.name))) continue;
                if (tag == "ANIMPROPV3" && !(in >> std::quoted(def.sourceSheet))) continue;
                if (!(in >> playback >> def.fps
                         >> def.collision.x >> def.collision.y
                         >> def.collision.width >> def.collision.height >> frameCount) ||
                    !ReadPlayback(playback, def.playback))
                    continue;
            }
            else
            {
                if (!(in >> def.collision.x >> def.collision.y
                         >> def.collision.width >> def.collision.height
                         >> def.fps >> frameCount))
                    continue;
                def.id = LegacyId("anim_prop", animProps.size());
                def.name = LegacyName("Animated Prop", animProps.size());
            }
            if (ReadFrames(in, frameCount, def.frames) && !def.id.empty())
                animProps.push_back(std::move(def));
        }
        else if (tag == "ANIMDECOR" || tag == "ANIMDECORV2" || tag == "ANIMDECORV3")
        {
            AnimSpriteDef def{};
            int frameCount = 0;
            if (tag == "ANIMDECORV2" || tag == "ANIMDECORV3")
            {
                int playback = 0;
                if (!(in >> std::quoted(def.id) >> std::quoted(def.name))) continue;
                if (tag == "ANIMDECORV3" && !(in >> std::quoted(def.sourceSheet))) continue;
                if (!(in >> playback >> def.fps >> frameCount) ||
                    !ReadPlayback(playback, def.playback))
                    continue;
            }
            else
            {
                if (!(in >> def.fps >> frameCount)) continue;
                def.id = LegacyId("anim_decor", animDecors.size());
                def.name = LegacyName("Animated Decor", animDecors.size());
            }
            if (ReadFrames(in, frameCount, def.frames) && !def.id.empty())
                animDecors.push_back(std::move(def));
        }
        else
        {
            char* end = nullptr;
            const long colValue = std::strtol(tag.c_str(), &end, 10);
            if (!end || *end != '\0') continue;
            int row = 0, spanCols = 0, spanRows = 0, typeIdx = -1;
            if (!(in >> row >> spanCols >> spanRows >> typeIdx)) continue;
            if (typeIdx < 0 || typeIdx >= (int)TileType::Count) continue;
            rects[typeIdx] = { (float)(colValue * kTileSize), (float)(row * kTileSize),
                               (float)(spanCols * kTileSize), (float)(spanRows * kTileSize) };
            assigned[typeIdx] = true;
        }
    }
    return true;
}

int TileDefSet::FindAssetIndex(RoomAssetKind kind, std::string_view id) const
{
    auto find = [id](const auto& definitions)
    {
        for (int i = 0; i < (int)definitions.size(); ++i)
            if (definitions[(std::size_t)i].id == id) return i;
        return -1;
    };
    switch (kind)
    {
    case RoomAssetKind::Prop:      return find(props);
    case RoomAssetKind::AnimProp:  return find(animProps);
    case RoomAssetKind::Decor:     return find(decors);
    case RoomAssetKind::AnimDecor: return find(animDecors);
    }
    return -1;
}
