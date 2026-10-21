#include "MetaProgression.h"
#include "DevTools.h"   // MO_ITCH_DEMO
#include "GameBalance.h"
#include "GameTypes.h"
#include "Grimoire.h"
#include "Relic.h"
#include "SaveFiles.h"
#include "SaveIds.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <set>

namespace {
int ClampCount(long long value, int cap) { return (int)std::max(0LL, std::min(value, (long long)cap)); }
int SaturatingAdd(int value, int amount) { return ClampCount((long long)value + amount, MetaProgressionManager::kMaxCurrency); }
bool ValidClass(PlayerClass cls) { return (int)cls >= 0 && (int)cls < (int)PlayerClass::Count; }
bool ValidBiome(Biome biome) { return (int)biome >= 0 && (int)biome < MetaProgressionManager::kBiomeCount; }
const int kMasteryThresholds[] = {5, 15, 35, 65, 100};
const int kMasteryRewards[] = {10, 20, 35, 50, 75};

// "kind/id" death-cause keys: a kind name, a slash and a stable id.
constexpr std::size_t kMaxDeathCauseKeyLength = 2 * SaveFiles::kMaxIdLength;

// Village announcements (VillageUnlockBit) by stable name.
struct VillageNotice { unsigned bit; const char* id; };
const VillageNotice kVillageNotices[] = {
    { MetaProgressionManager::kSeenTrophyHall, "TrophyHall" },
    { MetaProgressionManager::kSeenArmory,     "Armory" },
};

JsonValue NameList(const bool* flags, int count, const char* (*nameOf)(int))
{
    JsonValue list = JsonValue::MakeArray();
    for (int i = 0; i < count; ++i)
        if (flags[i])
            if (const char* name = nameOf(i)) list.Push(JsonValue::MakeString(name));
    return list;
}
const char* RelicName(int index)    { return SaveIdOf((RelicType)index); }
const char* CreatureName(int index) { return BestiarySaveId(index); }
const char* FeatName(int index)     { return SaveIdOf((Feat)index); }
const char* BiomeName(int index)    { return SaveIdOf((Biome)index); }
const char* UnlockName(int index)   { return SaveIdOf((MetaUnlockType)index); }

// Reads a list of names, each listed once; unknown names are the caller's to skip.
template <typename Apply>
void ReadNames(const JsonReader& list, Apply&& apply)
{
    std::set<std::string> seen;
    for (std::size_t i = 0; i < list.Count(); ++i)
    {
        const JsonReader item = list.Item(i);
        const std::string name = item.AsString(SaveFiles::kMaxIdLength);
        if (!seen.insert(name).second) { item.Fail("listed twice"); return; }
        apply(name);
    }
}
}


// ─────────────────────────────────────────────────────────────────────────────
// Unlock table — order must match the MetaUnlockType enum exactly. Saves use
// the stable names in SaveIds.cpp, so reordering no longer breaks a profile,
// but a new unlock needs a new name there too.
// ─────────────────────────────────────────────────────────────────────────────
static const MetaUnlockInfo kUnlockTable[(int)MetaUnlockType::Count] = {
    // name                    description                                    cost  prerequisite
    { "Fireball",            "Adds Fireball to the\nability pool",             25, MetaUnlockType::Count },
    { "Ice Lance",           "Adds Ice Lance to the\nability pool",             25, MetaUnlockType::Count },
    { "Chain Lightning",     "Adds Chain Lightning to\nthe ability pool",        25, MetaUnlockType::Count },
    { "Meteor",              "Adds Meteor to the\nability pool",              50, MetaUnlockType::Count },
    { "Blizzard",            "Adds Blizzard to the\nability pool",              50, MetaUnlockType::Count },
    { "Thunderstorm",        "Adds Thunderstorm to the\nability pool",             50, MetaUnlockType::Count },
    { "Zeph's Stipend I",    "Start every run with\n+30 gold",                 20, MetaUnlockType::Count },
    { "Zeph's Stipend II",   "Start every run with\n+60 gold",                 40, MetaUnlockType::StartingGold1 },
    { "Zeph's Stipend III",  "Start every run with\n+120 gold",                80, MetaUnlockType::StartingGold2 },
    { "Deep Pockets I",      "Keep 20% of your gold\nwhen you die",            35, MetaUnlockType::Count },
    { "Deep Pockets II",     "Keep 40% of your gold\nwhen you die",            70, MetaUnlockType::GoldRetention1 },
    { "Deep Pockets III",    "Keep 60% of your gold\nwhen you die",           140, MetaUnlockType::GoldRetention2 },
    { "Mana Flow I",         "+50% mana regeneration\nevery run",              30, MetaUnlockType::Count },
    { "Mana Flow II",        "+100% mana regeneration\nevery run",             60, MetaUnlockType::ManaFlow1 },
    { "Vitality I",          "+20% max HP at the start\nof every run",         40, MetaUnlockType::Count },
    { "Vitality II",         "+35% max HP at the start\nof every run",         80, MetaUnlockType::Vitality1 },
    { "Arcane Attunement",   "Zeph's first reroll\neach visit is free",       100, MetaUnlockType::Count },
    { "Ability Mastery",     "Begin each run with a\nsecond class ability",   200, MetaUnlockType::FreeShopReroll },
    { "Bulwark",             "+10 Defense at the start\nof every run",         60, MetaUnlockType::Count },
    { "Heirloom",            "Start each run with a\nrandom relic",           120, MetaUnlockType::Count },
    { "Echo Surge",          "+50% Echoes from\nevery kill",             90, MetaUnlockType::Count },
    { "Second Wind",         "Revive once per run\nat 40% health",           160, MetaUnlockType::Count },
    { "Cartographer's Echo", "The dungeon map reveals\nevery room's type",    75, MetaUnlockType::Count },
};

