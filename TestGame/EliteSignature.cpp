#include "EliteSignature.h"
#include "GameBalance.h"

#include <algorithm>
#include <cmath>

// ── EliteActionClock ─────────────────────────────────────────────────────────

void EliteActionClock::Start(EliteActionTiming timing)
{
    _timing = timing;
    _stage = EliteActionStage::Telegraph;
    _remaining = std::max(0.f, timing.telegraph);
    // A zero-length telegraph is allowed but the caller still sees the stage
    // once; the next Update advances it immediately.
}

bool EliteActionClock::Update(float deltaTime)
{
    if (_stage == EliteActionStage::Ready)
        return false;

    _remaining -= deltaTime;
    if (_remaining > 0.f)
        return false;

    // Carry leftover time into the next stage so large frames stay accurate.
    float overshoot = -_remaining;
    switch (_stage)
    {
    case EliteActionStage::Telegraph:
        _stage = EliteActionStage::Active;
        _remaining = std::max(0.f, _timing.active) - overshoot;
        break;
    case EliteActionStage::Active:
        _stage = EliteActionStage::Recovery;
        _remaining = std::max(0.f, _timing.recovery) - overshoot;
        break;
    case EliteActionStage::Recovery:
    default:
        _stage = EliteActionStage::Ready;
        _remaining = 0.f;
        break;
    }
    return true;
}

void EliteActionClock::Cancel()
{
    _stage = EliteActionStage::Ready;
    _remaining = 0.f;
}

float EliteActionClock::GetStageProgress() const
{
    float total = 0.f;
    switch (_stage)
    {
    case EliteActionStage::Telegraph: total = _timing.telegraph; break;
    case EliteActionStage::Active:    total = _timing.active;    break;
    case EliteActionStage::Recovery:  total = _timing.recovery;  break;
    default: return 0.f;
    }
    if (total <= 0.f)
        return 1.f;
    return std::clamp(1.f - _remaining / total, 0.f, 1.f);
}

// ── EliteEventQueue ──────────────────────────────────────────────────────────

bool EliteEventQueue::Push(const EliteSignatureEvent& event)
{
    if (_count >= kCapacity)
        return false;
    _items[(_head + _count) % kCapacity] = event;
    _count++;
    return true;
}

bool EliteEventQueue::Pop(EliteSignatureEvent& event)
{
    if (_count <= 0)
        return false;
    event = _items[_head];
    _head = (_head + 1) % kCapacity;
    _count--;
    return true;
}

void EliteEventQueue::Clear()
{
    _head = 0;
    _count = 0;
}

// ── Modifier compatibility ───────────────────────────────────────────────────

namespace
{
    constexpr std::uint8_t Bit(EliteModifier modifier)
    {
        return (std::uint8_t)(1u << (int)modifier);
    }

    // One mask per archetype, in EliteArchetype order (matches the design doc).
    constexpr std::uint8_t kCompatible[(int)EliteArchetype::Count] = {
        (std::uint8_t)(Bit(EliteModifier::Cage) | Bit(EliteModifier::GuardLinks) | Bit(EliteModifier::Enrage)),          // Ogre
        (std::uint8_t)(Bit(EliteModifier::GuardLinks) | Bit(EliteModifier::Enrage) | Bit(EliteModifier::ArenaPressure)), // Infernal
        (std::uint8_t)(Bit(EliteModifier::Cage) | Bit(EliteModifier::GuardLinks) | Bit(EliteModifier::ArenaPressure)),   // Bonechill
        (std::uint8_t)(Bit(EliteModifier::GuardLinks) | Bit(EliteModifier::Enrage) | Bit(EliteModifier::ArenaPressure)), // Stormclub
        (std::uint8_t)(Bit(EliteModifier::Enrage) | Bit(EliteModifier::ArenaPressure)),                                  // Venomfang
    };
}

bool IsEliteModifierCompatible(EliteArchetype archetype, EliteModifier modifier)
{
    if ((int)archetype < 0 || archetype >= EliteArchetype::Count)
        return false;
    if ((int)modifier < 0 || modifier >= EliteModifier::Count)
        return false;
    return (kCompatible[(int)archetype] & Bit(modifier)) != 0;
}

