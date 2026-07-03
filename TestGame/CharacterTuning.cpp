#pragma warning(disable: 4996)  // fopen/fprintf are safe here; paths are internal

#include "CharacterTuning.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unordered_map>

namespace
{
    struct CacheEntry
    {
        bool            exists = false;
        CharacterTuning tuning{};
    };

    std::unordered_map<std::string, CacheEntry> s_cache;

    std::string FilePath(const std::string& characterName)
    {
        return "charactertuning_" + characterName + ".txt";
    }

    bool LoadFromFile(const std::string& characterName, CharacterTuning& outTuning)
    {
        FILE* f = fopen(FilePath(characterName).c_str(), "r");
        if (!f)
            return false;

        char key[64], val[64];
        while (fscanf(f, " %63[^=]=%63s", key, val) == 2)
        {
            float value = (float)atof(val);
            if      (strcmp(key, "scale") == 0)               { outTuning.hasScale = true; outTuning.scale = value; }
            else if (strcmp(key, "collision_x") == 0)         { outTuning.hasCollision = true; outTuning.collisionRel.x = value; }
            else if (strcmp(key, "collision_y") == 0)         { outTuning.collisionRel.y = value; }
            else if (strcmp(key, "collision_w") == 0)         { outTuning.collisionRel.width = value; }
            else if (strcmp(key, "collision_h") == 0)         { outTuning.collisionRel.height = value; }
            else if (strcmp(key, "capsule_radius") == 0)      { outTuning.hasCapsule = true; outTuning.capsuleRadius = value; }
            else if (strcmp(key, "capsule_half_height") == 0) { outTuning.capsuleHalfHeight = value; }
            else if (strcmp(key, "capsule_offset_x") == 0)    { outTuning.capsuleOffset.x = value; }
            else if (strcmp(key, "capsule_offset_y") == 0)    { outTuning.capsuleOffset.y = value; }
            else if (strcmp(key, "attackbox_w") == 0)         { outTuning.hasAttackBox = true; outTuning.attackBoxWidth = value; }
            else if (strcmp(key, "attackbox_h") == 0)         { outTuning.attackBoxHeight = value; }
            else if (strcmp(key, "attackbox_ox") == 0)        { outTuning.attackBoxOffsetX = value; }
            else if (strcmp(key, "attackbox_oy") == 0)        { outTuning.attackBoxOffsetY = value; }
            else if (strncmp(key, "anim_frametime_", 15) == 0)
            {
                int index = atoi(key + 15);
                if (index >= 0 && index < CharacterTuning::kMaxAnims)
                    outTuning.animFrameTime[index] = value;
            }
            else if (strncmp(key, "anim_", 5) == 0)
            {
                // Per-anim keys: anim_<i>_body_x / _body_y / _body_r,
                // anim_<i>_melee_x / _y / _w / _h, anim_<i>_draw_x / _draw_y
                int index = atoi(key + 5);
                const char* field = strchr(key + 5, '_');
                if (field != nullptr && index >= 0 && index < CharacterTuning::kMaxAnims)
                {
                    field++;   // skip the underscore
                    if      (strcmp(field, "body_x") == 0)  { outTuning.animBody[index].set = true; outTuning.animBody[index].x = value; }
                    else if (strcmp(field, "body_y") == 0)  { outTuning.animBody[index].y = value; }
                    else if (strcmp(field, "body_r") == 0)  { outTuning.animBody[index].radius = value; }
                    else if (strcmp(field, "melee_x") == 0) { outTuning.animMelee[index].set = true; outTuning.animMelee[index].rect.x = value; }
                    else if (strcmp(field, "melee_y") == 0) { outTuning.animMelee[index].rect.y = value; }
                    else if (strcmp(field, "melee_w") == 0) { outTuning.animMelee[index].rect.width = value; }
                    else if (strcmp(field, "melee_h") == 0) { outTuning.animMelee[index].rect.height = value; }
                    else if (strcmp(field, "draw_x") == 0)  { outTuning.animDraw[index].set = true; outTuning.animDraw[index].x = value; }
                    else if (strcmp(field, "draw_y") == 0)  { outTuning.animDraw[index].y = value; }
                }
            }
        }
        fclose(f);
        return true;
    }
}

