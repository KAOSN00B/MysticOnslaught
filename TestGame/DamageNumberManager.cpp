#include "DamageNumberManager.h"
#include "VirtualCanvas.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>

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
    case DamageNumberOutcome::Armour:       label = "ARMOUR"; break;
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
    else
        std::snprintf(buffer, sizeof(buffer), "%d", slot.value);
    return buffer;
}

Color DamageNumberManager::ColorFor(const Slot& slot) const
{
    switch (slot.outcome)
    {
    case DamageNumberOutcome::Critical:     return Color{ 255, 205, 45, 255 };
    case DamageNumberOutcome::Incoming:     return Color{ 255, 175, 195, 255 };
    case DamageNumberOutcome::Armour:       return Color{ 130, 205, 255, 255 };
    case DamageNumberOutcome::Blocked:
    case DamageNumberOutcome::Immune:
    case DamageNumberOutcome::Invulnerable:
    case DamageNumberOutcome::Dodge:
    case DamageNumberOutcome::Airborne:
    case DamageNumberOutcome::Underground:  return Color{ 195, 205, 225, 255 };
    default:                                return Color{ 230, 45, 50, 255 };
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
        float life = std::max(0.01f, _settings.lifetime);
        float t = std::clamp(slot.age / life, 0.f, 1.f);
        float damageT = std::clamp(slot.value / std::max(1.f, _settings.damageReference), 0.f, 1.f);
        float size = _settings.minFontSize + (_settings.maxFontSize - _settings.minFontSize) * damageT;
        if (slot.outcome == DamageNumberOutcome::Critical) size *= 1.25f;
        if (slot.killingBlow) size *= 1.12f;
        float punch = t < 0.16f ? 1.f + (0.16f - t) / 0.16f * (slot.killingBlow ? 0.55f : 0.35f) : 1.f;
        size = std::min(size * punch, _settings.maxFontSize * 1.55f);

        float rise = _settings.riseSpeed * t * (slot.outcome == DamageNumberOutcome::Critical ? 1.2f : 1.f);
        float drift = slot.driftSign * _settings.horizontalDrift * t;
        Vector2 pos{
            slot.worldPos.x + worldOffset.x + kVirtualWidth * 0.5f + drift,
            slot.worldPos.y + worldOffset.y + kVirtualHeight * 0.5f - rise
        };
        char buffer[32]{};
        const char* text = FormatSlot(slot, buffer);
        Vector2 measured = MeasureTextEx(font, text, size, 0.f);
        pos.x -= measured.x * 0.5f;
        float alpha = 1.f - t * t;
        float outline = std::max(1.f, _settings.outlineOffset);
        Color shadow = Fade(BLACK, alpha * 0.8f);
        DrawTextEx(font, text, { pos.x - outline, pos.y }, size, 0.f, shadow);
        DrawTextEx(font, text, { pos.x + outline, pos.y }, size, 0.f, shadow);
        DrawTextEx(font, text, { pos.x, pos.y - outline }, size, 0.f, shadow);
        DrawTextEx(font, text, { pos.x, pos.y + outline }, size, 0.f, shadow);
        DrawTextEx(font, text, pos, size, 0.f, Fade(ColorFor(slot), alpha));
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
