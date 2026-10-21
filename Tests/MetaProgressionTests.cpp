#include "MetaProgression.h"
#include "GameTypes.h"
#include "Grimoire.h"
#include "Relic.h"
#include "DevTools.h"
#include "Json.h"
#include "SaveFiles.h"
#include "SaveStore.h"
#include "UserDataPath.h"
#include <filesystem>
#include <climits>
#include <cstdio>
#include <fstream>
#include <string>
#include <cstdlib>
#define CHECK(x) do { if (!(x)) { printf("FAIL line %d: %s\n", __LINE__, #x); return 1; } } while (0)

static const std::string kProfile = SaveFiles::ProfilePath();
static void clean() {
    std::error_code e;
    for (const char* suffix : { "", ".bak", ".tmp", ".rejected" }) std::filesystem::remove_all(kProfile + suffix, e);
}
static std::string readAll(const std::string& path) {
    std::ifstream f(path, std::ios::binary); return std::string((std::istreambuf_iterator<char>(f)), {});
}
static void writeAll(const std::string& path, const std::string& bytes) {
    const std::filesystem::path parent = std::filesystem::path(path).parent_path();
    if (!parent.empty()) std::filesystem::create_directories(parent);
    std::ofstream(path, std::ios::binary | std::ios::trunc) << bytes;
}
// Signs a document exactly as the game does: what a buggy build, or someone
// who extracted the key, could write. Content validation must still hold.
static void writeSigned(const JsonValue& document) {
    std::string json; WriteJson(document, json); writeAll(kProfile, SaveFile::Seal(SaveKind::Profile, json));
}
static JsonValue savedDocument() {
    JsonValue doc; std::string error;
    return SaveFile::ReadDocument(kProfile, SaveKind::Profile, SaveFiles::kMaxProfileBytes, doc, error) ? doc : JsonValue();
}
static bool listHas(const JsonValue& list, const char* id) {
    for (std::size_t i = 0; list.IsArray() && i < list.Size(); ++i) if (list.At(i).GetString() == id) return true;
    return false;
}
// A valid profile with one member of one section replaced.
static JsonValue withField(const char* section, const char* key, JsonValue value) {
    MetaProgressionManager base; base.BankCells(5);
    JsonValue doc = base.ToJson();
    if (!section) { doc.Set(key, value); return doc; }
    JsonValue part = *doc.Find(section); part.Set(key, value); doc.Set(section, part); return doc;
}
static bool loadsAsDefault(const JsonValue& document) {
    clean(); writeSigned(document);
    MetaProgressionManager m; m.Load();
    return m.GetLoadResult() == SaveLoadResult::Rejected && m.GetBankedCells() == 0 && std::filesystem::exists(kProfile + ".rejected");
}
static bool loadsWithBank(const JsonValue& document, int bank) {
    clean(); writeSigned(document);
    MetaProgressionManager m; m.Load();
    return m.GetLoadResult() == SaveLoadResult::Loaded && m.GetBankedCells() == bank;
}
int main() {
 clean();
#if MO_ITCH_DEMO
 { MetaProgressionManager seed; seed.BankCells(777); writeSigned(seed.ToJson()); }
 const std::string before = readAll(kProfile);
 MetaProgressionManager demo; demo.Load(); CHECK(demo.GetBankedCells()==0); CHECK(!demo.IsRogueUnlocked());
 demo.BankCells(8); demo.RecordBossDefeated(PlayerClass::Mage,Biome::Forest);
 CHECK(demo.GetBankedCells()==43);
 MetaProgressionManager fresh; fresh.Load(); CHECK(fresh.GetBankedCells()==0);
 CHECK(readAll(kProfile)==before);
 clean(); puts("PASS: demo ignores existing profile and writes no progression"); return 0;
#elif defined(_WIN32) && !MO_DEV_TOOLS
 CHECK(std::filesystem::path(kProfile).is_absolute());
 CHECK(std::filesystem::path(kProfile).parent_path().filename()=="Saves");
 writeAll("metaprogress.cfg", "banked_cells=777\n");
 MetaProgressionManager installed; installed.Load(); CHECK(installed.GetBankedCells()==0);
 installed.BankCells(18); installed.BankCells(5); CHECK(installed.DidLastSaveSucceed());
 CHECK(std::filesystem::exists(kProfile)); CHECK(std::filesystem::exists(kProfile+".bak"));
 MetaProgressionManager installedReload; installedReload.Load(); CHECK(installedReload.GetBankedCells()==23);
 writeAll(kProfile, "garbage");
 MetaProgressionManager installedRecovery; installedRecovery.Load(); CHECK(installedRecovery.GetBankedCells()==18);
 CHECK(installedRecovery.GetLoadResult()==SaveLoadResult::RestoredBackup);
 CHECK(readAll("metaprogress.cfg")=="banked_cells=777\n");
 std::remove("metaprogress.cfg"); clean();
 puts("PASS: installed profile path in Saves/, no local-profile import or overwrite, per-user backup and repair");
#else
 MetaProgressionManager p;
 p.BankCells(INT_MAX); p.BankCells(1); CHECK(p.GetBankedCells() == 1000000000);
 CHECK(!p.Purchase(static_cast<MetaUnlockType>(-1)));
 CHECK(!p.IsUnlocked(static_cast<MetaUnlockType>(999)));
 p.RecordAscensionCleared(INT_MAX); CHECK(p.GetMaxAscensionUnlocked() == 0);
 clean(); p = MetaProgressionManager();
 CHECK(p.RecordRoomCleared(static_cast<PlayerClass>(-1)) == 0);
 CHECK(p.RecordBossDefeated(PlayerClass::Mage, static_cast<Biome>(999)) == 0);
 CHECK(p.GetClassRooms(PlayerClass::Mage) == 0);
 for(int i=0;i<4;++i) CHECK(p.RecordRoomCleared(PlayerClass::Mage) == 0);
 CHECK(p.RecordRoomCleared(PlayerClass::Mage) == 10);
 CHECK(p.GetClassMasteryTier(PlayerClass::Mage) == 1);
 CHECK(p.RecordBossDefeated(PlayerClass::Mage, Biome::Forest) == 25);
 CHECK(p.RecordBossDefeated(PlayerClass::Mage, Biome::Forest) == 20);
 CHECK(p.RecordBossDefeated(PlayerClass::Warrior, Biome::Forest) == 10);
 CHECK(p.GetBankedCells() == 65);
 CHECK(p.HasClearedBiome(Biome::Forest));
 MetaProgressionManager loaded; loaded.Load();
 CHECK(loaded.GetLoadResult() == SaveLoadResult::Loaded && loaded.GetLoadProblem().empty());
 CHECK(loaded.GetBankedCells() == 65); CHECK(loaded.GetClassBosses(PlayerClass::Mage) == 2);
 CHECK(loaded.GetClassMasteryTier(PlayerClass::Warrior) == 1);
 CHECK(loaded.RecordBossDefeated(PlayerClass::Hunter, Biome::Forest) == 10);
 for(int i=0;i<120;++i) loaded.RecordRoomCleared(PlayerClass::Mage);
 CHECK(loaded.GetClassMasteryTier(PlayerClass::Mage) == 5);
 const int bank = loaded.GetBankedCells();
 for(int i=0;i<5;++i) CHECK(loaded.RecordRoomCleared(PlayerClass::Mage) == 0);
 CHECK(loaded.GetBankedCells() == bank);
 loaded.BankCells(7); // backup contains the pre-deposit profile
 writeAll(kProfile, "{\"schema\":1}");                     // not a signed save at all
 MetaProgressionManager recovered; recovered.Load(); CHECK(recovered.GetBankedCells() == bank);
 CHECK(recovered.GetLoadResult() == SaveLoadResult::RestoredBackup);
 CHECK(readAll(kProfile+".rejected") == "{\"schema\":1}");
 recovered.BankCells(3); // never replace valid backup with corrupt primary
 MetaProgressionManager savedAgain; savedAgain.Load(); CHECK(savedAgain.GetBankedCells() == bank+3);
 clean();

 // Clean slate: the old key/value profile is neither read nor touched.
 writeAll("metaprogress.cfg", "banked_cells=80\nlifetime_cells=120\nunlock_0=1\nrogue_unlocked=1\n");
 {
     MetaProgressionManager slate; slate.Load();
     CHECK(slate.GetLoadResult()==SaveLoadResult::Missing && !slate.HasAnyProgress() && !slate.IsRogueUnlocked());
     slate.BankCells(4);
     CHECK(readAll("metaprogress.cfg")=="banked_cells=80\nlifetime_cells=120\nunlock_0=1\nrogue_unlocked=1\n");
     std::remove("metaprogress.cfg");
 }
 clean();

 // The document: stable names, not enum numbers.
 {
     MetaProgressionManager named; named.BankCells(500);
     CHECK(named.Purchase(MetaUnlockType::FreeShopReroll)); CHECK(named.Purchase(MetaUnlockType::BonusStarterAbility));
     named.SetStarterChoice(PlayerClass::Rogue, AbilityType::PoisonVial);
     named.RecordBossDefeated(PlayerClass::Hunter, Biome::Jungle);
     named.MarkRelicFound((int)RelicType::LastStand); named.MarkCreatureFound(11); named.EarnFeat((int)Feat::Collector);
     named.SaveIfDirty();
     const JsonValue doc = savedDocument();
     CHECK(listHas(*doc.Find("poeUnlocks"), "FreeShopReroll") && listHas(*doc.Find("poeUnlocks"), "BonusStarterAbility"));
     CHECK(doc.Find("classes")->Find("Rogue")->Find("starter")->GetString()=="PoisonVial");
     CHECK(listHas(*doc.Find("domainsCleared"), "Jungle") && listHas(*doc.Find("unlockedClasses"), "Rogue"));
     CHECK(listHas(*doc.Find("grimoire")->Find("relics"), "LastStand"));
     CHECK(listHas(*doc.Find("grimoire")->Find("creatures"), "Ogre"));
     CHECK(listHas(*doc.Find("grimoire")->Find("feats"), "Collector"));
     CHECK(readAll(kProfile).find("PoisonVial")==std::string::npos);   // not plain text on disk
 }
 clean();

 // Hostile but correctly signed documents: every rule rejects the whole file.
 CHECK(loadsWithBank(withField(nullptr, "unknownFutureSection", JsonValue::MakeObject()), 5));   // tolerated
 CHECK(loadsAsDefault(withField(nullptr, "schema", JsonValue::MakeInt(2))));
 CHECK(loadsAsDefault(withField("echoes", "banked", JsonValue::MakeInt(-2))));
 CHECK(loadsAsDefault(withField("echoes", "banked", JsonValue::MakeInt(1000000001))));
 CHECK(loadsAsDefault(withField("echoes", "banked", JsonValue::MakeDouble(5.5))));
 CHECK(loadsAsDefault(withField("echoes", "banked", JsonValue::MakeString("5"))));
 CHECK(loadsAsDefault(withField(nullptr, "villageGold", JsonValue::MakeInt(-1))));
 CHECK(loadsAsDefault(withField(nullptr, "onboardingComplete", JsonValue::MakeInt(1))));
 {
     JsonValue twice = JsonValue::MakeArray();
     twice.Push(JsonValue::MakeString("Bulwark")); twice.Push(JsonValue::MakeString("Bulwark"));
     CHECK(loadsAsDefault(withField(nullptr, "poeUnlocks", twice)));
     JsonValue numbers = JsonValue::MakeArray(); numbers.Push(JsonValue::MakeInt(3));
     CHECK(loadsAsDefault(withField(nullptr, "poeUnlocks", numbers)));
     JsonValue unknown = JsonValue::MakeArray(); unknown.Push(JsonValue::MakeString("UnlockLaserEyes"));
     unknown.Push(JsonValue::MakeString("Bulwark"));
     clean(); writeSigned(withField(nullptr, "poeUnlocks", unknown));
     MetaProgressionManager m; m.Load();                                         // removed content is skipped
     CHECK(m.GetLoadResult()==SaveLoadResult::Loaded && m.IsUnlocked(MetaUnlockType::Bulwark));
 }
 {
     JsonValue classes = *MetaProgressionManager().ToJson().Find("classes");
     JsonValue mage = *classes.Find("Mage");
     mage.Set("rooms", JsonValue::MakeInt(2147483647)); classes.Set("Mage", mage);
     CHECK(loadsAsDefault(withField(nullptr, "classes", classes)));
     mage.Set("rooms", JsonValue::MakeInt(1)); mage.Set("masteryRank", JsonValue::MakeInt(999)); classes.Set("Mage", mage);
     CHECK(loadsAsDefault(withField(nullptr, "classes", classes)));
     JsonValue fine = *MetaProgressionManager().ToJson().Find("classes");
     fine.Set("Necromancer", *fine.Find("Mage"));                                 // a class this build lacks
     CHECK(loadsWithBank(withField(nullptr, "classes", fine), 5));
 }
 {
     JsonValue asc = JsonValue::MakeObject();
     asc.Set("selected", JsonValue::MakeInt(3)); asc.Set("maxUnlocked", JsonValue::MakeInt(2));
     CHECK(loadsAsDefault(withField(nullptr, "ascension", asc)));
     asc.Set("selected", JsonValue::MakeInt(7)); asc.Set("maxUnlocked", JsonValue::MakeInt(7));
     CHECK(loadsAsDefault(withField(nullptr, "ascension", asc)));
 }
 {
     JsonValue layout = JsonValue::MakeArray();
     JsonValue building = JsonValue::MakeObject();
     building.Set("asset", JsonValue::MakeString("Gate,=bad")); building.Set("x", JsonValue::MakeInt(1)); building.Set("y", JsonValue::MakeInt(1));
     layout.Push(building);
     CHECK(loadsAsDefault(withField("village", "layout", layout)));
     building.Set("asset", JsonValue::MakeString("Flowers")); building.Set("x", JsonValue::MakeInt(1000000)); layout = JsonValue::MakeArray(); layout.Push(building);
     CHECK(loadsAsDefault(withField("village", "layout", layout)));
     JsonValue tooMany = JsonValue::MakeArray();
     building.Set("x", JsonValue::MakeInt(1));
     for (int i = 0; i <= MetaProgressionManager::kMaxVillagePlacements; ++i) tooMany.Push(building);
     CHECK(loadsAsDefault(withField("village", "layout", tooMany)));
 }
 {
     JsonValue clock = JsonValue::MakeObject();
     clock.Set("hours", JsonValue::MakeFloat(30.f));
     CHECK(loadsAsDefault(withField("village", "clock", clock)));
     clock.Set("hours", JsonValue::MakeString("noon"));
     CHECK(loadsAsDefault(withField("village", "clock", clock)));
 }
 clean();

 MetaProgressionManager allClasses;
 for(int cls=0;cls<(int)PlayerClass::Count;++cls) {
     CHECK(allClasses.RecordBossDefeated((PlayerClass)cls,Biome::Caverns)==(cls==0 ? 35 : 10));
     CHECK(allClasses.GetClassMasteryProgress((PlayerClass)cls)==5);
     CHECK(allClasses.GetClassMasteryTarget((PlayerClass)cls)==15);
 }
 CHECK(allClasses.GetBankedCells()==85);
 // An edited byte fails the signature and loads the last valid backup.
 allClasses.BankCells(4);
 {std::string bytes = readAll(kProfile); bytes[bytes.size()/2] ^= 0x04; writeAll(kProfile, bytes);}
 MetaProgressionManager signatureRecovery; signatureRecovery.Load(); CHECK(signatureRecovery.GetBankedCells()==85);
 CHECK(signatureRecovery.GetLoadProblem().find("signature mismatch")!=std::string::npos);
 clean();
 std::filesystem::create_directories(kProfile+".tmp");
 MetaProgressionManager denied; denied.BankCells(12); CHECK(!denied.DidLastSaveSucceed());
 CHECK(!denied.GetLastSaveProblem().empty());
 CHECK(denied.GetBankedCells()==12); std::filesystem::remove(kProfile+".tmp");
 denied.SaveIfDirty(); CHECK(denied.DidLastSaveSucceed());
 MetaProgressionManager retry; retry.Load(); CHECK(retry.GetBankedCells()==12);
 clean();
 // Deferred room progress survives a failed calm-moment flush without another clear.
 MetaProgressionManager deferred; CHECK(deferred.RecordRoomCleared(PlayerClass::Hunter)==0);
 CHECK(!std::filesystem::exists(kProfile));
 std::filesystem::create_directories(kProfile+".tmp");
 deferred.SaveIfDirty(); CHECK(!deferred.DidLastSaveSucceed());
 std::filesystem::remove(kProfile+".tmp");
 deferred.SaveIfDirty(); CHECK(deferred.DidLastSaveSucceed());
 MetaProgressionManager deferredReload; deferredReload.Load(); CHECK(deferredReload.GetClassRooms(PlayerClass::Hunter)==1);
 // A successful flush clears dirty: another flush must not touch the blocked disk.
 std::filesystem::create_directories(kProfile+".tmp");
 deferred.SaveIfDirty(); CHECK(deferred.DidLastSaveSucceed());
 std::filesystem::remove(kProfile+".tmp");
 clean();
 // A recovered generation is already the primary again.
 MetaProgressionManager recoverySource; recoverySource.BankCells(18); recoverySource.BankCells(7);
 writeAll(kProfile, "damaged");
 MetaProgressionManager repair; repair.Load(); CHECK(repair.GetBankedCells()==18);
 std::remove((kProfile+".bak").c_str());
 MetaProgressionManager repaired; repaired.Load(); CHECK(repaired.GetBankedCells()==18);
 clean();
 // Backup-copy and primary-replacement failures also keep the save pending.
 MetaProgressionManager backupBlocked; backupBlocked.BankCells(10);
 std::filesystem::create_directories(kProfile+".bak");
 backupBlocked.BankCells(5); CHECK(!backupBlocked.DidLastSaveSucceed());
 MetaProgressionManager stillIntact; stillIntact.Load(); CHECK(stillIntact.GetBankedCells()==10);
 std::filesystem::remove(kProfile+".bak");
 backupBlocked.SaveIfDirty(); CHECK(backupBlocked.DidLastSaveSucceed());
 MetaProgressionManager copyRetried; copyRetried.Load(); CHECK(copyRetried.GetBankedCells()==15);
 clean();
 std::filesystem::create_directories(kProfile);
 MetaProgressionManager replaceBlocked; replaceBlocked.BankCells(9); CHECK(!replaceBlocked.DidLastSaveSucceed());
 std::filesystem::remove(kProfile);
 replaceBlocked.SaveIfDirty(); CHECK(replaceBlocked.DidLastSaveSucceed());
 MetaProgressionManager replaceRetried; replaceRetried.Load(); CHECK(replaceRetried.GetBankedCells()==9);
 clean();
 // The final-boss route and the end-screen fallback can both notify the same win.
 // A duplicate callback must not grant another ascension or lose completion on reload.
 MetaProgressionManager victory;
 for (int tier=0; tier<=6; ++tier) {
     victory.SetSelectedAscension(tier);
     victory.RecordAscensionCleared(tier); victory.RecordGameCompleted();
     victory.RecordAscensionCleared(tier); victory.RecordGameCompleted();
     CHECK(victory.GetMaxAscensionUnlocked()==(tier<6 ? tier+1 : 6));
     CHECK(victory.GetBankedCells()==0);
     MetaProgressionManager victoryReload; victoryReload.Load();
     CHECK(victoryReload.HasCompletedGame());
     CHECK(victoryReload.GetMaxAscensionUnlocked()==(tier<6 ? tier+1 : 6));
     CHECK(victoryReload.GetSelectedAscension()==tier);
     victory=victoryReload;
 }
 clean();
 // Class roster milestones: fresh profiles start with Rogue/Warlock locked;
 // the first boss unlocks Rogue, a third distinct domain unlocks Warlock.
 MetaProgressionManager roster;
 CHECK(!roster.IsRogueUnlocked()); CHECK(!roster.IsWarlockUnlocked()); CHECK(roster.TakeNewClassUnlocks()==0);
 for(int i=0;i<10;++i) roster.RecordRoomCleared(PlayerClass::Mage);
 CHECK(!roster.IsRogueUnlocked());
 roster.RecordBossDefeated(PlayerClass::Mage,Biome::Forest);
 CHECK(roster.IsRogueUnlocked()); CHECK(!roster.IsWarlockUnlocked());
 CHECK(roster.TakeNewClassUnlocks()==(1u<<(int)PlayerClass::Rogue)); CHECK(roster.TakeNewClassUnlocks()==0);
 roster.RecordBossDefeated(PlayerClass::Warrior,Biome::Forest);   // same domain twice is still one domain
 CHECK(!roster.IsWarlockUnlocked());
 roster.RecordBossDefeated(PlayerClass::Warrior,Biome::Caverns);
 CHECK(!roster.IsWarlockUnlocked());
 roster.RecordBossDefeated(PlayerClass::Hunter,Biome::Jungle);
 CHECK(roster.IsWarlockUnlocked()); CHECK(roster.TakeNewClassUnlocks()==(1u<<(int)PlayerClass::Warlock));
 MetaProgressionManager rosterReload; rosterReload.Load();
 CHECK(rosterReload.IsRogueUnlocked()); CHECK(rosterReload.IsWarlockUnlocked()); CHECK(rosterReload.TakeNewClassUnlocks()==0);
 roster.RecordGameCompleted(); CHECK(roster.TakeNewClassUnlocks()==(1u<<(int)PlayerClass::Paladin));
 clean();
 // Milestones already met by a loaded profile unlock on load and are written
 // back at the next calm save (no re-announcement).
 {
     MetaProgressionManager seed;
     seed.RecordBossDefeated(PlayerClass::Warrior, Biome::Caverns); seed.RecordBossDefeated(PlayerClass::Warrior, Biome::Forest);
     seed.RecordBossDefeated(PlayerClass::Warrior, Biome::Jungle);
     JsonValue doc = seed.ToJson(); doc.Set("unlockedClasses", JsonValue::MakeArray());
     clean(); writeSigned(doc);
     MetaProgressionManager retro; retro.Load();
     CHECK(retro.IsRogueUnlocked()); CHECK(retro.IsWarlockUnlocked()); CHECK(retro.TakeNewClassUnlocks()==0);
     retro.SaveIfDirty(); CHECK(retro.DidLastSaveSucceed());
     const JsonValue saved = savedDocument();
     CHECK(listHas(*saved.Find("unlockedClasses"), "Rogue") && listHas(*saved.Find("unlockedClasses"), "Warlock"));
 }
 clean();
 // Deep Pockets: the advertised share of carried gold survives a death.
 MetaProgressionManager pockets; pockets.BankCells(1000);
 CHECK(pockets.GetGoldKeptOnDeath(250)==0);
 CHECK(pockets.Purchase(MetaUnlockType::GoldRetention1)); CHECK(pockets.GetGoldKeptOnDeath(250)==50);
 CHECK(pockets.Purchase(MetaUnlockType::GoldRetention2)); CHECK(pockets.GetGoldKeptOnDeath(250)==100);
 CHECK(pockets.Purchase(MetaUnlockType::GoldRetention3)); CHECK(pockets.GetGoldKeptOnDeath(250)==150);
 CHECK(pockets.GetGoldKeptOnDeath(5)==3); CHECK(pockets.GetGoldKeptOnDeath(-4)==0);
 CHECK(!pockets.HasFreeShopReroll()); CHECK(!pockets.Purchase(MetaUnlockType::BonusStarterAbility));
 CHECK(pockets.Purchase(MetaUnlockType::FreeShopReroll)); CHECK(pockets.HasFreeShopReroll());
 CHECK(pockets.Purchase(MetaUnlockType::BonusStarterAbility)); CHECK(pockets.HasBonusStarterAbility());
 MetaProgressionManager pocketsReload; pocketsReload.Load();
 CHECK(pocketsReload.HasFreeShopReroll() && pocketsReload.HasBonusStarterAbility() && pocketsReload.GetGoldKeptOnDeath(250)==150);
 // Village wallet persists, clamps and skips redundant writes.
 pocketsReload.SetVillageWallet(275); pocketsReload.SetVillageWallet(-9);
 MetaProgressionManager walletReload; walletReload.Load(); CHECK(walletReload.GetVillageWallet()==0);
 walletReload.SetVillageWallet(275);
 MetaProgressionManager walletReload2; walletReload2.Load(); CHECK(walletReload2.GetVillageWallet()==275);
 clean();
 // The time of day goes out with the next calm save, in quarter-hour steps,
 // and is never progress on its own.
 {
     MetaProgressionManager clockSave; clockSave.Load(); CHECK(!clockSave.HasClock());
     clockSave.SetClockHours(21.5f); CHECK(clockSave.HasClock() && clockSave.GetClockHours()==21.5f);
     clockSave.SetClockHours(21.6f); CHECK(clockSave.GetClockHours()==21.5f);   // under a quarter hour
     clockSave.SetClockHours(30.f); CHECK(clockSave.GetClockHours()==21.5f);    // not an hour
     MetaProgressionManager notYet; notYet.Load(); CHECK(!notYet.HasClock());   // no save of its own
     clockSave.SaveIfDirty(); CHECK(clockSave.DidLastSaveSucceed());
     MetaProgressionManager clockReload; clockReload.Load();
     CHECK(clockReload.HasClock() && clockReload.GetClockHours()==21.5f);
     CHECK(!clockReload.HasAnyProgress());
 }
 clean();
 // Continue only exists with real progress; New Game over a profile resets it.
 {
     MetaProgressionManager fresh; CHECK(!fresh.HasAnyProgress());
     MetaProgressionManager played; played.RecordRoomCleared(PlayerClass::Mage); CHECK(played.HasAnyProgress());
     played.BankCells(40); played.SetVillageState({ {"VillageGate",864,0} }, 25);
     played.ResetProfile();
     CHECK(!played.HasAnyProgress()); CHECK(played.GetBankedCells()==0); CHECK(!played.HasVillageLayout());
     MetaProgressionManager reloaded; reloaded.Load(); CHECK(!reloaded.HasAnyProgress());
 }
 clean();
 // Paladin also unlocks after five different domains (no full win required).
 {
     MetaProgressionManager pal;
     const Biome five[] = { Biome::Forest, Biome::Caverns, Biome::Jungle, Biome::Graveyard, Biome::Wastelands };
     for (int i=0;i<4;++i) pal.RecordBossDefeated(PlayerClass::Warrior, five[i]);
     CHECK(!pal.IsPaladinUnlocked());
     pal.TakeNewClassUnlocks();
     pal.RecordBossDefeated(PlayerClass::Warrior, five[4]);
     CHECK(pal.IsPaladinUnlocked());
     CHECK((pal.TakeNewClassUnlocks() & (1u<<(int)PlayerClass::Paladin)) != 0);
     MetaProgressionManager palReload; palReload.Load(); CHECK(palReload.IsPaladinUnlocked());
 }
 clean();
 // Vitality is proportional to the class's base HP (rounded up, at least +1).
 {
     MetaProgressionManager vit; vit.BankCells(500);
     CHECK(vit.GetVitalityBonus(6)==0 && vit.GetVitalityBonus(10)==0);
     CHECK(vit.Purchase(MetaUnlockType::Vitality1));
     CHECK(vit.GetVitalityBonus(6)==2 && vit.GetVitalityBonus(8)==2 && vit.GetVitalityBonus(10)==2);
     CHECK(vit.Purchase(MetaUnlockType::Vitality2));
     CHECK(vit.GetVitalityBonus(6)==3 && vit.GetVitalityBonus(8)==3 && vit.GetVitalityBonus(10)==4);
     CHECK(vit.GetVitalityBonus(0)==0);
 }
 // Poe salvages half the carried Echoes on death.
 CHECK(MetaProgressionManager::GetEchoesKeptOnDeath(0)==0);
 CHECK(MetaProgressionManager::GetEchoesKeptOnDeath(1)==0);
 CHECK(MetaProgressionManager::GetEchoesKeptOnDeath(9)==4);
 CHECK(MetaProgressionManager::GetEchoesKeptOnDeath(10)==5);
 CHECK(MetaProgressionManager::GetEchoesKeptOnDeath(-3)==0);
 clean();
 // Village buildings + wallet are ONE transaction: after a failed write the
 // disk still holds the previous consistent pair, never a building without
 // its cost or a refund without the removal.
 {
     typedef MetaProgressionManager::VillagePlacement VP;
     MetaProgressionManager village; CHECK(!village.HasVillageLayout());
     std::vector<VP> start = { {"VillageGate",864,0}, {"ZephsShop",1190,210} };
     village.SetVillageState(start, 300); CHECK(village.DidLastSaveSucceed());
     MetaProgressionManager v1; v1.Load();
     CHECK(v1.HasVillageLayout()); CHECK(v1.GetVillagePlacements().size()==2);
     CHECK(v1.GetVillagePlacements()[1].asset=="ZephsShop" && v1.GetVillagePlacements()[1].x==1190 && v1.GetVillagePlacements()[1].y==210);
     CHECK(v1.GetVillageWallet()==300);
     // Buy a 150g building, but the profile write fails (blocked temp file).
     std::filesystem::create_directories(kProfile+".tmp");
     std::vector<VP> bought = start; bought.push_back({"PoeAltar",400,600});
     village.SetVillageState(bought, 150); CHECK(!village.DidLastSaveSucceed());
     MetaProgressionManager afterFail; afterFail.Load();
     CHECK(afterFail.GetVillagePlacements().size()==2); CHECK(afterFail.GetVillageWallet()==300);   // old pair, intact
     std::filesystem::remove(kProfile+".tmp");
     village.SaveIfDirty(); CHECK(village.DidLastSaveSucceed());                                    // retried as one unit
     MetaProgressionManager afterRetry; afterRetry.Load();
     CHECK(afterRetry.GetVillagePlacements().size()==3); CHECK(afterRetry.GetVillageWallet()==150);
     // Hostile names/values never reach the file; an empty layout is still "a layout".
     std::vector<VP> hostile = { {"Gate,=bad",1,1}, {"",2,2}, {"Flowers",5,6}, {"Far",9999999,-9999999} };
     village.SetVillageState(hostile, -50); CHECK(village.DidLastSaveSucceed());
     MetaProgressionManager h; h.Load();
     CHECK(h.GetVillagePlacements().size()==2 && h.GetVillagePlacements()[0].asset=="Flowers"); CHECK(h.GetVillageWallet()==0);
     CHECK(h.GetVillagePlacements()[1].x==100000 && h.GetVillagePlacements()[1].y==-100000);
     village.SetVillageState({}, 10);
     MetaProgressionManager empty; empty.Load(); CHECK(empty.HasVillageLayout()); CHECK(empty.GetVillagePlacements().empty());
     // Signed: editing a building out by hand fails the signature -> backup.
     village.SetVillageState(start, 77);
     village.SetVillageState(bought, 27);
     {std::string bytes = readAll(kProfile); bytes[bytes.size()-10] ^= 0x01; writeAll(kProfile, bytes);}
     MetaProgressionManager tampered; tampered.Load();
     CHECK(tampered.GetVillagePlacements().size()==2 && tampered.GetVillageWallet()==77);
 }
 clean();
 // Armory starter choices and village unlock notices persist with the profile.
 {
     MetaProgressionManager a;
     CHECK(a.GetStarterChoice(PlayerClass::Rogue)==AbilityType::None);
     a.SetStarterChoice(PlayerClass::Rogue, AbilityType::PoisonVial);
     a.SetStarterChoice(PlayerClass::Mage, AbilityType::IceSpread);
     a.SetStarterChoice((PlayerClass)99, AbilityType::Smite);              // invalid class ignored
     a.MarkVillageUnlockSeen(MetaProgressionManager::kSeenArmory);
     MetaProgressionManager b; b.Load();
     CHECK(b.GetStarterChoice(PlayerClass::Rogue)==AbilityType::PoisonVial);
     CHECK(b.GetStarterChoice(PlayerClass::Mage)==AbilityType::IceSpread);
     CHECK(b.GetStarterChoice(PlayerClass::Warrior)==AbilityType::None);
     CHECK(b.HasSeenVillageUnlock(MetaProgressionManager::kSeenArmory));
     CHECK(!b.HasSeenVillageUnlock(MetaProgressionManager::kSeenTrophyHall));
     b.SetStarterChoice(PlayerClass::Rogue, AbilityType::None);           // back to the class default
     MetaProgressionManager c; c.Load(); CHECK(c.GetStarterChoice(PlayerClass::Rogue)==AbilityType::None);
     c.ResetProfile();
     CHECK(c.GetStarterChoice(PlayerClass::Mage)==AbilityType::None);
     CHECK(!c.HasSeenVillageUnlock(MetaProgressionManager::kSeenArmory));
 }
 // Death tallies (lifetime, by cause and domain) persist; ResetProfile clears them.
 {
     clean();
     MetaProgressionManager d;
     d.RecordDeath(DamageSource::FromEnemy("Ogre", true, false), Biome::Caverns);
     d.RecordDeath(DamageSource::FromEnemy("Ogre", false, false), Biome::Caverns);
     d.RecordDeath(DamageSource::FromStatus(DamageNames::kBurn, DamageSource::FromEnemy("Infernal", true, false)), Biome::Forest);
     MetaProgressionManager e; e.Load();
     CHECK(e.GetDeathCount()==3 && e.GetDeathsByCause("enemy/Ogre")==2 && e.GetDeathsByCause("status/Burn")==1);
     CHECK(e.GetDeathsInDomain(Biome::Caverns)==2 && e.GetDeathsInDomain(Biome::Forest)==1);
     JsonValue doc = e.ToJson(); JsonValue deaths = *doc.Find("deaths"); JsonValue byCause = *deaths.Find("byCause");
     byCause.Set("enemy/Ogre", JsonValue::MakeInt(9)); deaths.Set("byCause", byCause); doc.Set("deaths", deaths);
     CHECK(loadsAsDefault(doc));                                          // more of one cause than deaths in total
     e.ResetProfile(); CHECK(e.GetDeathCount()==0 && e.GetDeathsByCause("enemy/Ogre")==0);
 }
 clean();
 // Grimoire: found relics and creatures and earned feats persist, each once.
 {
     clean();
     MetaProgressionManager a; a.Load();
     CHECK(a.FoundRelicCount()==0 && a.FoundCreatureCount()==0 && a.FeatCount()==0);
     a.MarkRelicFound(3); a.MarkRelicFound(3); a.MarkRelicFound(59);
     a.MarkRelicFound(-1); a.MarkRelicFound(MetaProgressionManager::kMaxGrimoireEntries);   // ignored
     a.MarkCreatureFound(25);
     CHECK(a.EarnFeat(4)); CHECK(!a.EarnFeat(4));        // only the first time is news
     CHECK(a.FoundRelicCount()==2 && a.HasFoundRelic(59) && !a.HasFoundRelic(4));
     a.SaveIfDirty();                                      // marking only flags the profile dirty
     MetaProgressionManager b; b.Load();
     CHECK(b.FoundRelicCount()==2 && b.HasFoundRelic(3) && b.HasFoundRelic(59));
     CHECK(b.FoundCreatureCount()==1 && b.HasFoundCreature(25));
     CHECK(b.FeatCount()==1 && b.HasFeat(4) && !b.HasFeat(5));
     b.ResetProfile();
     CHECK(b.FoundRelicCount()==0 && b.FoundCreatureCount()==0 && b.FeatCount()==0);
 }
 clean(); puts("PASS: currency, enum validation, finite mastery, first-clear deduplication, round trip, backup recovery, clean slate (old profile ignored), stable names, hostile signed documents, all classes, signature tampering, deferred retries, primary repair, copy/replace failures, repeated victory and ascension persistence, class milestones (live + on load), Deep Pockets, village wallet, village layout+wallet transaction, Armory starter choices, village unlock notices, Grimoire discoveries and feats");
#endif
}