const MetaUnlockInfo& GetMetaUnlockInfo(MetaUnlockType type)
{
    static const MetaUnlockInfo invalid = {"Unavailable", "", 0, MetaUnlockType::Count};
    return (int)type >= 0 && (int)type < (int)MetaUnlockType::Count ? kUnlockTable[(int)type] : invalid;
}

// ── Profile document ──────────────────────────────────────────────────────────
JsonValue MetaProgressionManager::ToJson() const
{
    JsonValue doc = JsonValue::MakeObject();
    doc.Set("schema", JsonValue::MakeInt(SaveFiles::kProfileSchema));
    JsonValue& runs = doc.Set("runs", JsonValue::MakeObject());
    runs.Set("nextId", JsonValue::MakeInt(_nextRunId));
    runs.Set("active", JsonValue::MakeInt(_activeRunId));
    JsonValue& echoes = doc.Set("echoes", JsonValue::MakeObject());
    echoes.Set("banked", JsonValue::MakeInt(_bankedCells));
    echoes.Set("lifetime", JsonValue::MakeInt(_lifetimeCells));
    doc.Set("villageGold", JsonValue::MakeInt(_goldCarryover));
    doc.Set("poeUnlocks", NameList(_unlocked, (int)MetaUnlockType::Count, UnlockName));

    JsonValue& classes = doc.Set("classes", JsonValue::MakeObject());
    for (int i = 0; i < (int)PlayerClass::Count; ++i)
    {
        JsonValue& entry = classes.Set(SaveIdOf((PlayerClass)i), JsonValue::MakeObject());
        entry.Set("rooms", JsonValue::MakeInt(_classRooms[i]));
        entry.Set("bosses", JsonValue::MakeInt(_classBosses[i]));
        entry.Set("masteryRank", JsonValue::MakeInt(_classRank[i]));
        if (const char* starter = _starterChoice[i] >= 0 ? SaveIdOf((AbilityType)_starterChoice[i]) : nullptr)
            entry.Set("starter", JsonValue::MakeString(starter));
    }
    JsonValue& roster = doc.Set("unlockedClasses", JsonValue::MakeArray());
    if (_rogueUnlocked)   roster.Push(JsonValue::MakeString(SaveIdOf(PlayerClass::Rogue)));
    if (_warlockUnlocked) roster.Push(JsonValue::MakeString(SaveIdOf(PlayerClass::Warlock)));
    if (_paladinUnlocked) roster.Push(JsonValue::MakeString(SaveIdOf(PlayerClass::Paladin)));
    doc.Set("onboardingComplete", JsonValue::MakeBool(_onboardingComplete));
    doc.Set("gameCompleted", JsonValue::MakeBool(_gameCompleted));
    JsonValue& ascension = doc.Set("ascension", JsonValue::MakeObject());
    ascension.Set("selected", JsonValue::MakeInt(_selectedAscension));
    ascension.Set("maxUnlocked", JsonValue::MakeInt(_maxAscensionUnlocked));
    doc.Set("domainsCleared", NameList(_biomeCleared, kBiomeCount, BiomeName));
    JsonValue& deaths = doc.Set("deaths", JsonValue::MakeObject());
    deaths.Set("total", JsonValue::MakeInt(_deathTotal));
    JsonValue& byCause = deaths.Set("byCause", JsonValue::MakeObject());
    for (const auto& [key, count] : _deathsByCause) byCause.Set(key, JsonValue::MakeInt(count));
    JsonValue& byDomain = deaths.Set("byDomain", JsonValue::MakeObject());
    for (int i = 0; i < kBiomeCount; ++i)
        if (_deathsByDomain[i] > 0 && BiomeName(i)) byDomain.Set(BiomeName(i), JsonValue::MakeInt(_deathsByDomain[i]));

    JsonValue& grimoire = doc.Set("grimoire", JsonValue::MakeObject());
    grimoire.Set("relics", NameList(_relicFound, kMaxGrimoireEntries, RelicName));
    grimoire.Set("creatures", NameList(_creatureFound, kMaxGrimoireEntries, CreatureName));
    grimoire.Set("feats", NameList(_featEarned, kMaxGrimoireEntries, FeatName));

    JsonValue& village = doc.Set("village", JsonValue::MakeObject());
    JsonValue& announced = village.Set("announced", JsonValue::MakeArray());
    for (const VillageNotice& notice : kVillageNotices)
        if (_villageSeen & notice.bit) announced.Push(JsonValue::MakeString(notice.id));
    if (_hasVillageLayout)   // absent: never laid out, so the authored default is used
    {
        JsonValue& layout = village.Set("layout", JsonValue::MakeArray());
        for (const VillagePlacement& placement : _villagePlacements)
        {
            JsonValue& item = layout.Push(JsonValue::MakeObject());
            item.Set("asset", JsonValue::MakeString(placement.asset));
            item.Set("x", JsonValue::MakeInt(placement.x));
            item.Set("y", JsonValue::MakeInt(placement.y));
        }
    }
    if (_hasClock)   // absent: never played, so the engine's starting hour is used
    {
        JsonValue& clock = village.Set("clock", JsonValue::MakeObject());
        clock.Set("hours", JsonValue::MakeFloat(_clockHours));
    }
    return doc;
}

