#include "AudioManager.h"

#include <cassert>
#include <cstdio>

namespace
{
    void TestDungeonBossUsesBossBattleMusic()
    {
        AudioContext ctx{};
        ctx.gameState = GameState::DungeonRun;
        ctx.currentRoomType = RoomType::Boss;
        ctx.bossFightActive = true;
        ctx.roomClearPending = false;

        assert(ResolveDungeonRunMusicCue(ctx) == MusicCue::BossBattle);

        ctx.roomClearPending = true;
        assert(ResolveDungeonRunMusicCue(ctx) == MusicCue::Dungeon);
    }

    void TestOrdinaryDungeonRoomKeepsDungeonTheme()
    {
        AudioContext ctx{};
        ctx.gameState = GameState::DungeonRun;
        ctx.currentRoomType = RoomType::Standard;
        ctx.bossFightActive = false;

        assert(ResolveDungeonRunMusicCue(ctx) == MusicCue::Dungeon);
    }

    void TestEveryBiomeHasItsOwnTheme()
    {
        // Caverns keeps the original dungeon theme; every biome is distinct.
        bool seen[64] = {};
        for (int b = 0; b < 10; ++b)
        {
            const MusicCue cue = BiomeMusicCue((Biome)b);
            assert(cue != MusicCue::None);
            assert(!seen[(int)cue]);
            seen[(int)cue] = true;
        }
        assert(BiomeMusicCue(Biome::Caverns) == MusicCue::Dungeon);
        assert(BiomeMusicCue(Biome::Forest) == MusicCue::Forest);

        AudioContext ctx{};
        ctx.gameState = GameState::DungeonRun;
        ctx.currentRoomType = RoomType::Standard;
        ctx.currentBiome = Biome::Jungle;
        assert(ResolveDungeonRunMusicCue(ctx) == MusicCue::BiomeJungle);   // runs follow the biome
    }

    void TestFinalDomainBossUsesFinalTheme()
    {
        AudioContext ctx{};
        ctx.gameState = GameState::DungeonRun;
        ctx.currentRoomType = RoomType::Boss;
        ctx.bossFightActive = true;
        ctx.finalDomain = true;
        assert(ResolveDungeonRunMusicCue(ctx) == MusicCue::FinalBoss);
    }

    void TestCombatRoomClearVictoryCues()
    {
        assert(ResolveRoomClearVictoryCue(RoomType::Standard) == MusicCue::BattleVictory);
        assert(ResolveRoomClearVictoryCue(RoomType::Elite) == MusicCue::BattleVictory);
        assert(ResolveRoomClearVictoryCue(RoomType::Treasure) == MusicCue::BattleVictory);
        assert(ResolveRoomClearVictoryCue(RoomType::Boss) == MusicCue::BossVictory);
        assert(ResolveRoomClearVictoryCue(RoomType::Rest) == MusicCue::None);
        assert(ResolveRoomClearVictoryCue(RoomType::Store) == MusicCue::None);
    }
}

int main()
{
    {
        // The crossroads between domains keeps the journey theme, whatever biome
        // its room is drawn in.
        AudioContext crossroads{};
        crossroads.gameState = GameState::DungeonRun;
        crossroads.currentBiome = Biome::Caverns;
        crossroads.atCrossroads = true;
        assert(ResolveDungeonRunMusicCue(crossroads) == MusicCue::WorldMap);
        crossroads.atCrossroads = false;
        assert(ResolveDungeonRunMusicCue(crossroads) == BiomeMusicCue(Biome::Caverns));
    }
    TestDungeonBossUsesBossBattleMusic();
    TestOrdinaryDungeonRoomKeepsDungeonTheme();
    TestEveryBiomeHasItsOwnTheme();
    TestFinalDomainBossUsesFinalTheme();
    TestCombatRoomClearVictoryCues();
    std::puts("Audio music routing tests passed: unique biome themes, final boss, victory cues");
    return 0;
}
