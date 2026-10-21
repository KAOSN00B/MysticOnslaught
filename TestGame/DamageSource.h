#pragma once

#include <string>
#include <utility>

// ─────────────────────────────────────────────────────────────────────────────
// DAMAGE SOURCE — what hurt the player: the run log's cause of death, and the
// death lessons built on it later.
//
// Ids are stable names. Enemies use their Bestiary type ("SkeletonArcher",
// "Ogre", "Slime"); hazards and statuses use the names below. Whoever damages
// the player names itself (Character::TakeDamageFrom, or a DamageSourceScope
// around an enemy's turn), and burn and poison ticks report as a status
// together with whoever applied it.
// ─────────────────────────────────────────────────────────────────────────────

enum class DamageSourceKind { Unknown, Enemy, Hazard, Status };

struct DamageSource
{
    DamageSourceKind kind = DamageSourceKind::Unknown;
    std::string id;         // enemy type, hazard or status name
    bool elite = false;     // an elite (or one of its signature attacks)
    bool boss  = false;
    std::string by;         // statuses: the id of whatever applied it ("Infernal", "LavaPit")

    static DamageSource FromEnemy(std::string type, bool isElite, bool isBoss)
    {
        DamageSource source;
        source.kind = DamageSourceKind::Enemy;
        source.id = std::move(type);
        source.elite = isElite;
        source.boss = isBoss;
        return source;
    }

    static DamageSource FromHazard(const char* name)
    {
        DamageSource source;
        source.kind = DamageSourceKind::Hazard;
        source.id = name;
        return source;
    }

    // A status tick, credited to whoever applied the status.
    static DamageSource FromStatus(const char* name, const DamageSource& appliedBy)
    {
        DamageSource source;
        source.kind = DamageSourceKind::Status;
        source.id = name;
        source.elite = appliedBy.elite;
        source.boss = appliedBy.boss;
        source.by = appliedBy.id;
        return source;
    }
};

// Stable hazard and status names.
namespace DamageNames
{
    constexpr const char* kLavaPool       = "LavaPool";        // room hazard floor zone
    constexpr const char* kFireTotem      = "FireTotem";       // room hazard turret bolts
    constexpr const char* kFireballTorch  = "FireballTorch";   // room hazard lane bolts
    constexpr const char* kPit            = "Pit";             // falling off the floor
    constexpr const char* kLavaPit        = "LavaPit";         // falling into lava
    constexpr const char* kWastelandBlast = "WastelandBlast";  // Wastelands ground eruptions
    constexpr const char* kLostCityBeam   = "LostCityBeam";    // Lost City sweeping beams
    constexpr const char* kEliteCage      = "EliteCage";       // stepping outside an elite's cage
    constexpr const char* kVolatileCorpse = "VolatileCorpse";  // Volatile rooms: corpse clouds
    constexpr const char* kBurn           = "Burn";
    constexpr const char* kPoison         = "Poison";
}

inline const char* DamageSourceKindName(DamageSourceKind kind)
{
    switch (kind)
    {
    case DamageSourceKind::Enemy:  return "enemy";
    case DamageSourceKind::Hazard: return "hazard";
    case DamageSourceKind::Status: return "status";
    default:                       return "unknown";
    }
}
