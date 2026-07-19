// Deterministic assertion tests for the pure elite signature primitives.
// Build standalone (no raylib linking needed — Vector2 is header-only):
//   cl /nologo /std:c++17 /EHsc /DELITE_SIGNATURE_TEST_MAIN ^
//      /I"C:\CLibraries\raylib-5.5_win64_msvc16\include" ^
//      TestGame\EliteSignature.cpp TestGame\EncounterPattern.cpp ^
//      TestGame\EliteSignatureTests.cpp ^
//      /link /OUT:x64\Debug\EliteSignatureTests.exe

#include "EliteSignature.h"
#include "EncounterPattern.h"
#include "GameBalance.h"

#include <cassert>
#include <cmath>
#include <cstdio>

static void TestActionClockTraversesReadableStages()
{
    EliteActionClock clock;
    clock.Start({ 0.40f, 0.20f, 0.35f });
    assert(clock.GetStage() == EliteActionStage::Telegraph);
    assert(!clock.Update(0.39f));
    assert(clock.GetStage() == EliteActionStage::Telegraph);
    assert(clock.Update(0.02f));
    assert(clock.GetStage() == EliteActionStage::Active);
    assert(clock.Update(0.20f));
    assert(clock.GetStage() == EliteActionStage::Recovery);
    assert(clock.Update(0.35f));
    assert(clock.GetStage() == EliteActionStage::Ready);
}

static void TestActionClockCancelAndLargeFrames()
{
    EliteActionClock clock;
    clock.Start({ 0.30f, 0.20f, 0.30f });
    clock.Cancel();
    assert(clock.GetStage() == EliteActionStage::Ready);
    assert(!clock.Update(1.0f));   // cancelled clocks stay Ready

    // One huge frame still lands in the correct later stage (overshoot carry).
    clock.Start({ 0.10f, 0.50f, 0.30f });
    assert(clock.Update(0.30f) == 1);            // crosses telegraph into active
    assert(clock.GetStage() == EliteActionStage::Active);
    assert(clock.GetStageRemaining() > 0.29f && clock.GetStageRemaining() < 0.31f);

    // A severe hitch can cross TWO stages in one update — both are reported.
    clock.Start({ 0.10f, 0.10f, 0.50f });
    assert(clock.Update(0.25f) == 2);            // telegraph AND active crossed
    assert(clock.GetStage() == EliteActionStage::Recovery);
    assert(clock.GetStageRemaining() > 0.44f && clock.GetStageRemaining() < 0.46f);

    // A single update can complete the whole action.
    clock.Start({ 0.10f, 0.10f, 0.10f });
    assert(clock.Update(1.0f) == 3);
    assert(clock.GetStage() == EliteActionStage::Ready);

    // Zero-duration stages are crossed immediately and can never loop forever.
    clock.Start({ 0.f, 0.f, 0.20f });
    assert(clock.GetStage() == EliteActionStage::Telegraph);
    assert(clock.Update(0.05f) == 2);            // through telegraph + active
    assert(clock.GetStage() == EliteActionStage::Recovery);
}

static void TestGuardLinksReduceButNeverNullify()
{
    assert(ApplyGuardLinkReduction(10) == 4);
    assert(ApplyGuardLinkReduction(2) == 1);
    assert(ApplyGuardLinkReduction(1) == 1);
    assert(ApplyGuardLinkReduction(0) == 0);
    assert(ApplyGuardLinkReduction(-5) == -5);   // heals/zero pass through untouched
}

static void TestModifierCompatibilityMatchesDesign()
{
    assert(IsEliteModifierCompatible(EliteArchetype::Ogre, EliteModifier::Cage));
    assert(IsEliteModifierCompatible(EliteArchetype::Ogre, EliteModifier::GuardLinks));
    assert(IsEliteModifierCompatible(EliteArchetype::Ogre, EliteModifier::Enrage));
    assert(!IsEliteModifierCompatible(EliteArchetype::Ogre, EliteModifier::ArenaPressure));

    assert(!IsEliteModifierCompatible(EliteArchetype::Infernal, EliteModifier::Cage));
    assert(IsEliteModifierCompatible(EliteArchetype::Infernal, EliteModifier::ArenaPressure));

    assert(IsEliteModifierCompatible(EliteArchetype::Bonechill, EliteModifier::Cage));
    assert(!IsEliteModifierCompatible(EliteArchetype::Bonechill, EliteModifier::Enrage));

    assert(!IsEliteModifierCompatible(EliteArchetype::Stormclub, EliteModifier::Cage));
    assert(IsEliteModifierCompatible(EliteArchetype::Stormclub, EliteModifier::Enrage));

    assert(IsEliteModifierCompatible(EliteArchetype::Venomfang, EliteModifier::Enrage));
    assert(IsEliteModifierCompatible(EliteArchetype::Venomfang, EliteModifier::ArenaPressure));
    assert(!IsEliteModifierCompatible(EliteArchetype::Venomfang, EliteModifier::GuardLinks));
    assert(!IsEliteModifierCompatible(EliteArchetype::Venomfang, EliteModifier::Cage));
}

