#pragma once

#include "raylib.h"

#include <array>
#include <cstdint>

// ─────────────────────────────────────────────────────────────────────────────
// EliteSignature — shared, allocation-free primitives for the five elite-room
// miniboss kits (Ogre, Infernal, Bonechill, Stormclub, Venomfang).
//
// Every dangerous elite action follows the same readable sequence:
//   Telegraph → Lock → Execution (Active) → Recovery
// The EliteActionClock walks those stages; elites emit EliteSignatureEvent
// values into a bounded per-enemy queue; CombatDirector consumes the events and
// owns the resulting attack zones, VFX, sounds and player damage.
//
// These types are deliberately pure (no Engine/Enemy dependencies) so they can
// be unit-tested standalone (EliteSignatureTests.cpp) and reused later for
// full Diablo-style boss survival phases.
// ─────────────────────────────────────────────────────────────────────────────

enum class EliteArchetype : std::uint8_t
{
    Ogre, Infernal, Bonechill, Stormclub, Venomfang,
    Count   // "not an elite" — the default for ordinary enemies
};

// Elite-room challenge modifiers. The legacy Leap modifier is gone (movement is
// now part of individual kits) and Guard Links is damage REDUCTION, never
// invulnerability.
enum class EliteModifier : std::int8_t
{
    Random = -1,
    Cage, GuardLinks, Enrage, ArenaPressure,
    Count
};

enum class EliteActionStage : std::uint8_t { Ready, Telegraph, Active, Recovery };

enum class EliteEventKind : std::uint8_t
{
    Telegraph,    // warning appears — target area / direction becomes clear
    Lock,         // direction or landing point committed
    Execute,      // damage/movement happens exactly where warned
    Recover,      // punish window begins
    PhaseChange,  // one-time 50% health escalation announcement
    TrailPatch    // lingering hazard patch dropped along a path
};

enum class EliteMove : std::uint8_t
{
    None,
    OgreCharge,
    InfernalCinderMarch,
    InfernalFurnaceBurst,
    BonechillPermafrostSlam,
    StormclubThunderLeap,
    VenomfangPounce,
    // Boss encounter-pattern moves share the same event/zone pipeline.
    MolarbeastCharge,      // set-piece lane charge (telegraph → lock → dash)
    MolarbeastLavaTrail,   // burning ground left along a finished charge lane
    WerewolfClawLane       // sequential claw slash lanes with safe gaps
};

enum class EliteZoneShape : std::uint8_t { Disc, Lane, Cone };

enum class EliteStatusPayload : std::uint8_t { None, Burn, Chill, Shock, Poison, Knockback };

// Telegraph / active / recovery durations in seconds for one authored action.
struct EliteActionTiming
{
    float telegraph = 0.f;
    float active    = 0.f;
    float recovery  = 0.f;
};

// Walks Ready → Telegraph → Active → Recovery → Ready.
//
// NOTE: the five live elites deliberately own their own bespoke state machines
// (charge/march/leap/pounce need custom movement per stage); this clock is the
// shared sequence primitive for the upcoming boss encounter-pattern system.
//
// Update processes EVERY stage boundary the elapsed time crosses — one severe
// frame hitch can legally cross telegraph AND active — and returns how many
// boundaries were crossed so the caller can emit each beat exactly once.
// Zero-duration stages are crossed immediately and can never loop forever.
class EliteActionClock
{
public:
    void Start(EliteActionTiming timing);
    int  Update(float deltaTime);   // number of stage boundaries crossed (0..3)
    void Cancel();                  // safe abort back to Ready (phase change, death)
    EliteActionStage GetStage() const { return _stage; }
    float GetStageProgress() const;                    // 0..1 within the current stage
    float GetStageRemaining() const { return _remaining; }

private:
    EliteActionTiming _timing{};
    EliteActionStage  _stage = EliteActionStage::Ready;
    float             _remaining = 0.f;
};

// One authored combat beat, emitted by an elite and consumed by CombatDirector.
struct EliteSignatureEvent
{
    EliteEventKind kind      = EliteEventKind::Telegraph;
    EliteArchetype archetype = EliteArchetype::Ogre;
    EliteMove      move      = EliteMove::None;
    std::uint32_t  sequence  = 0;
    Vector2        origin{};
    Vector2        target{};
    Vector2        direction{ 1.f, 0.f };
    int            phase     = 0;
};

