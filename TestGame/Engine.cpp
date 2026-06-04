#define _CRT_SECURE_NO_WARNINGS
#include "Engine.h"

#ifdef PLATFORM_WEB
#include <emscripten.h>
#endif

#include "AnimationUtils.h"
#include "AssetPaths.h"
#include "CapsuleCollision.h"
#include "NineSlice.h"
#include "raymath.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <functional>
#include <limits>
#include <queue>

namespace
{
    void DrawScrollingCheckerboard(float sw, float sh, Color dark, Color light, float speedX, float speedY, int cell = 80)
    {
        const int period = cell * 2;
        float t    = (float)GetTime();
        int   offX = (int)fmodf(t * speedX, (float)period);
        int   offY = (int)fmodf(t * speedY, (float)period);
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

    const char* GetDebugRoomTypeName(RoomType type)
    {
        switch (type)
        {
        case RoomType::Elite:    return "Elite";
        case RoomType::Rest:     return "Rest";
        case RoomType::Treasure: return "Treasure";
        case RoomType::Store:    return "Shop";
        case RoomType::Boss:     return "Boss";
        default:                 return "Standard";
        }
    }

}

Engine::Engine()
    : _gameState(_runState.StateRef())
    , _howToPlayFrom(_runState.HowToPlayFromRef())
    , _htpTab(_runState.HowToPlayTabRef())
    , _htpSlideOffset(_runState.HowToPlaySlideOffsetRef())
{
    Init();
}

Engine::~Engine()
{
    _nav.CancelAndReset();

    Enemy::UnloadSharedResources();
    Cyclops::UnloadSharedResources();
    Ogre::UnloadSharedResources();
    Molarbeast::UnloadSharedResources();
    // [SHELVED] FireBallPickup/SwordBeamPickup/FreezePickup unload calls removed
    HealPickup::UnloadSharedResources();
    ManaGemPickup::UnloadSharedResources();
    GoldPickup::UnloadSharedResources();
    SpreadProjectile::UnloadSharedResources();
    LavaBallProjectile::UnloadSharedResources();
    if (_audioInitialised)
    {
        UnloadSound(_pickupSound);
        UnloadSound(_fireballCastSound);
        UnloadSound(_explosionSound);
        UnloadSound(_roomClearExplosionSound);
        UnloadSound(_lavaBallImpactSound);
        UnloadSound(_buttonPressSound);
        _audio.Shutdown();
    }
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
    UnloadTexture(_roomClearExplosionTex);
    UnloadTexture(_abilityIconFireTex);
    UnloadTexture(_abilityIconIceTex);
    UnloadTexture(_abilityIconElectricTex);
    UnloadTexture(_mapIconNormal);
    UnloadTexture(_mapIconElite);
    UnloadTexture(_mapIconShop);
    UnloadTexture(_mapIconTreasure);
    UnloadTexture(_mapIconBoss);
    UnloadTexture(_mapIconRest);
    UnloadTexture(_upgradeAttackPowerTex);
    UnloadTexture(_upgradeAttackRangeTex);
    UnloadTexture(_upgradeHealthTex);
    UnloadTexture(_upgradeMagicTex);
    UnloadTexture(_upgradeDefenseTex);
    UnloadTexture(_upgradeMoveSpeedTex);
    UnloadTexture(_shopBorderTex);
    UnloadTexture(_shopZephTex);
    _pauseUI.Unload();
    if (_audioInitialised)
        CloseAudioDevice();
    CloseWindow();
}

void Engine::Init()
{
    InitWindow(_windowWidth, _windowHeight, "Mystic Onslaught");
    SetTargetFPS(60);

#ifdef PLATFORM_WEB
    {
        int mobile = emscripten_run_script_int(
            "(/android|iphone|ipad|mobile/i.test(navigator.userAgent)) ? 1 : 0"
        );
        _touchModeActive = (mobile > 0);
    }
#endif

    EnsureAudioInitialized();

    SetExitKey(KEY_NULL);

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
    _roomClearExplosionTex = LoadTexture(AssetPath("PowerUps/Flame_Explosion.png").c_str());
    _vfx.Init(&_fireballCastTex, &_fireballHitTex, &_genericHitTex,
              &_iceHitTex, &_lightningCastTex, &_healEffectTex);
    _abilityIconFireTex     = LoadTexture(AssetPath("PowerUps/FireBallPickup.png").c_str());
    _abilityIconIceTex      = LoadTexture(AssetPath("PowerUps/IceSpellPickup.png").c_str());
    _abilityIconElectricTex = LoadTexture(AssetPath("PowerUps/LightningPickup.png").c_str());
    _shopBorderTex          = LoadTexture(AssetPath("UI/PauseBoarder.png").c_str());
    _shopZephTex            = LoadTexture(AssetPath("UI/Zeph.png").c_str());
    _shop.Init(ShopTextures{
        &_shopBorderTex,
        &_shopZephTex,
        &_upgradeAttackPowerTex,
        &_upgradeAttackRangeTex,
        &_upgradeHealthTex,
        &_upgradeMagicTex,
        &_upgradeDefenseTex,
        &_upgradeMoveSpeedTex,
        &_abilityIconFireTex,
        &_abilityIconIceTex,
        &_abilityIconElectricTex
    });

    _mapIconNormal   = LoadTexture(AssetPath("TileSet/MapIcons/NormalRoom.png").c_str());
    _mapIconElite    = LoadTexture(AssetPath("TileSet/MapIcons/EliteRoom.png").c_str());
    _mapIconShop     = LoadTexture(AssetPath("TileSet/MapIcons/ShopRoom.png").c_str());
    _mapIconTreasure = LoadTexture(AssetPath("TileSet/MapIcons/TreasureRoom.png").c_str());
    _mapIconBoss     = LoadTexture(AssetPath("TileSet/MapIcons/BossRoom.png").c_str());
    _mapIconRest     = LoadTexture(AssetPath("TileSet/MapIcons/RestRoom.png").c_str());

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
    _vfx.Clear();
    _enemies.clear();

    _pickupSpawnTimer = kDefaultTimedPickupInterval;

    ResetRunState();
}

void Engine::EnsureAudioInitialized()
{
    if (_audioInitialised)
        return;

    InitAudioDevice();
    _audioInitialised = true;

    _pickupSound       = LoadSound(AssetPath("Sounds/PickupSound.ogg").c_str());
    _fireballCastSound = LoadSound(AssetPath("Sounds/GS1_Spell_Fire.ogg").c_str());
    _explosionSound    = LoadSound(AssetPath("Sounds/GS1_Spell_Explode.ogg").c_str());
    _roomClearExplosionSound = LoadSound(AssetPath("Sounds/Explosion.ogg").c_str());
    {
        // Lavaball impact should only play for the first second of the clip.
        // Cropping the wave at load time keeps playback simple and lets each
        // collision just trigger a normal PlaySound call.
        Wave lavaImpactWave = LoadWave(AssetPath("Sounds/Explosion.ogg").c_str());
        int oneSecondFrame = lavaImpactWave.sampleRate;
        if (oneSecondFrame > 0 && (int)lavaImpactWave.frameCount > oneSecondFrame)
            WaveCrop(&lavaImpactWave, 0, oneSecondFrame);
        _lavaBallImpactSound = LoadSoundFromWave(lavaImpactWave);
        UnloadWave(lavaImpactWave);
    }
    _buttonPressSound = LoadSound(AssetPath("Sounds/ButtonPress.ogg").c_str());
    _audio.Init();

    SetSoundPitch(_buttonPressSound, 1.25f);
    SetSoundVolume(_buttonPressSound, 0.35f);

    SetSoundPitch(_pickupSound, 1.35f);
    SetSoundVolume(_pickupSound, 0.45f);
    SetSoundVolume(_lavaBallImpactSound, 0.45f);
    SetSoundVolume(_roomClearExplosionSound, 0.60f);

    // Reload player sounds that failed to load during Engine::Init() because
    // the audio device didn't exist yet (web deferred-init path).
    _player.ReloadSounds();
}

void Engine::StartVictoryMusic(MusicCue cue)
{
    if (cue == MusicCue::BattleVictory)
    {
        _audio.StartBattleVictory();
        return;
    }
    if (cue == MusicCue::BossVictory)
        _audio.StartBossVictory();
}

void Engine::ResetMusicState()
{
    _audio.Reset();
    _demoEndTouchHeld = false;
}

void Engine::UpdateMusicSystem()
{
    AudioContext ctx{};
    ctx.gameState = _gameState;
    ctx.howToPlayFrom = _howToPlayFrom;
    ctx.awaitingStartingAbility = _awaitingStartingAbility;
    ctx.currentRoomType = _currentRoomType;
    ctx.roomClearPending = _roomClearPending;
    ctx.bossFightActive = IsBossFightActive();
    ctx.actBiome = GetBiomeForAct(_currentAct);
    ctx.currentBiome = _currentBiome;
    _audio.Update(ctx);
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

    _eliteRewardGranted      = false;
    // Clear all elite-room systems before room setup/spawns so the new room can
    // repopulate them correctly. Doing this after SpawnEnemies() breaks the
    // main-game elite flow by wiping the miniboss pointer/mechanic state while
    // leaving the spawned elite invulnerability intact.
    _eliteMechanic           = -1;
    _eliteMinibossPtr        = nullptr;
    _eliteCageRadius         = 0.f;
    _eliteIsLeaping          = false;
    _eliteEnrageWarningTimer = 0.f;
    _eliteHazardSpawnTimer   = 0.f;

    // Store room — place Zeph at map centre and stock the shop
    if (type == RoomType::Store)
    {
        float mapW = _map.width  * _mapScale;
        float mapH = _map.height * _mapScale;
        _shop.Enter({ mapW * 0.5f, mapH * 0.40f }, _player, _currentAct);
    }

    std::string actName = GetBiomeName(GetBiomeForAct(_currentAct));
    _message = "Act " + std::to_string(_currentAct) + " - " + actName;

    _waveStarting   = true;
    _waveIntroTimer = 0.f;

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

void Engine::DebugStartRun()
{
    ResetRunState();
    _debug.Activate();
    _awaitingStartingAbility = false;
    _levelUpReturnState = GameState::PregenTest;
    _message = "Debug Dungeon";

    _dungeonGen.Generate();
    _tileDefs = {};
    _tileDefs.LoadFromFile("tilemapper_Caverns.txt");
    {
        std::string sheetPath  = std::string(kTilesheetFolder) + "/Caverns.png";
        std::string groundPath = std::string(kTilesheetFolder) + "/Ground TIles.png";
        _tileRenderer.Init(sheetPath.c_str(), groundPath.c_str(), _tileDefs);
    }

    const auto& rooms = _dungeonGen.GetRooms();
    int startIdx = _dungeonGen.GetStartIndex();
    _pregenViewedRoomIdx = startIdx;
    const DungeonRoom& startRoom = rooms[startIdx];
    _pregenRoomLayout = RoomLayout::Generate(
        startRoom.hasNorth, startRoom.hasSouth,
        startRoom.hasEast,  startRoom.hasWest, startRoom.type,
        (int)_tileDefs.props.size(),      (int)_tileDefs.decors.size(),
        (int)_tileDefs.animDecors.size(), (int)_tileDefs.animProps.size());
    _pregenRoomStates.clear();
    _pregenEntryDoorSide = PregenDoorSide::None;
    ApplyPregenRoomDoorState(_pregenRoomLayout, _pregenViewedRoomIdx, _pregenEntryDoorSide);
    _pregenEnemiesSpawned = false;
    _pregenScrolling      = false;
    _pregenView           = PregenView::Play;
    _gameState            = GameState::PregenTest;

    float hw = (float)GetScreenWidth()  * 0.5f;
    float hh = (float)GetScreenHeight() * 0.5f;
    _player.SetWorldPos({ hw, hh });
    _player.Revive();
    _cameraPos = { hw, hh };

    RebuildPregenNav();
    DebugRestartPregenRoomAs(RoomType::Standard);

    _fadeInTimer = 1.0f;
    _fadeInDuration = 1.0f;
}

void Engine::DebugSetEliteMechanic(int mechanic)
{
    _debug.SetForcedEliteMechanic(mechanic);
    DebugRestartRoomAs(RoomType::Elite);
}


void Engine::DebugRestartPregenRoomAs(RoomType type)
{
    if (_pregenViewedRoomIdx < 0) return;

    ClearPregenEnemies();
    _roomClearPending = false;
    _pendingExp = 0.f;
    _expTallyAccum = 0.f;
    _expTallyDone = false;
    _tallyChoiceChaining = false;
    _eliteRewardGranted = false;
    _eliteMechanic = -1;
    _eliteMinibossPtr = nullptr;
    _eliteCageRadius = 0.f;
    _eliteEnrageWarningTimer = 0.f;
    _eliteHazardSpawnTimer = 0.f;
    _eliteIsLeaping = false;
    _eliteLeapCooldown = 0.f;
    _eliteLeapTimer = 0.f;
    _pregenClearEffects.clear();

    const auto& rooms = _dungeonGen.GetRooms();
    bool hasNorth = true;
    bool hasSouth = true;
    bool hasEast = true;
    bool hasWest = true;
    if (_pregenViewedRoomIdx >= 0 && _pregenViewedRoomIdx < (int)rooms.size())
    {
        const DungeonRoom& room = rooms[_pregenViewedRoomIdx];
        hasNorth = room.hasNorth;
        hasSouth = room.hasSouth;
        hasEast  = room.hasEast;
        hasWest  = room.hasWest;
    }

    _currentRoomType = type;
    _currentRoom = (_currentRoom % 5) + 1;
    if (type == RoomType::Boss)
        _currentRoom = 6;
    _message = std::string("Debug - ") + GetDebugRoomTypeName(type) + " Tile Room";

    _pregenRoomLayout = RoomLayout::Generate(
        hasNorth, hasSouth, hasEast, hasWest, type,
        (int)_tileDefs.props.size(), (int)_tileDefs.decors.size(),
        (int)_tileDefs.animDecors.size(), (int)_tileDefs.animProps.size());
    _pregenRoomStates[_pregenViewedRoomIdx].cleared = false;
    ApplyPregenRoomDoorState(_pregenRoomLayout, _pregenViewedRoomIdx, _pregenEntryDoorSide);

    float sw = (float)GetScreenWidth();
    float sh = (float)GetScreenHeight();
    float cellW = sw / (float)RoomLayout::kCols;
    float cellH = sh / (float)RoomLayout::kRows;
    _player.SetWorldPos({ sw * 0.5f, sh * 0.5f });
    _player.Revive();
    _player.GrantInvulnerability(kWaveSpawnProtectionDuration);
    _cameraPos = { sw * 0.5f, sh * 0.5f };

    RebuildPregenNav();

    auto spawnAt = [&](auto spawnFn, int count = 1) {
        for (int n = 0; n < count; n++)
            spawnFn(GetPregenSpawnPos(cellW, cellH));
    };

    if (type == RoomType::Boss)
    {
        SpawnMolarbeast({ sw * 0.5f, sh * 0.28f });
        ApplyPregenBossExitTiles(TileType::DoorLocked);
        _pregenEnemiesSpawned = true;
    }
    else if (type == RoomType::Elite)
    {
        SpawnOgre(GetPregenSpawnPos(cellW, cellH));
        spawnAt([&](Vector2 p) { SpawnBasicEnemy(p); }, 2);
        _pregenEnemiesSpawned = true;
    }
    else if (type == RoomType::Standard)
    {
        spawnAt([&](Vector2 p) { SpawnBasicEnemy(p); }, 2);
        SpawnCyclops(GetPregenSpawnPos(cellW, cellH));
        _pregenEnemiesSpawned = true;
    }
    else if (type == RoomType::Treasure)
    {
        _pregenRoomStates[_pregenViewedRoomIdx].cleared = true;
        ApplyPregenRoomDoorState(_pregenRoomLayout, _pregenViewedRoomIdx, _pregenEntryDoorSide);
        auto spawnGold = [&](GoldDenomination denom, float ox, float oy)
        {
            auto g = std::make_unique<GoldPickup>();
            g->Init(Vector2{ sw * 0.5f + ox, sh * 0.5f + oy }, denom);
            _pickups.push_back(std::move(g));
        };
        spawnGold(GoldDenomination::Ten, 0.f, 0.f);
        spawnGold(GoldDenomination::Five, -60.f, 24.f);
        spawnGold(GoldDenomination::Five, 60.f, 24.f);
    }
    else
    {
        _pregenRoomStates[_pregenViewedRoomIdx].cleared = true;
        ApplyPregenRoomDoorState(_pregenRoomLayout, _pregenViewedRoomIdx, _pregenEntryDoorSide);
    }

    _pregenScrolling = false;
    _pregenView = PregenView::Play;
    _gameState = GameState::PregenTest;
}
void Engine::DebugRestartRoomAs(RoomType type)
{
    if (_gameState == GameState::PregenTest)
    {
        DebugRestartPregenRoomAs(type);
        return;
    }
    for (auto& enemy : _enemies)
    {
        enemy->SetActive(false);
        enemy->Teleport(Vector2{ -5000.f, -5000.f });
    }

    _pickups.clear();
    _spreadProjectiles.clear();
    _ultimateBlasts.clear();
    _vfx.Clear();
    _cyclopsLasers.clear();
    _lavaBalls.clear();
    ClearBossSupportAdds();
    _bossCyclopsSupport.enemy = nullptr;
    _bossCyclopsSupport.respawnTimer = 0.f;
    _bossOgreSupport.enemy = nullptr;
    _bossOgreSupport.respawnTimer = 0.f;

    _roomClearPending = false;
    _pendingExp = 0.f;
    _expTallyAccum = 0.f;
    _expTallyDone = false;
    _tallyChoiceChaining = false;
    _eliteRewardGranted = false;
    _eliteMechanic = -1;
    _eliteMinibossPtr = nullptr;
    _eliteCageRadius = 0.f;
    _eliteEnrageWarningTimer = 0.f;
    _eliteHazardSpawnTimer = 0.f;
    _eliteIsLeaping = false;
    _eliteLeapCooldown = 0.f;
    _eliteLeapTimer = 0.f;

    _currentRoomType = type;
    _currentRoom = (_currentRoom % 5) + 1;
    if (type == RoomType::Boss)
        _currentRoom = 6;
    _wave = std::max(1, _wave + 1);

    Biome nextBiome = GetBiomeForAct(_currentAct);
    _currentBiome = nextBiome;
    _pendingBiome = nextBiome;
    _biomeTransitionActive = false;
    _biomeTransitionSwapped = false;
    _biomeTransitionTimer = 0.f;
    ApplyBiome(nextBiome);
    PopulatePropsForBiome(nextBiome);
    {
        std::vector<Rectangle> propRects;
        propRects.reserve(_props.size() + 1);
        for (auto& prop : _props)
            propRects.push_back(prop.GetCollisionRec());
        // Block the bottom boundary zone so A* never routes waypoints into it.
        {
            const float navMapW = _map.width  * _mapScale;
            const float navMapH = _map.height * _mapScale;
            const float navBot  = (_currentBiome == Biome::Forest || _currentBiome == Biome::Jungle) ? 160.f : 220.f;
            propRects.push_back({ 0.f, navMapH - navBot, navMapW, navBot });
        }
        _nav.Rebuild(_map.width * _mapScale, _map.height * _mapScale, propRects);
    }

    _waveStarting = false;
    _waveIntroTimer = 0.f;
    _fadeInTimer = 0.6f;
    _fadeInDuration = 0.6f;
    _pickupSpawnTimer = kDefaultTimedPickupInterval;
    _player.GrantInvulnerability(kWaveSpawnProtectionDuration);

    _message = std::string("Debug - ") + GetDebugRoomTypeName(type) + " Area";

    if (type == RoomType::Treasure)
    {
        _roomClearPending = true;
        const float mapW = _map.width * _mapScale;
        const float mapH = _map.height * _mapScale;
        const Vector2 centre{ mapW * 0.5f, mapH * 0.5f };
        auto spawnGold = [&](GoldDenomination denom, float ox, float oy)
        {
            auto g = std::make_unique<GoldPickup>();
            g->Init(Vector2{ centre.x + ox, centre.y + oy }, denom);
            _pickups.push_back(std::move(g));
        };
        spawnGold(GoldDenomination::Ten, 0.f, 0.f);
        spawnGold(GoldDenomination::Five, -60.f, 24.f);
        spawnGold(GoldDenomination::Five, 60.f, 24.f);
    }
    else if (type == RoomType::Store)
    {
        _roomClearPending = true;
        float mapW = _map.width  * _mapScale;
        float mapH = _map.height * _mapScale;
        _shop.Enter({ mapW * 0.5f, mapH * 0.40f }, _player, _currentAct);
    }
    else if (type == RoomType::Rest)
    {
        _roomClearPending = true;
    }
    else
    {
        SpawnEnemies();
    }

    _gameState = GameState::Play;
}

// (ShowRoomChoiceScreen / GenerateRoomChoices removed — replaced by map system)

Biome Engine::GetBiomeForAct(int act) const
{
    int idx = act - 1;
    if (idx >= 0 && idx < (int)_biomeSequence.size())
        return _biomeSequence[idx];
    // Fallback if sequence not generated yet
    bool isDungeon = _startBiomeDungeon ? ((act % 2) == 1) : ((act % 2) == 0);
    return isDungeon ? Biome::Caverns : Biome::Forest;
}


// ── GenerateActMap ────────────────────────────────────────────────────────────
// Builds a 6-row branching DAG for the current act.
// Row 0 = entry (Standard), rows 1-4 = branching, row 5 = Boss.
//
// Special room guarantee:
//   All four specials appear exactly once per act across two special rows:
//     Row 2 (early)  → Elite + Treasure  (combat/reward, available from the start)
//     Row 3 (late)   → Shop  + Rest      (utility/recovery, only after 3 rows of combat)
//   Each row has 4 nodes: 2 specials + 2 Standard fillers.
//   The 2 special positions are chosen independently per row (out of 4 slots)
//   so the rewarded lanes shift each run and force different routing decisions.
//
//   A path visits exactly one node per row → at most 2 specials per run.
//   Rows 1 and 4 are all Standard (2–4 nodes) for additional branching variety.
void Engine::GenerateActMap()
{
    _actMap.clear();
    _currentMapNodeIdx = -1;
    _mapOpenTimer = 0.f;

    // ── 1. Assign specials to rows by type ───────────────────────────────
    // Row 2 (early) always gets Elite + Treasure — combat/reward specials.
    // Row 3 (late, after 3 rows of combat) always gets Shop + Rest —
    //   utility/recovery specials that only make sense once the player has
    //   earned gold and taken some damage.
    // Each pair is randomly swapped so which side of the row each appears on
    // is still unpredictable.
    RoomType row2Pair[2] = { RoomType::Elite,    RoomType::Treasure };
    RoomType row3Pair[2] = { RoomType::Store,     RoomType::Rest     };
    if (GetRandomValue(0, 1)) std::swap(row2Pair[0], row2Pair[1]);
    if (GetRandomValue(0, 1)) std::swap(row3Pair[0], row3Pair[1]);

    // ── 2. Pick special positions in each 4-node special row ─────────────
    // Each special row has 4 nodes: 2 specials + 2 Standard fillers.
    // Pick 2 distinct positions (0-3) for the specials; the other 2 are Standard.
    // Chosen independently per row so the "empty lanes" shift each run.
    auto pickTwoDistinct = [](int& a, int& b, int max) {
        a = GetRandomValue(0, max);
        do { b = GetRandomValue(0, max); } while (b == a);
    };

    int sp2a, sp2b;   // special positions in row 2
    int sp3a, sp3b;   // special positions in row 3
    pickTwoDistinct(sp2a, sp2b, 3);
    pickTwoDistinct(sp3a, sp3b, 3);

    // Build the 4-element type arrays for rows 2 and 3
    RoomType row2Types[4], row3Types[4];
    {
        int si = 0;
        for (int i = 0; i < 4; i++)
            row2Types[i] = (i == sp2a || i == sp2b) ? row2Pair[si++] : RoomType::Standard;
        si = 0;
        for (int i = 0; i < 4; i++)
            row3Types[i] = (i == sp3a || i == sp3b) ? row3Pair[si++] : RoomType::Standard;
    }

    // ── 3. Decide per-row node counts ─────────────────────────────────────
    // Rows 2 and 3 are fixed at 4 (needed for special placement).
    // Rows 1 and 4 vary (2-4) for additional graph shape variety.
    int rowCounts[6];
    rowCounts[0] = 1;
    rowCounts[1] = GetRandomValue(2, 4);
    rowCounts[2] = 4;
    rowCounts[3] = 4;
    rowCounts[4] = GetRandomValue(2, 4);
    rowCounts[5] = 1;

    // ── 4. Create nodes ───────────────────────────────────────────────────
    int rowStart[6];
    for (int r = 0; r < 6; r++)
    {
        rowStart[r] = (int)_actMap.size();
        for (int i = 0; i < rowCounts[r]; i++)
        {
            MapNode n;
            n.row   = r;
            // normX: single-node rows centre at 0.5; multi-node rows spread 0.2–0.8
            n.normX = (rowCounts[r] == 1) ? 0.5f
                    : 0.2f + (float)i / (rowCounts[r] - 1) * 0.6f;

            if      (r == 0) n.type = RoomType::Standard;
            else if (r == 5) n.type = RoomType::Boss;
            else if (r == 2) n.type = row2Types[i];
            else if (r == 3) n.type = row3Types[i];
            else             n.type = RoomType::Standard;   // rows 1 and 4

            n.available = (r == 0);
            n.completed = false;
            _actMap.push_back(n);
        }
    }

    // ── 5. Build connections ──────────────────────────────────────────────
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

    // ── 6. Compute draw positions ─────────────────────────────────────────
    // Boss (row 5) sits at the top; entry (row 0) at the bottom.
    // Graph sits between the left stats panel and the right journey panel.
    const float mapTop  = 130.f;
    const float mapBot  = (float)_windowHeight - 110.f;
    const float rowH    = (mapBot - mapTop) / 5.f;
    const float mapLeft = (float)_windowWidth  * 0.30f;
    const float mapRight= (float)_windowWidth  * 0.76f;  // right side reserved for journey panel

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

    // Store room — place Zeph at map centre and stock the shop
    if (_currentRoomType == RoomType::Store)
    {
        float mapW = _map.width  * _mapScale;
        float mapH = _map.height * _mapScale;
        _shop.Enter({ mapW * 0.5f, mapH * 0.40f }, _player, _currentAct);
    }

    _message = "Act " + std::to_string(_currentAct) + " - " + GetBiomeName(GetBiomeForAct(_currentAct));

    _waveStarting   = true;
    _waveIntroTimer = 0.f;

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
        {
            std::vector<Rectangle> propRects;
            propRects.reserve(_props.size() + 1);
            for (auto& prop : _props)
                propRects.push_back(prop.GetCollisionRec());
            // Block the bottom boundary zone so A* never routes waypoints into it.
            {
                const float navMapW = _map.width  * _mapScale;
                const float navMapH = _map.height * _mapScale;
                const float navBot  = (_currentBiome == Biome::Forest || _currentBiome == Biome::Jungle) ? 160.f : 220.f;
                propRects.push_back({ 0.f, navMapH - navBot, navMapW, navBot });
            }
            _nav.Rebuild(_map.width * _mapScale, _map.height * _mapScale, propRects);
        }

        // Most room-to-room transitions should feel immediate. For same-biome
        // rooms we start the encounter right away instead of pausing behind
        // the intro banner.
        _waveStarting = false;
        if (_currentRoomType == RoomType::Treasure)
        {
            _roomClearPending = true;

            // Spawn a pile of gold coins at the centre of the map.
            const float mapW = _map.width  * _mapScale;
            const float mapH = _map.height * _mapScale;
            const Vector2 centre{ mapW * 0.5f, mapH * 0.5f };
            const float spread = 60.f;

            auto spawnGold = [&](GoldDenomination denom, float ox, float oy)
            {
                auto g = std::make_unique<GoldPickup>();
                g->Init(Vector2{ centre.x + ox, centre.y + oy }, denom);
                _pickups.push_back(std::move(g));
            };

            // 1 ten-gold in the middle, 2 five-golds, and 3 singles scattered around
            spawnGold(GoldDenomination::Ten,    0.f,     0.f);
            spawnGold(GoldDenomination::Five,  -spread,  spread * 0.5f);
            spawnGold(GoldDenomination::Five,   spread,  spread * 0.5f);
            spawnGold(GoldDenomination::Single, 0.f,    -spread);
            spawnGold(GoldDenomination::Single,-spread * 0.6f, -spread * 0.6f);
            spawnGold(GoldDenomination::Single, spread * 0.6f, -spread * 0.6f);
        }
        else
        {
            SpawnEnemies();
            _player.GrantInvulnerability(kWaveSpawnProtectionDuration);
        }
    }

    // Fade in from black each time a room is entered.
    _fadeInTimer    = 1.2f;
    _fadeInDuration = 1.2f;
    _gameState = GameState::Play;
}

void Engine::CompleteCurrentMapNode()
{
    if (_currentMapNodeIdx < 0 || _currentMapNodeIdx >= (int)_actMap.size())
        return;

    _actMap[_currentMapNodeIdx].completed = true;
    for (int next : _actMap[_currentMapNodeIdx].nextNodes)
        if (next >= 0 && next < (int)_actMap.size())
            _actMap[next].available = true;
}

void Engine::HandleRoomContinueAction()
{
    _roomClearPending = false;

    if (_currentRoomType == RoomType::Treasure)
    {
        CompleteCurrentMapNode();
        GenerateLevelUpOptions(LevelUpOfferContext::TreasureBasic);
        _levelUpReturnState = GameState::Map;
        _levelUpOpenTimer   = 0.25f;
        _gameState          = GameState::LevelUpChoice;
        return;
    }

    if (_currentRoomType == RoomType::Boss)
    {
        CompleteCurrentMapNode();

        if (_bossesDefeated >= 2)
        {
            _gameState = GameState::DemoEnd;
            return;
        }

        _currentAct++;
        GenerateActMap();
        _mapKeySelectedIdx = -1;
        _mapOpenTimer = 0.4f;
        _gameState = GameState::Map;
        return;
    }

    CompleteCurrentMapNode();
    _mapKeySelectedIdx = -1;
    _mapOpenTimer = 0.4f;
    _gameState = GameState::Map;
}

void Engine::SpawnEnemies()
{
    CombatSpawnContext ctx{};
    ctx.map = &_map;
    ctx.mapScale = _mapScale;
    ctx.currentRoomType = _currentRoomType;
    ctx.currentAct = _currentAct;
    ctx.currentRoom = _currentRoom;
    ctx.forcedEliteMechanic = _debug.GetForcedEliteMechanic();
    ctx.pickups = &_pickups;
    ctx.eliteMechanic = &_eliteMechanic;
    ctx.eliteMinibossPtr = &_eliteMinibossPtr;
    ctx.eliteCageCenter = &_eliteCageCenter;
    ctx.eliteCageRadius = &_eliteCageRadius;
    ctx.eliteCageDamageTimer = &_eliteCageDamageTimer;
    ctx.eliteEnrageWarningTimer = &_eliteEnrageWarningTimer;
    ctx.eliteIsLeaping = &_eliteIsLeaping;
    ctx.eliteLeapCooldown = &_eliteLeapCooldown;
    ctx.eliteLeapTimer = &_eliteLeapTimer;
    ctx.eliteHazardSpawnTimer = &_eliteHazardSpawnTimer;
    ctx.isSpawnPositionValid = [&](Vector2 pos) { return IsSpawnPositionValid(pos); };
    ctx.spawnBasicEnemy = [&](Vector2 pos) { return SpawnBasicEnemy(pos); };
    ctx.spawnCyclops = [&](Vector2 pos) { return SpawnCyclops(pos); };
    ctx.spawnOgre = [&](Vector2 pos) { return SpawnOgre(pos); };
    ctx.spawnMolarbeast = [&](Vector2 pos) { SpawnMolarbeast(pos); };
    ctx.spawnBossSupportAdds = [&]() { SpawnBossSupportAdds(); };
    _combatDirector.SpawnEnemies(ctx);
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
    UpdateMusicSystem();

    BeginDrawing();
    ClearBackground(WHITE);
    Draw();
    EndDrawing();
}

void Engine::Update(float dt)
{
    // Secret unlock: F12 or \ to enable debug mode access.
    if (IsKeyPressed(KEY_F12) || IsKeyPressed(KEY_BACKSLASH))
        _demoCompleted = true;

    switch (_gameState)
    {
    case GameState::Menu:
    {
        _menu.SetDebugUnlocked(_demoCompleted);
        _menu.Update();

        if (_menu.StartPressed())
        {
            _debug.Deactivate();
            ResetRunState();

            // ── Enter the Caverns dungeon directly ────────────────────────────
            _dungeonGen.Generate();
            _tileDefs = {};
            _tileDefs.LoadFromFile("tilemapper_Caverns.txt");
            {
                std::string sheetPath  = std::string(kTilesheetFolder) + "/Caverns.png";
                std::string groundPath = std::string(kTilesheetFolder) + "/Ground TIles.png";
                _tileRenderer.Init(sheetPath.c_str(), groundPath.c_str(), _tileDefs);
            }

            const auto& rooms2 = _dungeonGen.GetRooms();
            int startIdx2 = _dungeonGen.GetStartIndex();
            _pregenViewedRoomIdx = startIdx2;
            const DungeonRoom& startRoom2 = rooms2[startIdx2];
            _pregenRoomLayout = RoomLayout::Generate(
                startRoom2.hasNorth, startRoom2.hasSouth,
                startRoom2.hasEast,  startRoom2.hasWest, startRoom2.type,
                (int)_tileDefs.props.size(),      (int)_tileDefs.decors.size(),
                (int)_tileDefs.animDecors.size(), (int)_tileDefs.animProps.size());
            _pregenRoomStates.clear();
            _pregenEntryDoorSide = PregenDoorSide::None;
            ApplyPregenRoomDoorState(_pregenRoomLayout, _pregenViewedRoomIdx, _pregenEntryDoorSide);
            _pregenEnemiesSpawned = false;
            _pregenScrolling      = false;
            _pregenView           = PregenView::Play;

            float hw2 = (float)GetScreenWidth()  * 0.5f;
            float hh2 = (float)GetScreenHeight() * 0.5f;
            _player.SetWorldPos({ hw2, hh2 });
            _player.Revive();
            _cameraPos = { hw2, hh2 };

            RebuildPregenNav();
            ClearPregenEnemies();
            SpawnPregenRoomEnemies();
            if (_pregenViewedRoomIdx == _dungeonGen.GetBossIndex())
                ApplyPregenBossExitTiles(TileType::DoorLocked);

            _fadeInTimer = 1.0f;
            _fadeInDuration = 1.0f;
            _gameState = GameState::PregenTest;
        }
        if (_menu.DebugPressed() && _demoCompleted)
        {
            DebugStartRun();
        }
        if (_menu.QuitPressed())
            _shouldClose = true;
        if (_menu.HowToPressed())
        {
            _runState.OpenHowToPlay(GameState::Menu, _touchModeActive);
        }
        if (_menu.PregenTestPressed())
        {
            _dungeonGen.Generate();
            _pregenView          = PregenView::Graph;
            _pregenViewedRoomIdx = -1;

            // Load tile definitions from the Caverns tileset save file.
            _tileDefs = {};
            _tileDefs.LoadFromFile("tilemapper_Caverns.txt");

            // Load the tilesheet into the renderer.
            std::string sheetPath  = std::string(kTilesheetFolder) + "/Caverns.png";
            std::string groundPath = std::string(kTilesheetFolder) + "/Ground TIles.png";
            _tileRenderer.Init(sheetPath.c_str(), groundPath.c_str(), _tileDefs);

            _gameState = GameState::PregenTest;
        }
        if (_menu.TileMapperPressed())
        {
            _tileMapper.Init(kTilesheetFolder);
            _gameState = GameState::TileMapper;
        }
        break;
    }

    case GameState::PregenTest:
        UpdatePregenTest(dt);
        break;

    case GameState::TileMapper:
        _tileMapper.Update();
        if (_tileMapper.WantsToExit())
        {
            _tileMapper.Unload();
            _menu.Init();
            _gameState = GameState::Menu;
        }
        break;

    case GameState::HowToPlay:
    {
        if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_BACKSPACE))
        {
            if (_howToPlayFrom == GameState::Menu)
                _menu.Init();
            _runState.ReturnFromHowToPlay();
        }
        break;
    }

