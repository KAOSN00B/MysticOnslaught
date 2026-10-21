#include "CombatPresentation.h"
#include <cstdio>
#include <cstdlib>

static int failures = 0;
static void Check(bool condition, const char* description)
{
    if (!condition) { std::fprintf(stderr, "FAIL: %s\n", description); ++failures; }
}

int main()
{
    CombatImpactGate gate;
    Check(!gate.Admit(1.0, true, false, false), "ongoing damage must not interrupt movement");
    Check(gate.Admit(1.0, false, false, false), "direct strike gets an impact beat");
    Check(!gate.Admit(1.001, false, false, false), "multi-target hit does not stack global feedback");
    Check(gate.Admit(1.001, false, true, false), "a killing blow can accent a normal hit");
    Check(!gate.Admit(1.002, false, true, false), "chain kills cannot repeatedly freeze the simulation");
    Check(gate.Admit(1.2, true, true, false), "a DOT kill still has a death accent");
    Check(gate.Admit(1.4, false, false, true), "critical hit after cooldown is admitted");
    Check(!gate.Admit(1.401, false, false, true), "critical area damage has a bounded accent");
    gate = CombatImpactGate{};
    Check(gate.Admit(0.0, false, false, false), "reset allows an immediate first strike");
    const float skirmish = EncounterBreathingInterval(EncounterProfile::Skirmish);
    const float assault = EncounterBreathingInterval(EncounterProfile::Assault);
    const float swarm = EncounterBreathingInterval(EncounterProfile::Swarm);
    Check(skirmish > assault && assault > swarm, "profiles have distinct reinforcement rhythms");
    Check(!ShouldReserveEncounterBatch(skirmish, skirmish, 0, 0), "cleared opening gets a short breathing beat");
    Check(ShouldReserveEncounterBatch(skirmish - .5f, skirmish, 0, 0), "empty arena is promptly refilled after breathing beat");
    Check(!ShouldReserveEncounterBatch(.3f, skirmish, 1, 0), "one survivor cannot bypass the profile interval");
    Check(ShouldReserveEncounterBatch(0.f, skirmish, 1, 0), "elapsed interval permits a batch");
    Check(!ShouldReserveEncounterBatch(-1.f, skirmish, 0, 1), "pending warnings cannot be overlapped by another batch");
    Check(ImpactShakeEnvelope(0.f, .2f) == 0.f, "shake settles to zero");
    Check(ImpactShakeEnvelope(.1f, .2f) < .6f, "shake decays during its lifetime");
    Check(ImpactShakeEnvelope(1.f, .2f) <= 1.f, "shake envelope is bounded");
    Check(ImpactShakeEnvelope(.2f, 0.f) == 0.f, "zero duration is safe");
    if (failures) return EXIT_FAILURE;
    std::puts("Combat presentation: 19 checks passed");
    return EXIT_SUCCESS;
}
