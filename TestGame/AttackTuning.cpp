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
    return AttackTuningKey(AttackOwnerForAbility(i), GetAbilityName(ability));
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
}
