#include "VillageLayoutData.h"

#include <algorithm>
#include <fstream>
#include <sstream>

VillageLayout VillageLayoutLoader::Fallback()
{
    VillageLayout layout;
    layout.objects.push_back({ "VillageGraveyard", { 310.f, 190.f }, 2.f, true });
    layout.objects.push_back({ "ZephsShop", { 1190.f, 210.f }, 2.f, true });
    return layout;
}

VillageLayout VillageLayoutLoader::Load(const std::string& path)
{
    std::ifstream file(path);
    if (!file)
        return Fallback();

    VillageLayout layout;
    std::string line;
    while (std::getline(file, line))
    {
        std::istringstream stream(line);
        std::string command;
        stream >> command;
        if (command.empty() || command[0] == '#')
            continue;

        if (command == "size")
        {
            int width = 0;
            int height = 0;
            if (stream >> width >> height && width > 0 && height > 0)
            {
                layout.width = width;
                layout.height = height;
            }
        }
        else if (command == "spawn")
        {
            std::string asset;
            std::string marker;
            if (stream >> asset >> marker)
            {
                layout.spawnAssetName = asset;
                layout.spawnMarkerName = marker;
            }
        }
        else if (command == "exit")
        {
            Rectangle exit{};
            if (stream >> exit.x >> exit.y >> exit.width >> exit.height && exit.width > 0.f && exit.height > 0.f)
                layout.exitRect = exit;
        }
        else if (command == "object")
        {
            VillageLayoutObject object;
            std::string rule;
            if (stream >> object.assetName >> object.worldOrigin.x >> object.worldOrigin.y >> object.scale >> rule)
            {
                object.scale = std::clamp(object.scale, 0.25f, 8.f);
                object.permanent = (rule == "permanent");
                layout.objects.push_back(object);
            }
        }
    }

    return layout.objects.empty() ? Fallback() : layout;
}

Vector2 VillageLayoutLoader::LocalToWorld(const VillageLayoutObject& object, Vector2 localPoint)
{
    return {
        object.worldOrigin.x + localPoint.x * object.scale,
        object.worldOrigin.y + localPoint.y * object.scale,
    };
}

Rectangle VillageLayoutLoader::LocalToWorld(const VillageLayoutObject& object, Rectangle localRect)
{
    return {
        object.worldOrigin.x + localRect.x * object.scale,
        object.worldOrigin.y + localRect.y * object.scale,
        localRect.width * object.scale,
        localRect.height * object.scale,
    };
}
