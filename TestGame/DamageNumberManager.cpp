#include "DamageNumberManager.h"
#include "VirtualCanvas.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>

namespace
{
    // The life of a number (t = age / lifetime, 0..1): it spawns oversized and
    // settles fast (pop), rises with an ease-out, holds full opacity, then fades
    // smoothly. Crits are bigger, flash white then settle to gold, and shake
    // while they flash. Hits on the hero are red.
    constexpr float kPopDuration        = 0.16f;   // share of the life spent settling
    constexpr float kPopNormal          = 0.40f;   // extra size at spawn
    constexpr float kPopCritical        = 0.85f;
    constexpr float kPopDamageOverTime  = 0.15f;
    constexpr float kPopKillingBlow     = 0.20f;   // added on a kill
    constexpr float kCritSize           = 1.40f;
    constexpr float kKillingBlowSize    = 1.12f;
    constexpr float kDamageOverTimeSize = 0.85f;
    constexpr float kSizeCap            = 1.55f;   // x maxFontSize
    constexpr float kCritSizeCap        = 2.10f;
    constexpr float kCritRise           = 1.20f;
    constexpr float kFadeStart          = 0.55f;   // full opacity until here
    constexpr float kCritFlash          = 0.22f;   // share of the life spent going white -> gold
    constexpr float kCritShakePixels    = 3.f;
    constexpr float kCritShakeSpeed     = 70.f;    // radians per second
    constexpr float kShadowAlpha        = 0.8f;

    constexpr Color kNormalColor   = { 255, 248, 235, 255 };   // warm white
    constexpr Color kCriticalColor = { 255, 205, 45, 255 };    // gold
    constexpr Color kFlashColor    = { 255, 255, 255, 255 };
    constexpr Color kIncomingColor = { 235, 55, 50, 255 };     // hits on the hero
    constexpr Color kStatusColor   = { 195, 205, 225, 255 };   // BLOCKED, IMMUNE, ...

    float EaseOutCubic(float x)
    {
        const float u = 1.f - x;
        return 1.f - u * u * u;
    }

    float SmoothStep(float x) { return x * x * (3.f - 2.f * x); }

    unsigned char LerpChannel(unsigned char a, unsigned char b, float t)
    {
        return (unsigned char)std::lround(a + (b - a) * t);
    }

    Color LerpColor(Color a, Color b, float t)
    {
        return Color{ LerpChannel(a.r, b.r, t), LerpChannel(a.g, b.g, t),
                      LerpChannel(a.b, b.b, t), LerpChannel(a.a, b.a, t) };
    }
}

void DamageNumberManager::Init(Font font)
{
    _font = font;
    _stats.capacity = kCapacity;
    Clear();
}

bool DamageNumberManager::CanMerge(const Slot& slot, const DamageNumberEvent& event) const
{
    if (!slot.active || slot.targetId != event.targetId || slot.attackId != event.attackId)
        return false;
    if (slot.outcome != event.outcome || slot.damageOverTime != event.damageOverTime)
        return false;
    if (slot.age > _settings.mergeWindow)
        return false;
    if (event.outcome == DamageNumberOutcome::Critical || event.backstab || slot.backstab)
        return false;
    return event.outcome == DamageNumberOutcome::Normal || event.damageOverTime;
}

int DamageNumberManager::Priority(const DamageNumberEvent& event) const
{
    if (event.killingBlow && event.boss) return 100;
    if (event.killingBlow && event.elite) return 95;
    if (event.outcome == DamageNumberOutcome::Critical && event.killingBlow) return 90;
    if (event.backstab || event.outcome == DamageNumberOutcome::Critical) return 80;
    if (event.killingBlow) return 70;
    if (event.outcome != DamageNumberOutcome::Normal && event.outcome != DamageNumberOutcome::Incoming) return 60;
    if (event.outcome == DamageNumberOutcome::Normal && !event.damageOverTime) return 40;
    if (event.outcome == DamageNumberOutcome::Incoming) return 35;
    return 20;
}

int DamageNumberManager::Priority(const Slot& slot) const
{
    DamageNumberEvent event{};
    event.outcome = slot.outcome;
    event.backstab = slot.backstab;
    event.killingBlow = slot.killingBlow;
    event.elite = slot.elite;
    event.boss = slot.boss;
    event.damageOverTime = slot.damageOverTime;
    return Priority(event);
}

int DamageNumberManager::FindFreeSlot() const
{
    for (int i = 0; i < kCapacity; ++i)
        if (!_slots[i].active) return i;
    return -1;
}

