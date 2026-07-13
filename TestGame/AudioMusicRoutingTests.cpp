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
    TestDungeonBossUsesBossBattleMusic();
    TestOrdinaryDungeonRoomKeepsDungeonTheme();
    TestCombatRoomClearVictoryCues();
    std::puts("Audio music routing tests passed");
    return 0;
}