bool MetaProgressionManager::FromJson(const JsonValue& document, MetaProgressionManager& out, std::string& error)
{
    JsonReadErrors errors;
    const JsonReader root(document, errors);
    MetaProgressionManager next;
    root.Int("schema", SaveFiles::kProfileSchema, SaveFiles::kProfileSchema);

    JsonReader runs;
    if (root.OptionalObject("runs", runs))
    {
        next._nextRunId = runs.Int("nextId", 1, kMaxRunId);
        next._activeRunId = runs.Int("active", 0, next._nextRunId - 1);
    }

    const JsonReader echoes = root.Object("echoes");
    next._bankedCells = (int)echoes.Int("banked", 0, kMaxCurrency);
    next._lifetimeCells = (int)echoes.Int("lifetime", 0, kMaxCurrency);
    next._goldCarryover = (int)root.Int("villageGold", 0, kMaxCurrency);
    ReadSaveIdList<MetaUnlockType>(root.Array("poeUnlocks", SaveFiles::kMaxIdListEntries), SaveFiles::kMaxIdLength,
                                   [&](MetaUnlockType type) { next._unlocked[(int)type] = true; });

    const JsonReader classes = root.Map("classes", SaveFiles::kMaxIdListEntries);
    for (std::size_t i = 0; i < classes.Count(); ++i)
    {
        PlayerClass cls{};
        if (!SaveIdTo(classes.KeyAt(i), cls)) continue;   // a class this build doesn't have
        const JsonReader entry = classes.ValueAt(i);
        const int c = (int)cls;
        next._classRooms[c] = (int)entry.Int("rooms", 0, kMaxRecordedClears);
        next._classBosses[c] = (int)entry.Int("bosses", 0, kMaxRecordedClears);
        next._classRank[c] = (int)entry.Int("masteryRank", 0, kMasteryRanks);
        AbilityType starter{};
        next._starterChoice[c] = SaveIdTo(entry.StringOr("starter", std::string(), SaveFiles::kMaxIdLength), starter)
            ? (int)starter : -1;
    }
    ReadSaveIdList<PlayerClass>(root.Array("unlockedClasses", SaveFiles::kMaxIdListEntries), SaveFiles::kMaxIdLength,
        [&](PlayerClass cls) {
            if (cls == PlayerClass::Rogue)   next._rogueUnlocked = true;
            if (cls == PlayerClass::Warlock) next._warlockUnlocked = true;
            if (cls == PlayerClass::Paladin) next._paladinUnlocked = true;
        });
    next._onboardingComplete = root.Bool("onboardingComplete");
    next._gameCompleted = root.Bool("gameCompleted");
    const JsonReader ascension = root.Object("ascension");
    next._maxAscensionUnlocked = (int)ascension.Int("maxUnlocked", 0, kMaxAscension);
    next._selectedAscension = (int)ascension.Int("selected", 0, next._maxAscensionUnlocked);
    ReadSaveIdList<Biome>(root.Array("domainsCleared", SaveFiles::kMaxIdListEntries), SaveFiles::kMaxIdLength,
                          [&](Biome biome) { next._biomeCleared[(int)biome] = true; });
    JsonReader deaths;
    if (root.OptionalObject("deaths", deaths))
    {
        next._deathTotal = (int)deaths.Int("total", 0, kMaxRecordedClears);
        const JsonReader byCause = deaths.Map("byCause", kMaxDeathCauses);
        for (std::size_t i = 0; i < byCause.Count(); ++i)
        {
            const std::string& key = byCause.KeyAt(i);
            if (key.empty() || key.size() > kMaxDeathCauseKeyLength) { byCause.ValueAt(i).Fail("bad cause name"); break; }
            next._deathsByCause[key] = (int)byCause.ValueAt(i).AsInt(1, next._deathTotal);
        }
        const JsonReader byDomain = deaths.Map("byDomain", SaveFiles::kMaxIdListEntries);
        for (std::size_t i = 0; i < byDomain.Count(); ++i)
        {
            const int count = (int)byDomain.ValueAt(i).AsInt(1, next._deathTotal);
            Biome biome{};
            if (SaveIdTo(byDomain.KeyAt(i), biome)) next._deathsByDomain[(int)biome] = count;
        }
    }

    const JsonReader grimoire = root.Object("grimoire");
    ReadSaveIdList<RelicType>(grimoire.Array("relics", SaveFiles::kMaxIdListEntries), SaveFiles::kMaxIdLength,
        [&](RelicType relic) { if ((int)relic < kMaxGrimoireEntries) next._relicFound[(int)relic] = true; });
    ReadNames(grimoire.Array("creatures", SaveFiles::kMaxIdListEntries), [&](const std::string& name) {
        int index = -1;
        if (BestiaryIndexFromSaveId(name, index) && index < kMaxGrimoireEntries) next._creatureFound[index] = true;
    });
    ReadSaveIdList<Feat>(grimoire.Array("feats", SaveFiles::kMaxIdListEntries), SaveFiles::kMaxIdLength,
        [&](Feat feat) { if ((int)feat < kMaxGrimoireEntries) next._featEarned[(int)feat] = true; });

    const JsonReader village = root.Object("village");
    ReadNames(village.Array("announced", SaveFiles::kMaxIdListEntries), [&](const std::string& name) {
        for (const VillageNotice& notice : kVillageNotices)
            if (name == notice.id) next._villageSeen |= notice.bit;
    });
    JsonReader layout;
    if (village.OptionalArray("layout", kMaxVillagePlacements, layout))
    {
        next._hasVillageLayout = true;
        for (std::size_t i = 0; i < layout.Count(); ++i)
        {
            const JsonReader item = layout.Item(i);
            VillagePlacement placement;
            placement.asset = item.String("asset", SaveFiles::kMaxIdLength);
            if (!IsValidVillageAssetName(placement.asset)) item.Fail("invalid asset name");
            placement.x = (int)item.Int("x", -kMaxVillageCoordinate, kMaxVillageCoordinate);
            placement.y = (int)item.Int("y", -kMaxVillageCoordinate, kMaxVillageCoordinate);
            next._villagePlacements.push_back(placement);
        }
    }
    JsonReader clock;
    if (village.OptionalObject("clock", clock))
    {
        next._hasClock = true;
        next._clockHours = clock.Float("hours", 0.f, Balance::DayNight::kHoursPerDay);
    }

    if (!errors.Ok()) { error = errors.first; return false; }

    // A rank never lags its points, so a later clear can't replay its reward.
    for (int i = 0; i < (int)PlayerClass::Count; ++i)
        while (next._classRank[i] < kMasteryRanks &&
               next.GetClassMasteryProgress((PlayerClass)i) >= kMasteryThresholds[next._classRank[i]])
            ++next._classRank[i];
    // Milestones the profile already meets unlock now; the flags are written
    // back at the next calm save.
    if (next.EvaluateClassMilestones() != 0) next._dirty = true;
    next._newClassUnlocks = 0;   // a load is not an in-game unlock moment
    out = next;
    return true;
}

