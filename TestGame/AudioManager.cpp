#include "AudioManager.h"

#include "AssetPaths.h"

#include <algorithm>

namespace
{
    bool IsMusicCueFinished(const Music& music)
    {
        return GetMusicTimeLength(music) > 0.f
            && GetMusicTimePlayed(music) >= GetMusicTimeLength(music) - 0.02f;
    }
}

MusicCue ResolveDungeonRunMusicCue(const AudioContext& ctx)
{
    if (ctx.atCrossroads)
        return MusicCue::WorldMap;
    if (ctx.currentRoomType == RoomType::Boss
        && ctx.bossFightActive
        && !ctx.roomClearPending)
        return ctx.finalDomain ? MusicCue::FinalBoss : MusicCue::BossBattle;
    return BiomeMusicCue(ctx.currentBiome);
}

MusicCue BiomeMusicCue(Biome biome)
{
    switch (biome)
    {
    case Biome::Forest:        return MusicCue::Forest;
    case Biome::Jungle:        return MusicCue::BiomeJungle;
    case Biome::Caverns:       return MusicCue::Dungeon;
    case Biome::LostCity:      return MusicCue::BiomeLostCity;
    case Biome::Wastelands:    return MusicCue::BiomeWastelands;
    case Biome::DemonsInsides: return MusicCue::BiomeDark;
    case Biome::Graveyard:     return MusicCue::BiomeGraveyard;
    case Biome::AncientCastle: return MusicCue::BiomeGrand;
    case Biome::TheSanctuary:  return MusicCue::BiomeSanctuary;
    case Biome::DreamRealm:    return MusicCue::BiomeEthereal;
    default:                   return MusicCue::Dungeon;
    }
}

MusicCue ResolveRoomClearVictoryCue(RoomType roomType)
{
    switch (roomType)
    {
    case RoomType::Boss:
        return MusicCue::BossVictory;
    case RoomType::Standard:
    case RoomType::Elite:
    case RoomType::Treasure:
        return MusicCue::BattleVictory;
    default:
        return MusicCue::None;
    }
}

void AudioManager::Init()
{
    _titleThemeMusic = LoadMusicStream(AssetPath("Music/TitleTheme.ogg").c_str());
    _pauseThemeMusic = LoadMusicStream(AssetPath("Music/PauseMenu.ogg").c_str());
    _dungeonThemeMusic = LoadMusicStream(AssetPath("Music/DungeonTheme.ogg").c_str());
    // Forest: Mystwood Reverie (Ovani Fantasy Vol. 7). The older ForestTheme.ogg
    // is kept on disk but no longer routed.
    _forestThemeMusic = LoadMusicStream(AssetPath("Music/Forest_MystwoodReverie.ogg").c_str());
    // Per-mood biome themes (Synth Fantasy pack, streamed WAV — desktop only).
    _darkBiomeMusic     = LoadMusicStream(AssetPath("Music/BiomeDark.wav").c_str());
    _grandBiomeMusic    = LoadMusicStream(AssetPath("Music/BiomeGrand.wav").c_str());
    _etherealBiomeMusic = LoadMusicStream(AssetPath("Music/BiomeEthereal.wav").c_str());
    // Ovani Fantasy Vol. 7 seamless loops (tools/audio/make_music_loops.py).
    _jungleMusic     = LoadMusicStream(AssetPath("Music/Jungle_FeysongFields.ogg").c_str());
    _lostCityMusic   = LoadMusicStream(AssetPath("Music/LostCity_TombOfKings.ogg").c_str());
    _wastelandsMusic = LoadMusicStream(AssetPath("Music/Wastelands_EnemyGrounds.ogg").c_str());
    _graveyardMusic  = LoadMusicStream(AssetPath("Music/Graveyard_BlackDoves.ogg").c_str());
    _sanctuaryMusic  = LoadMusicStream(AssetPath("Music/Sanctuary_Above.ogg").c_str());
    _villageMusic    = LoadMusicStream(AssetPath("Music/Village_Denmark.ogg").c_str());
    _worldMapMusic   = LoadMusicStream(AssetPath("Music/WorldMap_StormbornDestiny.ogg").c_str());
    _finalBossMusic  = LoadMusicStream(AssetPath("Music/FinalBoss_ElderAwakening.ogg").c_str());
    _bossBattleMusic = LoadMusicStream(AssetPath("Music/BossBattle.ogg").c_str());
    _shopThemeMusic = LoadMusicStream(AssetPath("Music/ZephsShop.ogg").c_str());
    _battleVictoryMusic = LoadMusicStream(AssetPath("Music/BattleVictory.ogg").c_str());
    _bossVictoryMusic = LoadMusicStream(AssetPath("Music/BossVictory.ogg").c_str());
    _gameOverMusic = LoadMusicStream(AssetPath("Music/GameOver.ogg").c_str());

    _titleThemeMusic.looping = true;
    _pauseThemeMusic.looping = true;
    _dungeonThemeMusic.looping = true;
    _forestThemeMusic.looping = true;
    _darkBiomeMusic.looping = true;
    _grandBiomeMusic.looping = true;
    _etherealBiomeMusic.looping = true;
    for (Music* music : { &_jungleMusic, &_lostCityMusic, &_wastelandsMusic, &_graveyardMusic,
                          &_sanctuaryMusic, &_villageMusic, &_worldMapMusic, &_finalBossMusic })
    {
        music->looping = true;
        SetMusicVolume(*music, 0.35f);
    }
    _bossBattleMusic.looping = true;
    _shopThemeMusic.looping = true;
    _battleVictoryMusic.looping = false;
    _bossVictoryMusic.looping = false;
    _gameOverMusic.looping = false;

    SetMusicVolume(_titleThemeMusic, 0.35f);
    SetMusicVolume(_pauseThemeMusic, 0.35f);
    SetMusicVolume(_dungeonThemeMusic, 0.35f);
    SetMusicVolume(_forestThemeMusic, 0.35f);
    SetMusicVolume(_darkBiomeMusic, 0.35f);
    SetMusicVolume(_grandBiomeMusic, 0.35f);
    SetMusicVolume(_etherealBiomeMusic, 0.35f);
    SetMusicVolume(_bossBattleMusic, 0.35f);
    SetMusicVolume(_shopThemeMusic, 0.35f);
    SetMusicVolume(_battleVictoryMusic, 0.65f);
    SetMusicVolume(_bossVictoryMusic, 0.65f);
    SetMusicVolume(_gameOverMusic, 0.55f);
}

