#pragma once

#include "EliteSignature.h"

#include <cstdint>

// ─────────────────────────────────────────────────────────────────────────────
// EncounterPattern — neutral encounter primitives for the hybrid boss model
// (one authored survival set piece per phase + a small deck of ordinary
// attacks). Bosses and elites share ONE event/zone pipeline: the elite
// vocabulary generalizes here rather than bosses pretending to be elites.
//
// Pure and allocation-free so everything is testable standalone alongside
// EliteSignatureTests.
// ─────────────────────────────────────────────────────────────────────────────

// Neutral aliases over the proven elite shapes (review-approved gradual path).
using EncounterEvent = EliteSignatureEvent;
using EncounterStage = EliteActionStage;
using EncounterClock = EliteActionClock;

// ── Attack cards ─────────────────────────────────────────────────────────────
// Each ordinary boss move is a card. The selector FILTERS invalid cards first
// (wrong phase, out of range, immediate repeat, cooldown group still hot),
// then weighted-picks from the survivors — a boss never chooses an attack
// merely because a random roll happened, and never repeats itself back-to-back
// unless the card explicitly allows it.
struct AttackCard
{
    int           id = 0;
    std::uint32_t allowedPhasesMask = 0x7;   // bit per phase (bit 0 = phase one)
    float         minRange = 0.f;            // valid only when the target is this far…
    float         maxRange = 1.0e9f;         // …and no farther
    int           weight = 1;                // relative pick weight among survivors
    int           cooldownGroup = -1;        // -1 = no group; groups share a hot timer
    bool          canFollowSelf = false;     // immediate repetition allowed?
};

// Deterministic deck selection. Returns the chosen card's INDEX into `cards`,
// or -1 when no card is valid (the caller repositions or uses a conservative
// fallback — never a forced pick). `groupCooldowns` holds one remaining-seconds
// timer per cooldown group; a group is hot while its timer is above zero.
int SelectAttackCard(const AttackCard* cards, int cardCount,
                     int phase, float distanceToTarget,
                     int previousCardId,
                     const float* groupCooldowns, int groupCount,
                     std::uint32_t seed);