bool MetaProgressionManager::HasAnyProgress() const
{
    if (_onboardingComplete || _bankedCells > 0 || _lifetimeCells > 0 || _goldCarryover > 0 ||
        _hasVillageLayout || _maxAscensionUnlocked > 0 || _gameCompleted)
        return true;
    for (int i=0;i<(int)PlayerClass::Count;++i)
        if (_classRooms[i] > 0 || _classBosses[i] > 0) return true;
    for (int i=0;i<(int)MetaUnlockType::Count;++i)
        if (_unlocked[i]) return true;
    return false;
}

void MetaProgressionManager::ResetProfile()
{
    // Run ids keep counting, so no checkpoint from before can match a new run.
    const long long nextRunId = _nextRunId;
    *this = MetaProgressionManager();
    _nextRunId = nextRunId;
    Save();
}

std::string MetaProgressionManager::DeathCauseKey(const DamageSource& cause)
{
    return std::string(DamageSourceKindName(cause.kind)) + "/" + (cause.id.empty() ? "unknown" : cause.id);
}

void MetaProgressionManager::RecordDeath(const DamageSource& cause, Biome biome)
{
    if (_deathTotal >= kMaxRecordedClears) return;
    ++_deathTotal;
    const std::string key = DeathCauseKey(cause).substr(0, kMaxDeathCauseKeyLength);
    auto it = _deathsByCause.find(key);
    if (it != _deathsByCause.end()) ++it->second;
    else if ((int)_deathsByCause.size() < kMaxDeathCauses) _deathsByCause[key] = 1;
    if (ValidBiome(biome)) ++_deathsByDomain[(int)biome];
    Save();
}