void AudioManager::Shutdown()
{
    Reset();
    UnloadMusicStream(_titleThemeMusic);
    UnloadMusicStream(_pauseThemeMusic);
    UnloadMusicStream(_dungeonThemeMusic);
    UnloadMusicStream(_forestThemeMusic);
    UnloadMusicStream(_darkBiomeMusic);
    UnloadMusicStream(_grandBiomeMusic);
    UnloadMusicStream(_etherealBiomeMusic);
    for (Music* music : { &_jungleMusic, &_lostCityMusic, &_wastelandsMusic, &_graveyardMusic,
                          &_sanctuaryMusic, &_villageMusic, &_worldMapMusic, &_finalBossMusic })
        UnloadMusicStream(*music);
    UnloadMusicStream(_bossBattleMusic);
    UnloadMusicStream(_shopThemeMusic);
    UnloadMusicStream(_battleVictoryMusic);
    UnloadMusicStream(_bossVictoryMusic);
    UnloadMusicStream(_gameOverMusic);
}

Music* AudioManager::GetMusicByCue(MusicCue cue)
{
    switch (cue)
    {
    case MusicCue::Title:         return &_titleThemeMusic;
    case MusicCue::Pause:         return &_pauseThemeMusic;
    case MusicCue::Dungeon:       return &_dungeonThemeMusic;
    case MusicCue::Forest:        return &_forestThemeMusic;
    case MusicCue::BiomeDark:     return &_darkBiomeMusic;
    case MusicCue::BiomeGrand:    return &_grandBiomeMusic;
    case MusicCue::BiomeEthereal: return &_etherealBiomeMusic;
    case MusicCue::BossBattle:    return &_bossBattleMusic;
    case MusicCue::Shop:          return &_shopThemeMusic;
    case MusicCue::BattleVictory: return &_battleVictoryMusic;
    case MusicCue::BossVictory:   return &_bossVictoryMusic;
    case MusicCue::GameOver:      return &_gameOverMusic;
    case MusicCue::BiomeJungle:     return &_jungleMusic;
    case MusicCue::BiomeLostCity:   return &_lostCityMusic;
    case MusicCue::BiomeWastelands: return &_wastelandsMusic;
    case MusicCue::BiomeGraveyard:  return &_graveyardMusic;
    case MusicCue::BiomeSanctuary:  return &_sanctuaryMusic;
    case MusicCue::Village:         return &_villageMusic;
    case MusicCue::WorldMap:        return &_worldMapMusic;
    case MusicCue::FinalBoss:       return &_finalBossMusic;
    default:                      return nullptr;
    }
}

