#include "Engine.h"

#include "AnimationUtils.h"
#include "AssetPaths.h"
#include "raymath.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <queue>

namespace
{
    // ── Resource economy constants ────────────────────────────────────────────
    // Mana is now primarily passive-regen (see Character::kManaRegenPerSecond).
    // Pickups are supplemental / precious, not the main resource source.
    // Boss fights suppress all timed and drop pickups — they should be won on
    // build and execution, not on floor loot.
    //
    // Store hook: when a store is added between key waves, these intervals can
    // be raised further and the store becomes the main recovery vector.

    // Seconds between timed heal drops during normal waves.
    constexpr float kDefaultTimedPickupInterval = 45.f;

    // Drop chance per enemy kill (normal waves only — boss fight suppresses drops).
    constexpr int kEnemyDropChancePercent = 8;

    constexpr float kBiomeFadeOutDuration = 3.f;
    constexpr float kBiomeFadeInDuration = 1.f;

    // Spawn protection gives the player time to reposition if a wave enters on
    // top of them or very near the player after pathing/spawn validation.
    constexpr float kWaveSpawnProtectionDuration = 2.f;

    // Boss support adds keep the fight active, but they should respawn far
    // enough away that the player gets a real reposition window before the
    // next Ogre/Cyclops pressure cycle starts.
    constexpr float kBossSupportRespawnDelay = 28.f;
    constexpr float kBossSupportMinPlayerDistance = 520.f;
    // Ogre enters the boss fight on a delay so the player has time to read
    // the boss before juggling three threats simultaneously.
    constexpr float kBossOgreInitialDelay = 22.f;

    int ClampInt(int value, int minValue, int maxValue)
    {
        if (value < minValue)
            return minValue;
        if (value > maxValue)
            return maxValue;
        return value;
    }

    int GetNavigationIndexForGrid(int col, int row, int cols)
    {
        return row * cols + col;
    }

    bool IsNavigationCellBlockedForGrid(const std::vector<bool>& blocked, int cols, int rows, int col, int row)
    {
        if (col < 0 || col >= cols || row < 0 || row >= rows)
            return true;

        return blocked[GetNavigationIndexForGrid(col, row, cols)];
    }

    bool FindNearestOpenCellForGrid(const std::vector<bool>& blocked, int cols, int rows, int& col, int& row)
    {
        if (!IsNavigationCellBlockedForGrid(blocked, cols, rows, col, row))
            return true;

        for (int radius = 1; radius < std::max(cols, rows); ++radius)
        {
            for (int dc = -radius; dc <= radius; ++dc)
            {
                for (int dr = -radius; dr <= radius; ++dr)
                {
                    if (std::abs(dc) != radius && std::abs(dr) != radius)
                        continue;

                    int nextCol = col + dc;
                    int nextRow = row + dr;
                    if (!IsNavigationCellBlockedForGrid(blocked, cols, rows, nextCol, nextRow))
                    {
                        col = nextCol;
                        row = nextRow;
                        return true;
                    }
                }
            }
        }

        return false;
    }

    int GetClosestOpenNavigationIndexForGrid(const std::vector<bool>& blocked, int cols, int rows, int col, int row)
    {
        int nextCol = ClampInt(col, 0, std::max(0, cols - 1));
        int nextRow = ClampInt(row, 0, std::max(0, rows - 1));

        if (!FindNearestOpenCellForGrid(blocked, cols, rows, nextCol, nextRow))
            return -1;

        return GetNavigationIndexForGrid(nextCol, nextRow, cols);
    }

    Engine::NavigationRefreshResult BuildNavigationRefreshResult(
        const std::vector<bool>& blocked,
        int cols,
        int rows,
        float cellSize,
        Vector2 playerFeet)
    {
        Engine::NavigationRefreshResult result{};

        if (cols <= 0 || rows <= 0)
            return result;

        result.distanceField.assign(cols * rows, std::numeric_limits<int>::max());

        int playerCol = ClampInt((int)(playerFeet.x / cellSize), 0, cols - 1);
        int playerRow = ClampInt((int)(playerFeet.y / cellSize), 0, rows - 1);
        result.playerNavIndex = GetClosestOpenNavigationIndexForGrid(blocked, cols, rows, playerCol, playerRow);

        if (result.playerNavIndex < 0)
            return result;

        using QueueItem = std::pair<int, int>;
        std::priority_queue<QueueItem, std::vector<QueueItem>, std::greater<QueueItem>> frontier;
        result.distanceField[result.playerNavIndex] = 0;
        frontier.push({ 0, result.playerNavIndex });

        struct SearchOffset { int col, row, cost; };
        static const std::array<SearchOffset, 8> navOffsets{{
            {1,0,10},{-1,0,10},{0,1,10},{0,-1,10},
            {1,1,14},{1,-1,14},{-1,1,14},{-1,-1,14}
        }};

        while (!frontier.empty())
        {
            int cost = frontier.top().first;
            int currentIndex = frontier.top().second;
            frontier.pop();

            if (cost > result.distanceField[currentIndex])
                continue;

            int currentCol = currentIndex % cols;
            int currentRow = currentIndex / cols;

            for (const SearchOffset& off : navOffsets)
            {
                int nextCol = currentCol + off.col;
                int nextRow = currentRow + off.row;

                if (IsNavigationCellBlockedForGrid(blocked, cols, rows, nextCol, nextRow))
                    continue;

                if (off.col != 0 && off.row != 0)
                {
                    if (IsNavigationCellBlockedForGrid(blocked, cols, rows, currentCol + off.col, currentRow) ||
                        IsNavigationCellBlockedForGrid(blocked, cols, rows, currentCol, currentRow + off.row))
                    {
                        continue;
                    }
                }

                int nextIndex = GetNavigationIndexForGrid(nextCol, nextRow, cols);
                int nextCost = cost + off.cost;

                if (nextCost >= result.distanceField[nextIndex])
                    continue;

                result.distanceField[nextIndex] = nextCost;
                frontier.push({ nextCost, nextIndex });
            }
        }

        return result;
    }
}

Engine::Engine()
{
    Init();
}

Engine::~Engine()
{
    if (_navRefreshJob.valid())
        _navRefreshJob.wait();

    Enemy::UnloadSharedResources();
    Cyclops::UnloadSharedResources();
    Ogre::UnloadSharedResources();
    Molarbeast::UnloadSharedResources();
    // [SHELVED] FireBallPickup/SwordBeamPickup/FreezePickup unload calls removed
    HealPickup::UnloadSharedResources();
    ManaGemPickup::UnloadSharedResources();
    SpreadProjectile::UnloadSharedResources();
    LavaBallProjectile::UnloadSharedResources();
    UnloadSound(_pickupSound);
    UnloadSound(_fireballCastSound);
    UnloadSound(_explosionSound);
    UnloadSound(_lavaBallImpactSound);
    UnloadSound(_buttonPressSound);
    UnloadTexture(_map);
    UnloadTexture(_pillarTex);
    UnloadTexture(_torchTex);
    UnloadTexture(_pillarTorchTex);
    UnloadTexture(_treeTex);
    UnloadTexture(_smallTreeTex);
    UnloadTexture(_rockTex);
    UnloadTexture(_bigRockTex);
    UnloadTexture(_fireballCastTex);
    UnloadTexture(_fireballHitTex);
    UnloadTexture(_genericHitTex);
    UnloadTexture(_iceHitTex);
    UnloadTexture(_lightningCastTex);
    UnloadTexture(_healEffectTex);
    UnloadTexture(_abilityIconFireTex);
    UnloadTexture(_abilityIconIceTex);
    UnloadTexture(_abilityIconElectricTex);
    UnloadTexture(_upgradeAttackPowerTex);
    UnloadTexture(_upgradeAttackRangeTex);
    UnloadTexture(_upgradeHealthTex);
    UnloadTexture(_upgradeMagicTex);
    UnloadTexture(_upgradeDefenseTex);
    UnloadTexture(_upgradeMoveSpeedTex);
    _pauseUI.Unload();
    CloseWindow();
}

void Engine::Init()
{
    InitWindow(_windowWidth, _windowHeight, "Mystic Onslaught");
    SetTargetFPS(60);
    InitAudioDevice();

    _leaderboard.Load("leaderboard.txt");

    SetExitKey(KEY_NULL);

    _pickupSound      = LoadSound(AssetPath("Sounds/PickupSound.mp3").c_str());
    _fireballCastSound= LoadSound(AssetPath("Sounds/GS1_Spell_Fire.mp3").c_str());
    _explosionSound   = LoadSound(AssetPath("Sounds/GS1_Spell_Explode.mp3").c_str());
    {
        // Lavaball impact should only play for the first second of the clip.
        // Cropping the wave at load time keeps playback simple and lets each
        // collision just trigger a normal PlaySound call.
        Wave lavaImpactWave = LoadWave(AssetPath("Sounds/Explosion.wav").c_str());
        int oneSecondFrame = lavaImpactWave.sampleRate;
        if (oneSecondFrame > 0 && (int)lavaImpactWave.frameCount > oneSecondFrame)
            WaveCrop(&lavaImpactWave, 0, oneSecondFrame);
        _lavaBallImpactSound = LoadSoundFromWave(lavaImpactWave);
        UnloadWave(lavaImpactWave);
    }
    _buttonPressSound = LoadSound(AssetPath("Sounds/ButtonPress.mp3").c_str());

    SetSoundPitch (_buttonPressSound, 1.25f);
    SetSoundVolume(_buttonPressSound, 0.35f);

    SetSoundPitch (_pickupSound, 1.35f);
    SetSoundVolume(_pickupSound, 0.45f);
    SetSoundVolume(_lavaBallImpactSound, 0.45f);

    _pauseUI.Init();
    LoadKeybindings();

    _menu.Init();

    _pillarTex      = LoadTexture(AssetPath("TileSet/Pillar.png").c_str());
    _torchTex       = LoadTexture(AssetPath("TileSet/Torch.png").c_str());
    _pillarTorchTex = LoadTexture(AssetPath("TileSet/PillarTorch.png").c_str());
    _treeTex        = LoadTexture(AssetPath("ForestLevel/Tree.png").c_str());
    _smallTreeTex   = LoadTexture(AssetPath("ForestLevel/SmallTree.png").c_str());
    _rockTex        = LoadTexture(AssetPath("ForestLevel/Rock.png").c_str());
    _bigRockTex     = LoadTexture(AssetPath("ForestLevel/BigRock.png").c_str());
    _fireballCastTex = LoadTexture(AssetPath("PowerUps/Fireball_Cast.png").c_str());
    _fireballHitTex  = LoadTexture(AssetPath("PowerUps/Fireball_Hit.png").c_str());
    _genericHitTex   = LoadTexture(AssetPath("PowerUps/Hit03.png").c_str());
    _iceHitTex       = LoadTexture(AssetPath("PowerUps/Ice_Shard_Hit.png").c_str());
    _lightningCastTex = LoadTexture(AssetPath("PowerUps/LightningCast.png").c_str());
    _healEffectTex = LoadTexture(AssetPath("PowerUps/Health_Up.png").c_str());
    _abilityIconFireTex     = LoadTexture(AssetPath("PowerUps/FireBallPickup.png").c_str());
    _abilityIconIceTex      = LoadTexture(AssetPath("PowerUps/IceSpellPickup.png").c_str());
    _abilityIconElectricTex = LoadTexture(AssetPath("PowerUps/LightningPickup.png").c_str());

    _upgradeAttackPowerTex = LoadTexture(AssetPath("UI/Upgrades/AttackPower.png").c_str());
    _upgradeAttackRangeTex = LoadTexture(AssetPath("UI/Upgrades/AttackRange.png").c_str());
    _upgradeHealthTex      = LoadTexture(AssetPath("UI/Upgrades/HealthUpgrade.png").c_str());
    _upgradeMagicTex       = LoadTexture(AssetPath("UI/Upgrades/Magic.png").c_str());
    _upgradeDefenseTex     = LoadTexture(AssetPath("UI/Upgrades/Defense.png").c_str());
    _upgradeMoveSpeedTex   = LoadTexture(AssetPath("UI/Upgrades/MoveSpeed.png").c_str());

    _props.clear();
    _pickups.clear();
    _spreadProjectiles.clear();
    _ultimateBlasts.clear();
    _lavaBalls.clear();
    _effects.clear();
    _enemies.clear();

    _pickupSpawnTimer = kDefaultTimedPickupInterval;

    ResetRunState();
}

// ── Room progression — replaces the old SpawnWave() system ───────────────────
// Called whenever the player is about to enter a specific room type.
// Handles act advancement, biome transitions, and encounter setup.
void Engine::StartNextRoom(RoomType type)
{
    _currentRoom++;
    if (_currentRoom > 6)
    {
        _currentRoom = 1;
        _currentAct++;
    }
    _wave++;  // _wave = total rooms entered — feeds GetEnemyPowerLevelForWave()
    _currentRoomType = type;

    // Non-combat rooms get a rest timer before the choice screen appears.
    if (type == RoomType::Rest || type == RoomType::Store)
        _roomClearTimer = 5.0f;
    else
        _roomClearTimer = 0.f;

    std::string actName = GetBiomeName(GetBiomeForAct(_currentAct));
    _message = "Act " + std::to_string(_currentAct) + " - " + actName;

    _waveStarting   = true;
    _waveIntroTimer = _waveIntroDuration;

    // Biome transition when the act changes
    Biome nextBiome = GetBiomeForAct(_currentAct);
    if (nextBiome != _currentBiome)
    {
        _pendingBiome              = nextBiome;
        _biomeTransitionActive     = true;
        _biomeTransitionSwapped    = false;
        _biomeTransitionTimer      = kBiomeFadeOutDuration + kBiomeFadeInDuration;
    }
}

// (ShowRoomChoiceScreen / GenerateRoomChoices removed — replaced by map system)

Biome Engine::GetBiomeForAct(int act) const
{
    // Alternates Dungeon / Forest.  _startBiomeDungeon flips each run for variety.
    bool isDungeon = _startBiomeDungeon ? ((act % 2) == 1) : ((act % 2) == 0);
    return isDungeon ? Biome::Dungeon : Biome::Forest;
}

// ── PickRoomTypeForRow ────────────────────────────────────────────────────────
// Weighted random room type based on depth within the act (row 1 = early, 4 = pre-boss).
RoomType Engine::PickRoomTypeForRow(int row)
{
    switch (row)
    {
    case 1:
    {
        int r = GetRandomValue(0, 9);
        if (r <= 4) return RoomType::Standard;
        if (r <= 6) return RoomType::Rest;
        if (r <= 8) return RoomType::Elite;
        return RoomType::Treasure;
    }
    case 2:
    {
        int r = GetRandomValue(0, 9);
        if (r <= 2) return RoomType::Standard;
        if (r <= 4) return RoomType::Elite;
        if (r <= 6) return RoomType::Rest;
        if (r <= 7) return RoomType::Treasure;
        return RoomType::Store;
    }
    case 3:
    {
        int r = GetRandomValue(0, 9);
        if (r <= 2) return RoomType::Standard;
        if (r <= 4) return RoomType::Elite;
        if (r <= 6) return RoomType::Treasure;
        if (r <= 7) return RoomType::Rest;
        return RoomType::Store;
    }
    case 4:
        // Pre-boss: Standard or Elite only
        return (GetRandomValue(0, 1) == 0) ? RoomType::Standard : RoomType::Elite;
    default:
        return RoomType::Standard;
    }
}

// ── GenerateActMap ────────────────────────────────────────────────────────────
// Builds the full 6-row directed-acyclic node graph for the current act.
// Row 0 = entry (Standard), rows 1-4 = branching options, row 5 = Boss.
// All rows and connections are visible immediately (Slay the Spire style).
void Engine::GenerateActMap()
{
    _actMap.clear();
    _currentMapNodeIdx = -1;
    _mapOpenTimer = 0.f;

    // ── 1. Decide per-row node counts ──────────────────────────────────────
    int rowCounts[6];
    rowCounts[0] = 1;
    rowCounts[5] = 1;
    for (int r = 1; r <= 4; r++)
        rowCounts[r] = GetRandomValue(1, 3);

    // ── 2. Create nodes ───────────────────────────────────────────────────
    int rowStart[6];
    for (int r = 0; r < 6; r++)
    {
        rowStart[r] = (int)_actMap.size();
        for (int i = 0; i < rowCounts[r]; i++)
        {
            MapNode n;
            n.row = r;
            n.normX = (rowCounts[r] == 1) ? 0.5f
                      : 0.2f + (float)i / (rowCounts[r] - 1) * 0.6f;

            if (r == 0)       n.type = RoomType::Standard;
            else if (r == 5)  n.type = RoomType::Boss;
            else              n.type = PickRoomTypeForRow(r);

            n.available  = (r == 0);   // only the entry node starts clickable
            n.completed  = false;
            _actMap.push_back(n);
        }
    }

    // ── 3. Build connections ──────────────────────────────────────────────
    for (int r = 0; r < 5; r++)
    {
        int sR  = rowStart[r],     cR  = rowCounts[r];
        int sR1 = rowStart[r + 1], cR1 = rowCounts[r + 1];

        // Primary: map each row-r node proportionally to a row-(r+1) node
        for (int i = 0; i < cR; i++)
        {
            float t = (cR > 1) ? (float)i / (cR - 1) : 0.5f;
            int j = (int)roundf(t * (float)(cR1 - 1));
            j = std::max(0, std::min(cR1 - 1, j));
            int src = sR + i, dst = sR1 + j;
            auto& nxt = _actMap[src].nextNodes;
            if (std::find(nxt.begin(), nxt.end(), dst) == nxt.end())
                nxt.push_back(dst);
        }

        // Guarantee every row-(r+1) node has at least one incoming connection
        for (int j = 0; j < cR1; j++)
        {
            int dst = sR1 + j;
            bool found = false;
            for (int i = 0; i < cR && !found; i++)
            {
                auto& nxt = _actMap[sR + i].nextNodes;
                found = std::find(nxt.begin(), nxt.end(), dst) != nxt.end();
            }
            if (!found)
            {
                float dX = _actMap[dst].normX;
                int bestSrc = sR;
                float bestD = 999.f;
                for (int i = 0; i < cR; i++)
                {
                    float d = fabsf(_actMap[sR + i].normX - dX);
                    if (d < bestD) { bestD = d; bestSrc = sR + i; }
                }
                _actMap[bestSrc].nextNodes.push_back(dst);
            }
        }

        // Optional extra branches (~30% chance) for wider path variety
        for (int i = 0; i < cR; i++)
        {
            if (cR1 > 1 && GetRandomValue(0, 9) < 3)
            {
                auto& nxt = _actMap[sR + i].nextNodes;
                if (nxt.empty()) continue;
                int baseJ = nxt[0] - sR1;
                int altJ  = (baseJ > 0) ? baseJ - 1 : baseJ + 1;
                altJ = std::max(0, std::min(cR1 - 1, altJ));
                int alt = sR1 + altJ;
                if (std::find(nxt.begin(), nxt.end(), alt) == nxt.end())
                    nxt.push_back(alt);
            }
        }
    }

    // ── 4. Compute draw positions ─────────────────────────────────────────
    // Boss (row 5) sits at the top; entry (row 0) at the bottom.
    const float mapTop  = 130.f;
    const float mapBot  = (float)_windowHeight - 110.f;
    const float rowH    = (mapBot - mapTop) / 5.f;
    const float mapLeft = (float)_windowWidth  * 0.20f;
    const float mapRight= (float)_windowWidth  * 0.80f;

    for (auto& node : _actMap)
    {
        float y = mapBot - node.row * rowH;
        float x = mapLeft + node.normX * (mapRight - mapLeft);
        node.drawPos = { x, y };
    }
}

// ── EnterMapRoom ──────────────────────────────────────────────────────────────
// Called when the player clicks an available map node.
// Sets up the room state and transitions to Play.
void Engine::EnterMapRoom(int idx)
{
    if (idx < 0 || idx >= (int)_actMap.size()) return;

    _roomClearPending = false;

    // Lock all nodes at or behind the entered row so the player can never go backward.
    int enteredRow = _actMap[idx].row;
    for (auto& n : _actMap)
        if (n.row <= enteredRow)
            n.available = false;

    _currentMapNodeIdx = idx;

    // Row 0 = Room 1, Row 5 = Room 6 (Boss)
    _currentRoom     = _actMap[idx].row + 1;
    _wave++;
    _currentRoomType = _actMap[idx].type;

    if (_currentRoomType == RoomType::Rest || _currentRoomType == RoomType::Store)
        _roomClearTimer = 5.0f;
    else
        _roomClearTimer = 0.f;

    _message = "Act " + std::to_string(_currentAct) + " - " + GetBiomeName(GetBiomeForAct(_currentAct));

    _waveStarting   = true;
    _waveIntroTimer = _waveIntroDuration;

    Biome nextBiome = GetBiomeForAct(_currentAct);
    if (nextBiome != _currentBiome)
    {
        _pendingBiome           = nextBiome;
        _biomeTransitionActive  = true;
        _biomeTransitionSwapped = false;
        _biomeTransitionTimer   = kBiomeFadeOutDuration + kBiomeFadeInDuration;
    }
    else
    {
        // Same biome — re-randomise prop layout so every room feels distinct.
        PopulatePropsForBiome(_currentBiome);
        BuildNavigationGrid();
        _navRefreshTimer = 0.f;   // force distance-field recompute next frame
    }

    // Fade in from black each time a room is entered.
    _fadeInTimer    = 1.2f;
    _fadeInDuration = 1.2f;

    _gameState = GameState::Play;
}

void Engine::SpawnEnemies()
{
    float mapW = _map.width * _mapScale;
    float mapH = _map.height * _mapScale;

    // Boss room — Molarbeast + supports
    if (_currentRoomType == RoomType::Boss)
    {
        Vector2 pos{ mapW * 0.5f, mapH * 0.28f };
        SpawnMolarbeast(pos);
        SpawnBossSupportAdds();
        return;
    }

    // Non-combat rooms — Rest, Store spawn no enemies here.
    // Treasure is handled before SpawnEnemies() is called (via UpdateGamePlay).
    if (_currentRoomType == RoomType::Rest ||
        _currentRoomType == RoomType::Store)
    {
        // Spawn heal pickups for rest rooms
        if (_currentRoomType == RoomType::Rest)
        {
            int healCount = GetRandomValue(2, 3);
            for (int i = 0; i < healCount; i++)
            {
                Vector2 pos{};
                for (int a = 0; a < 30; a++)
                {
                    pos.x = (float)GetRandomValue(300, (int)mapW - 300);
                    pos.y = (float)GetRandomValue(300, (int)mapH - 300);
                    if (IsSpawnPositionValid(pos)) break;
                }
                auto p = std::make_unique<HealPickup>();
                p->Init(pos);
                _pickups.push_back(std::move(p));
            }
        }
        return;
    }

    // ── Combat room composition table ────────────────────────────────────────
    // Act 1 introduces enemy types gradually (rooms 1-5).
    // Act 2+ uses a consistently harder baseline.
    // Elite rooms add bonus enemies on top of the standard count.
    int regularCount = 0, cyclopsCount = 0, ogreCount = 0;

    if (_currentAct == 1)
    {
        switch (_currentRoom)
        {
        case 1: regularCount = 2; cyclopsCount = 0; ogreCount = 0; break;
        case 2: regularCount = 4; cyclopsCount = 0; ogreCount = 0; break;
        case 3: regularCount = 4; cyclopsCount = 1; ogreCount = 0; break;
        case 4: regularCount = 5; cyclopsCount = 1; ogreCount = 0; break;
        case 5: regularCount = 6; cyclopsCount = 1; ogreCount = 1; break;
        default: regularCount = 4; cyclopsCount = 0; ogreCount = 0; break;
        }
    }
    else
    {
        // Acts 2+: full pressure from room 1
        switch (_currentRoom)
        {
        case 1: regularCount = 6; cyclopsCount = 1; ogreCount = 1; break;
        case 2: regularCount = 7; cyclopsCount = 1; ogreCount = 1; break;
        case 3: regularCount = 7; cyclopsCount = 2; ogreCount = 1; break;
        case 4: regularCount = 8; cyclopsCount = 2; ogreCount = 2; break;
        case 5: regularCount = 8; cyclopsCount = 2; ogreCount = 2; break;
        default: regularCount = 6; cyclopsCount = 1; ogreCount = 1; break;
        }
    }

    // Elite modifier: harder variant of the standard composition
    if (_currentRoomType == RoomType::Elite)
    {
        regularCount  += 2;
        cyclopsCount  += 1;
    }

    auto spawnPos = [&]() -> Vector2 {
        Vector2 p{};
        for (int a = 0; a < 40; a++)
        {
            p.x = (float)GetRandomValue(300, (int)mapW - 300);
            p.y = (float)GetRandomValue(300, (int)mapH - 300);
            if (IsSpawnPositionValid(p)) break;
        }
        return p;
    };

    for (int i = 0; i < regularCount; i++)
    {
        Vector2 p = spawnPos();
        if (TryGetPooledEnemySpawn(p)) continue;
        auto enemy = std::make_unique<Enemy>(p);
        enemy->Init();
        ConfigureSpawnedEnemy(*enemy);
        _enemies.push_back(std::move(enemy));
    }

    for (int i = 0; i < cyclopsCount; i++)
        SpawnCyclops(spawnPos());

    for (int i = 0; i < ogreCount; i++)
        SpawnOgre(spawnPos());
}

void Engine::Run()
{
    while (!WindowShouldClose() && !_shouldClose)
        RunFrame();
}

void Engine::RunFrame()
{
    float dt = GetFrameTime();

    Update(dt);

    BeginDrawing();
    ClearBackground(WHITE);
    Draw();
    EndDrawing();
}