int MetaProgressionManager::GetDeathsByCause(const std::string& key) const
{
    auto it = _deathsByCause.find(key);
    return it == _deathsByCause.end() ? 0 : it->second;
}

int MetaProgressionManager::GetDeathsInDomain(Biome biome) const
{
    return ValidBiome(biome) ? _deathsByDomain[(int)biome] : 0;
}

long long MetaProgressionManager::BeginRun()
{
    _activeRunId = _nextRunId;
    if (_nextRunId < kMaxRunId) ++_nextRunId;
    Save();
    return _activeRunId;
}

void MetaProgressionManager::EndActiveRun()
{
    if (_activeRunId == 0) return;
    _activeRunId = 0;
    Save();   // on failure the profile stays dirty and the next calm save retries
}

int MetaProgressionManager::GetTotalBossesDefeated() const
{
    long long total = 0;
    for (int i=0;i<(int)PlayerClass::Count;++i) total += _classBosses[i];
    return ClampCount(total, kMaxRecordedClears);
}

int MetaProgressionManager::GetClearedBiomeCount() const
{
    int count = 0;
    for (int i=0;i<kBiomeCount;++i) if (_biomeCleared[i]) ++count;
    return count;
}

unsigned MetaProgressionManager::EvaluateClassMilestones()
{
    unsigned unlocked = 0;
    if (!_paladinUnlocked && (_gameCompleted || GetClearedBiomeCount() >= kPaladinDomainMilestone))
    {
        _paladinUnlocked = true;
        unlocked |= 1u << (int)PlayerClass::Paladin;
    }
    if (!_rogueUnlocked && GetTotalBossesDefeated() >= 1)
    {
        _rogueUnlocked = true;
        unlocked |= 1u << (int)PlayerClass::Rogue;
    }
    if (!_warlockUnlocked && GetClearedBiomeCount() >= kWarlockDomainMilestone)
    {
        _warlockUnlocked = true;
        unlocked |= 1u << (int)PlayerClass::Warlock;
    }
    _newClassUnlocks |= unlocked;
    return unlocked;
}

unsigned MetaProgressionManager::TakeNewClassUnlocks()
{
    const unsigned mask = _newClassUnlocks;
    _newClassUnlocks = 0;
    return mask;
}