bool AudioManager::IsLoopMusicCue(MusicCue cue) const
{
    return cue == MusicCue::Title
        || cue == MusicCue::Pause
        || cue == MusicCue::Dungeon
        || cue == MusicCue::Forest
        || cue == MusicCue::BiomeDark
        || cue == MusicCue::BiomeGrand
        || cue == MusicCue::BiomeEthereal
        || cue == MusicCue::BossBattle
        || cue == MusicCue::Shop
        || IsBiomeLoopCue(cue)
        || cue == MusicCue::Village
        || cue == MusicCue::WorldMap
        || cue == MusicCue::FinalBoss;
}

bool AudioManager::IsVictoryMusicCue(MusicCue cue) const
{
    return cue == MusicCue::BattleVictory || cue == MusicCue::BossVictory;
}

bool AudioManager::IsBiomeLoopCue(MusicCue cue) const
{
    return cue == MusicCue::Forest
        || cue == MusicCue::Dungeon
        || cue == MusicCue::BiomeDark
        || cue == MusicCue::BiomeGrand
        || cue == MusicCue::BiomeEthereal
        || cue == MusicCue::BiomeJungle
        || cue == MusicCue::BiomeLostCity
        || cue == MusicCue::BiomeWastelands
        || cue == MusicCue::BiomeGraveyard
        || cue == MusicCue::BiomeSanctuary;
}

MusicCue AudioManager::GetBiomeMusicCue(Biome biome) const
{
    return BiomeMusicCue(biome);
}

bool AudioManager::ShouldHoldSilenceForVictory(const AudioContext& ctx) const
{
    if (!_silenceAfterVictory)
        return false;

    return ctx.gameState != GameState::Map
        && ctx.gameState != GameState::Menu
        && ctx.gameState != GameState::HowToPlay
        && ctx.gameState != GameState::Keybindings
        && ctx.gameState != GameState::Pause
        && ctx.gameState != GameState::Settings
        && ctx.gameState != GameState::Shop
        && ctx.gameState != GameState::DemoEnd;
}

float AudioManager::GetMusicBaseVolume(MusicCue cue) const
{
    switch (cue)
    {
    case MusicCue::Title:         return 0.35f;
    case MusicCue::Pause:         return 0.35f;
    case MusicCue::Dungeon:       return 0.35f;
    case MusicCue::Forest:        return 0.35f;
    case MusicCue::BiomeDark:     return 0.35f;
    case MusicCue::BiomeGrand:    return 0.35f;
    case MusicCue::BiomeEthereal: return 0.35f;
    case MusicCue::BossBattle:    return 0.35f;
    case MusicCue::Shop:          return 0.35f;
    case MusicCue::BattleVictory: return 0.65f;
    case MusicCue::BossVictory:   return 0.65f;
    case MusicCue::GameOver:      return 0.55f;
    case MusicCue::BiomeJungle:
    case MusicCue::BiomeLostCity:
    case MusicCue::BiomeWastelands:
    case MusicCue::BiomeGraveyard:
    case MusicCue::BiomeSanctuary:
    case MusicCue::Village:
    case MusicCue::WorldMap:
    case MusicCue::FinalBoss:     return 0.35f;
    default:                      return 0.f;
    }
}

MusicCue AudioManager::GetDesiredLoopMusicCue(const AudioContext& ctx) const
{
    if (ShouldHoldSilenceForVictory(ctx))
        return MusicCue::None;

    switch (ctx.gameState)
    {
    case GameState::Menu:
    case GameState::DemoEnd:
    case GameState::ClassSelect:
        return MusicCue::Title;

    case GameState::HowToPlay:
        return (ctx.howToPlayFrom == GameState::Pause) ? MusicCue::Pause : MusicCue::Title;

    case GameState::Keybindings:
    case GameState::Pause:
    case GameState::Settings:
        return MusicCue::Pause;

    case GameState::Shop:
    case GameState::MetaShop:
    case GameState::Armory:
        return MusicCue::Shop;

    case GameState::Map:
        return GetBiomeMusicCue(ctx.actBiome);

    case GameState::Village:
    case GameState::CosmeticShop:
        return MusicCue::Village;

    case GameState::WorldMap:
        return MusicCue::WorldMap;

    case GameState::DungeonRun:
        return ResolveDungeonRunMusicCue(ctx);

    case GameState::GameOver:
        return MusicCue::None;

    case GameState::Play:
        if (ctx.currentRoomType == RoomType::Boss && !ctx.roomClearPending && ctx.bossFightActive)
            return MusicCue::BossBattle;
        return GetBiomeMusicCue(ctx.actBiome);

    case GameState::LevelUpChoice:
        if (ctx.awaitingStartingAbility)
            return GetBiomeMusicCue(ctx.actBiome);
        return _silenceAfterVictory ? MusicCue::None : GetBiomeMusicCue(ctx.currentBiome);

    case GameState::AbilityChoice:
    case GameState::ExpTally:
        return _silenceAfterVictory ? MusicCue::None : GetBiomeMusicCue(ctx.currentBiome);

    default:
        return MusicCue::None;
    }
}

