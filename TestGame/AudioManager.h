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
    Biome actBiome = Biome::Dungeon;
    Biome currentBiome = Biome::Dungeon;
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