void MetaProgressionManager::Load()
{
#if MO_ITCH_DEMO
    return; // Public demo intentionally stays stateless.
#else
    SaveFile file(SaveFiles::ProfilePath(), SaveKind::Profile, SaveFiles::kMaxProfileBytes);
    const SaveLoadResult result = file.Load([this](const JsonValue& document, std::string& error) {
        return FromJson(document, *this, error);
    });
    _loadResult = result;
    _loadProblem = file.LastError();
#endif
}

void MetaProgressionManager::Save() const
{
    // An explicit save can follow any immediate mutation, not only room clears.
    // Keep it pending through every early failure so SaveIfDirty can retry it.
    _dirty = true;
#if MO_ITCH_DEMO
    _dirty = false;
    return; // Preserve the existing stateless demo contract.
#else
    SaveFile file(SaveFiles::ProfilePath(), SaveKind::Profile, SaveFiles::kMaxProfileBytes);
    _lastSaveSucceeded = file.Save(ToJson(), [](const JsonValue& document, std::string& error) {
        MetaProgressionManager check;
        return FromJson(document, check, error);
    });
    _lastSaveProblem = file.LastError();
    if (_lastSaveSucceeded) _dirty = false;
#endif
}

void MetaProgressionManager::SetSelectedAscension(int tier)
{
    if (tier < 0) tier = 0;
    if (tier > _maxAscensionUnlocked) tier = _maxAscensionUnlocked;
    _selectedAscension = tier;
    Save();
}

void MetaProgressionManager::RecordAscensionCleared(int tier)
{
    if (tier < 0 || tier > _maxAscensionUnlocked || tier > kMaxAscension) return;
    // Winning at a tier unlocks the next one.
    int newMax = tier + 1;
    if (newMax > kMaxAscension) newMax = kMaxAscension;
    if (newMax > _maxAscensionUnlocked)
    {
        _maxAscensionUnlocked = newMax;
        Save();
    }
}

void MetaProgressionManager::SetOnboardingComplete(bool complete)
{
    if (_onboardingComplete == complete) return;
    _onboardingComplete = complete;
    Save();
}

void MetaProgressionManager::SetRogueUnlocked(bool unlocked)
{
    if (_rogueUnlocked == unlocked) return;
    _rogueUnlocked = unlocked;
    Save();
}

void MetaProgressionManager::SetWarlockUnlocked(bool unlocked)
{
    if (_warlockUnlocked == unlocked) return;
    _warlockUnlocked = unlocked;
    Save();
}

void MetaProgressionManager::RecordGameCompleted()
{
    if (_gameCompleted) return;
    _gameCompleted = true;
    EvaluateClassMilestones();   // a win always unlocks the Paladin
    Save();
}

// ── Banking ───────────────────────────────────────────────────────────────────
void MetaProgressionManager::BankCells(int amount)
{
    if (amount <= 0) return;
    AddBankedReward(amount);
    Save();
}

// ── Unlock purchases ──────────────────────────────────────────────────────────
bool MetaProgressionManager::IsUnlocked(MetaUnlockType type) const
{
    if (type == MetaUnlockType::Count) return true;   // "no prerequisite" sentinel
    if ((int)type < 0 || (int)type >= (int)MetaUnlockType::Count) return false;
    return _unlocked[(int)type];
}

bool MetaProgressionManager::CanPurchase(MetaUnlockType type) const
{
    if ((int)type < 0 || (int)type >= (int)MetaUnlockType::Count) return false;
    if (_unlocked[(int)type]) return false;

    const MetaUnlockInfo& info = kUnlockTable[(int)type];
    if (!IsUnlocked(info.prerequisite)) return false;
    return _bankedCells >= info.cost;
}

bool MetaProgressionManager::Purchase(MetaUnlockType type)
{
    if (!CanPurchase(type)) return false;

    _bankedCells -= kUnlockTable[(int)type].cost;
    _unlocked[(int)type] = true;
    Save();
    return true;
}

// ── Ability pool gating ───────────────────────────────────────────────────────
bool MetaProgressionManager::IsAbilityUnlocked(AbilityType type) const
{
    switch (type)
    {
    case AbilityType::FireBolt:         return IsUnlocked(MetaUnlockType::UnlockFireBolt);
    case AbilityType::IceBolt:          return IsUnlocked(MetaUnlockType::UnlockIceBolt);
    case AbilityType::ElectricBolt:     return IsUnlocked(MetaUnlockType::UnlockElectricBolt);
    case AbilityType::FireUltimate:     return IsUnlocked(MetaUnlockType::UnlockFireUltimate);
    case AbilityType::IceUltimate:      return IsUnlocked(MetaUnlockType::UnlockIceUltimate);
    case AbilityType::ElectricUltimate: return IsUnlocked(MetaUnlockType::UnlockElectricUltimate);
    default:                            return true;   // spreads always available
    }
}

