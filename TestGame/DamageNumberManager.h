#pragma once

#include "raylib.h"

#include <array>
#include <algorithm>
#include <cmath>
#include <cstdint>

inline int CalculateActualDamage(float healthBefore, float healthAfter)
{
    return (int)std::lround(std::max(0.f, healthBefore - healthAfter));
}

enum class DamageNumberOutcome : unsigned char
{
    Normal,
    Critical,
    Blocked,
    Immune,
    Invulnerable,
    Dodge,
    Armour,
    Airborne,
    Underground,
    Incoming
};

struct DamageNumberEvent
{
    std::uint64_t targetId = 0;
    std::uint32_t attackId = 0;
    Vector2 worldPos{};
    int finalDamage = 0;
    DamageNumberOutcome outcome = DamageNumberOutcome::Normal;
    bool backstab = false;
    bool killingBlow = false;
    bool elite = false;
    bool boss = false;
    bool damageOverTime = false;
};

struct DamageNumberSettings
{
    bool enabled = true;
    bool freezeAnimation = false;
    int visibleCap = 32;
    int minFontSize = 20;
    int maxFontSize = 42;
    float damageReference = 12.f;
    float riseSpeed = 92.f;
    float horizontalDrift = 22.f;
    float lifetime = 0.90f;
    float outlineOffset = 2.f;
    float mergeWindow = 0.20f;
};

struct DamageNumberStats
{
    int capacity = 96;
    int active = 0;
    int visible = 0;
    int mergeCandidates = 0;
    std::uint64_t submitted = 0;
    std::uint64_t merged = 0;
    std::uint64_t suppressed = 0;
    std::uint64_t replaced = 0;
    int highWater = 0;
};

class DamageNumberManager
{
public:
    static constexpr int kCapacity = 96;

    void Init(Font font = {});
    void Submit(const DamageNumberEvent& event);
    void Update(float dt);
    void Draw(Vector2 worldOffset) const;
    void Clear();

    DamageNumberSettings& GetSettings() { return _settings; }
    const DamageNumberSettings& GetSettings() const { return _settings; }
    const DamageNumberStats& GetStats() const { return _stats; }

    int DebugValueForTarget(std::uint64_t targetId, DamageNumberOutcome outcome) const;
    int DebugOutcomeCount(DamageNumberOutcome outcome) const;

private:
    struct Slot
    {
        bool active = false;
        std::uint64_t targetId = 0;
        std::uint32_t attackId = 0;
        std::uint64_t sequence = 0;
        Vector2 worldPos{};
        int value = 0;
        DamageNumberOutcome outcome = DamageNumberOutcome::Normal;
        float age = 0.f;
        float driftSign = 1.f;
        bool backstab = false;
        bool killingBlow = false;
        bool elite = false;
        bool boss = false;
        bool damageOverTime = false;
    };

    bool CanMerge(const Slot& slot, const DamageNumberEvent& event) const;
    int Priority(const Slot& slot) const;
    int Priority(const DamageNumberEvent& event) const;
    int FindFreeSlot() const;
    int FindReplacementSlot(int incomingPriority) const;
    void RefreshStats();
    const char* FormatSlot(const Slot& slot, char (&buffer)[32]) const;
    Color ColorFor(const Slot& slot) const;

    std::array<Slot, kCapacity> _slots{};
    DamageNumberSettings _settings{};
    DamageNumberStats _stats{};
    Font _font{};
    std::uint64_t _sequence = 0;
};