void Engine::Update(float dt)
{
    switch (_gameState)
    {
    case GameState::Menu:
    {
        _menu.Update();

        if (_menu.StartPressed())
        {
            _touchModeActive = _menu.IsTouchMode();
            ResetRunState();
            _fadeInTimer = 2.0f; _fadeInDuration = 2.0f;
            GenerateStartingAbilityOptions();
            _awaitingStartingAbility = true;
            _levelUpReturnState = GameState::Map;  // after picking, show the act map
            _levelUpOpenTimer = 0.8f;
            _gameState = GameState::LevelUpChoice;
        }
        if (_menu.QuitPressed())
            _shouldClose = true;
        if (_menu.HowToPressed())
        {
            _howToPlayFrom = GameState::Menu;
            _gameState = GameState::HowToPlay;
        }
        if (_menu.LeaderboardPressed())
            _gameState = GameState::Leaderboard;

        break;
    }

    case GameState::HowToPlay:
    {
        if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_BACKSPACE))
        {
            if (_howToPlayFrom == GameState::Menu)
                _menu.Init();
            _gameState = _howToPlayFrom;
        }
        break;
    }

    case GameState::Keybindings:
        // Navigation handled in the Draw case (Back button)
        break;

    case GameState::Play:
    {
        UpdateGamePlay(dt);
        break;
    }

    case GameState::Pause:
    {
        if (IsKeyPressed(KEY_ESCAPE))
            _gameState = GameState::Play;

        break;
    }

    case GameState::GameOver:
        break;

    case GameState::LevelUpChoice:
        if (_levelUpOpenTimer > 0.f)
            _levelUpOpenTimer -= dt;
        break;

    case GameState::AbilityChoice:
        if (_abilityChoiceOpenTimer > 0.f)
            _abilityChoiceOpenTimer -= dt;
        break;

    case GameState::Map:
        if (_mapOpenTimer > 0.f)
        {
            _mapOpenTimer -= dt;
        }
        else
        {
            // Collect available (clickable) node indices in left-to-right order.
            std::vector<int> avail;
            for (int i = 0; i < (int)_actMap.size(); i++)
                if (_actMap[i].available && !_actMap[i].completed)
                    avail.push_back(i);

            // Sort by horizontal draw position so Left/Right feel spatial.
            std::sort(avail.begin(), avail.end(), [this](int a, int b){
                return _actMap[a].drawPos.x < _actMap[b].drawPos.x;
            });

            if (!avail.empty())
            {
                // Auto-select the first node if nothing is selected yet.
                auto it = std::find(avail.begin(), avail.end(), _mapKeySelectedIdx);
                if (it == avail.end())
                {
                    _mapKeySelectedIdx = avail[0];
                    it = avail.begin();
                }

                int pos = (int)(it - avail.begin());
                int n   = (int)avail.size();

                if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D))
                    _mapKeySelectedIdx = avail[(pos + 1) % n];
                if (IsKeyPressed(KEY_LEFT)  || IsKeyPressed(KEY_A))
                    _mapKeySelectedIdx = avail[(pos + n - 1) % n];

                if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE))
                    EnterMapRoom(_mapKeySelectedIdx);
            }
        }
        break;
    }
}

void Engine::UpdateGamePlay(float dt)
{
    if (IsKeyPressed(KEY_ESCAPE))
    {
        _gameState = GameState::Pause;
        return;
    }

    // M key opens the map as soon as combat ends — same effect as clicking Continue.
    if (_roomClearPending && IsKeyPressed(KEY_M))
    {
        _roomClearPending = false;
        if (_currentMapNodeIdx >= 0 && _currentMapNodeIdx < (int)_actMap.size())
        {
            _actMap[_currentMapNodeIdx].completed = true;
            for (int next : _actMap[_currentMapNodeIdx].nextNodes)
                if (next >= 0 && next < (int)_actMap.size())
                    _actMap[next].available = true;
        }
        _mapKeySelectedIdx = -1;
        _mapOpenTimer = 0.4f;
        _gameState = GameState::Map;
        return;
    }

    if (_fadeInTimer > 0.f)
        _fadeInTimer -= dt;
    if (_bossWarningTimer > 0.f)
        _bossWarningTimer -= dt;
    if (_biomeTransitionActive)
        UpdateBiomeTransition(dt);

    // Block attacks during room intro and ultimate cinematic.
    // Also lock combat in non-combat rooms (rest/store/treasure) so the player
    // can't accidentally trigger the attack animation while browsing.
    bool inNonCombatRoom = (_currentRoomType == RoomType::Rest  ||
                            _currentRoomType == RoomType::Store ||
                            _currentRoomType == RoomType::Treasure);
    _player.SetCombatLocked(_waveStarting || _ultimatePhase != UltimatePhase::None || inNonCombatRoom);

    // Touch controls — must be set on player before Update() consumes them
    _player.SetTouchModeEnabled(_touchModeActive);
    if (_touchModeActive)
        UpdateTouchControls();

    _player.Update(dt);

    // During the cinematic ultimate sequence everything except the player
    // animation is frozen. Drain any pending cast request so it doesn't fire
    // twice when play resumes.
    if (_ultimatePhase != UltimatePhase::None)
    {
        _player.ConsumeCastRequest();
        UpdateUltimateSequence(dt);
        return;
    }

    Character::CastType castType = _player.ConsumeCastRequest();

    if (castType == Character::CastType::FireSpread ||
        castType == Character::CastType::IceSpread  ||
        castType == Character::CastType::ElectricSpread)
    {
        AbilityType element =
            (castType == Character::CastType::FireSpread)     ? AbilityType::FireSpread :
            (castType == Character::CastType::IceSpread)      ? AbilityType::IceSpread  :
                                                                 AbilityType::ElectricSpread;
        SpawnCastEffect(castType);
        StopSound(_fireballCastSound);
        PlaySound(_fireballCastSound);
        SpawnSpreadBurst(element);
    }
    else if (castType == Character::CastType::FireUltimate  ||
             castType == Character::CastType::IceUltimate   ||
             castType == Character::CastType::ElectricUltimate)
    {
        AbilityType element =
            (castType == Character::CastType::FireUltimate)     ? AbilityType::FireUltimate  :
            (castType == Character::CastType::IceUltimate)      ? AbilityType::IceUltimate   :
                                                                   AbilityType::ElectricUltimate;
        TriggerUltimateSequence(element);
    }
    else if (castType == Character::CastType::FireBolt  ||
             castType == Character::CastType::IceBolt   ||
             castType == Character::CastType::ElectricBolt)
    {
        AbilityType element =
            (castType == Character::CastType::FireBolt)     ? AbilityType::FireBolt  :
            (castType == Character::CastType::IceBolt)      ? AbilityType::IceBolt   :
                                                               AbilityType::ElectricBolt;
        // Reuse the matching spread cast effect for visual feedback
        Character::CastType effectType =
            (element == AbilityType::FireBolt)     ? Character::CastType::FireSpread :
            (element == AbilityType::IceBolt)      ? Character::CastType::IceSpread  :
                                                     Character::CastType::ElectricSpread;
        SpawnCastEffect(effectType);
        StopSound(_fireballCastSound);
        PlaySound(_fireballCastSound);
        SpawnBolt(element);
    }

    if (_player.GetHealthValue() <= 0.f && !_playerDying)
    {
        _playerDying = true;
        _gameOverTimer = _gameOverDelay;
    }

    if (_playerDying)
    {
        _gameOverTimer -= dt;

        if (_gameOverTimer <= 0.f)
        {
            _awaitingNameEntry = true;
            _pauseUI.ResetNameEntry();
            _gameState = GameState::GameOver;
        }

        return;
    }

    if (_waveStarting)
    {
        _waveIntroTimer -= dt;

        if (!_biomeTransitionActive && _waveIntroTimer <= 0.f)
        {
            _waveStarting = false;

            // Treasure room: grant a free upgrade card, then return to map.
            if (_currentRoomType == RoomType::Treasure)
            {
                // Mark this node complete now so the map is ready on return.
                if (_currentMapNodeIdx >= 0 && _currentMapNodeIdx < (int)_actMap.size())
                {
                    _actMap[_currentMapNodeIdx].completed = true;
                    for (int next : _actMap[_currentMapNodeIdx].nextNodes)
                        if (next >= 0 && next < (int)_actMap.size())
                            _actMap[next].available = true;
                }
                GenerateLevelUpOptions();
                _levelUpReturnState = GameState::Map;
                _levelUpOpenTimer   = 0.25f;
                _gameState          = GameState::LevelUpChoice;
                return;
            }

            SpawnEnemies();
            _player.GrantInvulnerability(kWaveSpawnProtectionDuration);
        }
    }
    else
    {
        _gameTimer += dt;
        ApplyCompletedNavigationRefresh();

        // Timed heal drops — boss fights and non-combat rooms suppress this.
        if (_currentRoomType != RoomType::Rest  &&
            _currentRoomType != RoomType::Store &&
            _currentRoomType != RoomType::Treasure)
        {
            _pickupSpawnTimer -= dt;
            if (_pickupSpawnTimer <= 0.f)
            {
                SpawnTimedPickup();
                _pickupSpawnTimer = kDefaultTimedPickupInterval;
            }
        }

        // Non-combat rooms (Rest/Store) use a countdown timer before showing the map.
        if (_currentRoomType == RoomType::Rest || _currentRoomType == RoomType::Store)
        {
            _roomClearTimer -= dt;
            if (_roomClearTimer <= 0.f)
            {
                if (_currentMapNodeIdx >= 0 && _currentMapNodeIdx < (int)_actMap.size())
                {
                    _actMap[_currentMapNodeIdx].completed = true;
                    for (int next : _actMap[_currentMapNodeIdx].nextNodes)
                        if (next >= 0 && next < (int)_actMap.size())
                            _actMap[next].available = true;
                }
                _mapOpenTimer = 0.4f;
                _gameState = GameState::Map;
            }
            // Don't fall into the enemy-count check below for these rooms
        }
        else if (!_roomClearPending && GetActiveEnemyCount() == 0)
        {
            // After the very first room clears, guarantee a level-up.
            if (_wave == 1 && !_wave1LevelUpDone)
            {
                _wave1LevelUpDone = true;
                int prevLevel  = _player.GetLevel();
                int remaining  = _player.GetExpToNext() - _player.GetExp();
                if (remaining > 0)
                    _player.AddExp(remaining);
                if (_player.GetLevel() > prevLevel)
                {
                    GenerateLevelUpOptions();
                    _levelUpReturnState = GameState::Play;
                    _levelUpOpenTimer   = 0.25f;
                    _gameState          = GameState::LevelUpChoice;
                    return;
                }
            }

            // Boss room cleared — offer ability upgrade (AbilityChoice is its own break).
            if (_currentRoomType == RoomType::Boss && _lastAbilityChoiceWave != _wave)
            {
                _lastAbilityChoiceWave  = _wave;
                GenerateAbilityChoiceOptions();
                _abilityChoiceOpenTimer = 0.5f;
                _pendingRoomChoice      = true;  // after picking ability → new act map
                _gameState = GameState::AbilityChoice;
                return;
            }

            // All other combat rooms: wait for the player to deliberately advance.
            _roomClearPending = true;
        }

        _navRefreshTimer -= dt;
        Vector2 playerFeet = _player.GetFeetWorldPos();
        int playerCol = ClampInt((int)(playerFeet.x / _navCellSize), 0, std::max(0, _navCols - 1));
        int playerRow = ClampInt((int)(playerFeet.y / _navCellSize), 0, std::max(0, _navRows - 1));
        int playerNavIndex = (_navCols > 0 && _navRows > 0) ? GetClosestOpenNavigationIndex(playerCol, playerRow) : -1;
        if (!_navRefreshInFlight && (_navRefreshTimer <= 0.f || playerNavIndex != _lastPlayerNavIndex))
        {
            RefreshNavigationField();
            _navRefreshTimer = _navRefreshInterval;
        }

        std::vector<Vector2> propCenters;
        propCenters.reserve(_props.size());
        for (auto& prop : _props)
            propCenters.push_back(prop.GetWorldPos());

        for (auto& enemy : _enemies)
        {
            if (!enemy->IsActive())
                continue;

            Vector2 navigationTarget = playerFeet;
            bool hasNavigationTarget = false;

            if (enemy->IsBoss())
            {
                // Boss pathing always comes from the A* helper so large enemies
                // can route around pillars consistently instead of only
                // pathing when line of sight is already broken.
                navigationTarget = GetAStarTarget(enemy->GetWorldPos(), playerFeet);
                hasNavigationTarget = !Vector2Equals(navigationTarget, playerFeet);
            }
            else if (!enemy->UsesDirectPursuit() &&
                !HasLineOfSight(enemy->GetWorldPos(), playerFeet))
            {
                navigationTarget = GetNavigationTarget(enemy->GetWorldPos(), playerFeet);
                hasNavigationTarget = !Vector2Equals(navigationTarget, playerFeet);
            }

            enemy->Update(dt, playerFeet, navigationTarget, hasNavigationTarget, _enemies, propCenters);

            // Cyclops shares the enemy pool, but still emits a separate laser
            // projectile when its charge completes. The engine owns the laser
            // list so projectile collision stays centralized.
            if (Cyclops* cyclops = enemy->AsCyclops())
            {
                if (cyclops->WantsToFire())
                {
                    CyclopsLaserProjectile laser;
                    laser.Init(cyclops->GetWorldPos(), cyclops->GetFireDirection(), cyclops->GetAttackPower());
                    _cyclopsLasers.push_back(laser);
                    cyclops->OnFired();
                    cyclops->PlayAttackSound();
                }
            }

            // Ogre rushes request impact shake when they end on a player, prop,
            // wall, or arena boundary. The engine owns the camera system, so
            // it consumes that one-shot request here after each enemy update.
            if (Ogre* ogre = enemy->AsOgre())
            {
                if (ogre->ConsumeImpactShakeRequest())
                    TriggerScreenShake(8.f, 0.14f);
            }

            // Molarbeast uses an engine-owned lavaball list for the same reason
            // Cyclops uses engine-owned lasers: collision with the player and
            // arena boundaries stays centralized in one place.
            if (Molarbeast* molarbeast = enemy->AsMolarbeast())
            {
                if (molarbeast->WantsToFireLavaBall())
                {
                    LavaBallProjectile projectile;
                    Vector2 toTarget = Vector2Subtract(molarbeast->GetQueuedLavaBallTarget(), molarbeast->GetLavaBallSpawnPos());
                    projectile.Init(molarbeast->GetLavaBallSpawnPos(), toTarget);
                    _lavaBalls.push_back(projectile);
                    molarbeast->OnLavaBallSpawned();
                }

                if (molarbeast->ConsumeImpactShakeRequest())
                    TriggerScreenShake(8.f, 0.14f);
            }
        }

        HandlePlayerMeleeDamage();
        UpdateSpreadProjectiles(dt);
        UpdateLavaBallProjectiles(dt);
        UpdateEffects(dt);
        UpdateEnemyCount(dt);
        UpdateBossSupportRespawns(dt);
        UpdateCyclopsLasers(dt);
    }

    for (auto& pickup : _pickups)
    {
        if (!pickup->IsActive())
            continue;

        if (CheckCollisionRecs(_player.GetCollisionRec(), pickup->GetCollisionRec()))
        {
            StopSound(_pickupSound);
            PlaySound(_pickupSound);
            pickup->OnCollect(_player);
        }
    }

    _pickups.erase(
        std::remove_if(_pickups.begin(), _pickups.end(),
            [](const std::unique_ptr<Pickup>& pickup) { return !pickup->IsActive(); }),
        _pickups.end());

    int healEffectsToSpawn = _player.ConsumeHealEffectRequests();
    for (int i = 0; i < healEffectsToSpawn; ++i)
        SpawnHealEffect();

    HandleCollisions();

    {
        float mapW = _map.width  * _mapScale;
        float mapH = _map.height * _mapScale;

        // _cameraPos = world position shown at screen centre
        float camMinX = _windowWidth  / 2.f;
        float camMaxX = mapW - _windowWidth  / 2.f;
        float camMinY = _windowHeight / 2.f;
        float camMaxY = mapH - _windowHeight / 2.f;

        _cameraPos.x = _player.GetWorldPos().x;
        _cameraPos.y = _player.GetWorldPos().y;
        if (_cameraPos.x < camMinX) _cameraPos.x = camMinX;
        if (_cameraPos.x > camMaxX) _cameraPos.x = camMaxX;
        if (_cameraPos.y < camMinY) _cameraPos.y = camMinY;
        if (_cameraPos.y > camMaxY) _cameraPos.y = camMaxY;

        // _mapPos: top-left of map on screen, aligned with the object rendering convention
        _mapPos.x = _windowWidth  / 2.f - _cameraPos.x;
        _mapPos.y = _windowHeight / 2.f - _cameraPos.y;
    }

    if (_shakeTimer > 0.f)
    {
        _shakeTimer -= dt;

        float x = GetRandomValue(-100, 100) / 50.f * _shakeStrength;
        float y = GetRandomValue(-100, 100) / 50.f * _shakeStrength;
        _shakeOffset = Vector2{ x, y };
    }
    else
    {
        _shakeOffset = Vector2Zero();
    }
}

void Engine::Draw()
{
    switch (_gameState)
    {
    case GameState::Menu:
    {
        _menu.Draw();
        break;
    }

    case GameState::Play:
    {
        DrawWorld();
        DrawUltimateSequence();
        DrawHUD();

        // ── "Continue" button — shown after all enemies are defeated ─────────
        if (_roomClearPending)
        {
            const float sw = (float)GetScreenWidth();
            const float sh = (float)GetScreenHeight();

            const char* btnTxt = _touchModeActive ? "Continue" : "Continue  [M]";
            int btnTxtSz = 28;
            const float btnW = 220.f, btnH = 58.f;
            float btnX = sw / 2.f - btnW / 2.f;
            float btnY = sh * 0.82f;
            Rectangle btn{ btnX, btnY, btnW, btnH };

            Vector2 mouse = GetMousePosition();
            bool hov = CheckCollisionPointRec(mouse, btn);

            DrawRectangleRounded(btn, 0.35f, 8,
                hov ? Color{50, 160, 80, 235} : Color{25, 90, 45, 210});
            DrawRectangleRoundedLines(btn, 0.35f, 8,
                hov ? Color{120, 255, 150, 255} : Color{60, 180, 90, 180});
            DrawText(btnTxt,
                (int)(btnX + btnW / 2.f - MeasureText(btnTxt, btnTxtSz) / 2.f),
                (int)(btnY + btnH / 2.f - btnTxtSz / 2.f),
                btnTxtSz, hov ? Color{210, 255, 220, 255} : RAYWHITE);

            if (hov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
            {
                _roomClearPending = false;
                if (_currentMapNodeIdx >= 0 && _currentMapNodeIdx < (int)_actMap.size())
                {
                    _actMap[_currentMapNodeIdx].completed = true;
                    for (int next : _actMap[_currentMapNodeIdx].nextNodes)
                        if (next >= 0 && next < (int)_actMap.size())
                            _actMap[next].available = true;
                }
                _mapKeySelectedIdx = -1;
                _mapOpenTimer = 0.4f;
                _gameState = GameState::Map;
            }
        }

        if (_fadeInTimer > 0.f)
        {
            float alpha = (_fadeInDuration > 0.f) ? (_fadeInTimer / _fadeInDuration) : 0.f;
            DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Fade(BLACK, alpha));
        }
        if (_biomeTransitionActive)
            DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Fade(BLACK, GetBiomeTransitionAlpha()));
        break;
    }

    case GameState::Pause:
    {
        Vector2 shakenMapPos = Vector2Add(_mapPos, _shakeOffset);
        Vector2 cameraRef    = Vector2Subtract(_cameraPos, _shakeOffset);
        Vector2 worldOffset  = { -_cameraPos.x + _shakeOffset.x, -_cameraPos.y + _shakeOffset.y };

        DrawTextureEx(_map, shakenMapPos, 0.0f, _mapScale, WHITE);

        for (auto& prop : _props)
            prop.Render(cameraRef);

        for (auto& pickup : _pickups)
            pickup->Draw(worldOffset);

        for (const auto& projectile : _spreadProjectiles)
            projectile.Draw(worldOffset);

        for (const auto& projectile : _lavaBalls)
            projectile.Draw(worldOffset);

        for (const auto& laser : _cyclopsLasers)
            laser.Draw(worldOffset);

        DrawEffects(worldOffset);

        for (auto& enemy : _enemies)
        {
            if (!enemy->IsActive())
                continue;
            enemy->DrawEnemy(cameraRef);
        }

        _player.DrawPlayer(cameraRef);

        int pauseResult = _pauseUI.DrawPause();
        if (pauseResult != 0) { StopSound(_buttonPressSound); PlaySound(_buttonPressSound); }
        if (pauseResult == 1)
            _gameState = GameState::Play;
        else if (pauseResult == 2)
        {
            _howToPlayFrom = GameState::Pause;
            _gameState = GameState::HowToPlay;
        }
        else if (pauseResult == 3)
            _shouldClose = true;
        else if (pauseResult == 4)
        {
            _keybindingsEdit = _player.GetBindings();
            _gameState = GameState::Keybindings;
        }

        break;
    }

    case GameState::GameOver:
    {
        if (_awaitingNameEntry)
        {
            std::string confirmed = _pauseUI.DrawNameEntry(_wave, _enemiesKilled);  // _wave = rooms entered
            if (!confirmed.empty())
            {
                _leaderboard.AddEntry(_wave, _enemiesKilled, confirmed);
                _leaderboard.Save("leaderboard.txt");
                _awaitingNameEntry = false;
            }
        }
        else
        {
            int goResult = _pauseUI.DrawGameOver(_wave, _enemiesKilled, _leaderboard.GetEntries());
            if (goResult != 0) { StopSound(_buttonPressSound); PlaySound(_buttonPressSound); }
            if (goResult == 1) { ResetRunState(); _fadeInTimer = 2.0f; _fadeInDuration = 2.0f; GenerateStartingAbilityOptions(); _awaitingStartingAbility = true; _levelUpReturnState = GameState::Map; _levelUpOpenTimer = 0.8f; _gameState = GameState::LevelUpChoice; }
            else if (goResult == 2) { ResetRunState(); _menu.Init(); _gameState = GameState::Menu; }
            else if (goResult == 3) _shouldClose = true;
        }
        break;
    }

    case GameState::HowToPlay:
    {
        DrawHowToPlay();
        break;
    }

    case GameState::Keybindings:
    {
        if (_pauseUI.DrawKeybindings(_keybindingsEdit))
        {
            _player.SetBindings(_keybindingsEdit);
            SaveKeybindings();
            _gameState = GameState::Pause;
        }
        break;
    }

    case GameState::Leaderboard:
    {
        if (_pauseUI.DrawLeaderboardScreen(_leaderboard.GetEntries()))
        {
            _menu.Init();
            _gameState = GameState::Menu;
        }
        break;
    }

    case GameState::LevelUpChoice:
    {
        DrawWorld();
        DrawHUD();
        DrawLevelUpChoice();
        break;
    }

    case GameState::AbilityChoice:
    {
        DrawWorld();
        DrawHUD();
        DrawAbilityChoice();
        break;
    }

    case GameState::Map:
    {
        DrawMap();
        break;
    }

    }
}

void Engine::HandleCollisions()
{
    float mapW = _map.width  * _mapScale;
    float mapH = _map.height * _mapScale;

    // Per-side player boundaries
    const float marginLeft   = 76.f;
    const float marginRight  = 96.f;
    const float marginTop    = 42.f;   // let the player go a little higher
    const float marginBottom = (_currentBiome == Biome::Forest) ? 220.f : 320.f;

    Vector2 pos = _player.GetWorldPos();
    if (pos.x < marginLeft  || pos.x > mapW - marginRight
     || pos.y < marginTop   || pos.y > mapH - marginBottom)
    {
        if (_player.IsBeingForcedPushed())
            _player.OnForcedPushCollision();

        // Always clamp — stops forced pushes and normal movement at the boundary
        pos.x = std::max(marginLeft,        std::min(pos.x, mapW - marginRight));
        pos.y = std::max(marginTop,         std::min(pos.y, mapH - marginBottom));
        _player.SetWorldPos(pos);
    }

    for (auto& prop : _props)
    {
        if (CheckCollisionRecs(prop.GetCollisionRec(), _player.GetCollisionRec()))
        {
            if (_player.IsBeingForcedPushed())
                _player.OnForcedPushCollision();

            // Always eject the player fully outside the prop via MTV
            Rectangle pr    = _player.GetCollisionRec();
            Rectangle propr = prop.GetCollisionRec();
            float overlapX = std::min(pr.x + pr.width,  propr.x + propr.width)  - std::max(pr.x, propr.x);
            float overlapY = std::min(pr.y + pr.height, propr.y + propr.height) - std::max(pr.y, propr.y);
            Vector2 ppos = _player.GetWorldPos();
            if (overlapX < overlapY)
            {
                float dir = (pr.x + pr.width * 0.5f < propr.x + propr.width * 0.5f) ? -1.f : 1.f;
                _player.SetWorldPos({ ppos.x + dir * overlapX, ppos.y });
            }
            else
            {
                float dir = (pr.y + pr.height * 0.5f < propr.y + propr.height * 0.5f) ? -1.f : 1.f;
                _player.SetWorldPos({ ppos.x, ppos.y + dir * overlapY });
            }
        }

        for (auto& enemy : _enemies)
        {
            if (!enemy->IsActive())
                continue;
            if (enemy->IgnoresPropCollisions())
                continue;
            if (CheckCollisionCircleRec(prop.GetEnemyCollisionCenter(), prop.GetEnemyCollisionRadius(), enemy->GetCollisionRec()))
            {
                if (enemy->IsBeingForcedPushed())
                {
                    enemy->OnForcedPushCollision();
                    continue;
                }

                if (Ogre* ogre = enemy->AsOgre())
                {
                    if (ogre->IsRushing())
                        ogre->OnRushBlocked();
                }
                else if (Molarbeast* molarbeast = enemy->AsMolarbeast())
                {
                    if (molarbeast->IsDashing())
                        molarbeast->OnDashBlocked();
                }

                // Always eject the enemy fully outside the prop via MTV
                Rectangle er    = enemy->GetCollisionRec();
                Rectangle propr = prop.GetCollisionRec();
                if (CheckCollisionRecs(er, propr))
                {
                    float overlapX = std::min(er.x + er.width,  propr.x + propr.width)  - std::max(er.x, propr.x);
                    float overlapY = std::min(er.y + er.height, propr.y + propr.height) - std::max(er.y, propr.y);
                    Vector2 epos = enemy->GetWorldPos();
                    if (overlapX < overlapY)
                    {
                        float dir = (er.x + er.width * 0.5f < propr.x + propr.width * 0.5f) ? -1.f : 1.f;
                        enemy->Teleport({ epos.x + dir * overlapX, epos.y });
                    }
                    else
                    {
                        float dir = (er.y + er.height * 0.5f < propr.y + propr.height * 0.5f) ? -1.f : 1.f;
                        enemy->Teleport({ epos.x, epos.y + dir * overlapY });
                    }
                }
                else
                {
                    enemy->UndoMovement();
                }
            }
        }

    }

      // Enemies should obey the same arena rectangle as the player. Using the
      // shared per-side margins keeps all actors inside the intended playable
      // space instead of letting special enemies, like the ogre, rush beyond
      // the lower boundary.
      for (auto& enemy : _enemies)
      {
          if (!enemy->IsActive())
              continue;
          if (enemy->IsDying()) continue;
          Vector2 pos = enemy->GetWorldPos();
          if (pos.x < marginLeft  || pos.x > mapW - marginRight ||
              pos.y < marginTop   || pos.y > mapH - marginBottom)
          {
              if (enemy->IsBeingForcedPushed())
                  enemy->OnForcedPushCollision();
              else if (Ogre* ogre = enemy->AsOgre())
              {
                  if (ogre->IsRushing())
                      ogre->OnRushBlocked();
                  else
                      enemy->UndoMovement();
              }
              else if (Molarbeast* molarbeast = enemy->AsMolarbeast())
              {
                  if (molarbeast->IsDashing())
                      molarbeast->OnDashBlocked();
                  else
                      enemy->UndoMovement();
              }
              else
              {
                  enemy->UndoMovement();
              }
        }
    }


    // Player vs enemy solid collision — dash passes straight through.
    // After the dash ends, eject the player if they landed inside an enemy.
    if (!_player.IsDashing())
    {
        for (auto& enemy : _enemies)
        {
            if (!enemy->IsActive() || !enemy->IsAlive())
                continue;

            Rectangle pr = _player.GetCollisionRec();
            Rectangle er = enemy->GetCollisionRec();
            if (!CheckCollisionRecs(pr, er))
                continue;

            // If the player is being force-pushed by an Ogre charge, hitting an
            // enemy should stop the push (same behaviour as hitting a prop/wall).
            // Using UndoMovement here causes the player to jitter inside the enemy
            // because the forced-push velocity overrides the revert every frame.
            // While the player is being force-pushed they slide through enemies;
            // only walls and props stop the push.
            if (_player.IsBeingForcedPushed())
                continue;

            // Normal case: revert the movement that caused the overlap
            _player.UndoMovement();

            // If still overlapping (e.g. stuck inside after a dash), eject via MTV
            pr = _player.GetCollisionRec();
            if (CheckCollisionRecs(pr, er))
            {
                float overlapX = std::min(pr.x + pr.width,  er.x + er.width)  - std::max(pr.x, er.x);
                float overlapY = std::min(pr.y + pr.height, er.y + er.height) - std::max(pr.y, er.y);
                Vector2 pos    = _player.GetWorldPos();
                if (overlapX < overlapY)
                {
                    float dir = (pr.x + pr.width * 0.5f < er.x + er.width * 0.5f) ? -1.f : 1.f;
                    _player.SetWorldPos({ pos.x + dir * overlapX, pos.y });
                }
                else
                {
                    float dir = (pr.y + pr.height * 0.5f < er.y + er.height * 0.5f) ? -1.f : 1.f;
                    _player.SetWorldPos({ pos.x, pos.y + dir * overlapY });
                }
            }
        }
    }
}