// One live danger area owned by CombatDirector. Zones snapshot world positions
// when spawned and never follow the elite afterward — what was warned is what
// hits.
struct EliteAttackZone
{
    bool               active = false;
    std::uint32_t      sequence = 0;
    EliteArchetype     owner  = EliteArchetype::Ogre;
    EliteMove          move   = EliteMove::None;
    EliteZoneShape     shape  = EliteZoneShape::Disc;
    EliteStatusPayload status = EliteStatusPayload::None;
    Vector2            start{};
    Vector2            end{};
    float              radius = 0.f;             // disc radius / lane half-width
    float              halfAngleRadians = 0.f;   // cones only
    float              telegraphRemaining = 0.f; // no damage until this reaches zero
    float              telegraphDuration = 0.f;  // authored warning length (drives the arm-up ramp)
    float              activeRemaining = 0.f;
    float              tickInterval = 0.f;       // 0 = one-shot zone
    float              tickRemaining = 0.f;
    float              damage = 0.f;
    bool               hitPlayer = false;        // one-shot zones hit at most once
};

// Debug/QA snapshot of one elite's signature runtime.
struct EliteSignatureTelemetry
{
    EliteActionStage stage = EliteActionStage::Ready;
    float   cooldown = 0.f;
    Vector2 lockedTarget{};
    int     phase = 0;
    int     casts = 0;
    int     hits = 0;
    int     droppedEvents = 0;
};

// Fixed-capacity FIFO. Push returns false (and the caller counts a drop) when
// full — the queue never allocates and never grows.
class EliteEventQueue
{
public:
    static constexpr int kCapacity = 12;

    bool Push(const EliteSignatureEvent& event);
    bool Pop(EliteSignatureEvent& event);
    void Clear();
    int  GetCount() const { return _count; }

private:
    std::array<EliteSignatureEvent, kCapacity> _items{};
    int _head = 0;
    int _count = 0;
};

// ── Modifier compatibility (mirrors the design spec's matrix) ────────────────
//   Ogre:      Cage, GuardLinks, Enrage
//   Infernal:  GuardLinks, Enrage, ArenaPressure
//   Bonechill: Cage, GuardLinks, ArenaPressure
//   Stormclub: GuardLinks, Enrage, ArenaPressure
//   Venomfang: Enrage, ArenaPressure
bool IsEliteModifierCompatible(EliteArchetype archetype, EliteModifier modifier);

// Shared display strings for the four modifiers — the ONE source every HUD
// surface (entrance banner, room badge, room intro, debug panel, telemetry)
// reads, so labels can never drift from the real rules again.
const char* EliteModifierName(int modifier);        // "Guard Links"
const char* EliteModifierShortName(int modifier);   // "GUARDED"
const char* EliteModifierCondition(int modifier);   // banner condition line

// Deterministically picks a compatible modifier for this archetype from `seed`.
// A forced modifier (debug panel) is honoured only when compatible; otherwise
// the seeded compatible choice is used. Never returns an incompatible value.
EliteModifier ChooseEliteModifier(EliteArchetype archetype, std::uint32_t seed,
                                  int forcedModifier = -1);

// Guard Links: linked elites take 40% of resolved damage, minimum 1 for any
// positive hit — reduction the player can SEE, never silent immunity.
int ApplyGuardLinkReduction(int damage);

// One-time 50% phase latch. Returns true only the first time health crosses
// the threshold; a zero/negative max health can never trigger it.
bool ShouldEnterElitePhaseTwo(bool alreadyLatched, float health, float maxHealth);

// Distance from a point to a line segment (lane/trail hit tests and warnings).
float DistancePointToSegment(Vector2 point, Vector2 start, Vector2 end);

// Rotate a vector by an angle (fissure/lane fans, branch spreads).
Vector2 RotateVector(Vector2 vector, float radians);

// Evenly fans `count` directions across `totalSpreadRadians`, centred on
// `baseDirection` (index 0 = most counter-clockwise). Used for Infernal
// fissures, Bonechill ice lanes and Stormclub lightning branches, so every
// multi-lane attack keeps authored, walkable gaps.
Vector2 EliteSpreadDirection(Vector2 baseDirection, int index, int count,
                             float totalSpreadRadians);

// Bonechill frontal frost armour: 45% reduction — ceil keeps chip damage real,
// so the armour NEVER reduces a positive hit to zero.
int ApplyBonechillFrontReduction(int damage);

// ── Ogre charge sequencing (pure, testable) ─────────────────────────────────
// SECOND WIND: phase two performs two charges; a wall impact always ends the
// whole sequence immediately (into the stun punish window).
int  NextOgreChargeCount(bool phaseTwo);
bool ShouldEndOgreChargeSequence(int remainingCharges, bool hitWall);
