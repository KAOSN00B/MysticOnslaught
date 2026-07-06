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

void AudioManager::Init()
{
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
}

void AudioManager::Shutdown()
{
    UnloadMusicStream(_titleThemeMusic);
    UnloadMusicStream(_pauseThemeMusic);
    UnloadMusicStream(_dungeonThemeMusic);
    UnloadMusicStream(_forestThemeMusic);
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
    case MusicCue::BossBattle:    return &_bossBattleMusic;
    case MusicCue::Shop:          return &_shopThemeMusic;
    case MusicCue::BattleVictory: return &_battleVictoryMusic;
    case MusicCue::BossVictory:   return &_bossVictoryMusic;
    case MusicCue::GameOver:      return &_gameOverMusic;
    default:                      return nullptr;
    }
}

bool AudioManager::IsLoopMusicCue(MusicCue cue) const
{
    return cue == MusicCue::Title
        || cue == MusicCue::Pause
        || cue == MusicCue::Dungeon
        || cue == MusicCue::Forest
        || cue == MusicCue::BossBattle
        || cue == MusicCue::Shop;
}

bool AudioManager::IsVictoryMusicCue(MusicCue cue) const
{
    return cue == MusicCue::BattleVictory || cue == MusicCue::BossVictory;
}

MusicCue AudioManager::GetBiomeMusicCue(Biome biome) const
{
    switch (biome)
    {
    case Biome::Forest:
    case Biome::Jungle:
        return MusicCue::Forest;
    default:
        return MusicCue::Dungeon;
    }
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
    case MusicCue::BossBattle:    return 0.35f;
    case MusicCue::Shop:          return 0.35f;
    case MusicCue::BattleVictory: return 0.65f;
    case MusicCue::BossVictory:   return 0.65f;
    case MusicCue::GameOver:      return 0.55f;
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
        return MusicCue::Shop;

    case GameState::Map:
        return GetBiomeMusicCue(ctx.actBiome);

    case GameState::DungeonRun:
        return MusicCue::Dungeon;

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
    if (!music || music->ctxData == nullptr)
        return;

    StopMusicStream(*music);
    if (_currentMusicCue == cue)
        _currentMusicCue = MusicCue::None;
}

void AudioManager::StartMusicCue(MusicCue cue, float startTime)
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
        SetMusicVolume(*music, GetMusicBaseVolume(cue) * _musicVolumeScale);

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
    {
        _musicFadeInTimer = 0.f;
    }
}

void AudioManager::SuspendCurrentLoopForCue(MusicCue cue, bool resumeOnMapOnly)
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

void AudioManager::ResumePausedLoopMusic()
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
    _suspendedMusicCue = MusicCue::None;
    _suspendedMusicTime = 0.f;
    _resumeSuspendedMusicOnMapOnly = false;
    if (_currentMusicCue != MusicCue::None)
        StopMusicCue(_currentMusicCue);
    StartMusicCue(MusicCue::BossVictory);
}

void AudioManager::Reset()
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
    _gameOverMusicPlayed = false;
}

void AudioManager::Update(const AudioContext& ctx)
{
    if (ctx.gameState != GameState::GameOver)
        _gameOverMusicPlayed = false;

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
            if (IsLoopMusicCue(_currentMusicCue))
            {
                float baseVolume = GetMusicBaseVolume(_currentMusicCue) * _musicVolumeScale;
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

    if (IsLoopMusicCue(_currentMusicCue) && _currentMusicCue != desiredCue)
        StopMusicCue(_currentMusicCue);

    StartMusicCue(desiredCue);
}
