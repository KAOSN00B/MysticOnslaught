// =============================================================================
// EngineRoomEvents.cpp — the decision-room events beyond the Risk Shrine.
//
// Each event is a Standard room tagged at generation (RollDungeonRoomSpecials)
// with a RoomSpecialType and three option indices, so re-entering the room or
// resuming a saved run shows the same offer. The player walks to the shrine,
// takes one card (or walks away and comes back later), and the room is spent.
//
//   Cursed Cache   a reward now, a curse on the next combat rooms
//   Omen Stone     a two-sided modifier for the next combat room
//   Locked Armory  pay gold to open one of three cases
//   Mutation Altar a permanent build trade for the rest of the run
//   Relic Forge    salvage or reforge your newest relic, or temper them all
//   Rest Site      rest, train or fortify (from the second domain on)
//
// Curses and omens reuse the Risk Shrine's RunModifier levers (enemy HP and
// damage, gold and XP), so every downside has a visible tell in the room.
// =============================================================================

#include "Engine.h"
#include "RoomEvents.h"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <string>

const RoomEventPresentation& GetRoomEventPresentation(RoomSpecialType type)
{
    static const RoomEventPresentation kTable[] = {
        { "",               "",                                   "",               Color{ 240, 176,  80, 255 } },   // None
        { "RISK SHRINE",    "Bind a contract, or walk on",        "Risk Shrine",    Color{ 240, 176,  80, 255 } },
        { "CURSED CACHE",   "Take it now, pay for it later",      "Cursed Cache",   Color{ 205,  95, 215, 255 } },
        { "OMEN STONE",     "Choose what the next room becomes",  "Omen Stone",     Color{ 120, 175, 255, 255 } },
        { "LOCKED ARMORY",  "Gold opens one case",                "Locked Armory",  Color{ 235, 205, 110, 255 } },
        { "MUTATION ALTAR", "Reshape yourself, at a price",       "Mutation Altar", Color{ 120, 230, 140, 255 } },
        { "RELIC FORGE",    "Melt, remake or temper your relics", "Relic Forge",    Color{ 255, 125,  60, 255 } },
        { "REST SITE",      "Rest, train or fortify",             "Rest Site",      Color{ 255, 190, 110, 255 } },
    };
    static_assert(sizeof(kTable) / sizeof(kTable[0]) == (size_t)RoomSpecialType::Count,
                  "every RoomSpecialType needs a presentation row");
    const int i = (int)type;
    return kTable[(i >= 0 && i < (int)RoomSpecialType::Count) ? i : 0];
}

namespace
{
    // ── Cursed Cache ────────────────────────────────────────────────────────
    enum class CacheReward { Gold, Relic, FullRestore, Echoes };
    struct CacheOffer
    {
        const char* name;
        const char* flavor;
        CacheReward reward;
        const char* curseText;    // the downside block
        const char* curseLabel;   // HUD pill while it lasts
        float enemyHpMult, enemyDmgMult, goldMult;
        int   rooms;
    };
    const CacheOffer kCacheOffers[] = {
        { "Blood Gold",      "\"Every coin remembers.\"", CacheReward::Gold,
          "Enemies hit +30% harder\nfor the next 2 rooms", "BLOOD DEBT", 1.00f, 1.30f, 1.f, 2 },
        { "Forbidden Relic", "\"It was sealed for a reason.\"", CacheReward::Relic,
          "Enemies are +35% tougher\nfor the next 3 rooms", "FORBIDDEN PRICE", 1.35f, 1.00f, 1.f, 3 },
        { "Hollow Vigor",    "\"Borrowed breath.\"", CacheReward::FullRestore,
          "Enemies +25% tougher and\n+15% deadlier, 2 rooms", "HOLLOW DEBT", 1.25f, 1.15f, 1.f, 2 },
        { "Stolen Echoes",   "\"The dead want them back.\"", CacheReward::Echoes,
          "Enemies drop no gold\nfor the next 2 rooms", "BARREN PURSE", 1.00f, 1.00f, 0.f, 2 },
    };
    constexpr int kCacheOfferCount = (int)(sizeof(kCacheOffers) / sizeof(kCacheOffers[0]));
    constexpr Color kCurseTint{ 205, 95, 215, 255 };