EliteModifier ChooseEliteModifier(EliteArchetype archetype, std::uint32_t seed,
                                  int forcedModifier)
{
    if ((int)archetype < 0 || archetype >= EliteArchetype::Count)
        return EliteModifier::Enrage;   // safe universal fallback

    if (forcedModifier >= 0 && forcedModifier < (int)EliteModifier::Count &&
        IsEliteModifierCompatible(archetype, (EliteModifier)forcedModifier))
        return (EliteModifier)forcedModifier;

    // Collect the compatible set, then index it with the seed so every result
    // is compatible by construction (no rejection loops, fully deterministic).
    EliteModifier compatible[(int)EliteModifier::Count];
    int compatibleCount = 0;
    for (int i = 0; i < (int)EliteModifier::Count; ++i)
        if (IsEliteModifierCompatible(archetype, (EliteModifier)i))
            compatible[compatibleCount++] = (EliteModifier)i;

    if (compatibleCount == 0)
        return EliteModifier::Enrage;

    // Cheap integer hash so consecutive room indices don't pick a pattern.
    std::uint32_t hashed = seed;
    hashed ^= hashed >> 16; hashed *= 0x7feb352du;
    hashed ^= hashed >> 15; hashed *= 0x846ca68bu;
    hashed ^= hashed >> 16;
    return compatible[hashed % (std::uint32_t)compatibleCount];
}

// ── Damage / phase helpers ───────────────────────────────────────────────────

int ApplyGuardLinkReduction(int damage)
{
    if (damage <= 0)
        return damage;
    int reduced = (int)std::ceil((float)damage * Balance::Elite::kGuardLinksDamageTaken);
    return std::max(1, reduced);
}

bool ShouldEnterElitePhaseTwo(bool alreadyLatched, float health, float maxHealth)
{
    if (alreadyLatched || maxHealth <= 0.f)
        return false;
    return health <= maxHealth * Balance::Elite::kPhaseThreshold;
}

Vector2 RotateVector(Vector2 vector, float radians)
{
    const float cosine = std::cos(radians);
    const float sine   = std::sin(radians);
    return Vector2{ vector.x * cosine - vector.y * sine,
                    vector.x * sine + vector.y * cosine };
}

Vector2 EliteSpreadDirection(Vector2 baseDirection, int index, int count,
                             float totalSpreadRadians)
{
    if (count <= 1)
        return baseDirection;
    const float step = totalSpreadRadians / (float)(count - 1);
    const float angle = -totalSpreadRadians * 0.5f + step * (float)index;
    return RotateVector(baseDirection, angle);
}

int ApplyBonechillFrontReduction(int damage)
{
    if (damage <= 0)
        return damage;
    int reduced = (int)std::ceil((float)damage * Balance::Elite::kBonechillFrontDamageTaken);
    return std::max(1, reduced);
}

int NextOgreChargeCount(bool phaseTwo)
{
    return phaseTwo ? 2 : 1;
}

bool ShouldEndOgreChargeSequence(int remainingCharges, bool hitWall)
{
    return hitWall || remainingCharges <= 0;
}

float DistancePointToSegment(Vector2 point, Vector2 start, Vector2 end)
{
    const float segmentX = end.x - start.x;
    const float segmentY = end.y - start.y;
    const float lengthSquared = segmentX * segmentX + segmentY * segmentY;

    float t = 0.f;
    if (lengthSquared > 0.0001f)
    {
        t = ((point.x - start.x) * segmentX + (point.y - start.y) * segmentY) / lengthSquared;
        t = std::clamp(t, 0.f, 1.f);
    }
    const float closestX = start.x + segmentX * t;
    const float closestY = start.y + segmentY * t;
    const float deltaX = point.x - closestX;
    const float deltaY = point.y - closestY;
    return std::sqrt(deltaX * deltaX + deltaY * deltaY);
}