void Engine::UpdateEnemyCount(float dt)
{
    for (auto& enemy : _enemies)
    {
        if (!enemy->IsActive())
            continue;

        Vector2 dropPos = enemy->GetWorldPos();
        if (enemy->UpdateDeath(dt))
        {
            // Boss support adds respawn on a timer while the Molarbeast is
            // alive. Track their death moment before deactivating the pooled
            // object so the engine can bring back the same pressure slot later.
            if (enemy.get() == _bossCyclopsSupport.enemy && IsBossFightActive())
                _bossCyclopsSupport.respawnTimer = kBossSupportRespawnDelay;
            if (enemy.get() == _bossOgreSupport.enemy && IsBossFightActive())
                _bossOgreSupport.respawnTimer = kBossSupportRespawnDelay;

            // Exp rules:
            //  Boss kill    → flat bonus of 10 × rooms-entered (grows each act)
            //  Support add  → no exp while boss is alive (block easy farming)
            //  Normal enemy → standard per-enemy exp
            bool isBoss = (dynamic_cast<Molarbeast*>(enemy.get()) != nullptr);
            int prevLevel = _player.GetLevel();

            if (isBoss)
                _player.AddExp(10 * _wave);  // _wave = total rooms entered
            else if (!IsBossFightActive())
                _player.AddExp(enemy->GetExpValue());
            // else: support add during active boss fight — no exp

            if (_player.GetLevel() > prevLevel)
            {
                GenerateLevelUpOptions();
                _levelUpReturnState = GameState::Play;
                _levelUpOpenTimer = 0.25f;
                _gameState = GameState::LevelUpChoice;
            }
            _enemiesKilled++;
            SpawnEnemyDrop(dropPos);
            enemy->SetActive(false);
            enemy->Teleport(Vector2{ -5000.f, -5000.f });
        }
    }
}

void Engine::TriggerScreenShake(float strength, float duration)
{
    _shakeStrength = strength;
    _shakeTimer = duration;
}

void Engine::DrawWorld()
{
    // Map: top-left is _mapPos + shake
    Vector2 shakenMapPos = Vector2Add(_mapPos, _shakeOffset);

    // Props / enemies / player: worldPos - cameraRef + screenCenter
    Vector2 cameraRef = Vector2Subtract(_cameraPos, _shakeOffset);

    // Pickups / projectiles / effects: worldPos + worldOffset + screenCenter
    Vector2 worldOffset = { -_cameraPos.x + _shakeOffset.x, -_cameraPos.y + _shakeOffset.y };

    DrawTextureEx(_map, shakenMapPos, 0.0f, _mapScale, WHITE);

    for (auto& pickup : _pickups)
        pickup->Draw(worldOffset);

    for (const auto& projectile : _spreadProjectiles)
        projectile.Draw(worldOffset);

    DrawUltimateBlasts(worldOffset);

    for (const auto& projectile : _lavaBalls)
        projectile.Draw(worldOffset);

    for (const auto& laser : _cyclopsLasers)
        laser.Draw(worldOffset);

    DrawEffects(worldOffset);

    for (auto& enemy : _enemies)
    {
        if (!enemy->IsActive())
            continue;
        enemy->DrawEnemy(cameraRef);
    }

    _player.DrawPlayer(cameraRef);

    // Props drawn last so trees/rocks render in front of characters (depth illusion)
    for (auto& prop : _props)
        prop.Render(cameraRef);

    // Floating damage numbers — cull expired then draw remaining
    {
        float now = (float)GetTime();
        _floatingTexts.erase(
            std::remove_if(_floatingTexts.begin(), _floatingTexts.end(),
                [now](const FloatingText& ft)
                { return now - ft.spawnTime >= FloatingText::kLifetime; }),
            _floatingTexts.end());

        const float sw2 = GetScreenWidth()  / 2.f;
        const float sh2 = GetScreenHeight() / 2.f;
        for (const auto& ft : _floatingTexts)
        {
            float t     = (now - ft.spawnTime) / FloatingText::kLifetime;
            float yOff  = -55.f * t;
            float screenX = ft.worldPos.x + worldOffset.x + sw2;
            float screenY = ft.worldPos.y + worldOffset.y + sh2 + yOff;
            const char* txt = TextFormat("%d", ft.value);
            int   tw    = MeasureText(txt, 22);
            float alpha = 1.f - t;
            DrawText(txt, (int)(screenX - tw / 2.f), (int)screenY, 22, Fade(ft.color, alpha));
        }
    }
}

void Engine::DrawHUD()
{
    {
    static constexpr float kNewBarW   = 400.f;
    static constexpr float kNewBarH   = 28.f;
    static constexpr float kNewBarGap = 8.f;
    static constexpr float kNewBotPad = 12.f;
    static constexpr float kNewTopPad = 18.f;

    const float hpBarY   = kNewTopPad;
    const float manaBarY = hpBarY + kNewBarH + kNewBarGap;
    const float slotY    = (float)GetScreenHeight() - kNewBotPad - 80.f;
    const float expBarY  = slotY - 14.f - kNewBarH;
    const float barX     = (float)GetScreenWidth() / 2.f - kNewBarW / 2.f;

    auto drawLabelBox = [&](const char* text, float x, float y, int fontSize, Color textColor)
    {
        int textW = MeasureText(text, fontSize);
        const float padX = 12.f;
        const float padY = 8.f;
        DrawRectangleRounded(
            Rectangle{ x - padX, y - padY, textW + padX * 2.f, fontSize + padY * 2.f },
            0.18f, 6, Fade(BLACK, 0.55f));
        DrawText(text, (int)x, (int)y, fontSize, textColor);
    };

    drawLabelBox(("Enemies Defeated: " + std::to_string(_enemiesKilled)).c_str(), 20.f, 16.f, 28, RAYWHITE);
    drawLabelBox(("Enemies Left: " + std::to_string(GetActiveEnemyCount())).c_str(), 20.f, 58.f, 28, RAYWHITE);

    {
        bool isBoss = (_currentRoomType == RoomType::Boss);
        bool isElite = (_currentRoomType == RoomType::Elite);
        const char* roomTypeSuffix =
            isBoss    ? "  BOSS" :
            isElite   ? "  ELITE" :
            (_currentRoomType == RoomType::Rest)     ? "  REST" :
            (_currentRoomType == RoomType::Treasure) ? "  TREASURE" :
            (_currentRoomType == RoomType::Store)    ? "  SHOP" : "";
        const char* roomLabel = TextFormat("Act %d  Room %d%s",
            _currentAct, _currentRoom, roomTypeSuffix);
        int roomLabelW = MeasureText(roomLabel, 32);
        Color labelColor = isBoss ? ORANGE : (isElite ? Color{255,140,0,255} : RAYWHITE);
        drawLabelBox(roomLabel, (float)(GetScreenWidth() - roomLabelW - 130), 18.f, 32, labelColor);
    }

    {
        float maxHp = _player.GetMaxHealthValue();
        float curHp = _player.GetHealthValue();
        float hpPct = (maxHp > 0.f) ? (curHp / maxHp) : 0.f;
        Color fillColor = (hpPct > 0.5f) ? GREEN : (hpPct > 0.25f) ? YELLOW : RED;

        DrawRectangleRounded({ barX, hpBarY, kNewBarW * hpPct, kNewBarH }, 0.3f, 6, fillColor);
        DrawRectangleRoundedLines({ barX, hpBarY, kNewBarW, kNewBarH }, 0.3f, 6, Fade(WHITE, 0.25f));

        const char* hpLabel = TextFormat("HP  %.0f / %.0f", curHp, maxHp);
        int labelW = MeasureText(hpLabel, 18);
        DrawText(hpLabel,
            (int)(barX + kNewBarW / 2.f - labelW / 2.f),
            (int)(hpBarY + kNewBarH / 2.f - 9.f),
            18, RAYWHITE);
    }

    {
        int curMana = _player.GetMana();
        int maxMana = _player.GetMaxMana();
        float manaPct = (maxMana > 0) ? (float)curMana / (float)maxMana : 0.f;
        static const Color kManaFill = { 60, 120, 255, 230 };

        DrawRectangleRounded({ barX, manaBarY, kNewBarW * manaPct, kNewBarH }, 0.3f, 6, kManaFill);
        DrawRectangleRoundedLines({ barX, manaBarY, kNewBarW, kNewBarH }, 0.3f, 6, Fade(WHITE, 0.25f));

        const char* manaLabel = TextFormat("MP  %d / %d", curMana, maxMana);
        int manaLabelW = MeasureText(manaLabel, 18);
        DrawText(manaLabel,
            (int)(barX + kNewBarW / 2.f - manaLabelW / 2.f),
            (int)(manaBarY + kNewBarH / 2.f - 9.f),
            18, RAYWHITE);
    }

    {
        int level = _player.GetLevel();
        int exp = _player.GetExp();
        int expToNext = _player.GetExpToNext();
        int maxLevel = _player.GetMaxLevel();
        float expPct = (level < maxLevel && expToNext > 0) ? (float)exp / (float)expToNext : 1.f;
        static const Color kExpFill = { 255, 210, 0, 230 };

        DrawRectangleRounded({ barX, expBarY, kNewBarW, kNewBarH }, 0.3f, 6, Fade(BLACK, 0.75f));
        if (level < maxLevel)
            DrawRectangleRounded({ barX, expBarY, kNewBarW * expPct, kNewBarH }, 0.3f, 6, kExpFill);
        DrawRectangleRoundedLines({ barX, expBarY, kNewBarW, kNewBarH }, 0.3f, 6, Fade(WHITE, 0.25f));

        const char* levelText = (level < maxLevel)
            ? TextFormat("Lv.%d  %d/%d EXP", level, exp, expToNext)
            : "Lv.MAX";
        int textW = MeasureText(levelText, 18);
        DrawText(levelText,
            (int)(barX + kNewBarW / 2.f - textW / 2.f),
            (int)(expBarY + kNewBarH / 2.f - 9.f),
            18, kExpFill);
    }

    if (_touchModeActive)
    {
        DrawTouchAbilityArc();
        _touch.Draw(_windowWidth, _windowHeight);

        // Pause button — top-right corner, above the wave label
        {
            static constexpr float kPauseW = 90.f;
            static constexpr float kPauseH = 48.f;
            static constexpr float kPausePad = 14.f;
            Rectangle pauseRec{ (float)_windowWidth - kPauseW - kPausePad, kPausePad, kPauseW, kPauseH };
            DrawRectangleRounded(pauseRec, 0.22f, 6, Fade(BLACK, 0.55f));
            DrawRectangleRoundedLines(pauseRec, 0.22f, 6, Fade(WHITE, 0.40f));
            int pw = MeasureText("II", 26);
            DrawText("II",
                (int)(pauseRec.x + pauseRec.width / 2.f - pw / 2.f),
                (int)(pauseRec.y + pauseRec.height / 2.f - 13.f),
                26, RAYWHITE);
        }
    }
    else
    {
        DrawAbilityBar();
    }
    DrawMiniMap();

    if (_bossWarningTimer > 0.f)
    {
        const char* warning = "DON'T GET TOO CLOSE";
        int warningSize = 34;
        int warningWidth = MeasureText(warning, warningSize);
        drawLabelBox(warning, (float)(GetScreenWidth() / 2 - warningWidth / 2), 96.f, warningSize, ORANGE);
    }

    }
    return;

    // ── Shared bottom-bar layout constants (mirrored in DrawAbilityBar) ───────
    static constexpr float kBarW   = 400.f;
    static constexpr float kBarH   = 28.f;
    static constexpr float kBarGap = 8.f;
    static constexpr float kBotPad = 12.f;

    const float expBarY  = (float)GetScreenHeight() - kBotPad - kBarH;
    const float manaBarY = expBarY - kBarGap - kBarH;
    const float hpBarY   = manaBarY - kBarGap - kBarH;
    const float barX     = (float)GetScreenWidth() / 2.f - kBarW / 2.f;

    // ── Top banner ────────────────────────────────────────────────────────────
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight() / 8, Fade(BLACK, 0.6f));

    DrawText(TextFormat("Time: %.1f", _gameTimer), 85 + GetScreenWidth() / 2 - 150, 60, 30, RAYWHITE);
    DrawText(("Enemies Defeated: " + std::to_string(_enemiesKilled)).c_str(), 20, 10, 30, RAYWHITE);
    DrawText(("Enemies Left: " + std::to_string(GetActiveEnemyCount())).c_str(), 20, 60, 30, RAYWHITE);

    // ── Wave display — top right ──────────────────────────────────────────────
    {
        bool isBoss = (_wave > 0 && _wave % 5 == 0);
        const char* waveLabel = isBoss
            ? TextFormat("Wave %d  - BOSS", _wave)
            : TextFormat("Wave %d", _wave);
        int waveLabelW = MeasureText(waveLabel, 32);
        DrawText(waveLabel, GetScreenWidth() - waveLabelW - 20, 20, 32,
            isBoss ? ORANGE : RAYWHITE);
    }

    // ── HP bar (bottom, above EXP) ────────────────────────────────────────────
    {
        float maxHp = _player.GetMaxHealthValue();
        float curHp = _player.GetHealthValue();
        float hpPct = (maxHp > 0.f) ? (curHp / maxHp) : 0.f;

        Color fillColor = (hpPct > 0.5f) ? GREEN :
                          (hpPct > 0.25f) ? YELLOW : RED;

        DrawRectangleRounded({ barX, hpBarY, kBarW, kBarH }, 0.3f, 6, Fade(BLACK, 0.75f));
        DrawRectangleRounded({ barX, hpBarY, kBarW * hpPct, kBarH }, 0.3f, 6, fillColor);
        DrawRectangleRoundedLines({ barX, hpBarY, kBarW, kBarH }, 0.3f, 6, Fade(WHITE, 0.25f));

        const char* hpLabel = TextFormat("HP  %.0f / %.0f", curHp, maxHp);
        int labelW = MeasureText(hpLabel, 18);
        DrawText(hpLabel,
            (int)(barX + kBarW / 2.f - labelW / 2.f),
            (int)(hpBarY + kBarH / 2.f - 9.f),
            18, RAYWHITE);
    }

    // ── Mana bar (middle) ─────────────────────────────────────────────────────
    {
        int   curMana  = _player.GetMana();
        int   maxMana  = _player.GetMaxMana();
        float manaPct  = (maxMana > 0) ? (float)curMana / (float)maxMana : 0.f;

        static const Color kManaFill = { 60, 120, 255, 230 };

        DrawRectangleRounded({ barX, manaBarY, kBarW, kBarH }, 0.3f, 6, Fade(BLACK, 0.75f));
        DrawRectangleRounded({ barX, manaBarY, kBarW * manaPct, kBarH }, 0.3f, 6, kManaFill);
        DrawRectangleRoundedLines({ barX, manaBarY, kBarW, kBarH }, 0.3f, 6, Fade(WHITE, 0.25f));

        const char* manaLabel = TextFormat("MP  %d / %d", curMana, maxMana);
        int manaLabelW = MeasureText(manaLabel, 18);
        DrawText(manaLabel,
            (int)(barX + kBarW / 2.f - manaLabelW / 2.f),
            (int)(manaBarY + kBarH / 2.f - 9.f),
            18, RAYWHITE);
    }

    // ── EXP bar (very bottom) ─────────────────────────────────────────────────
    {
        int   level    = _player.GetLevel();
        int   exp      = _player.GetExp();
        int   expToNext= _player.GetExpToNext();
        int   maxLevel = _player.GetMaxLevel();
        float expPct   = (level < maxLevel && expToNext > 0) ? (float)exp / (float)expToNext : 1.f;

        static const Color kExpFill = { 255, 210, 0, 230 };

        DrawRectangleRounded({ barX, expBarY, kBarW, kBarH }, 0.3f, 6, Fade(BLACK, 0.75f));
        if (level < maxLevel)
            DrawRectangleRounded({ barX, expBarY, kBarW * expPct, kBarH }, 0.3f, 6, kExpFill);
        DrawRectangleRoundedLines({ barX, expBarY, kBarW, kBarH }, 0.3f, 6, Fade(WHITE, 0.25f));

        const char* levelText = (level < maxLevel)
            ? TextFormat("Lv.%d  %d/%d EXP", level, exp, expToNext)
            : "Lv.MAX";
        int textW = MeasureText(levelText, 18);
        DrawText(levelText,
            (int)(barX + kBarW / 2.f - textW / 2.f),
            (int)(expBarY + kBarH / 2.f - 9.f),
            18, kExpFill);
    }

    DrawAbilityBar();
    DrawMiniMap();

    if (_bossWarningTimer > 0.f)
    {
        const char* warning = "DON'T GET TOO CLOSE";
        int warningSize = 34;
        int warningWidth = MeasureText(warning, warningSize);
        DrawText(warning,
            GetScreenWidth() / 2 - warningWidth / 2,
            96,
            warningSize,
            ORANGE);
    }
}