    // ── Omen Stone ──────────────────────────────────────────────────────────
    struct OmenOffer
    {
        const char* name;
        const char* label;    // HUD pill
        const char* cost;
        const char* gift;
        float enemyHpMult, enemyDmgMult, goldMult, xpMult;
        Color tint;
    };
    const OmenOffer kOmenOffers[] = {
        { "Omen of Plenty", "PLENTY",   "Enemies are +20% tougher\nin the next room",  "+75% gold\nin the next room",
          1.20f, 1.00f, 1.75f, 1.00f, Color{ 235, 205,  90, 255 } },
        { "Omen of Frailty", "FRAILTY", "Half the XP\nin the next room",               "Enemies have 30% less HP\nin the next room",
          0.70f, 1.00f, 1.00f, 0.50f, Color{ 150, 200, 255, 255 } },
        { "Omen of the Hunt", "THE HUNT", "Enemies hit +25% harder\nin the next room", "+75% XP\nin the next room",
          1.00f, 1.25f, 1.00f, 1.75f, Color{ 140, 220, 120, 255 } },
        { "Omen of Calm", "CALM",       "Half the gold\nin the next room",              "Enemies hit 30% softer\nin the next room",
          1.00f, 0.70f, 0.50f, 1.00f, Color{ 170, 230, 230, 255 } },
        { "Omen of Ruin", "RUIN",       "Enemies +25% tougher\nand +25% deadlier",      "Double gold and XP\nin the next room",
          1.25f, 1.25f, 2.00f, 2.00f, Color{ 235, 110, 100, 255 } },
    };
    constexpr int kOmenOfferCount = (int)(sizeof(kOmenOffers) / sizeof(kOmenOffers[0]));

    // ── Locked Armory ───────────────────────────────────────────────────────
    constexpr int kArmoryBasePrice[3] = { 45, 90, 150 };   // Iron, Silver, Gilded; +25% per domain

    // ── Mutation Altar ──────────────────────────────────────────────────────
    struct MutationOffer { const char* name; const char* flavor; const char* sacrifice; const char* gain; };
    const MutationOffer kMutationOffers[] = {
        { "Glass Marrow",    "\"Sharper, and easier to break.\"", "-20% max HP",     "+20% damage for the\nrest of the run" },
        { "Quickened Blood", "\"Run before it catches you.\"",    "-10% max HP",     "+12% move speed\nDash recharges 20% faster" },
        { "Arcane Graft",    "\"Trade the shell for the spark.\"", "-10 Defense",    "+6 max mana\n+20% mana regen" },
        { "Iron Hide",       "\"Heavy, and hard to kill.\"",      "-10% move speed", "+10 Defense\n+15% max HP" },
    };
    constexpr int kMutationOfferCount = (int)(sizeof(kMutationOffers) / sizeof(kMutationOffers[0]));
}

int RoomEventOfferCount(RoomSpecialType type)
{
    switch (type)
    {
    case RoomSpecialType::CursedReward:  return kCacheOfferCount;
    case RoomSpecialType::OmenProphecy:  return kOmenOfferCount;
    case RoomSpecialType::MutationAltar: return kMutationOfferCount;
    case RoomSpecialType::None:          return 0;
    default:                             return 3;   // Risk Shrine, Armory, Forge, Rest: always all three
    }
}

void Engine::TagRoomSpecial(DungeonRoomState& st, RoomSpecialType type)
{
    st.special        = type;
    st.specialClaimed = false;
    st.specialChoice  = -1;
    const int count = RoomEventOfferCount(type);
    std::vector<int> picks((size_t)std::max(count, 0));
    std::iota(picks.begin(), picks.end(), 0);
    if (count > 3)   // seeded (GetRandomValue follows the run seed), so dailies match
        for (int i = count - 1; i > 0; --i)
            std::swap(picks[(size_t)i], picks[(size_t)GetRandomValue(0, i)]);
    for (int i = 0; i < 3; ++i)
        st.specialOptions[i] = i < count ? picks[(size_t)i] : -1;
    if (count > 3)   // shown in authored order: a stable, readable left-to-right
        std::sort(st.specialOptions, st.specialOptions + 3);
}

