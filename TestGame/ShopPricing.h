#pragma once

// Zeph's price rules, shared by ShopManager and the tests. The shop tier comes
// from RunProgression::ShopTierForZone (1 = opening Forest / no run, 6 = the
// final domain), so prices rise with campaign depth instead of staying flat.

#include "GameBalance.h"

namespace ShopPricing
{
    inline float AbilityInflation(int tier)
    {
        return 1.0f + Balance::Economy::kActPriceInflation * (float)((tier < 1 ? 1 : tier) - 1);
    }

    inline float WareInflation(int tier)
    {
        return 1.0f + Balance::Economy::kWareTierInflation * (float)((tier < 1 ? 1 : tier) - 1);
    }

    inline int Scaled(int basePrice, float inflation)
    {
        return (int)((float)basePrice * inflation + 0.5f);
    }

    // Price to raise an owned ability from currentLevel to currentLevel + 1.
    // Both the offer card and the left-panel Upgrade button use this.
    inline int AbilityUpgradePrice(int currentLevel, int tier)
    {
        const int base = Balance::Economy::kAbilityUpgradeBase +
                         ((currentLevel < 1 ? 1 : currentLevel) - 1) * Balance::Economy::kAbilityUpgradeStep;
        return Scaled(base, AbilityInflation(tier));
    }
}