int DamageNumberManager::FindReplacementSlot(int incomingPriority) const
{
    int candidate = -1;
    int lowestPriority = std::numeric_limits<int>::max();
    for (int i = 0; i < kCapacity; ++i)
    {
        int priority = Priority(_slots[i]);
        if (priority < incomingPriority && priority < lowestPriority)
        {
            candidate = i;
            lowestPriority = priority;
        }
    }
    return candidate;
}

void DamageNumberManager::Submit(const DamageNumberEvent& event)
{
    ++_stats.submitted;
    if (!_settings.enabled)
    {
        ++_stats.suppressed;
        return;
    }
    if (event.finalDamage <= 0 && event.outcome == DamageNumberOutcome::Normal)
    {
        ++_stats.suppressed;
        return;
    }

    for (Slot& slot : _slots)
    {
        if (!CanMerge(slot, event)) continue;
        slot.value += event.finalDamage;
        slot.worldPos.x = (slot.worldPos.x + event.worldPos.x) * 0.5f;
        slot.worldPos.y = (slot.worldPos.y + event.worldPos.y) * 0.5f;
        slot.killingBlow = slot.killingBlow || event.killingBlow;
        slot.elite = slot.elite || event.elite;
        slot.boss = slot.boss || event.boss;
        slot.age = 0.f;
        ++_stats.merged;
        RefreshStats();
        return;
    }

    int index = FindFreeSlot();
    if (index < 0)
    {
        index = FindReplacementSlot(Priority(event));
        if (index < 0)
        {
            ++_stats.suppressed;
            return;
        }
        ++_stats.replaced;
    }

    Slot& slot = _slots[index];
    slot = {};
    slot.active = true;
    slot.targetId = event.targetId;
    slot.attackId = event.attackId;
    slot.sequence = ++_sequence;
    slot.worldPos = event.worldPos;
    slot.value = event.finalDamage;
    slot.outcome = event.outcome;
    slot.driftSign = ((event.targetId ^ event.attackId ^ slot.sequence) & 1ULL) ? 1.f : -1.f;
    slot.backstab = event.backstab;
    slot.killingBlow = event.killingBlow;
    slot.elite = event.elite;
    slot.boss = event.boss;
    slot.damageOverTime = event.damageOverTime;
    RefreshStats();
}

void DamageNumberManager::Update(float dt)
{
    if (_settings.freezeAnimation)
    {
        RefreshStats();
        return;
    }
    for (Slot& slot : _slots)
    {
        if (!slot.active) continue;
        slot.age += dt;
        if (slot.age >= _settings.lifetime)
            slot.active = false;
    }
    RefreshStats();
}

const char* DamageNumberManager::FormatSlot(const Slot& slot, char (&buffer)[32]) const
{
    const char* label = nullptr;
    switch (slot.outcome)
    {
    case DamageNumberOutcome::Blocked:      label = "BLOCKED"; break;
    case DamageNumberOutcome::Immune:       label = "IMMUNE"; break;
    case DamageNumberOutcome::Invulnerable: label = "INVULNERABLE"; break;
    case DamageNumberOutcome::Dodge:        label = "DODGE"; break;
    case DamageNumberOutcome::Airborne:     label = "AIRBORNE"; break;
    case DamageNumberOutcome::Underground:  label = "UNDERGROUND"; break;
    default: break;
    }
    if (label != nullptr)
    {
        std::snprintf(buffer, sizeof(buffer), "%s", label);
        return buffer;
    }
    if (slot.backstab)
        std::snprintf(buffer, sizeof(buffer), "%d BACKSTAB", slot.value);
    else if (slot.outcome == DamageNumberOutcome::Critical)
        std::snprintf(buffer, sizeof(buffer), "%d!", slot.value);
    else
        std::snprintf(buffer, sizeof(buffer), "%d", slot.value);
    return buffer;
}

Color DamageNumberManager::ColorFor(const Slot& slot) const
{
    switch (slot.outcome)
    {
    case DamageNumberOutcome::Critical:     return kCriticalColor;
    case DamageNumberOutcome::Incoming:     return kIncomingColor;
    case DamageNumberOutcome::Blocked:
    case DamageNumberOutcome::Immune:
    case DamageNumberOutcome::Invulnerable:
    case DamageNumberOutcome::Dodge:
    case DamageNumberOutcome::Airborne:
    case DamageNumberOutcome::Underground:  return kStatusColor;
    default:                                return kNormalColor;
    }
}