// ── Permanent bonus queries ───────────────────────────────────────────────────
int MetaProgressionManager::GetStartingGoldBonus() const
{
    if (IsUnlocked(MetaUnlockType::StartingGold3)) return 120;
    if (IsUnlocked(MetaUnlockType::StartingGold2)) return 60;
    if (IsUnlocked(MetaUnlockType::StartingGold1)) return 30;
    return 0;
}

float MetaProgressionManager::GetGoldRetentionPercent() const
{
    if (IsUnlocked(MetaUnlockType::GoldRetention3)) return 0.6f;
    if (IsUnlocked(MetaUnlockType::GoldRetention2)) return 0.4f;
    if (IsUnlocked(MetaUnlockType::GoldRetention1)) return 0.2f;
    return 0.f;
}

int MetaProgressionManager::GetGoldKeptOnDeath(int carriedGold) const
{
    if (carriedGold <= 0) return 0;
    // Integer percent so 60% of 5 gold is exactly 3, never 2.9999 -> 2.
    const int percent = (int)std::lround(GetGoldRetentionPercent() * 100.f);
    return (int)(((long long)carriedGold * percent) / 100);
}

float MetaProgressionManager::GetManaRegenMultiplier() const
{
    if (IsUnlocked(MetaUnlockType::ManaFlow2)) return 2.0f;
    if (IsUnlocked(MetaUnlockType::ManaFlow1)) return 1.5f;
    return 1.0f;
}

float MetaProgressionManager::GetStartingDefenseBonus() const
{
    return IsUnlocked(MetaUnlockType::Bulwark) ? Balance::Stats::kDefenseMajor : 0.f;
}

int MetaProgressionManager::GetVitalityBonus(int baseMaxHealth) const
{
    // Proportional, so a 60-HP Rogue and a 100-HP Warrior gain the same share
    // (a flat +40 would be +67% for the Rogue and +40% for the Warrior).
    int percent = 0;
    if (IsUnlocked(MetaUnlockType::Vitality2))      percent = kVitalityPercent2;
    else if (IsUnlocked(MetaUnlockType::Vitality1)) percent = kVitalityPercent1;
    if (percent <= 0 || baseMaxHealth <= 0) return 0;
    return std::max(1, (baseMaxHealth * percent + 99) / 100);   // round up, at least +1
}

int MetaProgressionManager::GetEchoesKeptOnDeath(int carriedEchoes)
{
    // Poe salvages half of what a fallen Mystic carried (rounded down), so a
    // new player's first runs still move meta progression forward; banking at
    // Zeph's camps keeps all of it.
    return carriedEchoes > 0 ? (carriedEchoes * kEchoSalvagePercent) / 100 : 0;
}

// ── Village wallet ────────────────────────────────────────────────────────────
void MetaProgressionManager::SetVillageWallet(int gold)
{
    const int clamped = ClampCount(gold, kMaxCurrency);
    if (clamped == _goldCarryover) return;   // unchanged wallet: no disk write
    _goldCarryover = clamped;
    Save();
}

// ── Time of day ───────────────────────────────────────────────────────────────
void MetaProgressionManager::SetClockHours(float hours)
{
    if (!std::isfinite(hours) || hours < 0.f || hours >= Balance::DayNight::kHoursPerDay) return;
    if (_hasClock && std::fabs(hours - _clockHours) < kClockSaveStepHours) return;
    _clockHours = hours;
    _hasClock = true;
    _dirty = true;   // written by the next calm save (SaveIfDirty), never on its own
}

bool MetaProgressionManager::IsValidVillageAssetName(const std::string& name)
{
    if (name.empty() || name.size() > 64) return false;
    for (unsigned char c : name)
        if (!(std::isalnum(c) || c == '_' || c == '-' || c == '.')) return false;
    return true;
}

AbilityType MetaProgressionManager::GetStarterChoice(PlayerClass cls) const
{
    if (!ValidClass(cls) || _starterChoice[(int)cls] < 0) return AbilityType::None;
    return (AbilityType)_starterChoice[(int)cls];
}