    case GameState::Keybindings:
    case GameState::TouchButtonMapping:
        // Navigation handled in the Draw case (Back/Save/Default buttons)
        break;

    case GameState::Pause:
    {
        if (IsKeyPressed(KEY_ESCAPE))
            _gameState = _stateBeforePause;

        break;
    }

    case GameState::GameOver:
        break;

    case GameState::DemoEnd:
    {
        bool anyKey = (GetKeyPressed() != 0);
        bool anyMouse = IsMouseButtonPressed(MOUSE_LEFT_BUTTON)
                     || IsMouseButtonPressed(MOUSE_RIGHT_BUTTON)
                     || IsMouseButtonPressed(MOUSE_MIDDLE_BUTTON);
        bool touchDown = (_touchModeActive && GetTouchPointCount() > 0);
        bool touchTap = touchDown && !_demoEndTouchHeld;
        _demoEndTouchHeld = touchDown;

        if (anyKey || anyMouse || touchTap)
        {
            ResetRunState();
            _menu.Init();
            _gameState = GameState::Menu;
        }
        break;
    }

    case GameState::LevelUpChoice:
        if (_levelUpOpenTimer > 0.f)
            _levelUpOpenTimer -= dt;
        break;

    case GameState::AbilityChoice:
        if (_abilityChoiceOpenTimer > 0.f)
            _abilityChoiceOpenTimer -= dt;
        break;

    case GameState::Shop:
        if (_shop.Update(_player, _debug.IsActive()))
        {
            _gameState = GameState::Play;
            if (_currentRoomType == RoomType::Store)
                _roomClearPending = true;
        }
        break;

    }
}

