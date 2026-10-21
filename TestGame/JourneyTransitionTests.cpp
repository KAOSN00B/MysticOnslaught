#include "JourneyTransition.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <limits>
#include <stdexcept>

int main()
{
    JourneyTransition journey;
    int calls = 0;
    assert(journey.Begin("Forest", {[&] { ++calls; }, [&] { ++calls; }}));
    assert(!journey.Begin("Duplicate", {[&] { calls += 100; }}));
    for (int i = 0; i < 20; ++i) journey.Update(0.1f);
    assert(calls == 0 && journey.Alpha() == 1.f);
    journey.Presented();
    journey.Update(0.01f);
    assert(calls == 1 && journey.Active() && !journey.Revealing());
    journey.Update(0.01f);
    assert(calls == 1); // every stage yields a rendered frame
    journey.Presented();
    journey.Update(0.01f);
    assert(calls == 2 && journey.Revealing() && journey.Alpha() == 1.f);
    journey.Update(0.1f);
    assert(journey.Active() && journey.Alpha() > 0.f && journey.Alpha() < 1.f);
    for (int i = 0; i < 20; ++i) journey.Update(0.1f);
    assert(!journey.Active() && calls == 2 && journey.Alpha() == 0.f);
    assert(!journey.Begin("Empty", {}));
    assert(journey.Begin("Failure", {[] { throw std::runtime_error("missing destination"); }}));
    journey.Update(std::numeric_limits<float>::quiet_NaN());
    assert(std::isfinite(journey.Alpha()));
    journey.Update(-2.f);
    assert(journey.Alpha() == 0.f);
    for (int i = 0; i < 5; ++i) journey.Update(0.1f);
    journey.Presented();
    journey.Update(0.01f);
    assert(journey.Revealing() && journey.Error() == "missing destination");
    for (int i = 0; i < 20; ++i) journey.Update(0.1f);
    assert(journey.Begin("Retry", {[&] { ++calls; }}));
    assert(journey.Error().empty());
    JourneyTransition failure;
    bool executing = false;
    int cleanupCalls = 0;
    auto guarded = JourneyTransition::GuardStep(executing,
        [] { throw std::runtime_error("stage failure"); },
        [] { throw std::runtime_error("recovery failure"); });
    assert(failure.Begin("Recovery throws", {guarded}, [&] { ++cleanupCalls; }));
    failure.Update(0.1f); failure.Update(0.1f); failure.Presented(); failure.Update(0.01f);
    assert(!executing && cleanupCalls == 1 && failure.Revealing());
    assert(!failure.Error().empty());
    for (int i=0; i<10; ++i) failure.Update(0.1f);
    assert(cleanupCalls == 1);
    assert(failure.Begin("Success cleanup", {[] {}}, [&] { ++cleanupCalls; }));
    failure.Update(0.1f); failure.Update(0.1f); failure.Presented(); failure.Update(0.01f);
    assert(cleanupCalls == 2 && failure.Error().empty());
    JourneyTransition emptyMessage;
    bool unsafeStepRan = false;
    assert(emptyMessage.Begin("Empty exception", {[] { throw std::runtime_error(""); }, [&] { unsafeStepRan = true; }}));
    emptyMessage.Update(0.1f); emptyMessage.Update(0.1f); emptyMessage.Presented(); emptyMessage.Update(0.01f);
    assert(emptyMessage.Revealing() && !emptyMessage.Error().empty());
    emptyMessage.Presented(); emptyMessage.Update(0.01f);
    assert(!unsafeStepRan);
    std::puts("Journey transition tests passed: presentation barriers, staging, duplicate requests, reveal, failure, invalid dt.");
}