void DamageNumberManager::Draw(Vector2 worldOffset) const
{
    if (!_settings.enabled) return;

    std::array<int, kCapacity> ordered{};
    int count = 0;
    for (int i = 0; i < kCapacity; ++i)
        if (_slots[i].active) ordered[count++] = i;
    std::sort(ordered.begin(), ordered.begin() + count, [&](int a, int b) {
        int pa = Priority(_slots[a]);
        int pb = Priority(_slots[b]);
        return pa == pb ? _slots[a].sequence < _slots[b].sequence : pa > pb;
    });

    int visible = std::min(count, std::max(0, _settings.visibleCap));
    Font font = (_font.texture.id != 0) ? _font : GetFontDefault();
    for (int order = 0; order < visible; ++order)
    {
        const Slot& slot = _slots[ordered[order]];
        const bool crit = slot.outcome == DamageNumberOutcome::Critical;
        const float life = std::max(0.01f, _settings.lifetime);
        const float t = std::clamp(slot.age / life, 0.f, 1.f);
        const float damageT = std::clamp(slot.value / std::max(1.f, _settings.damageReference), 0.f, 1.f);
        float size = _settings.minFontSize + (_settings.maxFontSize - _settings.minFontSize) * damageT;
        if (crit) size *= kCritSize;
        if (slot.killingBlow) size *= kKillingBlowSize;
        if (slot.damageOverTime) size *= kDamageOverTimeSize;

        // Pop: spawn oversized, settle with an ease-out.
        float pop = slot.damageOverTime ? kPopDamageOverTime : crit ? kPopCritical : kPopNormal;
        if (slot.killingBlow) pop += kPopKillingBlow;
        const float settle = EaseOutCubic(std::min(1.f, t / kPopDuration));
        size = std::min(size * (1.f + pop * (1.f - settle)), _settings.maxFontSize * (crit ? kCritSizeCap : kSizeCap));

        // Rise fast then hang; hold full opacity, then fade out smoothly.
        const float travel = EaseOutCubic(t);
        const float rise = _settings.riseSpeed * travel * (crit ? kCritRise : 1.f);
        const float drift = slot.driftSign * _settings.horizontalDrift * travel;
        const float alpha = 1.f - SmoothStep(std::clamp((t - kFadeStart) / (1.f - kFadeStart), 0.f, 1.f));

        // Crits flash white, settle to gold, and shake while they flash.
        const float flash = crit ? std::min(1.f, t / kCritFlash) : 1.f;
        const float shake = (flash < 1.f) ? std::sin(slot.age * kCritShakeSpeed) * kCritShakePixels * (1.f - flash) : 0.f;
        Color color = ColorFor(slot);
        if (crit) color = LerpColor(kFlashColor, color, flash);

        Vector2 pos{
            slot.worldPos.x + worldOffset.x + kVirtualWidth * 0.5f + drift + shake,
            slot.worldPos.y + worldOffset.y + kVirtualHeight * 0.5f - rise
        };
        char buffer[32]{};
        const char* text = FormatSlot(slot, buffer);
        Vector2 measured = MeasureTextEx(font, text, size, 0.f);
        pos.x -= measured.x * 0.5f;
        pos.y -= measured.y * 0.5f;   // grow from the centre, not the top edge
        float outline = std::max(1.f, _settings.outlineOffset);
        Color shadow = Fade(BLACK, alpha * kShadowAlpha);
        DrawTextEx(font, text, { pos.x - outline, pos.y }, size, 0.f, shadow);
        DrawTextEx(font, text, { pos.x + outline, pos.y }, size, 0.f, shadow);
        DrawTextEx(font, text, { pos.x, pos.y - outline }, size, 0.f, shadow);
        DrawTextEx(font, text, { pos.x, pos.y + outline }, size, 0.f, shadow);
        DrawTextEx(font, text, pos, size, 0.f, Fade(color, alpha));
    }
}

void DamageNumberManager::Clear()
{
    for (Slot& slot : _slots) slot = {};
    _sequence = 0;
    _stats = {};
    _stats.capacity = kCapacity;
}

void DamageNumberManager::RefreshStats()
{
    int active = 0;
    int mergeCandidates = 0;
    for (const Slot& slot : _slots)
    {
        if (!slot.active) continue;
        ++active;
        if (slot.age <= _settings.mergeWindow) ++mergeCandidates;
    }
    _stats.active = active;
    _stats.visible = std::min(active, std::max(0, _settings.visibleCap));
    _stats.mergeCandidates = mergeCandidates;
    _stats.highWater = std::max(_stats.highWater, active);
}

int DamageNumberManager::DebugValueForTarget(std::uint64_t targetId, DamageNumberOutcome outcome) const
{
    for (const Slot& slot : _slots)
        if (slot.active && slot.targetId == targetId && slot.outcome == outcome)
            return slot.value;
    return 0;
}

int DamageNumberManager::DebugOutcomeCount(DamageNumberOutcome outcome) const
{
    int count = 0;
    for (const Slot& slot : _slots)
        if (slot.active && slot.outcome == outcome) ++count;
    return count;
}