AbilityType Engine::FindWeakestUpgradableAbility() const
{
    AbilityType target = AbilityType::None;
    int lowestLevel = 999;
    for (AbilityType ability : kAllAbilities)
    {
        if (IsUltimateAbility(ability) || !_player.HasLearnedAbility(ability) ||
            !_player.CanUpgradeAbility(ability)) continue;
        const int level = _player.GetAbilityLevel(ability);
        if (level < lowestLevel) { lowestLevel = level; target = ability; }
    }
    return target;
}

int Engine::RoomEventGold(int base) const
{
    const int domain = std::clamp(_worldZone, 0, 5);
    return (int)std::lround(base * (1.f + 0.25f * (float)domain) / 5.f) * 5;   // whole fives
}

void Engine::BuildRoomEventCards(const DungeonRoomState& st)
{
    const RoomEventPresentation& look = GetRoomEventPresentation(st.special);
    ChoiceCardScreen& s = _decisionScreen;
    s.title       = look.title;
    s.titleColor  = look.color;
    s.showPurse   = false;
    const bool atCap = AcceptedModifierCount() >= kMaxAcceptedModifiers;
    const int  domain = std::clamp(_worldZone, 0, 5);

    // The weakest learned ability a Train / Silver Case would raise.
    const AbilityType weakest = FindWeakestUpgradableAbility();
    const std::string trainText = weakest != AbilityType::None
        ? TextFormat("Upgrade %s\nto level %d", GetAbilityName(weakest), _player.GetAbilityLevel(weakest) + 1)
        : std::string("+30 max HP\n(abilities are maxed)");

    switch (st.special)
    {
    case RoomSpecialType::CursedReward:
        s.subtitle    = "Take a gift from the cache. Its curse follows you into the next rooms.";
        s.confirmVerb = "TAKE IT";
        s.confirmHint = "Take";
        s.bgColorA = Color{ 30, 14, 32, 255 }; s.bgColorB = Color{ 42, 20, 46, 255 };
        for (int i = 0; i < 3; ++i)
        {
            const int oi = st.specialOptions[i];
            if (oi < 0 || oi >= kCacheOfferCount) continue;
            const CacheOffer& o = kCacheOffers[oi];
            ChoiceCard c;
            c.title = o.name; c.flavor = o.flavor; c.accentBar = true; c.tint = kCurseTint;
            c.downsideHeader = "CURSE";
            c.downsideText   = o.curseText;
            c.upsideHeader   = "GIFT";
            switch (o.reward)
            {
            case CacheReward::Gold:        c.upsideText = TextFormat("+%d gold now", RoomEventGold(60)); break;
            case CacheReward::Relic:       c.upsideText = "Choose 1 of 3 relics\nright now"; break;
            case CacheReward::FullRestore: c.upsideText = "Heal to full HP\nand refill mana"; break;
            case CacheReward::Echoes:      c.upsideText = TextFormat("+%d Echoes now", 20 + 10 * domain); break;
            }
            if (atCap) { c.enabled = false; c.disabledReason = "TOO MANY BURDENS"; }
            s.cards.push_back(c);
        }
        break;

    case RoomSpecialType::OmenProphecy:
        s.subtitle    = "Read one omen. It shapes the next combat room, for better and worse.";
        s.confirmVerb = "ACCEPT";
        s.confirmHint = "Accept";
        s.bgColorA = Color{ 14, 20, 36, 255 }; s.bgColorB = Color{ 20, 28, 50, 255 };
        for (int i = 0; i < 3; ++i)
        {
            const int oi = st.specialOptions[i];
            if (oi < 0 || oi >= kOmenOfferCount) continue;
            const OmenOffer& o = kOmenOffers[oi];
            ChoiceCard c;
            c.title = o.name; c.accentBar = true; c.tint = o.tint;
            c.downsideHeader = "COST"; c.downsideText = o.cost;
            c.upsideHeader   = "GIFT"; c.upsideText   = o.gift;
            if (atCap) { c.enabled = false; c.disabledReason = "TOO MANY OMENS"; }
            s.cards.push_back(c);
        }
        break;

    case RoomSpecialType::LockedArmory:
    {
        s.subtitle    = "Three locked cases. You have time to pry open one.";
        s.confirmVerb = "OPEN";
        s.confirmHint = "Open";
        s.showPurse   = true;
        s.bgColorA = Color{ 30, 26, 14, 255 }; s.bgColorB = Color{ 42, 36, 20, 255 };
        static const char* kCaseNames[3] = { "Iron Case", "Silver Case", "Gilded Case" };
        const std::string caseGifts[3] = {
            "Restore 40% HP\nand +5 Defense",
            trainText,
            "Choose 1 of 3 relics",
        };
        for (int i = 0; i < 3; ++i)
        {
            ChoiceCard c;
            c.title = kCaseNames[i]; c.accentBar = true; c.tint = look.color;
            c.costGold = RoomEventGold(kArmoryBasePrice[i]);
            c.downsideHeader = "ONE CASE ONLY";
            c.downsideText   = "The other locks\nseal for good";
            c.upsideHeader   = "INSIDE";
            c.upsideText     = caseGifts[i];
            s.cards.push_back(c);
        }
        break;
    }

    case RoomSpecialType::MutationAltar:
        s.subtitle    = "The altar reshapes you for the rest of the run. Every gift takes something.";
        s.confirmVerb = "MUTATE";
        s.confirmHint = "Mutate";
        s.bgColorA = Color{ 14, 30, 20, 255 }; s.bgColorB = Color{ 20, 42, 28, 255 };
        for (int i = 0; i < 3; ++i)
        {
            const int oi = st.specialOptions[i];
            if (oi < 0 || oi >= kMutationOfferCount) continue;
            const MutationOffer& o = kMutationOffers[oi];
            ChoiceCard c;
            c.title = o.name; c.flavor = o.flavor; c.accentBar = true; c.tint = look.color;
            c.downsideHeader = "SACRIFICE"; c.downsideText = o.sacrifice;
            c.upsideHeader   = "GAIN";      c.upsideText   = o.gain;
            s.cards.push_back(c);
        }
        break;

    case RoomSpecialType::RelicForge:
    {
        s.subtitle    = "The forge works on your newest relic, or charges them all.";
        s.confirmVerb = "FORGE";
        s.confirmHint = "Forge";
        s.bgColorA = Color{ 34, 18, 12, 255 }; s.bgColorB = Color{ 48, 26, 16, 255 };
        const int relics = _player.GetRelicCount();
        const RelicType newest = _player.GetRelicAt(relics - 1);
        const std::string destroy = newest != RelicType::Count
            ? std::string("Destroy ") + GetRelicInfo(newest).name : std::string("Needs a relic");
        ChoiceCard salvage;
        salvage.title = "Salvage"; salvage.accentBar = true; salvage.tint = look.color;
        salvage.downsideHeader = "COST"; salvage.downsideText = destroy;
        salvage.upsideHeader   = "GAIN";
        salvage.upsideText     = TextFormat("+%d gold\nRestore 30%% HP", RoomEventGold(50));
        ChoiceCard reforge = salvage;
        reforge.title      = "Reforge";
        reforge.upsideText = "Choose 1 of 3\nnew relics";
        if (newest == RelicType::Count)
        {
            salvage.enabled = reforge.enabled = false;
            salvage.disabledReason = reforge.disabledReason = "NO RELIC TO FORGE";
        }
        ChoiceCard overcharge;
        overcharge.title = "Temper"; overcharge.accentBar = true; overcharge.tint = look.color;
        overcharge.downsideHeader = "COST"; overcharge.downsideText = "-15% max HP";
        overcharge.upsideHeader   = "GAIN";
        overcharge.upsideText     = TextFormat("+5%% damage per relic\nyou carry (now %d)", relics);
        s.cards.push_back(salvage);
        s.cards.push_back(reforge);
        s.cards.push_back(overcharge);
        break;
    }

    case RoomSpecialType::RestSite:
    {
        s.subtitle    = "The fire is warm. There is time for one thing.";
        s.confirmVerb = "CHOOSE";
        s.confirmHint = "Choose";
        s.bgColorA = Color{ 34, 22, 12, 255 }; s.bgColorB = Color{ 46, 30, 16, 255 };
        const int healAmount = (int)std::ceil(_player.GetMaxHealthValue() * 0.4f);
        ChoiceCard rest;
        rest.title = "Rest"; rest.flavor = "\"Close your eyes. Just for a moment.\"";
        rest.accentBar = true; rest.tint = Color{ 140, 230, 140, 255 };
        rest.upsideHeader = "HEAL";
        rest.upsideText   = TextFormat("Heal 40%% of max HP\n(+%d HP)", healAmount);
        ChoiceCard train;
        train.title = "Train"; train.flavor = "\"Again. Slower. Again.\"";
        train.accentBar = true; train.tint = Color{ 150, 200, 255, 255 };
        train.upsideHeader = "SHARPEN";
        train.upsideText   = trainText;
        ChoiceCard fortify;
        fortify.title = "Fortify"; fortify.flavor = "\"Mend the straps. Tighten the plates.\"";
        fortify.accentBar = true; fortify.tint = look.color;
        fortify.upsideHeader = "DEFENSE";
        fortify.upsideText   = TextFormat("+%.0f Defense for\nthe rest of the run", Balance::Stats::kDefenseMajor);
        s.cards.push_back(rest);
        s.cards.push_back(train);
        s.cards.push_back(fortify);
        break;
    }

    default:
        break;
    }
}

