#pragma warning(disable: 4996)   // fopen/fscanf are fine here; paths are internal

#include "AttackTuning.h"
#include "TuningFiles.h"

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <unordered_map>

const char* AttackOwnerForAbility(int idx)
{
    if (idx < 9)  return "Mage";
    if (idx < 18) return "Warrior";
    if (idx < 27) return "Rogue";
    if (idx < 36) return "Hunter";
    if (idx < 45) return "Paladin";
    return "Warlock";
}

std::string AttackSanitise(const std::string& s)
{
    std::string out;
    for (char c : s)
        out += (c == ' ' || c == '/' || c == '\\' || c == ':') ? '_' : c;
    return out;
}

std::string AttackTuningKey(const std::string& owner, const std::string& name)
{
    return AttackSanitise(owner + "_" + name);
}

std::string AttackTuningKeyForAbility(AbilityType ability)
{
    int i = (int)ability;
    // Mage spells were redesigned without changing their enum/save identities.
    // Keep their original tuning filenames so existing authored fire points and
    // projectile values remain live under the new player-facing names.
    static const char* kMageLegacyNames[9] = {
        "Fire Spread", "Ice Spread", "Electric Spread",
        "Fire Bolt", "Ice Bolt", "Electric Bolt",
        "Fire Ultimate", "Ice Ultimate", "Electric Ultimate"
    };
    if (i >= 0 && i < 9)
        return AttackTuningKey("Mage", kMageLegacyNames[i]);
    // Preserve existing authored tuning after the player-facing rename from
    // Shield Bash to Shoulder Charge.
    if (ability == AbilityType::ShieldBash)
        return AttackTuningKey(AttackOwnerForAbility(i), "Shield Bash");
    return AttackTuningKey(AttackOwnerForAbility(i), GetAbilityName(ability));
}

std::string AttackTuningKeyForBasic(int playerClass)
{
    // PlayerClass order: Mage, Warrior, Hunter, Rogue, Paladin, Warlock.
    static const char* kClassNames[] = { "Mage", "Warrior", "Hunter", "Rogue", "Paladin", "Warlock" };
    const char* owner = (playerClass >= 0 && playerClass < 6) ? kClassNames[playerClass] : "Mage";
    return AttackTuningKey(owner, "Basic");
}

namespace
{
    std::unordered_map<std::string, AttackTuning> g_cache;
    std::unordered_map<std::string, bool>         g_exists;   // key -> file present?

