#include "Engine.h"

#include "AnimationUtils.h"
#include "AssetPaths.h"
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

    bool IsMusicCueFinished(const Music& music)
    {
        return GetMusicTimeLength(music) > 0.f
            && GetMusicTimePlayed(music) >= GetMusicTimeLength(music) - 0.02f;
    }

}

Engine::Engine()
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
    UnloadSound(_pickupSound);
    UnloadSound(_fireballCastSound);
    UnloadSound(_explosionSound);
    UnloadSound(_lavaBallImpactSound);
    UnloadSound(_buttonPressSound);
    UnloadMusicStream(_titleThemeMusic);
    UnloadMusicStream(_pauseThemeMusic);
    UnloadMusicStream(_dungeonThemeMusic);
    UnloadMusicStream(_forestThemeMusic);
    UnloadMusicStream(_bossBattleMusic);
    UnloadMusicStream(_shopThemeMusic);
    UnloadMusicStream(_battleVictoryMusic);
    UnloadMusicStream(_bossVictoryMusic);
    UnloadMusicStream(_gameOverMusic);
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
    _pauseUI.Unload();
    CloseAudioDevice();
    CloseWindow();
}

void Engine::Init()
{
    InitWindow(_windowWidth, _windowHeight, "Mystic Onslaught");
    SetTargetFPS(60);
    InitAudioDevice();

    SetExitKey(KEY_NULL);

    _pickupSound      = LoadSound(AssetPath("Sounds/PickupSound.ogg").c_str());
    _fireballCastSound= LoadSound(AssetPath("Sounds/GS1_Spell_Fire.ogg").c_str());
    _explosionSound   = LoadSound(AssetPath("Sounds/GS1_Spell_Explode.ogg").c_str());
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
    _titleThemeMusic = LoadMusicStream(AssetPath("Music/TitleTheme.ogg").c_str());
    _pauseThemeMusic = LoadMusicStream(AssetPath("Music/PauseMenu.ogg").c_str());
    _dungeonThemeMusic = LoadMusicStream(AssetPath("Music/DungeonTheme.ogg").c_str());
    _forestThemeMusic = LoadMusicStream(AssetPath("Music/ForestTheme.ogg").c_str());
    _bossBattleMusic = LoadMusicStream(AssetPath("Music/BossBattle.ogg").c_str());
    _shopThemeMusic = LoadMusicStream(AssetPath("Music/ZephsShop.ogg").c_str());
    _battleVictoryMusic = LoadMusicStream(AssetPath("Music/BattleVictory.ogg").c_str());
    _bossVictoryMusic = LoadMusicStream(AssetPath("Music/BossVictory.ogg").c_str());
    _gameOverMusic = LoadMusicStream(AssetPath("Music/GameOver.ogg").c_str());

    _titleThemeMusic.looping = true;
    _pauseThemeMusic.looping = true;
    _dungeonThemeMusic.looping = true;
    _forestThemeMusic.looping = true;
    _bossBattleMusic.looping = true;
    _shopThemeMusic.looping = true;
    _battleVictoryMusic.looping = false;
    _bossVictoryMusic.looping = false;
    _gameOverMusic.looping = false;

    SetMusicVolume(_titleThemeMusic, 0.35f);
    SetMusicVolume(_pauseThemeMusic, 0.35f);
    SetMusicVolume(_dungeonThemeMusic, 0.35f);
    SetMusicVolume(_forestThemeMusic, 0.35f);
    SetMusicVolume(_bossBattleMusic, 0.35f);
    SetMusicVolume(_shopThemeMusic, 0.35f);
    SetMusicVolume(_battleVictoryMusic, 0.65f);
    SetMusicVolume(_bossVictoryMusic, 0.65f);
    SetMusicVolume(_gameOverMusic, 0.55f);

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
    _vfx.Init(&_fireballCastTex, &_fireballHitTex, &_genericHitTex,
              &_iceHitTex, &_lightningCastTex, &_healEffectTex);
    _abilityIconFireTex     = LoadTexture(AssetPath("PowerUps/FireBallPickup.png").c_str());
    _abilityIconIceTex      = LoadTexture(AssetPath("PowerUps/IceSpellPickup.png").c_str());
    _abilityIconElectricTex = LoadTexture(AssetPath("PowerUps/LightningPickup.png").c_str());
    _shopBorderTex          = LoadTexture(AssetPath("UI/PauseBoarder.png").c_str());
    _shop.Init(ShopTextures{
        &_shopBorderTex,
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

Music* Engine::GetMusicByCue(MusicCue cue)
{
    switch (cue)
    {
    case MusicCue::Title:         return &_titleThemeMusic;
    case MusicCue::Pause:         return &_pauseThemeMusic;
    case MusicCue::Dungeon:       return &_dungeonThemeMusic;
    case MusicCue::Forest:        return &_forestThemeMusic;
    case MusicCue::BossBattle:    return &_bossBattleMusic;
    case MusicCue::Shop:          return &_shopThemeMusic;
    case MusicCue::BattleVictory: return &_battleVictoryMusic;
    case MusicCue::BossVictory:   return &_bossVictoryMusic;
    case MusicCue::GameOver:      return &_gameOverMusic;
    default:                      return nullptr;
    }
}

bool Engine::IsLoopMusicCue(MusicCue cue) const
{
    return cue == MusicCue::Title
        || cue == MusicCue::Pause
        || cue == MusicCue::Dungeon
        || cue == MusicCue::Forest
        || cue == MusicCue::BossBattle
        || cue == MusicCue::Shop;
}

bool Engine::IsVictoryMusicCue(MusicCue cue) const
{
    return cue == MusicCue::BattleVictory || cue == MusicCue::BossVictory;
}

MusicCue Engine::GetBiomeMusicCue(Biome biome) const
{
    switch (biome)
    {
    case Biome::Forest:
    case Biome::Swamp:
    case Biome::Tundra:
        return MusicCue::Forest;
    default:
        return MusicCue::Dungeon;
    }
}

bool Engine::ShouldHoldSilenceForVictory() const
{
    if (!_silenceAfterVictory)
        return false;

    return _gameState != GameState::Map
        && _gameState != GameState::Menu
        && _gameState != GameState::HowToPlay
        && _gameState != GameState::Keybindings
        && _gameState != GameState::Pause
        && _gameState != GameState::Shop
        && _gameState != GameState::DemoEnd;
}

float Engine::GetMusicBaseVolume(MusicCue cue) const
{
    switch (cue)
    {
    case MusicCue::Title:         return 0.35f;
    case MusicCue::Pause:         return 0.35f;
    case MusicCue::Dungeon:       return 0.35f;
    case MusicCue::Forest:        return 0.35f;
    case MusicCue::BossBattle:    return 0.35f;
    case MusicCue::Shop:          return 0.35f;
    case MusicCue::BattleVictory: return 0.65f;
    case MusicCue::BossVictory:   return 0.65f;
    case MusicCue::GameOver:      return 0.55f;
    default:                      return 0.f;
    }
}

MusicCue Engine::GetDesiredLoopMusicCue() const
{
    if (ShouldHoldSilenceForVictory())
        return MusicCue::None;

    switch (_gameState)
    {
    case GameState::Menu:
    case GameState::DemoEnd:
        return MusicCue::Title;

    case GameState::HowToPlay:
        return (_howToPlayFrom == GameState::Pause) ? MusicCue::Pause : MusicCue::Title;

    case GameState::Keybindings:
    case GameState::Pause:
        return MusicCue::Pause;

    case GameState::Shop:
        return MusicCue::Shop;

    case GameState::Map:
        return GetBiomeMusicCue(GetBiomeForAct(_currentAct));

    case GameState::GameOver:
        return MusicCue::None;

    case GameState::Play:
        if (_currentRoomType == RoomType::Boss && !_roomClearPending && IsBossFightActive())
            return MusicCue::BossBattle;
        return GetBiomeMusicCue(GetBiomeForAct(_currentAct));

    case GameState::LevelUpChoice:
        if (_awaitingStartingAbility)
            return GetBiomeMusicCue(GetBiomeForAct(_currentAct));
        return _silenceAfterVictory ? MusicCue::None : GetBiomeMusicCue(_currentBiome);

    case GameState::AbilityChoice:
    case GameState::ExpTally:
        return _silenceAfterVictory ? MusicCue::None : GetBiomeMusicCue(_currentBiome);

    default:
        return MusicCue::None;
    }
}

void Engine::StopMusicCue(MusicCue cue)
{
    Music* music = GetMusicByCue(cue);
    if (!music || music->ctxData == nullptr)
        return;

    StopMusicStream(*music);
    if (_currentMusicCue == cue)
        _currentMusicCue = MusicCue::None;
}

void Engine::StartMusicCue(MusicCue cue, float startTime)
{
    if (cue == MusicCue::None)
        return;

    if (_currentMusicCue != MusicCue::None && _currentMusicCue != cue)
        StopMusicCue(_currentMusicCue);

    Music* music = GetMusicByCue(cue);
    if (!music || music->ctxData == nullptr)
        return;

    if (IsLoopMusicCue(cue))
        SetMusicVolume(*music, 0.f);
    else
        SetMusicVolume(*music, GetMusicBaseVolume(cue));

    PlayMusicStream(*music);
    if (startTime > 0.f)
        SeekMusicStream(*music, startTime);
    _currentMusicCue = cue;
    if (IsLoopMusicCue(cue))
    {
        _musicFadeInDuration = 2.0f;
        _musicFadeInTimer = _musicFadeInDuration;
    }
    else
        _musicFadeInTimer = 0.f;
}

void Engine::SuspendCurrentLoopForCue(MusicCue cue, bool resumeOnMapOnly)
{
    if (!IsLoopMusicCue(_currentMusicCue))
        return;

    Music* current = GetMusicByCue(_currentMusicCue);
    if (!current || current->ctxData == nullptr)
        return;

    _suspendedMusicCue = _currentMusicCue;
    _suspendedMusicTime = GetMusicTimePlayed(*current);
    _resumeSuspendedMusicOnMapOnly = resumeOnMapOnly;
    PauseMusicStream(*current);
    _currentMusicCue = MusicCue::None;

    StartMusicCue(cue);
}

void Engine::StartVictoryMusic(MusicCue cue)
{
    if (!IsVictoryMusicCue(cue))
        return;

    _activeVictoryCue = cue;

    if (cue == MusicCue::BattleVictory)
    {
        _silenceAfterVictory = false;
        if (IsLoopMusicCue(_currentMusicCue))
            SuspendCurrentLoopForCue(cue, false);
        else
            StartMusicCue(cue);
        return;
    }

    _silenceAfterVictory = true;
    _suspendedMusicCue = MusicCue::None;
    _suspendedMusicTime = 0.f;
    _resumeSuspendedMusicOnMapOnly = false;
    if (_currentMusicCue != MusicCue::None)
        StopMusicCue(_currentMusicCue);
    StartMusicCue(cue);
}

void Engine::ResumePausedLoopMusic()
{
    if (_suspendedMusicCue == MusicCue::None)
        return;

    Music* music = GetMusicByCue(_suspendedMusicCue);
    if (!music || music->ctxData == nullptr)
        return;

    ResumeMusicStream(*music);
    SetMusicVolume(*music, 0.f);
    _currentMusicCue = _suspendedMusicCue;
    _suspendedMusicCue = MusicCue::None;
    _suspendedMusicTime = 0.f;
    _resumeSuspendedMusicOnMapOnly = false;
    _musicFadeInDuration = 1.5f;
    _musicFadeInTimer = _musicFadeInDuration;
}

void Engine::ResetMusicState()
{
    if (_currentMusicCue != MusicCue::None)
        StopMusicCue(_currentMusicCue);

    _suspendedMusicCue = MusicCue::None;
    _suspendedMusicTime = 0.f;
    _activeVictoryCue = MusicCue::None;
    _musicFadeInTimer = 0.f;
    _musicFadeInDuration = 2.0f;
    _resumeSuspendedMusicOnMapOnly = false;
    _silenceAfterVictory = false;
    _demoEndTouchHeld = false;
    _gameOverMusicPlayed = false;
}

void Engine::UpdateMusicSystem()
{
    if (_gameState != GameState::GameOver)
        _gameOverMusicPlayed = false;

    if (_gameState == GameState::GameOver && _currentMusicCue == MusicCue::None && !_gameOverMusicPlayed)
    {
        _gameOverMusicPlayed = true;
        StartMusicCue(MusicCue::GameOver);
        return;
    }

    if (_currentMusicCue != MusicCue::None)
    {
        Music* current = GetMusicByCue(_currentMusicCue);
        if (current && current->ctxData != nullptr)
        {
            UpdateMusicStream(*current);
            if (IsLoopMusicCue(_currentMusicCue))
            {
                float baseVolume = GetMusicBaseVolume(_currentMusicCue);
                if (_musicFadeInTimer > 0.f)
                {
                    _musicFadeInTimer = std::max(0.f, _musicFadeInTimer - GetFrameTime());
                    float t = 1.f - (_musicFadeInTimer / _musicFadeInDuration);
                    SetMusicVolume(*current, baseVolume * std::max(0.f, std::min(1.f, t)));
                }
                else
                {
                    SetMusicVolume(*current, baseVolume);
                }
            }
        }
    }

    if (_currentMusicCue == MusicCue::GameOver)
    {
        Music* current = GetMusicByCue(_currentMusicCue);
        if (current && current->ctxData != nullptr && IsMusicStreamPlaying(*current))
            return;

        if (current && current->ctxData != nullptr)
        {
            StopMusicStream(*current);
            _currentMusicCue = MusicCue::None;
        }
        return;
    }

    if (IsVictoryMusicCue(_currentMusicCue))
    {
        Music* current = GetMusicByCue(_currentMusicCue);
        if (current && current->ctxData != nullptr && IsMusicStreamPlaying(*current))
            return;

        if (current && current->ctxData != nullptr)
        {
            StopMusicStream(*current);
            _currentMusicCue = MusicCue::None;
        }

        if (_activeVictoryCue == MusicCue::BattleVictory)
        {
            _activeVictoryCue = MusicCue::None;
            ResumePausedLoopMusic();
            return;
        }

        _activeVictoryCue = MusicCue::None;
        return;
    }

    MusicCue desiredCue = GetDesiredLoopMusicCue();
    if (ShouldHoldSilenceForVictory())
        desiredCue = MusicCue::None;

    if (_silenceAfterVictory && !ShouldHoldSilenceForVictory())
        _silenceAfterVictory = false;

    if (desiredCue == MusicCue::None)
    {
        if (IsLoopMusicCue(_currentMusicCue))
            StopMusicCue(_currentMusicCue);
        return;
    }

    if (desiredCue == _currentMusicCue)
        return;

    if (_suspendedMusicCue == desiredCue && !_resumeSuspendedMusicOnMapOnly)
    {
        ResumePausedLoopMusic();
        return;
    }

    if (_suspendedMusicCue == desiredCue && _resumeSuspendedMusicOnMapOnly && _gameState == GameState::Map)
    {
        ResumePausedLoopMusic();
        return;
    }

    if ((desiredCue == MusicCue::Pause || desiredCue == MusicCue::Shop)
        && IsLoopMusicCue(_currentMusicCue)
        && _currentMusicCue != desiredCue)
    {
        SuspendCurrentLoopForCue(desiredCue, false);
        return;
    }

    if (IsLoopMusicCue(_currentMusicCue) && _currentMusicCue != desiredCue)
        StopMusicCue(_currentMusicCue);

    StartMusicCue(desiredCue);
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
    _eliteHazards.clear();
    _eliteHazardSpawnTimer   = 0.f;

    // Store room — place Zeph at map centre and stock the shop
    if (type == RoomType::Store)
    {
        float mapW = _map.width  * _mapScale;
        float mapH = _map.height * _mapScale;
        _shop.Enter({ mapW * 0.5f, mapH * 0.5f }, _player, _currentAct);
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
    _levelUpReturnState = GameState::Play;
    _message = "Debug Arena";
    DebugRestartRoomAs(RoomType::Standard);
}

void Engine::DebugSetEliteMechanic(int mechanic)
{
    _debug.SetForcedEliteMechanic(mechanic);
    DebugRestartRoomAs(RoomType::Elite);
}

void Engine::DebugRestartRoomAs(RoomType type)
{
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
    _eliteHazards.clear();
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
            const float navBot  = (_currentBiome == Biome::Forest || _currentBiome == Biome::Swamp) ? 160.f : 220.f;
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
        _shop.Enter({ mapW * 0.5f, mapH * 0.5f }, _player, _currentAct);
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
    return isDungeon ? Biome::Dungeon : Biome::Forest;
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
                const float navBot  = (_currentBiome == Biome::Forest || _currentBiome == Biome::Swamp) ? 160.f : 220.f;
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
                Vector2 pos{ mapW * 0.5f, mapH * 0.5f };
                for (int a = 0; a < 30; a++)
                {
                    Vector2 candidate{
                        (float)GetRandomValue(300, (int)mapW - 300),
                        (float)GetRandomValue(300, (int)mapH - 300)
                    };
                    if (IsSpawnPositionValid(candidate)) { pos = candidate; break; }
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
    // Enemies always spawn from the four map corners so the player can never
    // pre-stack them by standing in one spot. Cycling ensures distribution.
    int cornerIdx = 0;
    auto spawnPos = [&]() -> Vector2 {
        const float m = 400.f;
        const Vector2 corners[4] = {
            { m,        m        },   // top-left
            { mapW - m, m        },   // top-right
            { m,        mapH - m },   // bottom-left
            { mapW - m, mapH - m },   // bottom-right
        };
        Vector2 base = corners[cornerIdx % 4];
        cornerIdx++;

        // Nudge away from a prop if the corner happens to overlap one
        if (IsSpawnPositionValid(base)) return base;
        for (int a = 0; a < 10; a++)
        {
            Vector2 p{
                base.x + (float)GetRandomValue(-120, 120),
                base.y + (float)GetRandomValue(-120, 120)
            };
            if (IsSpawnPositionValid(p)) return p;
        }
        return base;
    };

    if (_currentRoomType == RoomType::Elite)
    {
        const int fodderCount = std::min(7, 3 + _currentAct + std::max(0, _currentRoom - 2));
        const int eliteTypeRoll = GetRandomValue(0, 2);
        Enemy* eliteMiniboss = nullptr;
        switch (eliteTypeRoll)
        {
        case 0: eliteMiniboss = SpawnBasicEnemy(spawnPos()); break;
        case 1: eliteMiniboss = SpawnCyclops(spawnPos()); break;
        case 2: eliteMiniboss = SpawnOgre(spawnPos()); break;
        }

        if (eliteMiniboss != nullptr)
            eliteMiniboss->SetIsEliteMiniboss(true);

        // ── Elite-room systems setup ──────────────────────────────────────
        {
            _eliteMechanic    = (_debug.GetForcedEliteMechanic() >= 0) ? _debug.GetForcedEliteMechanic() : GetRandomValue(0, 4);
            _eliteMinibossPtr = eliteMiniboss;

            const float mapW = _map.width  * _mapScale;
            const float mapH = _map.height * _mapScale;

            // Reset everything; only activate the chosen mechanic
            _eliteCageRadius         = 0.f;
            _eliteCageDamageTimer    = 0.f;
            _eliteEnrageWarningTimer = 0.f;
            _eliteIsLeaping          = false;
            _eliteLeapCooldown       = 0.f;
            _eliteLeapTimer          = 0.f;
            _eliteHazards.clear();
            _eliteHazardSpawnTimer   = 0.f;

            switch (_eliteMechanic)
            {
            case 0: // Arena Constriction
                _eliteCageCenter      = { mapW * 0.5f, mapH * 0.5f };
                _eliteCageRadius      = kEliteCageRadius;
                _eliteCageDamageTimer = kEliteCageDamageInterval;
                break;

            case 1: // Invulnerability Links
                if (_eliteMinibossPtr)
                    _eliteMinibossPtr->SetInvulnerable(true);
                break;

            case 2: // Permanent Enrage
                if (_eliteMinibossPtr)
                    _eliteMinibossPtr->ApplyEnrage();
                _eliteEnrageWarningTimer = kEliteEnrageWarningDuration;
                break;

            case 3: // Gap-Closer Leap
                _eliteLeapCooldown = kLeapInterval;
                break;

            case 4: // Room Hazards
                _eliteHazardSpawnTimer = (float)GetRandomValue(
                    (int)(kHazardVolleyMinInterval * 100.f),
                    (int)(kHazardVolleyMaxInterval * 100.f)) / 100.f;
                break;
            }
        }

        for (int i = 0; i < fodderCount; i++)
        {
            SpawnBasicEnemy(spawnPos());
        }
        return;
    }

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

    for (int i = 0; i < regularCount; i++)
        SpawnBasicEnemy(spawnPos());

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
            _touchModeActive = _menu.IsTouchMode();
            _debug.Deactivate();
            ResetRunState();
            _fadeInTimer = 2.0f; _fadeInDuration = 2.0f;
            GenerateStartingAbilityOptions();
            _awaitingStartingAbility = true;
            _levelUpReturnState = GameState::Map;  // after picking, show the act map
            _levelUpOpenTimer = 0.8f;
            _gameState = GameState::LevelUpChoice;
        }
        if (_menu.DebugPressed() && _demoCompleted)
        {
            _touchModeActive = _menu.IsTouchMode();
            DebugStartRun();
        }
        if (_menu.QuitPressed())
            _shouldClose = true;
        if (_menu.HowToPressed())
        {
            _howToPlayFrom  = GameState::Menu;
            _htpTab         = _touchModeActive ? 3 : 0;
            _htpSlideOffset = 0.f;
            _gameState      = GameState::HowToPlay;
        }
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

    case GameState::ExpTally:
        UpdateExpTally(dt);
        break;

    case GameState::Shop:
        if (_shop.Update(_player)) _gameState = GameState::Play;
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
    if (_debug.IsActive())
    {
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

    if (IsKeyPressed(KEY_ESCAPE))
    {
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

        // ── Elite-room systems tick ───────────────────────────────────────
        if (_currentRoomType == RoomType::Elite)
        {
            // 0. Arena cage — tick damage when player is outside the ring
            if (_eliteMechanic == 0 && _eliteCageRadius > 0.f)
            {
                float dist = Vector2Distance(_player.GetWorldPos(), _eliteCageCenter);
                if (dist > _eliteCageRadius)
                {
                    _eliteCageDamageTimer -= dt;
                    if (_eliteCageDamageTimer <= 0.f)
                    {
                        _player.TakeDamage(1, _eliteCageCenter);
                        _eliteCageDamageTimer = kEliteCageDamageInterval;
                    }
                }
                else
                {
                    _eliteCageDamageTimer = kEliteCageDamageInterval;
                }
            }

            // 1. Bodyguard shield — remove invulnerability once all grunts are gone
            if (_eliteMechanic == 1
                && _eliteMinibossPtr && _eliteMinibossPtr->IsInvulnerable()
                && _eliteMinibossPtr->IsActive() && !_eliteMinibossPtr->IsDying())
            {
                bool anyGruntAlive = false;
                for (const auto& e : _enemies)
                {
                    if (e.get() == _eliteMinibossPtr) continue;
                    if (e->IsActive() && e->IsAlive() && !e->IsDying())
                    { anyGruntAlive = true; break; }
                }
                if (!anyGruntAlive)
                    _eliteMinibossPtr->SetInvulnerable(false);
            }

            // 2. Enrage warning banner timer
            if (_eliteMechanic == 2 && _eliteEnrageWarningTimer > 0.f)
                _eliteEnrageWarningTimer -= dt;

            // 3. Gap-closer leap
            if (_eliteMechanic == 3
                && _eliteMinibossPtr && _eliteMinibossPtr->IsActive()
                && _eliteMinibossPtr->IsAlive() && !_eliteMinibossPtr->IsDying())
            {
                if (!_eliteIsLeaping)
                {
                    _eliteLeapCooldown -= dt;
                    if (_eliteLeapCooldown <= 0.f)
                    {
                        _eliteLeapStartPos = _eliteMinibossPtr->GetWorldPos();
                        _eliteLeapTarget   = _player.GetFeetWorldPos();
                        _eliteIsLeaping    = true;
                        _eliteLeapTimer    = kLeapDuration;
                        _eliteMinibossPtr->SetLeapFrozen(true);
                    }
                }
                else
                {
                    // Freeze the boss in place during the wind-up
                    _eliteMinibossPtr->Teleport(_eliteLeapStartPos);
                    _eliteLeapTimer -= dt;

                    if (_eliteLeapTimer <= 0.f)
                    {
                        _eliteMinibossPtr->Teleport(_eliteLeapTarget);
                        _eliteMinibossPtr->SetLeapFrozen(false);
                        _eliteIsLeaping    = false;
                        _eliteLeapCooldown = kLeapInterval;

                        float dist = Vector2Distance(_player.GetWorldPos(), _eliteLeapTarget);
                        if (dist <= kLeapAoERadius)
                            _player.TakeDamage(kLeapAoEDamage, _eliteLeapTarget);

                        TriggerScreenShake(8.f, 0.25f);
                    }
                }
            }

            // 4. Room hazards — random lightning strikes near the player
            if (_eliteMechanic == 4)
            {
                _eliteHazardSpawnTimer -= dt;
                if (_eliteHazardSpawnTimer <= 0.f)
                {
                    const float mapW = _map.width * _mapScale;
                    const float mapH = _map.height * _mapScale;
                    const float marginLeft = 120.f;
                    const float marginRight = 120.f;
                    const float marginTop = 90.f;
                    const float marginBottom = 220.f;
                    const Vector2 playerPos = _player.GetWorldPos();
                    const int volleyCount = GetRandomValue(kHazardVolleyMinCount, kHazardVolleyMaxCount);

                    for (int i = 0; i < volleyCount; ++i)
                    {
                        Vector2 spawnPos{};
                        bool foundSpawn = false;

                        for (int attempt = 0; attempt < 24; ++attempt)
                        {
                            spawnPos = {
                                (float)GetRandomValue((int)marginLeft, (int)(mapW - marginRight)),
                                (float)GetRandomValue((int)marginTop,  (int)(mapH - marginBottom))
                            };

                            if (Vector2Distance(spawnPos, playerPos) < 240.f)
                                continue;
                            if (!IsSpawnPositionValid(spawnPos))
                                continue;

                            foundSpawn = true;
                            break;
                        }

                        if (!foundSpawn)
                            continue;

                        Vector2 toPlayer = Vector2Subtract(playerPos, spawnPos);
                        float baseAngle = atan2f(toPlayer.y, toPlayer.x);
                        float spread = ((float)GetRandomValue(-28, 28)) * DEG2RAD;
                        float angle = baseAngle + spread;

                        LavaBallProjectile projectile;
                        projectile.Init(spawnPos, Vector2{ cosf(angle), sinf(angle) });
                        _lavaBalls.push_back(projectile);
                    }

                    _eliteHazardSpawnTimer = (float)GetRandomValue(
                        (int)(kHazardVolleyMinInterval * 100.f),
                        (int)(kHazardVolleyMaxInterval * 100.f)) / 100.f;
                    TriggerScreenShake(2.f, 0.06f);
                }
            }
        }

        // Non-combat rooms still use the same Continue button flow as combat
        // rooms; they just complete immediately once their intro ends.
        if (_currentRoomType == RoomType::Rest || _currentRoomType == RoomType::Store)
        {
            _roomClearPending = true;
        }

        // ── Store room — Zeph NPC logic ───────────────────────────────────
        if (_currentRoomType == RoomType::Store)
        {
            if (_shop.UpdateNpc(_player)) _gameState = GameState::Shop;
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
                navigationTarget = _nav.GetAStarTarget(enemy->GetWorldPos(), playerFeet);
                hasNavigationTarget = !Vector2Equals(navigationTarget, playerFeet);
            }
            else if (!enemy->UsesDirectPursuit() &&
                !_nav.HasLineOfSight(enemy->GetWorldPos(), playerFeet))
            {
                navigationTarget = _nav.GetTarget(enemy->GetWorldPos(), playerFeet);
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
                    if (cyclops->GetFireMode() == Cyclops::FireMode::Scatter)
                        laser.InitScatter(cyclops->GetWorldPos(), cyclops->GetFireDirection(), cyclops->GetAttackPower());
                    else
                        laser.InitSweep(cyclops->GetWorldPos(), cyclops->GetFireDirection(), cyclops->GetAttackPower());
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
        _vfx.Update(dt);
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

    case GameState::Play:
    {
        DrawWorld();
        DrawUltimateSequence();
        DrawHUD();
        if (_debug.IsActive())
            _debug.Draw(_currentAct, _currentRoom, GetDebugRoomTypeName(_currentRoomType));

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

              if (_currentRoomType == RoomType::Boss && _bossesDefeated < 2)
              {
                  std::string nextBiomeText = std::string("Next Biome: ") + GetBiomeName(GetBiomeForAct(_currentAct + 1));
                  int infoSz = 24;
                  DrawText(nextBiomeText.c_str(),
                      (int)(sw * 0.5f - MeasureText(nextBiomeText.c_str(), infoSz) * 0.5f),
                      (int)(btnY - 42.f),
                      infoSz,
                      Color{255, 214, 102, 240});
              }

              bool pressed = hov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
            if (!pressed)
            {
                int tc = GetTouchPointCount();
                for (int i = 0; i < tc; ++i)
                {
                    if (CheckCollisionPointRec(GetTouchPosition(i), btn))
                    {
                        pressed = true;
                        break;
                    }
                }
            }

            if (pressed)
            {
                StopSound(_buttonPressSound);
                PlaySound(_buttonPressSound);
                HandleRoomContinueAction();
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

        DrawCyclopsLasers(worldOffset);

        _vfx.Draw(worldOffset, _player.GetWorldPos(), _player.GetCastOrigin());

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
            _howToPlayFrom  = GameState::Pause;
            _htpTab         = _touchModeActive ? 3 : 0;
            _htpSlideOffset = 0.f;
            _gameState      = GameState::HowToPlay;
        }
        else if (pauseResult == 3)
            _shouldClose = true;
        else if (pauseResult == 4)
        {
            _keybindingsEdit = _player.GetBindings();
            _gameState = GameState::Keybindings;
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

    case GameState::ExpTally:
    {
        DrawWorld();
        DrawHUD();
        DrawExpTally();
        break;
    }

    case GameState::Map:
    {
        DrawMap();
        break;
    }

    case GameState::Shop:
    {
        _shop.Draw(_player);
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
    const float marginBottom = ((_currentBiome == Biome::Forest || _currentBiome == Biome::Swamp)) ? 160.f : 220.f;

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
            // EXP is banked into _pendingExp and drained on the post-battle tally screen.
            bool isBoss = (dynamic_cast<Molarbeast*>(enemy.get()) != nullptr);
            bool isOgre = (dynamic_cast<Ogre*>(enemy.get()) != nullptr);

            if (isBoss)
            {
                _pendingExp += 10.f * _wave;
                _bossesDefeated++;
                if (_bossesDefeated >= 2)
                    _demoCompleted = true;
            }
            else if (!IsBossFightActive())
                _pendingExp += (float)enemy->GetExpValue();
            // else: support add during active boss fight — no exp

            _enemiesKilled++;
            SpawnEnemyDrop(dropPos, isOgre, isBoss);
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
    static constexpr float kNewBarW   = 400.f;
    static constexpr float kNewBarH   = 28.f;
    static constexpr float kNewBarGap = 8.f;
    static constexpr float kNewBotPad = 12.f;
    static constexpr float kNewTopPad = 18.f;

    const float hpBarY   = kNewTopPad;
    const float manaBarY = hpBarY + kNewBarH + kNewBarGap;
    const float slotY    = (float)GetScreenHeight() - kNewBotPad - 80.f;
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

    drawLabelBox(("Gold: " + std::to_string(_player.GetGold())).c_str(), 20.f, 16.f, 28, GOLD);
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
        Color fillColor = (hpPct > 0.70f) ? GREEN : (hpPct > 0.30f) ? YELLOW : RED;

        if (hpPct <= 0.30f)
        {
            float pulse      = (sinf((float)GetTime() * (2.f * PI / 3.f)) + 1.f) * 0.5f;
            float exps[]     = { 4.f, 9.f, 15.f };
            float alphas[]   = { 0.45f, 0.28f, 0.13f };
            for (int i = 0; i < 3; i++)
            {
                float e = exps[i];
                DrawRectangleRounded(
                    { barX - e, hpBarY - e, kNewBarW + e * 2.f, kNewBarH + e * 2.f },
                    0.35f, 6, Fade(RED, alphas[i] * pulse));
            }
        }

        DrawRectangleRounded({ barX, hpBarY, kNewBarW * hpPct, kNewBarH }, 0.3f, 6, fillColor);
        DrawRectangleRoundedLines({ barX, hpBarY, kNewBarW, kNewBarH }, 0.3f, 6, Fade(WHITE, 0.25f));

        const char* hpLabel = TextFormat("HP  %.0f / %.0f", curHp, maxHp);
        int labelW = MeasureText(hpLabel, 18);
        DrawText(hpLabel,
            (int)(barX + kNewBarW / 2.f - labelW / 2.f),
            (int)(hpBarY + kNewBarH / 2.f - 9.f),
            18, BLACK);
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
            18, BLACK);
    }

    // Elite mechanic label (persistent, top-right under room label)
    if (_currentRoomType == RoomType::Elite && _eliteMechanic >= 0)
    {
        static constexpr const char* kMechanicNames[] = {
            "ARENA CONSTRICTION",
            "INVULNERABILITY LINKS",
            "PERMANENT ENRAGE",
            "GAP-CLOSER LEAP",
            "ROOM HAZARDS"
        };
        static constexpr Color kMechanicColors[] = {
            Color{220, 40, 200, 255},   // 0 cage — magenta
            Color{180, 100, 255, 255},  // 1 bodyguard — purple
            Color{255, 60,  60, 255},   // 2 enrage — red
            Color{255,180,  60, 255},   // 3 leap — orange
            Color{255,220,  80, 255},   // 4 hazards — yellow
        };
        const char* mechLabel = kMechanicNames[_eliteMechanic];
        Color mechColor = kMechanicColors[_eliteMechanic];
        int mw = MeasureText(mechLabel, 20);
        drawLabelBox(mechLabel,
            (float)(GetScreenWidth() - mw - 130), 58.f, 20, mechColor);
    }

    // Elite enrage warning banner
    if (_currentRoomType == RoomType::Elite && _eliteEnrageWarningTimer > 0.f)
    {
        const float sw = (float)GetScreenWidth();
        const float sh = (float)GetScreenHeight();

        // Fade in over first 0.5s, hold, fade out over last 0.5s
        float alpha = 1.f;
        if (_eliteEnrageWarningTimer > kEliteEnrageWarningDuration - 0.5f)
            alpha = (kEliteEnrageWarningDuration - _eliteEnrageWarningTimer) / 0.5f;
        else if (_eliteEnrageWarningTimer < 0.5f)
            alpha = _eliteEnrageWarningTimer / 0.5f;
        alpha = std::max(0.f, std::min(1.f, alpha));

        const char* line1 = "ELITE ENCOUNTER";
        const char* line2 = "CONDITION: ENRAGED  |  FAST & LETHAL";

        const int sz1 = 48;
        const int sz2 = 28;
        const float bannerH = 120.f;
        const float bannerY = sh * 0.38f;

        // Dark semi-transparent background
        DrawRectangle(0, (int)bannerY, (int)sw, (int)bannerH, Fade(Color{20,0,0,220}, alpha));
        DrawRectangle(0, (int)bannerY, (int)sw, 3, Fade(Color{200,0,0,255}, alpha));
        DrawRectangle(0, (int)(bannerY + bannerH - 3), (int)sw, 3, Fade(Color{200,0,0,255}, alpha));

        int w1 = MeasureText(line1, sz1);
        int w2 = MeasureText(line2, sz2);

        DrawText(line1, (int)(sw / 2.f - w1 / 2.f), (int)(bannerY + 14.f), sz1,
                 Fade(Color{255, 60, 60, 255}, alpha));
        DrawText(line2, (int)(sw / 2.f - w2 / 2.f), (int)(bannerY + 14.f + sz1 + 8.f), sz2,
                 Fade(Color{255, 180, 60, 255}, alpha));
    }

    if (_debug.IsActive() && !_touchModeActive)
    {
        const char* debugHint = "Press F1 for Debug Menu";
        const int hintSz = 16;
        const int hintW = MeasureText(debugHint, hintSz);
        const float boxW = (float)hintW + 24.f;
        const float boxH = 34.f;
        const float boxX = (float)GetScreenWidth() - boxW - 14.f;
        const float boxY = 14.f;

        DrawRectangleRounded({ boxX, boxY, boxW, boxH }, 0.22f, 6, Fade(BLACK, 0.58f));
        DrawRectangleRoundedLines({ boxX, boxY, boxW, boxH }, 0.22f, 6, Fade(Color{ 255, 214, 150, 255 }, 0.45f));
        DrawText(debugHint,
            (int)(boxX + 12.f),
            (int)(boxY + boxH * 0.5f - hintSz * 0.5f),
            hintSz,
            Color{ 255, 235, 210, 255 });
    }

    if (_touchModeActive)
    {
        DrawTouchAbilityArc();
        _touch.Draw(_windowWidth, _windowHeight);
        if (_debug.IsActive())
        {
            const Rectangle debugTab{ (float)_windowWidth - 54.f, (float)_windowHeight * 0.43f, 42.f, 132.f };
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
    const float sw = (float)GetScreenWidth();
    const float sh = (float)GetScreenHeight();

    // Dark gradient background
    DrawRectangleGradientV(0, 0, (int)sw, (int)sh,
        Color{8, 4, 20, 255}, Color{20, 10, 40, 255});

    // Decorative top and bottom bars
    DrawRectangle(0, 0,           (int)sw, 6,  Color{120, 60, 200, 200});
    DrawRectangle(0, (int)sh - 6, (int)sw, 6,  Color{120, 60, 200, 200});

    float cy = sh * 0.12f;

    // Title
    const char* title = "THANKS FOR PLAYING!";
    int titleSz = 64;
    DrawText(title, (int)(sw * 0.5f - MeasureText(title, titleSz) * 0.5f),
             (int)cy, titleSz, Color{220, 180, 255, 255});
    cy += titleSz + 18.f;

    // Game name
    const char* gameName = "Mystic Onslaught  -  DEMO";
    int nameSz = 30;
    DrawText(gameName, (int)(sw * 0.5f - MeasureText(gameName, nameSz) * 0.5f),
             (int)cy, nameSz, Color{160, 130, 210, 220});
    cy += nameSz + 50.f;

    // Divider
    DrawLineEx({ sw * 0.25f, cy }, { sw * 0.75f, cy }, 1.f, Color{120, 60, 200, 120});
    cy += 30.f;

    // Thank-you message
    const char* lines[] = {
        "You've completed the demo — thank you for your time!",
        "The full game is currently in development.",
        "Your feedback means the world.",
    };
    int msgSz = 26;
    for (auto& line : lines)
    {
        DrawText(line, (int)(sw * 0.5f - MeasureText(line, msgSz) * 0.5f),
                 (int)cy, msgSz, Color{200, 200, 220, 220});
        cy += msgSz + 14.f;
    }
    cy += 30.f;

    // Run stats
    const char* stats[] = {
        TextFormat("Enemies defeated:  %d",  _enemiesKilled),
        TextFormat("Bosses slain:      %d",  _bossesDefeated),
        TextFormat("Gold collected:    %dg", _player.GetGold()),
        TextFormat("Player level:      %d",  _player.GetLevel()),
    };
    int statSz = 22;
    for (auto& s : stats)
    {
        DrawText(s, (int)(sw * 0.5f - MeasureText(s, statSz) * 0.5f),
                 (int)cy, statSz, Color{180, 220, 180, 210});
        cy += statSz + 10.f;
    }
    cy += 40.f;

    // Debug unlock hint (only shown when unlocked via beating the game)
    if (_demoCompleted)
    {
        const char* hint = "Debug Mode unlocked - use the Debug button on the main menu, then press F1 or F10 in-game";
        int hintSz = 16;
        DrawText(hint, (int)(sw * 0.5f - MeasureText(hint, hintSz) * 0.5f),
                 (int)cy, hintSz, Color{160, 120, 255, 180});
        cy += hintSz + 30.f;
    }

    const char* returnHint = _touchModeActive
        ? "Tap anywhere to return to the main menu"
        : "Press any key or click anywhere to return to the main menu";
    int returnHintSz = 18;
    DrawText(returnHint,
        (int)(sw * 0.5f - MeasureText(returnHint, returnHintSz) * 0.5f),
        (int)cy, returnHintSz, Color{210, 200, 235, 190});
    cy += returnHintSz + 24.f;

    // Return to menu button
    Vector2 mouse = GetMousePosition();
    Rectangle btn = { sw * 0.5f - 160.f, sh * 0.88f, 320.f, 54.f };
    bool hov = CheckCollisionPointRec(mouse, btn);
    DrawRectangleRounded(btn, 0.4f, 8,
        hov ? Color{70, 30, 130, 240} : Color{40, 16, 80, 200});
    DrawRectangleRoundedLines(btn, 0.4f, 8,
        hov ? Color{180, 100, 255, 255} : Color{120, 60, 200, 180});
    const char* btnTxt = "Return to Main Menu";
    int btnSz = 24;
    DrawText(btnTxt,
        (int)(btn.x + btn.width  * 0.5f - MeasureText(btnTxt, btnSz) * 0.5f),
        (int)(btn.y + btn.height * 0.5f - btnSz * 0.5f),
        btnSz, hov ? Color{220, 180, 255, 255} : RAYWHITE);

    if (hov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
    {
        ResetRunState();
        _menu.Init();
        _gameState = GameState::Menu;
    }
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
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Fade(BLACK, 0.65f));

    const float sw = (float)GetScreenWidth();
    const float sh = (float)GetScreenHeight();
    const float cx = sw * 0.5f;
    const int levelsGained = std::max(0, _player.GetLevel() - _tallyStartLevel);

    // Title
    const char* title = (levelsGained > 0) ? "Level Up!" : "Room Cleared!";
    static constexpr int kTitleSize = 52;
    int titleW = MeasureText(title, kTitleSize);
    DrawText(title, (int)(cx - titleW * 0.5f), (int)(sh * 0.28f), kTitleSize, RAYWHITE);

    // Level display
    int level    = _player.GetLevel();
    int maxLevel = _player.GetMaxLevel();
    const char* levelStr = TextFormat("Level  %d", level);
    static constexpr int kLevelSize = 38;
    int levelW = MeasureText(levelStr, kLevelSize);
    DrawText(levelStr, (int)(cx - levelW * 0.5f), (int)(sh * 0.41f), kLevelSize,
        Color{ 255, 210, 0, 255 });

    if (levelsGained > 0)
    {
        auto prevInt = [levelsGained](int currentValue, int perLevelGain) -> int
        {
            return currentValue - perLevelGain * levelsGained;
        };
        auto prevFloat = [levelsGained](float currentValue, float perLevelGain) -> float
        {
            return currentValue - perLevelGain * (float)levelsGained;
        };

        std::string hpLine = "HP  " + std::to_string(prevInt((int)_player.GetMaxHealthValue(), Character::kLevelHpGain))
            + " -> " + std::to_string((int)_player.GetMaxHealthValue());
        std::string atkLine = "ATK  " + std::to_string((int)std::ceil(prevFloat(_player.GetAttackPowerValue(), Character::kLevelAttackGain)))
            + " -> " + std::to_string((int)std::ceil(_player.GetAttackPowerValue()));
        std::string defLine = "DEF  " + std::to_string((int)std::ceil(prevFloat(_player.GetDefense(), Character::kLevelDefenseGain) * 100.f))
            + "% -> " + std::to_string((int)std::ceil(_player.GetDefense() * 100.f)) + "%";
        std::string manaLine = "MP  " + std::to_string(prevInt(_player.GetMaxMana(), Character::kLevelManaGain))
            + " -> " + std::to_string(_player.GetMaxMana());

        static constexpr int kGainSize = 24;
        float gainY = sh * 0.465f;
        DrawText(hpLine.c_str(), (int)(cx - MeasureText(hpLine.c_str(), kGainSize) * 0.5f), (int)gainY, kGainSize, Color{190, 255, 190, 255});
        DrawText(atkLine.c_str(), (int)(cx - MeasureText(atkLine.c_str(), kGainSize) * 0.5f), (int)(gainY + 30.f), kGainSize, Color{255, 210, 160, 255});
        DrawText(defLine.c_str(), (int)(cx - MeasureText(defLine.c_str(), kGainSize) * 0.5f), (int)(gainY + 60.f), kGainSize, Color{180, 220, 255, 255});
        DrawText(manaLine.c_str(), (int)(cx - MeasureText(manaLine.c_str(), kGainSize) * 0.5f), (int)(gainY + 90.f), kGainSize, Color{165, 195, 255, 255});
    }

    // EXP bar
    static const Color kExpFill = { 255, 210, 0, 230 };
    static constexpr float kBarW = 520.f;
    static constexpr float kBarH = 38.f;
    const float barX = cx - kBarW * 0.5f;
    const float barY = (levelsGained > 0) ? sh * 0.64f : sh * 0.51f;

    int curExp    = _player.GetExp();
    int expToNext = _player.GetExpToNext();
    float expPct  = (level < maxLevel && expToNext > 0)
        ? (float)curExp / (float)expToNext : 1.f;

    DrawRectangleRounded({ barX, barY, kBarW, kBarH }, 0.3f, 6, Fade(BLACK, 0.75f));
    if (level < maxLevel)
        DrawRectangleRounded({ barX, barY, kBarW * expPct, kBarH }, 0.3f, 6, kExpFill);
    DrawRectangleRoundedLines({ barX, barY, kBarW, kBarH }, 0.3f, 6, Fade(WHITE, 0.30f));

    const char* expLabel = (level < maxLevel)
        ? TextFormat("%d / %d  EXP", curExp, expToNext)
        : "MAX LEVEL";
    int expLabelW = MeasureText(expLabel, 20);
    DrawText(expLabel,
        (int)(cx - expLabelW * 0.5f),
        (int)(barY + kBarH * 0.5f - 10.f),
        20, RAYWHITE);

    // Pending EXP still to arrive
    if (_pendingExp > 0.f)
    {
        const char* pendingStr = TextFormat("+%d EXP incoming", (int)_pendingExp);
        int pendingW = MeasureText(pendingStr, 26);
        DrawText(pendingStr,
            (int)(cx - pendingW * 0.5f),
            (int)(barY + kBarH + 14.f),
            26, Fade(kExpFill, 0.85f));
    }

    // Dismiss / skip hint
    float pulse = 0.60f + 0.40f * sinf((float)GetTime() * 4.f);
    if (_expTallyDone)
    {
        const char* hint;
        if (_tallyLevelUpsRemaining > 0 && !_tallyChoiceChaining)
        {
            // Level-up(s) waiting — tell the player something exciting is coming.
            hint = _touchModeActive ? "Tap to choose an upgrade!" : "Space / Enter  —  Choose an Upgrade!";
        }
        else
        {
            hint = _touchModeActive ? "Tap to Continue" : "Space / Enter  to Continue";
        }
        int hintW = MeasureText(hint, 26);
        DrawText(hint, (int)(cx - hintW * 0.5f), (int)(sh * 0.70f), 26, Fade(RAYWHITE, pulse));
    }
    else if (!_touchModeActive)
    {
        const char* skipHint = "Space / Enter  to skip";
        int skipW = MeasureText(skipHint, 20);
        DrawText(skipHint, (int)(cx - skipW * 0.5f), (int)(sh * 0.70f), 20, Fade(RAYWHITE, 0.45f));
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
    const float mapCentreX = sw * 0.53f;
    std::string header = "Act " + std::to_string(_currentAct)
                       + "  -  " + GetBiomeName(actBiome);
    int hSz = 42;
    DrawText(header.c_str(),
        (int)(mapCentreX - MeasureText(header.c_str(), hSz) / 2.f), 28, hSz, Color{255, 214, 102, 255});
    const char* sub = "Choose your path";
    DrawText(sub,
        (int)(mapCentreX - MeasureText(sub, 22) / 2.f), 78, 22,
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
        const float pW  = 468.f;
        const float pad = 30.f;
        const float legendIndent = 20.f;
        const float titleSize = 28.f;
        const float statFont = 24.f;
        const float baseStatRowH = 36.f;
        const float legendIconSize = 44.f;
        const float legendLabelSize = 24.f;
        const float legendDescSize = 17.f;
        const float baseLegendRowH = 66.f;
        const float legendGap = 18.f;
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

        statRow("Attack",  TextFormat("%d", (int)std::ceil((float)_player.GetMeleeDamage())));
        statRow("Defense", TextFormat("%d%%", (int)std::ceil(_player.GetDefense() * 100.f)));

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
        const float jX  = sw * 0.785f;
        const float jY  = 98.f;
        const float jW  = sw - jX - 48.f;
        const float jH  = sh - jY - 98.f;
        const float pad = 22.f;

        DrawRectangleRounded({ jX, jY, jW, jH }, 0.05f, 8,
            Fade(Color{12, 71, 84, 255}, 0.92f));
        DrawRectangleRoundedLines({ jX, jY, jW, jH }, 0.05f, 8,
            Fade(Color{130, 235, 255, 255}, 0.30f));

        float cy = jY + pad;
        DrawText("JOURNEY", (int)(jX + pad), (int)cy, 26, Color{255, 214, 102, 255});
        cy += 26.f + 6.f;
        DrawLineEx({ jX + pad, cy }, { jX + jW - pad, cy }, 1.f,
            Fade(Color{130, 235, 255, 255}, 0.55f));
        cy += 14.f;

        // Visited-room squares — one per completed node, coloured by room type
        const float sqSz  = 28.f;
        const float sqGap = 8.f;
        const float sqRowW = jW - pad * 2.f;
        (void)sqRowW;
        float sx = jX + pad;

        int drawn = 0;
        for (const auto& node : _actMap)
        {
            if (!node.completed) continue;

            Color col = nodeColor(node.type);
            DrawRectangleRounded({ sx, cy, sqSz, sqSz }, 0.25f, 4, Fade(col, 0.85f));
            DrawRectangleRoundedLines({ sx, cy, sqSz, sqSz }, 0.25f, 4, col);

            sx += sqSz + sqGap;
            drawn++;
            if (sx + sqSz > jX + jW - pad)
            {
                sx = jX + pad;
                cy += sqSz + sqGap;
            }
        }

        if (drawn == 0)
        {
            DrawText("No rooms cleared yet",
                (int)(jX + pad), (int)cy, 16, Fade(RAYWHITE, 0.45f));
            cy += 20.f;
        }
        else
        {
            cy += sqSz + sqGap + 6.f;
        }

        cy += 16.f;
        DrawText(TextFormat("Rooms: %d", drawn),
            (int)(jX + pad), (int)cy, 20, Color{188, 228, 238, 200});
        cy += 28.f;
        DrawText(TextFormat("Gold:  %d", _player.GetGold()),
            (int)(jX + pad), (int)cy, 20, Color{255, 214, 102, 220});

        // ── Biome progress diamonds — fixed zone: bottom 56% of the panel ──
        {
            // Zone boundaries derived purely from the panel box
            const float divY    = jY + jH * 0.44f;          // divider between rooms and diamonds
            const float zoneTop = divY + 26.f;               // below "BIOMES" label
            const float zoneBot = jY + jH - pad;             // bottom of panel minus padding
            const float zoneH   = zoneBot - zoneTop;

            // Divider + section label
            DrawLineEx({ jX + pad, divY }, { jX + jW - pad, divY }, 1.f,
                Fade(Color{130, 235, 255, 255}, 0.30f));
            DrawText("BIOMES", (int)(jX + pad), (int)(divY + 4.f), 18,
                Color{255, 214, 102, 200});

            // Each act gets an equal slot; diamond is centered inside its slot
            const float slot   = zoneH / (float)kTotalActs;

            // Half-size: fit within slot height and panel width
            const float halfH  = slot * 0.38f;
            const float halfW  = (jW - pad * 2.f) * 0.42f;
            const float half   = std::min(halfH, halfW);

            const float dcx    = jX + jW * 0.5f;

            // Diamond draw helper — state: 0=visited, 1=current, 2=future
            auto drawDiamond = [&](float cx, float dcy, float h, const char* label, int state)
            {
                if (state != 0)
                {
                    Color fill = (state == 1)
                        ? Color{255, 185, 30, 230}   // current — gold
                        : Color{12,  12,  20, 220};  // future  — near-black
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

                int fs = (int)(h * 0.40f);
                fs = std::max(9, std::min(fs, 16));
                Color tc = (state == 0) ? Color{110, 110, 120, 180}
                         : (state == 1) ? Color{255, 245, 190, 255}
                                        : Color{ 70,  70,  85, 200};
                int tw2 = MeasureText(label, fs);
                DrawText(label, (int)(cx - tw2 / 2.f), (int)(dcy - fs / 2.f), fs, tc);
            };

            for (int i = 0; i < kTotalActs; i++)
            {
                // i=0 → act 5 at top; i=4 → act 1 at bottom
                int   act  = kTotalActs - i;
                float dcy  = zoneTop + (i + 0.5f) * slot;
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
            DrawLineEx(n.drawPos, m.drawPos, 2.5f, lc);
        }
    }

    // ── Nodes ─────────────────────────────────────────────────────────────
    static constexpr float kIconSz   = 52.f;
    static constexpr float kIconHalf = kIconSz * 0.5f;
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
            Color tint = n.completed ? Fade(DARKGRAY, 0.45f)
                       : n.available ? (hov ? WHITE : Fade(WHITE, 0.88f))
                       :               Fade(WHITE, 0.20f);
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
        int dSz = 20;
        DrawText(dsc,
            (int)(mapCentreX - MeasureText(dsc, dSz) / 2.f),
            (int)(sh - 90.f), dSz, Color{206, 242, 255, 230});
    }

    // ── Footer hint ───────────────────────────────────────────────────────
    const char* hint = _touchModeActive
        ? "Tap a highlighted node to enter"
        : "Click node  or  A/D  to select  -  Enter / Space  to confirm";
    int ftSz = 18;
    DrawText(hint,
        (int)(mapCentreX - MeasureText(hint, ftSz) / 2.f),
        (int)(sh - 55.f), ftSz, Color{173, 223, 236, 185});
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
            name = "Defense";
            desc = "Def " + pctString(_player.GetDefense()) + " -> "
                + pctString(std::min(_player.GetDefense() + 0.06f, 0.60f));
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
            desc = "Def " + pctString(_player.GetDefense()) + " -> "
                + pctString(std::min(_player.GetDefense() + 0.08f, 0.60f));
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
                + "\nDef " + pctString(_player.GetDefense()) + " -> "
                + pctString(std::min(_player.GetDefense() + 0.08f, 0.60f));
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
            bgNormal = Color{30,15,42,210}; bgHover = Color{60,25,85,230};
            borderNormal = Color{130,45,200,150}; borderHover = Color{210,100,255,255};
            stripColor = Color{100,35,165,220}; nameColor = Color{225,155,255,255}; rarityLabel = "EPIC"; break;
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

    // Prop dots — blue
    for (const auto& prop : _props)
    {
        Vector2 dot = toMini(prop.GetWorldPos());
        DrawCircleV(dot, 3.f, Fade(SKYBLUE, 0.55f));
    }

    // Pickup dots — gold for gold pickups, green for everything else
    for (const auto& pickup : _pickups)
    {
        if (!pickup->IsActive())
            continue;

        Vector2 dot = toMini(pickup->GetWorldPos());
        if (pickup->GetType() == PickupType::Gold)
            DrawCircleV(dot, 4.f, Fade(GOLD, 0.95f));
        else
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
    case Biome::Dungeon: return "Dungeon";
    case Biome::Forest:  return "Forest";
    case Biome::Swamp:   return "Swamp";
    case Biome::Volcano: return "Volcano";
    case Biome::Tundra:  return "Tundra";
    case Biome::Crypt:   return "Crypt";
    case Biome::Desert:  return "Desert";
    case Biome::Ruins:   return "Ruins";
    default:             return "???";
    }
}

void Engine::PopulatePropsForBiome(Biome biome)
{
    _props.clear();

    // Boss room is an open arena — no props
    if (_currentRoomType == RoomType::Boss)
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
    bool useForestMap = (biome == Biome::Forest || biome == Biome::Swamp);
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
            const float navBot  = (_currentBiome == Biome::Forest || _currentBiome == Biome::Swamp) ? 160.f : 220.f;
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
            Biome::Dungeon, Biome::Forest
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
        _startBiomeDungeon = (_biomeSequence[0] == Biome::Dungeon);  // keep fallback in sync
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
    if (_debug.IsActive())
    {
        const Rectangle debugTab{ (float)_windowWidth - 54.f, (float)_windowHeight * 0.43f, 42.f, 132.f };
        const int tabTouchCount = GetTouchPointCount();

        if (tabTouchCount == 0)
        {
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) &&
                CheckCollisionPointRec(GetMousePosition(), debugTab))
            {
                _debug.ToggleOpen();
                return;
            }
        }
        else
        {
            for (int i = 0; i < tabTouchCount; ++i)
            {
                if (CheckCollisionPointRec(GetTouchPosition(i), debugTab))
                {
                    _debug.ToggleOpen();
                    return;
                }
            }
        }
    }

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