void Engine::DrawAbilityBar()
{
    {
    const int totalSlots = _player.GetMaxAbilitySlots();
    const float slotSize = 80.f;
    const float slotGap = 10.f;
    static constexpr float kNewBotPad = 12.f;
    const float slotY = (float)GetScreenHeight() - kNewBotPad - slotSize;
    const float totalW = totalSlots * slotSize + (totalSlots - 1) * slotGap;
    const float startX = GetScreenWidth() / 2.f - totalW / 2.f;

    Vector2 mouse = GetMousePosition();

    for (int i = 0; i < totalSlots; i++)
    {
        AbilityType ability = _player.GetLearnedAbility(i);
        bool isEmpty = (ability == AbilityType::None);
        bool canCast = !isEmpty && _player.GetMana() >= GetAbilityManaCost(ability);
        float x = startX + i * (slotSize + slotGap);
        Rectangle slot{ x, slotY, slotSize, slotSize };
        bool hovered = !isEmpty && CheckCollisionPointRec(mouse, slot);

        Color bgColor = isEmpty ? Fade(BLACK, 0.30f) : (hovered ? Fade(BLACK, 0.80f) : Fade(BLACK, 0.55f));
        Color borderColor = isEmpty ? Fade(WHITE, 0.12f) :
            hovered ? Fade(GOLD, 0.70f) :
            canCast ? Fade(LIGHTGRAY, 0.35f) : Fade(RED, 0.40f);
        DrawRectangleRounded(slot, 0.18f, 6, bgColor);
        DrawRectangleRoundedLines(slot, 0.18f, 6, borderColor);

        if (isEmpty)
        {
            DrawText(GetKeyName(_player.GetAbilityKey(i)),
                (int)(x + 6.f), (int)(slotY + 6.f), 14, Fade(WHITE, 0.25f));
            continue;
        }

        DrawText(GetKeyName(_player.GetAbilityKey(i)),
            (int)(x + 6.f), (int)(slotY + 6.f), 14, Fade(WHITE, 0.6f));

        if (hovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
            _player.TriggerAbilityCast(i);

        const Texture2D* iconTex = &_abilityIconFireTex;
        if (ability == AbilityType::IceSpread || ability == AbilityType::IceBolt || ability == AbilityType::IceUltimate)
            iconTex = &_abilityIconIceTex;
        else if (ability == AbilityType::ElectricSpread || ability == AbilityType::ElectricBolt || ability == AbilityType::ElectricUltimate)
            iconTex = &_abilityIconElectricTex;

        Color iconTint = canCast ? WHITE : Fade(WHITE, 0.35f);
        float maxIconSize = slotSize * 0.55f;
        float iconScale = std::min(maxIconSize / (float)iconTex->width, maxIconSize / (float)iconTex->height);
        float iw = iconTex->width * iconScale;
        float ih = iconTex->height * iconScale;
        float cx = x + slotSize * 0.5f;
        float cy = slotY + slotSize * 0.42f;
        DrawTextureEx(*iconTex, { cx - iw * 0.5f, cy - ih * 0.5f }, 0.f, iconScale, iconTint);

        const char* abilityName = GetAbilityName(ability);
        int nameW = MeasureText(abilityName, 12);
        DrawText(abilityName,
            (int)(x + slotSize / 2.f - nameW / 2.f),
            (int)(slotY + slotSize - 18.f),
            12, canCast ? RAYWHITE : Fade(GRAY, 0.6f));

        int abilityLv = _player.GetAbilityLevel(ability);
        if (abilityLv > 1)
        {
            const char* badge = TextFormat("Lv%d", abilityLv);
            int badgeW = MeasureText(badge, 12);
            Color badgeColor = (abilityLv >= 3) ? GOLD : Fade(SKYBLUE, 0.9f);
            DrawText(badge,
                (int)(x + slotSize - badgeW - 4.f),
                (int)(slotY + slotSize - 16.f),
                12, badgeColor);
        }
    }

    }
    return;

    const int   totalSlots = _player.GetMaxAbilitySlots();  // always 4
    const float slotSize   = 80.f;
    const float slotGap    = 10.f;

    // Position just above the HP bar — mirrors DrawHUD() bottom-bar layout.
    static constexpr float kBarH   = 28.f;
    static constexpr float kBarGap = 8.f;
    static constexpr float kBotPad = 12.f;
    const float expBarY  = (float)GetScreenHeight() - kBotPad - kBarH;
    const float manaBarY = expBarY  - kBarGap - kBarH;
    const float hpBarY   = manaBarY - kBarGap - kBarH;
    const float slotY    = hpBarY   - 10.f - slotSize;
    const float totalW = totalSlots * slotSize + (totalSlots - 1) * slotGap;
    const float startX = GetScreenWidth() / 2.f - totalW / 2.f;

    Vector2 mouse = GetMousePosition();

    for (int i = 0; i < totalSlots; i++)
    {
        AbilityType ability = _player.GetLearnedAbility(i);
        bool        isEmpty  = (ability == AbilityType::None);
        bool        canCast  = !isEmpty && _player.GetMana() >= GetAbilityManaCost(ability);
        float       x        = startX + i * (slotSize + slotGap);
        Rectangle   slot     { x, slotY, slotSize, slotSize };
        bool        hovered  = !isEmpty && CheckCollisionPointRec(mouse, slot);

        // Background + border
        Color bgColor     = isEmpty ? Fade(BLACK, 0.30f) : (hovered ? Fade(BLACK, 0.80f) : Fade(BLACK, 0.55f));
        Color borderColor = isEmpty ? Fade(WHITE,  0.12f) :
                            hovered ? Fade(GOLD,   0.70f) :
                            canCast ? Fade(LIGHTGRAY, 0.35f) : Fade(RED, 0.40f);
        DrawRectangleRounded(slot, 0.18f, 6, bgColor);
        DrawRectangleRoundedLines(slot, 0.18f, 6, borderColor);

        if (isEmpty)
        {
            // Show key label only so the player knows the slot exists
            DrawText(GetKeyName(_player.GetAbilityKey(i)),
                (int)(x + 6.f), (int)(slotY + 6.f), 14, Fade(WHITE, 0.25f));
            continue;
        }

        // Key label
        DrawText(GetKeyName(_player.GetAbilityKey(i)),
            (int)(x + 6.f), (int)(slotY + 6.f), 14, Fade(WHITE, 0.6f));

        // Click to cast
        if (hovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
            _player.TriggerAbilityCast(i);

        // Icon
        const Texture2D* iconTex = &_abilityIconFireTex;
        if      (ability == AbilityType::IceSpread      ||
                 ability == AbilityType::IceBolt        ||
                 ability == AbilityType::IceUltimate)      iconTex = &_abilityIconIceTex;
        else if (ability == AbilityType::ElectricSpread ||
                 ability == AbilityType::ElectricBolt   ||
                 ability == AbilityType::ElectricUltimate) iconTex = &_abilityIconElectricTex;

        Color iconTint    = canCast ? WHITE : Fade(WHITE, 0.35f);
        float maxIconSize = slotSize * 0.55f;
        float iconScale   = std::min(maxIconSize / (float)iconTex->width,
                                     maxIconSize / (float)iconTex->height);
        float iw = iconTex->width  * iconScale;
        float ih = iconTex->height * iconScale;
        float cx = x + slotSize * 0.5f;
        float cy = slotY + slotSize * 0.42f;
        DrawTextureEx(*iconTex, { cx - iw * 0.5f, cy - ih * 0.5f }, 0.f, iconScale, iconTint);

        // Ability name
        const char* abilityName = GetAbilityName(ability);
        int nameW = MeasureText(abilityName, 12);
        DrawText(abilityName,
            (int)(x + slotSize / 2.f - nameW / 2.f),
            (int)(slotY + slotSize - 18.f),
            12, canCast ? RAYWHITE : Fade(GRAY, 0.6f));

        // Level badge
        int abilityLv = _player.GetAbilityLevel(ability);
        if (abilityLv > 1)
        {
            const char* badge  = TextFormat("Lv%d", abilityLv);
            int         badgeW = MeasureText(badge, 12);
            Color       badgeColor = (abilityLv >= 3) ? GOLD : Fade(SKYBLUE, 0.9f);
            DrawText(badge,
                (int)(x + slotSize - badgeW - 4.f),
                (int)(slotY + slotSize - 16.f),
                12, badgeColor);
        }
    }
}

void Engine::DrawWaveIntro()
{
    if (!_waveStarting)
        return;

    DrawRectangle(0, GetScreenHeight() / 2 - 80, GetScreenWidth(), 160, Fade(BLACK, 0.7f));

    int sw = GetScreenWidth();
    int sh = GetScreenHeight();
    int fontSize = 60;
    int midY     = sh / 2;

    // ── Room type label (center, primary) ────────────────────────────────────
    const char* roomLabel = nullptr;
    Color       roomColor = YELLOW;

    switch (_currentRoomType)
    {
    case RoomType::Boss:
        roomLabel = "- Boss Room -";
        roomColor = RED;
        break;
    case RoomType::Elite:
        roomLabel = "Elite Room";
        roomColor = Color{ 255, 140, 0, 255 };  // orange
        break;
    case RoomType::Rest:
        roomLabel = "Rest";
        roomColor = Color{ 100, 230, 120, 255 }; // green
        break;
    case RoomType::Treasure:
        roomLabel = "Treasure Room";
        roomColor = GOLD;
        break;
    case RoomType::Store:
        roomLabel = "Shop";
        roomColor = Color{ 220, 200, 60, 255 };
        break;
    default:  // Standard
        roomLabel = "Standard";
        roomColor = YELLOW;
        break;
    }

    // Very first room of the run — just show the biome name as the intro.
    if (_wave == 1 && _currentAct == 1)
    {
        const char* biomeName = GetBiomeName(_currentBiome);
        int bw = MeasureText(biomeName, fontSize);
        DrawText(biomeName, sw / 2 - bw / 2, midY - 30, fontSize, GOLD);
        return;
    }

    // New act intro (room 1 of any act after act 1)
    bool isFirstRoomOfNewAct = (_currentRoom == 1 && _currentAct > 1);
    int roomLabelY = (isFirstRoomOfNewAct || _currentRoomType == RoomType::Boss) ? midY - 55 : midY - 30;

    // Act label line (shown on new acts and boss rooms)
    if (isFirstRoomOfNewAct)
    {
        std::string actStr = "Act " + std::to_string(_currentAct) + " — " + GetBiomeName(GetBiomeForAct(_currentAct));
        int aw = MeasureText(actStr.c_str(), 42);
        DrawText(actStr.c_str(), sw / 2 - aw / 2, roomLabelY - 52, 42, LIGHTGRAY);
    }

    // Room label
    int rw = MeasureText(roomLabel, fontSize);
    DrawText(roomLabel, sw / 2 - rw / 2, roomLabelY, fontSize, roomColor);

    if (_currentRoomType == RoomType::Boss)
    {
        const char* actLine = (_message).c_str();
        int aw = MeasureText(actLine, 36);
        DrawText(actLine, sw / 2 - aw / 2, roomLabelY + fontSize + 10, 36, LIGHTGRAY);
    }
}

void Engine::GenerateStartingAbilityOptions()
{
    // Pool of all 6 abilities (3 spread + 3 bolt). Shuffle and show 3 so the
    // player always sees a varied starting choice.
    UpgradeType pool[6] = {
        UpgradeType::LearnFireSpread,
        UpgradeType::LearnIceSpread,
        UpgradeType::LearnElectricSpread,
        UpgradeType::LearnFireBolt,
        UpgradeType::LearnIceBolt,
        UpgradeType::LearnElectricBolt,
    };
    for (int i = 0; i < 6; i++)
    {
        int j = GetRandomValue(i, 5);
        UpgradeType tmp = pool[i]; pool[i] = pool[j]; pool[j] = tmp;
    }
    _levelUpOptions[0] = pool[0];
    _levelUpOptions[1] = pool[1];
    _levelUpOptions[2] = pool[2];
}

void Engine::GenerateLevelUpOptions()
{
    // Separate pools by rarity — abilities are reserved for the 5th-wave screen.
    UpgradeType commonPool[6] = {
        UpgradeType::AttackPower, UpgradeType::AttackRange,
        UpgradeType::MaxHealth,   UpgradeType::MaxMana,
        UpgradeType::Defense,     UpgradeType::MoveSpeed
    };
    UpgradeType rarePool[6] = {
        UpgradeType::IronConstitution, UpgradeType::SwiftFeet, UpgradeType::Ferocity,
        UpgradeType::ArcaneMind,       UpgradeType::IronSkin,  UpgradeType::BladeEdge
    };
    UpgradeType epicPool[5] = {
        UpgradeType::WarGod,    UpgradeType::Resilience, UpgradeType::BladeStorm,
        UpgradeType::Juggernaut, UpgradeType::ArcaneColossus
    };

    // Rarity weights shift toward higher-quality cards as the player levels up.
    int level = _player.GetLevel();
    int commonW, rareW, epicW;
    if      (level <= 5)  { commonW = 70; rareW = 25; epicW =  5; }
    else if (level <= 10) { commonW = 40; rareW = 45; epicW = 15; }
    else if (level <= 15) { commonW = 20; rareW = 45; epicW = 35; }
    else                  { commonW = 10; rareW = 35; epicW = 55; }

    // Shuffle each pool so we don't always draw from the same end.
    auto shuffle3 = [](UpgradeType* pool, int size) {
        for (int i = 0; i < size; i++) {
            int j = GetRandomValue(i, size - 1);
            UpgradeType tmp = pool[i]; pool[i] = pool[j]; pool[j] = tmp;
        }
    };
    shuffle3(commonPool, 6);
    shuffle3(rarePool,   6);
    shuffle3(epicPool,   5);

    int cIdx = 0, rIdx = 0, eIdx = 0;
    for (int i = 0; i < 3; i++)
    {
        int total = commonW + rareW + epicW;
        int roll  = GetRandomValue(0, total - 1);

        UpgradeType picked = UpgradeType::AttackPower;
        if (roll < commonW && cIdx < 6)          picked = commonPool[cIdx++];
        else if (roll < commonW + rareW && rIdx < 6) picked = rarePool[rIdx++];
        else if (eIdx < 5)                         picked = epicPool[eIdx++];
        else if (cIdx < 6)                         picked = commonPool[cIdx++];
        else if (rIdx < 6)                         picked = rarePool[rIdx++];

        _levelUpOptions[i] = picked;
    }

    // Abilities are now handled exclusively by the 5th-wave ability screen.
    _showUltimateRow   = false;
    _ultimateRowPicked = false;
    _regularRowPicked  = false;
}

// =============================================================================
// 5th-wave ability choice screen
// =============================================================================

void Engine::GenerateAbilityChoiceOptions()
{
    _abilityChoiceOptionCount = 0;
    _abilityChoiceSwapPending = false;

    // All base ability types in the same order as AbilityType enum
    static const AbilityType allAbilities[9] = {
        AbilityType::FireSpread,     AbilityType::IceSpread,     AbilityType::ElectricSpread,
        AbilityType::FireBolt,       AbilityType::IceBolt,       AbilityType::ElectricBolt,
        AbilityType::FireUltimate,   AbilityType::IceUltimate,   AbilityType::ElectricUltimate
    };
    static const UpgradeType learnTypes[9] = {
        UpgradeType::LearnFireSpread,    UpgradeType::LearnIceSpread,    UpgradeType::LearnElectricSpread,
        UpgradeType::LearnFireBolt,      UpgradeType::LearnIceBolt,      UpgradeType::LearnElectricBolt,
        UpgradeType::LearnFireUltimate,  UpgradeType::LearnIceUltimate,  UpgradeType::LearnElectricUltimate
    };
    static const UpgradeType upgradeTypes[9] = {
        UpgradeType::UpgradeFireSpread,    UpgradeType::UpgradeIceSpread,    UpgradeType::UpgradeElectricSpread,
        UpgradeType::UpgradeFireBolt,      UpgradeType::UpgradeIceBolt,      UpgradeType::UpgradeElectricBolt,
        UpgradeType::UpgradeFireUltimate,  UpgradeType::UpgradeIceUltimate,  UpgradeType::UpgradeElectricUltimate
    };

    UpgradeType pool[18];
    int poolSize = 0;

    for (int i = 0; i < 9; i++)
    {
        if (!_player.HasLearnedAbility(allAbilities[i]))
            pool[poolSize++] = learnTypes[i];
        else if (_player.CanUpgradeAbility(allAbilities[i]))
            pool[poolSize++] = upgradeTypes[i];
    }

    // Fisher-Yates shuffle then pick up to 3
    for (int i = 0; i < poolSize; i++)
    {
        int j = GetRandomValue(i, poolSize - 1);
        UpgradeType tmp = pool[i]; pool[i] = pool[j]; pool[j] = tmp;
    }

    _abilityChoiceOptionCount = (poolSize < 3) ? poolSize : 3;
    for (int i = 0; i < _abilityChoiceOptionCount; i++)
        _abilityChoiceOptions[i] = pool[i];
}

void Engine::DrawAbilityChoice()
{
    const float sw = (float)GetScreenWidth();
    const float sh = (float)GetScreenHeight();

    DrawRectangle(0, 0, (int)sw, (int)sh, Fade(BLACK, 0.68f));

    bool ready = (_abilityChoiceOpenTimer <= 0.f);
    Vector2 mouse = GetMousePosition();

    // Helper: map Learn*/Upgrade* → the underlying AbilityType
    auto upgradeToAbility = [](UpgradeType ut) -> AbilityType
    {
        int base = (int)ut;
        if (base >= (int)UpgradeType::UpgradeFireSpread)
            return (AbilityType)(base - (int)UpgradeType::UpgradeFireSpread);
        if (base >= (int)UpgradeType::LearnFireSpread)
            return (AbilityType)(base - (int)UpgradeType::LearnFireSpread);
        return AbilityType::None;
    };

    auto elementColor = [](AbilityType ab) -> Color
    {
        if (ab == AbilityType::FireSpread  || ab == AbilityType::FireBolt  || ab == AbilityType::FireUltimate)
            return Color{255, 110,  20, 255};
        if (ab == AbilityType::IceSpread   || ab == AbilityType::IceBolt   || ab == AbilityType::IceUltimate)
            return Color{100, 210, 255, 255};
        return Color{255, 220,  30, 255};  // electric
    };

    // ── Swap sub-mode ─────────────────────────────────────────────────────────
    if (_abilityChoiceSwapPending)
    {
        const char* title = "Replace which ability?";
        int tSz = 42;
        DrawText(title, (int)(sw/2.f - MeasureText(title, tSz)/2.f), (int)(sh*0.06f), tSz, GOLD);

        const float cardW = 210.f, cardH = 260.f, cardGap = 28.f;
        int count = _player.GetLearnedCount();
        float totalW = count * (cardW + cardGap) - cardGap;
        float sx = sw/2.f - totalW/2.f;

        for (int i = 0; i < count; i++)
        {
            float cx = sx + i * (cardW + cardGap);
            Rectangle card{cx, sh/2.f - cardH/2.f, cardW, cardH};
            bool hov = ready && CheckCollisionPointRec(mouse, card);

            DrawRectangleRounded(card, 0.12f, 8, hov ? Color{80,20,20,235} : Color{40,15,15,210});
            DrawRectangleRoundedLines(card, 0.12f, 8, hov ? RED : Color{180,55,55,160});

            AbilityType ab = _player.GetLearnedAbility(i);
            Color ec = elementColor(ab);
            // Small element dot
            DrawCircleV({cx + cardW/2.f, card.y + cardH*0.30f}, 22.f, Fade(ec, 0.3f));
            DrawCircleV({cx + cardW/2.f, card.y + cardH*0.30f}, 14.f, Fade(ec, 0.7f));

            const char* abName = GetAbilityName(ab);
            int nSz = 20;
            DrawText(abName, (int)(cx + cardW/2.f - MeasureText(abName, nSz)/2.f),
                     (int)(card.y + cardH*0.56f), nSz, hov ? GOLD : RAYWHITE);

            int lv = _player.GetAbilityLevel(ab);
            char lvStr[16]; snprintf(lvStr, sizeof(lvStr), "Lv %d", lv);
            int lvSz = 17;
            DrawText(lvStr, (int)(cx + cardW/2.f - MeasureText(lvStr, lvSz)/2.f),
                     (int)(card.y + cardH*0.70f), lvSz, Color{255,200,80,255});

            if (ready && hov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
            {
                _player.RemoveAbilityAtSlot(i);
                _player.ApplyUpgrade(_abilityChoiceSwapTarget);
                _abilityChoiceSwapPending = false;
                if (_pendingRoomChoice)
                {
                    _pendingRoomChoice = false;
                    _currentAct++;
                    GenerateActMap();
                    _mapOpenTimer = 0.4f;
                    _gameState = GameState::Map;
                }
                else
                    _gameState = GameState::Play;
            }
        }

        // Cancel button
        Rectangle cancelBtn{sw/2.f - 90.f, sh*0.82f, 180.f, 44.f};
        bool cHov = ready && CheckCollisionPointRec(mouse, cancelBtn);
        DrawRectangleRounded(cancelBtn, 0.4f, 8, cHov ? Color{55,55,55,230} : Color{28,28,28,200});
        DrawRectangleRoundedLines(cancelBtn, 0.4f, 8, cHov ? WHITE : GRAY);
        const char* cTxt = "Cancel";
        int cSz = 22;
        DrawText(cTxt, (int)(cancelBtn.x + cancelBtn.width/2.f - MeasureText(cTxt,cSz)/2.f),
                 (int)(cancelBtn.y + cancelBtn.height/2.f - cSz/2.f), cSz, RAYWHITE);
        if (ready && cHov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
            _abilityChoiceSwapPending = false;
        return;
    }

    // ── Main ability choice ────────────────────────────────────────────────────
    const char* title = "Ability Upgrade!";
    int tSz = 46;
    DrawText(title, (int)(sw/2.f - MeasureText(title, tSz)/2.f), (int)(sh*0.06f), tSz, GOLD);

    const char* sub = "Choose an ability to learn or upgrade:";
    int subSz = 24;
    DrawText(sub, (int)(sw/2.f - MeasureText(sub, subSz)/2.f),
             (int)(sh*0.06f + tSz + 8.f), subSz, Color{200,200,200,200});

    const float cardW = 280.f, cardH = 360.f, cardGap = 40.f;
    int count = _abilityChoiceOptionCount;
    float totalW = count * (cardW + cardGap) - cardGap;
    float sx = sw/2.f - totalW/2.f;
    float cardY = sh/2.f - cardH/2.f - 10.f;

    for (int i = 0; i < count; i++)
    {
        float cx = sx + i * (cardW + cardGap);
        Rectangle card{cx, cardY, cardW, cardH};
        bool hov = ready && CheckCollisionPointRec(mouse, card);

        UpgradeType opt = _abilityChoiceOptions[i];
        bool isLearn   = ((int)opt >= (int)UpgradeType::LearnFireSpread &&
                          (int)opt <= (int)UpgradeType::LearnElectricUltimate);
        AbilityType ab = upgradeToAbility(opt);
        Color ec       = elementColor(ab);

        // Card background tinted by element
        Color bgN = Color{(uint8_t)(ec.r/7), (uint8_t)(ec.g/7), (uint8_t)(ec.b/7), 210};
        Color bgH = Color{(uint8_t)(ec.r/4), (uint8_t)(ec.g/4), (uint8_t)(ec.b/4), 235};
        DrawRectangleRounded(card, 0.12f, 8, hov ? bgH : bgN);
        DrawRectangleRoundedLines(card, 0.12f, 8, hov ? ec : Color{ec.r,ec.g,ec.b,120});

        // Tag: NEW / UPGRADE
        const char* tag = isLearn ? "NEW" : "UPGRADE";
        Color tagColor  = isLearn ? Color{100,255,120,255} : Color{255,200,50,255};
        int tagSz = 17;
        DrawText(tag, (int)(cx + cardW/2.f - MeasureText(tag, tagSz)/2.f),
                 (int)(cardY + 12.f), tagSz, tagColor);

        // Element icon circle
        float circleY = cardY + cardH * 0.30f;
        DrawCircleV({cx + cardW/2.f, circleY}, 42.f, Fade(ec, 0.20f));
        DrawCircleV({cx + cardW/2.f, circleY}, 28.f, Fade(ec, 0.55f));

        // Ability name
        const char* abName = GetAbilityName(ab);
        int nSz = 26;
        DrawText(abName, (int)(cx + cardW/2.f - MeasureText(abName, nSz)/2.f),
                 (int)(cardY + cardH*0.54f), nSz, hov ? GOLD : RAYWHITE);

        // Level info / description
        if (!isLearn)
        {
            int lv = _player.GetAbilityLevel(ab);
            char lvStr[32]; snprintf(lvStr, sizeof(lvStr), "Lv %d  \xE2\x86\x92  Lv %d", lv, lv + 1);
            int lvSz = 19;
            DrawText(lvStr, (int)(cx + cardW/2.f - MeasureText(lvStr, lvSz)/2.f),
                     (int)(cardY + cardH*0.66f), lvSz, Color{255,200,80,255});
            const char* upDesc = "+1 spell damage";
            int udSz = 17;
            DrawText(upDesc, (int)(cx + cardW/2.f - MeasureText(upDesc, udSz)/2.f),
                     (int)(cardY + cardH*0.76f), udSz, LIGHTGRAY);
        }
        else
        {
            // Split ability description across two lines
            const char* abDesc = GetAbilityDesc(ab);
            std::string ds = abDesc;
            int nl = (int)ds.find('\n');
            int dSz = 19;
            if (nl != (int)std::string::npos)
            {
                std::string l1 = ds.substr(0, nl), l2 = ds.substr(nl + 1);
                DrawText(l1.c_str(), (int)(cx + cardW/2.f - MeasureText(l1.c_str(),dSz)/2.f),
                         (int)(cardY + cardH*0.66f), dSz, LIGHTGRAY);
                DrawText(l2.c_str(), (int)(cx + cardW/2.f - MeasureText(l2.c_str(),dSz)/2.f),
                         (int)(cardY + cardH*0.66f + dSz + 4), dSz, LIGHTGRAY);
            }
            else
                DrawText(abDesc, (int)(cx + cardW/2.f - MeasureText(abDesc,dSz)/2.f),
                         (int)(cardY + cardH*0.66f), dSz, LIGHTGRAY);
        }

        if (ready && hov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        {
            if (isLearn && _player.GetLearnedCount() >= _player.GetMaxAbilitySlots())
            {
                // No free slot — enter swap mode
                _abilityChoiceSwapPending = true;
                _abilityChoiceSwapTarget  = opt;
            }
            else
            {
                _player.ApplyUpgrade(opt);
                _abilityChoiceSwapPending = false;
                if (_pendingRoomChoice)
                {
                    _pendingRoomChoice = false;
                    _currentAct++;
                    GenerateActMap();
                    _mapOpenTimer = 0.4f;
                    _gameState = GameState::Map;
                }
                else
                    _gameState = GameState::Play;
            }
        }
    }

    if (count == 0)
    {
        const char* msg = "All abilities are at max level!";
        int mSz = 28;
        DrawText(msg, (int)(sw/2.f - MeasureText(msg, mSz)/2.f), (int)(sh/2.f - mSz/2.f), mSz, LIGHTGRAY);
    }

    // Continue button
    Rectangle contBtn{sw/2.f - 110.f, sh*0.87f, 220.f, 52.f};
    bool contHov = ready && CheckCollisionPointRec(mouse, contBtn);
    DrawRectangleRounded(contBtn, 0.4f, 8, contHov ? Color{35,60,35,235} : Color{18,35,18,200});
    DrawRectangleRoundedLines(contBtn, 0.4f, 8, contHov ? Color{100,210,100,255} : Color{55,120,55,160});
    const char* contTxt = "Continue";
    int contSz = 24;
    DrawText(contTxt, (int)(contBtn.x + contBtn.width/2.f - MeasureText(contTxt, contSz)/2.f),
             (int)(contBtn.y + contBtn.height/2.f - contSz/2.f), contSz,
             contHov ? Color{160,255,160,255} : RAYWHITE);
    if (ready && contHov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
    {
        if (_pendingRoomChoice)
        {
            _pendingRoomChoice = false;
            _currentAct++;
            GenerateActMap();
            _mapOpenTimer = 0.4f;
            _gameState = GameState::Map;
        }
        else
            _gameState = GameState::Play;
    }
}

// ── DrawMap ───────────────────────────────────────────────────────────────────
// Full-screen Slay-the-Spire–style act map.  Shows the entire node graph for
// the current act; available nodes are highlighted and clickable.
void Engine::DrawMap()
{
    const float sw = (float)GetScreenWidth();
    const float sh = (float)GetScreenHeight();
    bool ready     = (_mapOpenTimer <= 0.f);
    Vector2 mouse  = GetMousePosition();

    // ── Background ────────────────────────────────────────────────────────
    DrawRectangle(0, 0, (int)sw, (int)sh, Color{10, 8, 20, 255});

    // ── Header ────────────────────────────────────────────────────────────
    std::string header = "Act " + std::to_string(_currentAct)
                       + "  —  " + GetBiomeName(GetBiomeForAct(_currentAct));
    int hSz = 42;
    DrawText(header.c_str(),
             (int)(sw/2.f - MeasureText(header.c_str(), hSz)/2.f), 28, hSz, GOLD);

    const char* sub = "Choose your path";
    int subSz = 22;
    DrawText(sub, (int)(sw/2.f - MeasureText(sub, subSz)/2.f), 78, subSz,
             Color{180, 180, 180, 200});

    if (_actMap.empty()) return;

    // ── Node colour / name helpers ────────────────────────────────────────
    auto nodeColor = [](RoomType rt) -> Color {
        switch (rt) {
        case RoomType::Boss:     return Color{220,  40,  40, 255};
        case RoomType::Elite:    return Color{230, 130,  20, 255};
        case RoomType::Rest:     return Color{ 60, 190,  90, 255};
        case RoomType::Treasure: return Color{220, 190,  30, 255};
        case RoomType::Store:    return Color{ 80, 190, 230, 255};
        default:                 return Color{ 80, 130, 220, 255};
        }
    };
    auto nodeName = [](RoomType rt) -> const char* {
        switch (rt) {
        case RoomType::Boss:     return "BOSS";
        case RoomType::Elite:    return "ELITE";
        case RoomType::Rest:     return "REST";
        case RoomType::Treasure: return "CHEST";
        case RoomType::Store:    return "SHOP";
        default:                 return "FIGHT";
        }
    };
    auto nodeDesc = [](RoomType rt) -> const char* {
        switch (rt) {
        case RoomType::Boss:     return "Act finale - ability upgrade on clear";
        case RoomType::Elite:    return "Harder encounter - better EXP reward";
        case RoomType::Rest:     return "No combat - heal pickups appear";
        case RoomType::Treasure: return "No combat - free upgrade card";
        case RoomType::Store:    return "No combat - shop (coming soon)";
        default:                 return "Mixed enemies - standard EXP reward";
        }
    };

    // ── Connection lines ──────────────────────────────────────────────────
    for (int i = 0; i < (int)_actMap.size(); i++)
    {
        const MapNode& n = _actMap[i];
        for (int nextIdx : n.nextNodes)
        {
            if (nextIdx < 0 || nextIdx >= (int)_actMap.size()) continue;
            const MapNode& m = _actMap[nextIdx];
            Color lc = n.completed ? Color{70, 70, 90, 180} : Color{110, 110, 140, 110};
            DrawLineEx(n.drawPos, m.drawPos, 2.5f, lc);
        }
    }

    // ── Nodes ─────────────────────────────────────────────────────────────
    const float nodeR = 28.f;
    int hoveredIdx = -1;

    for (int i = 0; i < (int)_actMap.size(); i++)
    {
        const MapNode& n = _actMap[i];
        Color ec = nodeColor(n.type);

        bool hov = ready && !n.completed &&
                   CheckCollisionPointCircle(mouse, n.drawPos, nodeR + 5.f);
        if (hov) hoveredIdx = i;

        if (n.completed)
        {
            DrawCircleV(n.drawPos, nodeR, Fade(ec, 0.22f));
            DrawCircleLines((int)n.drawPos.x, (int)n.drawPos.y, nodeR, Fade(ec, 0.40f));
            DrawCircleV(n.drawPos, 7.f, Fade(ec, 0.55f));   // dim centre dot
        }
        else if (n.available)
        {
            float pulse  = (float)GetTime();
            float glow   = 0.22f + 0.10f * sinf(pulse * 3.0f);
            DrawCircleV(n.drawPos, nodeR + 7.f, Fade(ec, glow));
            DrawCircleV(n.drawPos, nodeR, Fade(ec, hov ? 0.60f : 0.42f));
            DrawCircleLines((int)n.drawPos.x, (int)n.drawPos.y, nodeR,
                            hov ? ec : Color{ec.r, ec.g, ec.b, 200});
            if (hov)
                DrawCircleLines((int)n.drawPos.x, (int)n.drawPos.y, nodeR + 5.f,
                                Fade(ec, 0.45f));
        }
        else
        {
            // Future node — visible but greyed
            DrawCircleV(n.drawPos, nodeR, Fade(ec, 0.14f));
            DrawCircleLines((int)n.drawPos.x, (int)n.drawPos.y, nodeR, Fade(ec, 0.28f));
        }

        // Label inside the node
        const char* nm = nodeName(n.type);
        int nSz = 13;
        Color lblC = n.completed ? Fade(LIGHTGRAY, 0.45f) :
                     n.available ? (hov ? GOLD : RAYWHITE) :
                     Fade(LIGHTGRAY, 0.40f);
        DrawText(nm, (int)(n.drawPos.x - MeasureText(nm, nSz)/2.f),
                 (int)(n.drawPos.y - nSz/2.f), nSz, lblC);

        // Click
        if (ready && n.available && !n.completed &&
            CheckCollisionPointCircle(mouse, n.drawPos, nodeR) &&
            IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        {
            EnterMapRoom(i);
        }
    }

    // ── Hovered node description ──────────────────────────────────────────
    if (hoveredIdx >= 0)
    {
        const MapNode& hn = _actMap[hoveredIdx];
        const char* dsc = nodeDesc(hn.type);
        int dSz = 20;
        int dw  = MeasureText(dsc, dSz);
        DrawText(dsc, (int)(sw/2.f - dw/2.f), (int)(sh - 90.f), dSz, LIGHTGRAY);
    }

    // ── Keyboard-selected node highlight ─────────────────────────────────
    if (_mapKeySelectedIdx >= 0 && _mapKeySelectedIdx < (int)_actMap.size())
    {
        const MapNode& kn = _actMap[_mapKeySelectedIdx];
        if (kn.available && !kn.completed)
        {
            // White dashed-style double ring to distinguish from mouse hover
            DrawCircleLines((int)kn.drawPos.x, (int)kn.drawPos.y, nodeR + 7.f,  WHITE);
            DrawCircleLines((int)kn.drawPos.x, (int)kn.drawPos.y, nodeR + 11.f, Fade(WHITE, 0.35f));
        }
    }

    // ── Current node indicator ────────────────────────────────────────────
    if (_currentMapNodeIdx >= 0 && _currentMapNodeIdx < (int)_actMap.size())
    {
        const MapNode& cur = _actMap[_currentMapNodeIdx];
        if (cur.completed)
            DrawCircleLines((int)cur.drawPos.x, (int)cur.drawPos.y, nodeR + 9.f,
                            Fade(YELLOW, 0.55f));
    }

    // ── Footer hint ───────────────────────────────────────────────────────
    const char* hint = _touchModeActive
        ? "Tap a highlighted node to enter"
        : "Click node  or  A/D  ←/→  to select  •  Enter / Space  to confirm";
    int ftSz = 18;
    DrawText(hint, (int)(sw/2.f - MeasureText(hint, ftSz)/2.f),
             (int)(sh - 55.f), ftSz, Color{140, 140, 140, 170});
}

void Engine::DrawLevelUpChoice()
{
    const float sw = (float)GetScreenWidth();
    const float sh = (float)GetScreenHeight();

    // Dim overlay
    DrawRectangle(0, 0, (int)sw, (int)sh, Fade(BLACK, 0.65f));

    // Determine which rows are still visible
    bool showUlt = _showUltimateRow && !_ultimateRowPicked && !_awaitingStartingAbility;
    bool showReg = !_regularRowPicked;

    // Title
    const char* title;
    if (_awaitingStartingAbility)
        title = "Choose your starting ability:";
    else if (showUlt && showReg)
        title = "LEVEL UP!  Choose an Ultimate AND an Upgrade:";
    else if (showUlt)
        title = "Now choose your Ultimate:";
    else if (showReg)
        title = "Now choose an Upgrade:";
    else
        title = "LEVEL UP!";

    int titleSz = 44;
    int titleW  = MeasureText(title, titleSz);
    DrawText(title, (int)(sw / 2.f - titleW / 2.f), (int)(sh * 0.06f), titleSz, GOLD);

    // Card layout
    const float cardW   = 280.f;
    const float cardH   = 360.f;
    const float cardGap = 40.f;
    const float totalW  = 3.f * cardW + 2.f * cardGap;
    const float startX  = sw / 2.f - totalW / 2.f;

    // Row Y positions — stack both rows when both visible, otherwise center the lone row
    float ultRowY, regRowY;
    if (showUlt && showReg)
    {
        float totalH = cardH * 2.f + 50.f;
        float topY   = sh / 2.f - totalH / 2.f;
        ultRowY = topY;
        regRowY = topY + cardH + 50.f;
    }
    else
    {
        ultRowY = sh / 2.f - cardH / 2.f;
        regRowY = sh / 2.f - cardH / 2.f;
    }

    // Use cardY to keep the rest of the code below working for the regular row
    const float cardY = regRowY;

    // Map UpgradeType to display info
    auto getUpgradeInfo = [&](UpgradeType type, const char*& name, const char*& desc, Texture2D*& icon)
    {
        switch (type)
        {
        case UpgradeType::AttackPower:
            name = "Attack Power";
            desc = "+10% melee\ndamage";
            icon = &_upgradeAttackPowerTex;
            break;
        case UpgradeType::AttackRange:
            name = "Attack Range";
            desc = "+10% melee\nreach";
            icon = &_upgradeAttackRangeTex;
            break;
        case UpgradeType::MaxHealth:
            name = "Max Health";
            desc = "+15% max HP\n(heals too)";
            icon = &_upgradeHealthTex;
            break;
        case UpgradeType::MaxMana:
            name = "Max Mana";
            desc = "+15 max MP";
            icon = &_upgradeMagicTex;
            break;
        case UpgradeType::Defense:
            name = "Defense";
            desc = "+6% damage\nreduction";
            icon = &_upgradeDefenseTex;
            break;
        case UpgradeType::MoveSpeed:
            name = "Move Speed";
            desc = "+10% movement\nspeed";
            icon = &_upgradeMoveSpeedTex;
            break;
        // ── Rare ──────────────────────────────────────────────────────────────
        case UpgradeType::IronConstitution:
            name = "Iron Constitution";
            desc = "+25% max HP\n(heals too)";
            icon = &_upgradeHealthTex;
            break;
        case UpgradeType::SwiftFeet:
            name = "Swift Feet";
            desc = "+15% move\nspeed";
            icon = &_upgradeMoveSpeedTex;
            break;
        case UpgradeType::Ferocity:
            name = "Ferocity";
            desc = "+15% attack\npower";
            icon = &_upgradeAttackPowerTex;
            break;
        case UpgradeType::ArcaneMind:
            name = "Arcane Mind";
            desc = "+25 max mana";
            icon = &_upgradeMagicTex;
            break;
        case UpgradeType::IronSkin:
            name = "Iron Skin";
            desc = "+8% damage\nreduction";
            icon = &_upgradeDefenseTex;
            break;
        case UpgradeType::BladeEdge:
            name = "Blade Edge";
            desc = "+10% attack power\n+8% range";
            icon = &_upgradeAttackRangeTex;
            break;
        // ── Epic ──────────────────────────────────────────────────────────────
        case UpgradeType::WarGod:
            name = "War God";
            desc = "+20% attack power\n+10% range";
            icon = &_upgradeAttackPowerTex;
            break;
        case UpgradeType::Resilience:
            name = "Resilience";
            desc = "+30% max HP\nheal 3";
            icon = &_upgradeHealthTex;
            break;
        case UpgradeType::BladeStorm:
            name = "Blade Storm";
            desc = "+18% attack power\n+18% speed";
            icon = &_upgradeAttackPowerTex;
            break;
        case UpgradeType::Juggernaut:
            name = "Juggernaut";
            desc = "+20% max HP\n+8% defense";
            icon = &_upgradeHealthTex;
            break;
        case UpgradeType::ArcaneColossus:
            name = "Arcane Colossus";
            desc = "+30 mana\n+15% attack power";
            icon = &_upgradeMagicTex;
            break;
        case UpgradeType::LearnFireSpread:
            name = "Fire Spread";
            desc = "8 fireballs\nburn on hit";
            icon = &_abilityIconFireTex;
            break;
        case UpgradeType::LearnIceSpread:
            name = "Ice Spread";
            desc = "8 ice shards\nfreeze on hit";
            icon = &_abilityIconIceTex;
            break;
        case UpgradeType::LearnElectricSpread:
            name = "Electric Spread";
            desc = "8 bolts\nstun randomly";
            icon = &_abilityIconElectricTex;
            break;
        case UpgradeType::LearnFireBolt:
            name = "Fire Bolt";
            desc = "Aimed fireball\nhigh damage + burn";
            icon = &_abilityIconFireTex;
            break;
        case UpgradeType::LearnIceBolt:
            name = "Ice Bolt";
            desc = "Aimed shard\nhigh damage + freeze";
            icon = &_abilityIconIceTex;
            break;
        case UpgradeType::LearnElectricBolt:
            name = "Electric Bolt";
            desc = "Aimed bolt\nhigh damage + stun";
            icon = &_abilityIconElectricTex;
            break;
        case UpgradeType::LearnFireUltimate:
            name = "Fire Ultimate";
            desc = "Blasts everywhere\n4 dmg + burn, all MP";
            icon = &_abilityIconFireTex;
            break;
        case UpgradeType::LearnIceUltimate:
            name = "Ice Ultimate";
            desc = "Blasts everywhere\n4 dmg + freeze, all MP";
            icon = &_abilityIconIceTex;
            break;
        case UpgradeType::LearnElectricUltimate:
            name = "Elec. Ultimate";
            desc = "Blasts everywhere\n4 dmg + stun, all MP";
            icon = &_abilityIconElectricTex;
            break;
        default:
            name = "???";
            desc = "";
            icon = &_upgradeAttackPowerTex;
            break;
        }
    };

    Vector2 mouse = GetMousePosition();
    bool ready = (_levelUpOpenTimer <= 0.f);

    // ── Ultimate row (level 3 only, top) ─────────────────────────────────────
    if (showUlt)
    {
        // Row label
        int lblSz = 22;
        const char* lbl = "- Elemental Ultimate -";
        DrawText(lbl, (int)(sw / 2.f - MeasureText(lbl, lblSz) / 2.f),
                 (int)(ultRowY - lblSz - 8.f), lblSz, Color{255,200,80,200});

        for (int i = 0; i < 3; i++)
        {
            float x = startX + i * (cardW + cardGap);
            Rectangle card{ x, ultRowY, cardW, cardH };
            bool hovered = ready && CheckCollisionPointRec(mouse, card);

            // Gold-tinted card background to distinguish from regular row
            Color bgColor = hovered ? Color{55, 40, 10, 230} : Color{35, 25, 5, 215};
            DrawRectangleRounded(card, 0.12f, 8, bgColor);
            DrawRectangleRoundedLines(card, 0.12f, 8,
                hovered ? Color{255,180,0,255} : Color{200,140,30,160});

            const char* name = ""; const char* desc = ""; Texture2D* icon = nullptr;
            getUpgradeInfo(_levelUpUltimateOptions[i], name, desc, icon);

            // Icon / pattern area
            {
                const float iconAreaSize = cardW * 0.52f;
                const float previewCX    = x + cardW / 2.f;
                const float previewCY    = ultRowY + cardH * 0.10f + iconAreaSize / 2.f;

                UpgradeType opt = _levelUpUltimateOptions[i];
                Color ec =
                    (opt == UpgradeType::LearnFireUltimate)     ? Color{255,110, 20,255} :
                    (opt == UpgradeType::LearnIceUltimate)      ? Color{100,210,255,255} :
                                                                  Color{255,220, 30,255};
                Color ecDim = {ec.r, ec.g, ec.b, 80};
                const float outerR = iconAreaSize * 0.44f;
                DrawCircleLinesV({previewCX, previewCY}, outerR, ecDim);
                const float dotAngles[10] = {0.f,36.f,72.f,108.f,144.f,180.f,216.f,252.f,288.f,324.f};
                const float dotDists[10]  = {0.55f,0.80f,0.65f,0.90f,0.50f,0.75f,0.85f,0.60f,0.95f,0.70f};
                for (int d = 0; d < 10; d++)
                {
                    float rad  = dotAngles[d] * DEG2RAD;
                    float dist = outerR * dotDists[d];
                    Vector2 dp = {previewCX + cosf(rad)*dist, previewCY + sinf(rad)*dist};
                    DrawCircleV(dp, 5.f, ecDim);
                    DrawCircleV(dp, 3.f, ec);
                }
                if (icon && icon->id != 0)
                {
                    float iconTarget = (opt == UpgradeType::LearnFireUltimate) ? 44.f : 72.f;
                    float iconScale  = std::min(iconTarget/(float)icon->width, iconTarget/(float)icon->height);
                    float iw = icon->width * iconScale; float ih = icon->height * iconScale;
                    DrawTextureEx(*icon, {previewCX - iw*0.5f, previewCY - ih*0.5f}, 0.f, iconScale, WHITE);
                }
            }

            // Name
            int nameSz = 26;
            int nameW  = MeasureText(name, nameSz);
            DrawText(name, (int)(x + cardW/2.f - nameW/2.f),
                     (int)(ultRowY + cardH*0.58f), nameSz, hovered ? GOLD : RAYWHITE);

            // Description
            std::string descStr = desc;
            int nlPos = (int)descStr.find('\n');
            int descSz = 20;
            if (nlPos != (int)std::string::npos)
            {
                std::string line1 = descStr.substr(0, nlPos);
                std::string line2 = descStr.substr(nlPos + 1);
                DrawText(line1.c_str(), (int)(x + cardW/2.f - MeasureText(line1.c_str(),descSz)/2.f),
                         (int)(ultRowY + cardH*0.72f), descSz, LIGHTGRAY);
                DrawText(line2.c_str(), (int)(x + cardW/2.f - MeasureText(line2.c_str(),descSz)/2.f),
                         (int)(ultRowY + cardH*0.72f + descSz + 4), descSz, LIGHTGRAY);
            }
            else
            {
                DrawText(desc, (int)(x + cardW/2.f - MeasureText(desc,descSz)/2.f),
                         (int)(ultRowY + cardH*0.72f), descSz, LIGHTGRAY);
            }

            if (ready && hovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
            {
                _player.ApplyUpgrade(_levelUpUltimateOptions[i]);
                _ultimateRowPicked = true;
                if (_regularRowPicked)
                {
                    if (_levelUpReturnState == GameState::Map) _mapOpenTimer = 0.4f;
                    _gameState = _levelUpReturnState;
                }
            }
        }
    }

    // ── Regular row ──────────────────────────────────────────────────────────
    if (showReg)
    {
        // Row label when both rows are visible
        if (showUlt)
        {
            int lblSz = 22;
            const char* lbl = "- Upgrade -";
            DrawText(lbl, (int)(sw / 2.f - MeasureText(lbl, lblSz) / 2.f),
                     (int)(regRowY - lblSz - 8.f), lblSz, Color{200,200,200,180});
        }

    for (int i = 0; i < 3; i++)
    {
        float x = startX + i * (cardW + cardGap);
        Rectangle card{ x, cardY, cardW, cardH };

        bool hovered = ready && CheckCollisionPointRec(mouse, card);

        // Rarity-based colors
        UpgradeRarity rarity = _player.GetUpgradeRarity(_levelUpOptions[i]);
        Color bgNormal, bgHover, borderNormal, borderHover, stripColor;
        Color nameColor;
        const char* rarityLabel;
        switch (rarity)
        {
        case UpgradeRarity::Common:
            bgNormal = Color{22,22,25,210}; bgHover = Color{42,42,50,230};
            borderNormal = Color{120,120,120,130}; borderHover = Color{200,200,200,220};
            stripColor = Color{80,80,80,200}; nameColor = RAYWHITE; rarityLabel = "COMMON"; break;
        case UpgradeRarity::Rare:
            bgNormal = Color{15,20,42,210}; bgHover = Color{25,40,85,230};
            borderNormal = Color{55,100,200,150}; borderHover = Color{100,160,255,255};
            stripColor = Color{35,75,180,210}; nameColor = Color{160,210,255,255}; rarityLabel = "RARE"; break;
        default: // Epic
            bgNormal = Color{30,15,42,210}; bgHover = Color{60,25,85,230};
            borderNormal = Color{130,45,200,150}; borderHover = Color{210,100,255,255};
            stripColor = Color{100,35,165,220}; nameColor = Color{225,155,255,255}; rarityLabel = "EPIC"; break;
        }
        Color bgColor = hovered ? bgHover : bgNormal;
        DrawRectangleRounded(card, 0.12f, 8, bgColor);
        DrawRectangleRoundedLines(card, 0.12f, 8, hovered ? borderHover : borderNormal);

        const char* name = "";
        const char* desc = "";
        Texture2D* icon  = nullptr;
        getUpgradeInfo(_levelUpOptions[i], name, desc, icon);

        // Icon area — ability unlocks get a drawn attack-pattern preview so the
        // player can immediately see "burst vs single shot". Stat upgrades keep
        // their regular texture icon.
        {
            const float iconAreaSize = cardW * 0.52f;
            const float previewCX    = x + cardW / 2.f;
            const float previewCY    = cardY + cardH * 0.10f + iconAreaSize / 2.f;

            UpgradeType opt = _levelUpOptions[i];
            bool isSpreadAbility   = (opt == UpgradeType::LearnFireSpread  ||
                                      opt == UpgradeType::LearnIceSpread   ||
                                      opt == UpgradeType::LearnElectricSpread);
            bool isBoltAbility     = (opt == UpgradeType::LearnFireBolt    ||
                                      opt == UpgradeType::LearnIceBolt     ||
                                      opt == UpgradeType::LearnElectricBolt);
            bool isUltimateAbility = (opt == UpgradeType::LearnFireUltimate    ||
                                      opt == UpgradeType::LearnIceUltimate     ||
                                      opt == UpgradeType::LearnElectricUltimate);

            if (isSpreadAbility || isBoltAbility || isUltimateAbility)
            {
                // Pick element colour
                Color ec =
                    (opt == UpgradeType::LearnFireSpread    || opt == UpgradeType::LearnFireBolt    || opt == UpgradeType::LearnFireUltimate)     ? Color{255, 110,  20, 255} :
                    (opt == UpgradeType::LearnIceSpread     || opt == UpgradeType::LearnIceBolt     || opt == UpgradeType::LearnIceUltimate)      ? Color{100, 210, 255, 255} :
                                                                                                                                                    Color{255, 220,  30, 255};
                Color ecDim = { ec.r, ec.g, ec.b, 80 };

                if (isSpreadAbility)
                {
                    // 8 radiating arrows from a centre point
                    const float armLen  = iconAreaSize * 0.40f;
                    const float headLen = 10.f;
                    const float headW   = 6.f;
                    const float angles[8] = { 0.f, 45.f, 90.f, 135.f, 180.f, 225.f, 270.f, 315.f };

                    for (float deg : angles)
                    {
                        float rad  = deg * DEG2RAD;
                        float cx2  = cosf(rad);
                        float cy2  = sinf(rad);
                        Vector2 shaft0 = { previewCX + cx2 * 22.f, previewCY + cy2 * 22.f };
                        Vector2 tip    = { previewCX + cx2 * armLen, previewCY + cy2 * armLen };

                        DrawLineEx(shaft0, tip, 2.2f, ec);

                        Vector2 perpL = { -cy2, cx2 };
                        Vector2 hb1 = { tip.x - cx2 * headLen + perpL.x * headW,
                                        tip.y - cy2 * headLen + perpL.y * headW };
                        Vector2 hb2 = { tip.x - cx2 * headLen - perpL.x * headW,
                                        tip.y - cy2 * headLen - perpL.y * headW };
                        DrawTriangle(tip, hb1, hb2, ec);
                    }
                }
                else if (isBoltAbility)
                {
                    // Single large arrow aimed to the right
                    const float armLen  = iconAreaSize * 0.38f;
                    const float headLen = 18.f;
                    const float headW   = 11.f;

                    Vector2 shaftStart = { previewCX - armLen * 0.55f, previewCY };
                    Vector2 tip        = { previewCX + armLen,          previewCY };
                    DrawLineEx(shaftStart, tip, 4.5f, ec);

                    Vector2 hb1 = { tip.x - headLen, tip.y - headW };
                    Vector2 hb2 = { tip.x - headLen, tip.y + headW };
                    DrawTriangle(tip, hb1, hb2, ec);
                }
                else // ultimate
                {
                    // Scattered burst: outer ring + 10 dots at irregular distances
                    // to suggest "explosions everywhere". Positions are deterministic
                    // so they don't flicker each frame.
                    const float outerR = iconAreaSize * 0.44f;
                    DrawCircleLinesV({ previewCX, previewCY }, outerR, ecDim);

                    const float dotAngles[10] = { 0.f, 36.f, 72.f, 108.f, 144.f, 180.f, 216.f, 252.f, 288.f, 324.f };
                    const float dotDists[10]  = { 0.55f, 0.80f, 0.65f, 0.90f, 0.50f, 0.75f, 0.85f, 0.60f, 0.95f, 0.70f };

                    for (int d = 0; d < 10; d++)
                    {
                        float rad  = dotAngles[d] * DEG2RAD;
                        float dist = outerR * dotDists[d];
                        Vector2 dp = { previewCX + cosf(rad) * dist, previewCY + sinf(rad) * dist };
                        DrawCircleV(dp, 5.f, ecDim);
                        DrawCircleV(dp, 3.f, ec);
                    }
                }

                // Icon centred on top of the arrow pattern
                if (icon && icon->id != 0)
                {
                    // Fire icon reads clearly at a smaller size; ice and electric
                    // sprites are visually smaller so give them more room.
                    float iconTarget =
                        (opt == UpgradeType::LearnIceSpread      || opt == UpgradeType::LearnIceBolt      || opt == UpgradeType::LearnIceUltimate      ||
                         opt == UpgradeType::LearnElectricSpread || opt == UpgradeType::LearnElectricBolt || opt == UpgradeType::LearnElectricUltimate)
                        ? 72.f : 44.f;
                    float iconScale = std::min(iconTarget / (float)icon->width,
                                               iconTarget / (float)icon->height);
                    float iw = icon->width  * iconScale;
                    float ih = icon->height * iconScale;
                    DrawTextureEx(*icon, { previewCX - iw * 0.5f, previewCY - ih * 0.5f },
                                  0.f, iconScale, WHITE);
                }
            }
            else if (icon && icon->id != 0)
            {
                // Stat upgrade — draw the regular texture icon
                float iconScale = std::min(iconAreaSize / (float)icon->width,
                                           iconAreaSize / (float)icon->height);
                float iconW = icon->width  * iconScale;
                float iconH = icon->height * iconScale;
                float iconX = previewCX - iconW / 2.f;
                float iconY = cardY + cardH * 0.10f + (iconAreaSize - iconH) / 2.f;
                DrawTextureEx(*icon, { iconX, iconY }, 0.f, iconScale, WHITE);
            }
        }

        // Name (rarity-tinted when not hovered)
        int nameSz = 26;
        int nameW  = MeasureText(name, nameSz);
        DrawText(name,
            (int)(x + cardW / 2.f - nameW / 2.f),
            (int)(cardY + cardH * 0.58f),
            nameSz, hovered ? GOLD : nameColor);

        // Description — manual newline split
        std::string descStr = desc;
        int nlPos = (int)descStr.find('\n');
        int descSz = 20;
        if (nlPos != (int)std::string::npos)
        {
            std::string line1 = descStr.substr(0, nlPos);
            std::string line2 = descStr.substr(nlPos + 1);
            int l1W = MeasureText(line1.c_str(), descSz);
            int l2W = MeasureText(line2.c_str(), descSz);
            DrawText(line1.c_str(),
                (int)(x + cardW / 2.f - l1W / 2.f),
                (int)(cardY + cardH * 0.72f), descSz, LIGHTGRAY);
            DrawText(line2.c_str(),
                (int)(x + cardW / 2.f - l2W / 2.f),
                (int)(cardY + cardH * 0.72f + descSz + 4), descSz, LIGHTGRAY);
        }
        else
        {
            int dW = MeasureText(desc, descSz);
            DrawText(desc,
                (int)(x + cardW / 2.f - dW / 2.f),
                (int)(cardY + cardH * 0.72f), descSz, LIGHTGRAY);
        }

        // Rarity strip at card bottom
        {
            const float stripH = 26.f;
            Rectangle stripRect{ x + 2.f, cardY + cardH - stripH - 2.f, cardW - 4.f, stripH };
            DrawRectangleRec(stripRect, stripColor);
            int rarSz = 15;
            DrawText(rarityLabel,
                (int)(x + cardW / 2.f - MeasureText(rarityLabel, rarSz) / 2.f),
                (int)(cardY + cardH - stripH - 2.f + (stripH - rarSz) / 2.f),
                rarSz, WHITE);
        }

        // Click to select — blocked until open timer expires
        if (ready && hovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        {
            _player.ApplyUpgrade(_levelUpOptions[i]);
            _awaitingStartingAbility = false;
            _regularRowPicked = true;
            if (!_showUltimateRow || _ultimateRowPicked)
            {
                if (_levelUpReturnState == GameState::Map) _mapOpenTimer = 0.4f;
                _gameState = _levelUpReturnState;
            }
        }
    }
    } // end showReg

    // Hint / loading cue
    const char* hint;
    if (_levelUpOpenTimer > 0.f)
        hint = "Loading...";
    else if (showUlt && showReg)
        hint = "Choose one from each row";
    else
        hint = "Click a card to choose";

    Color hintColor = (_levelUpOpenTimer > 0.f) ? Fade(GRAY, 0.6f) : Fade(LIGHTGRAY, 0.7f);
    float hintRefY  = showReg ? (regRowY + cardH + 28.f) : (ultRowY + cardH + 28.f);
    int hintW = MeasureText(hint, 22);
    DrawText(hint, (int)(sw / 2.f - hintW / 2.f), (int)hintRefY, 22, hintColor);
}

void Engine::HandlePlayerMeleeDamage()
{
    if (!_player.CanApplyMeleeDamage())
        return;

    bool hitAny = false;
    Rectangle attackRec = _player.GetAttackCollisionRec();

    for (auto& enemy : _enemies)
    {
        if (!enemy->IsActive())
            continue;
        if (!enemy->IsAlive())
            continue;

        if (CheckCollisionRecs(attackRec, enemy->GetHitCollisionRec()))
        {
            int dmg = _player.GetMeleeDamage();
            enemy->TakeDamage(dmg, _player.GetWorldPos());
            SpawnFloatingText(enemy->GetWorldPos(), dmg, YELLOW);
            SpawnHitEffect(Character::CastType::None, enemy->GetWorldPos(), _player.GetFacingDirection());
            hitAny = true;
        }
    }

    _player.ConsumeMeleeDamageFrame();

    if (hitAny)
        TriggerScreenShake(6.f, 0.07f);
}

void Engine::SpawnSpreadBurst(AbilityType element)
{
    static const std::array<Vector2, 8> directions{
        Vector2{  1.f,  0.f },
        Vector2{ -1.f,  0.f },
        Vector2{  0.f,  1.f },
        Vector2{  0.f, -1.f },
        Vector2{  1.f,  1.f },
        Vector2{  1.f, -1.f },
        Vector2{ -1.f,  1.f },
        Vector2{ -1.f, -1.f }
    };

    Vector2 origin = _player.GetCastOrigin();

    for (const Vector2& dir : directions)
    {
        SpreadProjectile projectile;
        projectile.Init(origin, dir, element);
        _spreadProjectiles.push_back(projectile);
    }
}

void Engine::SpawnBolt(AbilityType element)
{
    SpreadProjectile projectile;
    projectile.Init(_player.GetCastOrigin(), _player.GetFacingDirection(), element);
    _spreadProjectiles.push_back(projectile);
}

void Engine::SpawnUltimateBurst(AbilityType element)
{
    // Fill the entire visible screen with blasts. Positions are computed in
    // screen space then converted to world space so they always cover the
    // full 1920x1080 view regardless of where the player is.
    const int   blastCount = 25;
    const float lifetime   = _ultCinematicDuration;
    const float margin     = 80.f;   // keep icons away from the very edge

    const float sw = (float)GetScreenWidth();
    const float sh = (float)GetScreenHeight();
    Vector2 playerPos = _player.GetWorldPos();

    for (int i = 0; i < blastCount; i++)
    {
        float sx = (float)GetRandomValue((int)margin, (int)(sw - margin));
        float sy = (float)GetRandomValue((int)margin, (int)(sh - margin));

        // Convert screen-space position to world-space so DrawUltimateBlasts
        // maps it back to the same screen pixel via the worldOffset calculation.
        Vector2 pos = {
            playerPos.x + sx - sw * 0.5f,
            playerPos.y + sy - sh * 0.5f
        };

        // Angle each blast outward from the screen center so they look like
        // they're radiating away from the player.
        float rotation = atan2f(sy - sh * 0.5f, sx - sw * 0.5f) * RAD2DEG;

        UltimateBlast blast;
        blast.worldPos  = pos;
        blast.element   = element;
        blast.lifetime  = lifetime;
        blast.timer     = lifetime;
        blast.rotation  = rotation;
        _ultimateBlasts.push_back(blast);
    }

    TriggerScreenShake(6.f, 0.3f);
}

void Engine::UpdateUltimateBlasts(float dt)
{
    // Purely visual during the cinematic phase — damage is applied all at once
    // by ApplyUltimateImpact at the Impact phase transition.
    for (auto& blast : _ultimateBlasts)
        blast.timer -= dt;

    _ultimateBlasts.erase(
        std::remove_if(_ultimateBlasts.begin(), _ultimateBlasts.end(),
            [](const UltimateBlast& b) { return b.timer <= 0.f; }),
        _ultimateBlasts.end());
}

void Engine::TriggerUltimateSequence(AbilityType element)
{
    _ultimatePhase       = UltimatePhase::WindUp;
    _ultimatePhaseTimer  = 0.f;
    _ultimateCircleAngle = 0.f;
    _ultimateElement     = element;
    _ultimateBlasts.clear();
    StopSound(_fireballCastSound);
    PlaySound(_fireballCastSound);
}

void Engine::UpdateUltimateSequence(float dt)
{
    _ultimatePhaseTimer  += dt;
    _ultimateCircleAngle += dt * 2.2f;

    switch (_ultimatePhase)
    {
    case UltimatePhase::WindUp:
        if (_ultimatePhaseTimer >= _ultWindUpDuration)
        {
            _ultimatePhase      = UltimatePhase::Cinematic;
            _ultimatePhaseTimer = 0.f;
            SpawnUltimateBurst(_ultimateElement);
        }
        break;

    case UltimatePhase::Cinematic:
        UpdateUltimateBlasts(dt);
        if (_ultimatePhaseTimer >= _ultCinematicDuration)
        {
            _ultimatePhase      = UltimatePhase::Impact;
            _ultimatePhaseTimer = 0.f;
            ApplyUltimateImpact();
            _ultimateBlasts.clear();
            TriggerScreenShake(14.f, 0.45f);
            StopSound(_explosionSound);
            PlaySound(_explosionSound);
        }
        break;

    case UltimatePhase::Impact:
        if (_ultimatePhaseTimer >= _ultImpactDuration)
        {
            _ultimatePhase      = UltimatePhase::Release;
            _ultimatePhaseTimer = 0.f;
        }
        break;

    case UltimatePhase::Release:
        if (_ultimatePhaseTimer >= _ultReleaseDuration)
        {
            _ultimatePhase      = UltimatePhase::None;
            _ultimatePhaseTimer = 0.f;
        }
        break;

    default:
        break;
    }
}

void Engine::ApplyUltimateImpact()
{
    // Hit every enemy currently visible on screen (with a small margin beyond edges).
    const float halfW = GetScreenWidth()  * 0.55f;
    const float halfH = GetScreenHeight() * 0.55f;
    Vector2 playerPos = _player.GetWorldPos();

    for (auto& enemy : _enemies)
    {
        if (!enemy->IsActive() || !enemy->IsAlive())
            continue;

        Vector2 delta = Vector2Subtract(enemy->GetWorldPos(), playerPos);
        if (fabsf(delta.x) > halfW || fabsf(delta.y) > halfH)
            continue;

        int dmg = enemy->AsMolarbeast() ? std::min(3, _player.GetUltimateHitDamage(_ultimateElement)) : _player.GetUltimateHitDamage(_ultimateElement);
        enemy->TakeDamage(dmg, playerPos);

        if (_ultimateElement == AbilityType::FireUltimate)
            enemy->ApplyBurn(0.5f, _player.GetBoltBurnDamage(_ultimateElement), playerPos);
        else if (_ultimateElement == AbilityType::IceUltimate)
            enemy->ApplyFreeze(3.f);
        else if (_ultimateElement == AbilityType::ElectricUltimate)
            enemy->ApplyElectricCharge();
    }
}

void Engine::DrawUltimateSequence()
{
    if (_ultimatePhase == UltimatePhase::None)
        return;

    const float sw = (float)GetScreenWidth();
    const float sh = (float)GetScreenHeight();
    const float cx = sw * 0.5f;
    const float cy = sh * 0.5f;

    Color ec =
        (_ultimateElement == AbilityType::FireUltimate)     ? Color{255, 110,  20, 255} :
        (_ultimateElement == AbilityType::IceUltimate)      ? Color{ 80, 200, 255, 255} :
                                                               Color{255, 220,   0, 255};

    // ── Dark overlay (ramps up during wind-up, holds, fades on release) ───────
    if (_ultimatePhase != UltimatePhase::Impact)
    {
        float alpha =
            (_ultimatePhase == UltimatePhase::WindUp)
                ? (_ultimatePhaseTimer / _ultWindUpDuration) * 0.55f :
            (_ultimatePhase == UltimatePhase::Release)
                ? 0.55f * (1.f - _ultimatePhaseTimer / _ultReleaseDuration)
                : 0.55f;
        DrawRectangle(0, 0, (int)sw, (int)sh, Fade(BLACK, alpha));
    }

    // ── Impact flash ─────────────────────────────────────────────────────────
    if (_ultimatePhase == UltimatePhase::Impact)
    {
        float t = 1.f - (_ultimatePhaseTimer / _ultImpactDuration);
        DrawRectangle(0, 0, (int)sw, (int)sh, Fade(WHITE, t * 0.85f));
        // Expanding ring in element colour
        Color ringCol = { ec.r, ec.g, ec.b, (unsigned char)(t * 210.f) };
        DrawCircleLinesV({ cx, cy }, (1.f - t) * sw * 0.9f, ringCol);
    }

    // ── Magic circle (wind-up + cinematic) ───────────────────────────────────
    if (_ultimatePhase == UltimatePhase::WindUp || _ultimatePhase == UltimatePhase::Cinematic)
    {
        float progress = (_ultimatePhase == UltimatePhase::WindUp)
            ? _ultimatePhaseTimer / _ultWindUpDuration : 1.f;

        const float radii[3]  = { 75.f, 120.f, 165.f };
        const float speeds[3] = { 1.0f, -0.65f, 0.45f };

        for (int r = 0; r < 3; r++)
        {
            float radius = radii[r] * progress;
            float angle  = _ultimateCircleAngle * speeds[r];
            unsigned char a = (unsigned char)(190.f * progress);
            Color ringCol = { ec.r, ec.g, ec.b, a };

            DrawCircleLinesV({ cx, cy }, radius, ringCol);

            // 4 glowing markers rotating around each ring
            for (int m = 0; m < 4; m++)
            {
                float ma = angle + m * (PI * 0.5f);
                Vector2 mp = { cx + cosf(ma) * radius, cy + sinf(ma) * radius };
                DrawCircleV(mp, 5.f * progress, ringCol);
            }
        }

        // Soft glow around the player's screen position
        DrawCircleV({ cx, cy }, 48.f * progress,
            { ec.r, ec.g, ec.b, (unsigned char)(85.f * progress) });
    }
}

void Engine::DrawUltimateBlasts(Vector2 worldOffset)
{
    for (const auto& blast : _ultimateBlasts)
    {
        if (blast.timer <= 0.f)
            continue;

        // Progress 0→1 as blast ages
        float progress = 1.f - (blast.timer / blast.lifetime);

        // Pulse envelope: pop in fast, hold, fade out
        float pulse;
        if      (progress < 0.3f)  pulse = progress / 0.3f;
        else if (progress < 0.75f) pulse = 1.f;
        else                       pulse = 1.f - (progress - 0.75f) / 0.25f;

        if (pulse <= 0.f) continue;

        // Animated sprite sheet — ultimate types use their own 64x64 sheets
        const Texture2D& tex = SpreadProjectile::GetAnimTexture(blast.element);

        const float fw        = (float)SpreadProjectile::GetFrameWFor(blast.element);
        const float fh        = (float)SpreadProjectile::GetFrameHFor(blast.element);
        const int   frameCount = SpreadProjectile::GetFrameCountFor(blast.element);

        float elapsed = blast.lifetime - blast.timer;
        int   frame   = (int)(elapsed / (1.f / 16.f)) % frameCount;

        Rectangle src = GetAnimationFrameRect(tex, (int)fw, (int)fh, frame);

        const float maxSize = 200.f;
        float size = maxSize * pulse;

        Vector2 screenPos = {
            blast.worldPos.x + worldOffset.x + GetScreenWidth()  * 0.5f,
            blast.worldPos.y + worldOffset.y + GetScreenHeight() * 0.5f
        };

        // Fixed facing direction per blast — no spinning
        float rotation = blast.rotation;

        Rectangle dest     = { screenPos.x, screenPos.y, size, size };
        Vector2   origin   = { size * 0.5f, size * 0.5f };
        unsigned char alpha = (unsigned char)(pulse * 255.f);

        // Soft glow behind — slightly larger, low alpha
        float     glowSize   = size * 1.65f;
        Rectangle glowDest   = { screenPos.x, screenPos.y, glowSize, glowSize };
        Vector2   glowOrigin = { glowSize * 0.5f, glowSize * 0.5f };
        DrawTexturePro(tex, src, glowDest, glowOrigin, rotation,
            { 255, 255, 255, (unsigned char)(pulse * 55.f) });

        // Main animated frame
        DrawTexturePro(tex, src, dest, origin, rotation, { 255, 255, 255, alpha });
    }
}

void Engine::UpdateEffects(float dt)
{
    for (auto& effect : _effects)
    {
        if (!effect.active)
            continue;

        effect.runningTime += dt;
        if (effect.runningTime >= effect.frameTime)
        {
            effect.runningTime = 0.f;
            effect.frame++;

            if (effect.frame >= effect.frameCount)
                effect.active = false;
        }
    }

    _effects.erase(
        std::remove_if(_effects.begin(), _effects.end(),
            [](const AnimatedEffect& effect) { return !effect.active; }),
        _effects.end());
}

void Engine::DrawEffects(Vector2 worldOffset)
{
    for (const auto& effect : _effects)
    {
        if (!effect.active || effect.texture == nullptr || effect.texture->id == 0)
            continue;

        Vector2 worldPos = effect.followPlayer
            ? (effect.followPlayerCenter
                ? Vector2Add(_player.GetWorldPos(), Vector2{ effect.offset.x, effect.offset.y })
                : Vector2Add(_player.GetCastOrigin(), Vector2{ effect.offset.x, effect.offset.y }))
            : effect.worldPos;

        Vector2 screenPos = Vector2Add(worldPos, worldOffset);
        screenPos.x += GetScreenWidth() / 2.f;
        screenPos.y += GetScreenHeight() / 2.f;

        float rotation = atan2f(effect.direction.y, effect.direction.x) * RAD2DEG;
        Rectangle source = GetAnimationFrameRect(*effect.texture, effect.frameWidth, effect.frameHeight, effect.frame);
        Rectangle dest{
            screenPos.x,
            screenPos.y,
            effect.frameWidth * effect.scale,
            effect.frameHeight * effect.scale
        };

        DrawTexturePro(*effect.texture, source, dest,
            Vector2{ dest.width * 0.5f, dest.height * 0.5f }, rotation, effect.tint);
    }
}

void Engine::SpawnCastEffect(Character::CastType castType)
{
    AnimatedEffect effect{};
    effect.followPlayer = true;
    effect.worldPos = _player.GetCastOrigin();
    effect.direction = _player.GetFacingDirection();
    effect.active = true;

    switch (castType)
    {
    case Character::CastType::FireSpread:
        effect.texture = &_fireballCastTex;
        effect.frameCount = 8;
        effect.scale = 4.f;
        break;
    case Character::CastType::IceSpread:
        effect.texture = &_iceHitTex;
        effect.frameCount = 8;
        effect.scale = 4.f;
        effect.tint = Color{ 100, 200, 255, 255 };
        break;
    case Character::CastType::ElectricSpread:
        effect.texture = &_lightningCastTex;
        effect.frameCount = 8;
        effect.scale = 4.f;
        break;
    default:
        effect.active = false;
        break;
    }

    if (effect.active)
        _effects.push_back(effect);
}

void Engine::SpawnHitEffect(Character::CastType castType, Vector2 worldPos, Vector2 direction)
{
    AnimatedEffect effect{};
    effect.followPlayer = false;
    effect.worldPos = worldPos;
    effect.direction = direction;
    effect.active = true;
    effect.frameTime = 1.f / 20.f;

    switch (castType)
    {
    case Character::CastType::None:
        effect.texture = &_genericHitTex;
        effect.frameCount = 5;
        effect.scale = 3.5f;
        effect.tint = Color{ 255, 150, 150, 255 };
        break;

    case Character::CastType::FireSpread:
        effect.texture = &_fireballHitTex;
        effect.frameCount = 6;
        effect.scale = 4.f;
        effect.tint = WHITE;
        break;
    case Character::CastType::IceSpread:
        effect.texture = &_iceHitTex;
        effect.frameCount = 5;
        effect.scale = 4.f;
        effect.tint = Color{ 100, 200, 255, 255 };
        break;
    case Character::CastType::ElectricSpread:
        effect.texture = &_genericHitTex;   // Hit03.png — lightning impact sprite
        effect.frameCount = 5;
        effect.scale = 4.f;
        effect.tint = WHITE;
        break;

    default:
        effect.active = false;
        break;
    }

    if (effect.active)
        _effects.push_back(effect);
}

void Engine::SpawnHealEffect()
{
    AnimatedEffect effect{};
    effect.texture = &_healEffectTex;
    effect.followPlayer = true;
    effect.followPlayerCenter = true;
    effect.offset = Vector2{ 0.f, -20.f };
    effect.direction = Vector2{ 1.f, 0.f };
    effect.tint = WHITE;
    effect.frameCount = 13;
    effect.frameTime = 1.f / 16.f;
    effect.scale = 4.5f;
    effect.active = (_healEffectTex.id != 0);

    if (effect.active)
        _effects.push_back(effect);
}

void Engine::UpdateSpreadProjectiles(float dt)
{
    for (auto& projectile : _spreadProjectiles)
    {
        if (!projectile.IsActive())
            continue;

        projectile.Update(dt);

        if (!projectile.IsActive())
            continue;

        // Helper: map element -> CastType so hit effects use the right sprite
        auto elementToCastType = [](AbilityType el) -> Character::CastType
        {
            if (el == AbilityType::IceSpread     || el == AbilityType::IceBolt)      return Character::CastType::IceSpread;
            if (el == AbilityType::ElectricSpread|| el == AbilityType::ElectricBolt) return Character::CastType::ElectricSpread;
            return Character::CastType::FireSpread;
        };

        // Map-boundary wall check — same margins as player collision
        {
            float mapW = _map.width  * _mapScale;
            float mapH = _map.height * _mapScale;
            Vector2 p  = projectile.GetWorldPos();
            if (p.x < 76.f || p.x > mapW - 96.f ||
                p.y < 42.f || p.y > mapH - 320.f)
            {
                SpawnHitEffect(elementToCastType(projectile.GetElement()),
                               projectile.GetWorldPos(), projectile.GetDirection());
                projectile.Destroy();
                continue;
            }
        }

        for (auto& prop : _props)
        {
            if (CheckCollisionRecs(projectile.GetCollisionRec(), prop.GetCollisionRec()))
            {
                SpawnHitEffect(elementToCastType(projectile.GetElement()),
                               projectile.GetWorldPos(), projectile.GetDirection());
                projectile.Destroy();
                break;
            }
        }

        if (!projectile.IsActive())
            continue;

        for (auto& enemy : _enemies)
        {
            if (!enemy->IsActive() || !enemy->IsAlive())
                continue;

            if (!CheckCollisionRecs(projectile.GetCollisionRec(), enemy->GetHitCollisionRec()))
                continue;

            AbilityType element = projectile.GetElement();

            bool isBolt = (element == AbilityType::FireBolt  ||
                           element == AbilityType::IceBolt   ||
                           element == AbilityType::ElectricBolt);

            // Bolts deal more damage per shot; boss caps at 2 from bolts vs 1 from spread
            int hitDamage;
            if (enemy->AsMolarbeast() != nullptr)
                hitDamage = isBolt ? 2 : 1;
            else
                hitDamage = isBolt ? _player.GetBoltHitDamage(element) : _player.GetSpreadHitDamage(element);
            enemy->TakeDamage(hitDamage, _player.GetWorldPos());
            {
                Color dmgColor = (element == AbilityType::IceSpread   || element == AbilityType::IceBolt)      ? SKYBLUE  :
                                 (element == AbilityType::ElectricSpread || element == AbilityType::ElectricBolt) ? YELLOW   : ORANGE;
                SpawnFloatingText(enemy->GetWorldPos(), hitDamage, dmgColor);
            }

            // Per-element on-hit effect — same for both spread and bolt of the same element
            Character::CastType hitEffectType = Character::CastType::FireSpread;
            if (element == AbilityType::FireSpread || element == AbilityType::FireBolt)
            {
                int burnDmg = isBolt ? _player.GetBoltBurnDamage(element) : _player.GetSpreadBurnDamage(element);
                enemy->ApplyBurn(1.f, burnDmg, _player.GetWorldPos());
                hitEffectType = Character::CastType::FireSpread;
            }
            else if (element == AbilityType::IceSpread || element == AbilityType::IceBolt)
            {
                enemy->ApplyFreeze(3.f);
                hitEffectType = Character::CastType::IceSpread;
            }
            else if (element == AbilityType::ElectricSpread || element == AbilityType::ElectricBolt)
            {
                enemy->ApplyElectricCharge();
                hitEffectType = Character::CastType::ElectricSpread;
            }

            SpawnHitEffect(hitEffectType, projectile.GetWorldPos(), projectile.GetDirection());
            projectile.Destroy();
            TriggerScreenShake(4.f, 0.05f);
            StopSound(_explosionSound);
            PlaySound(_explosionSound);
            break;
        }
    }

    _spreadProjectiles.erase(
        std::remove_if(_spreadProjectiles.begin(), _spreadProjectiles.end(),
            [](const SpreadProjectile& p) { return !p.IsActive(); }),
        _spreadProjectiles.end());
}

void Engine::SpawnFloatingText(Vector2 worldPos, int value, Color color)
{
    FloatingText ft;
    ft.worldPos  = worldPos;
    ft.value     = value;
    ft.color     = color;
    ft.spawnTime = (float)GetTime();
    _floatingTexts.push_back(ft);
}

void Engine::SpawnEnemyDrop(Vector2 worldPos)
{
    // Boss fights suppress all drops — the fight is won on build + passive regen,
    // not on floor loot. Support add kills also fall through this path.
    if (IsBossFightActive())
        return;

    // Rare drop: heals only. Mana gems removed from the drop pool —
    // passive regen (Character::kManaRegenPerSecond) is now the mana source.
    // ManaGemPickup class kept on disk as legacy, not spawned here.
    if (GetRandomValue(1, 100) > kEnemyDropChancePercent)
        return;

    // Find a valid drop position (avoid spawning inside a prop)
    Vector2 dropPos = worldPos;
    if (!IsSpawnPositionValid(dropPos))
    {
        float mapW = _map.width * _mapScale;
        float mapH = _map.height * _mapScale;
        for (int attempt = 0; attempt < 20; ++attempt)
        {
            Vector2 candidate{
                worldPos.x + (float)GetRandomValue(-180, 180),
                worldPos.y + (float)GetRandomValue(-180, 180)
            };
            candidate.x = (float)ClampInt((int)candidate.x, 100, (int)mapW - 100);
            candidate.y = (float)ClampInt((int)candidate.y, 100, (int)mapH - 100);
            if (IsSpawnPositionValid(candidate)) { dropPos = candidate; break; }
        }
    }

    auto p = std::make_unique<HealPickup>();
    p->Init(dropPos);
    _pickups.push_back(std::move(p));
}

void Engine::SpawnTimedPickup()
{
    // Boss fights are self-contained — no timed pickups during them.
    // The timer keeps ticking so a pickup is not immediately ready when
    // the boss dies; it resets naturally after kDefaultTimedPickupInterval.
    if (IsBossFightActive())
        return;

    // Timed pickups are heals only. Mana gems removed from the timed pool —
    // passive regen covers mana recovery between waves.
    float mapW = _map.width  * _mapScale;
    float mapH = _map.height * _mapScale;

    Vector2 pos{};
    int attempts = 0;
    do
    {
        pos = Vector2{
            (float)GetRandomValue(300, (int)mapW - 300),
            (float)GetRandomValue(300, (int)mapH - 300)
        };
        attempts++;
    } while (!IsSpawnPositionValid(pos) && attempts < 40);

    auto p = std::make_unique<HealPickup>();
    p->Init(pos);
    p->SetTimerSpawned(true);
    _pickups.push_back(std::move(p));
}

void Engine::DrawHowToPlay()
{
    const float sw = (float)GetScreenWidth();
    const float sh = (float)GetScreenHeight();

    // ── Font sizes (screen-relative) ────────────────────────────────────────
    const int titleSz  = (int)(sh * 0.074f);   // ~80
    const int headerSz = (int)(sh * 0.038f);   // ~41
    const int labelSz  = (int)(sh * 0.030f);   // ~32
    const int descSz   = (int)(sh * 0.024f);   // ~26

    // ── Background — slow-scrolling checkerboard ────────────────────────────
    {
        const int   cell  = 80;
        const Color dark  = Color{ 52, 38, 26, 255 };
        const Color light = Color{ 72, 54, 36, 255 };

        const int period = cell * 2;
        float t    = (float)GetTime();
        int   offX = (int)fmodf(t * 22.f, (float)period);
        int   offY = (int)fmodf(t * 12.f, (float)period);
        int   phaseX = offX / cell;
        int   phaseY = offY / cell;
        int   pixX   = offX % cell;
        int   pixY   = offY % cell;

        for (int gy = -1; gy <= (int)(sh / cell) + 1; gy++)
        {
            for (int gx = -1; gx <= (int)(sw / cell) + 1; gx++)
            {
                bool isDark = (((gx + phaseX) + (gy + phaseY)) % 2 + 2) % 2 == 0;
                DrawRectangle(gx * cell - pixX, gy * cell - pixY,
                    cell, cell, isDark ? dark : light);
            }
        }
    }

    // ── Title banner ────────────────────────────────────────────────────────
    float titleBannerY = sh * 0.02f;
    float titleBannerH = titleSz + sh * 0.03f;
    DrawRectangle(0, (int)titleBannerY, (int)sw, (int)titleBannerH, Fade(BLACK, 0.6f));
    const char* title = "How To Play";
    int titleW = MeasureText(title, titleSz);
    DrawText(title, (int)(sw / 2.f - titleW / 2.f), (int)(titleBannerY + sh * 0.012f), titleSz, YELLOW);

    // ── Layout anchors ───────────────────────────────────────────────────────
    const float contentY  = titleBannerY + titleBannerH + sh * 0.025f;
    const float dividerX  = sw * 0.50f;              // centre divider
    const float gap       = sw * 0.035f;             // gap either side of divider
    const float colL      = sw * 0.03f;              // left col start
    const float iconCX    = dividerX + gap * 0.6f;   // icon centre — well clear of the line
    const float colRText  = dividerX + gap + 28.f;   // right col text start
    const float rowGap    = sh * 0.090f;

    // ── Divider ─────────────────────────────────────────────────────────────
    DrawLineEx({ dividerX, contentY }, { dividerX, sh - sh * 0.09f }, 2.f, Fade(WHITE, 0.30f));

    // ── LEFT column — Controls ───────────────────────────────────────────────
    float rowY = contentY;
    DrawText("CONTROLS", (int)colL, (int)rowY, headerSz, ORANGE);
    rowY += headerSz + sh * 0.018f;

    struct CtrlEntry { const char* key; const char* desc; };
    CtrlEntry controls[] = {
        { "W A S D",         "Move"                        },
        { "SPACE",           "Dash  (brief invincibility)" },
        { "Left Click",      "Melee attack"                },
        { "1 / 2 / 3 / 4",  "Use learned ability"         },
        { "Scroll",          "Cycle active ability slot"   },
        { "ESC",             "Pause / unpause"             },
    };

    for (auto& c : controls)
    {
        int kw = MeasureText(c.key, labelSz);
        float badgeH = (float)labelSz + 10.f;
        DrawRectangleRounded({ colL, rowY - 4.f, (float)kw + 18.f, badgeH },
            0.3f, 4, Fade(BLACK, 0.7f));
        DrawRectangleRoundedLines({ colL, rowY - 4.f, (float)kw + 18.f, badgeH },
            0.3f, 4, Fade(WHITE, 0.5f));
        DrawText(c.key,  (int)colL + 9, (int)rowY, labelSz, WHITE);
        DrawText(c.desc, (int)(colL + kw + 30.f), (int)rowY, descSz, LIGHTGRAY);
        rowY += rowGap * 0.82f;
    }

    // EXP section below controls
    rowY += sh * 0.01f;
    DrawText("EXP & LEVELS", (int)colL, (int)rowY, headerSz, ORANGE);
    rowY += headerSz + sh * 0.012f;
    DrawText("Kill enemies to earn EXP and level up.",                   (int)colL, (int)rowY, descSz, LIGHTGRAY); rowY += descSz + 6;
    DrawText("Choose 1 of 3 upgrade cards each level.",                  (int)colL, (int)rowY, descSz, LIGHTGRAY); rowY += descSz + 6;
    DrawText("EXP threshold doubles each level (10, 20, 40\xE2\x80\xA6)", (int)colL, (int)rowY, descSz, LIGHTGRAY); rowY += descSz + 6;
    DrawText("Max level: 20.",                                           (int)colL, (int)rowY, descSz, LIGHTGRAY);

    // ── RIGHT column — Pickups & Enemies ─────────────────────────────────────
    float rowR = contentY;
    DrawText("PICKUPS & ENEMIES", (int)colRText, (int)rowR, headerSz, ORANGE);
    rowR += headerSz + sh * 0.018f;

    struct PickupEntry { const char* name; const char* desc; int shape; };
    PickupEntry entries[] = {
        // shape 0 = mana gem (blue/purple), 1 = heal (red cross), 2 = enemy blob
        { "Mana Gem",  "Restores mana. Cast abilities with 1–4.",  0 },
        { "Heal",      "Restores 1 HP.",                           1 },
        { "Enemy",     "Chases you. Drops a pickup on death.",     2 },
    };

    for (auto& e : entries)
    {
        float cy = rowR + labelSz * 0.5f;

        // Icon — drawn at iconCX, well to the right of the divider
        if (e.shape == 0)
        {
            // Mana gem — blue/purple
            DrawCircleV({ iconCX, cy }, 20.f, Fade(PURPLE,  0.45f));
            DrawCircleV({ iconCX, cy }, 13.f, Fade(SKYBLUE, 0.85f));
            DrawCircleV({ iconCX, cy },  5.f, Fade(WHITE,   0.90f));
        }
        else if (e.shape == 1)
        {
            // Heal — red cross
            DrawCircleV({ iconCX, cy }, 20.f, Fade(RED,  0.35f));
            DrawCircleV({ iconCX, cy }, 14.f, Fade(RED,  0.85f));
            DrawCircleV({ iconCX, cy },  6.f, Fade(PINK, 0.90f));
            DrawLineEx({ iconCX - 8.f, cy }, { iconCX + 8.f, cy }, 3.f, WHITE);
            DrawLineEx({ iconCX, cy - 8.f }, { iconCX, cy + 8.f }, 3.f, WHITE);
        }
        else
        {
            // Enemy
            DrawCircleV({ iconCX, cy }, 20.f, Fade(MAROON, 0.7f));
            DrawCircleV({ iconCX, cy - 7.f }, 10.f, Fade(RED,    0.85f));
            DrawCircleV({ iconCX, cy + 9.f }, 13.f, Fade(MAROON, 0.9f));
        }

        // Name + desc to the right of the icon
        float textX = iconCX + 28.f;
        DrawText(e.name, (int)textX, (int)rowR, labelSz, WHITE);
        DrawText(e.desc, (int)textX, (int)(rowR + labelSz + 4), descSz, LIGHTGRAY);

        rowR += rowGap * 1.05f;
    }

    // ── Back button ──────────────────────────────────────────────────────────
    const float btnW = sw * 0.13f;
    const float btnH = sh * 0.055f;
    const float btnX = sw / 2.f - btnW / 2.f;
    const float btnY = sh - btnH - sh * 0.018f;

    Rectangle backBtn{ btnX, btnY, btnW, btnH };
    bool hovered = CheckCollisionPointRec(GetMousePosition(), backBtn);

    DrawRectangleRounded(backBtn, 0.3f, 6, hovered ? Fade(GRAY, 0.9f) : Fade(DARKGRAY, 0.85f));
    DrawRectangleRoundedLines(backBtn, 0.3f, 6, Fade(WHITE, 0.5f));

    const char* backLabel = (_howToPlayFrom == GameState::Pause) ? "Resume Game" : "< Back";
    int backLabelSz = (int)(sh * 0.030f);
    int backW = MeasureText(backLabel, backLabelSz);
    DrawText(backLabel,
        (int)(btnX + btnW / 2.f - backW / 2.f),
        (int)(btnY + btnH / 2.f - backLabelSz / 2.f),
        backLabelSz, WHITE);

    if (hovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
    {
        StopSound(_buttonPressSound); PlaySound(_buttonPressSound);
        if (_howToPlayFrom == GameState::Menu)
            _menu.Init();
        _gameState = _howToPlayFrom;
    }
}

void Engine::DrawMiniMap()
{
    const float mapW = _map.width  * _mapScale;
    const float mapH = _map.height * _mapScale;

    const float miniW = 160.f;
    const float miniH = miniW * (mapH / mapW);
    const float padding = 12.f;
    const float originX = padding;
    const float originY = 112.f;

    // Background
    DrawRectangleRounded(Rectangle{ originX - 2.f, originY - 2.f, miniW + 4.f, miniH + 4.f },
        0.08f, 4, Fade(BLACK, 0.85f));
    DrawRectangle((int)originX, (int)originY, (int)miniW, (int)miniH, Fade(DARKGRAY, 0.5f));

    auto toMini = [&](Vector2 worldPos) -> Vector2 {
        return Vector2{
            originX + (worldPos.x / mapW) * miniW,
            originY + (worldPos.y / mapH) * miniH
        };
    };

    // Enemy dots — red
    for (const auto& enemy : _enemies)
    {
        if (!enemy->IsActive())
            continue;
        if (!enemy->IsAlive())
            continue;

        Vector2 dot = toMini(enemy->GetWorldPos());
        if (enemy->AsMolarbeast() != nullptr)
            DrawCircleV(dot, 6.f, Fade(MAROON, 0.95f));
        else if (enemy->AsCyclops() != nullptr)
            DrawCircleV(dot, 4.f, Fade(ORANGE, 0.9f));
        else if (enemy->AsOgre() != nullptr)
            DrawCircleV(dot, 4.f, Fade(BROWN, 0.9f));
        else
            DrawCircleV(dot, 3.f, Fade(RED, 0.9f));
    }

    // Cyclops dots — orange

    // Pickup dots — green for all active pickups
    for (const auto& pickup : _pickups)
    {
        if (!pickup->IsActive())
            continue;

        Vector2 dot = toMini(pickup->GetWorldPos());
        DrawCircleV(dot, 4.f, Fade(GREEN, 0.9f));
    }

    // Player dot — yellow
    Vector2 playerDot = toMini(_player.GetWorldPos());
    DrawCircleV(playerDot, 5.f, Fade(YELLOW, 1.0f));

    DrawRectangleLinesEx(Rectangle{ originX, originY, miniW, miniH }, 1.f, Fade(LIGHTGRAY, 0.6f));
}

Vector2 Engine::GetRandomPropPosition()
{
    float mapW = _map.width * _mapScale;
    float mapH = _map.height * _mapScale;
    Vector2 playerStart{ mapW * 0.5f, mapH * 0.5f };

    float minX = mapW * (_currentBiome == Biome::Forest ? 0.13f : 0.05f);
    float maxX = mapW * (_currentBiome == Biome::Forest ? 0.87f : 0.95f);
    float minY = mapH * (_currentBiome == Biome::Forest ? 0.13f : 0.05f);
    float maxY = mapH * (_currentBiome == Biome::Forest ? 0.80f : 0.85f);

    float minSpacing = 308.f;
    float playerSafeRadius = 320.f;
    int attempts = 0;
    const int maxAttempts = 50;

    while (attempts < maxAttempts)
    {
        Vector2 pos{};
        pos.x = (float)GetRandomValue((int)minX, (int)maxX);
        pos.y = (float)GetRandomValue((int)minY, (int)maxY);

        bool tooClose = false;

        if (Vector2Distance(pos, playerStart) < playerSafeRadius)
            tooClose = true;

        for (auto& prop : _props)
        {
            if (Vector2Distance(pos, prop.GetWorldPos()) < minSpacing)
            {
                tooClose = true;
                break;
            }
        }

        if (!tooClose)
            return pos;

        attempts++;
    }

    Vector2 fallback{};
    fallback.x = (float)GetRandomValue((int)minX, (int)maxX);
    fallback.y = (float)GetRandomValue((int)minY, (int)maxY);
    return fallback;
}

Biome Engine::GetBiomeForWave(int wave) const
{
    // Legacy — superseded by GetBiomeForAct(). Kept to avoid linker errors
    // if any call sites remain outside the main progression path.
    (void)wave;
    return GetBiomeForAct(_currentAct);
}

const char* Engine::GetBiomeName(Biome biome) const
{
    return biome == Biome::Forest ? "Forest" : "Dungeon";
}

void Engine::PopulatePropsForBiome(Biome biome)
{
    _props.clear();

    if (biome == Biome::Forest)
    {
        // Forest props are all solid blockers, so a mixed random rotation is
        // enough to make each pass through the biome read differently without
        // changing the existing prop/pathfinding systems.
        int propCount = GetRandomValue(9, 13);
        for (int i = 0; i < propCount; ++i)
        {
            Vector2 pos = GetRandomPropPosition();
            int choice = GetRandomValue(0, 3);
            if (choice == 0)
            {
                _props.push_back(Prop{ pos, _treeTex, 1, 0, 0, 6.f });
                _props.back().SetCollisionTopFraction(0.25f);
                _props.back().SetCollisionSideFraction(0.30f);
            }
            else if (choice == 1)
            {
                _props.push_back(Prop{ pos, _smallTreeTex, 1, 0, 0, 6.f });
                _props.back().SetCollisionTopFraction(0.25f);
                _props.back().SetCollisionSideFraction(0.30f);
            }
            else if (choice == 2)
            {
                _props.push_back(Prop{ pos, _rockTex, 1, 0, 0, 5.f });
            }
            else
            {
                _props.push_back(Prop{ pos, _bigRockTex, 1, 0, 0, 4.f });
            }
        }
        return;
    }

    // Dungeon keeps the original authored mix of pillars and torches.
    int propCount = GetRandomValue(7, 10);
    int pillarCount = propCount - 3;

    for (int i = 0; i < pillarCount; ++i)
    {
        Vector2 pos = GetRandomPropPosition();
        _props.push_back(Prop{ pos, _pillarTex });
    }
    for (int i = 0; i < 2; ++i)
    {
        Vector2 pos = GetRandomPropPosition();
        _props.push_back(Prop{ pos, _torchTex, 8, 32, 29, 2.5f });
    }

    Vector2 pos = GetRandomPropPosition();
    _props.push_back(Prop{ pos, _pillarTorchTex, 8, 32, 52, 3.f, 1.f / 10.f, 17 });
}

void Engine::ApplyBiome(Biome biome)
{
    // The active map swaps between dungeon and forest. ForestMap is authored at
    // half the pixel dimensions of the dungeon map, so it uses 2x the scale to
    // preserve the same playable world size and camera feel.
    if (_navRefreshJob.valid())
        _navRefreshJob.wait();
    _navRefreshInFlight = false;
    _lastPlayerNavIndex = -1;

    if (_map.id != 0)
        UnloadTexture(_map);

    if (biome == Biome::Forest)
    {
        _map = LoadTexture(AssetPath("ForestLevel/ForestMap.png").c_str());
        _mapScale = 6.f;
    }
    else
    {
        _map = LoadTexture(AssetPath("TileSet/Map.png").c_str());
        _mapScale = 3.f;
    }

    _currentBiome = biome;
    PopulatePropsForBiome(biome);
    BuildNavigationGrid();

    // Biome swaps happen between waves, so recentering the player gives them
    // a clean neutral start in the new arena before enemies spawn in.
    _player.SetWorldPos(Vector2{ _map.width * _mapScale * 0.5f, _map.height * _mapScale * 0.5f });
    _navRefreshTimer = 0.f;
}

void Engine::UpdateBiomeTransition(float dt)
{
    if (!_biomeTransitionActive)
        return;

    float totalDuration = kBiomeFadeOutDuration + kBiomeFadeInDuration;
    float previousElapsed = totalDuration - _biomeTransitionTimer;
    _biomeTransitionTimer = std::max(0.f, _biomeTransitionTimer - dt);
    float currentElapsed = totalDuration - _biomeTransitionTimer;

    if (!_biomeTransitionSwapped &&
        previousElapsed < kBiomeFadeOutDuration &&
        currentElapsed >= kBiomeFadeOutDuration)
    {
        ApplyBiome(_pendingBiome);
        RefreshNavigationField();
        if (_navRefreshJob.valid())
            _navRefreshJob.wait();
        ApplyCompletedNavigationRefresh();
        _biomeTransitionSwapped = true;
    }

    if (_biomeTransitionTimer <= 0.f)
    {
        _biomeTransitionActive = false;
        _biomeTransitionSwapped = false;
    }
}

float Engine::GetBiomeTransitionAlpha() const
{
    if (!_biomeTransitionActive)
        return 0.f;

    float totalDuration = kBiomeFadeOutDuration + kBiomeFadeInDuration;
    float elapsed = totalDuration - _biomeTransitionTimer;
    if (elapsed < kBiomeFadeOutDuration)
        return elapsed / kBiomeFadeOutDuration;

    float fadeInElapsed = elapsed - kBiomeFadeOutDuration;
    return 1.f - std::min(1.f, fadeInElapsed / kBiomeFadeInDuration);
}

void Engine::BuildNavigationGrid()
{
    float mapW = _map.width * _mapScale;
    float mapH = _map.height * _mapScale;

    _navCols = std::max(1, (int)std::ceil(mapW / _navCellSize));
    _navRows = std::max(1, (int)std::ceil(mapH / _navCellSize));
    _navBlocked.assign(_navCols * _navRows, false);
    _navDistance.assign(_navCols * _navRows, std::numeric_limits<int>::max());

    for (int row = 0; row < _navRows; ++row)
    {
        for (int col = 0; col < _navCols; ++col)
        {
            Rectangle cellRect{
                col * _navCellSize,
                row * _navCellSize,
                _navCellSize,
                _navCellSize
            };

            for (auto& prop : _props)
            {
                // Inflate the prop's collision rect by half a nav cell on every
                // side. This ensures enemies route around props with a clearance
                // margin rather than hugging the edge of the hitbox and wedging.
                Rectangle rec = prop.GetCollisionRec();
                const float pad = _navCellSize * 0.5f;
                Rectangle inflated{ rec.x - pad, rec.y - pad,
                                    rec.width + pad * 2.f, rec.height + pad * 2.f };
                if (CheckCollisionRecs(cellRect, inflated))
                {
                    _navBlocked[GetNavigationIndex(col, row)] = true;
                    break;
                }
            }
        }
    }
}

bool Engine::IsNavigationCellBlocked(int col, int row) const
{
    if (col < 0 || col >= _navCols || row < 0 || row >= _navRows)
        return true;
    return _navBlocked[GetNavigationIndex(col, row)];
}

int Engine::GetNavigationIndex(int col, int row) const
{
    return row * _navCols + col;
}

bool Engine::FindNearestOpenCell(int& col, int& row) const
{
    if (!IsNavigationCellBlocked(col, row))
        return true;

    for (int radius = 1; radius < std::max(_navCols, _navRows); ++radius)
    {
        for (int dc = -radius; dc <= radius; ++dc)
        {
            for (int dr = -radius; dr <= radius; ++dr)
            {
                if (std::abs(dc) != radius && std::abs(dr) != radius)
                    continue;

                int nc = col + dc;
                int nr = row + dr;

                if (!IsNavigationCellBlocked(nc, nr))
                {
                    col = nc;
                    row = nr;
                    return true;
                }
            }
        }
    }

    return false;
}

int Engine::GetClosestOpenNavigationIndex(int col, int row) const
{
    int nextCol = ClampInt(col, 0, std::max(0, _navCols - 1));
    int nextRow = ClampInt(row, 0, std::max(0, _navRows - 1));

    if (!FindNearestOpenCell(nextCol, nextRow))
        return -1;

    return GetNavigationIndex(nextCol, nextRow);
}

bool Engine::HasLineOfSight(Vector2 start, Vector2 end) const
{
    Vector2 dir = Vector2Subtract(end, start);
    float dist = Vector2Length(dir);

    if (dist < 0.01f)
        return true;

    dir = Vector2Normalize(dir);

    // Perpendicular offset for tube sampling — wide enough to catch an enemy
    // approaching at an angle whose body overlaps a blocked cell that the
    // center line just misses.
    Vector2 perp{ -dir.y, dir.x };
    const float tubeHalfWidth = _navCellSize * 0.4f;
    float step = _navCellSize * 0.5f;

    for (float t = 0.f; t < dist; t += step)
    {
        Vector2 center = Vector2Add(start, Vector2Scale(dir, t));

        // Check center plus left and right offsets
        for (float side : { 0.f, tubeHalfWidth, -tubeHalfWidth })
        {
            Vector2 point = Vector2Add(center, Vector2Scale(perp, side));
            int col = (int)(point.x / _navCellSize);
            int row = (int)(point.y / _navCellSize);
            if (IsNavigationCellBlocked(col, row))
                return false;
        }
    }

    return true;
}

Vector2 Engine::GetNavigationTarget(Vector2 startWorldPos, Vector2 targetWorldPos) const
{
    if (_navCols <= 0 || _navRows <= 0)
        return targetWorldPos;

    int startCol = ClampInt((int)(startWorldPos.x / _navCellSize), 0, _navCols - 1);
    int startRow = ClampInt((int)(startWorldPos.y / _navCellSize), 0, _navRows - 1);

    if (!FindNearestOpenCell(startCol, startRow))
        return targetWorldPos;

    static const std::array<std::pair<int,int>, 8> offsets{{
        {1,0},{-1,0},{0,1},{0,-1},{1,1},{1,-1},{-1,1},{-1,-1}
    }};

    int startIndex = GetNavigationIndex(startCol, startRow);
    int bestIndex  = startIndex;
    int bestCost   = _navDistance[startIndex];

    for (int i = 0; i < (int)offsets.size(); ++i)
    {
        int dc = offsets[i].first;
        int dr = offsets[i].second;
        int nc = startCol + dc;
        int nr = startRow + dr;

        if (IsNavigationCellBlocked(nc, nr))
            continue;

        if (dc != 0 && dr != 0)
        {
            if (IsNavigationCellBlocked(startCol + dc, startRow) ||
                IsNavigationCellBlocked(startCol, startRow + dr))
                continue;
        }

        int nextIndex = GetNavigationIndex(nc, nr);
        int nextCost  = _navDistance[nextIndex];

        if (nextCost < bestCost)
        {
            bestCost  = nextCost;
            bestIndex = nextIndex;
        }
    }

    if (bestIndex == startIndex || bestCost == std::numeric_limits<int>::max())
        return targetWorldPos;

    int nextCol = bestIndex % _navCols;
    int nextRow = bestIndex / _navCols;

    return Vector2{
        nextCol * _navCellSize + _navCellSize * 0.5f,
        nextRow * _navCellSize + _navCellSize * 0.5f
    };
}

Vector2 Engine::GetAStarTarget(Vector2 startWorldPos, Vector2 targetWorldPos) const
{
    if (_navCols <= 0 || _navRows <= 0)
        return targetWorldPos;

    int startCol = ClampInt((int)(startWorldPos.x / _navCellSize), 0, _navCols - 1);
    int startRow = ClampInt((int)(startWorldPos.y / _navCellSize), 0, _navRows - 1);
    int goalCol  = ClampInt((int)(targetWorldPos.x / _navCellSize), 0, _navCols - 1);
    int goalRow  = ClampInt((int)(targetWorldPos.y / _navCellSize), 0, _navRows - 1);

    // Snap start and goal to the nearest open cells in case they sit inside a blocked tile.
    if (!FindNearestOpenCell(startCol, startRow))
        return targetWorldPos;
    FindNearestOpenCell(goalCol, goalRow);

    int startIdx = GetNavigationIndex(startCol, startRow);
    int goalIdx  = GetNavigationIndex(goalCol,  goalRow);

    if (startIdx == goalIdx)
        return targetWorldPos;

    const int total = _navCols * _navRows;

    struct Node
    {
        int f, g, idx;
        bool operator>(const Node& o) const { return f > o.f; }
    };

    std::vector<int>  gScore(total, INT_MAX);
    std::vector<int>  parent(total, -1);
    std::vector<bool> closed(total, false);
    std::priority_queue<Node, std::vector<Node>, std::greater<Node>> open;

    // Octile heuristic (integer scaled x10) — admissible for 8-directional movement.
    auto heuristic = [&](int col, int row) -> int
    {
        int dc = std::abs(col - goalCol);
        int dr = std::abs(row - goalRow);
        return 10 * (dc + dr) + (14 - 20) * std::min(dc, dr);  // = 10*(dc+dr) - 6*min
    };

    gScore[startIdx] = 0;
    open.push({ heuristic(startCol, startRow), 0, startIdx });

    static constexpr int dc[] = { 1, -1,  0,  0,  1,  1, -1, -1 };
    static constexpr int dr[] = { 0,  0,  1, -1,  1, -1,  1, -1 };
    static constexpr int cost[]= {10, 10, 10, 10, 14, 14, 14, 14 };

    bool found = false;
    while (!open.empty())
    {
        Node cur = open.top();
        open.pop();

        if (cur.idx == goalIdx) { found = true; break; }
        if (closed[cur.idx]) continue;
        closed[cur.idx] = true;

        int col = cur.idx % _navCols;
        int row = cur.idx / _navCols;

        for (int i = 0; i < 8; ++i)
        {
            int nc = col + dc[i];
            int nr = row + dr[i];
            if (nc < 0 || nc >= _navCols || nr < 0 || nr >= _navRows) continue;

            int nIdx = GetNavigationIndex(nc, nr);
            if (_navBlocked[nIdx] || closed[nIdx]) continue;

            // For diagonals, require both shared cardinal neighbours to be open
            // so the path doesn't clip through prop corners.
            if (i >= 4 && (IsNavigationCellBlocked(col + dc[i], row) ||
                           IsNavigationCellBlocked(col, row + dr[i])))
                continue;

            int tentG = gScore[cur.idx] + cost[i];
            if (tentG < gScore[nIdx])
            {
                parent[nIdx] = cur.idx;
                gScore[nIdx] = tentG;
                open.push({ tentG + heuristic(nc, nr), tentG, nIdx });
            }
        }
    }

    if (!found)
        return targetWorldPos;

    // Walk the parent chain back to find the first step after the start cell.
    int step = goalIdx;
    while (parent[step] != -1 && parent[step] != startIdx)
        step = parent[step];

    if (parent[step] == -1)
        return targetWorldPos;

    int stepCol = step % _navCols;
    int stepRow = step / _navCols;

    return Vector2{
        stepCol * _navCellSize + _navCellSize * 0.5f,
        stepRow * _navCellSize + _navCellSize * 0.5f
    };
}

void Engine::RefreshNavigationField()
{
    if (_navCols <= 0 || _navRows <= 0 || _navRefreshInFlight)
        return;

    Vector2 playerFeet = _player.GetFeetWorldPos();
    std::vector<bool> blockedCopy = _navBlocked;
    int cols = _navCols;
    int rows = _navRows;
    float cellSize = _navCellSize;

#ifdef __EMSCRIPTEN__
    // The first web build keeps navigation single-threaded so it does not rely
    // on browser pthread support. The desktop build still uses the async job.
    NavigationRefreshResult result = BuildNavigationRefreshResult(blockedCopy, cols, rows, cellSize, playerFeet);
    _navDistance = std::move(result.distanceField);
    _lastPlayerNavIndex = result.playerNavIndex;
#else
    // Build the shared flow field off-thread using copies of the immutable
    // grid data for this refresh. Enemy movement still consumes the finished
    // field on the main thread, so gameplay mutation stays single-threaded.
    _navRefreshJob = std::async(std::launch::async,
        [blocked = std::move(blockedCopy), cols, rows, cellSize, playerFeet]() mutable
        {
            return BuildNavigationRefreshResult(blocked, cols, rows, cellSize, playerFeet);
        });
    _navRefreshInFlight = true;
#endif
}

void Engine::ApplyCompletedNavigationRefresh()
{
#ifdef __EMSCRIPTEN__
    return;
#else
    if (!_navRefreshInFlight || !_navRefreshJob.valid())
        return;

    if (_navRefreshJob.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
        return;

    NavigationRefreshResult result = _navRefreshJob.get();
    _navDistance = std::move(result.distanceField);
    _lastPlayerNavIndex = result.playerNavIndex;
    _navRefreshInFlight = false;
#endif
}

void Engine::RespawnOutOfBoundsEnemies()
{
    float mapW = _map.width  * _mapScale;
    float mapH = _map.height * _mapScale;
    const float hardMargin = 60.f;   // enemy is truly outside if beyond this

    for (auto& enemy : _enemies)
    {
        if (!enemy->IsActive())
            continue;
        if (enemy->IsDying() || !enemy->IsAlive())
            continue;

        Vector2 pos = enemy->GetWorldPos();
        bool outOfBounds = pos.x < hardMargin || pos.y < hardMargin ||
                           pos.x > mapW - hardMargin || pos.y > mapH - hardMargin;

        if (!outOfBounds)
            continue;

        // Find a valid position away from props, other enemies, and the player
        Vector2 newPos{};
        bool found = false;

        for (int attempt = 0; attempt < 50; ++attempt)
        {
            newPos.x = (float)GetRandomValue(300, (int)(mapW - 300));
            newPos.y = (float)GetRandomValue(300, (int)(mapH - 300));

            if (!IsSpawnPositionValid(newPos))
                continue;

            // Also keep a safe distance from the player
            if (Vector2Distance(newPos, _player.GetWorldPos()) < 300.f)
                continue;

            found = true;
            break;
        }

        if (found)
            enemy->Teleport(newPos);
    }
}

bool Engine::IsSpawnPositionValid(Vector2 pos)
{
    const float safeDistance = 200.f;
    const float playerSafeDistance = 260.f;

    // Reject if the spawn cell or any neighbour is blocked by a prop on the nav grid
    int col = (int)(pos.x / _navCellSize);
    int row = (int)(pos.y / _navCellSize);
    for (int dc = -1; dc <= 1; dc++)
        for (int dr = -1; dr <= 1; dr++)
            if (IsNavigationCellBlocked(col + dc, row + dr))
                return false;

    for (auto& prop : _props)
    {
        if (Vector2Distance(pos, prop.GetWorldPos()) < safeDistance)
            return false;
    }

    for (auto& enemy : _enemies)
    {
        if (!enemy->IsActive())
            continue;
        if (Vector2Distance(pos, enemy->GetWorldPos()) < safeDistance)
            return false;
    }

    // Keep enemy, boss, and timed pickup spawns away from the player so
    // nothing materializes directly on top of them.
    if (Vector2Distance(pos, _player.GetWorldPos()) < playerSafeDistance)
        return false;

    return true;
}

void Engine::ResetRunState()
{
    if (_navRefreshJob.valid())
        _navRefreshJob.wait();

    _navRefreshInFlight = false;
    _wave          = 0;
    _enemiesKilled = 0;
    _navRefreshTimer = 0.f;
    _lastPlayerNavIndex = -1;
    _gameTimer = 0.f;
    _playerDying = false;
    _awaitingNameEntry = false;
    _awaitingStartingAbility = false;
    _waveStarting        = true;
    _wave1LevelUpDone    = false;
    _bossWarningTimer    = 0.f;
    _biomeTransitionActive = false;
    _biomeTransitionSwapped = false;
    _biomeTransitionTimer = 0.f;
    _ultimatePhase       = UltimatePhase::None;
    _ultimatePhaseTimer  = 0.f;
    _ultimateCircleAngle = 0.f;
    _showUltimateRow     = false;
    _ultimateRowPicked   = false;
    _regularRowPicked    = false;
    _lastAbilityChoiceWave    = -1;
    _abilityChoiceSwapPending = false;
    _abilityChoiceOptionCount = 0;

    // ── Room / act progression reset ──────────────────────────────────────
    _currentAct        = 1;
    _currentRoom       = 0;
    _currentRoomType   = RoomType::Standard;
    _pendingRoomChoice  = false;
    _roomClearPending   = false;
    _roomClearTimer     = 0.f;
    _currentMapNodeIdx  = -1;
    _mapKeySelectedIdx  = -1;
    _mapOpenTimer       = 0.f;
    _actMap.clear();

    // Random starting biome so the player doesn't always begin in the same world.
    _startBiomeDungeon = (GetRandomValue(0, 1) == 0);

    _spreadProjectiles.clear();
    _ultimateBlasts.clear();
    _lavaBalls.clear();
    _cyclopsLasers.clear();
    _pickups.clear();
    _effects.clear();
    _player.Init();
    _currentBiome = GetBiomeForAct(1);   // no initial biome transition
    _pendingBiome = _currentBiome;
    ApplyBiome(_currentBiome);
    _bossCyclopsSupport = {};
    _bossOgreSupport = {};

    for (auto& enemy : _enemies)
    {
        enemy->SetActive(false);
        enemy->Teleport(Vector2{ -5000.f, -5000.f });
    }

    _pickupSpawnTimer = kDefaultTimedPickupInterval;

    RefreshNavigationField();
    if (_navRefreshJob.valid())
        _navRefreshJob.wait();
    ApplyCompletedNavigationRefresh();
    GenerateActMap();  // builds the full act 1 node map; player clicks to start
}

int Engine::GetActiveEnemyCount() const
{
    int count = 0;
    for (const auto& enemy : _enemies)
    {
        if (enemy->IsActive())
            count++;
    }

    return count;
}

bool Engine::IsBossFightActive() const
{
    for (const auto& enemy : _enemies)
    {
        if (!enemy->IsActive())
            continue;
        if (!enemy->IsAlive())
            continue;
        if (enemy->AsMolarbeast() != nullptr)
            return true;
    }

    return false;
}

bool Engine::TryGetFarSpawnPosition(Vector2& pos, float minPlayerDistance)
{
    float mapW = _map.width * _mapScale;
    float mapH = _map.height * _mapScale;

    for (int attempt = 0; attempt < 60; ++attempt)
    {
        Vector2 candidate{
            (float)GetRandomValue(300, (int)mapW - 300),
            (float)GetRandomValue(300, (int)mapH - 300)
        };

        if (!IsSpawnPositionValid(candidate))
            continue;
        if (Vector2Distance(candidate, _player.GetWorldPos()) < minPlayerDistance)
            continue;

        pos = candidate;
        return true;
    }

    return false;
}

void Engine::SpawnBossSupportAdds()
{
    // Cyclops arrives immediately — ranged pressure from a distance is readable.
    Vector2 cyclopsPos{};
    if (TryGetFarSpawnPosition(cyclopsPos, kBossSupportMinPlayerDistance))
        _bossCyclopsSupport.enemy = SpawnCyclops(cyclopsPos);
    _bossCyclopsSupport.respawnTimer = 0.f;

    // Ogre is held back so the player gets ~22s to learn the boss before a
    // second melee threat joins. The pending-spawn path in UpdateBossSupportRespawns
    // handles the null-enemy timer countdown.
    _bossOgreSupport.enemy = nullptr;
    _bossOgreSupport.respawnTimer = kBossOgreInitialDelay;
}

void Engine::ClearBossSupportAdds()
{
    if (_bossCyclopsSupport.enemy != nullptr)
    {
        _bossCyclopsSupport.enemy->SetActive(false);
        _bossCyclopsSupport.enemy->Teleport(Vector2{ -5000.f, -5000.f });
    }

    if (_bossOgreSupport.enemy != nullptr)
    {
        _bossOgreSupport.enemy->SetActive(false);
        _bossOgreSupport.enemy->Teleport(Vector2{ -5000.f, -5000.f });
    }

    _bossCyclopsSupport = {};
    _bossOgreSupport = {};
}

void Engine::UpdateBossSupportRespawns(float dt)
{
    if (!IsBossFightActive())
    {
        ClearBossSupportAdds();
        return;
    }

    if (_bossCyclopsSupport.enemy != nullptr &&
        !_bossCyclopsSupport.enemy->IsActive() &&
        _bossCyclopsSupport.respawnTimer > 0.f)
    {
        _bossCyclopsSupport.respawnTimer -= dt;
        if (_bossCyclopsSupport.respawnTimer <= 0.f)
        {
            Vector2 spawnPos{};
            if (TryGetFarSpawnPosition(spawnPos, kBossSupportMinPlayerDistance))
                _bossCyclopsSupport.enemy = SpawnCyclops(spawnPos);
            _bossCyclopsSupport.respawnTimer = 0.f;
        }
    }

    // Ogre: handles both the initial delayed entry (enemy == nullptr) and
    // subsequent respawns after it has been killed.
    bool ogrePendingSpawn = (_bossOgreSupport.enemy == nullptr && _bossOgreSupport.respawnTimer > 0.f);
    bool ogrePendingRespawn = (_bossOgreSupport.enemy != nullptr && !_bossOgreSupport.enemy->IsActive() && _bossOgreSupport.respawnTimer > 0.f);
    if (ogrePendingSpawn || ogrePendingRespawn)
    {
        _bossOgreSupport.respawnTimer -= dt;
        if (_bossOgreSupport.respawnTimer <= 0.f)
        {
            Vector2 spawnPos{};
            if (TryGetFarSpawnPosition(spawnPos, kBossSupportMinPlayerDistance))
                _bossOgreSupport.enemy = SpawnOgre(spawnPos);
            _bossOgreSupport.respawnTimer = 0.f;
        }
    }
}

bool Engine::TryGetPooledEnemySpawn(Vector2 pos)
{
    for (auto& enemy : _enemies)
    {
        if (enemy->IsActive())
            continue;
        if (enemy->AsCyclops() != nullptr)
            continue;
        if (enemy->AsOgre() != nullptr)
            continue;
        if (enemy->AsMolarbeast() != nullptr)
            continue;

        enemy->ResetForSpawn(pos);
        ConfigureSpawnedEnemy(*enemy);
        return true;
    }

    return false;
}

bool Engine::TryGetPooledCyclopsSpawn(Vector2 pos)
{
    for (auto& enemy : _enemies)
    {
        if (enemy->IsActive())
            continue;

        Cyclops* cyclops = enemy->AsCyclops();
        if (cyclops == nullptr)
            continue;

        cyclops->ResetForSpawn(pos);
        ConfigureSpawnedEnemy(*cyclops);
        return true;
    }

    return false;
}

int Engine::GetCyclopsSpawnCountForWave(int wave) const
{
    // Superseded by the curated wave table in SpawnEnemies — kept for
    // any legacy call sites that may still reference it.
    (void)wave;
    return 0;
}

bool Engine::TryGetPooledOgreSpawn(Vector2 pos)
{
    for (auto& enemy : _enemies)
    {
        if (enemy->IsActive())
            continue;

        Ogre* ogre = enemy->AsOgre();
        if (ogre == nullptr)
            continue;

        ogre->ResetForSpawn(pos);
        ConfigureSpawnedEnemy(*ogre);
        return true;
    }

    return false;
}

bool Engine::TryGetPooledMolarbeastSpawn(Vector2 pos)
{
    for (auto& enemy : _enemies)
    {
        if (enemy->IsActive())
            continue;

        Molarbeast* molarbeast = enemy->AsMolarbeast();
        if (molarbeast == nullptr)
            continue;

        molarbeast->ResetForSpawn(pos);
        ConfigureSpawnedEnemy(*molarbeast);
        return true;
    }

    return false;
}

int Engine::GetOgreSpawnCountForWave(int wave) const
{
    // Superseded by the curated wave table in SpawnEnemies — kept for
    // any legacy call sites that may still reference it.
    (void)wave;
    return 0;
}

int Engine::GetEnemyPowerLevelForWave(int wave) const
{
    // Power level advances every 10 waves so stat growth is slower and more
    // readable. Composition and behaviour scaling carry the early game feel.
    // waves  1-9:  power 1   waves 10-19: power 2
    // waves 20-29: power 3   etc.
    if (wave <= 0)
        return 1;

    return 1 + ((wave - 1) / 10);
}

void Engine::ConfigureSpawnedEnemy(Enemy& enemy)
{
    // All enemy types share the same spawn-tuning path:
    // 1. SetWaveScale — fixed base stats + behavioural timing for this room.
    // 2. ApplyEnemyPowerLevel — single global multiplier, advances every 10 rooms.
    // 3. SetTarget — restore player pointer so pooled enemies rejoin the run.
    // _wave = total rooms entered this run, used here for scaling only.
    enemy.SetWaveScale(_wave);
    enemy.ApplyEnemyPowerLevel(GetEnemyPowerLevelForWave(_wave));
    enemy.SetTarget(&_player);
}

Enemy* Engine::SpawnCyclops(Vector2 pos)
{
    for (auto& enemy : _enemies)
    {
        if (enemy->IsActive())
            continue;

        Cyclops* cyclops = enemy->AsCyclops();
        if (cyclops == nullptr)
            continue;

        cyclops->ResetForSpawn(pos);
        ConfigureSpawnedEnemy(*cyclops);
        return cyclops;
    }

    // No pooled instance - create a new one.
    auto cyclops = std::make_unique<Cyclops>(pos);
    cyclops->Init();
    ConfigureSpawnedEnemy(*cyclops);
    Enemy* cyclopsPtr = cyclops.get();
    _enemies.push_back(std::move(cyclops));
    return cyclopsPtr;
}

Enemy* Engine::SpawnOgre(Vector2 pos)
{
    for (auto& enemy : _enemies)
    {
        if (enemy->IsActive())
            continue;

        Ogre* ogre = enemy->AsOgre();
        if (ogre == nullptr)
            continue;

        ogre->ResetForSpawn(pos);
        ConfigureSpawnedEnemy(*ogre);
        return ogre;
    }

    auto ogre = std::make_unique<Ogre>(pos);
    ogre->Init();
    ConfigureSpawnedEnemy(*ogre);
    Enemy* ogrePtr = ogre.get();
    _enemies.push_back(std::move(ogre));
    return ogrePtr;
}

void Engine::SpawnMolarbeast(Vector2 pos)
{
    if (TryGetPooledMolarbeastSpawn(pos))
    {
        _bossWarningTimer = 4.f;
        return;
    }

    auto molarbeast = std::make_unique<Molarbeast>(pos);
    molarbeast->Init();
    ConfigureSpawnedEnemy(*molarbeast);
    _enemies.push_back(std::move(molarbeast));
    _bossWarningTimer = 4.f;
}

// =============================================================================
void Engine::UpdateCyclopsLasers(float dt)
{
    for (auto& laser : _cyclopsLasers)
    {
        if (!laser.IsActive()) continue;

        laser.Update(dt);
        if (!laser.IsActive()) continue;

        // Destroyed by props
        for (auto& prop : _props)
        {
            if (CheckCollisionRecs(laser.GetCollisionRec(), prop.GetCollisionRec()))
            {
                laser.Destroy();
                break;
            }
        }

        if (!laser.IsActive()) continue;

        // Damages player on hit
        if (_player.IsAlive() &&
            CheckCollisionRecs(laser.GetCollisionRec(), _player.GetCollisionRec()))
        {
            int laserDmg = laser.GetDamage();
            _player.TakeDamage(laserDmg, laser.GetWorldPos());
            SpawnFloatingText(_player.GetWorldPos(), -laserDmg, RED);
            laser.Destroy();

            // Cyclops laser already has a strong visual read from the charge
            // glow and beam draw, so keep the hit shake lighter than melee and
            // explosive ability impacts to avoid overdriving the camera.
            TriggerScreenShake(2.5f, 0.07f);
        }
    }

    _cyclopsLasers.erase(
        std::remove_if(_cyclopsLasers.begin(), _cyclopsLasers.end(),
            [](const CyclopsLaserProjectile& l) { return !l.IsActive(); }),
        _cyclopsLasers.end());
}

void Engine::UpdateLavaBallProjectiles(float dt)
{
    const float mapW = _map.width * _mapScale;
    const float mapH = _map.height * _mapScale;
    const float marginLeft = 76.f;
    const float marginRight = 96.f;
    const float marginTop = 42.f;
    const float marginBottom = 320.f;

    for (auto& projectile : _lavaBalls)
    {
        if (!projectile.IsActive())
            continue;

        projectile.Update(dt);
        if (!projectile.IsActive())
            continue;

        Rectangle collisionRec = projectile.GetCollisionRec();

        // Arena wall and prop checks only matter while the ball is travelling.
        if (projectile.IsFlying())
        {
            bool hitArenaWall =
                collisionRec.x < marginLeft ||
                collisionRec.x + collisionRec.width > mapW - marginRight ||
                collisionRec.y < marginTop ||
                collisionRec.y + collisionRec.height > mapH - marginBottom;

            if (hitArenaWall)
            {
                projectile.BeginHit();
                StopSound(_lavaBallImpactSound);
                PlaySound(_lavaBallImpactSound);
                continue;
            }

            bool hitProp = false;
            for (const auto& prop : _props)
            {
                if (CheckCollisionRecs(collisionRec, prop.GetCollisionRec()))
                {
                    hitProp = true;
                    break;
                }
            }
            if (hitProp)
            {
                projectile.BeginHit();
                StopSound(_lavaBallImpactSound);
                PlaySound(_lavaBallImpactSound);
                continue;
            }
        }

        // Player collision — only register once per projectile (HasHitPlayer guard).
        if (_player.IsAlive() &&
            !projectile.HasHitPlayer() &&
            CheckCollisionRecs(collisionRec, _player.GetCollisionRec()))
        {
            static constexpr int kLavaBallDamage = 2;
            _player.TakeDamage(kLavaBallDamage, projectile.GetWorldPos());
            SpawnFloatingText(_player.GetWorldPos(), -kLavaBallDamage, RED);
            projectile.OnPlayerHit();
            if (projectile.IsFlying())
            {
                projectile.BeginHit();
                StopSound(_lavaBallImpactSound);
                PlaySound(_lavaBallImpactSound);
            }
        }
    }

    _lavaBalls.erase(
        std::remove_if(_lavaBalls.begin(), _lavaBalls.end(),
            [](const LavaBallProjectile& projectile) { return !projectile.IsActive(); }),
        _lavaBalls.end());
}

void Engine::SaveKeybindings()
{
    const KeyBindings& b = _player.GetBindings();
    FILE* f = nullptr;
    fopen_s(&f, "keybindings.cfg", "w");
    if (!f) return;
    std::fprintf(f, "moveUp %d\n",    (int)b.moveUp);
    std::fprintf(f, "moveDown %d\n",  (int)b.moveDown);
    std::fprintf(f, "moveLeft %d\n",  (int)b.moveLeft);
    std::fprintf(f, "moveRight %d\n", (int)b.moveRight);
    std::fprintf(f, "dash %d\n",      (int)b.dash);
    std::fprintf(f, "attack %d\n",    (int)b.attack);
    std::fprintf(f, "ability0 %d\n",  (int)b.ability[0]);
    std::fprintf(f, "ability1 %d\n",  (int)b.ability[1]);
    std::fprintf(f, "ability2 %d\n",  (int)b.ability[2]);
    std::fclose(f);
}

void Engine::LoadKeybindings()
{
    FILE* f = nullptr;
    fopen_s(&f, "keybindings.cfg", "r");
    if (!f) return;
    KeyBindings b;
    char key[32];
    int  value;
    while (fscanf_s(f, "%31s %d", key, (unsigned)sizeof(key), &value) == 2)
    {
        if      (std::strcmp(key, "moveUp")    == 0) b.moveUp     = (KeyboardKey)value;
        else if (std::strcmp(key, "moveDown")  == 0) b.moveDown   = (KeyboardKey)value;
        else if (std::strcmp(key, "moveLeft")  == 0) b.moveLeft   = (KeyboardKey)value;
        else if (std::strcmp(key, "moveRight") == 0) b.moveRight  = (KeyboardKey)value;
        else if (std::strcmp(key, "dash")      == 0) b.dash       = (KeyboardKey)value;
        else if (std::strcmp(key, "attack")    == 0) b.attack     = (KeyboardKey)value;
        else if (std::strcmp(key, "ability0")  == 0) b.ability[0] = (KeyboardKey)value;
        else if (std::strcmp(key, "ability1")  == 0) b.ability[1] = (KeyboardKey)value;
        else if (std::strcmp(key, "ability2")  == 0) b.ability[2] = (KeyboardKey)value;
    }
    std::fclose(f);
    _player.SetBindings(b);
}

// ─────────────────────────────────────────────────────────────────────────────
// Touch Controls
// ─────────────────────────────────────────────────────────────────────────────

// Returns the screen-space centre of touch-mode ability arc button for `slot`.
// Buttons are arranged in a quarter-circle arc above the ATK button.
// Screen angles: 270° = straight up, 210° = upper-left.
// Free helper — computes the bounding rect for touch-mode ability slot `slot`.
// 4 square slots in a right-aligned row, above the ATK/DASH buttons.
static Rectangle TouchAbilityRect(int slot, int screenW, int screenH)
{
    static constexpr float kTouchSlotSize = 96.f;
    static constexpr float kTouchSlotGap  = 16.f;
    static constexpr float kRightPad      = 24.f;
    const float slotY  = (float)screenH - TouchControls::kBtnBotPad
                         - TouchControls::kBtnRadius - 20.f - kTouchSlotSize;
    const float totalW = 4.f * kTouchSlotSize + 3.f * kTouchSlotGap;
    const float startX = (float)screenW - kRightPad - totalW;
    return { startX + (float)slot * (kTouchSlotSize + kTouchSlotGap),
             slotY, kTouchSlotSize, kTouchSlotSize };
}

// Sets player touch direction/attack/dash from _touch, then scans for new
// touches on the ability arc.  Called each gameplay frame when touch mode is on.
void Engine::UpdateTouchControls()
{
    _touch.Update(_windowWidth, _windowHeight);

    _player.SetTouchDirection(_touch.joystickDir);
    if (_touch.attackPressed) _player.SetTouchAttack();
    if (_touch.dashPressed)   _player.SetTouchDash();

    ScanAbilityArcTaps();

    // Pause button (top-right corner) — same rect as DrawHUD draws it
    static constexpr float kPauseW   = 90.f;
    static constexpr float kPauseH   = 48.f;
    static constexpr float kPausePad = 14.f;
    const Rectangle pauseRec{ (float)_windowWidth - kPauseW - kPausePad, kPausePad, kPauseW, kPauseH };

    const int tc = GetTouchPointCount();
    if (tc == 0)
    {
        // Mouse simulation
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) &&
            CheckCollisionPointRec(GetMousePosition(), pauseRec))
        {
            _gameState = GameState::Pause;
        }
    }
    else
    {
        // Real touch — check any new touch ID not already owned by other controls
        int joyId  = _touch.GetJoyTouchId();
        int atkId  = _touch.GetAtkTouchId();
        int dashId = _touch.GetDashTouchId();
        for (int i = 0; i < tc; i++)
        {
            int id = GetTouchPointId(i);
            if (id == joyId || id == atkId || id == dashId) continue;
            bool seen = false;
            for (int sid : _abilityTapSeenIds) if (sid == id) { seen = true; break; }
            if (!seen && CheckCollisionPointRec(GetTouchPosition(i), pauseRec))
            {
                _gameState = GameState::Pause;
                return;
            }
        }
    }
}

// Detects new touches on ability arc buttons and triggers the matching cast.
// Tracks seen touch IDs to avoid re-triggering on hold.
void Engine::ScanAbilityArcTaps()
{
    if (_waveStarting || _ultimatePhase != UltimatePhase::None)
        return;

    const int tc         = GetTouchPointCount();
    const int totalSlots = _player.GetMaxAbilitySlots();

    auto hitSlot = [&](Vector2 pos) -> int
    {
        for (int s = 0; s < totalSlots; s++)
            if (CheckCollisionPointRec(pos, TouchAbilityRect(s, _windowWidth, _windowHeight))) return s;
        return -1;
    };

    // ── Mouse simulation path ─────────────────────────────────────────────────
    if (tc == 0)
    {
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        {
            int slot = hitSlot(GetMousePosition());
            if (slot >= 0) _player.TriggerAbilityCast(slot);
        }
        return;
    }

    // ── Real touch path ───────────────────────────────────────────────────────
    // Remove IDs that have lifted
    _abilityTapSeenIds.erase(
        std::remove_if(_abilityTapSeenIds.begin(), _abilityTapSeenIds.end(),
            [tc](int id)
            {
                for (int i = 0; i < tc; i++)
                    if (GetTouchPointId(i) == id) return false;
                return true;
            }),
        _abilityTapSeenIds.end());

    int joyId  = _touch.GetJoyTouchId();
    int atkId  = _touch.GetAtkTouchId();
    int dashId = _touch.GetDashTouchId();

    for (int i = 0; i < tc; i++)
    {
        int id = GetTouchPointId(i);
        if (id == joyId || id == atkId || id == dashId) continue;

        bool seen = false;
        for (int sid : _abilityTapSeenIds) if (sid == id) { seen = true; break; }
        if (seen) continue;

        _abilityTapSeenIds.push_back(id);
        int slot = hitSlot(GetTouchPosition(i));
        if (slot >= 0) _player.TriggerAbilityCast(slot);
    }
}

// Draws touch-mode ability slots as square icons — same visual language as the
// desktop bar, just repositioned and slightly larger for thumb tapping.
void Engine::DrawTouchAbilityArc()
{
    const int totalSlots = _player.GetMaxAbilitySlots();

    for (int slot = 0; slot < totalSlots; slot++)
    {
        AbilityType ability = _player.GetLearnedAbility(slot);
        Rectangle   rec     = TouchAbilityRect(slot, _windowWidth, _windowHeight);
        bool isEmpty = (ability == AbilityType::None);
        bool canCast = !isEmpty && (_player.GetMana() >= GetAbilityManaCost(ability));

        Color bgColor     = isEmpty ? Fade(BLACK, 0.30f) : Fade(BLACK, 0.55f);
        Color borderColor = isEmpty ? Fade(WHITE, 0.12f)
                          : canCast  ? Fade(LIGHTGRAY, 0.35f) : Fade(RED, 0.40f);

        DrawRectangleRounded(rec, 0.18f, 6, bgColor);
        DrawRectangleRoundedLines(rec, 0.18f, 6, borderColor);

        if (isEmpty)
        {
            // Show slot number dimly, same as desktop bar
            DrawText(GetKeyName(_player.GetAbilityKey(slot)),
                (int)(rec.x + 6.f), (int)(rec.y + 6.f), 14, Fade(WHITE, 0.25f));
            continue;
        }

        // Slot number in top-left corner
        DrawText(GetKeyName(_player.GetAbilityKey(slot)),
            (int)(rec.x + 6.f), (int)(rec.y + 6.f), 14, Fade(WHITE, 0.6f));

        // Element icon centred in the upper portion of the slot
        const Texture2D* iconTex = &_abilityIconFireTex;
        if (ability == AbilityType::IceSpread || ability == AbilityType::IceBolt ||
            ability == AbilityType::IceUltimate)
            iconTex = &_abilityIconIceTex;
        else if (ability == AbilityType::ElectricSpread || ability == AbilityType::ElectricBolt ||
                 ability == AbilityType::ElectricUltimate)
            iconTex = &_abilityIconElectricTex;

        Color iconTint  = canCast ? WHITE : Fade(WHITE, 0.35f);
        float maxIconSz = rec.width * 0.55f;
        float iconScale = std::min(maxIconSz / (float)iconTex->width,
                                   maxIconSz / (float)iconTex->height);
        float iw = iconTex->width  * iconScale;
        float ih = iconTex->height * iconScale;
        float cx = rec.x + rec.width  * 0.5f;
        float cy = rec.y + rec.height * 0.42f;
        DrawTextureEx(*iconTex, { cx - iw * 0.5f, cy - ih * 0.5f }, 0.f, iconScale, iconTint);

        // Ability name at the bottom, matching desktop bar
        const char* abilityName = GetAbilityName(ability);
        int nameW = MeasureText(abilityName, 12);
        DrawText(abilityName,
            (int)(rec.x + rec.width / 2.f - nameW / 2.f),
            (int)(rec.y + rec.height - 18.f),
            12, canCast ? RAYWHITE : Fade(GRAY, 0.6f));

        // Level badge
        int abilityLv = _player.GetAbilityLevel(ability);
        if (abilityLv > 1)
        {
            const char* badge = TextFormat("Lv%d", abilityLv);
            int badgeW = MeasureText(badge, 12);
            Color badgeColor = (abilityLv >= 3) ? GOLD : Fade(SKYBLUE, 0.9f);
            DrawText(badge,
                (int)(rec.x + rec.width - badgeW - 4.f),
                (int)(rec.y + rec.height - 16.f),
                12, badgeColor);
        }
    }
}

