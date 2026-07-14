#include "RoomAssetCatalog.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <fstream>

namespace fs = std::filesystem;

namespace
{
    bool IsPng(const fs::path& path)
    {
        std::string extension = path.extension().string();
        std::transform(extension.begin(), extension.end(), extension.begin(),
            [](unsigned char ch) { return (char)std::tolower(ch); });
        return extension == ".png";
    }

    fs::path FindMetadata(const fs::path& root, const fs::path& metadataRoot,
                          const std::string& stem)
    {
        const fs::path filename = "tilemapper_" + stem + ".txt";
        const fs::path candidates[] = {
            metadataRoot / filename,
            fs::current_path() / filename,
            fs::current_path() / "TestGame" / filename,
            root.parent_path() / filename,
            root.parent_path() / "TestGame" / filename,
        };
        for (const fs::path& candidate : candidates)
            if (!candidate.empty() && fs::exists(candidate)) return candidate;
        return {};
    }

    bool ReadPngSize(const fs::path& path, int& width, int& height)
    {
        unsigned char header[24]{};
        std::ifstream input(path, std::ios::binary);
        if (!input.read((char*)header, sizeof(header))) return false;
        static constexpr unsigned char signature[8] =
            { 137, 80, 78, 71, 13, 10, 26, 10 };
        if (!std::equal(std::begin(signature), std::end(signature), header)) return false;
        auto readBigEndian = [&](int offset)
        {
            return (int)((std::uint32_t)header[offset] << 24 |
                         (std::uint32_t)header[offset + 1] << 16 |
                         (std::uint32_t)header[offset + 2] << 8 |
                         (std::uint32_t)header[offset + 3]);
        };
        width = readBigEndian(16);
        height = readBigEndian(20);
        return width > 0 && height > 0;
    }
}

bool RoomAssetCatalog::Refresh(const fs::path& tilesetRoot,
                               const fs::path& metadataRoot)
{
    _sources.clear();
    _warnings.clear();
    try
    {
        if (!fs::is_directory(tilesetRoot))
        {
            _warnings.push_back("Tileset folder was not found: " + tilesetRoot.string());
            return false;
        }
        for (const fs::directory_entry& entry : fs::directory_iterator(tilesetRoot))
        {
            if (!entry.is_regular_file() || !IsPng(entry.path())) continue;

            RoomAssetSource source;
            source.stem = entry.path().stem().string();
            source.imagePath = entry.path();
            source.metadataPath = FindMetadata(tilesetRoot, metadataRoot, source.stem);

            int imageWidth = 0;
            int imageHeight = 0;
            if (ReadPngSize(entry.path(), imageWidth, imageHeight))
            {
                source.tileColumns = imageWidth / 16;
                source.tileRows = imageHeight / 16;
            }
            if (!source.metadataPath.empty() &&
                !source.definitions.LoadFromFile(source.metadataPath.string().c_str()))
            {
                _warnings.push_back("Could not read metadata for " + source.stem);
            }
            _sources.push_back(std::move(source));
        }
        std::sort(_sources.begin(), _sources.end(),
            [](const RoomAssetSource& lhs, const RoomAssetSource& rhs)
            {
                return lhs.stem < rhs.stem;
            });
    }
    catch (const std::exception& exception)
    {
        _warnings.push_back(exception.what());
        return false;
    }
    return !_sources.empty();
}

const RoomAssetSource* RoomAssetCatalog::Find(std::string_view stem) const
{
    for (const RoomAssetSource& source : _sources)
        if (source.stem == stem) return &source;
    return nullptr;
}
