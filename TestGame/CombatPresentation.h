#pragma once
#include "GameTypes.h"
#include <algorithm>

// Presentation admission only: never gates damage, rewards, or status effects.
class CombatImpactGate
{
public:
    bool Admit(double now, bool damageOverTime, bool killed, bool critical)
    {
        if (damageOverTime && !killed) return false;
        if (killed || critical)
        {
            if (now < _nextAccent) return false;
            _nextAccent = now + 0.12;
            _nextDirect = now + 0.075;
            return true;
        }
        if (now < _nextDirect) return false;
        _nextDirect = now + 0.075;
        return true;
    }
private:
    double _nextDirect = 0.0;
    double _nextAccent = 0.0;
};

// Preserve authored profiles, populations, geometry, and simultaneous caps.
inline float EncounterBreathingInterval(EncounterProfile profile)
{
    switch (profile)
    {
    case EncounterProfile::Skirmish: return 2.0f;
    case EncounterProfile::Assault: return 1.15f;
    case EncounterProfile::Swarm: return 0.65f;
    case EncounterProfile::Holdout: return 1.5f;
    }
    return 1.15f;
}

inline bool ShouldReserveEncounterBatch(float remaining, float interval, int active, int pending)
{
    if (pending > 0) return false;
    // Empty arenas need only a short breath; survivors do not bypass the rhythm.
    return remaining <= 0.f || (active == 0 && interval - remaining >= 0.45f);
}

inline float ImpactShakeEnvelope(float remaining, float duration)
{
    if (duration <= 0.f) return 0.f;
    const float t = std::clamp(remaining / duration, 0.f, 1.f);
    return t * t;
}
