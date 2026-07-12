#pragma warning(disable: 4996)   // fopen/fscanf are fine here; paths are internal

#include "AttackTuning.h"

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

    bool LoadFile(const std::string& key, AttackTuning& out)
    {
        std::string path = "attacktuning_" + key + ".txt";
        FILE* f = fopen(path.c_str(), "r");
        if (!f) return false;

        char line[256];
        while (fgets(line, sizeof(line), f))
        {
            char k[64]; char sval[128]; float val;
            if (sscanf(line, "%63[^=]=%127s", k, sval) == 2 && std::strcmp(k, "fx") == 0)
            {
                out.hasFx  = true;
                out.fxStem = (std::strcmp(sval, "none") == 0) ? "" : sval;
                continue;
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
            }
        }
        fclose(f);
        return true;
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

    bool Save(const std::string& key, const AttackTuning& t)
    {
        std::string path = "attacktuning_" + key + ".txt";
        FILE* f = fopen(path.c_str(), "w");
        if (!f) return false;

        if (t.hasBox)
        {
            fprintf(f, "box_x=%.3f\n", t.x);
            fprintf(f, "box_y=%.3f\n", t.y);
            fprintf(f, "box_w=%.3f\n", t.w);
            fprintf(f, "box_h=%.3f\n", t.h);
        }
        if (t.hasFx)
            fprintf(f, "fx=%s\n", t.fxStem.empty() ? "none" : t.fxStem.c_str());
        if (t.hasFxOffset)
        {
            fprintf(f, "fx_forward=%.3f\n", t.fxForward);
            fprintf(f, "fx_height=%.3f\n",  t.fxHeight);
        }
        if (t.hasFirePoint)
        {
            fprintf(f, "fire_forward=%.3f\n", t.fireForward);
            fprintf(f, "fire_height=%.3f\n",  t.fireHeight);
        }
        if (t.hasProjectile)
        {
            fprintf(f, "proj_scale=%.3f\n",  t.projScale);
            fprintf(f, "proj_radius=%.3f\n", t.projRadius);
            fprintf(f, "proj_speed=%.3f\n",  t.projSpeed);
            fprintf(f, "proj_life=%.3f\n",   t.projLifetime);
        }
        if (t.hasCooldown)
            fprintf(f, "cooldown=%.3f\n", t.cooldown);
        if (t.hasAbility)
        {
            fprintf(f, "aim_range=%.3f\n", t.aimRange);
            fprintf(f, "area_radius=%.3f\n", t.areaRadius);
            fprintf(f, "effect_length=%.3f\n", t.effectLength);
            fprintf(f, "effect_width=%.3f\n", t.effectWidth);
            fprintf(f, "effect_duration=%.3f\n", t.effectDuration);
            fprintf(f, "tick_interval=%.3f\n", t.tickInterval);
            fprintf(f, "chain_range=%.3f\n", t.chainRange);
            fprintf(f, "max_targets=%.3f\n", t.maxTargets);
            fprintf(f, "move_distance=%.3f\n", t.moveDistance);
            fprintf(f, "preview_angle=%.3f\n", t.previewAngle);
        }

        fclose(f);
        Reload(key);   // drop cache so the next Get() re-reads the new values
        return true;
    }
}
