#pragma once

#include "GameTypes.h"
#include "raylib.h"

struct AudioContext
{
    GameState gameState = GameState::Menu;
    GameState howToPlayFrom = GameState::Menu;
    bool awaitingStartingAbility = false;
    RoomType currentRoomType = RoomType::Standard;
    bool roomClearPending = false;
    bool bossFightActive = false;
    Biome actBiome = Biome::Caverns;
    Biome currentBiome = Biome::Caverns;
};

class AudioManager
{
public:
    void Init();
    void Shutdown();
    void Update(const AudioContext& ctx);
    void Reset();
    void StartBattleVictory();
    void StartBossVictory();

    // Volume scale setters (called by SettingsManager::ApplyVolumes).
    // These multiply against each cue's internal base volume.
    void  SetMusicVolumeScale(float v) { _musicVolumeScale = v < 0.f ? 0.f : v > 1.f ? 1.f : v; }
    void  SetSfxVolumeScale(float v)   { _sfxVolumeScale   = v < 0.f ? 0.f : v > 1.f ? 1.f : v; }
    float GetSfxVolumeScale() const    { return _sfxVolumeScale; }

private:
    Music* GetMusicByCue(MusicCue cue);
    MusicCue GetBiomeMusicCue(Biome biome) const;
    MusicCue GetDesiredLoopMusicCue(const AudioContext& ctx) const;
    float GetMusicBaseVolume(MusicCue cue) const;
    bool IsLoopMusicCue(MusicCue cue) const;
    bool IsVictoryMusicCue(MusicCue cue) const;
    bool ShouldHoldSilenceForVictory(const AudioContext& ctx) const;
    void StartMusicCue(MusicCue cue, float startTime = 0.f);
    void StopMusicCue(MusicCue cue);
    void SuspendCurrentLoopForCue(MusicCue cue, bool resumeOnMapOnly);
    void ResumePausedLoopMusic();

    Music _titleThemeMusic{};
    Music _pauseThemeMusic{};
    Music _dungeonThemeMusic{};
    Music _forestThemeMusic{};
    Music _bossBattleMusic{};
    Music _shopThemeMusic{};
    Music _battleVictoryMusic{};
    Music _bossVictoryMusic{};
    Music _gameOverMusic{};

    float    _musicVolumeScale = 1.0f;
    float    _sfxVolumeScale   = 1.0f;

    MusicCue _currentMusicCue = MusicCue::None;
    MusicCue _suspendedMusicCue = MusicCue::None;
    MusicCue _activeVictoryCue = MusicCue::None;
    float _suspendedMusicTime = 0.f;
    float _musicFadeInTimer = 0.f;
    float _musicFadeInDuration = 2.0f;
    bool _resumeSuspendedMusicOnMapOnly = false;
    bool _silenceAfterVictory = false;
    bool _gameOverMusicPlayed = false;
};
