// Grimoire tables: every creature the game can spawn has one bestiary entry,
// every boss guards a different domain, names are unique, and the feats are
// complete. Build: tools\run_tests.ps1 -Suite Grimoire

#include "Grimoire.h"
#include "Relic.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <set>
#include <string>

static int g_checks = 0;
#define CHECK(x) do { ++g_checks; if (!(x)) { std::printf("FAIL line %d: %s\n", __LINE__, #x); std::exit(1); } } while (0)

int main()
{
    // Every snapshot type Engine::GetDungeonSnapshotType can return maps to an entry.
    const char* spawned[] = {
        "Basic", "Cyclops", "Ogre", "Molarbeast", "SkeletonArcher", "FlameWisp", "SlimeSmall", "SlimeBig",
        "Sporeling", "Shieldbearer", "Phantom", "BomberImp", "Warchief", "LivingBlade", "Werewolf", "ChompBug",
        "Osiris", "TitanGuard", "ToxicVermin", "AncientBear", "AbyssSlime", "PumpkinJack", "Minotaur",
        "Infernal", "Bonechill", "Stormclub", "Venomfang",
    };
    std::set<int> covered;
    for (const char* type : spawned)
    {
        const int index = BestiaryIndexForType(type);
        if (index < 0) std::printf("  no bestiary entry for %s\n", type);
        CHECK(index >= 0);
        covered.insert(index);
    }
    CHECK((int)covered.size() == BestiaryCount());                       // and nothing unreachable
    CHECK(BestiaryIndexForType("Elite_Infernal") == BestiaryIndexForType("Infernal"));
    CHECK(BestiaryIndexForType("SlimeSmall") == BestiaryIndexForType("SlimeBig"));
    CHECK(BestiaryIndexForType("NotAThing") == -1);

    // Ten guardians, one per domain; names unique; every line has text.
    std::set<std::string> names;
    std::set<int> homes;
    int bosses = 0;
    for (int i = 0; i < BestiaryCount(); ++i)
    {
        const BestiaryEntry& e = GetBestiaryEntry(i);
        CHECK(e.name && *e.name && e.role && *e.role && e.lore && *e.lore);
        CHECK(names.insert(e.name).second);
        if (e.boss) { ++bosses; CHECK(homes.insert((int)e.home).second); }
    }
    CHECK(bosses == 10);
    CHECK(GetBestiaryEntry(BestiaryIndexForType("Molarbeast")).home == Biome::DemonsInsides);   // the final boss
    CHECK(GetBestiaryEntry(BestiaryIndexForType("AbyssSlime")).home == Biome::Caverns);

    // Feats: every one named and described, names unique; the thresholds a
    // run can actually reach.
    std::set<std::string> featNames;
    for (int i = 0; i < (int)Feat::Count; ++i)
    {
        const FeatInfo& f = GetFeatInfo((Feat)i);
        CHECK(f.name && *f.name && f.description && *f.description);
        CHECK(featNames.insert(f.name).second);
    }
    CHECK(kRelicSageRelics <= (int)RelicType::Count);
    CHECK(kBeastScholarCreatures <= BestiaryCount());
    CHECK(kCollectorRelics >= 2 && kSwiftVictorySeconds > 0.f);

    std::printf("Grimoire tests passed (%d checks): every creature has a bestiary entry, one guardian per domain, "
                "unique names, complete feats.\n", g_checks);
    return 0;
}
