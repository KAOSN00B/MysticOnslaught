// Relic choice health: no damage relic is strictly dominated by another of the
// same rarity across representative fight situations, relic bonuses stay
// additive and bounded, descriptions match the implemented numbers, and offers
// never include relics a class cannot use.
//
// Build: tools\run_tests.ps1 -Suite RelicBalance

#include "Relic.h"
#include "PlayerClass.h"
#include "GameBalance.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

static int g_checks = 0;
#define CHECK(x) do { ++g_checks; if (!(x)) { std::printf("FAIL line %d: %s\n", __LINE__, #x); std::exit(1); } } while (0)

struct Scenario { const char* name; RelicHitContext hit; };

static float Bonus(RelicType only, const RelicHitContext& hit)
{
    bool owned[(int)RelicType::Count] = {};
    owned[(int)only] = true;
    return RelicDamageBonus(owned, hit);
}

int main()
{
    // Representative moments of a fight. Values are the relic context only;
    // statuses are off unless the scenario is about them.
    std::vector<Scenario> scenarios;
    { RelicHitContext h; h.targetHpFraction = 1.f;  scenarios.push_back({ "opening hit",        h }); }
    { RelicHitContext h; h.targetHpFraction = 0.6f; h.playerMoving = true; scenarios.push_back({ "kiting mid-fight", h }); }
    { RelicHitContext h; h.targetHpFraction = 0.6f; scenarios.push_back({ "planted boss trade", h }); }
    { RelicHitContext h; h.targetHpFraction = 0.6f; h.bloodlustActive = true; scenarios.push_back({ "crowd kill chain", h }); }
    { RelicHitContext h; h.targetHpFraction = 0.2f; scenarios.push_back({ "finishing blow",     h }); }
    { RelicHitContext h; h.targetHpFraction = 0.6f; h.playerHpFraction = 0.3f; scenarios.push_back({ "low health",  h }); }
    { RelicHitContext h; h.targetHpFraction = 0.6f; h.earlyInFight = true; scenarios.push_back({ "first seconds",  h }); }

    // 1. The reported dominations are gone: for every pair of same-rarity
    //    conditional damage relics, each one wins at least one scenario.
    const RelicType commons[] = { RelicType::KeenEdge, RelicType::Bloodlust, RelicType::Momentum,
                                  RelicType::Ambush, RelicType::Bully };
    for (RelicType a : commons)
        for (RelicType b : commons)
        {
            if (a == b) continue;
            bool aWins = false;
            for (const Scenario& s : scenarios)
                if (Bonus(a, s.hit) > Bonus(b, s.hit) + 1e-4f) aWins = true;
            if (!aWins) std::printf("  %s never beats %s\n", GetRelicInfo(a).name, GetRelicInfo(b).name);
            CHECK(aWins);
        }
    // Swift Boots vs Momentum: Momentum is the damage pick, Swift Boots the
    // mobility pick (more speed + faster dash), so neither dominates.
    CHECK(kSwiftBootsSpeedMult > 1.10f);
    CHECK(kSwiftBootsDashCooldownMult < 1.f);
    { RelicHitContext moving; moving.playerMoving = true; moving.targetHpFraction = 0.6f;
      CHECK(Bonus(RelicType::Momentum, moving) > Bonus(RelicType::SwiftBoots, moving)); }
    // No common relic is a flat unconditional damage boost any more.
    for (RelicType c : commons)
    {
        bool zeroSomewhere = false;
        for (const Scenario& s : scenarios) if (Bonus(c, s.hit) == 0.f) zeroSomewhere = true;
        CHECK(zeroSomewhere);
    }

    // 2. Opener vs finisher are mutually exclusive on one hit (no double dip).
    {
        bool owned[(int)RelicType::Count] = {};
        owned[(int)RelicType::KeenEdge] = owned[(int)RelicType::Executioner] = true;
        for (float hp : { 1.f, 0.6f, 0.25f, 0.1f })
        {
            RelicHitContext h; h.targetHpFraction = hp;
            CHECK(RelicDamageBonus(owned, h) <= 0.35f + 1e-4f);
        }
    }

    // 3. Stacking is additive and bounded. No single relic is worth more than
    //    +40% in its best moment, and a run's worth of relics (8, the strongest
    //    ones, every condition true at once) stays under +300% before crits and
    //    status combos. Owning all 60 at once is impossible in a run, but even
    //    that stays finite and under +450%.
    {
        RelicHitContext all;
        all.targetFrozen = all.targetCharged = all.targetBurning = true;
        all.targetHpFraction = 1.f; all.playerHpFraction = 0.1f;
        all.playerMoving = all.bloodlustActive = true;
        all.targetEliteOrBoss = all.earlyInFight = all.lastEnemy = true;
        all.recentlyDashed = all.recentlyHurt = true;
        all.playerGold = 100000;
        RelicHitContext finisher = all;
        finisher.targetHpFraction = 0.2f;   // Executioner and Bully need a wounded target
        std::vector<float> best;
        for (int i = 0; i < (int)RelicType::Count; ++i)
        {
            const float b = std::max(Bonus((RelicType)i, all), Bonus((RelicType)i, finisher));
            CHECK(b <= 0.40f + 1e-4f);
            best.push_back(b);
        }
        std::sort(best.begin(), best.end(), [](float a, float b) { return a > b; });
        float runWorth = 0.f;
        for (int i = 0; i < 8; ++i) runWorth += best[(size_t)i];
        CHECK(runWorth < 3.0f);
        bool owned[(int)RelicType::Count];
        for (bool& o : owned) o = true;
        const float worst = std::max(RelicDamageBonus(owned, all), RelicDamageBonus(owned, finisher));
        CHECK(worst > 0.f && worst < 4.5f);
        std::printf("  strongest 8 relics, every condition: +%.0f%%; all %d at once: +%.0f%%\n",
                    runWorth * 100.f, (int)RelicType::Count, worst * 100.f);
    }

    // 3b. The expansion's situational relics pay exactly what they say, and
    //     only in their situation.
    {
        RelicHitContext none; none.targetHpFraction = 0.6f;
        for (RelicType t : { RelicType::GiantSlayer, RelicType::Ambush, RelicType::Duelist, RelicType::Bully,
                             RelicType::Afterimage, RelicType::Retaliation, RelicType::GildedGauntlet })
            CHECK(Bonus(t, none) == 0.f);
        { RelicHitContext h = none; h.targetEliteOrBoss = true; CHECK(std::fabs(Bonus(RelicType::GiantSlayer, h) - 0.25f) < 1e-4f); }
        { RelicHitContext h = none; h.earlyInFight = true;      CHECK(std::fabs(Bonus(RelicType::Ambush, h) - 0.40f) < 1e-4f); }
        { RelicHitContext h = none; h.lastEnemy = true;         CHECK(std::fabs(Bonus(RelicType::Duelist, h) - 0.30f) < 1e-4f); }
        { RelicHitContext h = none; h.targetHpFraction = 0.49f; CHECK(std::fabs(Bonus(RelicType::Bully, h) - 0.20f) < 1e-4f); }
        { RelicHitContext h = none; h.targetHpFraction = 0.5f;  CHECK(Bonus(RelicType::Bully, h) == 0.f); }
        { RelicHitContext h = none; h.recentlyDashed = true;    CHECK(std::fabs(Bonus(RelicType::Afterimage, h) - 0.40f) < 1e-4f); }
        { RelicHitContext h = none; h.recentlyHurt = true;      CHECK(std::fabs(Bonus(RelicType::Retaliation, h) - 0.30f) < 1e-4f); }
        { RelicHitContext h = none; h.playerGold = 99;   CHECK(Bonus(RelicType::GildedGauntlet, h) == 0.f); }
        { RelicHitContext h = none; h.playerGold = 250;  CHECK(std::fabs(Bonus(RelicType::GildedGauntlet, h) - 0.10f) < 1e-4f); }
        { RelicHitContext h = none; h.playerGold = 5000; CHECK(std::fabs(Bonus(RelicType::GildedGauntlet, h) - 0.30f) < 1e-4f); }
        CHECK(std::fabs(kAmbushWindow - 4.f) < 1e-4f && std::fabs(kAfterimageWindow - 1.f) < 1e-4f);
        CHECK(std::fabs(kRetaliationDuration - 3.f) < 1e-4f && kChainReactionChancePercent == 25);
        CHECK(std::fabs(kHourglassCooldownRate - 1.25f) < 1e-4f);
    }

    // 4. Descriptions state the implemented numbers.
    auto says = [](RelicType t, const char* text) { return std::strstr(GetRelicInfo(t).description, text) != nullptr; };
    CHECK(says(RelicType::KeenEdge, "+35%") && says(RelicType::KeenEdge, "full health"));
    CHECK(says(RelicType::Bloodlust, "+25%") && says(RelicType::Bloodlust, "4 seconds"));
    CHECK(std::fabs(kBloodlustDuration - 4.f) < 1e-4f);
    CHECK(says(RelicType::SwiftBoots, "+12%") && says(RelicType::SwiftBoots, "20% faster"));
    CHECK(std::fabs(kSwiftBootsSpeedMult - 1.12f) < 1e-4f && std::fabs(kSwiftBootsDashCooldownMult - 0.8f) < 1e-4f);
    CHECK(says(RelicType::Momentum, "+15%") && says(RelicType::Momentum, "on the move"));
    CHECK(says(RelicType::GiantSlayer, "+25%") && says(RelicType::Ambush, "+40%") && says(RelicType::Ambush, "4 seconds"));
    CHECK(says(RelicType::Duelist, "+30%") && says(RelicType::Bully, "+20%") && says(RelicType::Bully, "half health"));
    CHECK(says(RelicType::Afterimage, "+40%") && says(RelicType::Afterimage, "1 second"));
    CHECK(says(RelicType::Retaliation, "+30%") && says(RelicType::Retaliation, "3 seconds"));
    CHECK(says(RelicType::GildedGauntlet, "+5%") && says(RelicType::GildedGauntlet, "max +30%"));
    CHECK(says(RelicType::ChainReaction, "25%") && says(RelicType::Hourglass, "25% faster"));
    CHECK(says(RelicType::Stoneheart, "+40% max HP") && says(RelicType::Stoneheart, "-10% move speed"));
    CHECK(std::fabs(kStoneheartHealthMult - 1.40f) < 1e-4f && std::fabs(kStoneheartSpeedMult - 0.90f) < 1e-4f);
    CHECK(says(RelicType::Deadeye, "+20% crit chance") && std::fabs(Balance::Stats::kDeadeyeCritChance - 0.20f) < 1e-4f);
    CHECK(says(RelicType::StoneSkin, "+10 Defense") && says(RelicType::Bulwark, "+10 Defense") &&
          std::fabs(Balance::Stats::kDefenseMajor - 10.f) < 1e-4f);
    CHECK(says(RelicType::Ironclad, "+3") && says(RelicType::Ironclad, "max +15") &&
          std::fabs(Balance::Stats::kIroncladDefensePerRoom - 3.f) < 1e-4f &&
          std::fabs(Balance::Stats::kIroncladDefenseMax - 15.f) < 1e-4f);
    // Names are unique (the HUD, toasts and the forge refer to relics by name).
    for (int i = 0; i < (int)RelicType::Count; ++i)
        for (int j = i + 1; j < (int)RelicType::Count; ++j)
            CHECK(std::strcmp(GetRelicInfo((RelicType)i).name, GetRelicInfo((RelicType)j).name) != 0);
    // Card text fits: at most two lines of 26 characters.
    for (int i = 0; i < (int)RelicType::Count; ++i)
    {
        const std::string d = GetRelicInfo((RelicType)i).description;
        const size_t nl = d.find('\n');
        CHECK(d.find('\n', nl == std::string::npos ? d.size() : nl + 1) == std::string::npos);
        const std::string first = d.substr(0, nl), second = nl == std::string::npos ? "" : d.substr(nl + 1);
        if (first.size() > 26 || second.size() > 26) std::printf("  too long: %s\n", GetRelicInfo((RelicType)i).name);
        CHECK(first.size() <= 26 && second.size() <= 26);
    }
    CHECK(kMomentumWindow >= 0.3f);   // long enough to cover a rooted swing or cast after moving
    { RelicHitContext h; h.targetHpFraction = 1.f; CHECK(std::fabs(Bonus(RelicType::KeenEdge, h) - 0.35f) < 1e-4f); }
    { RelicHitContext h; h.bloodlustActive = true; CHECK(std::fabs(Bonus(RelicType::Bloodlust, h) - 0.25f) < 1e-4f); }
    { RelicHitContext h; h.playerMoving = true; CHECK(std::fabs(Bonus(RelicType::Momentum, h) - 0.15f) < 1e-4f); }
    for (int i = 0; i < (int)RelicType::Count; ++i)
    {
        const RelicInfo& info = GetRelicInfo((RelicType)i);
        CHECK(info.name != nullptr && info.name[0] != 0);
        CHECK(info.description != nullptr && info.description[0] != 0);
        CHECK(std::strstr(info.description, "all damage") == nullptr || (RelicType)i == RelicType::GlassCannon);
    }

    // 5. Class fit: no dead elemental offers, and every class still has a full
    //    3-card choice available for many picks.
    for (int c = 0; c < (int)PlayerClass::Count; ++c)
    {
        const PlayerClass cls = (PlayerClass)c;
        int fitting = 0;
        for (int i = 0; i < (int)RelicType::Count; ++i) if (RelicFitsClass((RelicType)i, cls)) ++fitting;
        CHECK(fitting >= 15);
        // Every class gets at least two relics of its own (class-mechanic or
        // status relics only some classes can use).
        int classOnly = 0;
        for (int i = 0; i < (int)RelicType::Count; ++i)
        {
            const RelicType t = (RelicType)i;
            int fits = 0;
            for (int o = 0; o < (int)PlayerClass::Count; ++o) fits += RelicFitsClass(t, (PlayerClass)o) ? 1 : 0;
            if (RelicFitsClass(t, cls) && fits < (int)PlayerClass::Count) ++classOnly;
        }
        CHECK(classOnly >= 2);
    }
    CHECK((int)RelicType::Count >= 60);
    CHECK(RelicFitsClass(RelicType::WarlordsCrest, PlayerClass::Warrior) && !RelicFitsClass(RelicType::WarlordsCrest, PlayerClass::Mage));
    CHECK(RelicFitsClass(RelicType::KindlingCore, PlayerClass::Mage) && !RelicFitsClass(RelicType::KindlingCore, PlayerClass::Warlock));
    CHECK(RelicFitsClass(RelicType::VenomGland, PlayerClass::Hunter) && !RelicFitsClass(RelicType::VenomGland, PlayerClass::Paladin));
    CHECK(RelicFitsClass(RelicType::BloodChalice, PlayerClass::Warlock) && !RelicFitsClass(RelicType::BloodChalice, PlayerClass::Mage));
    CHECK(RelicFitsClass(RelicType::AttunementStone, PlayerClass::Paladin) && !RelicFitsClass(RelicType::AttunementStone, PlayerClass::Hunter));
    CHECK(!RelicFitsClass(RelicType::SpellbladeSigil, PlayerClass::Mage) && RelicFitsClass(RelicType::SpellbladeSigil, PlayerClass::Hunter));
    CHECK(RelicFitsClass(RelicType::PyreHeart, PlayerClass::Warlock) && !RelicFitsClass(RelicType::GlacialBattery, PlayerClass::Warlock));
    CHECK(RelicFitsClass(RelicType::Overcharge, PlayerClass::Mage));
    CHECK(!RelicFitsClass(RelicType::Overcharge, PlayerClass::Warrior));
    CHECK(!RelicFitsClass(RelicType::Wildfire, PlayerClass::Hunter));
    CHECK(RelicFitsClass(RelicType::Wildfire, PlayerClass::Warlock));
    CHECK(RelicFitsClass(RelicType::Permafrost, PlayerClass::Paladin));      // class stuns use the freeze state
    CHECK(!RelicFitsClass(RelicType::Permafrost, PlayerClass::Warlock));
    CHECK(RelicFitsClass(RelicType::KeenEdge, PlayerClass::Rogue));

    std::printf("Relic balance tests passed (%d checks): no dominated commons, exclusive opener/finisher, "
                "bounded additive stacking, accurate descriptions, class-fit offers.\n", g_checks);
    return 0;
}
