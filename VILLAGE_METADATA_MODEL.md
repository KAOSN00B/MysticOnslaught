# Village Asset Metadata Model — handoff (2026-07-09)

New standalone module, **not yet wired into the game**: Codex adopts it when convenient.

- `TestGame/TestGame/VillageAssetData.h`
- `TestGame/TestGame/VillageAssetData.cpp`

Engine-agnostic (no raylib dependency, so it unit-tests on its own). Include the
header **after** `raylib.h` to get `VaToRaylib(VaVec2)` / `VaToRaylib(VaRect)`
converters to `Vector2` / `Rectangle`.

Verified: a standalone self-test compiled with `cl /std:c++17` (no raylib) against
the real `VillageAssets/ZephsShop.vasset` + `VillageGraveyard.vasset` — **all
checks pass**, reproducing the current runtime results (Graveyard = 10 colliders
+ Poe + Respawn markers; Zeph = 1 marker, 0 colliders, 201×100) and correct
world-space transforms.

## `.vasset` format v1 (extended, fully backward-compatible)

Existing files keep loading unchanged. Every new line is **optional**; unknown
lines are **ignored** (forward-compatible). Superset:

```
village_asset 1
image ZephsShop.png
size 201 100
id ZephShop                     # default = filename stem
display_name Zeph's Shop        # default = id
category Building               # Building|Decor|Path|Utility|Trophy      (default Building)
service Shop                    # None|Shop|Graveyard|Training|ClassChange|Wardrobe|Bestiary|Cartographer|TrophyHall|DungeonGate
cost 0                          # gold (default 0)
required 1                      # can't be skipped in real village (default 0)
unique 1                        # only one allowed in real village (default 0)
removable 0                     # default 1
collider 10 20 50 30            # x y w h, image-local px, solid (unchanged)
marker Zeph 50 124              # legacy form: name = meaning (unchanged)
marker Counter 60 130 type=Interact id=Zeph      # optional typed form
interact 40 90 60 40 type=Shop prompt="Browse wares"          # press-E zone (NEW)
door 10 150 40 20 target=interior_default spawn=entry blocks_closed=1 prompt="Enter"   # NEW, dormant
ambient 120 140 group=Villager max=2 unlock_key=build_zeph_shop                         # NEW, dormant
```

Marker `type` is taken from `type=` if present, else inferred from the name
(`Zeph`/`Poe`/`Respawn`/`Door`/`Interact`/`NPCSpawn`/`AmbientNpcSpawn`/`PlayerSpawn`).
`door` and `ambient` are parsed into the struct but **no gameplay reads them yet**.

## Adoption snippet (replaces the two hardcoded `fopen` blocks)

```cpp
#include "VillageAssetData.h"   // after raylib.h

VillageAssetData zeph;
if (VillageAssetLoader::Load(AssetPath("VillageAssets/ZephsShop.vasset"), zeph))
{
    VaVec2 origin{ placedWorldX, placedWorldY };
    float  scale = villageDrawScale;          // same scale the PNG is drawn at
    // Zeph NPC position:
    if (const VAssetMarker* m = zeph.FindMarkerByType(VillageMarkerType::Zeph))
        Vector2 zephWorld = VaToRaylib(zeph.MarkerWorldPos(*m, origin, scale));
    // Player collision vs authored colliders (once Zeph colliders are authored):
    for (int i = 0; i < (int)zeph.colliders.size(); ++i)
        Rectangle solid = VaToRaylib(zeph.ColliderWorldRect(i, origin, scale));
    // Footprint (blocks placement even with 0 colliders):
    Rectangle footprint = VaToRaylib(zeph.ImageWorldRect(origin, scale));
}
```

`VillageAssetLoader::LoadCatalog("VillageAssets")` scans the whole folder (uses
`<filesystem>`; project is C++17 so this is fine). Engine already enumerates the
folder via raylib `LoadDirectoryFiles`, so it can also just call `Load()` per file.

## To adopt
1. Add `VillageAssetData.h/.cpp` to `TestGame.vcxproj` (+ filters).
2. Swap the hardcoded Graveyard/Zeph `.vasset` reads for `VillageAssetLoader::Load`.
3. Later: author Zeph `collider` lines in `ZephsShop.vasset` (loader already supports them).
4. Later: migrate Zeph's hardcoded free/one-time/permanent rules to `cost 0` /
   `required 1` / `unique 1` / `removable 0` in `ZephsShop.vasset`.

## Legacy / unchanged
- `villageobject_*.txt` (tile-object builder) stays as-is for now.
- `.vasset` stays version 1 — no migration needed.
```
