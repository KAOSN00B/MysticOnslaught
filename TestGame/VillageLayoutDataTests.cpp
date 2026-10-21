#include "VillageLayoutData.h"

#include <cassert>
#include <cmath>

namespace
{
    bool Near(float lhs, float rhs)
    {
        return std::fabs(lhs - rhs) < 0.01f;
    }
}

int main()
{
    VillageLayout layout = VillageLayoutLoader::Load("VillageAssets/VillageLayout.vlayout");
    assert(layout.width == 1920);
    assert(layout.height == 1080);
    assert(layout.objects.size() == 9);
    assert(layout.objects[0].assetName == "VillageGraveyard");
    assert(layout.objects[1].assetName == "ZephsShop");
    assert(layout.objects[2].assetName == "VillageGate");
    const char* coreTown[] = { "VillageTownhouseA", "VillageTownhouseB", "VillageCottageA",
                               "VillageCottageB", "VillageTrophyHall", "VillageArmory" };
    for (int i = 0; i < 6; ++i)
    {
        const VillageLayoutObject& object = layout.objects[3 + i];
        assert(object.assetName == coreTown[i]);
        // Core plots are converted to 48px village cells: keep them exact.
        assert(std::fmod(object.worldOrigin.x, 48.f) == 0.f);
        assert(std::fmod(object.worldOrigin.y, 48.f) == 0.f);
    }
    for (const VillageLayoutObject& object : layout.objects)
        assert(object.permanent);
    assert(layout.spawnAssetName == "VillageGraveyard");
    assert(layout.spawnMarkerName == "Respawn");

    Vector2 markerWorld = VillageLayoutLoader::LocalToWorld(
        layout.objects[0], Vector2{ 104.76f, 117.84f });
    assert(Near(markerWorld.x, 519.52f));
    assert(Near(markerWorld.y, 425.68f));

    VillageLayout fallback = VillageLayoutLoader::Load("VillageAssets/does-not-exist.vlayout");
    assert(fallback.objects.size() == 3);
    assert(fallback.objects[0].assetName == "VillageGraveyard");
    assert(fallback.objects[1].assetName == "ZephsShop");
    assert(fallback.objects[2].assetName == "VillageGate");
    return 0;
}