static void TestModifierSelectionIsAlwaysCompatibleAndDeterministic()
{
    for (int archetypeIndex = 0; archetypeIndex < (int)EliteArchetype::Count; ++archetypeIndex)
    {
        const EliteArchetype archetype = (EliteArchetype)archetypeIndex;
        for (std::uint32_t seed = 0; seed < 1000; ++seed)
        {
            EliteModifier chosen = ChooseEliteModifier(archetype, seed);
            assert(IsEliteModifierCompatible(archetype, chosen));
            // Same seed, same result — re-entry and snapshots never reroll.
            assert(ChooseEliteModifier(archetype, seed) == chosen);
        }
    }

    // A compatible forced modifier is honoured exactly.
    assert(ChooseEliteModifier(EliteArchetype::Ogre, 7, (int)EliteModifier::Cage) == EliteModifier::Cage);
    // An incompatible forced modifier falls back to a compatible seeded choice.
    EliteModifier fallback = ChooseEliteModifier(EliteArchetype::Venomfang, 7, (int)EliteModifier::GuardLinks);
    assert(IsEliteModifierCompatible(EliteArchetype::Venomfang, fallback));
}

static void TestPhaseLatchFiresExactlyOnce()
{
    assert(ShouldEnterElitePhaseTwo(false, 6.f, 12.f));     // at exactly 50%
    assert(ShouldEnterElitePhaseTwo(false, 3.f, 12.f));     // deep below
    assert(!ShouldEnterElitePhaseTwo(false, 6.01f, 12.f));  // just above
    assert(!ShouldEnterElitePhaseTwo(true, 3.f, 12.f));     // already latched
    assert(!ShouldEnterElitePhaseTwo(false, 5.f, 0.f));     // broken max health
}

static void TestEventQueueIsBoundedAndFIFO()
{
    EliteEventQueue queue;
    for (int i = 0; i < EliteEventQueue::kCapacity; ++i)
        assert(queue.Push({ EliteEventKind::Telegraph, EliteArchetype::Ogre,
                            EliteMove::OgreCharge, (std::uint32_t)i }));
    assert(!queue.Push({}));                      // full queue rejects, never grows
    assert(queue.GetCount() == EliteEventQueue::kCapacity);

    for (int i = 0; i < EliteEventQueue::kCapacity; ++i)
    {
        EliteSignatureEvent event{};
        assert(queue.Pop(event));
        assert(event.sequence == (std::uint32_t)i);   // strict FIFO order
    }
    EliteSignatureEvent none{};
    assert(!queue.Pop(none));

    // Clear removes every pending event.
    queue.Push({ EliteEventKind::Execute, EliteArchetype::Venomfang, EliteMove::VenomfangPounce, 99 });
    queue.Clear();
    assert(queue.GetCount() == 0);
    assert(!queue.Pop(none));
}

static void TestBonechillFrontReductionNeverZeroes()
{
    assert(ApplyBonechillFrontReduction(10) == 6);   // ceil(10 * 0.55)
    assert(ApplyBonechillFrontReduction(2) == 2);    // ceil(1.1)
    assert(ApplyBonechillFrontReduction(1) == 1);    // chip damage stays real
    assert(ApplyBonechillFrontReduction(0) == 0);
}

static void TestSpreadDirectionsLeaveWalkableGaps()
{
    const float pi = 3.14159265f;
    const Vector2 forward{ 1.f, 0.f };

    // Middle of three fissures fires straight ahead; outer two are symmetric.
    Vector2 left   = EliteSpreadDirection(forward, 0, 3, 50.f * pi / 180.f);
    Vector2 middle = EliteSpreadDirection(forward, 1, 3, 50.f * pi / 180.f);
    Vector2 right  = EliteSpreadDirection(forward, 2, 3, 50.f * pi / 180.f);
    assert(std::fabs(middle.x - 1.f) < 0.001f && std::fabs(middle.y) < 0.001f);
    assert(std::fabs(left.y + right.y) < 0.001f);    // mirrored about forward

    // At Infernal fissure length, adjacent centerlines separate wider than two
    // lane half-widths — a real, walkable gap exists between fissures.
    const float length = Balance::Elite::kInfernalFissureLength;
    Vector2 leftEnd{ left.x * length, left.y * length };
    Vector2 middleEnd{ middle.x * length, middle.y * length };
    float separation = std::sqrt((leftEnd.x - middleEnd.x) * (leftEnd.x - middleEnd.x) +
                                 (leftEnd.y - middleEnd.y) * (leftEnd.y - middleEnd.y));
    assert(separation > 2.f * Balance::Elite::kInfernalFissureWidth);

    // Same guarantee for Bonechill's ice lanes.
    const float laneLength = Balance::Elite::kBonechillLaneLength;
    Vector2 laneA = EliteSpreadDirection(forward, 0, 3, 56.f * pi / 180.f);
    Vector2 laneB = EliteSpreadDirection(forward, 1, 3, 56.f * pi / 180.f);
    Vector2 laneAEnd{ laneA.x * laneLength, laneA.y * laneLength };
    Vector2 laneBEnd{ laneB.x * laneLength, laneB.y * laneLength };
    float laneSeparation = std::sqrt((laneAEnd.x - laneBEnd.x) * (laneAEnd.x - laneBEnd.x) +
                                     (laneAEnd.y - laneBEnd.y) * (laneAEnd.y - laneBEnd.y));
    assert(laneSeparation > 2.f * Balance::Elite::kBonechillLaneWidth);
}

