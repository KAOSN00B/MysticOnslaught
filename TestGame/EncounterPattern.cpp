#include "EncounterPattern.h"

namespace
{
    // Same cheap integer hash the elite modifier roll uses — deterministic per
    // seed so re-entry/tests can reproduce a selection exactly.
    std::uint32_t HashSeed(std::uint32_t seed)
    {
        seed ^= seed >> 16; seed *= 0x7feb352du;
        seed ^= seed >> 15; seed *= 0x846ca68bu;
        seed ^= seed >> 16;
        return seed;
    }
}

int SelectAttackCard(const AttackCard* cards, int cardCount,
                     int phase, float distanceToTarget,
                     int previousCardId,
                     const float* groupCooldowns, int groupCount,
                     std::uint32_t seed)
{
    if (cards == nullptr || cardCount <= 0)
        return -1;

    // Pass 1: filter. A card survives only when the phase allows it, the
    // target sits inside its range band, it is not an illegal immediate
    // repeat, and its cooldown group is cold.
    constexpr int kMaxCards = 16;
    int survivorIndices[kMaxCards];
    int survivorCount = 0;
    int totalWeight = 0;
    for (int cardIndex = 0; cardIndex < cardCount && cardIndex < kMaxCards; ++cardIndex)
    {
        const AttackCard& card = cards[cardIndex];
        if (phase >= 0 && (card.allowedPhasesMask & (1u << phase)) == 0)
            continue;
        if (distanceToTarget < card.minRange || distanceToTarget > card.maxRange)
            continue;
        if (!card.canFollowSelf && card.id == previousCardId)
            continue;
        if (card.cooldownGroup >= 0 && groupCooldowns != nullptr &&
            card.cooldownGroup < groupCount && groupCooldowns[card.cooldownGroup] > 0.f)
            continue;
        survivorIndices[survivorCount++] = cardIndex;
        totalWeight += (card.weight > 0) ? card.weight : 1;
    }

    if (survivorCount == 0 || totalWeight <= 0)
        return -1;

    // Pass 2: weighted pick among the survivors only.
    int roll = (int)(HashSeed(seed) % (std::uint32_t)totalWeight);
    for (int survivorIndex = 0; survivorIndex < survivorCount; ++survivorIndex)
    {
        const AttackCard& card = cards[survivorIndices[survivorIndex]];
        roll -= (card.weight > 0) ? card.weight : 1;
        if (roll < 0)
            return survivorIndices[survivorIndex];
    }
    return survivorIndices[survivorCount - 1];
}