    void ApplyLine(const char* line, AttackTuning& out)
    {
        char k[64]; char sval[128]; float val;
        if (sscanf(line, "%63[^=]=%127s", k, sval) == 2 && std::strcmp(k, "fx") == 0)
        {
            out.hasFx  = true;
            out.fxStem = (std::strcmp(sval, "none") == 0) ? "" : sval;
            return;
        }
        if (sscanf(line, "%63[^=]=%f", k, &val) == 2)
        {
            if      (std::strcmp(k, "box_x") == 0) { out.x = val; out.hasBox = true; }
            else if (std::strcmp(k, "box_y") == 0) { out.y = val; out.hasBox = true; }
            else if (std::strcmp(k, "box_w") == 0) { out.w = val; out.hasBox = true; }
            else if (std::strcmp(k, "box_h") == 0) { out.h = val; out.hasBox = true; }
            else if (std::strcmp(k, "fx_forward") == 0) { out.fxForward = val; out.hasFxOffset = true; }
            else if (std::strcmp(k, "fx_height")  == 0) { out.fxHeight  = val; out.hasFxOffset = true; }
            else if (std::strcmp(k, "fire_forward") == 0) { out.fireForward = val; out.hasFirePoint = true; }
            else if (std::strcmp(k, "fire_height")  == 0) { out.fireHeight  = val; out.hasFirePoint = true; }
            else if (std::strcmp(k, "proj_scale")   == 0) { out.projScale   = val; out.hasProjectile = true; }
            else if (std::strcmp(k, "proj_radius")  == 0) { out.projRadius  = val; out.hasProjectile = true; }
            else if (std::strcmp(k, "proj_speed")   == 0) { out.projSpeed   = val; out.hasProjectile = true; }
            else if (std::strcmp(k, "proj_life")    == 0) { out.projLifetime= val; out.hasProjectile = true; }
            else if (std::strcmp(k, "cooldown")     == 0) { out.cooldown    = val; out.hasCooldown = true; }
            else if (std::strcmp(k, "aim_range") == 0) { out.aimRange = val; out.hasAbility = true; }
            else if (std::strcmp(k, "area_radius") == 0) { out.areaRadius = val; out.hasAbility = true; }
            else if (std::strcmp(k, "effect_length") == 0) { out.effectLength = val; out.hasAbility = true; }
            else if (std::strcmp(k, "effect_width") == 0) { out.effectWidth = val; out.hasAbility = true; }
            else if (std::strcmp(k, "effect_duration") == 0) { out.effectDuration = val; out.hasAbility = true; }
            else if (std::strcmp(k, "tick_interval") == 0) { out.tickInterval = val; out.hasAbility = true; }
            else if (std::strcmp(k, "chain_range") == 0) { out.chainRange = val; out.hasAbility = true; }
            else if (std::strcmp(k, "max_targets") == 0) { out.maxTargets = val; out.hasAbility = true; }
            else if (std::strcmp(k, "move_distance") == 0) { out.moveDistance = val; out.hasAbility = true; }
            else if (std::strcmp(k, "preview_angle") == 0) { out.previewAngle = val; out.hasAbility = true; }
            else if (std::strcmp(k, "sig_telegraph") == 0) { out.telegraphTime = val; out.hasSignature = true; }
            else if (std::strcmp(k, "sig_active") == 0) { out.activeTime = val; out.hasSignature = true; }
            else if (std::strcmp(k, "sig_recovery") == 0) { out.recoveryTime = val; out.hasSignature = true; }
            else if (std::strcmp(k, "sig_cooldown") == 0) { out.signatureCooldown = val; out.hasSignature = true; }
            else if (std::strcmp(k, "sig_travel") == 0) { out.travelDistance = val; out.hasSignature = true; }
            else if (std::strcmp(k, "sig_phase_speed") == 0) { out.phaseSpeedMult = val; out.hasSignature = true; }
            else if (std::strcmp(k, "sig_phase_cooldown") == 0) { out.phaseCooldownMult = val; out.hasSignature = true; }
            else if (std::strcmp(k, "hit_area") == 0) { out.hasHitArea = (val != 0.f); }
            else if (std::strcmp(k, "hit_radius") == 0) { out.hitRadius = val; }
            else if (std::strcmp(k, "hit_inner") == 0) { out.hitInner = val; }
            else if (std::strcmp(k, "hit_forward") == 0) { out.hitForward = val; }
            else if (std::strcmp(k, "hit_length") == 0) { out.hitLength = val; }
            else if (std::strcmp(k, "hit_width") == 0) { out.hitWidth = val; }
            else if (std::strcmp(k, "hit_x") == 0) { out.hitX = val; }
            else if (std::strcmp(k, "hit_y") == 0) { out.hitY = val; }
            else if (std::strcmp(k, "hit_w") == 0) { out.hitW = val; }
            else if (std::strcmp(k, "hit_h") == 0) { out.hitH = val; }
        }
    }

    bool LoadFile(const std::string& key, AttackTuning& out)
    {
        std::string path = TuningFiles::Path("attacktuning_" + key + ".txt");
        FILE* f = fopen(path.c_str(), "r");
        if (!f) return false;

        char line[256];
        while (fgets(line, sizeof(line), f))
            ApplyLine(line, out);
        fclose(f);
        return true;
    }

    void Append(std::string& out, const char* format, ...)
    {
        char line[160];
        va_list args;
        va_start(args, format);
        vsnprintf(line, sizeof(line), format, args);
        va_end(args);
        out += line;
    }
}

namespace AttackTuningStore
{
    const AttackTuning* Get(const std::string& key)
    {
        auto ex = g_exists.find(key);
        if (ex != g_exists.end())
            return ex->second ? &g_cache[key] : nullptr;

        AttackTuning t;
        bool ok = LoadFile(key, t);
        g_exists[key] = ok;
        if (ok) g_cache[key] = t;
        return ok ? &g_cache[key] : nullptr;
    }

    void Reload(const std::string& key)
    {
        g_exists.erase(key);
        g_cache.erase(key);
    }

    void ReloadAll()
    {
        g_exists.clear();
        g_cache.clear();
    }

    void Parse(const std::string& text, AttackTuning& out)
    {
        size_t start = 0;
        while (start < text.size())
        {
            size_t end = text.find('\n', start);
            if (end == std::string::npos)
                end = text.size();
            ApplyLine(text.substr(start, end - start).c_str(), out);
            start = end + 1;
        }
    }