namespace CharacterTuningStore
{

const CharacterTuning* Get(const std::string& characterName)
{
    auto it = s_cache.find(characterName);
    if (it == s_cache.end())
    {
        CacheEntry entry;
        entry.exists = LoadFromFile(characterName, entry.tuning);
        it = s_cache.emplace(characterName, entry).first;
    }
    return it->second.exists ? &it->second.tuning : nullptr;
}

void Save(const std::string& characterName, const CharacterTuning& tuning)
{
#ifdef PLATFORM_WEB
    (void)tuning;
    Reload(characterName);
    return;   // no persistent file system on web
#else
    FILE* f = fopen(FilePath(characterName).c_str(), "w");
    if (!f)
        return;

    if (tuning.hasScale)
        fprintf(f, "scale=%.3f\n", tuning.scale);
    if (tuning.hasCollision)
    {
        fprintf(f, "collision_x=%.2f\n", tuning.collisionRel.x);
        fprintf(f, "collision_y=%.2f\n", tuning.collisionRel.y);
        fprintf(f, "collision_w=%.2f\n", tuning.collisionRel.width);
        fprintf(f, "collision_h=%.2f\n", tuning.collisionRel.height);
    }
    if (tuning.hasCapsule)
    {
        fprintf(f, "capsule_radius=%.2f\n",      tuning.capsuleRadius);
        fprintf(f, "capsule_half_height=%.2f\n", tuning.capsuleHalfHeight);
        fprintf(f, "capsule_offset_x=%.2f\n",    tuning.capsuleOffset.x);
        fprintf(f, "capsule_offset_y=%.2f\n",    tuning.capsuleOffset.y);
    }
    if (tuning.hasAttackBox)
    {
        fprintf(f, "attackbox_w=%.2f\n",  tuning.attackBoxWidth);
        fprintf(f, "attackbox_h=%.2f\n",  tuning.attackBoxHeight);
        fprintf(f, "attackbox_ox=%.2f\n", tuning.attackBoxOffsetX);
        fprintf(f, "attackbox_oy=%.2f\n", tuning.attackBoxOffsetY);
    }
    for (int i = 0; i < CharacterTuning::kMaxAnims; i++)
        if (tuning.animFrameTime[i] > 0.f)
            fprintf(f, "anim_frametime_%d=%.4f\n", i, tuning.animFrameTime[i]);

    for (int i = 0; i < CharacterTuning::kMaxAnims; i++)
    {
        if (tuning.animBody[i].set)
        {
            fprintf(f, "anim_%d_body_x=%.2f\n", i, tuning.animBody[i].x);
            fprintf(f, "anim_%d_body_y=%.2f\n", i, tuning.animBody[i].y);
            fprintf(f, "anim_%d_body_r=%.2f\n", i, tuning.animBody[i].radius);
        }
        if (tuning.animMelee[i].set)
        {
            fprintf(f, "anim_%d_melee_x=%.2f\n", i, tuning.animMelee[i].rect.x);
            fprintf(f, "anim_%d_melee_y=%.2f\n", i, tuning.animMelee[i].rect.y);
            fprintf(f, "anim_%d_melee_w=%.2f\n", i, tuning.animMelee[i].rect.width);
            fprintf(f, "anim_%d_melee_h=%.2f\n", i, tuning.animMelee[i].rect.height);
        }
        if (tuning.animDraw[i].set)
        {
            fprintf(f, "anim_%d_draw_x=%.2f\n", i, tuning.animDraw[i].x);
            fprintf(f, "anim_%d_draw_y=%.2f\n", i, tuning.animDraw[i].y);
        }
    }
    fclose(f);

    Reload(characterName);
#endif
}

void Reload(const std::string& characterName)
{
    s_cache.erase(characterName);
}

bool Delete(const std::string& characterName)
{
#ifdef PLATFORM_WEB
    Reload(characterName);
    return false;
#else
    bool removed = (remove(FilePath(characterName).c_str()) == 0);
    Reload(characterName);
    return removed;
#endif
}

}   // namespace CharacterTuningStore
