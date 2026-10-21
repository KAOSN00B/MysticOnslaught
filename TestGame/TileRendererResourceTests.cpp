#include "TileRenderer.h"
#include "RoomAssetCatalog.h"
#include "TextureCache.h"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <set>
#include <string>
namespace fs=std::filesystem;
static unsigned serial=0;
static int loads=0, frees=0;
static std::set<unsigned> live;
static std::string failedPath;
static void check(bool ok,const char* msg) { if(!ok) { std::fprintf(stderr,"FAIL: %s\n",msg); std::exit(1); } }
// Rendering is outside this resource-only test; fail if unexpectedly reached.
bool RoomPlacementClearsAtDoor(Rectangle,const RoomLayout&) { std::abort(); }
bool ShouldDrawGeneratedRoomFloor(int,int,const RoomLayout&) { std::abort(); }
bool ShouldDrawGeneratedRoomTile(TileType,const RoomLayout&) { std::abort(); }
bool RoomVisualClearedByOpenDoor(const RoomTilePlacement&,const RoomLayout&) { std::abort(); }
extern "C" {
double GetTime() { std::abort(); }
void DrawRectangle(int,int,int,int,Color) { std::abort(); }
void DrawTexturePro(Texture2D,Rectangle,Rectangle,Vector2,float,Color) { std::abort(); }
long GetFileModTime(const char* p) { std::error_code ec; auto t=fs::last_write_time(p,ec); return ec?0:static_cast<long>(t.time_since_epoch().count()/10000000); }
Texture2D LoadTexture(const char* p) { if(p==failedPath) return {}; ++loads; Texture2D t{}; t.id=++serial; live.insert(t.id); return t; }
void UnloadTexture(Texture2D t) { check(live.erase(t.id)==1,"release exactly once"); ++frees; }
}
int main() {
 const fs::path root=fs::temp_directory_path()/("mystic_renderer_"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
 fs::create_directories(root/"catalog");
 auto file=[](const fs::path& p) { std::ofstream(p)<<"test image"; };
 auto main=root/"main.png", ground=root/"ground.png", reward=root/"reward.png";
 file(main); file(ground); file(reward); file(root/"catalog"/"room.png");
 TileRenderer a,b; TileDefSet defs{};
 auto init=[&](TileRenderer& r) { r.Init(main.string().c_str(),ground.string().c_str(),reward.string().c_str(),defs); };
 init(a); init(a); init(b); check(loads==3 && frees==0,"actual renderers reuse init sheets across holders");
 RoomAssetCatalog catalog; catalog.Refresh(root/"catalog"); a.LoadRoomAssetCatalog(catalog); a.LoadRoomAssetCatalog(catalog); b.LoadRoomAssetCatalog(catalog);
 check(loads==4 && frees==0,"actual catalogs share textures without reuploads");
 fs::last_write_time(main,fs::last_write_time(main)+std::chrono::seconds(2)); init(a);
 check(loads==5 && frees==0,"changed image reloads while old renderer remains valid");
 init(b); check(loads==5 && frees==1,"old revision released after final holder updates");
 fs::remove(root/"catalog"/"room.png"); catalog.Refresh(root/"catalog"); a.LoadRoomAssetCatalog(catalog); b.LoadRoomAssetCatalog(catalog);
 check(frees==2,"removed catalog source released after both holders refresh");
 main=root/"changed.png"; file(main); init(a); check(loads==6,"changed path loads new sheet");
 failedPath=(root/"catalog"/"retry.png").string(); file(failedPath); catalog.Refresh(root/"catalog"); a.LoadRoomAssetCatalog(catalog);
 check(loads==6,"failed upload adds no resource"); failedPath.clear(); a.LoadRoomAssetCatalog(catalog); check(loads==7,"catalog retries failed upload");
 a.Unload(); b.Unload(); a.Unload(); b.Unload(); check(live.empty() && frees==loads && TextureCache::Get().ResidentCount()==0,"all actual renderer ownership released exactly once");
 fs::remove_all(root); std::puts("Tile renderer resource tests passed");
}