    std::string Serialize(const AttackTuning& t)
    {
        std::string out;
        if (t.hasBox)
        {
            Append(out, "box_x=%.3f\n", t.x);
            Append(out, "box_y=%.3f\n", t.y);
            Append(out, "box_w=%.3f\n", t.w);
            Append(out, "box_h=%.3f\n", t.h);
        }
        if (t.hasFx)
            Append(out, "fx=%s\n", t.fxStem.empty() ? "none" : t.fxStem.c_str());
        if (t.hasFxOffset)
        {
            Append(out, "fx_forward=%.3f\n", t.fxForward);
            Append(out, "fx_height=%.3f\n",  t.fxHeight);
        }
        if (t.hasFirePoint)
        {
            Append(out, "fire_forward=%.3f\n", t.fireForward);
            Append(out, "fire_height=%.3f\n",  t.fireHeight);
        }
        if (t.hasProjectile)
        {
            Append(out, "proj_scale=%.3f\n",  t.projScale);
            Append(out, "proj_radius=%.3f\n", t.projRadius);
            Append(out, "proj_speed=%.3f\n",  t.projSpeed);
            Append(out, "proj_life=%.3f\n",   t.projLifetime);
        }
        if (t.hasCooldown)
            Append(out, "cooldown=%.3f\n", t.cooldown);
        if (t.hasAbility)
        {
            Append(out, "aim_range=%.3f\n", t.aimRange);
            Append(out, "area_radius=%.3f\n", t.areaRadius);
            Append(out, "effect_length=%.3f\n", t.effectLength);
            Append(out, "effect_width=%.3f\n", t.effectWidth);
            Append(out, "effect_duration=%.3f\n", t.effectDuration);
            Append(out, "tick_interval=%.3f\n", t.tickInterval);
            Append(out, "chain_range=%.3f\n", t.chainRange);
            Append(out, "max_targets=%.3f\n", t.maxTargets);
            Append(out, "move_distance=%.3f\n", t.moveDistance);
            Append(out, "preview_angle=%.3f\n", t.previewAngle);
        }
        if (t.hasSignature)
        {
            Append(out, "sig_telegraph=%.3f\n", t.telegraphTime);
            Append(out, "sig_active=%.3f\n", t.activeTime);
            Append(out, "sig_recovery=%.3f\n", t.recoveryTime);
            Append(out, "sig_cooldown=%.3f\n", t.signatureCooldown);
            Append(out, "sig_travel=%.3f\n", t.travelDistance);
            Append(out, "sig_phase_speed=%.3f\n", t.phaseSpeedMult);
            Append(out, "sig_phase_cooldown=%.3f\n", t.phaseCooldownMult);
        }
        if (t.hasHitArea)
        {
            Append(out, "hit_area=1\n");
            Append(out, "hit_radius=%.3f\n", t.hitRadius);
            Append(out, "hit_inner=%.3f\n", t.hitInner);
            Append(out, "hit_forward=%.3f\n", t.hitForward);
            Append(out, "hit_length=%.3f\n", t.hitLength);
            Append(out, "hit_width=%.3f\n", t.hitWidth);
            Append(out, "hit_x=%.3f\n", t.hitX);
            Append(out, "hit_y=%.3f\n", t.hitY);
            Append(out, "hit_w=%.3f\n", t.hitW);
            Append(out, "hit_h=%.3f\n", t.hitH);
        }

        return out;
    }

    bool Save(const std::string& key, const AttackTuning& t)
    {
        std::string path = TuningFiles::Path("attacktuning_" + key + ".txt");
        FILE* f = fopen(path.c_str(), "w");
        if (!f) return false;

        const std::string text = Serialize(t);
        fwrite(text.data(), 1, text.size(), f);
        fclose(f);
        Reload(key);   // drop cache so the next Get() re-reads the new values
        return true;
    }
}

const AttackTuning* TunedHitArea(const std::string& key)
{
    const AttackTuning* tuning = AttackTuningStore::Get(key);
    return (tuning != nullptr && tuning->hasHitArea) ? tuning : nullptr;
}

float TunedHitRadius(const std::string& key, float fallback)
{
    const AttackTuning* area = TunedHitArea(key);
    return (area != nullptr && area->hitRadius > 0.f) ? area->hitRadius : fallback;
}

float TunedHitInner(const std::string& key, float fallback)
{
    const AttackTuning* area = TunedHitArea(key);
    return (area != nullptr && area->hitInner >= 0.f) ? area->hitInner : fallback;
}

float TunedHitForward(const std::string& key, float fallback)
{
    const AttackTuning* area = TunedHitArea(key);
    return (area != nullptr) ? area->hitForward : fallback;
}

float TunedHitLength(const std::string& key, float fallback)
{
    const AttackTuning* area = TunedHitArea(key);
    return (area != nullptr && area->hitLength > 0.f) ? area->hitLength : fallback;
}

float TunedHitWidth(const std::string& key, float fallback)
{
    const AttackTuning* area = TunedHitArea(key);
    return (area != nullptr && area->hitWidth > 0.f) ? area->hitWidth : fallback;
}