void MetaProgressionManager::SetStarterChoice(PlayerClass cls, AbilityType ability)
{
    if (!ValidClass(cls)) return;
    const int value = (ability == AbilityType::None || (int)ability < 0 || (int)ability >= (int)AbilityType::Count)
        ? -1 : (int)ability;
    if (_starterChoice[(int)cls] == value) return;
    _starterChoice[(int)cls] = value;
    Save();
}

void MetaProgressionManager::MarkVillageUnlockSeen(unsigned bit)
{
    if ((_villageSeen & bit) == bit) return;
    _villageSeen |= bit;
    Save();
}

void MetaProgressionManager::SetVillageState(const std::vector<VillagePlacement>& placements, int wallet)
{
    std::vector<VillagePlacement> clean;
    for (VillagePlacement p : placements)
    {
        if (!IsValidVillageAssetName(p.asset) || (int)clean.size() >= kMaxVillagePlacements) continue;
        p.x = std::clamp(p.x, -kMaxVillageCoordinate, kMaxVillageCoordinate);
        p.y = std::clamp(p.y, -kMaxVillageCoordinate, kMaxVillageCoordinate);
        clean.push_back(p);
    }
    _villagePlacements = std::move(clean);
    _hasVillageLayout = true;
    _goldCarryover = ClampCount(wallet, kMaxCurrency);
    Save();   // one write: buildings and the gold they cost always agree on disk
}

void MetaProgressionManager::AddBankedReward(int amount)
{
    if (amount<=0) return;
    _bankedCells=SaturatingAdd(_bankedCells,amount);
    _lifetimeCells=SaturatingAdd(_lifetimeCells,amount);
}
int MetaProgressionManager::GetClassRooms(PlayerClass cls) const { return ValidClass(cls) ? _classRooms[(int)cls] : 0; }
int MetaProgressionManager::GetClassBosses(PlayerClass cls) const { return ValidClass(cls) ? _classBosses[(int)cls] : 0; }
int MetaProgressionManager::GetClassMasteryTier(PlayerClass cls) const { return ValidClass(cls) ? _classRank[(int)cls] : 0; }
int MetaProgressionManager::GetClassMasteryProgress(PlayerClass cls) const {
    return ValidClass(cls) ? std::min(100,GetClassRooms(cls)+5*GetClassBosses(cls)) : 0;
}
int MetaProgressionManager::GetClassMasteryTarget(PlayerClass cls) const {
    return ValidClass(cls) ? kMasteryThresholds[std::min(kMasteryRanks-1,GetClassMasteryTier(cls))] : 0;
}
int MetaProgressionManager::GetMasteryThreshold(int rank) { return rank>=1 && rank<=kMasteryRanks ? kMasteryThresholds[rank-1] : 0; }
int MetaProgressionManager::GetMasteryReward(int rank) { return rank>=1 && rank<=kMasteryRanks ? kMasteryRewards[rank-1] : 0; }
int MetaProgressionManager::GetBiomeFirstClearReward(Biome biome) { return ValidBiome(biome) ? 25 : 0; }
bool MetaProgressionManager::HasClearedBiome(Biome biome) const { return ValidBiome(biome) && _biomeCleared[(int)biome]; }
int MetaProgressionManager::AwardMastery(PlayerClass cls) {
    int& rank=_classRank[(int)cls]; int reward=0;
    while(rank<kMasteryRanks && GetClassMasteryProgress(cls)>=kMasteryThresholds[rank]) reward+=kMasteryRewards[rank++];
    return reward;
}
int MetaProgressionManager::RecordRoomCleared(PlayerClass cls) {
    if (!ValidClass(cls)) return 0;
    int& rooms=_classRooms[(int)cls]; if (rooms<kMaxRecordedClears) ++rooms;
    int reward=AwardMastery(cls); AddBankedReward(reward);
    if (reward>0) Save(); else _dirty=true;
    return reward;
}
int MetaProgressionManager::RecordBossDefeated(PlayerClass cls, Biome biome) {
    if (!ValidClass(cls) || !ValidBiome(biome)) return 0;
    int& bosses=_classBosses[(int)cls]; if (bosses<kMaxRecordedClears) ++bosses;
    int reward=AwardMastery(cls);
    if (!_biomeCleared[(int)biome]) { _biomeCleared[(int)biome]=true; reward+=GetBiomeFirstClearReward(biome); }
    AddBankedReward(reward);
    EvaluateClassMilestones();   // persisted by the same save as the clear itself
    Save(); return reward;
}