static void TestOgreChargeSequencing()
{
    assert(NextOgreChargeCount(false) == 1);
    assert(NextOgreChargeCount(true) == 2);
    // A wall impact ends the sequence no matter how many charges remain.
    assert(ShouldEndOgreChargeSequence(1, true));
    assert(ShouldEndOgreChargeSequence(0, false));
    assert(!ShouldEndOgreChargeSequence(1, false));   // second charge still owed
}

static void TestDistancePointToSegmentGeometry()
{
    const float tolerance = 0.001f;
    // Horizontal segment: point above the middle.
    assert(std::fabs(DistancePointToSegment({ 5.f, 3.f }, { 0.f, 0.f }, { 10.f, 0.f }) - 3.f) < tolerance);
    // Beyond the end: distance to the endpoint.
    assert(std::fabs(DistancePointToSegment({ 14.f, 3.f }, { 0.f, 0.f }, { 10.f, 0.f }) - 5.f) < tolerance);
    // Vertical segment.
    assert(std::fabs(DistancePointToSegment({ 2.f, 5.f }, { 0.f, 0.f }, { 0.f, 10.f }) - 2.f) < tolerance);
    // Zero-length segment collapses to point distance.
    assert(std::fabs(DistancePointToSegment({ 3.f, 4.f }, { 0.f, 0.f }, { 0.f, 0.f }) - 5.f) < tolerance);
}

static void TestAttackCardSelection()
{
    // A tiny Molarbeast-shaped deck: Dash (id 0, mid+ range, group 0) and
    // Volley (id 1, any range, group 1, phase 2+ heavier weight elsewhere).
    const AttackCard deck[] = {
        { 0, 0x7, 250.f, 1.0e9f, 3, 0, false },   // Dash
        { 1, 0x7, 120.f, 950.f,  2, 1, false },   // Volley
        { 2, 0x4, 0.f,   1.0e9f, 5, -1, false },  // phase-three-only finisher
    };
    const int deckSize = 3;
    float coldGroups[2] = { 0.f, 0.f };

    // No immediate repeats: with Dash as the previous card, Dash never returns.
    for (std::uint32_t seed = 0; seed < 200; ++seed)
    {
        int picked = SelectAttackCard(deck, deckSize, 0, 500.f, 0, coldGroups, 2, seed);
        assert(picked == 1);   // only Volley survives in phase one
    }

    // Range filter: standing inside 250 removes Dash; past 950 removes Volley.
    assert(SelectAttackCard(deck, deckSize, 0, 150.f, -1, coldGroups, 2, 7) == 1);
    assert(SelectAttackCard(deck, deckSize, 0, 1200.f, -1, coldGroups, 2, 7) == 0);

    // Phase filter: the finisher only exists in phase three (phase index 2).
    for (std::uint32_t seed = 0; seed < 200; ++seed)
        assert(SelectAttackCard(deck, deckSize, 0, 500.f, -1, coldGroups, 2, seed) != 2);
    bool finisherSeen = false;
    for (std::uint32_t seed = 0; seed < 200; ++seed)
        if (SelectAttackCard(deck, deckSize, 2, 500.f, -1, coldGroups, 2, seed) == 2)
            finisherSeen = true;
    assert(finisherSeen);

    // Group cooldown filter, and the safe -1 fallback when nothing is valid.
    float hotGroups[2] = { 1.5f, 2.0f };
    assert(SelectAttackCard(deck, deckSize, 0, 500.f, -1, hotGroups, 2, 7) == -1);

    // Deterministic: the same seed always picks the same card.
    int firstPick = SelectAttackCard(deck, deckSize, 2, 500.f, -1, coldGroups, 2, 42);
    for (int repeat = 0; repeat < 10; ++repeat)
        assert(SelectAttackCard(deck, deckSize, 2, 500.f, -1, coldGroups, 2, 42) == firstPick);
}

#ifdef ELITE_SIGNATURE_TEST_MAIN
int main()
{
    TestActionClockTraversesReadableStages();
    TestActionClockCancelAndLargeFrames();
    TestGuardLinksReduceButNeverNullify();
    TestModifierCompatibilityMatchesDesign();
    TestModifierSelectionIsAlwaysCompatibleAndDeterministic();
    TestPhaseLatchFiresExactlyOnce();
    TestEventQueueIsBoundedAndFIFO();
    TestBonechillFrontReductionNeverZeroes();
    TestSpreadDirectionsLeaveWalkableGaps();
    TestOgreChargeSequencing();
    TestDistancePointToSegmentGeometry();
    TestAttackCardSelection();
    std::puts("Elite signature tests passed");
    return 0;
}
#endif
