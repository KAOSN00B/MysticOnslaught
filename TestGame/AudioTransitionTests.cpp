#include "AudioManager.h"
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <string>
struct Stream { bool playing=false; float volume=0, time=4; int updates=0, stops=0; };
static std::map<void*, Stream> streams;
static std::map<std::string, void*> names;
static float dt=0.1f;
static int nextId=0;
extern "C" {
bool FileExists(const char*) { return true; }
Music LoadMusicStream(const char* path) { Music m{}; m.ctxData=reinterpret_cast<void*>(static_cast<intptr_t>(++nextId)); streams[m.ctxData]={}; names[path]=m.ctxData; return m; }
void UnloadMusicStream(Music m) { streams.erase(m.ctxData); }
void PlayMusicStream(Music m) { streams[m.ctxData].playing=true; }
void StopMusicStream(Music m) { auto& s=streams[m.ctxData]; s.playing=false; s.time=0; ++s.stops; }
void PauseMusicStream(Music m) { streams[m.ctxData].playing=false; }
void ResumeMusicStream(Music m) { streams[m.ctxData].playing=true; }
void SeekMusicStream(Music m,float t) { streams[m.ctxData].time=t; }
void SetMusicVolume(Music m,float v) { streams[m.ctxData].volume=v; }
void UpdateMusicStream(Music m) { ++streams[m.ctxData].updates; }
bool IsMusicStreamPlaying(Music m) { return streams[m.ctxData].playing; }
float GetMusicTimePlayed(Music m) { return streams[m.ctxData].time; }
float GetMusicTimeLength(Music) { return 100; }
float GetFrameTime() { return dt; }
}
static void check(bool ok,const char* message) { if(!ok) { std::fprintf(stderr,"FAIL: %s\n",message); std::exit(1); } }
static Stream& stream(const char* name) { return streams.at(names.at(std::string("Music/")+name)); }
static void settle(AudioManager& a,const AudioContext& c) { for(int i=0;i<25;++i) a.Update(c); }
#ifndef AUDIO_ROUTING_STUBS_ONLY
int main() {
 AudioManager a; a.Init(); AudioContext c; c.inCombat=true; settle(a,c);
 c.gameState=GameState::Map; c.actBiome=Biome::Forest; a.Update(c);
 check(stream("TitleTheme.ogg").playing,"outgoing loop remains alive during crossfade");
 check(stream("Forest_MystwoodReverie.ogg").playing,"incoming loop starts during crossfade");
 int updates=stream("TitleTheme.ogg").updates;
 a.Update(c);
 check(stream("TitleTheme.ogg").updates==updates+1,"outgoing stream serviced each frame");
 check(stream("Forest_MystwoodReverie.ogg").volume>0 && stream("Forest_MystwoodReverie.ogg").volume<0.35f,"incoming volume ramps");
 check(stream("TitleTheme.ogg").volume>0 && stream("TitleTheme.ogg").volume<0.35f,"outgoing volume ramps");
 a.SetMusicVolumeScale(0); a.Update(c);
 check(stream("TitleTheme.ogg").volume==0 && stream("Forest_MystwoodReverie.ogg").volume==0,"settings mute both crossfade streams");
 a.SetMusicVolumeScale(1); settle(a,c);
 check(!stream("TitleTheme.ogg").playing && stream("TitleTheme.ogg").stops==1,"crossfade retires outgoing exactly once");
 c.gameState=GameState::Pause; a.Update(c); settle(a,c);
 check(!stream("Forest_MystwoodReverie.ogg").playing,"pause suspends gameplay loop");
 c.gameState=GameState::Map; a.Update(c); settle(a,c);
 check(!stream("PauseMenu.ogg").playing && stream("Forest_MystwoodReverie.ogg").playing,"resume retires pause cue");
 a.StartBattleVictory(); check(!stream("Forest_MystwoodReverie.ogg").playing && stream("BattleVictory.ogg").playing,"victory suspends loop");
 a.SetMusicVolumeScale(0); a.Update(c); check(stream("BattleVictory.ogg").volume==0,"settings apply to one-shots");
 stream("BattleVictory.ogg").playing=false; a.Update(c); settle(a,c);
 check(stream("Forest_MystwoodReverie.ogg").playing,"battle victory resumes loop");
 c.actBiome=Biome::DreamRealm; a.Update(c); c.actBiome=Biome::Forest; a.Update(c); settle(a,c);
 check(stream("Forest_MystwoodReverie.ogg").playing && !stream("BiomeEthereal.wav").playing,"rapid reversal leaves only desired loop");
 c.actBiome=Biome::DreamRealm; a.Update(c); a.StartBossVictory();
 check(!stream("Forest_MystwoodReverie.ogg").playing && !stream("BiomeEthereal.wav").playing && stream("BossVictory.ogg").playing,"boss victory clears both crossfade loops");
 stream("BossVictory.ogg").playing=false; c.gameState=GameState::Play; a.Update(c); a.Update(c);
 check(!stream("BiomeEthereal.wav").playing,"boss victory holds silence in room");
 c.gameState=GameState::Map; c.actBiome=Biome::Forest; settle(a,c);
 c.gameState=GameState::Pause; a.Update(c); a.Reset();
 for(auto& entry:streams) check(!entry.second.playing,"reset leaves no playing streams");
 check(stream("Forest_MystwoodReverie.ogg").time==0,"reset releases suspended playback position");
 a.Shutdown(); check(streams.empty(),"shutdown unloads every stream");
 std::puts("Audio transition tests passed");
}
#endif