bool Engine::ApplyRoomEventChoice(const DungeonRoomState& st, int card)
{
    if (card < 0 || card >= (int)_decisionScreen.cards.size() || !_decisionScreen.cards[(size_t)card].enabled)
        return false;
    const int option = (card < 3) ? st.specialOptions[card] : -1;
    const int domain = std::clamp(_worldZone, 0, 5);
    auto toast = [&](const std::string& text) { _contractToastText = text; _contractToastTimer = 4.f; };
    auto pushModifier = [&](const char* label, float hp, float dmg, float gold, float xp, int rooms, bool curse, Color tint)
    {
        RunModifier m;
        m.label = label; m.enemyHpMult = hp; m.enemyDmgMult = dmg; m.goldMult = gold; m.xpMult = xp;
        m.roomsRemaining = rooms; m.curse = curse; m.tint = tint; m.active = false;   // arms for the next combat room
        _runModifiers.push_back(m);
    };
    auto upgradeWeakest = [&]() -> std::string
    {
        const AbilityType weakest = FindWeakestUpgradableAbility();
        if (weakest == AbilityType::None)
        {
            _player.ScaleMaxHealth((_player.GetMaxHealthValue() + 30.f) / std::max(1.f, _player.GetMaxHealthValue()));
            return "+30 max HP";
        }
        _player.UpgradeAbility(weakest);
        return std::string(GetAbilityName(weakest)) + " reaches level " + std::to_string(_player.GetAbilityLevel(weakest));
    };

    switch (st.special)
    {
    case RoomSpecialType::CursedReward:
    {
        if (option < 0 || option >= kCacheOfferCount || AcceptedModifierCount() >= kMaxAcceptedModifiers) return false;
        const CacheOffer& o = kCacheOffers[option];
        switch (o.reward)
        {
        case CacheReward::Gold:        _player.AddGold(RoomEventGold(60)); break;
        case CacheReward::Relic:       ++_pendingRelicChoices; break;
        case CacheReward::FullRestore: _player.Heal(9999); _player.RestoreMana(_player.GetMaxMana()); break;
        case CacheReward::Echoes:      _player.AddCells(20 + 10 * domain); break;
        }
        pushModifier(o.curseLabel, o.enemyHpMult, o.enemyDmgMult, o.goldMult, 1.f, o.rooms, true, kCurseTint);
        toast(std::string(o.name) + " taken. " + o.curseLabel + " waits in the next room.");
        break;
    }

    case RoomSpecialType::OmenProphecy:
    {
        if (option < 0 || option >= kOmenOfferCount || AcceptedModifierCount() >= kMaxAcceptedModifiers) return false;
        const OmenOffer& o = kOmenOffers[option];
        pushModifier(o.label, o.enemyHpMult, o.enemyDmgMult, o.goldMult, o.xpMult, 1, false, o.tint);
        toast(std::string(o.name) + ": the next room is marked.");
        break;
    }

    case RoomSpecialType::LockedArmory:
    {
        const int price = _decisionScreen.cards[(size_t)card].costGold;
        if (_player.GetGold() < price) return false;
        _player.AddGold(-price);
        if (card == 0)
        {
            _player.Heal((int)std::ceil(_player.GetMaxHealthValue() * 0.4f));
            _player.AddDefense(Balance::Stats::kDefenseMinor);
            toast(TextFormat("Iron Case: patched up, +%.0f Defense.", Balance::Stats::kDefenseMinor));
        }
        else if (card == 1) toast("Silver Case: " + upgradeWeakest() + ".");
        else
        {
            ++_pendingRelicChoices;
            toast("Gilded Case: a relic waits inside.");
        }
        break;
    }

    case RoomSpecialType::MutationAltar:
    {
        if (option < 0 || option >= kMutationOfferCount) return false;
        switch (option)
        {
        case 0: _player.ScaleMaxHealth(0.80f); _player.ScaleEventDamage(1.20f); break;
        case 1: _player.ScaleMaxHealth(0.90f); _player.ScaleMoveSpeed(1.12f); _player.ScaleDashCooldown(0.80f); break;
        case 2: _player.AddDefense(-Balance::Stats::kDefenseMajor); _player.AddMaxMana(6); _player.ScaleManaRegen(1.20f); break;
        case 3: _player.ScaleMoveSpeed(0.90f); _player.AddDefense(Balance::Stats::kDefenseMajor); _player.ScaleMaxHealth(1.15f); break;
        }
        toast(std::string(kMutationOffers[option].name) + " takes hold.");
        EarnFeat(Feat::Reshaped);
        break;
    }

    case RoomSpecialType::RelicForge:
    {
        const RelicType newest = _player.GetRelicAt(_player.GetRelicCount() - 1);
        if (card == 0 || card == 1)
        {
            if (newest == RelicType::Count) return false;
            const std::string name = GetRelicInfo(newest).name;
            _player.RemoveRelic(newest);
            if (card == 0)
            {
                _player.AddGold(RoomEventGold(50));
                _player.Heal((int)std::ceil(_player.GetMaxHealthValue() * 0.3f));
                toast(name + " melted down for gold.");
            }
            else
            {
                ++_pendingRelicChoices;
                toast(name + " is gone. Choose what the forge makes.");
            }
        }
        else
        {
            _player.ScaleMaxHealth(0.85f);
            _player.AddRelicOvercharge(0.05f);
            toast(TextFormat("Relics tempered: +%d%% damage now.",
                             (int)std::lround(_player.GetRelicOverchargePerRelic() * 100.f * (float)_player.GetRelicCount())));
        }
        break;
    }

    case RoomSpecialType::RestSite:
        if (card == 0)
        {
            const int heal = (int)std::ceil(_player.GetMaxHealthValue() * 0.4f);
            _player.Heal(heal);
            toast("You rest by the fire.");
        }
        else if (card == 1) toast("Training: " + upgradeWeakest() + ".");
        else
        {
            _player.AddDefense(Balance::Stats::kDefenseMajor);
            toast(TextFormat("Fortified: +%.0f Defense.", Balance::Stats::kDefenseMajor));
        }
        break;

    default:
        return false;
    }
    return true;
}
