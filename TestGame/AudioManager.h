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
    bool inCombat = false;   // enemies alive in the current room → swell biome music
    Biome actBiome = Biome::Caverns;
    Biome currentBiome = Biome::Caverns;
};

// Pure routing helpers shared by AudioManager and gameplay. Keeping these free
// of playback state makes the intended procedural-dungeon cues testable.
MusicCue ResolveDungeonRunMusicCue(const AudioContext& ctx);
MusicCue ResolveRoomClearVictoryCue(RoomType roomType);

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
    // Biome exploration loops that duck to a calmer volume when no enemies are
    // present (boss/menu/shop loops are excluded — they stay at full volume).
    bool IsBiomeLoopCue(MusicCue cue) const;
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
    Music _darkBiomeMusic{};      // Demon's Insides, Graveyard
    Music _grandBiomeMusic{};     // Ancient Castle, The Sanctuary
    Music _etherealBiomeMusic{};  // Dream Realm
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
    // Smoothed 0..1 combat mix: 1 = full-volume combat, calmer floor when a room
    // is clear. Lerped in Update so the music eases between explore and fight.
    float _combatMix = 1.f;
};
