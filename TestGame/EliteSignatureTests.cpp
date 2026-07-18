// Deterministic assertion tests for the pure elite signature primitives.
// Build standalone (no raylib linking needed — Vector2 is header-only):
//   cl /nologo /std:c++17 /EHsc /DELITE_SIGNATURE_TEST_MAIN ^
//      /I"C:\CLibraries\raylib-5.5_win64_msvc16\include" ^
//      TestGame\EliteSignature.cpp TestGame\EliteSignatureTests.cpp ^
//      /link /OUT:x64\Debug\EliteSignatureTests.exe

#include "EliteSignature.h"
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
    assert(clock.Update(0.30f));                 // crosses telegraph into active
    assert(clock.GetStage() == EliteActionStage::Active);
    assert(clock.GetStageRemaining() > 0.29f && clock.GetStageRemaining() < 0.31f);
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
    TestOgreChargeSequencing();
    TestDistancePointToSegmentGeometry();
    std::puts("Elite signature tests passed");
    return 0;
}
#endif