void Engine::UpdateGamePlay(float dt)
{
    if (_debug.IsActive())
    {
        if (HandleDebugToggleTabInput())
            return;

        if (_debug.IsGodMode())
            _player.GrantInvulnerability(0.2f);

        DebugCommand cmd = _debug.Update();
        if (cmd.issued)
        {
            Vector2 spawnBase = _player.GetWorldPos();
            switch (cmd.action)
            {
            case DebugActionKind::GrantInvuln:
                _player.GrantInvulnerability(5.f); break;
            case DebugActionKind::ClearEnemiesContinue:
                for (auto& e : _enemies) { e->SetActive(false); e->Teleport(Vector2{ -5000.f, -5000.f }); }
                _roomClearPending = true; break;
            case DebugActionKind::RestartRoom:
                DebugRestartRoomAs((RoomType)cmd.value); break;
            case DebugActionKind::SetEliteMechanic:
                _debug.SetForcedEliteMechanic(cmd.value);
                DebugRestartRoomAs(RoomType::Elite); break;
            case DebugActionKind::SpawnGrunt:
                SpawnBasicEnemy(Vector2Add(spawnBase, Vector2{ 220.f, 40.f })); break;
            case DebugActionKind::SpawnCyclops:
                SpawnCyclops(Vector2Add(spawnBase, Vector2{ 260.f, -60.f })); break;
            case DebugActionKind::SpawnOgre:
                SpawnOgre(Vector2Add(spawnBase, Vector2{ -240.f, 50.f })); break;
            case DebugActionKind::SpawnBoss:
                SpawnMolarbeast(Vector2Add(spawnBase, Vector2{ 0.f, -260.f })); break;
            case DebugActionKind::Heal:
                _player.Heal(cmd.value); break;
            case DebugActionKind::RestoreMana:
                _player.RestoreMana(cmd.value); break;
            case DebugActionKind::AddGold:
                _player.AddGold(cmd.value); break;
            case DebugActionKind::AddExp:
                _player.AddExp(cmd.value); break;
            case DebugActionKind::TreasureCards:
                GenerateLevelUpOptions(LevelUpOfferContext::TreasureBasic);
                _levelUpReturnState = GameState::Play; _levelUpOpenTimer = 0.1f;
                _gameState = GameState::LevelUpChoice; break;
            case DebugActionKind::EliteReward:
                GenerateLevelUpOptions(LevelUpOfferContext::EliteReward);
                _levelUpReturnState = GameState::Play; _levelUpOpenTimer = 0.1f;
                _gameState = GameState::LevelUpChoice; break;
            case DebugActionKind::AbilityReward:
                GenerateAbilityChoiceOptions(); _abilityChoiceOpenTimer = 0.1f;
                _pendingRoomChoice = false; _gameState = GameState::AbilityChoice; break;
            case DebugActionKind::ApplyUpgrade:
                _player.ApplyUpgrade((UpgradeType)cmd.value); break;
            default: break;
            }
        }
    }

    // Hitbox editor toggle — 0 key while debug panel is active
    if (_debug.IsActive() && IsKeyPressed(KEY_ZERO))
    {
        _isHitboxEditorActive = !_isHitboxEditorActive;
        _hitboxSelectedEntity = nullptr;
        _hitboxEditAttack     = false;
        _hitboxNudgeAccum     = -kHitboxNudgeInitDelay;
        if (_isHitboxEditorActive)
            _debug.SetOpen(false);  // hide panel while editing
    }
    // Safety: auto-exit editor if debug is deactivated from outside
    if (_isHitboxEditorActive && !_debug.IsActive())
    {
        _isHitboxEditorActive = false;
        _hitboxSelectedEntity = nullptr;
    }
    if (_isHitboxEditorActive)
    {
        UpdateHitboxEditor();
        return;
    }

    if (IsKeyPressed(KEY_ESCAPE))
    {
        _stateBeforePause = GameState::Play;
        _gameState = GameState::Pause;
        return;
    }

    // M key confirms the current room's continue action.
    if (_roomClearPending && IsKeyPressed(KEY_M))
    {
        HandleRoomContinueAction();
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
                            _currentRoomType == RoomType::Treasure ||
                            _debug.IsOpen());
    const bool ultActive = (_ultimatePhase != UltimatePhase::None);
    _player.SetCombatLocked(_waveStarting || ultActive || inNonCombatRoom);
    _player.SetManaRegenPaused(ultActive);

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
        _vfx.SpawnCastEffect(castType, _player.GetCastOrigin(), _player.GetFacingDirection());
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
        _vfx.SpawnCastEffect(effectType, _player.GetCastOrigin(), _player.GetFacingDirection());
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

            // Treasure rooms wait for the same explicit Continue input as every
            // other room. Pressing Continue opens the free upgrade screen.
            if (_currentRoomType == RoomType::Treasure)
            {
                _roomClearPending = true;
                return;
            }

            SpawnEnemies();
            _player.GrantInvulnerability(kWaveSpawnProtectionDuration);
        }
    }
    else
    {
        _gameTimer += dt;
        _nav.ApplyPendingRefresh();

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

        EliteMechanicsContext eliteCtx{};
        eliteCtx.currentRoomType = _currentRoomType;
        eliteCtx.map = &_map;
        eliteCtx.mapScale = _mapScale;
        eliteCtx.player = &_player;
        eliteCtx.enemies = &_enemies;
        eliteCtx.lavaBalls = &_lavaBalls;
        eliteCtx.eliteMechanic = &_eliteMechanic;
        eliteCtx.eliteMinibossPtr = &_eliteMinibossPtr;
        eliteCtx.eliteCageCenter = &_eliteCageCenter;
        eliteCtx.eliteCageRadius = &_eliteCageRadius;
        eliteCtx.eliteCageDamageTimer = &_eliteCageDamageTimer;
        eliteCtx.eliteEnrageWarningTimer = &_eliteEnrageWarningTimer;
        eliteCtx.eliteIsLeaping = &_eliteIsLeaping;
        eliteCtx.eliteLeapStartPos = &_eliteLeapStartPos;
        eliteCtx.eliteLeapTarget = &_eliteLeapTarget;
        eliteCtx.eliteLeapCooldown = &_eliteLeapCooldown;
        eliteCtx.eliteLeapTimer = &_eliteLeapTimer;
        eliteCtx.eliteHazardSpawnTimer = &_eliteHazardSpawnTimer;
        eliteCtx.isSpawnPositionValid = [&](Vector2 pos) { return IsSpawnPositionValid(pos); };
        eliteCtx.triggerScreenShake = [&](float strength, float duration) { TriggerScreenShake(strength, duration); };
        _combatDirector.UpdateEliteMechanics(eliteCtx, dt);

        // Rest areas complete immediately once their intro ends. Store areas
        // wait until the player actually leaves Zeph's shop before showing the
        // continue prompt.
        if (_currentRoomType == RoomType::Rest)
        {
            _roomClearPending = true;
        }

        // ── Store room — Zeph NPC logic ───────────────────────────────────
        if (_currentRoomType == RoomType::Store)
        {
            Vector2 shopWorldOffset = { -_cameraPos.x + _shakeOffset.x, -_cameraPos.y + _shakeOffset.y };
            if (_shop.UpdateNpc(_player, shopWorldOffset, _touchModeActive)) _gameState = GameState::Shop;
        }
        else if (!_roomClearPending && GetActiveEnemyCount() == 0 && !_debug.IsActive())
        {
            // Elite clear bonus: scatter some gold near the player's position.
            if (_currentRoomType == RoomType::Elite && !_eliteRewardGranted)
            {
                _eliteRewardGranted = true;
                const Vector2 dropAnchor = _player.GetWorldPos();
                auto dropGold = [&](GoldDenomination denom, float ox, float oy)
                {
                    auto g = std::make_unique<GoldPickup>();
                    g->Init(Vector2{ dropAnchor.x + ox, dropAnchor.y + oy }, denom);
                    _pickups.push_back(std::move(g));
                };
                dropGold(GoldDenomination::Ten,    0.f,   -50.f);
                dropGold(GoldDenomination::Five,  -55.f,   20.f);
                dropGold(GoldDenomination::Five,   55.f,   20.f);
            }

            if (_currentRoomType == RoomType::Boss)
                StartVictoryMusic(MusicCue::BossVictory);
            else if (_currentRoomType == RoomType::Standard || _currentRoomType == RoomType::Elite)
                StartVictoryMusic(MusicCue::BattleVictory);

            // Transition to post-battle EXP tally.
            // Boss AbilityChoice is triggered from UpdateExpTally once the tally finishes.
            _expTallyDone           = false;
            _expTallyAccum          = 0.f;
            _tallyStartLevel        = _player.GetLevel();
            _tallyLevelUpsRemaining = 0;
            _tallyChoiceChaining    = false;
            _gameState              = GameState::ExpTally;
        }

        Vector2 playerFeet = _player.GetFeetWorldPos();
        _nav.TickRefresh(dt, playerFeet);

        EnemyRuntimeContext enemyRuntimeCtx{};
        enemyRuntimeCtx.player = &_player;
        enemyRuntimeCtx.nav = &_nav;
        enemyRuntimeCtx.props = &_props;
        enemyRuntimeCtx.enemies = &_enemies;
        enemyRuntimeCtx.cyclopsLasers = &_cyclopsLasers;
        enemyRuntimeCtx.lavaBalls = &_lavaBalls;
        enemyRuntimeCtx.triggerScreenShake = [&](float strength, float duration) { TriggerScreenShake(strength, duration); };
        _combatDirector.UpdateEnemyRuntime(enemyRuntimeCtx, dt);

        HandlePlayerMeleeDamage();
        UpdateSpreadProjectiles(dt);
        UpdateLavaBallProjectiles(dt);
        _vfx.Update(dt);
        UpdatePregenClearEffects(dt);
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
        _vfx.SpawnHealEffect();

    HandleCollisions();

    {
        // WorldConfig::ClampCamera handles both the scroll-clamp (world > screen)
        // and the centring case (world ≤ screen — e.g. FitToScreen on a large monitor).
        // GetMapScreenPos converts the clamped world-pos to a screen-space top-left
        // for the map texture draw call. Using the live screen size here means
        // a window resize mid-session (or a phone rotation) adapts automatically.
        const int sw = GetScreenWidth();
        const int sh = GetScreenHeight();

        _cameraPos = _worldConfig.ClampCamera(_player.GetWorldPos(), sw, sh);
        _mapPos    = _worldConfig.GetMapScreenPos(_cameraPos, sw, sh);
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

    case GameState::Pause:
    {
        if (_stateBeforePause == GameState::PregenTest)
        {
            // Paused from the dungeon — show the tile room as the frozen background.
            ClearBackground(Color{ 8, 6, 10, 255 });
            float scaleX = (float)GetScreenWidth()  / (RoomLayout::kCols * 16.f);
            float scaleY = (float)GetScreenHeight() / (RoomLayout::kRows * 16.f);
            if (_tileRenderer.IsLoaded())
                _tileRenderer.DrawRoom(_pregenRoomLayout, scaleX, scaleY, { 0.f, 0.f });
        }
        else
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

            DrawCyclopsLasers(worldOffset);

            _vfx.Draw(worldOffset, _player.GetWorldPos(), _player.GetCastOrigin());

            for (auto& enemy : _enemies)
            {
                if (!enemy->IsActive())
                    continue;
                enemy->DrawEnemy(cameraRef);
            }

            _player.DrawPlayer(cameraRef);
        }

        int pauseResult = _pauseUI.DrawPause();
        if (pauseResult != 0) { StopSound(_buttonPressSound); PlaySound(_buttonPressSound); }
        if (pauseResult == 1)
            _gameState = _stateBeforePause;
        else if (pauseResult == 2)
        {
            _runState.OpenHowToPlay(GameState::Pause, _touchModeActive);
        }
        else if (pauseResult == 3)
            _shouldClose = true;
        else if (pauseResult == 4)
        {
            if (_touchModeActive)
            {
                EnterTouchButtonMapping();
                _gameState = GameState::TouchButtonMapping;
            }
            else
            {
                _gameState = GameState::Keybindings;
            }
        }
        else if (pauseResult == 5)
        {
            ResetRunState();
            _menu.Init();
            _gameState = GameState::Menu;
        }

        break;
    }

    case GameState::DemoEnd:
        DrawDemoEnd();
        break;

    case GameState::PregenTest:
        DrawPregenTest();
        break;

    case GameState::TileMapper:
        _tileMapper.Draw();
        break;

    case GameState::GameOver:
    {
        int goResult = _pauseUI.DrawGameOver();
        if (goResult != 0) { StopSound(_buttonPressSound); PlaySound(_buttonPressSound); }
        if (goResult == 1) { ResetRunState(); _fadeInTimer = 2.0f; _fadeInDuration = 2.0f; GenerateStartingAbilityOptions(); _awaitingStartingAbility = true; _levelUpReturnState = GameState::Map; _levelUpOpenTimer = 0.8f; _gameState = GameState::LevelUpChoice; }
        else if (goResult == 2) { ResetRunState(); _menu.Init(); _gameState = GameState::Menu; }
        else if (goResult == 3) _shouldClose = true;
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

    case GameState::TouchButtonMapping:
    {
        int r = DrawTouchButtonMapping();
        if (r == 1)  // Save
        {
            for (int i = 0; i < 6; i++) _touchCustomPos[i] = _touchMappingPos[i];
            _touchLayoutCustom = true;
            ApplyTouchCustomLayout();
            _gameState = GameState::Pause;
        }
        else if (r == 2)  // Back
        {
            _gameState = GameState::Pause;
        }
        else if (r == 3)  // Default
        {
            // Restore everything to the original baked values captured on first open
            if (_touchDefaults.captured)
            {
                _hudCfg.touchAtkPadR    = _touchDefaults.atkPadR;
                _hudCfg.touchAtkPadB    = _touchDefaults.atkPadB;
                _hudCfg.touchDashOffset = _touchDefaults.dashOffset;
                _hudCfg.touchDashBotPad = _touchDefaults.dashBotPad;
                for (int s = 0; s < 4; s++) _touchSlotOffset[s] = _touchDefaults.slotOffset[s];
                for (int i = 0; i < 6; i++) _touchMappingPos[i] = _touchDefaults.pos[i];
            }
            _touchLayoutCustom   = false;
            _touchMappingDragIdx = -1;
        }
        break;
    }

    case GameState::LevelUpChoice:
    {
        if (_levelUpReturnState == GameState::PregenTest)
        {
            // Draw the dungeon room as background instead of the wave-based world.
            ClearBackground(Color{ 8, 6, 10, 255 });
            float scaleX = (float)GetScreenWidth()  / (RoomLayout::kCols * 16.f);
            float scaleY = (float)GetScreenHeight() / (RoomLayout::kRows * 16.f);
            if (_tileRenderer.IsLoaded())
                _tileRenderer.DrawRoom(_pregenRoomLayout, scaleX, scaleY, { 0.f, 0.f });
        }
        else
        {
            DrawWorld();
        }
        DrawHUD();
        DrawLevelUpChoice();
        break;
    }

    case GameState::AbilityChoice:
    {
        if (_levelUpReturnState == GameState::PregenTest)
        {
            ClearBackground(Color{ 8, 6, 10, 255 });
            float scaleX = (float)GetScreenWidth()  / (RoomLayout::kCols * 16.f);
            float scaleY = (float)GetScreenHeight() / (RoomLayout::kRows * 16.f);
            if (_tileRenderer.IsLoaded())
                _tileRenderer.DrawRoom(_pregenRoomLayout, scaleX, scaleY, { 0.f, 0.f });
        }
        else
        {
            DrawWorld();
        }
        DrawHUD();
        DrawAbilityChoice();
        break;
    }

    case GameState::Shop:
    {
        _shop.Draw(_player, _debug.IsActive());
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
    const float marginBottom = ((_currentBiome == Biome::Forest || _currentBiome == Biome::Jungle)) ? 160.f : 220.f;

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
        Vector2 mtv{};
        if (CheckCapsuleRect(_player.GetCapsule(), prop.GetCollisionRec(), mtv))
        {
            if (_player.IsBeingForcedPushed())
                _player.OnForcedPushCollision();

            Vector2 ppos = _player.GetWorldPos();
            _player.SetWorldPos({ ppos.x + mtv.x, ppos.y + mtv.y });
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

                // Eject the enemy via capsule MTV
                Vector2 eMtv{};
                if (CheckCapsuleRect(enemy->GetCapsule(), prop.GetCollisionRec(), eMtv))
                {
                    Vector2 epos = enemy->GetWorldPos();
                    enemy->Teleport({ epos.x + eMtv.x, epos.y + eMtv.y });
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

            Vector2 peMtv{};
            if (!CheckCapsuleCapsule(_player.GetCapsule(), enemy->GetCapsule(), peMtv))
                continue;

            if (_player.IsBeingForcedPushed())
                continue;

            // Apply MTV directly without undoing movement — lateral motion is preserved
            // giving a natural slide around enemy capsules instead of a hard stop.
            Vector2 pos = _player.GetWorldPos();
            _player.SetWorldPos({ pos.x + peMtv.x, pos.y + peMtv.y });
        }
    }
}

void Engine::UpdateEnemyCount(float dt)
{
    EnemyDeathContext ctx{};
    ctx.enemies = &_enemies;
    ctx.bossCyclopsSupport = &_bossCyclopsSupport;
    ctx.bossOgreSupport = &_bossOgreSupport;
    ctx.wave = _wave;
    ctx.enemiesKilled = &_enemiesKilled;
    ctx.bossesDefeated = &_bossesDefeated;
    ctx.demoCompleted = &_demoCompleted;
    ctx.pendingExp = &_pendingExp;
    ctx.spawnEnemyDrop = [&](Vector2 worldPos, bool isOgre, bool isBoss) { SpawnEnemyDrop(worldPos, isOgre, isBoss); };
    _combatDirector.UpdateEnemyDeaths(ctx, dt);
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

    DrawCyclopsLasers(worldOffset);

    _vfx.Draw(worldOffset, _player.GetWorldPos(), _player.GetCastOrigin());

    // Depth sort: props whose midpoint is above the player's feet are drawn
    // after the player (player walks behind their top half / canopy).
    // Props whose midpoint is below the player's feet are drawn before
    // (player walks in front of their trunk / base).
    float playerFeetY = _player.GetFeetWorldPos().y;

    for (auto& prop : _props)
        if (prop.GetSortY() < playerFeetY)
            prop.Render(cameraRef);

    for (auto& enemy : _enemies)
    {
        if (!enemy->IsActive())
            continue;
        enemy->DrawEnemy(cameraRef);
    }

    _player.DrawPlayer(cameraRef);

    for (auto& prop : _props)
        if (prop.GetSortY() >= playerFeetY)
            prop.Render(cameraRef);

    // ── Elite-room visuals ────────────────────────────────────────────────
    if (_currentRoomType == RoomType::Elite)
    {
        const float sw2 = GetScreenWidth()  / 2.f;
        const float sh2 = GetScreenHeight() / 2.f;

        // 0. Cage ring
        if (_eliteMechanic == 0 && _eliteCageRadius > 0.f)
        {
            float cx = _eliteCageCenter.x + worldOffset.x + sw2;
            float cy = _eliteCageCenter.y + worldOffset.y + sh2;
            float pulse = 0.45f + 0.20f * sinf((float)GetTime() * 2.5f);
            DrawCircleLines((int)cx, (int)cy, _eliteCageRadius,
                Fade(Color{220, 40, 200, 255}, pulse));
            DrawCircleLines((int)cx, (int)cy, _eliteCageRadius - 5.f,
                Fade(Color{140, 0, 255, 255}, pulse * 0.5f));
        }

        // 1. Bodyguard tethers + shield aura
        if (_eliteMechanic == 1
            && _eliteMinibossPtr && _eliteMinibossPtr->IsActive()
            && _eliteMinibossPtr->IsInvulnerable())
        {
            Vector2 bossScreen = {
                _eliteMinibossPtr->GetWorldPos().x + worldOffset.x + sw2,
                _eliteMinibossPtr->GetWorldPos().y + worldOffset.y + sh2
            };
            float auraPulse = 0.25f + 0.15f * sinf((float)GetTime() * 5.f);
            DrawCircleV(bossScreen, 90.f, Fade(Color{180, 100, 255, 255}, auraPulse));

            for (const auto& e : _enemies)
            {
                if (e.get() == _eliteMinibossPtr) continue;
                if (!e->IsActive() || !e->IsAlive() || e->IsDying()) continue;
                Vector2 gs = {
                    e->GetWorldPos().x + worldOffset.x + sw2,
                    e->GetWorldPos().y + worldOffset.y + sh2
                };
                float ta = 0.35f + 0.20f * sinf((float)GetTime() * 4.f);
                DrawLineEx(gs, bossScreen, 2.f, Fade(Color{200, 140, 255, 255}, ta));
                DrawCircleV(gs, 7.f, Fade(Color{220, 160, 255, 255}, ta * 0.7f));
            }
        }

        // 3. Leap shadow — pulsing ellipse at the targeted landing spot
        if (_eliteMechanic == 3 && _eliteIsLeaping)
        {
            Vector2 ss = {
                _eliteLeapTarget.x + worldOffset.x + sw2,
                _eliteLeapTarget.y + worldOffset.y + sh2
            };
            float lp = 0.5f + 0.30f * sinf((float)GetTime() * 8.f);
            DrawEllipse((int)ss.x, (int)ss.y, 68, 32,
                Fade(Color{60, 0, 160, 255}, lp * 0.7f));
            DrawEllipseLines((int)ss.x, (int)ss.y, 68, 32,
                Fade(Color{220, 140, 255, 255}, lp));
            const char* wt = "INCOMING!";
            int wSz = 22;
            DrawText(wt, (int)(ss.x - MeasureText(wt, wSz) / 2.f),
                (int)(ss.y - 52.f), wSz,
                Fade(Color{255, 60, 60, 255}, lp + 0.1f));
        }

        // 4. Room hazards now use live lavaball chaos, so there is no extra
        // floor-marker overlay here. The projectile draw path carries the read.
    }

    // ── Shop NPC (Zeph) ──────────────────────────────────────────────────────
    if (_currentRoomType == RoomType::Store)
        _shop.DrawNpc(worldOffset);

    _vfx.DrawFloatingTexts(worldOffset);
}

void Engine::DrawHUD()
{
    {
    const HUDConfig& hc = _hudCfg;

    // Icon height matches the old circle diameter (barH * 0.56 ≈ 27 px at barH=48).
    const float kArmourIconH = hc.barH * 0.56f;
    const float kArmourGapY  = 5.f;   // space between HP bar bottom and icon row top

    const float hpBarY   = hc.barTopPad;
    const float manaBarY = hpBarY + hc.barH + kArmourGapY + kArmourIconH + kArmourGapY;
    const float barX     = (float)GetScreenWidth() / 2.f - hc.barW / 2.f;

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

    drawLabelBox(("Gold: " + std::to_string(_player.GetGold())).c_str(),
        hc.goldX, hc.goldY, (int)hc.goldFs, GOLD);
    drawLabelBox(("Enemies Left: " + std::to_string(GetActiveEnemyCount())).c_str(),
        hc.enemiesX, hc.enemiesY, (int)hc.enemiesFs, RAYWHITE);

    {
        bool isBoss  = (_currentRoomType == RoomType::Boss);
        bool isElite = (_currentRoomType == RoomType::Elite);
        const char* roomTypeSuffix =
            isBoss    ? "  BOSS" :
            isElite   ? "  ELITE" :
            (_currentRoomType == RoomType::Rest)     ? "  REST" :
            (_currentRoomType == RoomType::Treasure) ? "  TREASURE" :
            (_currentRoomType == RoomType::Store)    ? "  SHOP" : "";
        const char* roomLabel = TextFormat("Act %d  Room %d%s",
            _currentAct, _currentRoom, roomTypeSuffix);
        int roomLabelW = MeasureText(roomLabel, (int)hc.actFs);
        Color labelColor = isBoss ? ORANGE : (isElite ? Color{255,140,0,255} : RAYWHITE);
        drawLabelBox(roomLabel,
            (float)(GetScreenWidth() - roomLabelW - (int)hc.actOffsetX),
            hc.actY, (int)hc.actFs, labelColor);
    }

    {
        float maxHp = _player.GetMaxHealthValue();
        float curHp = _player.GetHealthValue();
        float hpPct = (maxHp > 0.f) ? (curHp / maxHp) : 0.f;
        Color fillColor = (hpPct > 0.70f) ? GREEN : (hpPct > 0.30f) ? YELLOW : RED;

        if (hpPct <= 0.30f)
        {
            float pulse    = (sinf((float)GetTime() * (2.f * PI / 3.f)) + 1.f) * 0.5f;
            float exps[]   = { 4.f, 9.f, 15.f };
            float alphas[] = { 0.45f, 0.28f, 0.13f };
            for (int i = 0; i < 3; i++)
            {
                float e = exps[i];
                DrawRectangleRounded(
                    { barX - e, hpBarY - e, hc.barW + e * 2.f, hc.barH + e * 2.f },
                    0.35f, 6, Fade(RED, alphas[i] * pulse));
            }
        }

        DrawRectangleRounded({ barX, hpBarY, hc.barW * hpPct, hc.barH }, 0.3f, 6, fillColor);
        DrawRectangleRoundedLines({ barX, hpBarY, hc.barW, hc.barH }, 0.3f, 6, Fade(WHITE, 0.25f));

        int lFs    = (int)hc.barLabelFs;
        const char* hpLabel = TextFormat("HP  %.0f / %.0f", curHp, maxHp);
        int labelW = MeasureText(hpLabel, lFs);
        DrawText(hpLabel,
            (int)(barX + hc.barW / 2.f - labelW / 2.f),
            (int)(hpBarY + hc.barH / 2.f - lFs / 2.f),
            lFs, BLACK);

    }

    // ── Armour icons — Defense.png, small, centred below the HP bar ─────────
    // Filled = full opacity. Empty = faded so the player can still count slots.
    if (_upgradeDefenseTex.id != 0)
    {
        const int   armour   = _player.GetArmour();
        const int   maxArmour = _player.GetMaxArmour();
        const float scale    = kArmourIconH / (float)_upgradeDefenseTex.height;
        const float iconW    = _upgradeDefenseTex.width * scale;
        const float iconGap  = kArmourIconH * 0.35f;   // matches old circle spacing
        const float totalW   = maxArmour * iconW + (maxArmour - 1) * iconGap;
        float iconX = barX + hc.barW * 0.5f - totalW * 0.5f;
        const float iconY = hpBarY + hc.barH + kArmourGapY;

        for (int i = 0; i < maxArmour; i++)
        {
            Color tint = (i < armour) ? WHITE : Fade(WHITE, 0.18f);
            DrawTextureEx(_upgradeDefenseTex, { iconX, iconY }, 0.f, scale, tint);
            iconX += iconW + iconGap;
        }
    }

    {
        int curMana  = _player.GetMana();
        int maxMana  = _player.GetMaxMana();
        float manaPct = (maxMana > 0) ? (float)curMana / (float)maxMana : 0.f;
        static const Color kManaFill = { 60, 120, 255, 230 };

        DrawRectangleRounded({ barX, manaBarY, hc.barW * manaPct, hc.barH }, 0.3f, 6, kManaFill);
        DrawRectangleRoundedLines({ barX, manaBarY, hc.barW, hc.barH }, 0.3f, 6, Fade(WHITE, 0.25f));

        int lFs = (int)hc.barLabelFs;
        const char* manaLabel = TextFormat("MP  %d / %d", curMana, maxMana);
        int manaLabelW = MeasureText(manaLabel, lFs);
        DrawText(manaLabel,
            (int)(barX + hc.barW / 2.f - manaLabelW / 2.f),
            (int)(manaBarY + hc.barH / 2.f - lFs / 2.f),
            lFs, BLACK);
    }

    // ── EXP bar — sits below the mana bar ────────────────────────────────────
    {
        int   curExp = _player.GetExp();
        int   maxExp = _player.GetExpToNext();
        float expPct = (maxExp > 0) ? std::min((float)curExp / (float)maxExp, 1.f) : 0.f;
        float expBarY = manaBarY + hc.barH + hc.expBarGap;
        static const Color kExpFill = { 160, 60, 255, 220 };

        DrawRectangleRounded({ barX, expBarY, hc.barW * expPct, hc.expBarH }, 0.3f, 4, kExpFill);
        DrawRectangleRoundedLines({ barX, expBarY, hc.barW, hc.expBarH }, 0.3f, 4, Fade(WHITE, 0.22f));

        int eFs = (int)hc.expLabelFs;
        const char* expLabel = (_player.GetLevel() >= _player.GetMaxLevel())
            ? TextFormat("LVL MAX")
            : TextFormat("LVL %d   EXP  %d / %d", _player.GetLevel(), curExp, maxExp);
        int eLW = MeasureText(expLabel, eFs);
        DrawText(expLabel,
            (int)(barX + hc.barW / 2.f - eLW / 2.f),
            (int)(expBarY + hc.expBarH / 2.f - eFs / 2.f),
            eFs, WHITE);
    }

    if (_currentRoomType == RoomType::Elite && _eliteMechanic >= 0)
    {
        static constexpr const char* kMechanicNames[] = {
            "ARENA CONSTRICTION", "INVULNERABILITY LINKS", "PERMANENT ENRAGE",
            "GAP-CLOSER LEAP",    "ROOM HAZARDS"
        };
        static constexpr Color kMechanicColors[] = {
            Color{220, 40, 200, 255}, Color{180, 100, 255, 255}, Color{255, 60,  60, 255},
            Color{255,180,  60, 255}, Color{255,220,  80, 255},
        };
        const char* mechLabel = kMechanicNames[_eliteMechanic];
        Color mechColor = kMechanicColors[_eliteMechanic];
        int mw = MeasureText(mechLabel, 20);
        drawLabelBox(mechLabel, (float)(GetScreenWidth() - mw - (int)hc.actOffsetX), 58.f, 20, mechColor);
    }

    if (_currentRoomType == RoomType::Elite && _eliteEnrageWarningTimer > 0.f)
    {
        const float sw = (float)GetScreenWidth();
        const float sh = (float)GetScreenHeight();
        float alpha = 1.f;
        if (_eliteEnrageWarningTimer > kEliteEnrageWarningDuration - 0.5f)
            alpha = (kEliteEnrageWarningDuration - _eliteEnrageWarningTimer) / 0.5f;
        else if (_eliteEnrageWarningTimer < 0.5f)
            alpha = _eliteEnrageWarningTimer / 0.5f;
        alpha = std::max(0.f, std::min(1.f, alpha));

        const char* line1 = "ELITE ENCOUNTER";
        const char* line2 = "CONDITION: ENRAGED  |  FAST & LETHAL";
        const int sz1 = 48, sz2 = 28;
        const float bannerH = 120.f;
        const float bannerY = sh * 0.38f;
        DrawRectangle(0, (int)bannerY, (int)sw, (int)bannerH, Fade(Color{20,0,0,220}, alpha));
        DrawRectangle(0, (int)bannerY, (int)sw, 3, Fade(Color{200,0,0,255}, alpha));
        DrawRectangle(0, (int)(bannerY + bannerH - 3), (int)sw, 3, Fade(Color{200,0,0,255}, alpha));
        DrawText(line1, (int)(sw/2.f - MeasureText(line1,sz1)/2.f), (int)(bannerY+14.f), sz1, Fade(Color{255,60,60,255},alpha));
        DrawText(line2, (int)(sw/2.f - MeasureText(line2,sz2)/2.f), (int)(bannerY+14.f+sz1+8.f), sz2, Fade(Color{255,180,60,255},alpha));
    }

    if (_debug.IsActive())
        DrawDebugToggleTab();

    if (_touchModeActive)
    {
        DrawTouchAbilityArc();
        _touch.Draw(GetScreenWidth(), GetScreenHeight());

        // Pause button
        Rectangle pauseRec{ (float)_windowWidth - hc.touchPauseW - hc.touchPausePad,
                             hc.touchPausePad, hc.touchPauseW, hc.touchPauseH };
        DrawRectangleRounded(pauseRec, 0.22f, 6, Fade(BLACK, 0.55f));
        DrawRectangleRoundedLines(pauseRec, 0.22f, 6, Fade(WHITE, 0.40f));
        int pw = MeasureText("II", 26);
        DrawText("II",
            (int)(pauseRec.x + pauseRec.width / 2.f - pw / 2.f),
            (int)(pauseRec.y + pauseRec.height / 2.f - 13.f),
            26, RAYWHITE);
    }
    else
    {
        DrawAbilityBar();
    }

    // ── HUD debug editor ──────────────────────────────────────────────────
    if (IsKeyPressed(KEY_NINE))
        _hudEditorActive = !_hudEditorActive;

    if (_hudEditorActive)
    {
        constexpr int kN = 46;
        const char* varNames[kN] = {
            "0  Bar Width",       "1  Bar Height",      "2  Bar Gap",
            "3  Bar Top Pad",     "4  Bar Label Font",
            "5  Gold X",          "6  Gold Y",          "7  Gold Font",
            "8  Enemies X",       "9  Enemies Y",       "10 Enemies Font",
            "11 Act Offset X",    "12 Act Y",            "13 Act Font",
            "14 Mini X",          "15 Mini Y",           "16 Mini Width",
            "17 Dot Boss",        "18 Dot Elite",        "19 Dot Enemy",
            "20 Dot Prop",        "21 Dot Pickup",       "22 Dot Player",
            "23 Slot Size",       "24 Slot Gap",         "25 Slot Bot Pad",
            "26 Key Label Font",  "27 Name Font",
            "28 Joy Radius",      "29 Atk Radius",       "30 Dash Radius",
            "31 Atk Pad Right",   "32 Atk Pad Bot",      "33 Dash Offset",
            "34 Pause Width",     "35 Pause Height",     "36 Pause Pad",
            "37 Atk Label Font",  "38 Dash Label Font",
            "39 Touch Slot Size", "40 Touch Slot Gap",
            "41 Slot Right Pad",  "42 Slot Y Offset",
            "43 EXP Bar Height",  "44 EXP Bar Gap",      "45 EXP Label Font",
        };
        float* vars[kN] = {
            &_hudCfg.barW,        &_hudCfg.barH,        &_hudCfg.barGap,
            &_hudCfg.barTopPad,   &_hudCfg.barLabelFs,
            &_hudCfg.goldX,       &_hudCfg.goldY,       &_hudCfg.goldFs,
            &_hudCfg.enemiesX,    &_hudCfg.enemiesY,    &_hudCfg.enemiesFs,
            &_hudCfg.actOffsetX,  &_hudCfg.actY,        &_hudCfg.actFs,
            &_hudCfg.miniX,       &_hudCfg.miniY,       &_hudCfg.miniW,
            &_hudCfg.miniDotBoss, &_hudCfg.miniDotElite,&_hudCfg.miniDotEnemy,
            &_hudCfg.miniDotProp, &_hudCfg.miniDotPickup,&_hudCfg.miniDotPlayer,
            &_hudCfg.slotSz,      &_hudCfg.slotGap,     &_hudCfg.slotBotPad,
            &_hudCfg.slotKeyFs,   &_hudCfg.slotNameFs,
            &_hudCfg.touchJoyR,   &_hudCfg.touchAtkR,   &_hudCfg.touchDashR,
            &_hudCfg.touchAtkPadR,&_hudCfg.touchAtkPadB,&_hudCfg.touchDashOffset,
            &_hudCfg.touchPauseW, &_hudCfg.touchPauseH, &_hudCfg.touchPausePad,
            &_hudCfg.touchAtkFs,  &_hudCfg.touchDashFs,
            &_hudCfg.touchSlotSz, &_hudCfg.touchSlotGap,
            &_hudCfg.touchSlotRightPad, &_hudCfg.touchSlotYOff,
            &_hudCfg.expBarH, &_hudCfg.expBarGap, &_hudCfg.expLabelFs,
        };

        if (IsKeyPressed(KEY_UP))
            _hudEditorSelIdx = (_hudEditorSelIdx - 1 + kN) % kN;
        if (IsKeyPressed(KEY_DOWN))
            _hudEditorSelIdx = (_hudEditorSelIdx + 1) % kN;
        if (IsKeyDown(KEY_RIGHT)) *vars[_hudEditorSelIdx] += 1.f;
        if (IsKeyDown(KEY_LEFT))  *vars[_hudEditorSelIdx] -= 1.f;

        if (IsKeyPressed(KEY_S))
        {
            TraceLog(LOG_INFO, "=== HUD Editor Export ===");
            TraceLog(LOG_INFO, "barW=%g barH=%g barGap=%g barTopPad=%g barLabelFs=%g",
                hc.barW, hc.barH, hc.barGap, hc.barTopPad, hc.barLabelFs);
            TraceLog(LOG_INFO, "goldX=%g goldY=%g goldFs=%g", hc.goldX, hc.goldY, hc.goldFs);
            TraceLog(LOG_INFO, "enemiesX=%g enemiesY=%g enemiesFs=%g", hc.enemiesX, hc.enemiesY, hc.enemiesFs);
            TraceLog(LOG_INFO, "actOffsetX=%g actY=%g actFs=%g", hc.actOffsetX, hc.actY, hc.actFs);
            TraceLog(LOG_INFO, "miniX=%g miniY=%g miniW=%g", hc.miniX, hc.miniY, hc.miniW);
            TraceLog(LOG_INFO, "miniDots: boss=%g elite=%g enemy=%g prop=%g pickup=%g player=%g",
                hc.miniDotBoss, hc.miniDotElite, hc.miniDotEnemy, hc.miniDotProp, hc.miniDotPickup, hc.miniDotPlayer);
            TraceLog(LOG_INFO, "slotSz=%g slotGap=%g slotBotPad=%g slotKeyFs=%g slotNameFs=%g",
                hc.slotSz, hc.slotGap, hc.slotBotPad, hc.slotKeyFs, hc.slotNameFs);
            TraceLog(LOG_INFO, "touchJoyR=%g atkR=%g dashR=%g atkPadR=%g atkPadB=%g dashOff=%g",
                hc.touchJoyR, hc.touchAtkR, hc.touchDashR, hc.touchAtkPadR, hc.touchAtkPadB, hc.touchDashOffset);
            TraceLog(LOG_INFO, "pauseW=%g pauseH=%g pausePad=%g atkFs=%g dashFs=%g",
                hc.touchPauseW, hc.touchPauseH, hc.touchPausePad, hc.touchAtkFs, hc.touchDashFs);
            TraceLog(LOG_INFO, "touchSlotSz=%g touchSlotGap=%g touchSlotRightPad=%g touchSlotYOff=%g",
                hc.touchSlotSz, hc.touchSlotGap, hc.touchSlotRightPad, hc.touchSlotYOff);
            TraceLog(LOG_INFO, "expBarH=%g expBarGap=%g expLabelFs=%g",
                hc.expBarH, hc.expBarGap, hc.expLabelFs);
            TraceLog(LOG_INFO, "touchSlotOffsets: [0]=(%.1f,%.1f) [1]=(%.1f,%.1f) [2]=(%.1f,%.1f) [3]=(%.1f,%.1f)",
                _touchSlotOffset[0].x, _touchSlotOffset[0].y,
                _touchSlotOffset[1].x, _touchSlotOffset[1].y,
                _touchSlotOffset[2].x, _touchSlotOffset[2].y,
                _touchSlotOffset[3].x, _touchSlotOffset[3].y);
        }

        // Two-column panel
        constexpr int   kHalf = (kN + 1) / 2;  // 20
        constexpr float colW  = 310.f;
        constexpr float colGap= 20.f;
        constexpr float rowH  = 30.f;
        const float panW = colW * 2.f + colGap;
        const float panH = 40.f + kHalf * rowH;
        const float sw   = (float)GetScreenWidth();
        const float panX = sw * 0.5f - panW * 0.5f;
        const float panY = 10.f;

        DrawRectangle((int)panX, (int)panY, (int)panW, (int)panH, Fade(BLACK, 0.82f));
        DrawRectangleLines((int)panX, (int)panY, (int)panW, (int)panH, DARKGRAY);
        DrawLine((int)(panX+colW+colGap*0.5f),(int)(panY+36.f),
                 (int)(panX+colW+colGap*0.5f),(int)(panY+panH-4.f), DARKGRAY);
        DrawText("HUD EDITOR  [9] close", (int)(panX+8.f),(int)(panY+6.f),11,GRAY);
        DrawText("[UP/DOWN] sel  [L/R] nudge  [S] export  |  drag touch slots with mouse",
            (int)(panX+8.f),(int)(panY+20.f),10,DARKGRAY);

        for (int i = 0; i < kN; i++)
        {
            int   col  = i / kHalf;
            int   row  = i % kHalf;
            float cx   = panX + col * (colW + colGap);
            float ry   = panY + 38.f + row * rowH;
            bool  sel  = (i == _hudEditorSelIdx);
            Color col_c= sel ? YELLOW : WHITE;

            if (sel)
                DrawText("->", (int)(cx+4.f), (int)(ry+rowH*0.5f-7.f), 13, YELLOW);
            DrawText(varNames[i], (int)(cx+26.f), (int)(ry+rowH*0.5f-7.f), 13, col_c);

            const char* valStr = TextFormat("%.1f", *vars[i]);
            int valW = MeasureText(valStr, 13);
            DrawText(valStr, (int)(cx+colW-valW-6.f), (int)(ry+rowH*0.5f-7.f), 13, col_c);
        }
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
    DrawText(("Gold: " + std::to_string(_player.GetGold())).c_str(), 20, 10, 30, GOLD);
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

        Color fillColor = (hpPct > 0.70f) ? GREEN :
                          (hpPct > 0.30f) ? YELLOW : RED;

        if (hpPct <= 0.30f)
        {
            float pulse    = (sinf((float)GetTime() * (2.f * PI / 3.f)) + 1.f) * 0.5f;
            float exps[]   = { 4.f, 9.f, 15.f };
            float alphas[] = { 0.45f, 0.28f, 0.13f };
            for (int i = 0; i < 3; i++)
            {
                float e = exps[i];
                DrawRectangleRounded(
                    { barX - e, hpBarY - e, kBarW + e * 2.f, kBarH + e * 2.f },
                    0.35f, 6, Fade(RED, alphas[i] * pulse));
            }
        }

        DrawRectangleRounded({ barX, hpBarY, kBarW, kBarH }, 0.3f, 6, Fade(BLACK, 0.75f));
        DrawRectangleRounded({ barX, hpBarY, kBarW * hpPct, kBarH }, 0.3f, 6, fillColor);
        DrawRectangleRoundedLines({ barX, hpBarY, kBarW, kBarH }, 0.3f, 6, Fade(WHITE, 0.25f));

        const char* hpLabel = TextFormat("HP  %.0f / %.0f", curHp, maxHp);
        int labelW = MeasureText(hpLabel, 18);
        DrawText(hpLabel,
            (int)(barX + kBarW / 2.f - labelW / 2.f),
            (int)(hpBarY + kBarH / 2.f - 9.f),
            18, BLACK);
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
            18, BLACK);
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
    const HUDConfig& hc = _hudCfg;
    const int   totalSlots = _player.GetMaxAbilitySlots();
    const float slotSize   = hc.slotSz;
    const float slotGap    = hc.slotGap;
    const float slotY      = (float)GetScreenHeight() - hc.slotBotPad - slotSize;
    const float totalW     = totalSlots * slotSize + (totalSlots - 1) * slotGap;
    const float startX     = GetScreenWidth() / 2.f - totalW / 2.f;
    const int   keyFs      = (int)hc.slotKeyFs;
    const int   nameFs     = (int)hc.slotNameFs;

    Vector2 mouse = GetMousePosition();

    for (int i = 0; i < totalSlots; i++)
    {
        AbilityType ability = _player.GetLearnedAbility(i);
        bool isEmpty = (ability == AbilityType::None);
        bool canCast = !isEmpty && _player.GetMana() >= GetAbilityManaCost(ability);
        float x = startX + i * (slotSize + slotGap);
        Rectangle slot{ x, slotY, slotSize, slotSize };
        bool hovered = !isEmpty && CheckCollisionPointRec(mouse, slot);

        Color bgColor     = isEmpty ? Fade(BLACK, 0.30f) : (hovered ? Fade(BLACK, 0.80f) : Fade(BLACK, 0.55f));
        Color borderColor = isEmpty ? Fade(WHITE, 0.12f) :
            hovered ? Fade(GOLD, 0.70f) :
            canCast ? Fade(LIGHTGRAY, 0.35f) : Fade(RED, 0.40f);
        DrawRectangleRounded(slot, 0.18f, 6, bgColor);
        DrawRectangleRoundedLines(slot, 0.18f, 6, borderColor);

        if (isEmpty)
        {
            DrawText(GetKeyName(_player.GetAbilityKey(i)),
                (int)(x + 6.f), (int)(slotY + 6.f), keyFs, Fade(WHITE, 0.25f));
            continue;
        }

        DrawText(GetKeyName(_player.GetAbilityKey(i)),
            (int)(x + 6.f), (int)(slotY + 6.f), keyFs, Fade(WHITE, 0.6f));

        if (hovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
            _player.TriggerAbilityCast(i);

        const Texture2D* iconTex = &_abilityIconFireTex;
        if (ability == AbilityType::IceSpread || ability == AbilityType::IceBolt || ability == AbilityType::IceUltimate)
            iconTex = &_abilityIconIceTex;
        else if (ability == AbilityType::ElectricSpread || ability == AbilityType::ElectricBolt || ability == AbilityType::ElectricUltimate)
            iconTex = &_abilityIconElectricTex;

        Color iconTint    = canCast ? WHITE : Fade(WHITE, 0.35f);
        float maxIconSize = slotSize * 0.55f;
        float iconScale   = std::min(maxIconSize / (float)iconTex->width, maxIconSize / (float)iconTex->height);
        float iw = iconTex->width  * iconScale;
        float ih = iconTex->height * iconScale;
        float cx = x + slotSize * 0.5f;
        float cy = slotY + slotSize * 0.42f;
        DrawTextureEx(*iconTex, { cx - iw * 0.5f, cy - ih * 0.5f }, 0.f, iconScale, iconTint);

        const char* abilityName = GetAbilityName(ability);
        int nameW = MeasureText(abilityName, nameFs);
        DrawText(abilityName,
            (int)(x + slotSize / 2.f - nameW / 2.f),
            (int)(slotY + slotSize - nameFs - 6.f),
            nameFs, canCast ? RAYWHITE : Fade(GRAY, 0.6f));

        int abilityLv = _player.GetAbilityLevel(ability);
        if (abilityLv > 1)
        {
            const char* badge = TextFormat("Lv%d", abilityLv);
            int badgeW = MeasureText(badge, nameFs);
            Color badgeColor = (abilityLv >= 3) ? GOLD : Fade(SKYBLUE, 0.9f);
            DrawText(badge,
                (int)(x + slotSize - badgeW - 4.f),
                (int)(slotY + slotSize - nameFs - 4.f),
                nameFs, badgeColor);
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
        bool        canCast  = !isEmpty && _player.CanCastAbility(ability);
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
        roomLabel = "- Boss Area -";
        roomColor = RED;
        break;
    case RoomType::Elite:
        roomLabel = "Elite Area";
        roomColor = Color{ 255, 140, 0, 255 };  // orange
        break;
    case RoomType::Rest:
        roomLabel = "Rest Area";
        roomColor = Color{ 100, 230, 120, 255 }; // green
        break;
    case RoomType::Treasure:
        roomLabel = "Treasure Area";
        roomColor = GOLD;
        break;
    case RoomType::Store:
        roomLabel = "Shop Area";
        roomColor = Color{ 220, 200, 60, 255 };
        break;
    default:  // Standard
        roomLabel = "Standard Area";
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

void Engine::GenerateLevelUpOptions(LevelUpOfferContext context)
{
    // Separate pools by rarity. Ability learn/upgrade cards remain on the boss
    // reward screen; these offers are purely the RPG stat-growth layer.
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

    _levelUpOfferContext = context;

    auto shufflePool = [](UpgradeType* pool, int size) {
        for (int i = 0; i < size; i++)
        {
            int j = GetRandomValue(i, size - 1);
            UpgradeType tmp = pool[i];
            pool[i] = pool[j];
            pool[j] = tmp;
        }
    };
    shufflePool(commonPool, 6);
    shufflePool(rarePool,   6);
    shufflePool(epicPool,   5);

    const int level = _player.GetLevel();
    const int roomProgress = ((_currentAct - 1) * 5) + std::max(0, _currentRoom - 1);

    int commonW = 0;
    int rareW   = 0;
    int epicW   = 0;

    switch (context)
    {
    case LevelUpOfferContext::TreasureBasic:
        commonW = 100;
        break;

    case LevelUpOfferContext::EliteReward:
        rareW = std::max(55, 82 - roomProgress * 3);
        epicW = std::min(45, 18 + roomProgress * 3 + std::max(0, level - 5) * 2);
        break;

    case LevelUpOfferContext::StoreStock:
        commonW = std::max(30, 78 - roomProgress * 4);
        rareW   = std::min(55, 22 + roomProgress * 3);
        epicW   = (_currentAct >= 3) ? std::min(15, 5 + (_currentAct - 3) * 5) : 0;
        break;

    case LevelUpOfferContext::NormalLevel:
    default:
        commonW = std::max(18, 72 - level * 5 - roomProgress * 3);
        rareW   = std::min(57, 20 + level * 4 + roomProgress * 2);
        epicW   = std::max(6, 8 + std::max(0, level - 4) * 2 + roomProgress);
        break;
    }

    int cIdx = 0, rIdx = 0, eIdx = 0;
    for (int i = 0; i < 3; i++)
    {
        int total = commonW + rareW + epicW;
        int roll  = (total > 0) ? GetRandomValue(0, total - 1) : 0;

        UpgradeType picked = UpgradeType::AttackPower;
        if (roll < commonW && cIdx < 6)                     picked = commonPool[cIdx++];
        else if (roll < commonW + rareW && rIdx < 6)       picked = rarePool[rIdx++];
        else if (eIdx < 5)                                 picked = epicPool[eIdx++];
        else if (rIdx < 6)                                 picked = rarePool[rIdx++];
        else if (cIdx < 6)                                 picked = commonPool[cIdx++];

        _levelUpOptions[i] = picked;
    }

    // Ability row is only used for the run-start choice right now.
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
                    if (_bossesDefeated >= 2)
                        _gameState = GameState::DemoEnd;
                    else
                    {
                        _roomClearPending = true;
                        _gameState = GameState::Play;
                    }
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
                    if (_bossesDefeated >= 2)
                        _gameState = GameState::DemoEnd;
                    else
                    {
                        _roomClearPending = true;
                        _gameState = GameState::Play;
                    }
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
    const float contScale = _touchModeActive ? 1.3f : 1.f;
    Rectangle contBtn{sw/2.f - (220.f * contScale) / 2.f, sh*0.87f, 220.f * contScale, 52.f * contScale};
    bool contHov = ready && CheckCollisionPointRec(mouse, contBtn);
    DrawRectangleRounded(contBtn, 0.4f, 8, contHov ? Color{35,60,35,235} : Color{18,35,18,200});
    DrawRectangleRoundedLines(contBtn, 0.4f, 8, contHov ? Color{100,210,100,255} : Color{55,120,55,160});
    const char* contTxt = "Continue";
    int contSz = _touchModeActive ? 31 : 24;
    DrawText(contTxt, (int)(contBtn.x + contBtn.width/2.f - MeasureText(contTxt, contSz)/2.f),
             (int)(contBtn.y + contBtn.height/2.f - contSz/2.f), contSz,
             contHov ? Color{160,255,160,255} : RAYWHITE);
    if (ready && contHov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
    {
        if (_pendingRoomChoice)
        {
            _pendingRoomChoice = false;
            if (_bossesDefeated >= 2)
                _gameState = GameState::DemoEnd;
            else
            {
                _roomClearPending = true;
                _gameState = GameState::Play;
            }
        }
        else
            _gameState = GameState::Play;
    }
}

// ── DrawDemoEnd ───────────────────────────────────────────────────────────────
void Engine::DrawDemoEnd()
{
    DemoEndRenderContext ctx{};
    ctx.touchModeActive = _touchModeActive;
    ctx.demoCompleted = _demoCompleted;
    ctx.enemiesKilled = _enemiesKilled;
    ctx.bossesDefeated = _bossesDefeated;
    ctx.playerGold = _player.GetGold();
    ctx.playerLevel = _player.GetLevel();
    _overlayRenderer.DrawDemoEnd(ctx);
}

// ── UpdateExpTally ────────────────────────────────────────────────────────────
// Drains _pendingExp into the player at 50 EXP/sec with NO interruptions.
// Once the bar is full the player presses Continue, then level-up choices
// fire (one per level gained).  Skip input drains the remainder instantly.
void Engine::UpdateExpTally(float dt)
{
    bool skip = IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_M)
             || (_touchModeActive && IsMouseButtonPressed(MOUSE_LEFT_BUTTON));

    if (_expTallyDone)
    {
        if (skip)
        {
            _tallyChoiceChaining = false;
            if (_currentRoomType == RoomType::Elite && !_eliteRewardGranted)
            {
                _eliteRewardGranted = true;
                GenerateLevelUpOptions(LevelUpOfferContext::EliteReward);
                _levelUpReturnState = GameState::ExpTally;
                _levelUpOpenTimer   = 0.25f;
                _gameState          = GameState::LevelUpChoice;
            }
            else if (_currentRoomType == RoomType::Boss && _lastAbilityChoiceWave != _wave)
            {
                _lastAbilityChoiceWave  = _wave;
                GenerateAbilityChoiceOptions();
                _abilityChoiceOpenTimer = 0.5f;
                _pendingRoomChoice      = true;
                _gameState = GameState::AbilityChoice;
            }
            else
            {
                _roomClearPending = true;
                _gameState = GameState::Play;
            }
        }
        return;
    }

    // ── Bar still draining ────────────────────────────────────────────────────

    if (skip)
    {
        // Instantly drain all remaining EXP — no level-up interruption during drain.
        if (_pendingExp > 0.f)
        {
            _player.AddExp((int)_pendingExp);
            _pendingExp    = 0.f;
            _expTallyAccum = 0.f;
        }
        _tallyLevelUpsRemaining = _player.GetLevel() - _tallyStartLevel;
        _expTallyDone = true;
        return;
    }

    // Animated drain — 50 EXP per second, no interruptions.
    static constexpr float kDrainRate = 50.f;
    float drain = std::min(kDrainRate * dt, _pendingExp);
    _pendingExp    -= drain;
    _expTallyAccum += drain;

    int wholeExp = (int)_expTallyAccum;
    if (wholeExp > 0)
    {
        _expTallyAccum -= (float)wholeExp;
        _player.AddExp(wholeExp);   // level-ups handled silently inside AddExp
    }

    if (_pendingExp <= 0.f)
    {
        _pendingExp             = 0.f;
        _expTallyAccum          = 0.f;
        _tallyLevelUpsRemaining = _player.GetLevel() - _tallyStartLevel;
        _expTallyDone           = true;
    }
}

// ── DrawExpTally ──────────────────────────────────────────────────────────────
// Dark overlay drawn on top of the game world showing EXP bar filling and
// the player's current level.  Dismiss hint appears once the bar is full.
void Engine::DrawExpTally()
{
    ExpTallyRenderContext ctx{};
    ctx.player = &_player;
    ctx.tallyStartLevel = _tallyStartLevel;
    ctx.pendingExp = _pendingExp;
    ctx.expTallyDone = _expTallyDone;
    ctx.tallyLevelUpsRemaining = _tallyLevelUpsRemaining;
    ctx.tallyChoiceChaining = _tallyChoiceChaining;
    ctx.touchModeActive = _touchModeActive;
    _overlayRenderer.DrawExpTally(ctx);
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
    Biome actBiome = GetBiomeForAct(_currentAct);
    Color bgDark = Color{ 78, 62, 40, 255 };
    Color bgLight = Color{ 110, 88, 58, 255 };
    if (actBiome == Biome::Forest)
    {
        bgDark  = Color{ 30, 74, 42, 255 };
        bgLight = Color{ 48, 112, 66, 255 };
    }
    DrawScrollingCheckerboard(sw, sh, bgDark, bgLight, 18.f, 10.f);

    // ── Header — centred over the node graph area ─────────────────────────
    // Graph spans 30–76% of the screen; journey panel occupies 78–97%.
    const float mapCentreX = sw * _mapHeaderX;
    std::string header = "Act " + std::to_string(_currentAct)
                       + "  -  " + GetBiomeName(actBiome);
    int hSz = (int)_mapHeaderFs;
    DrawText(header.c_str(),
        (int)(mapCentreX - MeasureText(header.c_str(), hSz) / 2.f), (int)_mapHeaderY, hSz, Color{255, 214, 102, 255});
    const char* sub = "Choose your path";
    const float subCentreX = sw * _mapSubX;
    const int subFs = (int)_mapSubFs;
    DrawText(sub,
        (int)(subCentreX - MeasureText(sub, subFs) / 2.f), (int)_mapSubY, subFs,
        Color{206, 242, 255, 210});

    if (_actMap.empty()) return;

    // ── Helpers ───────────────────────────────────────────────────────────
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
    auto roomIcon = [&](RoomType rt) -> Texture2D* {
        switch (rt) {
        case RoomType::Standard: return &_mapIconNormal;
        case RoomType::Elite:    return &_mapIconElite;
        case RoomType::Rest:     return &_mapIconRest;
        case RoomType::Store:    return &_mapIconShop;
        case RoomType::Treasure: return &_mapIconTreasure;
        case RoomType::Boss:     return &_mapIconBoss;
        default:                 return nullptr;
        }
    };
    auto nodeDesc = [](RoomType rt) -> const char* {
        switch (rt) {
        case RoomType::Boss:     return "Act finale  -  boss area";
        case RoomType::Elite:    return "Miniboss area  -  rarer reward";
        case RoomType::Rest:     return "Safe area  -  heal pickups appear";
        case RoomType::Treasure: return "Reward area  -  free upgrade card";
        case RoomType::Store:    return "Shop area  -  placeholder for now";
        default:                 return "Combat area  -  standard area reward";
        }
    };

    // ── Left panel: stats + legend ────────────────────────────────────────
    {
        const float pX  = 48.f;
        const float pY  = 98.f;
        const float pH  = sh - pY - 98.f;
        const float pW  = 490.f;
        const float pad = 30.f;
        const float legendIndent = 20.f;
        const float titleSize = 32.f;
        const float statFont = 28.f;
        const float baseStatRowH = 44.f;
        const float legendIconSize = 56.f;
        const float legendLabelSize = 28.f;
        const float legendDescSize = 21.f;
        const float baseLegendRowH = 80.f;
        const float legendGap = 22.f;
        const float titleToRuleGap = 8.f;
        const float ruleToRowsGap = 16.f;
        const float sectionGapBase = 22.f;

        // Pre-calculate content height so the panel can grow to roughly match
        // the graph height without clipping smaller screens.
        int statRows = 5;  // Level, HP, MP, Attack, Defense
        if (_player.GetLevel() < _player.GetMaxLevel()) statRows++;  // EXP row
        float contentH = pad
                       + titleSize
                       + titleToRuleGap
                       + ruleToRowsGap
                       + statRows * baseStatRowH
                       + sectionGapBase
                       + titleSize
                       + titleToRuleGap
                       + ruleToRowsGap
                       + 6.f * baseLegendRowH
                       + pad;
        float panelH = std::max(contentH, pH);
        float extraSpace = panelH - contentH;
        float statRowH = baseStatRowH + extraSpace * 0.12f / (float)statRows;
        float legendRowH = baseLegendRowH + extraSpace * 0.88f / 6.f;
        float sectionGap = sectionGapBase + extraSpace * 0.08f;

        // Panel background
        DrawRectangleRounded({ pX, pY, pW, panelH }, 0.05f, 8,
            Fade(Color{12, 71, 84, 255}, 0.92f));
        DrawRectangleRoundedLines({ pX, pY, pW, panelH }, 0.05f, 8,
            Fade(Color{130, 235, 255, 255}, 0.30f));

        float cy         = pY + pad;
        float rightEdge  = pX + pW - pad;

        // ── Stats ──────────────────────────────────────────────────────
        DrawText("PLAYER STATS", (int)(pX + pad), (int)cy, (int)titleSize, Color{255, 214, 102, 255});
        cy += titleSize + titleToRuleGap;
        DrawLineEx({ pX + pad, cy }, { rightEdge, cy }, 1.f, Fade(Color{130, 235, 255, 255}, 0.55f));
        cy += ruleToRowsGap;

        // stat row: label left, coloured value right
        auto statRow = [&](const char* lbl, const char* val, Color valCol = RAYWHITE)
        {
            DrawText(lbl, (int)(pX + pad), (int)cy, (int)statFont, Color{188, 228, 238, 228});
            int vw = MeasureText(val, (int)statFont);
            DrawText(val, (int)(rightEdge - vw), (int)cy, (int)statFont, valCol);
            cy += statRowH;
        };

        statRow("Level",   TextFormat("%d / %d", _player.GetLevel(), _player.GetMaxLevel()));

        float hpPct = (_player.GetMaxHealthValue() > 0.f)
            ? _player.GetHealthValue() / _player.GetMaxHealthValue() : 0.f;
        Color hpCol = hpPct > 0.6f ? GREEN : hpPct > 0.30f ? YELLOW : RED;
        statRow("HP",
            TextFormat("%d / %d",
                (int)std::ceil(_player.GetHealthValue()),
                (int)std::ceil(_player.GetMaxHealthValue())), hpCol);

        statRow("MP",
            TextFormat("%d / %d", _player.GetMana(), _player.GetMaxMana()),
            Color{100, 160, 255, 255});

        statRow("Attack", TextFormat("%d", (int)std::ceil((float)_player.GetMeleeDamage())));
        statRow("Armour", TextFormat("%d / %d", _player.GetArmour(), _player.GetMaxArmour()),
            Color{ 120, 180, 255, 255 });

        if (_player.GetLevel() < _player.GetMaxLevel())
            statRow("EXP",
                TextFormat("%d / %d", _player.GetExp(), _player.GetExpToNext()),
                Color{220, 190, 50, 220});

        // ── Section divider ────────────────────────────────────────────
        cy += sectionGap;
        DrawLineEx({ pX + pad, cy }, { rightEdge, cy }, 1.f, Fade(Color{130, 235, 255, 255}, 0.24f));
        cy += 18.f;

        // ── Legend ─────────────────────────────────────────────────────
        DrawText("LEGEND", (int)(pX + pad), (int)cy, (int)titleSize, Color{255, 214, 102, 255});
        cy += titleSize + titleToRuleGap;
        DrawLineEx({ pX + pad, cy }, { rightEdge, cy }, 1.f, Fade(Color{130, 235, 255, 255}, 0.55f));
        cy += ruleToRowsGap;

        const float iconX = pX + pad + legendIndent;
        const float txtX  = iconX + legendIconSize + legendGap;

        auto legendRow = [&](Texture2D* icon, Color circleCol,
                             const char* name, const char* desc)
        {
            float iconY = cy + (legendRowH - legendIconSize) * 0.5f;
            if (icon && icon->id > 0)
            {
                Rectangle src = { 0, 0, (float)icon->width, (float)icon->height };
                DrawTexturePro(*icon, src, { iconX, iconY, legendIconSize, legendIconSize }, {}, 0.f, WHITE);
            }
            else
            {
                // Coloured circle for rooms without a PNG icon (Rest, Boss)
                Vector2 c = { iconX + legendIconSize * 0.5f, iconY + legendIconSize * 0.5f };
                DrawCircleV(c, legendIconSize * 0.44f, Fade(circleCol, 0.65f));
                DrawCircleLines((int)c.x, (int)c.y, legendIconSize * 0.44f, circleCol);
            }
            float labelY = cy + 3.f;
            float descY  = cy + legendLabelSize + 7.f;
            DrawText(name, (int)txtX, (int)labelY, (int)legendLabelSize, RAYWHITE);
            DrawText(desc, (int)txtX, (int)descY, (int)legendDescSize, Color{188, 228, 238, 215});
            cy += legendRowH;
        };

        legendRow(&_mapIconNormal,   { 80,130,220,255 }, "NORMAL AREA",   "Standard combat area");
        legendRow(&_mapIconElite,    {230,130, 20,255 }, "ELITE AREA",    "Miniboss area");
        legendRow(&_mapIconRest,     { 60,190, 90,255 }, "REST AREA",     "Safe area with food");
        legendRow(&_mapIconTreasure, {220,190, 30,255 }, "TREASURE AREA", "Free upgrade card");
        legendRow(&_mapIconShop,     { 80,190,230,255 }, "SHOP AREA",     "Placeholder shop area");
        legendRow(&_mapIconBoss,     {220, 40, 40,255 }, "BOSS AREA",     "Act finale");
    }

    // ── Right panel: journey history ──────────────────────────────────────
    {
        const float jX  = sw * _mapJourneyX;
        const float jY  = 98.f;
        const float jW  = sw - jX - 48.f;
        const float jH  = sh - jY - 98.f;
        const float pad = _mapPad;

        DrawRectangleRounded({ jX, jY, jW, jH }, 0.05f, 8,
            Fade(Color{12, 71, 84, 255}, 0.92f));
        DrawRectangleRoundedLines({ jX, jY, jW, jH }, 0.05f, 8,
            Fade(Color{130, 235, 255, 255}, 0.30f));

        float cy = jY + pad;
        DrawText("JOURNEY", (int)(jX + pad), (int)cy, (int)_mapTitleFs, Color{255, 214, 102, 255});
        cy += _mapTitleFs + 6.f;
        DrawLineEx({ jX + pad, cy }, { jX + jW - pad, cy }, 1.f,
            Fade(Color{130, 235, 255, 255}, 0.55f));
        cy += 14.f;

        // Visited-room tiles — one per completed node, stamped with the room
        // icon and crossed with a red diagonal slash.
        int drawn = 0;
        for (const auto& node : _actMap)
            if (node.completed) drawn++;

        int maxJourneySlots = 0;
        for (const auto& node : _actMap)
            maxJourneySlots = std::max(maxJourneySlots, node.row + 1);
        maxJourneySlots = std::max(1, maxJourneySlots);

        const float baseGap = _mapSqGap;
        const float availW = jW - pad * 2.f;
        float fitSqSz = _mapSqSz;
        fitSqSz = std::min(fitSqSz, (availW - baseGap * (maxJourneySlots - 1)) / (float)maxJourneySlots);
        fitSqSz = std::max(20.f, fitSqSz);

        float fitGap = (maxJourneySlots > 1)
            ? std::min(baseGap, std::max(4.f, (availW - fitSqSz * maxJourneySlots) / (float)(maxJourneySlots - 1)))
            : 0.f;
        const float historyRowH = fitSqSz;
        const float historyStartX = jX + pad;

        if (drawn == 0)
        {
            DrawText("No rooms cleared yet",
                (int)historyStartX, (int)(cy + historyRowH * 0.5f - _mapNoRoomsFs * 0.5f),
                (int)_mapNoRoomsFs, RAYWHITE);
        }
        else
        {
            float sx = historyStartX;

            for (const auto& node : _actMap)
            {
                if (!node.completed) continue;

                Color col = nodeColor(node.type);
                Rectangle tile{ sx, cy, fitSqSz, fitSqSz };
                DrawRectangleRounded(tile, 0.25f, 4, Fade(BLACK, 0.92f));
                DrawRectangleRoundedLines(tile, 0.25f, 4, Fade(col, 0.95f));

                if (Texture2D* icon = roomIcon(node.type); icon && icon->id > 0)
                {
                    const float iconPad = std::max(4.f, fitSqSz * 0.12f);
                    Rectangle src = { 0.f, 0.f, (float)icon->width, (float)icon->height };
                    Rectangle dst = {
                        sx + iconPad,
                        cy + iconPad,
                        fitSqSz - iconPad * 2.f,
                        fitSqSz - iconPad * 2.f
                    };
                    DrawTexturePro(*icon, src, dst, {}, 0.f, WHITE);
                }

                const float slashPad = std::max(3.f, fitSqSz * 0.10f);
                DrawLineEx(
                    { sx + slashPad,               cy + fitSqSz - slashPad },
                    { sx + fitSqSz - slashPad,     cy + slashPad },
                    std::max(3.f, fitSqSz * 0.08f),
                    Color{ 215, 55, 55, 255 });

                sx += fitSqSz + fitGap;
            }
        }

        cy += historyRowH + 12.f;

        cy += 16.f;
        DrawText(TextFormat("Gold:  %d", _player.GetGold()),
            (int)(jX + pad), (int)cy, (int)_mapRoomsFs, Color{255, 214, 102, 220});

        // ── Biome progress diamonds — size-aware zone that lifts upward as
        // the diamonds get larger so the divider and label make room ─────────
        {
            const float minDivY = cy + _mapRoomsFs + 20.f;
            const float zoneBot = jY + jH - pad;
            const float wantedHalf = _mapDiamondSz;
            const float wantedSlot = wantedHalf * 2.35f;
            const float wantedZoneH = wantedSlot * (float)kTotalActs;
            const float wantedHeadH = _mapBiomeTitleFs + 22.f;
            const float divY = std::max(minDivY, zoneBot - wantedZoneH - wantedHeadH);
            const float zoneTop = divY + wantedHeadH;
            const float zoneH   = std::max(1.f, zoneBot - zoneTop);

            DrawLineEx({ jX + pad, divY }, { jX + jW - pad, divY }, 1.f,
                Fade(Color{130, 235, 255, 255}, 0.30f));
            DrawText("BIOMES", (int)(jX + pad), (int)(divY + 4.f), (int)_mapBiomeTitleFs,
                Color{255, 214, 102, 200});

            const float slot   = zoneH / (float)kTotalActs;
            const float halfW  = (jW - pad * 2.f) * 0.42f;
            const float half   = std::min(_mapDiamondSz, std::min(halfW, slot * 0.45f));
            const float dcx    = jX + jW * 0.5f;

            auto drawDiamond = [&](float cx, float dcy, float h, const char* label, int state)
            {
                if (state != 0)
                {
                    Color fill = (state == 1)
                        ? Color{255, 185, 30, 230}
                        : Color{12,  12,  20, 220};
                    DrawTriangle({cx, dcy - h}, {cx + h, dcy}, {cx - h, dcy}, fill);
                    DrawTriangle({cx - h, dcy}, {cx + h, dcy}, {cx, dcy + h}, fill);
                }

                float thick  = (state == 1) ? 2.5f : 1.5f;
                Color border = (state == 1)
                    ? Color{255, 230, 120, 255}
                    : Color{130, 130, 140, 200};

                DrawLineEx({cx,     dcy - h}, {cx + h, dcy    }, thick, border);
                DrawLineEx({cx + h, dcy    }, {cx,     dcy + h}, thick, border);
                DrawLineEx({cx,     dcy + h}, {cx - h, dcy    }, thick, border);
                DrawLineEx({cx - h, dcy    }, {cx,     dcy - h}, thick, border);

                int fs = (int)(h * _mapBiomeLabelFs);
                fs = std::max(9, std::min(fs, 24));
                Color tc = (state == 0) ? Color{110, 110, 120, 180}
                         : (state == 1) ? Color{255, 245, 190, 255}
                                        : Color{ 70,  70,  85, 200};
                int tw2 = MeasureText(label, fs);
                DrawText(label, (int)(cx - tw2 / 2.f), (int)(dcy - fs / 2.f), fs, tc);
            };

            for (int i = 0; i < kTotalActs; i++)
            {
                int   act   = kTotalActs - i;
                float dcy   = zoneTop + (i + 0.5f) * slot;
                int   state = (act < _currentAct)  ? 0
                            : (act == _currentAct) ? 1
                                                   : 2;
                const char* name = ((int)_biomeSequence.size() >= act)
                    ? GetBiomeName(_biomeSequence[act - 1])
                    : "?";
                drawDiamond(dcx, dcy, half, name, state);
            }
        }
    }

    // ── Connection lines ──────────────────────────────────────────────────
    for (int i = 0; i < (int)_actMap.size(); i++)
    {
        const MapNode& n = _actMap[i];
        for (int nextIdx : n.nextNodes)
        {
            if (nextIdx < 0 || nextIdx >= (int)_actMap.size()) continue;
            const MapNode& m = _actMap[nextIdx];
            Color lc = n.completed ? Color{70, 136, 152, 185} : Color{120, 214, 234, 120};
            DrawLineEx(n.drawPos, m.drawPos, 3.5f, lc);
        }
    }

    // ── Nodes ─────────────────────────────────────────────────────────────
    const float kIconSz   = _mapIconSz;
    const float kIconHalf = kIconSz * 0.5f;
    int hoveredIdx = -1;

    for (int i = 0; i < (int)_actMap.size(); i++)
    {
        const MapNode& n  = _actMap[i];
        Color   ec        = nodeColor(n.type);
        Texture2D* icon   = roomIcon(n.type);
        bool    hasIcon   = (icon && icon->id > 0);

        Rectangle hitRect = { n.drawPos.x - kIconHalf - 4.f,
                               n.drawPos.y - kIconHalf - 4.f,
                               kIconSz + 8.f, kIconSz + 8.f };
        bool hov = ready && !n.completed && CheckCollisionPointRec(mouse, hitRect);
        if (hov) hoveredIdx = i;

        // Glow halo behind available nodes
        if (n.available)
        {
            float glow = 0.18f + 0.10f * sinf((float)GetTime() * 3.0f);
            DrawCircleV(n.drawPos, kIconHalf + 14.f, Fade(ec, glow));
            if (hov)
                DrawCircleV(n.drawPos, kIconHalf + 8.f, Fade(ec, 0.35f));
        }

        if (hasIcon)
        {
            Rectangle src = { 0, 0, (float)icon->width, (float)icon->height };
            Rectangle dst = { n.drawPos.x - kIconHalf, n.drawPos.y - kIconHalf,
                               kIconSz, kIconSz };
            Color tint = n.completed ? DARKGRAY
                       : n.available ? WHITE
                       :               Fade(WHITE, 0.35f);
            DrawTexturePro(*icon, src, dst, {}, 0.f, tint);
        }
        else
        {
            float r = kIconHalf - 2.f;
            float alpha = n.completed ? 0.28f : n.available ? 0.72f : 0.18f;
            DrawCircleV(n.drawPos, r, Fade(ec, alpha));
            DrawCircleLines((int)n.drawPos.x, (int)n.drawPos.y, r,
                n.available ? ec : Fade(ec, 0.45f));
            if (n.available && hov)
                DrawCircleLines((int)n.drawPos.x, (int)n.drawPos.y, r + 4.f,
                    Fade(ec, 0.55f));
        }

        // Click
        if (ready && n.available && !n.completed &&
            CheckCollisionPointRec(mouse, hitRect) &&
            IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        {
            EnterMapRoom(i);
        }
    }

    // ── Keyboard-selected node ring ───────────────────────────────────────
    if (_mapKeySelectedIdx >= 0 && _mapKeySelectedIdx < (int)_actMap.size())
    {
        const MapNode& kn = _actMap[_mapKeySelectedIdx];
        if (kn.available && !kn.completed)
        {
            DrawCircleLines((int)kn.drawPos.x, (int)kn.drawPos.y,
                kIconHalf + 9.f,  WHITE);
            DrawCircleLines((int)kn.drawPos.x, (int)kn.drawPos.y,
                kIconHalf + 14.f, Fade(WHITE, 0.35f));
        }
    }

    // ── Current-node indicator (yellow ring on last completed node) ───────
    if (_currentMapNodeIdx >= 0 && _currentMapNodeIdx < (int)_actMap.size())
    {
        const MapNode& cur = _actMap[_currentMapNodeIdx];
        if (cur.completed)
            DrawCircleLines((int)cur.drawPos.x, (int)cur.drawPos.y,
                kIconHalf + 11.f, Fade(YELLOW, 0.55f));
    }

    // ── Hovered node description ──────────────────────────────────────────
    if (hoveredIdx >= 0)
    {
        const char* dsc = nodeDesc(_actMap[hoveredIdx].type);
        int dSz = 24;
        DrawText(dsc,
            (int)(mapCentreX - MeasureText(dsc, dSz) / 2.f),
            (int)(sh - 90.f), dSz, Color{206, 242, 255, 230});
    }

    // ── Footer hint ───────────────────────────────────────────────────────
    const char* hint = _touchModeActive
        ? "Tap a highlighted node to enter"
        : "Click node  or  A/D  to select  -  Enter / Space  to confirm";
    int ftSz = (int)_mapHintFs;
    DrawText(hint,
        (int)(mapCentreX - MeasureText(hint, ftSz) / 2.f),
        (int)(sh - _mapHintY), ftSz, Color{173, 223, 236, 185});

    // ── Map right-panel debug editor ──────────────────────────────────────
    if (IsKeyPressed(KEY_NINE))
        _mapEditorActive = !_mapEditorActive;

    if (_mapEditorActive)
    {
        constexpr int kN = 19;
        const char* varNames[kN] = {
            "0  Journey Panel X",
            "1  Padding",
            "2  Title Font",
            "3  Rooms/Gold Font",
            "4  No Rooms Font",
            "5  Biome Title Font",
            "6  Diamond Label Fs",
            "7  Square Size",
            "8  Square Gap",
            "9  Icon Size",
            "10 Diamond Size",
            "11 Act X",
            "12 Act Y",
            "13 Act Font",
            "14 Path X",
            "15 Path Y",
            "16 Path Font",
            "17 Hint Y",
            "18 Hint Font",
        };
        float* vars[kN] = {
            &_mapJourneyX, &_mapPad, &_mapTitleFs, &_mapRoomsFs, &_mapNoRoomsFs,
            &_mapBiomeTitleFs, &_mapBiomeLabelFs, &_mapSqSz, &_mapSqGap, &_mapIconSz,
            &_mapDiamondSz, &_mapHeaderX, &_mapHeaderY, &_mapHeaderFs,
            &_mapSubX, &_mapSubY, &_mapSubFs, &_mapHintY, &_mapHintFs
        };

        if (IsKeyPressed(KEY_UP))
            _mapEditorSelIdx = (_mapEditorSelIdx - 1 + kN) % kN;
        if (IsKeyPressed(KEY_DOWN))
            _mapEditorSelIdx = (_mapEditorSelIdx + 1) % kN;

        // Fractional X/font fraction controls use finer steps.
        float step = (_mapEditorSelIdx == 0 || _mapEditorSelIdx == 6
                   || _mapEditorSelIdx == 11 || _mapEditorSelIdx == 14) ? 0.005f : 1.f;
        if (IsKeyDown(KEY_RIGHT)) *vars[_mapEditorSelIdx] += step;
        if (IsKeyDown(KEY_LEFT))  *vars[_mapEditorSelIdx] -= step;

        if (IsKeyPressed(KEY_S))
        {
            TraceLog(LOG_INFO, "=== Map Right-Panel Editor Export ===");
            TraceLog(LOG_INFO, "_mapJourneyX     = %.3ff;", _mapJourneyX);
            TraceLog(LOG_INFO, "_mapPad          = %.2ff;", _mapPad);
            TraceLog(LOG_INFO, "_mapTitleFs      = %.2ff;", _mapTitleFs);
            TraceLog(LOG_INFO, "_mapRoomsFs      = %.2ff;", _mapRoomsFs);
            TraceLog(LOG_INFO, "_mapNoRoomsFs    = %.2ff;", _mapNoRoomsFs);
            TraceLog(LOG_INFO, "_mapBiomeTitleFs = %.2ff;", _mapBiomeTitleFs);
            TraceLog(LOG_INFO, "_mapBiomeLabelFs = %.3ff;", _mapBiomeLabelFs);
            TraceLog(LOG_INFO, "_mapSqSz         = %.2ff;", _mapSqSz);
            TraceLog(LOG_INFO, "_mapSqGap        = %.2ff;", _mapSqGap);
            TraceLog(LOG_INFO, "_mapIconSz       = %.2ff;", _mapIconSz);
            TraceLog(LOG_INFO, "_mapDiamondSz    = %.2ff;", _mapDiamondSz);
            TraceLog(LOG_INFO, "_mapHeaderX      = %.3ff;", _mapHeaderX);
            TraceLog(LOG_INFO, "_mapHeaderY      = %.2ff;", _mapHeaderY);
            TraceLog(LOG_INFO, "_mapHeaderFs     = %.2ff;", _mapHeaderFs);
            TraceLog(LOG_INFO, "_mapSubX         = %.3ff;", _mapSubX);
            TraceLog(LOG_INFO, "_mapSubY         = %.2ff;", _mapSubY);
            TraceLog(LOG_INFO, "_mapSubFs        = %.2ff;", _mapSubFs);
            TraceLog(LOG_INFO, "_mapHintY        = %.2ff;", _mapHintY);
            TraceLog(LOG_INFO, "_mapHintFs       = %.2ff;", _mapHintFs);
        }

        // ── Panel draw ────────────────────────────────────────────────────
        constexpr float rowH = 30.f;
        constexpr float panW = 290.f;
        const float panH = 40.f + kN * rowH;
        const float panX = 10.f;
        const float panY = 10.f;

        DrawRectangle((int)panX, (int)panY, (int)panW, (int)panH, Fade(BLACK, 0.82f));
        DrawRectangleLines((int)panX, (int)panY, (int)panW, (int)panH, DARKGRAY);
        DrawText("MAP EDITOR  [9] close", (int)(panX + 8.f), (int)(panY + 6.f), 11, GRAY);
        DrawText("[UP/DOWN] sel  [L/R] nudge  [S] export",
            (int)(panX + 8.f), (int)(panY + 20.f), 10, DARKGRAY);

        for (int i = 0; i < kN; i++)
        {
            float ry  = panY + 38.f + i * rowH;
            bool  sel = (i == _mapEditorSelIdx);
            Color col = sel ? YELLOW : WHITE;

            if (sel)
                DrawText("->", (int)(panX + 4.f), (int)(ry + rowH * 0.5f - 7.f), 13, YELLOW);
            DrawText(varNames[i], (int)(panX + 26.f), (int)(ry + rowH * 0.5f - 7.f), 13, col);

            const char* valStr = TextFormat("%.3f", *vars[i]);
            int valW = MeasureText(valStr, 13);
            DrawText(valStr, (int)(panX + panW - valW - 6.f),
                (int)(ry + rowH * 0.5f - 7.f), 13, col);
        }
    }
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
    else if (_levelUpOfferContext == LevelUpOfferContext::TreasureBasic)
        title = "Treasure Room  -  Choose a basic upgrade:";
    else if (_levelUpOfferContext == LevelUpOfferContext::EliteReward)
        title = "Elite Reward  -  Choose a rarer upgrade:";
    else if (_levelUpOfferContext == LevelUpOfferContext::StoreStock)
        title = "Shop Stock  -  Choose one item:";
    else if (showUlt && showReg)
        title = "LEVEL UP!  Choose an Ultimate AND an Upgrade:";
    else if (showUlt)
        title = "Now choose your Ultimate:";
    else if (showReg)
        title = "Level Up  -  Choose a stat upgrade:";
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

    auto roundedStat = [](float value) -> int
    {
        return (int)std::ceil(value);
    };
    auto pctString = [&](float value) -> std::string
    {
        return std::to_string(roundedStat(value * 100.f)) + "%";
    };
    auto float1String = [](float value) -> std::string
    {
        char buffer[32];
        std::snprintf(buffer, sizeof(buffer), "%.1f", value);
        return std::string(buffer);
    };
    auto linePreview = [&](const char* label, float currentValue, float newValue, const char* suffix = "") -> std::string
    {
        return std::string(label) + " "
            + std::to_string(roundedStat(currentValue)) + suffix
            + " -> "
            + std::to_string(roundedStat(newValue)) + suffix;
    };
    auto getUpgradeInfo = [&](UpgradeType type, const char*& name, std::string& desc, Texture2D*& icon)
    {
        desc.clear();
        switch (type)
        {
        case UpgradeType::AttackPower:
            name = "Attack Power";
            desc = linePreview("Atk", _player.GetAttackPowerValue(), _player.GetAttackPowerValue() * 1.10f);
            icon = &_upgradeAttackPowerTex;
            break;
        case UpgradeType::AttackRange:
            name = "Attack Range";
            desc = "Range " + float1String(_player.GetAttackRangeMultiplierValue()) + "x -> "
                + float1String(_player.GetAttackRangeMultiplierValue() * 1.10f) + "x";
            icon = &_upgradeAttackRangeTex;
            break;
        case UpgradeType::MaxHealth:
            name = "Max Health";
            desc = linePreview("HP", _player.GetMaxHealthValue(),
                _player.GetMaxHealthValue() + std::max(1, (int)std::ceil(_player.GetMaxHealthValue() * 0.15f)));
            icon = &_upgradeHealthTex;
            break;
        case UpgradeType::MaxMana:
            name = "Max Mana";
            desc = linePreview("Mana", (float)_player.GetMaxMana(), (float)(_player.GetMaxMana() + 15));
            icon = &_upgradeMagicTex;
            break;
        case UpgradeType::Defense:
            name = "Armour";
            desc = "Armour " + std::to_string(_player.GetArmour()) + " -> "
                + std::to_string(std::min(_player.GetArmour() + 1, _player.GetMaxArmour()))
                + " / " + std::to_string(_player.GetMaxArmour())
                + "\nAbsorbs 1 hit";
            icon = &_upgradeDefenseTex;
            break;
        case UpgradeType::MoveSpeed:
            name = "Move Speed";
            desc = linePreview("Speed", _player.GetMoveSpeedValue(), _player.GetMoveSpeedValue() * 1.10f);
            icon = &_upgradeMoveSpeedTex;
            break;
        // ── Rare ──────────────────────────────────────────────────────────────
        case UpgradeType::IronConstitution:
            name = "Iron Constitution";
            desc = linePreview("HP", _player.GetMaxHealthValue(),
                _player.GetMaxHealthValue() + std::max(2, (int)std::ceil(_player.GetMaxHealthValue() * 0.25f)));
            icon = &_upgradeHealthTex;
            break;
        case UpgradeType::SwiftFeet:
            name = "Swift Feet";
            desc = linePreview("Speed", _player.GetMoveSpeedValue(), _player.GetMoveSpeedValue() * 1.15f);
            icon = &_upgradeMoveSpeedTex;
            break;
        case UpgradeType::Ferocity:
            name = "Ferocity";
            desc = linePreview("Atk", _player.GetAttackPowerValue(), _player.GetAttackPowerValue() * 1.15f);
            icon = &_upgradeAttackPowerTex;
            break;
        case UpgradeType::ArcaneMind:
            name = "Arcane Mind";
            desc = linePreview("Mana", (float)_player.GetMaxMana(), (float)(_player.GetMaxMana() + 25))
                + "\nRegen " + std::to_string(roundedStat(_player.GetManaRegenPerSecond()))
                + " -> " + std::to_string(roundedStat(_player.GetManaRegenPerSecond() * 1.20f));
            icon = &_upgradeMagicTex;
            break;
        case UpgradeType::IronSkin:
            name = "Iron Skin";
            desc = "Armour " + std::to_string(_player.GetArmour()) + " -> "
                + std::to_string(std::min(_player.GetArmour() + 1, _player.GetMaxArmour()))
                + " / " + std::to_string(_player.GetMaxArmour())
                + "\nAbsorbs 1 hit";
            icon = &_upgradeDefenseTex;
            break;
        case UpgradeType::BladeEdge:
            name = "Blade Edge";
            desc = linePreview("Atk", _player.GetAttackPowerValue(), _player.GetAttackPowerValue() * 1.10f)
                + "\nRange " + float1String(_player.GetAttackRangeMultiplierValue()) + "x -> "
                + float1String(_player.GetAttackRangeMultiplierValue() * 1.08f) + "x";
            icon = &_upgradeAttackRangeTex;
            break;
        // ── Epic ──────────────────────────────────────────────────────────────
        case UpgradeType::WarGod:
            name = "War God";
            desc = linePreview("Atk", _player.GetAttackPowerValue(), _player.GetAttackPowerValue() * 1.20f)
                + "\nRange " + float1String(_player.GetAttackRangeMultiplierValue()) + "x -> "
                + float1String(_player.GetAttackRangeMultiplierValue() * 1.10f) + "x";
            icon = &_upgradeAttackPowerTex;
            break;
        case UpgradeType::Resilience:
            name = "Resilience";
            desc = linePreview("HP", _player.GetMaxHealthValue(),
                _player.GetMaxHealthValue() + std::max(2, (int)std::ceil(_player.GetMaxHealthValue() * 0.30f)))
                + "\nHeal +3";
            icon = &_upgradeHealthTex;
            break;
        case UpgradeType::BladeStorm:
            name = "Blade Storm";
            desc = linePreview("Atk", _player.GetAttackPowerValue(), _player.GetAttackPowerValue() * 1.18f)
                + "\nSpeed " + std::to_string(roundedStat(_player.GetMoveSpeedValue())) + " -> "
                + std::to_string(roundedStat(_player.GetMoveSpeedValue() * 1.18f));
            icon = &_upgradeAttackPowerTex;
            break;
        case UpgradeType::Juggernaut:
            name = "Juggernaut";
            desc = linePreview("HP", _player.GetMaxHealthValue(),
                _player.GetMaxHealthValue() + std::max(2, (int)std::ceil(_player.GetMaxHealthValue() * 0.20f)))
                + "\nArmour " + std::to_string(_player.GetArmour()) + " -> "
                + std::to_string(std::min(_player.GetArmour() + 1, _player.GetMaxArmour()))
                + " / " + std::to_string(_player.GetMaxArmour());
            icon = &_upgradeHealthTex;
            break;
        case UpgradeType::ArcaneColossus:
            name = "Arcane Colossus";
            desc = linePreview("Mana", (float)_player.GetMaxMana(), (float)(_player.GetMaxMana() + 30))
                + "\nAtk " + std::to_string(roundedStat(_player.GetAttackPowerValue()))
                + " -> " + std::to_string(roundedStat(_player.GetAttackPowerValue() * 1.15f));
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

    auto drawMultilineCentered = [&](const std::string& text, float centerX, float startY, int fontSize, Color color)
    {
        std::size_t start = 0;
        int lineIndex = 0;
        while (start <= text.size())
        {
            std::size_t end = text.find('\n', start);
            std::string line = (end == std::string::npos) ? text.substr(start) : text.substr(start, end - start);
            int lineW = MeasureText(line.c_str(), fontSize);
            DrawText(line.c_str(), (int)(centerX - lineW / 2.f), (int)(startY + lineIndex * (fontSize + 4)), fontSize, color);
            if (end == std::string::npos)
                break;
            start = end + 1;
            lineIndex++;
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

            const char* name = ""; std::string desc; Texture2D* icon = nullptr;
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

            drawMultilineCentered(desc, x + cardW / 2.f, ultRowY + cardH * 0.72f, 20, LIGHTGRAY);

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
            bgNormal = Color{35,18,5,210}; bgHover = Color{70,35,10,230};
            borderNormal = Color{200,100,20,150}; borderHover = Color{255,160,50,255};
            stripColor = Color{180,85,15,220}; nameColor = Color{255,185,80,255}; rarityLabel = "EPIC"; break;
        }
        Color bgColor = hovered ? bgHover : bgNormal;
        DrawRectangleRounded(card, 0.12f, 8, bgColor);
        DrawRectangleRoundedLines(card, 0.12f, 8, hovered ? borderHover : borderNormal);

        const char* name = "";
        std::string desc;
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

        drawMultilineCentered(desc, x + cardW / 2.f, cardY + cardH * 0.72f, 20, LIGHTGRAY);

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
            _vfx.SpawnFloatingText(enemy->GetWorldPos(), dmg, YELLOW);
            _vfx.SpawnHitEffect(Character::CastType::None, enemy->GetWorldPos(), _player.GetFacingDirection());
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
                _vfx.SpawnHitEffect(elementToCastType(projectile.GetElement()),
                                    projectile.GetWorldPos(), projectile.GetDirection());
                projectile.Destroy();
                continue;
            }
        }

        for (auto& prop : _props)
        {
            if (CheckCollisionRecs(projectile.GetCollisionRec(), prop.GetCollisionRec()))
            {
                _vfx.SpawnHitEffect(elementToCastType(projectile.GetElement()),
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
                _vfx.SpawnFloatingText(enemy->GetWorldPos(), hitDamage, dmgColor);
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

            _vfx.SpawnHitEffect(hitEffectType, projectile.GetWorldPos(), projectile.GetDirection());
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

void Engine::SpawnEnemyDrop(Vector2 worldPos, bool isOgre, bool isBoss)
{
    // Boss: scatter a large gold reward at the arena centre.
    if (isBoss)
    {
        const float mapW = _map.width  * _mapScale;
        const float mapH = _map.height * _mapScale;
        const Vector2 centre{ mapW * 0.5f, mapH * 0.5f };
        auto spawnGold = [&](GoldDenomination denom, float ox, float oy)
        {
            auto g = std::make_unique<GoldPickup>();
            g->Init(Vector2{ centre.x + ox, centre.y + oy }, denom);
            _pickups.push_back(std::move(g));
        };
        // 8× Ten + 6× Five + 3× Single = ~113g jackpot
        spawnGold(GoldDenomination::Ten,      0.f,    0.f);
        spawnGold(GoldDenomination::Ten,    -55.f,  -60.f);
        spawnGold(GoldDenomination::Ten,     55.f,  -60.f);
        spawnGold(GoldDenomination::Ten,   -110.f,    0.f);
        spawnGold(GoldDenomination::Ten,    110.f,    0.f);
        spawnGold(GoldDenomination::Ten,    -55.f,   60.f);
        spawnGold(GoldDenomination::Ten,     55.f,   60.f);
        spawnGold(GoldDenomination::Ten,      0.f,   90.f);
        spawnGold(GoldDenomination::Five,  -140.f,  -40.f);
        spawnGold(GoldDenomination::Five,   140.f,  -40.f);
        spawnGold(GoldDenomination::Five,  -140.f,   40.f);
        spawnGold(GoldDenomination::Five,   140.f,   40.f);
        spawnGold(GoldDenomination::Five,     0.f, -100.f);
        spawnGold(GoldDenomination::Five,     0.f,  130.f);
        spawnGold(GoldDenomination::Single, -80.f, -110.f);
        spawnGold(GoldDenomination::Single,  80.f, -110.f);
        spawnGold(GoldDenomination::Single,   0.f, -150.f);
        return;
    }

    // Support adds during an active boss fight give no drops.
    if (IsBossFightActive())
        return;

    // Find a valid drop position (avoid spawning inside a prop).
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

    // Always drop one gold coin. Ogres are weighted toward higher denominations.
    // Ogre:    20% Single | 50% Five | 30% Ten
    // Regular: 50% Single | 35% Five | 15% Ten
    int roll = GetRandomValue(1, 100);
    GoldDenomination denom;
    if (isOgre)
        denom = (roll <= 20) ? GoldDenomination::Single
              : (roll <= 70) ? GoldDenomination::Five
              :                GoldDenomination::Ten;
    else
        denom = (roll <= 50) ? GoldDenomination::Single
              : (roll <= 85) ? GoldDenomination::Five
              :                GoldDenomination::Ten;

    // Pity: force at least Five if the last 5 drops were all Singles.
    if (denom == GoldDenomination::Single)
    {
        if (_goldDroughtCounter >= 5)
        {
            denom = GoldDenomination::Five;
            _goldDroughtCounter = 0;
        }
        else
        {
            _goldDroughtCounter++;
        }
    }
    else
    {
        _goldDroughtCounter = 0;
    }

    auto g = std::make_unique<GoldPickup>();
    g->Init(dropPos, denom);
    _pickups.push_back(std::move(g));

    // Rare bonus heal drop (8% chance, unchanged from before).
    if (GetRandomValue(1, 100) <= kEnemyDropChancePercent)
    {
        auto p = std::make_unique<HealPickup>();
        p->Init(dropPos);
        _pickups.push_back(std::move(p));
    }
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
    const float dt = GetFrameTime();

    // ── Font sizes ───────────────────────────────────────────────────────────
    const int titleSz  = (int)(sh * 0.062f);
    const int headerSz = (int)(sh * 0.034f);
    const int labelSz  = (int)(sh * 0.027f);
    const int descSz   = (int)(sh * 0.022f);
    const int tabSz    = (int)(sh * 0.026f);

    // ── Slide animation ──────────────────────────────────────────────────────
    _htpSlideOffset *= (1.f - std::min(dt * 14.f, 1.f));

    // ── Background ───────────────────────────────────────────────────────────
    DrawScrollingCheckerboard(sw, sh,
        Color{ 96, 34, 86, 255 },
        Color{ 132, 54, 116, 255 },
        22.f, 12.f);

    // ── Title bar ────────────────────────────────────────────────────────────
    const float titleBarH = sh * 0.095f;
    DrawRectangle(0, 0, (int)sw, (int)titleBarH, Fade(Color{ 50, 14, 56, 255 }, 0.88f));
    const char* title = "HOW TO PLAY";
    int titleW = MeasureText(title, titleSz);
    for (int ox = -2; ox <= 2; ox += 2)
        for (int oy = -2; oy <= 2; oy += 2)
            if (ox || oy)
                DrawText(title, (int)(sw / 2.f - titleW / 2.f) + ox,
                    (int)(titleBarH / 2.f - titleSz / 2.f) + oy, titleSz, BLACK);
    DrawText(title, (int)(sw / 2.f - titleW / 2.f),
        (int)(titleBarH / 2.f - titleSz / 2.f), titleSz, Color{ 255, 194, 92, 255 });

    // ── Tab bar ──────────────────────────────────────────────────────────────
    const char* tabLabels[] = { "BASICS", "ELEMENTS", "THE WORLD", "TOUCH" };
    const int   tabCount    = 4;
    const float tabBarY     = titleBarH + sh * 0.008f;
    const float tabBarH     = sh * 0.063f;
    const float tabW        = sw * 0.175f;
    const float tabGap      = sw * 0.012f;
    const float tabsTotal   = tabCount * tabW + (tabCount - 1) * tabGap;
    const float tabStartX   = sw / 2.f - tabsTotal / 2.f;

    for (int i = 0; i < tabCount; i++)
    {
        float tx = tabStartX + i * (tabW + tabGap);
        Rectangle tabRect = { tx, tabBarY, tabW, tabBarH };
        bool isActive  = (_htpTab == i);
        bool tabHov    = CheckCollisionPointRec(GetMousePosition(), tabRect);

        Color bgCol  = isActive  ? Color{ 185, 130, 30, 240 }
                     : tabHov    ? Color{ 100, 50,  90, 200 }
                     :             Color{ 58,  22,  56, 200 };
        Color edgeCol = isActive ? Color{ 255, 210, 80,  255 }
                      :            Fade(Color{ 200, 140, 220, 255 }, 0.45f);

        DrawRectangleRounded(tabRect, 0.22f, 6, bgCol);
        DrawRectangleRoundedLines(tabRect, 0.22f, 6, edgeCol);
        if (isActive)
            DrawRectangle((int)(tx + tabW * 0.1f), (int)(tabBarY + tabBarH - 4.f),
                          (int)(tabW * 0.8f), 4, Color{ 255, 210, 80, 255 });

        int lw = MeasureText(tabLabels[i], tabSz);
        Color textCol = isActive ? Color{ 20, 10, 5, 255 } : Color{ 220, 185, 240, 255 };
        DrawText(tabLabels[i],
            (int)(tx + tabW / 2.f - lw / 2.f),
            (int)(tabBarY + tabBarH / 2.f - tabSz / 2.f),
            tabSz, textCol);

        if (tabHov && !isActive && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        {
            _htpSlideOffset = (i > _htpTab ? 1.f : -1.f) * sw * 0.12f;
            _htpTab = i;
            StopSound(_buttonPressSound); PlaySound(_buttonPressSound);
        }
    }

    // ── Content panel ────────────────────────────────────────────────────────
    const float panelX = sw * 0.04f;
    const float panelY = tabBarY + tabBarH + sh * 0.010f;
    const float panelW = sw * 0.92f;
    const float panelH = sh * 0.755f;
    Rectangle panelRect = { panelX, panelY, panelW, panelH };

    if (_shopBorderTex.id != 0)
        DrawNineSlice(_shopBorderTex, 16.f, 28.f, panelRect, Fade(WHITE, 0.88f));
    else
        DrawRectangleRounded(panelRect, 0.04f, 8, Fade(Color{ 45, 12, 52, 255 }, 0.88f));

    // Content origin (shifted by slide offset)
    const float cx = panelX + sw * 0.03f + _htpSlideOffset;
    const float cy = panelY + sh * 0.025f;
    const float cw = panelW - sw * 0.06f;

    BeginScissorMode((int)panelX + 2, (int)panelY + 2, (int)panelW - 4, (int)panelH - 4);

    // ════════════════════════════════════════════════════════════════════════
    // TAB 0 — BASICS
    // ════════════════════════════════════════════════════════════════════════
    if (_htpTab == 0)
    {
        const float colW   = cw * 0.45f;
        const float midGap = cw * 0.10f;
        const float leftX  = cx;
        const float rightX = cx + colW + midGap;
        const float divX   = cx + colW + midGap / 2.f;

        const float basicsHeaderY = cy + sh * 0.022f;
        DrawText("KEYBOARD & MOVEMENT", (int)leftX,  (int)basicsHeaderY, headerSz, Color{ 255, 194, 92, 255 });
        DrawText("ACTIONS & ABILITIES",  (int)rightX, (int)basicsHeaderY, headerSz, Color{ 255, 194, 92, 255 });
        DrawLineEx({ divX, basicsHeaderY + headerSz + 4.f }, { divX, panelY + panelH - sh * 0.03f },
            1.5f, Fade(Color{ 220, 160, 240, 255 }, 0.35f));

        // Keyboard controls
        struct KBEntry { const char* key; const char* desc; };
        KBEntry kb[] = {
            { "W / A / S / D",  "Move"                       },
            { "SPACE",          "Dash  (brief invincibility)" },
            { "Left Click",     "Melee attack"                },
            { "1  2  3  4",     "Use ability in that slot"    },
            { "Scroll Wheel",   "Cycle active ability"        },
            { "ESC",            "Pause / unpause"             },
        };
        float rowY = basicsHeaderY + headerSz + sh * 0.022f;
        for (auto& k : kb)
        {
            int kw = MeasureText(k.key, labelSz);
            float badgeH = (float)labelSz + 10.f;
            DrawRectangleRounded({ leftX, rowY - 4.f, (float)kw + 18.f, badgeH },
                0.28f, 4, Fade(Color{ 70, 18, 66, 255 }, 0.80f));
            DrawRectangleRoundedLines({ leftX, rowY - 4.f, (float)kw + 18.f, badgeH },
                0.28f, 4, Fade(Color{ 255, 182, 236, 255 }, 0.55f));
            DrawText(k.key,  (int)leftX + 9, (int)rowY, labelSz, Color{ 255, 194, 92, 255 });
            DrawText(k.desc, (int)(leftX + kw + 30.f), (int)rowY, descSz, Color{ 20, 15, 25, 255 });
            rowY += sh * 0.072f;
        }

        // EXP & Gold
        rowY += sh * 0.010f;
        DrawText("EXP & GOLD", (int)leftX, (int)rowY, headerSz, Color{ 255, 194, 92, 255 });
        rowY += headerSz + sh * 0.012f;
        const char* expLines[] = {
            "Kill enemies to earn EXP and level up.",
            "Choose 1 of 3 upgrade cards each level.",
            "Enemies drop Gold coins when defeated.",
            "Spend Gold at Zeph's shop between biomes.",
        };
        for (auto& line : expLines)
        {
            DrawText(line, (int)leftX, (int)rowY, descSz, Color{ 20, 15, 25, 255 });
            rowY += descSz + sh * 0.008f;
        }

        // Actions column
        struct ActionEntry { const char* name; const char* line1; const char* line2; };
        ActionEntry acts[] = {
            { "DASH",
              "Quick burst of movement in any direction.",
              "You are invincible for its full duration." },
            { "MELEE",
              "Left-click to swing your sword.",
              "Hits all enemies in a forward arc." },
            { "ABILITIES",
              "Press 1-4 to cast a learned ability.",
              "Each ability costs Mana to activate." },
            { "MANA",
              "Blue gems drop from defeated enemies.",
              "Pick them up to fuel your abilities." },
        };
        float rowR = basicsHeaderY + headerSz + sh * 0.022f;
        for (auto& a : acts)
        {
            DrawText(a.name, (int)rightX, (int)rowR, labelSz, Color{ 255, 194, 92, 255 });
            rowR += labelSz + sh * 0.005f;
            DrawText(a.line1, (int)rightX, (int)rowR, descSz, Color{ 20, 15, 25, 255 });
            rowR += descSz + sh * 0.003f;
            DrawText(a.line2, (int)rightX, (int)rowR, descSz, Color{ 20, 15, 25, 255 });
            rowR += descSz + sh * 0.030f;
        }
    }

    // ════════════════════════════════════════════════════════════════════════
    // TAB 1 — ELEMENTS
    // ════════════════════════════════════════════════════════════════════════
    else if (_htpTab == 1)
    {
        const char* elemTitle = "THE MAGIC SYSTEM";
        DrawText(elemTitle,
            (int)(cx + cw / 2.f - MeasureText(elemTitle, headerSz) / 2.f),
            (int)cy, headerSz, Color{ 255, 194, 92, 255 });

        struct ElemEntry {
            const char* name;
            const char* effect;
            const char* desc1;
            const char* desc2;
            Color       nameCol;
            Texture2D*  icon;
        };
        ElemEntry elems[] = {
            { "FIRE",
              "BURN  —  Damage over time",
              "Enemies ignite and take periodic damage",
              "for several seconds after being hit.",
              Color{ 255, 120, 40, 255 },
              &_abilityIconFireTex },
            { "ICE",
              "FREEZE  —  Stuns the enemy",
              "Frozen enemies cannot move or attack.",
              "Break the freeze with a melee hit for bonus damage.",
              Color{ 100, 210, 255, 255 },
              &_abilityIconIceTex },
            { "ELECTRIC",
              "SHOCK  —  Amplifies melee damage",
              "Shocked enemies take greatly increased damage",
              "from your next melee strike.",
              Color{ 220, 220, 50, 255 },
              &_abilityIconElectricTex },
        };

        const float iconSz     = sh * 0.095f;
        const float rowSpacing = sh * 0.220f;
        float elemY = cy + headerSz + sh * 0.040f;

        for (auto& el : elems)
        {
            Rectangle iconRect = { cx, elemY, iconSz, iconSz };
            DrawRectangleRounded(iconRect, 0.22f, 6, Fade(el.nameCol, 0.18f));
            DrawRectangleRoundedLines(iconRect, 0.22f, 6, Fade(el.nameCol, 0.65f));
            if (el.icon->id != 0)
                DrawTexturePro(*el.icon,
                    { 0.f, 0.f, (float)el.icon->width, (float)el.icon->height },
                    { cx + iconSz * 0.1f, elemY + iconSz * 0.1f, iconSz * 0.8f, iconSz * 0.8f },
                    {}, 0.f, WHITE);
            else
                DrawCircleV({ cx + iconSz / 2.f, elemY + iconSz / 2.f },
                    iconSz * 0.35f, Fade(el.nameCol, 0.75f));

            float textX = cx + iconSz + sw * 0.025f;
            float textY = elemY;
            DrawText(el.name, (int)textX, (int)textY, headerSz, el.nameCol);
            textY += headerSz + sh * 0.006f;

            int effW = MeasureText(el.effect, labelSz);
            DrawRectangleRounded({ textX, textY - 2.f, (float)effW + 16.f, (float)labelSz + 10.f },
                0.3f, 4, Fade(el.nameCol, 0.22f));
            DrawRectangleRoundedLines({ textX, textY - 2.f, (float)effW + 16.f, (float)labelSz + 10.f },
                0.3f, 4, Fade(el.nameCol, 0.55f));
            DrawText(el.effect, (int)textX + 8, (int)textY, labelSz, el.nameCol);
            textY += labelSz + sh * 0.016f;
            DrawText(el.desc1, (int)textX, (int)textY, descSz, Color{ 210, 178, 220, 255 });
            textY += descSz + sh * 0.005f;
            DrawText(el.desc2, (int)textX, (int)textY, descSz, Color{ 170, 140, 185, 255 });

            elemY += rowSpacing;
        }
    }

    // ════════════════════════════════════════════════════════════════════════
    // TAB 2 — THE WORLD
    // ════════════════════════════════════════════════════════════════════════
    else if (_htpTab == 2)
    {
        const float colW   = cw * 0.45f;
        const float midGap = cw * 0.10f;
        const float leftX  = cx;
        const float rightX = cx + colW + midGap;
        const float divX   = cx + colW + midGap / 2.f;

        const float worldHeaderY = cy + sh * 0.022f;
        DrawText("MAP LEGEND",  (int)leftX,  (int)worldHeaderY, headerSz, Color{ 255, 194, 92, 255 });
        DrawText("ZEPH'S SHOP", (int)rightX, (int)worldHeaderY, headerSz, Color{ 255, 194, 92, 255 });
        DrawLineEx({ divX, worldHeaderY + headerSz + 4.f }, { divX, panelY + panelH - sh * 0.03f },
            1.5f, Fade(Color{ 220, 160, 240, 255 }, 0.35f));

        struct MapEntry { Texture2D* tex; Color fallback; const char* name; const char* desc; };
        MapEntry mapIcons[] = {
            { &_mapIconNormal,   GRAY,   "NORMAL ROOM", "Clear all enemies to proceed."         },
            { &_mapIconElite,    ORANGE, "ELITE ROOM",  "Tougher enemies, better drops."        },
            { &_mapIconShop,     GOLD,   "SHOP",        "Buy upgrades and potions from Zeph."   },
            { &_mapIconTreasure, YELLOW, "TREASURE",    "Find a powerful item for free."        },
            { &_mapIconBoss,     RED,    "BOSS",        "Defeat the boss to complete the biome."},
            { &_mapIconRest,     GREEN,  "REST SITE",   "Recover HP between battles."           },
        };
        const float mapIconSz = sh * 0.052f;
        float mapY = worldHeaderY + headerSz + sh * 0.022f;
        for (auto& mi : mapIcons)
        {
            if (mi.tex->id != 0)
                DrawTexturePro(*mi.tex,
                    { 0.f, 0.f, (float)mi.tex->width, (float)mi.tex->height },
                    { leftX, mapY, mapIconSz, mapIconSz }, {}, 0.f, WHITE);
            else
            {
                DrawRectangleRounded({ leftX, mapY, mapIconSz, mapIconSz }, 0.28f, 4, Fade(mi.fallback, 0.65f));
                DrawRectangleRoundedLines({ leftX, mapY, mapIconSz, mapIconSz }, 0.28f, 4, BLACK);
            }
            float tx = leftX + mapIconSz + sw * 0.015f;
            DrawText(mi.name, (int)tx, (int)mapY,                   labelSz, Color{ 255, 194, 92, 255 });
            DrawText(mi.desc, (int)tx, (int)(mapY + labelSz + 3.f), descSz,  Color{ 20, 15, 25, 255 });
            mapY += sh * 0.095f;
        }

        // Shop info
        struct ShopLine { const char* text; bool isHeader; };
        ShopLine shopLines[] = {
            { "Between biomes you visit Zeph's Shop.",         false },
            { "",                                              false },
            { "UPGRADES",                                      true  },
            { "Buy powerful passive boosts for your run.",     false },
            { "Epic-tier items offer rare, powerful effects.", false },
            { "",                                              false },
            { "REROLL",                                        true  },
            { "Pay gold to refresh the item selection.",       false },
            { "Cost increases with each reroll.",              false },
            { "",                                              false },
            { "POTIONS",                                       true  },
            { "Health Potion  —  restore HP instantly.",       false },
            { "Mana Potion  —  restore Mana instantly.",       false },
            { "",                                              false },
            { "DAILY DEAL",                                    true  },
            { "One item each visit is 25% off.",               false },
            { "Look for the gold price tag.",                  false },
        };
        float shopY = worldHeaderY + headerSz + sh * 0.022f;
        for (auto& sl : shopLines)
        {
            if (sl.text[0] == '\0') { shopY += descSz * 0.5f; continue; }
            Color col = sl.isHeader ? Color{ 255, 194, 92, 255 } : Color{ 20, 15, 25, 255 };
            int   sz  = sl.isHeader ? labelSz : descSz;
            DrawText(sl.text, (int)rightX, (int)shopY, sz, col);
            shopY += sz + sh * 0.007f;
        }
    }

    // ════════════════════════════════════════════════════════════════════════
    // TAB 3 — TOUCH CONTROLS
    // ════════════════════════════════════════════════════════════════════════
    else if (_htpTab == 3)
    {
        const char* touchTitle = "TOUCH CONTROLS";
        DrawText(touchTitle,
            (int)(cx + cw / 2.f - MeasureText(touchTitle, headerSz) / 2.f),
            (int)cy, headerSz, Color{ 255, 194, 92, 255 });

        // Screen split diagram
        const float diagW = cw * 0.78f;
        const float diagH = sh * 0.285f;
        const float diagX = cx + cw / 2.f - diagW / 2.f;
        const float diagY = cy + headerSz + sh * 0.025f;
        const float halfW = diagW / 2.f;

        DrawRectangleRounded({ diagX, diagY, diagW, diagH }, 0.06f, 8, Fade(Color{ 30, 10, 36, 255 }, 0.90f));
        DrawRectangleRoundedLines({ diagX, diagY, diagW, diagH }, 0.06f, 8, Fade(Color{ 220, 160, 240, 255 }, 0.55f));

        // Left half — movement
        DrawRectangleRounded({ diagX, diagY, halfW, diagH }, 0.06f, 8, Fade(Color{ 40, 80, 120, 255 }, 0.35f));
        float jsX = diagX + halfW / 2.f;
        float jsY = diagY + diagH / 2.f;
        DrawCircleV({ jsX, jsY }, diagH * 0.28f, Fade(Color{ 80, 130, 200, 255 }, 0.30f));
        DrawCircleLinesV({ jsX, jsY }, diagH * 0.28f, Fade(Color{ 120, 180, 255, 255 }, 0.65f));
        DrawCircleV({ jsX, jsY }, diagH * 0.11f, Fade(Color{ 160, 210, 255, 255 }, 0.80f));
        DrawText("MOVE",
            (int)(jsX - MeasureText("MOVE", descSz) / 2.f),
            (int)(diagY + diagH - descSz - sh * 0.015f),
            descSz, Color{ 150, 200, 255, 255 });

        DrawLineEx({ diagX + halfW, diagY + sh * 0.015f },
            { diagX + halfW, diagY + diagH - sh * 0.015f }, 1.5f, Fade(WHITE, 0.30f));

        // Right half — buttons
        DrawRectangleRounded({ diagX + halfW, diagY, halfW, diagH }, 0.06f, 8, Fade(Color{ 100, 40, 80, 255 }, 0.35f));
        float btnR  = diagH * 0.18f;
        float b1X   = diagX + halfW + halfW * 0.32f;
        float b2X   = diagX + halfW + halfW * 0.70f;
        float btY   = diagY + diagH * 0.42f;

        DrawCircleV({ b1X, btY }, btnR, Fade(Color{ 200, 80, 80, 255 }, 0.55f));
        DrawCircleLinesV({ b1X, btY }, btnR, Fade(WHITE, 0.50f));
        DrawText("ATK",
            (int)(b1X - MeasureText("ATK", descSz) / 2.f),
            (int)(btY - descSz / 2.f), descSz, WHITE);

        DrawCircleV({ b2X, btY }, btnR, Fade(Color{ 80, 120, 220, 255 }, 0.55f));
        DrawCircleLinesV({ b2X, btY }, btnR, Fade(WHITE, 0.50f));
        DrawText("DASH",
            (int)(b2X - MeasureText("DASH", descSz) / 2.f),
            (int)(btY - descSz / 2.f), descSz, WHITE);

        DrawText("COMBAT",
            (int)(diagX + halfW + halfW / 2.f - MeasureText("COMBAT", descSz) / 2.f),
            (int)(diagY + diagH - descSz - sh * 0.015f),
            descSz, Color{ 255, 150, 200, 255 });

        // Tips
        struct TipEntry { const char* label; const char* desc; };
        TipEntry tips[] = {
            { "JOYSTICK:",  "Drag anywhere on the left half to move your character." },
            { "ATTACK:",    "Tap the ATK button to swing your sword."                },
            { "DASH:",      "Tap DASH for a quick invincible burst of speed."        },
            { "ABILITIES:", "Tap any ability icon at the bottom of the screen."      },
            { "PAUSE:",     "Tap the pause icon in the top-right corner."            },
        };
        const float tipsX   = cx + cw / 2.f - cw * 0.35f;
        float       tipY    = diagY + diagH + sh * 0.030f;
        for (auto& tip : tips)
        {
            int lw = MeasureText(tip.label, labelSz);
            DrawText(tip.label, (int)tipsX,                         (int)tipY, labelSz, Color{ 255, 194, 92, 255 });
            DrawText(tip.desc,  (int)(tipsX + lw + sw * 0.012f),   (int)tipY, descSz,  Color{ 20, 15, 25, 255 });
            tipY += sh * 0.068f;
        }
    }

    EndScissorMode();

    // ── Back button ──────────────────────────────────────────────────────────
    const float btnW = sw * 0.14f;
    const float btnH = sh * 0.055f;
    const float btnX = sw / 2.f - btnW / 2.f;
    const float btnY = sh - btnH - sh * 0.016f;
    Rectangle backBtn{ btnX, btnY, btnW, btnH };
    bool hovered = CheckCollisionPointRec(GetMousePosition(), backBtn);

    DrawRectangleRounded(backBtn, 0.3f, 6, hovered ? Color{ 196, 86, 165, 240 } : Color{ 142, 58, 132, 228 });
    DrawRectangleRoundedLines(backBtn, 0.3f, 6, Fade(Color{ 255, 194, 92, 255 }, 0.68f));

    const char* backLabel   = (_howToPlayFrom == GameState::Pause) ? "Resume Game" : "< Back";
    int         backLabelSz = (int)(sh * 0.030f);
    int         backW       = MeasureText(backLabel, backLabelSz);
    DrawText(backLabel,
        (int)(btnX + btnW / 2.f - backW / 2.f),
        (int)(btnY + btnH / 2.f - backLabelSz / 2.f),
        backLabelSz, Color{ 255, 243, 214, 255 });

    if (hovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
    {
        StopSound(_buttonPressSound); PlaySound(_buttonPressSound);
        if (_howToPlayFrom == GameState::Menu)
            _menu.Init();
        _runState.ReturnFromHowToPlay();
    }
}


Vector2 Engine::GetRandomPropPosition()
{
    float mapW = _map.width * _mapScale;
    float mapH = _map.height * _mapScale;
    Vector2 playerStart{ mapW * 0.5f, mapH * 0.5f };

    // One full player unit of clearance from every map edge.
    // Player sprite is 32 px wide at draw scale 6 → 192 world-space pixels.
    // The bottom margin is larger because prop positions are top-left corners,
    // so tall props (trees) would otherwise hang off the bottom edge.
    const float margin       = 32.f * 6.f;         // 192 px — sides and top
    const float bottomMargin = margin * 1.5f;       // 288 px — bottom
    float minX = margin;
    float maxX = mapW - margin;
    float minY = margin;
    float maxY = mapH - bottomMargin;

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
    switch (biome)
    {
    case Biome::AncientCastle:  return "Ancient Castle";
    case Biome::Caverns:        return "Caverns";
    case Biome::DemonsInsides:  return "Demons Insides";
    case Biome::DreamRealm:     return "Dream Realm";
    case Biome::Forest:         return "Forest";
    case Biome::Graveyard:      return "Graveyard";
    case Biome::Jungle:         return "Jungle";
    case Biome::LostCity:       return "Lost City";
    case Biome::TheSanctuary:   return "The Sanctuary";
    case Biome::Wastelands:     return "Wastelands";
    default:                    return "???";
    }
}

void Engine::PopulatePropsForBiome(Biome biome)
{
    _props.clear();

    // Boss and Store rooms are open arenas — no props
    if (_currentRoomType == RoomType::Boss || _currentRoomType == RoomType::Store)
        return;

    if (biome == Biome::Forest)
    {
        // Forest props are all solid blockers, so a mixed random rotation is
        // enough to make each pass through the biome read differently without
        // changing the existing prop/pathfinding systems.
        int propCount = GetRandomValue(4, 6);
        for (int i = 0; i < propCount; ++i)
        {
            Vector2 pos = GetRandomPropPosition();
            int choice = GetRandomValue(0, 3);
            if (choice == 0)
            {
                _props.push_back(Prop{ pos, _treeTex, 1, 0, 0, 4.8f });
                _props.back().SetCollisionTopFraction(0.25f);
                _props.back().SetCollisionSideFraction(0.30f);
            }
            else if (choice == 1)
            {
                _props.push_back(Prop{ pos, _smallTreeTex, 1, 0, 0, 4.8f });
                _props.back().SetCollisionTopFraction(0.25f);
                _props.back().SetCollisionSideFraction(0.30f);
            }
            else if (choice == 2)
            {
                _props.push_back(Prop{ pos, _rockTex, 1, 0, 0, 4.f });
            }
            else
            {
                _props.push_back(Prop{ pos, _bigRockTex, 1, 0, 0, 3.2f });
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
    _nav.CancelAndReset();

    if (_map.id != 0)
        UnloadTexture(_map);

    // Forest-family biomes use the forest map; all others use the dungeon map.
    // New biome maps can be wired in here when the art is ready.
    bool useForestMap = (biome == Biome::Forest || biome == Biome::Jungle);
    if (useForestMap)
        _map = LoadTexture(AssetPath("ForestLevel/ForestMap.png").c_str());
    else
        _map = LoadTexture(AssetPath("TileSet/Map.png").c_str());

    // Compute the map scale for the current screen size.
    // WorldConfig derives the correct scale from mode + texture dimensions,
    // so the world always fits properly on 720p, 1080p, 4K, and phones.
    _worldConfig.Recalculate(_map, GetScreenWidth(), GetScreenHeight());
    _mapScale = _worldConfig.GetScale();

    _currentBiome = biome;
    PopulatePropsForBiome(biome);
    {
        std::vector<Rectangle> propRects;
        propRects.reserve(_props.size() + 1);
        for (auto& prop : _props)
            propRects.push_back(prop.GetCollisionRec());
        // Block the bottom boundary zone so A* never routes waypoints into it.
        {
            const float navMapW = _map.width  * _mapScale;
            const float navMapH = _map.height * _mapScale;
            const float navBot  = (_currentBiome == Biome::Forest || _currentBiome == Biome::Jungle) ? 160.f : 220.f;
            propRects.push_back({ 0.f, navMapH - navBot, navMapW, navBot });
        }
        _nav.Rebuild(_map.width * _mapScale, _map.height * _mapScale, propRects);
    }

    // Biome swaps happen between waves, so recentering the player gives them
    // a clean neutral start in the new arena before enemies spawn in.
    _player.SetWorldPos(Vector2{ _map.width * _mapScale * 0.5f, _map.height * _mapScale * 0.5f });
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
        _nav.RefreshSync(_player.GetFeetWorldPos());
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
    int col = (int)(pos.x / _nav.GetCellSize());
    int row = (int)(pos.y / _nav.GetCellSize());
    for (int dc = -1; dc <= 1; dc++)
        for (int dr = -1; dr <= 1; dr++)
            if (_nav.IsCellBlocked(col + dc, row + dr))
                return false;

    // Check against each prop's actual collision rect (inflated by safeDistance)
    // rather than distance from the top-left corner, so tall props like trees
    // correctly reject positions near their lower half.
    for (auto& prop : _props)
    {
        Rectangle rec = prop.GetCollisionRec();
        Rectangle inflated = {
            rec.x - safeDistance,
            rec.y - safeDistance,
            rec.width  + safeDistance * 2.f,
            rec.height + safeDistance * 2.f
        };
        if (CheckCollisionPointRec(pos, inflated))
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

    // Reject tiles that are completely cut off from the player by walls.
    // HasReachablePath checks the flow-field cost — max_int means the BFS
    // wave never reached this cell, so no path exists.
    if (!_nav.HasReachablePath(pos))
        return false;

    return true;
}

void Engine::ResetRunState()
{
    _nav.CancelAndReset();
    ResetMusicState();
    _wave          = 0;
    _enemiesKilled = 0;
    _bossesDefeated = 0;
    _gameTimer = 0.f;
    _playerDying = false;
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
    _levelUpOfferContext      = LevelUpOfferContext::NormalLevel;
    _eliteRewardGranted       = false;
    _debug.Deactivate();

    // ── Room / act progression reset ──────────────────────────────────────
    _currentAct        = 1;
    _currentRoom       = 0;
    _currentRoomType   = RoomType::Standard;
    _pendingRoomChoice  = false;
    _roomClearPending   = false;
    _roomClearTimer     = 0.f;
    _pendingExp             = 0.f;
    _expTallyAccum          = 0.f;
    _expTallyDone           = false;
    _tallyStartLevel        = 1;
    _tallyLevelUpsRemaining = 0;
    _tallyChoiceChaining    = false;
    _currentMapNodeIdx  = -1;
    _mapKeySelectedIdx  = -1;
    _mapOpenTimer       = 0.f;
    _actMap.clear();

    // Generate a random sequence of kTotalActs biomes, no two consecutive duplicates.
    {
        static constexpr Biome kAllBiomes[] = {
            Biome::Caverns, Biome::Forest
        };
        static constexpr int kBiomeCount = (int)(sizeof(kAllBiomes) / sizeof(kAllBiomes[0]));
        _biomeSequence.clear();
        Biome last = (Biome)-1;
        for (int i = 0; i < kTotalActs; i++)
        {
            Biome pick;
            do { pick = kAllBiomes[GetRandomValue(0, kBiomeCount - 1)]; }
            while (pick == last);
            _biomeSequence.push_back(pick);
            last = pick;
        }
        _startBiomeDungeon = (_biomeSequence[0] == Biome::Caverns);  // keep fallback in sync
    }

    _spreadProjectiles.clear();
    _ultimateBlasts.clear();
    _lavaBalls.clear();
    _cyclopsLasers.clear();
    _pickups.clear();
    _vfx.Clear();
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

    _nav.RefreshSync(_player.GetFeetWorldPos());
}

int Engine::GetActiveEnemyCount() const
{
    return _combatDirector.GetActiveEnemyCount(_enemies);
}

bool Engine::IsBossFightActive() const
{
    return _combatDirector.IsBossFightActive(_enemies);
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
    BossSupportContext ctx{};
    ctx.bossCyclopsSupport = &_bossCyclopsSupport;
    ctx.bossOgreSupport = &_bossOgreSupport;
    ctx.tryGetFarSpawnPosition = [&](Vector2& pos, float minPlayerDistance) { return TryGetFarSpawnPosition(pos, minPlayerDistance); };
    ctx.spawnCyclops = [&](Vector2 pos) { return SpawnCyclops(pos); };
    ctx.spawnOgre = [&](Vector2 pos) { return SpawnOgre(pos); };
    ctx.isBossFightActive = [&]() { return IsBossFightActive(); };
    _combatDirector.SpawnBossSupportAdds(ctx);
}

void Engine::ClearBossSupportAdds()
{
    _combatDirector.ClearBossSupportAdds(_bossCyclopsSupport, _bossOgreSupport);
}

void Engine::UpdateBossSupportRespawns(float dt)
{
    BossSupportContext ctx{};
    ctx.bossCyclopsSupport = &_bossCyclopsSupport;
    ctx.bossOgreSupport = &_bossOgreSupport;
    ctx.tryGetFarSpawnPosition = [&](Vector2& pos, float minPlayerDistance) { return TryGetFarSpawnPosition(pos, minPlayerDistance); };
    ctx.spawnCyclops = [&](Vector2 pos) { return SpawnCyclops(pos); };
    ctx.spawnOgre = [&](Vector2 pos) { return SpawnOgre(pos); };
    ctx.isBossFightActive = [&]() { return IsBossFightActive(); };
    _combatDirector.UpdateBossSupportRespawns(ctx, dt);
}

bool Engine::TryGetPooledEnemySpawn(Vector2 pos)
{
    return SpawnBasicEnemy(pos) != nullptr;
}

Enemy* Engine::SpawnBasicEnemy(Vector2 pos)
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
        return enemy.get();
    }

    auto enemy = std::make_unique<Enemy>(pos);
    enemy->Init();
    ConfigureSpawnedEnemy(*enemy);
    Enemy* enemyPtr = enemy.get();
    _enemies.push_back(std::move(enemy));
    return enemyPtr;
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
    // Give every enemy its own pointer to the shared nav grid so it can
    // extract waypoints on its own staggered timer (see Enemy::HandleMovement).
    enemy.SetNavigationGrid(&_nav);
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
std::vector<Vector2> Engine::GetCyclopsLaserEndpoints(const CyclopsLaserProjectile& laser) const
{
    std::vector<Vector2> endpoints;
    endpoints.reserve(laser.GetBeamCount());

    const Vector2 start = laser.GetWorldPos();
    const float maxLength = laser.GetCurrentBeamLength();

    for (int i = 0; i < laser.GetBeamCount(); ++i)
    {
        Vector2 dir = laser.GetBeamDirection(i);
        if (Vector2LengthSqr(dir) < 0.0001f)
            continue;

        Vector2 farEnd = Vector2Add(start, Vector2Scale(dir, maxLength));
        Vector2 bestEnd = farEnd;
        float bestDist = maxLength;

        for (const auto& prop : _props)
        {
            Rectangle rect = prop.GetCollisionRec();
            const Vector2 corners[4] = {
                { rect.x, rect.y },
                { rect.x + rect.width, rect.y },
                { rect.x + rect.width, rect.y + rect.height },
                { rect.x, rect.y + rect.height }
            };

            for (int edge = 0; edge < 4; ++edge)
            {
                Vector2 hit{};
                Vector2 edgeStart = corners[edge];
                Vector2 edgeEnd = corners[(edge + 1) % 4];
                if (!CheckCollisionLines(start, farEnd, edgeStart, edgeEnd, &hit))
                    continue;

                float hitDist = Vector2Distance(start, hit);
                if (hitDist < bestDist)
                {
                    bestDist = hitDist;
                    bestEnd = hit;
                }
            }
        }

        endpoints.push_back(bestEnd);
    }

    return endpoints;
}

bool Engine::SegmentHitsRect(Vector2 start, Vector2 end, float thickness, const Rectangle& rect) const
{
    Rectangle expanded{
        rect.x - thickness * 0.5f,
        rect.y - thickness * 0.5f,
        rect.width + thickness,
        rect.height + thickness
    };

    if (CheckCollisionPointRec(start, expanded) || CheckCollisionPointRec(end, expanded))
        return true;

    const Vector2 corners[4] = {
        { expanded.x, expanded.y },
        { expanded.x + expanded.width, expanded.y },
        { expanded.x + expanded.width, expanded.y + expanded.height },
        { expanded.x, expanded.y + expanded.height }
    };

    for (int edge = 0; edge < 4; ++edge)
    {
        Vector2 hit{};
        if (CheckCollisionLines(start, end, corners[edge], corners[(edge + 1) % 4], &hit))
            return true;
    }

    return false;
}

void Engine::DrawCyclopsLasers(Vector2 worldOffset)
{
    for (const auto& laser : _cyclopsLasers)
    {
        if (!laser.IsActive())
            continue;

        std::vector<Vector2> endpoints = GetCyclopsLaserEndpoints(laser);
        laser.Draw(worldOffset, endpoints.data(), (int)endpoints.size());
    }
}

void Engine::UpdateCyclopsLasers(float dt)
{
    for (auto& laser : _cyclopsLasers)
    {
        if (!laser.IsActive())
            continue;

        laser.Update(dt);
        if (!laser.IsActive())
            continue;

        if (_player.IsAlive() && laser.CanHitPlayer())
        {
            std::vector<Vector2> endpoints = GetCyclopsLaserEndpoints(laser);
            for (const Vector2& end : endpoints)
            {
                if (!SegmentHitsRect(laser.GetWorldPos(), end, laser.GetBeamWidth(), _player.GetCollisionRec()))
                    continue;

                int laserDmg = laser.GetDamage();
                _player.TakeDamage(laserDmg, laser.GetWorldPos());
                _vfx.SpawnFloatingText(_player.GetWorldPos(), -laserDmg, RED);
                laser.OnHitPlayer();
                TriggerScreenShake(2.5f, 0.07f);
                break;
            }
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
            _vfx.SpawnFloatingText(_player.GetWorldPos(), -kLavaBallDamage, RED);
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
    FILE* f = fopen("keybindings.cfg", "w");
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
    FILE* f = fopen("keybindings.cfg", "r");
    if (!f) return;
    KeyBindings b;
    char key[32];
    int  value;
    while (fscanf(f, "%31s %d", key, &value) == 2)
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
// `offset` is a per-slot drag offset applied on top of the computed base position.
static Rectangle TouchAbilityRect(int slot, int screenW, int screenH,
                                   float btnBotPad, float btnRadius,
                                   float slotSz, float slotGap, float rightPad, float yOff,
                                   Vector2 offset)
{
    const float slotY  = (float)screenH - btnBotPad - btnRadius - yOff - slotSz;
    const float totalW = 4.f * slotSz + 3.f * slotGap;
    const float startX = (float)screenW - rightPad - totalW;
    return { startX + (float)slot * (slotSz + slotGap) + offset.x,
             slotY + offset.y, slotSz, slotSz };
}

Rectangle Engine::GetDebugToggleTabRect() const
{
    const float screenW = (float)GetScreenWidth();
    const float screenH = (float)GetScreenHeight();
    return Rectangle{ screenW - 54.f, screenH * 0.43f, 42.f, 132.f };
}

bool Engine::HandleDebugToggleTabInput()
{
    if (!_debug.IsActive())
        return false;

    const Rectangle debugTab = GetDebugToggleTabRect();
    const int touchCount = GetTouchPointCount();

    if (touchCount == 0)
    {
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) &&
            CheckCollisionPointRec(GetMousePosition(), debugTab))
        {
            _debug.ToggleOpen();
            return true;
        }
        return false;
    }

    for (int i = 0; i < touchCount; ++i)
    {
        if (CheckCollisionPointRec(GetTouchPosition(i), debugTab))
        {
            _debug.ToggleOpen();
            return true;
        }
    }

    return false;
}

// ── Hitbox debug editor ───────────────────────────────────────────────────────

// ── Pregen combat helpers ──────────────────────────────────────────────────────

Vector2 Engine::GetPregenSpawnPos(float cellW, float cellH) const
{
    Vector2 playerPos = _player.GetWorldPos();
    float minDist = cellW * 4.f;   // stay at least 4 cells from the player

    for (int attempt = 0; attempt < 40; attempt++)
    {
        int col = GetRandomValue(2, RoomLayout::kCols - 3);
        int row = GetRandomValue(2, RoomLayout::kRows - 3);

        TileType t = _pregenRoomLayout.tiles[row][col];
        if (t != TileType::Floor && t != TileType::FloorVariant)
            continue;

        // Don't spawn on a prop cell.
        bool onProp = false;
        for (const SpritePlacement& p : _pregenRoomLayout.props)
            if (p.col == col && p.row == row) { onProp = true; break; }
        if (onProp) continue;

        float x = (col + 0.5f) * cellW;
        float y = (row + 0.5f) * cellH;
        float dx = x - playerPos.x;
        float dy = y - playerPos.y;
        if (dx * dx + dy * dy < minDist * minDist) continue;

        return { x, y };
    }

    // Fallback: far corner of the room.
    return { (float)GetScreenWidth() * 0.75f, (float)GetScreenHeight() * 0.5f };
}

void Engine::SpawnPregenRoomEnemies()
{
    if (_pregenViewedRoomIdx < 0) return;

    // Rooms that have already been cleared don't respawn.
    if (_pregenRoomStates[_pregenViewedRoomIdx].cleared) return;

    float sw    = (float)GetScreenWidth();
    float sh    = (float)GetScreenHeight();
    float cellW = sw / (float)RoomLayout::kCols;
    float cellH = sh / (float)RoomLayout::kRows;

    const auto& rooms = _dungeonGen.GetRooms();
    if (_pregenViewedRoomIdx >= (int)rooms.size()) return;

    int i       = _pregenViewedRoomIdx;
    int startIdx = _dungeonGen.GetStartIndex();
    int bossIdx  = _dungeonGen.GetBossIndex();
    RoomType type = rooms[i].type;

    // Non-combat rooms — pre-clear so we never try to spawn here again.
    if (i == startIdx || type == RoomType::Rest || type == RoomType::Treasure)
    {
        _pregenRoomStates[i].cleared = true;
        return;
    }

    auto spawnAt = [&](auto spawnFn, int count = 1) {
        for (int n = 0; n < count; n++)
            spawnFn(GetPregenSpawnPos(cellW, cellH));
    };

    // Count cleared rooms as a progression tier (0 = early, 1 = mid, 2 = late).
    int clearedRooms = 0;
    for (const auto& [idx, state] : _pregenRoomStates)
        if (state.cleared) clearedRooms++;
    int tier = (clearedRooms <= 2) ? 0 : (clearedRooms <= 5) ? 1 : 2;

    if (i == bossIdx)
    {
        // Spawn the boss at the centre-top of the room so it's always in bounds
        // regardless of player entry point. Avoids wall-clip from random placement.
        float bossX = sw * 0.5f;
        float bossY = sh * 0.28f;
        SpawnMolarbeast({ bossX, bossY });
        if (tier >= 1)
            spawnAt([&](Vector2 p){ SpawnCyclops(p); }, 1);
    }
    else if (type == RoomType::Elite)
    {
        SpawnOgre(GetPregenSpawnPos(cellW, cellH));
        spawnAt([&](Vector2 p){ SpawnBasicEnemy(p); }, tier == 0 ? 1 : 2);
    }
    else  // Standard
    {
        // Basic count: 1–2 early, 2–3 mid, 2–4 late.
        int minBasics = tier == 0 ? 1 : 2;
        int maxBasics = tier == 0 ? 2 : (tier == 1 ? 3 : 4);
        spawnAt([&](Vector2 p){ SpawnBasicEnemy(p); }, GetRandomValue(minBasics, maxBasics));

        // Cyclops: rare early, moderate mid, common late.
        int cyclopsRoll = tier == 0 ? 4 : (tier == 1 ? 2 : 1);   // 1-in-N chance
        if (GetRandomValue(0, cyclopsRoll) == 0)
            SpawnCyclops(GetPregenSpawnPos(cellW, cellH));
    }

    _pregenEnemiesSpawned = true;
}

void Engine::ClearPregenEnemies()
{
    for (auto& e : _enemies)
    {
        e->SetActive(false);
        e->Teleport({ -5000.f, -5000.f });
    }
    _spreadProjectiles.clear();
    _cyclopsLasers.clear();
    _lavaBalls.clear();
    _pickups.clear();
    _vfx.Clear();
    _pendingExp    = 0.f;
    _pregenEnemiesSpawned = false;
}


Engine::PregenDoorSide Engine::OppositePregenDoorSide(int dr, int dc) const
{
    if (dr < 0) return PregenDoorSide::South;
    if (dr > 0) return PregenDoorSide::North;
    if (dc < 0) return PregenDoorSide::East;
    if (dc > 0) return PregenDoorSide::West;
    return PregenDoorSide::None;
}

void Engine::ApplyPregenRoomDoorState(RoomLayout& layout, int roomIdx, PregenDoorSide entryDoorSide) const
{
    const auto& rooms = _dungeonGen.GetRooms();
    if (roomIdx < 0 || roomIdx >= (int)rooms.size()) return;

    const DungeonRoom& room = rooms[roomIdx];
    auto stateIt = _pregenRoomStates.find(roomIdx);
    bool cleared = (stateIt != _pregenRoomStates.end() && stateIt->second.cleared);
    bool alwaysOpen = cleared
        || roomIdx == _dungeonGen.GetStartIndex()
        || room.type == RoomType::Rest
        || room.type == RoomType::Treasure
        || room.type == RoomType::Store;

    int doorStartC = RoomLayout::kCols / 2 - 1;
    int doorStartR = RoomLayout::kRows / 2 - 1;

    auto shouldOpen = [&](PregenDoorSide side) {
        return alwaysOpen || side == entryDoorSide;
    };

    auto setNorth = [&](bool open) {
        for (int dc = 0; dc < 3; dc++)
            layout.tiles[0][doorStartC + dc] = open ? TileType::Floor : TileType::WallTopFace;
    };
    auto setSouth = [&](bool open) {
        for (int dc = 0; dc < 3; dc++)
            layout.tiles[RoomLayout::kRows - 1][doorStartC + dc] = open ? TileType::Floor : TileType::WallBottom;
    };
    auto setWest = [&](bool open) {
        for (int dr = 0; dr < 2; dr++)
            layout.tiles[doorStartR + dr][0] = open ? TileType::Floor : TileType::WallLeft;
    };
    auto setEast = [&](bool open) {
        for (int dr = 0; dr < 2; dr++)
            layout.tiles[doorStartR + dr][RoomLayout::kCols - 1] = open ? TileType::Floor : TileType::WallRight;
    };

    if (room.hasNorth) setNorth(shouldOpen(PregenDoorSide::North));
    if (room.hasSouth) setSouth(shouldOpen(PregenDoorSide::South));
    if (room.hasWest)  setWest(shouldOpen(PregenDoorSide::West));
    if (room.hasEast)  setEast(shouldOpen(PregenDoorSide::East));
}

bool Engine::IsPregenDoorOpen(PregenDoorSide side) const
{
    int doorStartC = RoomLayout::kCols / 2 - 1;
    int doorStartR = RoomLayout::kRows / 2 - 1;
    TileType t = TileType::None;

    switch (side)
    {
    case PregenDoorSide::North: t = _pregenRoomLayout.tiles[0][doorStartC + 1]; break;
    case PregenDoorSide::South: t = _pregenRoomLayout.tiles[RoomLayout::kRows - 1][doorStartC + 1]; break;
    case PregenDoorSide::West:  t = _pregenRoomLayout.tiles[doorStartR][0]; break;
    case PregenDoorSide::East:  t = _pregenRoomLayout.tiles[doorStartR][RoomLayout::kCols - 1]; break;
    default: return false;
    }

    return t == TileType::Floor || t == TileType::FloorVariant || t == TileType::DoorOpen;
}

void Engine::SpawnPregenDoorOpenEffects()
{
    const auto& rooms = _dungeonGen.GetRooms();
    if (_pregenViewedRoomIdx < 0 || _pregenViewedRoomIdx >= (int)rooms.size()) return;

    const DungeonRoom& room = rooms[_pregenViewedRoomIdx];
    float sw = (float)GetScreenWidth();
    float sh = (float)GetScreenHeight();
    float cellW = sw / (float)RoomLayout::kCols;
    float cellH = sh / (float)RoomLayout::kRows;
    int doorStartC = RoomLayout::kCols / 2 - 1;
    int doorStartR = RoomLayout::kRows / 2 - 1;
    size_t effectStartCount = _pregenClearEffects.size();

    auto addDoorEffect = [&](PregenDoorSide side, bool exists) {
        if (!exists || side == _pregenEntryDoorSide) return;
        Vector2 pos{};
        switch (side)
        {
        case PregenDoorSide::North: pos = { (doorStartC + 1.5f) * cellW, 0.65f * cellH }; break;
        case PregenDoorSide::South: pos = { (doorStartC + 1.5f) * cellW, (RoomLayout::kRows - 0.65f) * cellH }; break;
        case PregenDoorSide::West:  pos = { 0.65f * cellW, (doorStartR + 1.f) * cellH }; break;
        case PregenDoorSide::East:  pos = { (RoomLayout::kCols - 0.65f) * cellW, (doorStartR + 1.f) * cellH }; break;
        default: return;
        }
        _pregenClearEffects.push_back({ pos, 0.f });
    };

    addDoorEffect(PregenDoorSide::North, room.hasNorth);
    addDoorEffect(PregenDoorSide::South, room.hasSouth);
    addDoorEffect(PregenDoorSide::West,  room.hasWest);
    addDoorEffect(PregenDoorSide::East,  room.hasEast);

    if (_pregenViewedRoomIdx == _dungeonGen.GetBossIndex())
    {
        Rectangle exit = GetPregenBossExitTrigger();
        if (exit.width > 0.f && exit.height > 0.f)
            _pregenClearEffects.push_back({ { exit.x + exit.width * 0.5f, exit.y + exit.height * 0.5f }, 0.f });
    }
    if (_pregenClearEffects.size() == effectStartCount)
        _pregenClearEffects.push_back({ { sw * 0.5f, sh * 0.5f }, 0.f });

    if (_roomClearExplosionSound.frameCount > 0)
    {
        StopSound(_roomClearExplosionSound);
        PlaySound(_roomClearExplosionSound);
    }
}

void Engine::UpdatePregenClearEffects(float dt)
{
    static constexpr float kDuration = 1.75f;
    for (auto& effect : _pregenClearEffects)
        effect.timer += dt;
    _pregenClearEffects.erase(
        std::remove_if(_pregenClearEffects.begin(), _pregenClearEffects.end(),
            [](const PregenClearEffect& effect) { return effect.timer >= kDuration; }),
        _pregenClearEffects.end());
}

void Engine::DrawPregenClearEffects() const
{
    if (_roomClearExplosionTex.id == 0) return;

    static constexpr int kFrameW = 64;
    static constexpr int kFrameH = 64;
    static constexpr int kFrameCount = 24;
    static constexpr float kDuration = 1.75f;
    static constexpr float kScale = 2.0f;

    for (const PregenClearEffect& effect : _pregenClearEffects)
    {
        float pct = std::clamp(effect.timer / kDuration, 0.f, 0.999f);
        int frame = std::min((int)(pct * kFrameCount), kFrameCount - 1);
        Rectangle src = GetAnimationFrameRect(_roomClearExplosionTex, kFrameW, kFrameH, frame);
        Rectangle dst{
            effect.worldPos.x,
            effect.worldPos.y,
            kFrameW * kScale,
            kFrameH * kScale
        };
        DrawTexturePro(_roomClearExplosionTex, src, dst,
            { dst.width * 0.5f, dst.height * 0.5f }, 0.f, WHITE);
    }
}
void Engine::ApplyPregenBossExitTiles(TileType doorType)
{
    int bossIdx = _dungeonGen.GetBossIndex();
    if (bossIdx < 0) return;
    const auto& rooms = _dungeonGen.GetRooms();
    if (bossIdx >= (int)rooms.size()) return;
    const DungeonRoom& boss = rooms[bossIdx];

    int doorStartC = RoomLayout::kCols / 2 - 1;
    int doorStartR = RoomLayout::kRows / 2 - 1;

    // Place the exit on the first wall that has no existing dungeon connection.
    if (!boss.hasSouth)
    {
        for (int dc = 0; dc < 3; dc++)
            _pregenRoomLayout.tiles[RoomLayout::kRows - 1][doorStartC + dc] = doorType;
    }
    else if (!boss.hasNorth)
    {
        for (int dc = 0; dc < 3; dc++)
            _pregenRoomLayout.tiles[0][doorStartC + dc] = doorType;
    }
    else if (!boss.hasEast)
    {
        for (int dr = 0; dr < 2; dr++)
            _pregenRoomLayout.tiles[doorStartR + dr][RoomLayout::kCols - 1] = doorType;
    }
    else
    {
        for (int dr = 0; dr < 2; dr++)
            _pregenRoomLayout.tiles[doorStartR + dr][0] = doorType;
    }
}

Rectangle Engine::GetPregenBossExitTrigger() const
{
    int bossIdx = _dungeonGen.GetBossIndex();
    if (bossIdx < 0) return {};
    const auto& rooms = _dungeonGen.GetRooms();
    if (bossIdx >= (int)rooms.size()) return {};
    const DungeonRoom& boss = rooms[bossIdx];

    float sw    = (float)GetScreenWidth();
    float sh    = (float)GetScreenHeight();
    float cellW = sw / (float)RoomLayout::kCols;
    float cellH = sh / (float)RoomLayout::kRows;
    int doorStartC = RoomLayout::kCols / 2 - 1;
    int doorStartR = RoomLayout::kRows / 2 - 1;

    if (!boss.hasSouth)
        return { doorStartC * cellW, (RoomLayout::kRows - 1) * cellH, 3.f * cellW, cellH };
    if (!boss.hasNorth)
        return { doorStartC * cellW, 0.f, 3.f * cellW, cellH };
    if (!boss.hasEast)
        return { (RoomLayout::kCols - 1) * cellW, doorStartR * cellH, cellW, 2.f * cellH };
    return { 0.f, doorStartR * cellH, cellW, 2.f * cellH };
}

void Engine::RebuildPregenNav()
{
    float sw    = (float)GetScreenWidth();
    float sh    = (float)GetScreenHeight();
    float cellW = sw / (float)RoomLayout::kCols;
    float cellH = sh / (float)RoomLayout::kRows;

    std::vector<Rectangle> solids;

    // Wall tiles — every non-floor, non-void tile blocks a full cell.
    for (int r = 0; r < RoomLayout::kRows; r++)
    {
        for (int c = 0; c < RoomLayout::kCols; c++)
        {
            TileType t = _pregenRoomLayout.tiles[r][c];
            if (t == TileType::Floor        || t == TileType::FloorVariant ||
                t == TileType::DoorOpen     || t == TileType::Void         ||
                t == TileType::None)
                continue;
            solids.push_back({ c * cellW, r * cellH, cellW, cellH });
        }
    }

    // Props (full cell approximation is accurate enough for nav pathfinding).
    for (const SpritePlacement& p : _pregenRoomLayout.props)
        solids.push_back({ p.col * cellW, p.row * cellH, cellW, cellH });

    _nav.CancelAndReset();
    _nav.Rebuild(sw, sh, solids);
    _nav.RefreshSync(_player.GetFeetWorldPos());
}

void Engine::ResolvePregenEnemyCollisions()
{
    float sw    = (float)GetScreenWidth();
    float sh    = (float)GetScreenHeight();
    float cellW = sw / (float)RoomLayout::kCols;
    float cellH = sh / (float)RoomLayout::kRows;
    float pxSX  = cellW / 16.f;
    float pxSY  = cellH / 16.f;

    for (auto& e : _enemies)
    {
        if (!e->IsActive()) continue;

        // ── Wall tiles — only check the 5×5 neighbourhood around the enemy ────
        Vector2 ePos = e->GetWorldPos();
        int ec = std::max(0, std::min((int)(ePos.x / cellW), RoomLayout::kCols - 1));
        int er = std::max(0, std::min((int)(ePos.y / cellH), RoomLayout::kRows - 1));

        for (int r = std::max(0, er - 2); r <= std::min(RoomLayout::kRows - 1, er + 2); r++)
        {
            for (int c = std::max(0, ec - 2); c <= std::min(RoomLayout::kCols - 1, ec + 2); c++)
            {
                TileType t = _pregenRoomLayout.tiles[r][c];
                if (t == TileType::Floor        || t == TileType::FloorVariant ||
                    t == TileType::DoorOpen     || t == TileType::Void         ||
                    t == TileType::None)
                    continue;

                Rectangle wallRect{ c * cellW, r * cellH, cellW, cellH };
                Rectangle eRect = e->GetCollisionRec();
                if (!CheckCollisionRecs(eRect, wallRect)) continue;

                if (e->IsBeingForcedPushed())
                {
                    e->OnForcedPushCollision();
                }
                else
                {
                    float oL = (eRect.x + eRect.width)  - wallRect.x;
                    float oR = (wallRect.x + wallRect.width) - eRect.x;
                    float oT = (eRect.y + eRect.height) - wallRect.y;
                    float oB = (wallRect.y + wallRect.height) - eRect.y;
                    float rX = (oL < oR) ? -oL : oR;
                    float rY = (oT < oB) ? -oT : oB;
                    Vector2 p = e->GetWorldPos();
                    if (std::abs(rX) < std::abs(rY)) p.x += rX; else p.y += rY;
                    e->Teleport(p);
                }
            }
        }

        // ── Props — precise collision rect ────────────────────────────────────
        for (const SpritePlacement& prop : _pregenRoomLayout.props)
        {
            if (prop.defIdx < 0 || prop.defIdx >= (int)_tileDefs.props.size()) continue;
            const Rectangle& coll = _tileDefs.props[prop.defIdx].collision;
            Rectangle propRect{
                prop.col * cellW + coll.x * pxSX,
                prop.row * cellH + coll.y * pxSY,
                coll.width  * pxSX,
                coll.height * pxSY
            };
            Rectangle eRect = e->GetCollisionRec();
            if (!CheckCollisionRecs(eRect, propRect)) continue;

            if (e->IsBeingForcedPushed())
            {
                e->OnForcedPushCollision();
            }
            else
            {
                float oL = (eRect.x + eRect.width)  - propRect.x;
                float oR = (propRect.x + propRect.width)  - eRect.x;
                float oT = (eRect.y + eRect.height) - propRect.y;
                float oB = (propRect.y + propRect.height) - eRect.y;
                float rX = (oL < oR) ? -oL : oR;
                float rY = (oT < oB) ? -oT : oB;
                Vector2 p = e->GetWorldPos();
                if (std::abs(rX) < std::abs(rY)) p.x += rX; else p.y += rY;
                e->Teleport(p);
            }
        }
    }
}

Rectangle Engine::GetPregenRoomRect(int roomIdx) const
{
    const auto& rooms = _dungeonGen.GetRooms();
    if (roomIdx < 0 || roomIdx >= (int)rooms.size())
        return {};

    float sw = (float)GetScreenWidth();
    float sh = (float)GetScreenHeight();
    float margin = 90.f;
    float gridPx = std::min((sw - margin * 2.f) / DungeonGen::kGridSize,
                            (sh - margin * 2.f - 80.f) / DungeonGen::kGridSize);
    float startX = sw * 0.5f - (gridPx * DungeonGen::kGridSize) * 0.5f;
    float startY = margin;

    const DungeonRoom& room = rooms[roomIdx];
    float pad = gridPx * 0.12f;
    return Rectangle{
        startX + room.col * gridPx + pad,
        startY + room.row * gridPx + pad,
        gridPx - pad * 2.f,
        gridPx - pad * 2.f
    };
}

void Engine::UpdatePregenTest(float dt)
{
    // ── Play view — player walks around the room ──────────────────────────────
    if (_pregenView == PregenView::Play)
    {
        if (!_pregenScrolling && IsKeyPressed(KEY_ESCAPE))
        {
            _stateBeforePause = GameState::PregenTest;
            _gameState = GameState::Pause;
            return;
        }

        float sw = (float)GetScreenWidth();
        float sh = (float)GetScreenHeight();

        // ── Scroll animation ─────────────────────────────────────────────────
        if (_pregenScrolling)
        {
            _pregenScrollT += dt / kPregenScrollDur;
            if (_pregenScrollT >= 1.f)
            {
                _pregenScrollT       = 1.f;
                _pregenScrolling     = false;
                _pregenRoomLayout    = _pregenScrollNextLayout;
                _pregenViewedRoomIdx = _pregenScrollNextIdx;
                _pregenEntryDoorSide = _pregenScrollNextEntryDoorSide;
                ApplyPregenRoomDoorState(_pregenRoomLayout, _pregenViewedRoomIdx, _pregenEntryDoorSide);
                _player.SetWorldPos(_pregenScrollSpawnPos);
                _player.Revive();

                // Clear previous room's enemies BEFORE spawning the new set.
                ClearPregenEnemies();

                // Rebuild nav grid (walls + props) and spawn enemies for the new room.
                RebuildPregenNav();
                SpawnPregenRoomEnemies();
                if (_pregenViewedRoomIdx == _dungeonGen.GetBossIndex())
                    ApplyPregenBossExitTiles(TileType::DoorLocked);
            }
            _cameraPos = { sw * 0.5f, sh * 0.5f };
            return;
        }

        
        if (_debug.IsActive())
        {
            if (HandleDebugToggleTabInput())
                return;

            if (_debug.IsGodMode())
                _player.GrantInvulnerability(0.2f);

            DebugCommand cmd = _debug.Update();
            if (cmd.issued)
            {
                Vector2 spawnBase = _player.GetWorldPos();
                switch (cmd.action)
                {
                case DebugActionKind::GrantInvuln:
                    _player.GrantInvulnerability(5.f); break;
                case DebugActionKind::ClearEnemiesContinue:
                    for (auto& e : _enemies) { e->SetActive(false); e->Teleport(Vector2{ -5000.f, -5000.f }); }
                    _pregenRoomStates[_pregenViewedRoomIdx].cleared = true;
                    _pregenEnemiesSpawned = false;
                    ApplyPregenRoomDoorState(_pregenRoomLayout, _pregenViewedRoomIdx, _pregenEntryDoorSide);
                    if (_pregenViewedRoomIdx == _dungeonGen.GetBossIndex())
                        ApplyPregenBossExitTiles(TileType::DoorOpen);
                    SpawnPregenDoorOpenEffects();
                    RebuildPregenNav();
                    break;
                case DebugActionKind::RestartRoom:
                    DebugRestartPregenRoomAs((RoomType)cmd.value); break;
                case DebugActionKind::SetEliteMechanic:
                    _debug.SetForcedEliteMechanic(cmd.value);
                    DebugRestartPregenRoomAs(RoomType::Elite); break;
                case DebugActionKind::SpawnGrunt:
                    SpawnBasicEnemy(Vector2Add(spawnBase, Vector2{ 220.f, 40.f }));
                    _pregenEnemiesSpawned = true; break;
                case DebugActionKind::SpawnCyclops:
                    SpawnCyclops(Vector2Add(spawnBase, Vector2{ 260.f, -60.f }));
                    _pregenEnemiesSpawned = true; break;
                case DebugActionKind::SpawnOgre:
                    SpawnOgre(Vector2Add(spawnBase, Vector2{ -240.f, 50.f }));
                    _pregenEnemiesSpawned = true; break;
                case DebugActionKind::SpawnBoss:
                    SpawnMolarbeast(Vector2Add(spawnBase, Vector2{ 0.f, -260.f }));
                    _pregenEnemiesSpawned = true; break;
                case DebugActionKind::Heal:
                    _player.Heal(cmd.value); break;
                case DebugActionKind::RestoreMana:
                    _player.RestoreMana(cmd.value); break;
                case DebugActionKind::AddGold:
                    _player.AddGold(cmd.value); break;
                case DebugActionKind::AddExp:
                    _player.AddExp(cmd.value); break;
                case DebugActionKind::TreasureCards:
                    GenerateLevelUpOptions(LevelUpOfferContext::TreasureBasic);
                    _levelUpReturnState = GameState::PregenTest; _levelUpOpenTimer = 0.1f;
                    _gameState = GameState::LevelUpChoice; break;
                case DebugActionKind::EliteReward:
                    GenerateLevelUpOptions(LevelUpOfferContext::EliteReward);
                    _levelUpReturnState = GameState::PregenTest; _levelUpOpenTimer = 0.1f;
                    _gameState = GameState::LevelUpChoice; break;
                case DebugActionKind::AbilityReward:
                    GenerateAbilityChoiceOptions(); _abilityChoiceOpenTimer = 0.1f;
                    _levelUpReturnState = GameState::PregenTest; _pendingRoomChoice = false;
                    _gameState = GameState::AbilityChoice; break;
                case DebugActionKind::ApplyUpgrade:
                    _player.ApplyUpgrade((UpgradeType)cmd.value); break;
                default: break;
                }
            }
        }

        if (_debug.IsActive() && IsKeyPressed(KEY_ZERO))
        {
            _isHitboxEditorActive = !_isHitboxEditorActive;
            _hitboxSelectedEntity = nullptr;
            _hitboxEditAttack     = false;
            _hitboxNudgeAccum     = -kHitboxNudgeInitDelay;
            if (_isHitboxEditorActive)
                _debug.SetOpen(false);
        }
        if (_isHitboxEditorActive && !_debug.IsActive())
        {
            _isHitboxEditorActive = false;
            _hitboxSelectedEntity = nullptr;
        }
        if (_isHitboxEditorActive)
        {
            UpdateHitboxEditor();
            return;
        }
        // ── Normal player update ──────────────────────────────────────────────
        if (_player.GetHealthValue() <= 0.f && !_playerDying)
        {
            _playerDying   = true;
            _gameOverTimer = _gameOverDelay;
        }
        if (_playerDying)
        {
            _gameOverTimer -= dt;
            if (_gameOverTimer <= 0.f)
            {
                _playerDying = false;
                _gameState   = GameState::GameOver;
            }
            _cameraPos = { sw * 0.5f, sh * 0.5f };
            return;
        }

        _player.SetCombatLocked(false);
        _player.SetTouchModeEnabled(false);
        _player.Update(dt);
        _player.ConsumeCastRequest();

        float cellW = sw / (float)RoomLayout::kCols;
        float cellH = sh / (float)RoomLayout::kRows;

        Vector2 pos = _player.GetWorldPos();
        const auto& rooms = _dungeonGen.GetRooms();
        const DungeonRoom& cur = rooms[_pregenViewedRoomIdx];

        // Door ranges in world (= screen) coords.
        int   doorCentreC = RoomLayout::kCols / 2;
        float doorLeft    = (doorCentreC - 1) * cellW;
        float doorRight   = (doorCentreC + 2) * cellW;
        int   doorCentreR = RoomLayout::kRows / 2;
        float doorTop     = (doorCentreR - 1) * cellH;
        float doorBot     = (doorCentreR + 1) * cellH;

        // Which walls have open doorways the player can walk through?
        bool canPassNorth = cur.hasNorth && IsPregenDoorOpen(PregenDoorSide::North) && pos.x > doorLeft && pos.x < doorRight;
        bool canPassSouth = cur.hasSouth && IsPregenDoorOpen(PregenDoorSide::South) && pos.x > doorLeft && pos.x < doorRight;
        bool canPassWest  = cur.hasWest  && IsPregenDoorOpen(PregenDoorSide::West)  && pos.y > doorTop  && pos.y < doorBot;
        bool canPassEast  = cur.hasEast  && IsPregenDoorOpen(PregenDoorSide::East)  && pos.y > doorTop  && pos.y < doorBot;

        // Helper: begin a Zelda-style scroll into an adjacent room.
        // scrollVec describes which direction the CURRENT room slides offscreen.
        auto startScroll = [&](int dr, int dc, Vector2 scrollVec, Vector2 spawnPos)
        {
            int nextIdx = _dungeonGen.GetNeighborIndex(_pregenViewedRoomIdx, dr, dc);
            if (nextIdx < 0) return;
            const DungeonRoom& next = rooms[nextIdx];
            _pregenScrollNextLayout  = RoomLayout::Generate(
                next.hasNorth, next.hasSouth, next.hasEast, next.hasWest, next.type,
                (int)_tileDefs.props.size(), (int)_tileDefs.decors.size(),
                (int)_tileDefs.animDecors.size(), (int)_tileDefs.animProps.size());
            _pregenScrollNextEntryDoorSide = OppositePregenDoorSide(dr, dc);
            ApplyPregenRoomDoorState(_pregenScrollNextLayout, nextIdx, _pregenScrollNextEntryDoorSide);
            _pregenScrollNextIdx     = nextIdx;
            _pregenScrollSpawnPos    = spawnPos;
            _pregenScrollVec         = scrollVec;
            _pregenScrollT           = 0.f;
            _pregenScrolling         = true;
        };

        // Check door thresholds and trigger scroll.
        if      (canPassNorth && pos.y < cellH)
            startScroll(-1,  0, { 0.f,  1.f }, { pos.x, (RoomLayout::kRows - 2) * cellH });
        else if (canPassSouth && pos.y > (RoomLayout::kRows - 1) * cellH)
            startScroll(+1,  0, { 0.f, -1.f }, { pos.x, cellH * 2.f });
        else if (canPassWest  && pos.x < cellW)
            startScroll( 0, -1, { 1.f,  0.f }, { (RoomLayout::kCols - 2) * cellW, pos.y });
        else if (canPassEast  && pos.x > (RoomLayout::kCols - 1) * cellW)
            startScroll( 0, +1, {-1.f,  0.f }, { cellW * 2.f, pos.y });

        // Wall collision: solid walls clamp, door openings let the player through.
        if (!_pregenScrolling)
        {
            Vector2 posBefore = pos;
            if (!canPassNorth) pos.y = std::max(pos.y, cellH);
            if (!canPassSouth) pos.y = std::min(pos.y, (RoomLayout::kRows - 2) * cellH);
            if (!canPassWest)  pos.x = std::max(pos.x, cellW);
            if (!canPassEast)  pos.x = std::min(pos.x, (RoomLayout::kCols - 1) * cellW);
            _player.SetWorldPos(pos);
            // End any forced push the moment the player touches a wall.
            if ((pos.x != posBefore.x || pos.y != posBefore.y) && _player.IsBeingForcedPushed())
                _player.OnForcedPushCollision();

            // Prop collision — resolve player out of each prop's stored collision rect.
            float pxScaleX = cellW / 16.f;
            float pxScaleY = cellH / 16.f;
            for (const SpritePlacement& prop : _pregenRoomLayout.props)
            {
                if (prop.defIdx < 0 || prop.defIdx >= (int)_tileDefs.props.size()) continue;
                const Rectangle& coll = _tileDefs.props[prop.defIdx].collision;
                Rectangle propRect{
                    prop.col * cellW + coll.x * pxScaleX,
                    prop.row * cellH + coll.y * pxScaleY,
                    coll.width  * pxScaleX,
                    coll.height * pxScaleY
                };
                Rectangle playerRect = _player.GetCollisionRec();
                if (!CheckCollisionRecs(playerRect, propRect)) continue;

                bool wasPushed = _player.IsBeingForcedPushed();
                float overlapL = (playerRect.x + playerRect.width)  - propRect.x;
                float overlapR = (propRect.x   + propRect.width)    - playerRect.x;
                float overlapT = (playerRect.y + playerRect.height) - propRect.y;
                float overlapB = (propRect.y   + propRect.height)   - playerRect.y;
                float resolveX = (overlapL < overlapR) ? -overlapL : overlapR;
                float resolveY = (overlapT < overlapB) ? -overlapT : overlapB;

                Vector2 p = _player.GetWorldPos();
                if (std::abs(resolveX) < std::abs(resolveY))
                    p.x += resolveX;
                else
                    p.y += resolveY;
                _player.SetWorldPos(p);
                // End forced push on prop impact, same as in the main game.
                if (wasPushed)
                    _player.OnForcedPushCollision();
            }
        }

        // ── Combat update ─────────────────────────────────────────────────────
        _nav.TickRefresh(dt, _player.GetFeetWorldPos());
        _nav.ApplyPendingRefresh();

        EnemyRuntimeContext eCtx{};
        eCtx.player             = &_player;
        eCtx.nav                = &_nav;
        eCtx.props              = &_props;   // empty in pregen mode
        eCtx.enemies            = &_enemies;
        eCtx.cyclopsLasers      = &_cyclopsLasers;
        eCtx.lavaBalls          = &_lavaBalls;
        eCtx.triggerScreenShake = [&](float s, float d){ TriggerScreenShake(s, d); };
        _combatDirector.UpdateEnemyRuntime(eCtx, dt);

        HandlePlayerMeleeDamage();
        ResolvePregenEnemyCollisions();

        // Player-enemy capsule separation — gives enemies physical presence.
        // Dash passes through enemies; on landing inside one, the player is ejected.
        if (!_player.IsDashing())
        {
            for (auto& enemy : _enemies)
            {
                if (!enemy->IsActive() || !enemy->IsAlive()) continue;
                Vector2 mtv{};
                if (!CheckCapsuleCapsule(_player.GetCapsule(), enemy->GetCapsule(), mtv)) continue;
                if (_player.IsBeingForcedPushed()) continue;
                Vector2 ppos = _player.GetWorldPos();
                _player.SetWorldPos({ ppos.x + mtv.x, ppos.y + mtv.y });
            }
        }

        // Keep all enemies within the tile room bounds (one cell margin from edges).
        {
            float roomRight  = (float)GetScreenWidth()  - cellW;
            float roomBottom = (float)GetScreenHeight() - cellH;
            for (auto& enemy : _enemies)
            {
                if (!enemy->IsActive() || enemy->IsDying()) continue;
                Vector2 epos = enemy->GetWorldPos();
                bool clamped = false;
                if (epos.x < cellW)              { epos.x = cellW;        clamped = true; }
                if (epos.x > roomRight)          { epos.x = roomRight;    clamped = true; }
                if (epos.y < cellH)              { epos.y = cellH;        clamped = true; }
                if (epos.y > roomBottom)         { epos.y = roomBottom;   clamped = true; }
                if (clamped)
                {
                    if (Molarbeast* mb = enemy->AsMolarbeast()) { if (mb->IsDashing()) mb->OnDashBlocked(); }
                    else if (Ogre*  og = enemy->AsOgre())       { if (og->IsRushing()) og->OnRushBlocked(); }
                    enemy->Teleport(epos);
                }
            }
        }

        UpdateSpreadProjectiles(dt);
        UpdateLavaBallProjectiles(dt);
        UpdateCyclopsLasers(dt);
        _vfx.Update(dt);
        UpdatePregenClearEffects(dt);
        UpdateEnemyCount(dt);
        // Drain pending EXP slowly (50/sec) so the HUD bar animates, then trigger
        // a LevelUpChoice screen for each level the player gains — same as the
        // wave-based ExpTally flow but without the full overlay screen.
        if (!_pregenScrolling && _pendingExp > 0.f)
        {
            static constexpr float kExpDrainRate = 50.f;
            float drain        = std::min(kExpDrainRate * dt, _pendingExp);
            _pendingExp       -= drain;
            _expTallyAccum    += drain;

            int levelBefore = _player.GetLevel();
            int wholeExp    = (int)_expTallyAccum;
            if (wholeExp > 0)
            {
                _expTallyAccum -= (float)wholeExp;
                _player.AddExp(wholeExp);
            }
            if (_pendingExp <= 0.f)
            {
                _pendingExp    = 0.f;
                _expTallyAccum = 0.f;
            }
            _tallyLevelUpsRemaining += _player.GetLevel() - levelBefore;
        }

        // Show a LevelUpChoice card screen for every level gained once the EXP drain finishes.
        if (!_pregenScrolling && _pendingExp <= 0.f && _tallyLevelUpsRemaining > 0)
        {
            _tallyLevelUpsRemaining--;
            GenerateLevelUpOptions(LevelUpOfferContext::NormalLevel);
            _levelUpReturnState = GameState::PregenTest;
            _levelUpOpenTimer   = 0.25f;
            _gameState          = GameState::LevelUpChoice;
        }

        // ── Room clear detection ───────────────────────────────────────────────
        if (_pregenEnemiesSpawned)
        {
            bool allDead = true;
            for (const auto& e : _enemies)
                if (e->IsActive()) { allDead = false; break; }
            if (allDead)
            {
                _pregenRoomStates[_pregenViewedRoomIdx].cleared = true;
                _pregenEnemiesSpawned = false;
                ApplyPregenRoomDoorState(_pregenRoomLayout, _pregenViewedRoomIdx, _pregenEntryDoorSide);
                if (_pregenViewedRoomIdx == _dungeonGen.GetBossIndex())
                    ApplyPregenBossExitTiles(TileType::DoorOpen);
                SpawnPregenDoorOpenEffects();
                RebuildPregenNav();
            }
        }

        // ── Boss exit trigger ──────────────────────────────────────────────────
        int bossIdx = _dungeonGen.GetBossIndex();
        if (_pregenViewedRoomIdx == bossIdx && _pregenRoomStates[bossIdx].cleared)
        {
            Rectangle exitRect = GetPregenBossExitTrigger();
            if (CheckCollisionPointRec(_player.GetWorldPos(), exitRect))
            {
                // Boss cleared — generate a fresh dungeon.
                // (Biome/map selection will go here when ready.)
                ClearPregenEnemies();
                _dungeonGen.Generate();
                const auto& freshRooms = _dungeonGen.GetRooms();
                int freshStart = _dungeonGen.GetStartIndex();
                _pregenViewedRoomIdx = freshStart;
                const DungeonRoom& fr = freshRooms[freshStart];
                _pregenRoomLayout = RoomLayout::Generate(
                    fr.hasNorth, fr.hasSouth, fr.hasEast, fr.hasWest, fr.type,
                    (int)_tileDefs.props.size(),      (int)_tileDefs.decors.size(),
                    (int)_tileDefs.animDecors.size(), (int)_tileDefs.animProps.size());
                _pregenRoomStates.clear();
                _pregenEntryDoorSide = PregenDoorSide::None;
                ApplyPregenRoomDoorState(_pregenRoomLayout, _pregenViewedRoomIdx, _pregenEntryDoorSide);
                _pregenEnemiesSpawned = false;
                _pregenScrolling      = false;
                _player.SetWorldPos({ sw * 0.5f, sh * 0.5f });
                _player.Revive();
                _cameraPos = { sw * 0.5f, sh * 0.5f };
                RebuildPregenNav();
                SpawnPregenRoomEnemies();
                return;
            }
        }

        // Room fills the screen — camera stays fixed at screen centre.
        _cameraPos = { sw * 0.5f, sh * 0.5f };
        return;
    }

    // ── Room view — static tile preview ──────────────────────────────────────
    if (_pregenView == PregenView::Room)
    {
        if (IsKeyPressed(KEY_ESCAPE))
        {
            _pregenView          = PregenView::Graph;
            _pregenViewedRoomIdx = -1;
        }

        // [Enter] enters the room with the player for walking around.
        if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER))
        {
            float hw = GetScreenWidth()  * 0.5f;
            float hh = GetScreenHeight() * 0.5f;
            _player.SetWorldPos({ hw, hh });
            _player.Revive();
            _cameraPos = { hw, hh };
            _pregenView = PregenView::Play;

            // Build nav grid (walls + props) and spawn room enemies.
            RebuildPregenNav();
            ClearPregenEnemies();
            SpawnPregenRoomEnemies();
            if (_pregenViewedRoomIdx == _dungeonGen.GetBossIndex())
                ApplyPregenBossExitTiles(TileType::DoorLocked);
        }
        return;
    }

    // Graph view.
    if (IsKeyPressed(KEY_ESCAPE))
    {
        _tileRenderer.Unload();
        _menu.Init();
        _gameState = GameState::Menu;
        return;
    }
    if (IsKeyPressed(KEY_R))
        _dungeonGen.Generate();

    // Click a room node to open its tile-rendered preview.
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
    {
        const auto& rooms = _dungeonGen.GetRooms();
        for (int i = 0; i < (int)rooms.size(); i++)
        {
            if (CheckCollisionPointRec(GetMousePosition(), GetPregenRoomRect(i)))
            {
                const DungeonRoom& room = rooms[i];
                _pregenRoomLayout    = RoomLayout::Generate(
                    room.hasNorth, room.hasSouth, room.hasEast, room.hasWest, room.type,
                    (int)_tileDefs.props.size(), (int)_tileDefs.decors.size(),
                    (int)_tileDefs.animDecors.size(), (int)_tileDefs.animProps.size());
                _pregenViewedRoomIdx = i;
                _pregenEntryDoorSide = PregenDoorSide::None;
                ApplyPregenRoomDoorState(_pregenRoomLayout, _pregenViewedRoomIdx, _pregenEntryDoorSide);
                _pregenView          = PregenView::Room;
                break;
            }
        }
    }
}

void Engine::DrawPregenTest()
{
    const float sw = (float)GetScreenWidth();
    const float sh = (float)GetScreenHeight();

    // Scale so the room fills the entire game window exactly.
    float scaleX = sw / (RoomLayout::kCols * 16.f);
    float scaleY = sh / (RoomLayout::kRows * 16.f);

    // ── Room type label helper ────────────────────────────────────────────────
    auto drawRoomLabel = [&]()
    {
        const auto& rooms = _dungeonGen.GetRooms();
        if (_pregenViewedRoomIdx < 0 || _pregenViewedRoomIdx >= (int)rooms.size()) return;
        int i = _pregenViewedRoomIdx;
        const char* label =
            i == _dungeonGen.GetStartIndex() ? "START ROOM" :
            i == _dungeonGen.GetBossIndex()  ? "BOSS ROOM"  :
            i == _dungeonGen.GetKeyIndex()   ? "KEY ROOM"   :
            [&]() -> const char* {
                switch (rooms[i].type) {
                case RoomType::Elite:    return "ELITE ROOM";
                case RoomType::Rest:     return "REST ROOM";
                case RoomType::Treasure: return "TREASURE ROOM";
                default:                 return "STANDARD ROOM";
                }
            }();
        int lw = MeasureText(label, 24);
        DrawText(label, (int)(sw * 0.5f - lw * 0.5f), 16, 24, GOLD);
    };

    // ── Play view — tile room with live player ────────────────────────────────
    if (_pregenView == PregenView::Play)
    {
        ClearBackground(Color{ 8, 6, 10, 255 });

        if (_tileRenderer.IsLoaded())
        {
            if (_pregenScrolling)
            {
                float t    = _pregenScrollT;
                float ease = t * t * (3.f - 2.f * t);
                Vector2 curOff{  _pregenScrollVec.x * ease * sw,
                                 _pregenScrollVec.y * ease * sh };
                Vector2 nextOff{ curOff.x - _pregenScrollVec.x * sw,
                                 curOff.y - _pregenScrollVec.y * sh };
                _tileRenderer.DrawRoom(_pregenRoomLayout,       scaleX, scaleY, curOff);
                _tileRenderer.DrawRoom(_pregenScrollNextLayout, scaleX, scaleY, nextOff);
            }
            else
            {
                _tileRenderer.DrawRoom(_pregenRoomLayout, scaleX, scaleY, { 0.f, 0.f });
                DrawPregenClearEffects();

                // Enemies, projectiles, VFX — world == screen in pregen mode.
                Vector2 worldOffset{ -_cameraPos.x, -_cameraPos.y };
                for (const auto& proj : _spreadProjectiles)
                    proj.Draw(worldOffset);
                DrawCyclopsLasers(worldOffset);
                _vfx.Draw(worldOffset, _player.GetWorldPos(), _player.GetCastOrigin());
                for (auto& enemy : _enemies)
                {
                    if (!enemy->IsActive()) continue;
                    enemy->DrawEnemy(_cameraPos);
                }

                _player.DrawPlayer(_cameraPos);
            }
        }
        else
        {
            DrawText("Tilesheet not loaded.", 20, 20, 22, RED);
        }

        if (!_pregenScrolling)
        {
            DrawHUD();
            drawRoomLabel();
            if (_debug.IsActive())
            {
                DrawDebugToggleTab();
                if (_debug.IsOpen())
                    _debug.Draw(_currentAct, _currentRoom, GetDebugRoomTypeName(_currentRoomType));
            }
            if (_isHitboxEditorActive)
                DrawHitboxEditor();
        }
        return;
    }

    // ── Room view — static tile preview ──────────────────────────────────────
    if (_pregenView == PregenView::Room)
    {
        ClearBackground(Color{ 8, 6, 10, 255 });

        if (_tileRenderer.IsLoaded())
            _tileRenderer.DrawRoom(_pregenRoomLayout, scaleX, scaleY, { 0.f, 0.f });
        else
            DrawText("Tilesheet not loaded. Check the path in Engine.cpp.",
                20, 20, 22, RED);

        drawRoomLabel();
        DrawText("[Enter] Walk in room   [ESC] Back to map", 20, (int)(sh - 32.f), 18,
            Fade(WHITE, 0.55f));
        return;
    }

    // ── Graph view ────────────────────────────────────────────────────────────
    ClearBackground(Color{ 14, 14, 20, 255 });

    const auto& rooms = _dungeonGen.GetRooms();
    if (rooms.empty())
    {
        DrawText("No rooms generated.", 40, 40, 28, RED);
        return;
    }

    // Fit the dungeon grid into the centre of the screen.
    const float margin  = 90.f;
    const float gridPx  = std::min((sw - margin * 2.f) / DungeonGen::kGridSize,
                                   (sh - margin * 2.f - 80.f) / DungeonGen::kGridSize);
    const float startX  = sw * 0.5f - (gridPx * DungeonGen::kGridSize) * 0.5f;
    const float startY  = margin;

    int startIdx = _dungeonGen.GetStartIndex();
    int bossIdx  = _dungeonGen.GetBossIndex();
    int keyIdx   = _dungeonGen.GetKeyIndex();

    // Connections drawn first so they sit behind room squares.
    for (const auto& room : rooms)
    {
        float cx = startX + (room.col + 0.5f) * gridPx;
        float cy = startY + (room.row + 0.5f) * gridPx;
        if (room.hasEast)
            DrawLineEx({ cx, cy }, { cx + gridPx, cy }, 3.f, Fade(WHITE, 0.25f));
        if (room.hasSouth)
            DrawLineEx({ cx, cy }, { cx, cy + gridPx }, 3.f, Fade(WHITE, 0.25f));
    }

    // Room squares.
    for (int i = 0; i < (int)rooms.size(); i++)
    {
        const DungeonRoom& room = rooms[i];
        float pad = gridPx * 0.12f;
        Rectangle rect{
            startX + room.col * gridPx + pad,
            startY + room.row * gridPx + pad,
            gridPx - pad * 2.f,
            gridPx - pad * 2.f
        };

        Color col;
        const char* label = "";
        if (i == startIdx) { col = Color{ 70,  210,  80, 255 }; label = "START";   }
        else if (i == bossIdx)  { col = Color{210,  50,   50, 255 }; label = "BOSS";    }
        else if (i == keyIdx)   { col = Color{160,  80,  220, 255 }; label = "KEY";     }
        else switch (room.type)
        {
        case RoomType::Elite:    col = Color{220, 130,  40, 255 }; label = "ELITE";    break;
        case RoomType::Rest:     col = Color{ 55, 160,  80, 255 }; label = "REST";     break;
        case RoomType::Treasure: col = Color{200, 180,  40, 255 }; label = "CHEST";    break;
        case RoomType::Store:    col = Color{ 55, 140, 200, 255 }; label = "SHOP";     break;
        default:                 col = Color{ 75,  75,  90, 255 }; break;
        }

        DrawRectangleRounded(rect, 0.22f, 6, col);
        DrawRectangleRoundedLines(rect, 0.22f, 6, Fade(WHITE, 0.35f));

        if (label[0] != '\0')
        {
            int fs = (int)(gridPx * 0.16f);
            fs = std::max(8, std::min(fs, 20));
            int lw = MeasureText(label, fs);
            DrawText(label,
                (int)(rect.x + rect.width  * 0.5f - lw * 0.5f),
                (int)(rect.y + rect.height * 0.5f - fs * 0.5f),
                fs, WHITE);
        }
    }

    // Title
    const char* title = "PREGEN MAP TEST";
    int titleW = MeasureText(title, 32);
    DrawText(title, (int)(sw * 0.5f - titleW * 0.5f), 16, 32, GOLD);

    const char* info = TextFormat("%d rooms  |  Click a room to preview  |  [R] Regenerate  |  [ESC] Back",
        (int)rooms.size());
    int infoW = MeasureText(info, 18);
    DrawText(info, (int)(sw * 0.5f - infoW * 0.5f), (int)(sh - 32.f), 18, Fade(WHITE, 0.55f));

    // Colour legend
    struct LegendEntry { Color color; const char* name; };
    const LegendEntry legend[] = {
        { Color{ 70, 210,  80, 255}, "Start"    },
        { Color{210,  50,  50, 255}, "Boss"     },
        { Color{160,  80, 220, 255}, "Key Room" },
        { Color{220, 130,  40, 255}, "Elite"    },
        { Color{ 55, 160,  80, 255}, "Rest"     },
        { Color{200, 180,  40, 255}, "Treasure" },
        { Color{ 55, 140, 200, 255}, "Shop"     },
        { Color{ 75,  75,  90, 255}, "Standard" },
    };
    float lx = 18.f;
    float ly = sh - 60.f;
    for (const auto& e : legend)
    {
        DrawRectangleRounded({ lx, ly, 16.f, 16.f }, 0.3f, 4, e.color);
        DrawText(e.name, (int)(lx + 22.f), (int)(ly + 1.f), 14, Fade(WHITE, 0.65f));
        lx += MeasureText(e.name, 14) + 44.f;
    }
}

void Engine::UpdateHitboxEditor()
{
    if (IsKeyPressed(KEY_ESCAPE))
    {
        _isHitboxEditorActive = false;
        _hitboxSelectedEntity = nullptr;
        return;
    }

    // Click to select an entity
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
    {
        Vector2 camRef = Vector2Subtract(_cameraPos, _shakeOffset);
        float sw = (float)GetScreenWidth(), sh = (float)GetScreenHeight();
        auto toScreen = [&](Rectangle r) {
            return Rectangle{ r.x - camRef.x + sw / 2.f, r.y - camRef.y + sh / 2.f, r.width, r.height };
        };

        Vector2 mouse = GetMousePosition();
        BaseCharacter* hit = nullptr;

        // Helper: check if mouse is inside a capsule (screen space)
        auto mouseInCapsule = [&](const Capsule2D& cap) {
            Vector2 sc = { cap.center.x - camRef.x + sw*0.5f, cap.center.y - camRef.y + sh*0.5f };
            float syTop = sc.y - cap.halfHeight, syBot = sc.y + cap.halfHeight;
            Vector2 closest = { sc.x, std::max(syTop, std::min(mouse.y, syBot)) };
            return Vector2Distance(mouse, closest) < cap.radius;
        };

        if (mouseInCapsule(_player.GetCapsule()))
            hit = &_player;

        if (!hit)
        {
            for (auto& e : _enemies)
            {
                if (!e->IsActive()) continue;
                if (mouseInCapsule(e->GetCapsule()))
                {
                    hit = e.get();
                    break;
                }
            }
        }

        if (hit != _hitboxSelectedEntity)
            _hitboxEditAttack = false;
        _hitboxSelectedEntity = hit;
    }

    if (!_hitboxSelectedEntity) return;
    _hitboxSelectedEntity->EnsureCollisionShape();

    // TAB: toggle body / attack box (player and enemies)
    if (IsKeyPressed(KEY_TAB))
        _hitboxEditAttack = !_hitboxEditAttack;

    // S: export values to VS output window
    if (IsKeyPressed(KEY_S))
    {
        const char* typeName = "Unknown";
        if (_hitboxSelectedEntity == &_player)
            typeName = "Player";
        else if (Enemy* e = dynamic_cast<Enemy*>(_hitboxSelectedEntity))
        {
            if      (e->AsCyclops())    typeName = "Cyclops";
            else if (e->AsOgre())       typeName = "Ogre";
            else if (e->AsMolarbeast()) typeName = "Molarbeast";
            else                        typeName = "Grunt";
        }

        if (!_hitboxEditAttack)
        {
            Capsule2D cap = _hitboxSelectedEntity->GetCapsule();
            Vector2   off = _hitboxSelectedEntity->GetCapsuleOffset();
            TraceLog(LOG_WARNING, "=== CAPSULE EXPORT [%s] ===", typeName);
            TraceLog(LOG_WARNING, "s->_capsuleRadius     = %.2ff;", cap.radius);
            TraceLog(LOG_WARNING, "s->_capsuleHalfHeight = %.2ff;", cap.halfHeight);
            TraceLog(LOG_WARNING, "s->_capsuleOffset     = { %.2ff, %.2ff };", off.x, off.y);
            TraceLog(LOG_WARNING, "===========================");
        }
        else if (_hitboxSelectedEntity == &_player)
        {
            TraceLog(LOG_WARNING, "=== ATTACK EXPORT [Player] ===");
            TraceLog(LOG_WARNING, "_attackWidthAdjust  = %.2ff;", _player.GetAttackWidthAdjust());
            TraceLog(LOG_WARNING, "_attackHeightAdjust = %.2ff;", _player.GetAttackHeightAdjust());
            TraceLog(LOG_WARNING, "==============================");
        }
        else if (Enemy* e = dynamic_cast<Enemy*>(_hitboxSelectedEntity))
        {
            TraceLog(LOG_WARNING, "=== ATTACK EXPORT [%s] ===", typeName);
            TraceLog(LOG_WARNING, "float _attackBoxWidth   = %.2ff;", e->GetAttackBoxWidth());
            TraceLog(LOG_WARNING, "float _attackBoxHeight  = %.2ff;", e->GetAttackBoxHeight());
            TraceLog(LOG_WARNING, "float _attackBoxOffsetX = %.2ff;", e->GetAttackBoxOffsetX());
            TraceLog(LOG_WARNING, "float _attackBoxOffsetY = %.2ff;", e->GetAttackBoxOffsetY());
            TraceLog(LOG_WARNING, "==========================");
        }
    }

    // Arrow key nudge with hold-repeat
    float dt = GetFrameTime();
    bool shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);

    bool rDown = IsKeyDown(KEY_RIGHT), lDown = IsKeyDown(KEY_LEFT);
    bool uDown = IsKeyDown(KEY_UP),    dDown = IsKeyDown(KEY_DOWN);
    bool rPressed = IsKeyPressed(KEY_RIGHT), lPressed = IsKeyPressed(KEY_LEFT);
    bool uPressed = IsKeyPressed(KEY_UP),    dPressed = IsKeyPressed(KEY_DOWN);
    bool anyDown    = rDown    || lDown    || uDown    || dDown;
    bool anyPressed = rPressed || lPressed || uPressed || dPressed;

    float dx = 0.f, dy = 0.f;

    if (anyPressed)
    {
        if (rPressed) dx += 1.f;
        if (lPressed) dx -= 1.f;
        if (uPressed) dy -= 1.f;
        if (dPressed) dy += 1.f;
        _hitboxNudgeAccum = -kHitboxNudgeInitDelay;
    }
    else if (anyDown)
    {
        _hitboxNudgeAccum += dt;
        if (_hitboxNudgeAccum >= 0.f)
        {
            _hitboxNudgeAccum -= kHitboxNudgeRepeatRate;
            if (rDown) dx += 1.f;
            if (lDown) dx -= 1.f;
            if (uDown) dy -= 1.f;
            if (dDown) dy += 1.f;
        }
    }
    else
    {
        _hitboxNudgeAccum = -kHitboxNudgeInitDelay;
    }

    if (dx == 0.f && dy == 0.f) return;

    if (!_hitboxEditAttack)
    {
        if (!shift)
        {
            // LR = radius (fatter/skinnier), UD = halfHeight (UP=taller, DOWN=shorter)
            if (dx != 0.f)
                _hitboxSelectedEntity->SetCapsuleRadius(_hitboxSelectedEntity->GetCapsuleRadius() + dx);
            if (dy != 0.f)
                _hitboxSelectedEntity->SetCapsuleHalfHeight(_hitboxSelectedEntity->GetCapsuleHalfHeight() - dy);
        }
        else
        {
            // Shift+arrows = move capsule offset
            Vector2 off = _hitboxSelectedEntity->GetCapsuleOffset();
            off.x += dx;
            off.y += dy;
            _hitboxSelectedEntity->SetCapsuleOffset(off);
        }
    }
    else if (_hitboxSelectedEntity == &_player)
    {
        _player.SetAttackWidthAdjust(_player.GetAttackWidthAdjust() + dx);
        _player.SetAttackHeightAdjust(_player.GetAttackHeightAdjust() + dy);
    }
    else if (Enemy* e = dynamic_cast<Enemy*>(_hitboxSelectedEntity))
    {
        if (!shift)
        {
            e->SetAttackBoxOffsetX(e->GetAttackBoxOffsetX() + dx);
            e->SetAttackBoxOffsetY(e->GetAttackBoxOffsetY() + dy);
        }
        else
        {
            e->SetAttackBoxWidth(std::max(4.f,  e->GetAttackBoxWidth()  + dx));
            e->SetAttackBoxHeight(std::max(4.f, e->GetAttackBoxHeight() + dy));
        }
    }
}

void Engine::DrawHitboxEditor()
{
    Vector2 camRef = Vector2Subtract(_cameraPos, _shakeOffset);
    float sw = (float)GetScreenWidth();
    float sh = (float)GetScreenHeight();

    auto toScreen = [&](Rectangle r) {
        return Rectangle{ r.x - camRef.x + sw / 2.f, r.y - camRef.y + sh / 2.f, r.width, r.height };
    };

    auto drawEntity = [&](BaseCharacter* entity)
    {
        bool isSelected = (entity == _hitboxSelectedEntity);

        // Draw capsule body outline
        Capsule2D cap = entity->GetCapsule();
        Vector2 capScr = { cap.center.x - camRef.x + sw*0.5f, cap.center.y - camRef.y + sh*0.5f };
        Color bodyCol = (isSelected && !_hitboxEditAttack) ? GREEN : RED;
        DrawCapsule2DLines(capScr, cap.halfHeight, cap.radius, bodyCol);

        // Draw centre dot when selected
        if (isSelected && !_hitboxEditAttack)
            DrawCircle((int)capScr.x, (int)capScr.y, 5.f, BLUE);

        if (isSelected)
        {
            Rectangle atkScr = {};
            if (entity == &_player)
                atkScr = toScreen(_player.GetAttackCollisionRec());
            else if (Enemy* e = dynamic_cast<Enemy*>(entity))
                atkScr = toScreen(e->GetAttackCollisionRec());

            Color atkCol   = _hitboxEditAttack ? GREEN : YELLOW;
            float atkThick = _hitboxEditAttack ? 3.f   : 1.5f;
            DrawRectangleLinesEx(atkScr, atkThick, atkCol);

            if (_hitboxEditAttack)
            {
                const float hs = 7.f;
                DrawRectangle((int)(atkScr.x - hs/2.f),                (int)(atkScr.y - hs/2.f),                (int)hs, (int)hs, ORANGE);
                DrawRectangle((int)(atkScr.x + atkScr.width - hs/2.f), (int)(atkScr.y - hs/2.f),                (int)hs, (int)hs, ORANGE);
                DrawRectangle((int)(atkScr.x - hs/2.f),                (int)(atkScr.y + atkScr.height - hs/2.f),(int)hs, (int)hs, ORANGE);
                DrawRectangle((int)(atkScr.x + atkScr.width - hs/2.f), (int)(atkScr.y + atkScr.height - hs/2.f),(int)hs, (int)hs, ORANGE);
            }
        }
    };

    drawEntity(&_player);
    for (auto& e : _enemies)
        if (e->IsActive()) drawEntity(e.get());

    // Top status bar
    DrawRectangle(0, 0, (int)sw, 30, Fade(BLACK, 0.80f));
    const char* modeStr;
    if (!_hitboxSelectedEntity)
        modeStr = "[CAPSULE EDITOR]  Click entity to select  |  ESC: exit";
    else if (_hitboxEditAttack)
        modeStr = "[CAPSULE EDITOR]  ATTACK BOX  |  LR: width   UD: height  |  Shift+LR/UD: offset  |  TAB: capsule  |  S: export  |  ESC: exit";
    else
        modeStr = "[CAPSULE EDITOR]  CAPSULE  |  LR: radius   UD: halfHeight  |  Shift+Arrows: offset  |  TAB: attack  |  S: export  |  ESC: exit";
    DrawText(modeStr, 8, 5, 18, YELLOW);

    // Bottom values bar
    if (_hitboxSelectedEntity)
    {
        DrawRectangle(0, (int)(sh - 30.f), (int)sw, 30, Fade(BLACK, 0.80f));
        const char* info = "";
        if (!_hitboxEditAttack)
        {
            Capsule2D cap = _hitboxSelectedEntity->GetCapsule();
            Vector2   off = _hitboxSelectedEntity->GetCapsuleOffset();
            info = TextFormat("  Capsule  Radius: %.1f   HalfHeight: %.1f   Offset: (%.1f, %.1f)   [S to export]",
                              cap.radius, cap.halfHeight, off.x, off.y);
        }
        else if (_hitboxSelectedEntity == &_player)
        {
            info = TextFormat("  Attack  Width adj: %.1f   Height adj: %.1f   [S to export]",
                              _player.GetAttackWidthAdjust(), _player.GetAttackHeightAdjust());
        }
        else if (Enemy* e = dynamic_cast<Enemy*>(_hitboxSelectedEntity))
        {
            info = TextFormat("  Attack Box  Offset: (%.1f, %.1f)   Size: (%.1f, %.1f)   [S to export]",
                              e->GetAttackBoxOffsetX(), e->GetAttackBoxOffsetY(),
                              e->GetAttackBoxWidth(), e->GetAttackBoxHeight());
        }
        DrawText(info, 8, (int)(sh - 24.f), 18, WHITE);
    }
}

void Engine::DrawDebugToggleTab()
{
    if (!_debug.IsActive())
        return;

    const Rectangle debugTab = GetDebugToggleTabRect();
    DrawRectangleRounded(debugTab, 0.35f, 6, Fade(Color{ 92, 58, 26, 255 }, 0.78f));
    DrawRectangleRoundedLines(debugTab, 0.35f, 6, Fade(Color{ 255, 214, 150, 255 }, 0.66f));

    const char* tabLabel = _debug.IsOpen() ? "DBG <" : "DBG >";
    const int tabSz = 18;
    const int tabW = MeasureText(tabLabel, tabSz);
    DrawText(tabLabel,
        (int)(debugTab.x + debugTab.width * 0.5f - tabW * 0.5f),
        (int)(debugTab.y + debugTab.height * 0.5f - tabSz * 0.5f),
        tabSz, RAYWHITE);
}

// Sets player touch direction/attack/dash from _touch, then scans for new
// touches on the ability arc.  Called each gameplay frame when touch mode is on.
void Engine::UpdateTouchControls()
{
    const int screenW = GetScreenWidth();
    const int screenH = GetScreenHeight();

    // Sync touch layout from HUD config so editor changes take effect immediately
    _touch.kJoyRadius     = _hudCfg.touchJoyR;
    _touch.kBtnRadius     = _hudCfg.touchAtkR;
    _touch.kDashBtnRadius = _hudCfg.touchDashR;
    _touch.kBtnRightPad   = _hudCfg.touchAtkPadR;
    _touch.kBtnBotPad     = _hudCfg.touchAtkPadB;
    _touch.kDashBtnOffset = _hudCfg.touchDashOffset;
    _touch.kAtkLabelFs    = _hudCfg.touchAtkFs;
    _touch.kDashLabelFs   = _hudCfg.touchDashFs;
    _touch.kDashBotPad    = _hudCfg.touchDashBotPad;

    // ── Ability slot drag (only when HUD editor is open) ─────────────────────
    if (_hudEditorActive)
    {
        const Vector2 mousePos = GetMousePosition();
        const int ts = _player.GetMaxAbilitySlots();

        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && _touchSlotDragIdx < 0)
        {
            for (int s = 0; s < ts; s++)
            {
                Rectangle r = TouchAbilityRect(s, screenW, screenH,
                    _hudCfg.touchAtkPadB, _hudCfg.touchAtkR,
                    _hudCfg.touchSlotSz, _hudCfg.touchSlotGap,
                    _hudCfg.touchSlotRightPad, _hudCfg.touchSlotYOff,
                    _touchSlotOffset[s]);
                if (CheckCollisionPointRec(mousePos, r))
                {
                    _touchSlotDragIdx         = s;
                    _touchSlotDragMouseStart  = mousePos;
                    _touchSlotDragOffsetStart = _touchSlotOffset[s];
                    break;
                }
            }
        }

        if (_touchSlotDragIdx >= 0)
        {
            if (IsMouseButtonDown(MOUSE_LEFT_BUTTON))
            {
                Vector2 delta = Vector2Subtract(mousePos, _touchSlotDragMouseStart);
                _touchSlotOffset[_touchSlotDragIdx] = Vector2Add(_touchSlotDragOffsetStart, delta);
            }
            else
            {
                _touchSlotDragIdx = -1;
            }
            _player.SetTouchDirection(Vector2Zero());
            return;
        }
    }

    // ── Zeph tap priority ─────────────────────────────────────────────────────
    // If a new press lands near Zeph's sprite, skip _touch.Update() so the
    // joystick never activates. UpdateNpc() (called later in UpdateGamePlay)
    // will handle the tap independently.
    if (_currentRoomType == RoomType::Store && _shop.IsNearNpc())
    {
        const float sw2 = screenW * 0.5f;
        const float sh2 = screenH * 0.5f;
        const Vector2 worldOff = { -_cameraPos.x + _shakeOffset.x,
                                   -_cameraPos.y + _shakeOffset.y };
        const Vector2 npc = _shop.GetNpcPos();
        const Vector2 npcScreen = { npc.x + worldOff.x + sw2,
                                    npc.y + worldOff.y + sh2 };
        const Rectangle btnRect = _shop.GetNpcTouchBtnRect(npcScreen.x, npcScreen.y);

        bool zephHit = false;
        const int tc0 = GetTouchPointCount();
        if (tc0 == 0)
        {
            zephHit = IsMouseButtonPressed(MOUSE_LEFT_BUTTON) &&
                      CheckCollisionPointRec(GetMousePosition(), btnRect);
        }
        else
        {
            for (int i = 0; i < tc0; i++)
            {
                int id = GetTouchPointId(i);
                if (id == _touch.GetJoyTouchId() ||
                    id == _touch.GetAtkTouchId()  ||
                    id == _touch.GetDashTouchId()) continue;
                if (CheckCollisionPointRec(GetTouchPosition(i), btnRect))
                { zephHit = true; break; }
            }
        }
        if (zephHit) return;
    }

    _touch.Update(screenW, screenH);

    _player.SetTouchDirection(_touch.joystickDir);
    if (_touch.attackPressed) _player.SetTouchAttack();
    if (_touch.dashPressed)   _player.SetTouchDash();

    ScanAbilityArcTaps();

    // Pause button (top-right corner) — same rect as DrawHUD draws it
    const Rectangle pauseRec{ (float)screenW - _hudCfg.touchPauseW - _hudCfg.touchPausePad,
                               _hudCfg.touchPausePad, _hudCfg.touchPauseW, _hudCfg.touchPauseH };

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
    const int screenW    = GetScreenWidth();
    const int screenH    = GetScreenHeight();

    auto hitSlot = [&](Vector2 pos) -> int
    {
        for (int s = 0; s < totalSlots; s++)
            if (CheckCollisionPointRec(pos, TouchAbilityRect(s, screenW, screenH,
                    _touch.kBtnBotPad, _touch.kBtnRadius,
                    _hudCfg.touchSlotSz, _hudCfg.touchSlotGap,
                    _hudCfg.touchSlotRightPad, _hudCfg.touchSlotYOff,
                    _touchSlotOffset[s]))) return s;
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
    const int screenW    = GetScreenWidth();
    const int screenH    = GetScreenHeight();

    for (int slot = 0; slot < totalSlots; slot++)
    {
        AbilityType ability = _player.GetLearnedAbility(slot);
        Rectangle   rec     = TouchAbilityRect(slot, screenW, screenH,
                                  _touch.kBtnBotPad, _touch.kBtnRadius,
                                  _hudCfg.touchSlotSz, _hudCfg.touchSlotGap,
                                  _hudCfg.touchSlotRightPad, _hudCfg.touchSlotYOff,
                                  _touchSlotOffset[slot]);
        bool isEmpty = (ability == AbilityType::None);
        bool canCast = !isEmpty && _player.CanCastAbility(ability);
        bool isDragged = (_hudEditorActive && slot == _touchSlotDragIdx);

        Color bgColor     = isDragged ? Fade(DARKBLUE, 0.70f)
                          : isEmpty   ? Fade(BLACK, 0.30f) : Fade(BLACK, 0.55f);
        Color borderColor = isDragged ? Fade(YELLOW, 0.95f)
                          : isEmpty   ? Fade(WHITE, 0.12f)
                          : canCast   ? Fade(LIGHTGRAY, 0.35f) : Fade(RED, 0.40f);

        DrawRectangleRounded(rec, 0.18f, 6, bgColor);
        DrawRectangleRoundedLines(rec, 0.18f, 6, borderColor);

        if (isEmpty)
        {
            // Show slot number dimly, same as desktop bar
            DrawText(GetKeyName(_player.GetAbilityKey(slot)),
                (int)(rec.x + 8.f), (int)(rec.y + 8.f), 18, Fade(WHITE, 0.25f));
            continue;
        }

        // Slot number in top-left corner
        DrawText(GetKeyName(_player.GetAbilityKey(slot)),
            (int)(rec.x + 8.f), (int)(rec.y + 8.f), 18, Fade(WHITE, 0.6f));

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
        int nameW = MeasureText(abilityName, 16);
        DrawText(abilityName,
            (int)(rec.x + rec.width / 2.f - nameW / 2.f),
            (int)(rec.y + rec.height - 22.f),
            16, canCast ? RAYWHITE : Fade(GRAY, 0.6f));

        // Level badge
        int abilityLv = _player.GetAbilityLevel(ability);
        if (abilityLv > 1)
        {
            const char* badge = TextFormat("Lv%d", abilityLv);
            int badgeW = MeasureText(badge, 16);
            Color badgeColor = (abilityLv >= 3) ? GOLD : Fade(SKYBLUE, 0.9f);
            DrawText(badge,
                (int)(rec.x + rec.width - badgeW - 5.f),
                (int)(rec.y + rec.height - 20.f),
                16, badgeColor);
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Touch Button Mapping Screen
// ─────────────────────────────────────────────────────────────────────────────

float Engine::GetTouchMappingRadius(int idx) const
{
    if (idx == 0) return _hudCfg.touchAtkR;
    if (idx == 1) return _hudCfg.touchDashR;
    return _hudCfg.touchSlotSz * 0.58f;
}

void Engine::EnterTouchButtonMapping()
{
    _touchMappingDragIdx = -1;
    const int sw = GetScreenWidth();
    const int sh = GetScreenHeight();

    if (_touchLayoutCustom)
    {
        for (int i = 0; i < 6; i++) _touchMappingPos[i] = _touchCustomPos[i];
    }
    else
    {
        // Derive positions from current HUDConfig (only valid before any Save has dirtied it)
        _touchMappingPos[0] = { (float)sw - _hudCfg.touchAtkPadR,
                                 (float)sh - _hudCfg.touchAtkPadB };
        const float dashBotY = (_hudCfg.touchDashBotPad >= 0.f)
                               ? _hudCfg.touchDashBotPad : _hudCfg.touchAtkPadB;
        _touchMappingPos[1] = { (float)sw - _hudCfg.touchAtkPadR - _hudCfg.touchDashOffset,
                                 (float)sh - dashBotY };
        for (int s = 0; s < 4; s++)
        {
            Rectangle r = TouchAbilityRect(s, sw, sh,
                _hudCfg.touchAtkPadB, _hudCfg.touchAtkR,
                _hudCfg.touchSlotSz, _hudCfg.touchSlotGap,
                _hudCfg.touchSlotRightPad, _hudCfg.touchSlotYOff,
                _touchSlotOffset[s]);
            _touchMappingPos[2+s] = { r.x + r.width * 0.5f, r.y + r.height * 0.5f };
        }

        // Capture the baked-in defaults exactly once — before any Save can dirty _hudCfg
        if (!_touchDefaults.captured)
        {
            _touchDefaults.atkPadR    = _hudCfg.touchAtkPadR;
            _touchDefaults.atkPadB    = _hudCfg.touchAtkPadB;
            _touchDefaults.dashOffset = _hudCfg.touchDashOffset;
            _touchDefaults.dashBotPad = _hudCfg.touchDashBotPad;
            for (int s = 0; s < 4; s++) _touchDefaults.slotOffset[s] = _touchSlotOffset[s];
            for (int i = 0; i < 6; i++) _touchDefaults.pos[i] = _touchMappingPos[i];
            _touchDefaults.captured = true;
        }
    }
}

void Engine::ApplyTouchCustomLayout()
{
    const int sw = GetScreenWidth();
    const int sh = GetScreenHeight();

    _hudCfg.touchAtkPadR    = (float)sw - _touchCustomPos[0].x;
    _hudCfg.touchAtkPadB    = (float)sh - _touchCustomPos[0].y;
    _hudCfg.touchDashBotPad = (float)sh - _touchCustomPos[1].y;
    _hudCfg.touchDashOffset  = _touchCustomPos[0].x - _touchCustomPos[1].x;

    for (int s = 0; s < 4; s++)
    {
        Rectangle rDef = TouchAbilityRect(s, sw, sh,
            _hudCfg.touchAtkPadB, _hudCfg.touchAtkR,
            _hudCfg.touchSlotSz, _hudCfg.touchSlotGap,
            _hudCfg.touchSlotRightPad, _hudCfg.touchSlotYOff,
            {0.f, 0.f});
        Vector2 defCenter = { rDef.x + rDef.width * 0.5f, rDef.y + rDef.height * 0.5f };
        _touchSlotOffset[s] = { _touchCustomPos[2+s].x - defCenter.x,
                                 _touchCustomPos[2+s].y - defCenter.y };
    }
}

int Engine::DrawTouchButtonMapping()
{
    const int sw = GetScreenWidth();
    const int sh = GetScreenHeight();

    DrawWorld();
    DrawHUD();
    DrawRectangle(0, 0, sw, sh, Fade(BLACK, 0.54f));

    const Vector2 inputPos  = GetMousePosition();
    const bool    inputDown = IsMouseButtonDown(MOUSE_LEFT_BUTTON);
    const bool    inputPress= IsMouseButtonPressed(MOUSE_LEFT_BUTTON);

    auto hitButton = [&](Vector2 pos) -> int
    {
        for (int i = 0; i < 2; i++)
            if (Vector2Distance(pos, _touchMappingPos[i]) <= GetTouchMappingRadius(i)) return i;
        for (int i = 2; i < 6; i++)
        {
            float half = _hudCfg.touchSlotSz * 0.5f;
            Rectangle r{ _touchMappingPos[i].x - half, _touchMappingPos[i].y - half,
                         _hudCfg.touchSlotSz, _hudCfg.touchSlotSz };
            if (CheckCollisionPointRec(pos, r)) return i;
        }
        return -1;
    };

    if (inputPress && _touchMappingDragIdx < 0)
    {
        int hit = hitButton(inputPos);
        if (hit >= 0)
        {
            _touchMappingDragIdx   = hit;
            _touchMappingDragStart = inputPos;
            _touchMappingPosAtDrag = _touchMappingPos[hit];
        }
    }

    if (_touchMappingDragIdx >= 0)
    {
        if (inputDown)
        {
            Vector2 delta  = Vector2Subtract(inputPos, _touchMappingDragStart);
            Vector2 newPos = Vector2Add(_touchMappingPosAtDrag, delta);

            for (int i = 0; i < 6; i++)
            {
                if (i == _touchMappingDragIdx) continue;
                float minD = GetTouchMappingRadius(_touchMappingDragIdx) + GetTouchMappingRadius(i);
                Vector2 diff = Vector2Subtract(newPos, _touchMappingPos[i]);
                float dist   = Vector2Length(diff);
                if (dist < minD && dist > 0.001f)
                    newPos = Vector2Add(_touchMappingPos[i],
                                        Vector2Scale(Vector2Normalize(diff), minD));
            }

            float r = GetTouchMappingRadius(_touchMappingDragIdx);
            newPos.x = Clamp(newPos.x, r, (float)sw - r);
            newPos.y = Clamp(newPos.y, r, (float)sh - r);
            _touchMappingPos[_touchMappingDragIdx] = newPos;
        }
        else
        {
            _touchMappingDragIdx = -1;
        }
    }

    // ── ATK button ───────────────────────────────────────────────────────────
    {
        bool drag = (_touchMappingDragIdx == 0);
        DrawCircleV(_touchMappingPos[0], _hudCfg.touchAtkR,
                    drag ? Fade(YELLOW, 0.65f) : Fade(RED, 0.58f));
        DrawCircleLinesV(_touchMappingPos[0], _hudCfg.touchAtkR,
                         drag ? YELLOW : Fade(WHITE, 0.75f));
        const char* lbl = "ATTACK";
        int fs = (int)_hudCfg.touchAtkFs;
        DrawText(lbl,
            (int)(_touchMappingPos[0].x - MeasureText(lbl, fs) * 0.5f),
            (int)(_touchMappingPos[0].y - fs * 0.5f), fs, RAYWHITE);
    }

    // ── DASH button ──────────────────────────────────────────────────────────
    {
        bool drag = (_touchMappingDragIdx == 1);
        DrawCircleV(_touchMappingPos[1], _hudCfg.touchDashR,
                    drag ? Fade(YELLOW, 0.65f) : Fade(SKYBLUE, 0.58f));
        DrawCircleLinesV(_touchMappingPos[1], _hudCfg.touchDashR,
                         drag ? YELLOW : Fade(WHITE, 0.75f));
        const char* lbl = "DASH";
        int fs = (int)_hudCfg.touchDashFs;
        DrawText(lbl,
            (int)(_touchMappingPos[1].x - MeasureText(lbl, fs) * 0.5f),
            (int)(_touchMappingPos[1].y - fs * 0.5f), fs, RAYWHITE);
    }

    // ── Ability slots ─────────────────────────────────────────────────────────
    static const Color kSlotColors[4] = {
        {220,120, 60,255}, {60,160,220,255}, {120,200,80,255}, {180,80,200,255}
    };
    for (int s = 0; s < 4; s++)
    {
        int   idx  = 2 + s;
        bool  drag = (_touchMappingDragIdx == idx);
        float half = _hudCfg.touchSlotSz * 0.5f;
        Rectangle r{ _touchMappingPos[idx].x - half,
                     _touchMappingPos[idx].y - half,
                     _hudCfg.touchSlotSz, _hudCfg.touchSlotSz };
        DrawRectangleRounded(r, 0.18f, 6,
            drag ? Fade(YELLOW, 0.55f) : Fade(kSlotColors[s], 0.58f));
        DrawRectangleRoundedLines(r, 0.18f, 6,
            drag ? YELLOW : Fade(WHITE, 0.55f));
        const char* numLabel = TextFormat("%d", s + 1);
        int nfs = 42;
        DrawText(numLabel,
            (int)(r.x + r.width  * 0.5f - MeasureText(numLabel, nfs) * 0.5f),
            (int)(r.y + r.height * 0.5f - nfs * 0.5f),
            nfs, RAYWHITE);
    }

    // ── Center UI buttons ─────────────────────────────────────────────────────
    const float btnW   = 230.f;
    const float btnH   = 72.f;
    const float bGap   = 18.f;
    const float uiX    = (float)sw * 0.5f - btnW * 0.5f;
    const float totalH = 3.f * btnH + 2.f * bGap;
    const float uiY    = (float)sh * 0.5f - totalH * 0.5f;

    auto drawBtn = [&](const char* label, float y, Color col) -> bool
    {
        Rectangle rec{ uiX, y, btnW, btnH };
        bool hov = CheckCollisionPointRec(inputPos, rec) && _touchMappingDragIdx < 0;
        DrawRectangleRounded(rec, 0.24f, 6, hov ? Fade(col, 0.92f) : Fade(col, 0.72f));
        DrawRectangleRoundedLines(rec, 0.24f, 6, Fade(WHITE, 0.50f));
        int fs = 32;
        DrawText(label,
            (int)(rec.x + rec.width  * 0.5f - MeasureText(label, fs) * 0.5f),
            (int)(rec.y + rec.height * 0.5f - fs * 0.5f),
            fs, WHITE);
        return hov && inputPress;
    };

    int result = 0;
    if (drawBtn("Back",    uiY,                   Color{ 80,  80,  80, 255})) result = 2;
    if (drawBtn("Save",    uiY + btnH + bGap,     Color{ 55, 175,  85, 255})) result = 1;
    if (drawBtn("Default", uiY + (btnH + bGap)*2.f, Color{195, 135,  40, 255})) result = 3;

    const char* hint = "Drag buttons anywhere  |  they won't overlap";
    int hfs = 24;
    DrawText(hint,
        (int)((float)sw * 0.5f - MeasureText(hint, hfs) * 0.5f),
        22, hfs, Fade(WHITE, 0.80f));

    return result;
}