void AudioManager::StopMusicCue(MusicCue cue)
{
    Music* music = GetMusicByCue(cue);
    if (music && music->ctxData != nullptr)
        StopMusicStream(*music);
    if (_currentMusicCue == cue)
        _currentMusicCue = MusicCue::None;
    if (_outgoingMusicCue == cue)
        _outgoingMusicCue = MusicCue::None;
}

void AudioManager::FinishOutgoingMusic()
{
    const MusicCue cue = _outgoingMusicCue;
    _outgoingMusicCue = MusicCue::None;
    _outgoingFadeTimer = 0.f;
    Music* music = GetMusicByCue(cue);
    if (!music || music->ctxData == nullptr)
        return;
    if (cue == _suspendedMusicCue)
        PauseMusicStream(*music);
    else
        StopMusicStream(*music);
}

void AudioManager::StartMusicCue(MusicCue cue, float startTime)
{
    Music* music = GetMusicByCue(cue);
    if (!music || music->ctxData == nullptr || cue == _currentMusicCue)
        return;

    FinishOutgoingMusic();
    const bool crossfade = IsLoopMusicCue(_currentMusicCue) && IsLoopMusicCue(cue);
    if (crossfade)
    {
        _outgoingMusicCue = _currentMusicCue;
        _outgoingFadeTimer = 0.6f;
        _outgoingStartGain = _musicFadeInTimer > 0.f
            ? std::clamp(1.f - _musicFadeInTimer / _musicFadeInDuration, 0.f, 1.f) : 1.f;
    }
    else if (_currentMusicCue != MusicCue::None)
        StopMusicCue(_currentMusicCue);

    SetMusicVolume(*music, IsLoopMusicCue(cue) ? 0.f : GetMusicBaseVolume(cue) * _musicVolumeScale);
    PlayMusicStream(*music);
    if (startTime > 0.f)
        SeekMusicStream(*music, startTime);
    _currentMusicCue = cue;
    _musicFadeInDuration = crossfade ? 0.6f : 2.f;
    _musicFadeInTimer = IsLoopMusicCue(cue) ? _musicFadeInDuration : 0.f;
}

void AudioManager::SuspendCurrentLoopForCue(MusicCue cue, bool resumeOnMapOnly)
{
    if (!IsLoopMusicCue(_currentMusicCue))
        return;

    Music* current = GetMusicByCue(_currentMusicCue);
    if (!current || current->ctxData == nullptr)
        return;

    if (_suspendedMusicCue != MusicCue::None && _suspendedMusicCue != _currentMusicCue)
        StopMusicCue(_suspendedMusicCue);
    _suspendedMusicCue = _currentMusicCue;
    _suspendedMusicTime = GetMusicTimePlayed(*current);
    _resumeSuspendedMusicOnMapOnly = resumeOnMapOnly;
    if (!IsLoopMusicCue(cue))
    {
        FinishOutgoingMusic();
        PauseMusicStream(*current);
        _currentMusicCue = MusicCue::None;
    }

    StartMusicCue(cue);
}

void AudioManager::ResumePausedLoopMusic()
{
    if (_suspendedMusicCue == MusicCue::None)
        return;

    Music* music = GetMusicByCue(_suspendedMusicCue);
    if (!music || music->ctxData == nullptr)
        return;

    const MusicCue cue = _suspendedMusicCue;
    const float time = _suspendedMusicTime;
    StartMusicCue(cue, time);
    if (_outgoingMusicCue == MusicCue::None)
    {
        _musicFadeInDuration = 1.5f;
        _musicFadeInTimer = _musicFadeInDuration;
    }
    _suspendedMusicCue = MusicCue::None;
    _suspendedMusicTime = 0.f;
    _resumeSuspendedMusicOnMapOnly = false;
}

void AudioManager::StartBattleVictory()
{
    _activeVictoryCue = MusicCue::BattleVictory;
    _silenceAfterVictory = false;
    if (IsLoopMusicCue(_currentMusicCue))
        SuspendCurrentLoopForCue(MusicCue::BattleVictory, false);
    else
        StartMusicCue(MusicCue::BattleVictory);
}

void AudioManager::StartBossVictory()
{
    _activeVictoryCue = MusicCue::BossVictory;
    _silenceAfterVictory = true;
    FinishOutgoingMusic();
    if (_suspendedMusicCue != MusicCue::None && _suspendedMusicCue != _currentMusicCue)
        StopMusicCue(_suspendedMusicCue);
    _suspendedMusicCue = MusicCue::None;
    _suspendedMusicTime = 0.f;
    _resumeSuspendedMusicOnMapOnly = false;
    if (_currentMusicCue != MusicCue::None)
        StopMusicCue(_currentMusicCue);
    StartMusicCue(MusicCue::BossVictory);
}

void AudioManager::Reset()
{
    FinishOutgoingMusic();
    if (_suspendedMusicCue != MusicCue::None && _suspendedMusicCue != _currentMusicCue)
        StopMusicCue(_suspendedMusicCue);
    _combatMix = 1.f;
    if (_currentMusicCue != MusicCue::None)
        StopMusicCue(_currentMusicCue);

    _suspendedMusicCue = MusicCue::None;
    _suspendedMusicTime = 0.f;
    _activeVictoryCue = MusicCue::None;
    _musicFadeInTimer = 0.f;
    _musicFadeInDuration = 2.0f;
    _resumeSuspendedMusicOnMapOnly = false;
    _silenceAfterVictory = false;
    _gameOverMusicPlayed = false;
}

void AudioManager::Update(const AudioContext& ctx)
{
    if (ctx.gameState != GameState::GameOver)
        _gameOverMusicPlayed = false;

    // Ease the combat mix toward its target: full during a fight, a calmer
    // floor when the room is clear. Fast swell in, gentle settle out — biome
    // exploration loops multiply their volume by this (see below).
    {
        const float kCalmMix = 0.40f;
        float target = ctx.inCombat ? 1.f : kCalmMix;
        float step   = (target > _combatMix ? 2.0f : 0.7f) * GetFrameTime();
        _combatMix = (target > _combatMix) ? std::min(target, _combatMix + step)
                                           : std::max(target, _combatMix - step);
    }

    if (_outgoingMusicCue != MusicCue::None)
    {
        Music* outgoing = GetMusicByCue(_outgoingMusicCue);
        _outgoingFadeTimer = std::max(0.f, _outgoingFadeTimer - GetFrameTime());
        if (outgoing && outgoing->ctxData != nullptr)
        {
            UpdateMusicStream(*outgoing);
            float volume = GetMusicBaseVolume(_outgoingMusicCue) * _musicVolumeScale;
            if (IsBiomeLoopCue(_outgoingMusicCue))
                volume *= _combatMix;
            SetMusicVolume(*outgoing, volume * _outgoingStartGain * (_outgoingFadeTimer / 0.6f));
        }
        if (_outgoingFadeTimer <= 0.f)
            FinishOutgoingMusic();
    }

    if (ctx.gameState == GameState::GameOver && _currentMusicCue == MusicCue::None && !_gameOverMusicPlayed)
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
            if (!IsLoopMusicCue(_currentMusicCue))
                SetMusicVolume(*current, GetMusicBaseVolume(_currentMusicCue) * _musicVolumeScale);
            if (IsLoopMusicCue(_currentMusicCue))
            {
                float baseVolume = GetMusicBaseVolume(_currentMusicCue) * _musicVolumeScale;
                // Biome exploration music ducks to the calm floor when no
                // enemies are around; boss/menu/shop loops stay at full volume.
                if (IsBiomeLoopCue(_currentMusicCue))
                    baseVolume *= _combatMix;
                if (_musicFadeInTimer > 0.f)
                {
                    _musicFadeInTimer = std::max(0.f, _musicFadeInTimer - GetFrameTime());
                    float t = 1.f - (_musicFadeInTimer / _musicFadeInDuration);
                    SetMusicVolume(*current, baseVolume * std::clamp(t, 0.f, 1.f));
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

    MusicCue desiredCue = GetDesiredLoopMusicCue(ctx);
    if (ShouldHoldSilenceForVictory(ctx))
        desiredCue = MusicCue::None;

    if (_silenceAfterVictory && !ShouldHoldSilenceForVictory(ctx))
        _silenceAfterVictory = false;

    if (desiredCue == MusicCue::None)
    {
        FinishOutgoingMusic();
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

    if (_suspendedMusicCue == desiredCue && _resumeSuspendedMusicOnMapOnly && ctx.gameState == GameState::Map)
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

    StartMusicCue(desiredCue);
}
