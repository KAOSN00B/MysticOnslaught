#pragma warning(disable: 4996)  // fopen/fprintf are safe here; paths are internal

#include "CharacterTuning.h"

#include <algorithm>
#include <cstdarg>
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
        return TuningFiles::Path("charactertuning_" + characterName + ".txt");
    }

    // Event kinds as written in the file.
    constexpr const char* kEventKindNames[] = { "fx", "sound", "shake" };
    constexpr int kEventKindCount = (int)(sizeof(kEventKindNames) / sizeof(kEventKindNames[0]));
    constexpr const char* kNoFx = "-";
    constexpr int kEventFields = 6;   // kind:frame:x:y:size:fx

    // One event from "kind:frame:x:y:size:fx"; false when the text is malformed.
    bool ParseEvent(const char* text, CharacterTuning::AnimEvent& out)
    {
        std::string parts[kEventFields];
        int part = 0;
        for (const char* c = text; *c != 0; ++c)
        {
            if (*c == ':') { if (++part >= kEventFields) return false; continue; }
            parts[part] += *c;
        }
        if (part != kEventFields - 1)
            return false;
        int kind = -1;
        for (int i = 0; i < kEventKindCount; i++)
            if (parts[0] == kEventKindNames[i])
                kind = i;
        if (kind < 0)
            return false;
        out.kind  = (CharacterTuning::EventKind)kind;
        out.frame = atoi(parts[1].c_str());
        out.x     = (float)atof(parts[2].c_str());
        out.y     = (float)atof(parts[3].c_str());
        out.size  = (float)atof(parts[4].c_str());
        out.fx    = (parts[5] == kNoFx) ? std::string() : parts[5];
        return out.frame >= 0;
    }

    void ApplyKey(const char* key, const char* val, CharacterTuning& outTuning)
    {
        float value = (float)atof(val);
        if      (strcmp(key, "scale") == 0)               { outTuning.hasScale = true; outTuning.scale = value; }
        else if (strcmp(key, "size_set") == 0)            { outTuning.sizeSet = (value != 0.f); }
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
        else if (strcmp(key, "hurt_pad_x") == 0)          { outTuning.hasHurtPad = true; outTuning.hurtPadX = value; }
        else if (strcmp(key, "hurt_pad_y") == 0)          { outTuning.hasHurtPad = true; outTuning.hurtPadY = value; }
        else if (strncmp(key, "anim_frametime_", 15) == 0)
        {
            int index = atoi(key + 15);
            if (index >= 0 && index < CharacterTuning::kMaxAnims)
                outTuning.animFrameTime[index] = value;
        }
        else if (strncmp(key, "anim_", 5) == 0)
        {
            // Per-anim keys: anim_<i>_body_x / _body_y / _body_r,
            // anim_<i>_melee_x / _y / _w / _h, anim_<i>_draw_x / _draw_y,
            // anim_<i>_hit_start / _hit_end, anim_<i>_event_<n>
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
                else if (strcmp(field, "hit_start") == 0) { outTuning.animHit[index].set = true; outTuning.animHit[index].start = atoi(val); }
                else if (strcmp(field, "hit_end") == 0)   { outTuning.animHit[index].set = true; outTuning.animHit[index].end = atoi(val); }
                else if (strncmp(field, "event_", 6) == 0)
                {
                    CharacterTuning::AnimEvent event;
                    if ((int)outTuning.animEvents[index].size() < CharacterTuning::kMaxEventsPerAnim && ParseEvent(val, event))
                        outTuning.animEvents[index].push_back(event);
                }
            }
        }
    }

    bool LoadFromFile(const std::string& characterName, CharacterTuning& outTuning)
    {
        FILE* f = fopen(FilePath(characterName).c_str(), "r");
        if (!f)
            return false;

        std::string text;
        char chunk[512];
        size_t read = 0;
        while ((read = fread(chunk, 1, sizeof(chunk), f)) > 0)
            text.append(chunk, read);
        fclose(f);

        CharacterTuningStore::Parse(text, outTuning);
        return true;
    }

    std::string Number(float value)
    {
        char text[32];
        snprintf(text, sizeof(text), "%.2f", value);
        return text;
    }

    void AppendLine(std::string& out, const char* format, ...)
    {
        char line[96];
        va_list args;
        va_start(args, format);
        vsnprintf(line, sizeof(line), format, args);
        va_end(args);
        out += line;
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

void Parse(const std::string& text, CharacterTuning& outTuning)
{
    size_t start = 0;
    while (start < text.size())
    {
        size_t end = text.find('\n', start);
        if (end == std::string::npos)
            end = text.size();
        std::string line = text.substr(start, end - start);
        start = end + 1;

        char key[64], val[160];
        if (sscanf(line.c_str(), " %63[^=]=%159s", key, val) == 2)
            ApplyKey(key, val, outTuning);
    }
}

std::string Serialize(const CharacterTuning& tuning)
{
    std::string out;
    if (tuning.hasScale)
        AppendLine(out, "scale=%.3f\n", tuning.scale);
    if (tuning.sizeSet)
        AppendLine(out, "size_set=1\n");
    if (tuning.hasCollision)
    {
        AppendLine(out, "collision_x=%.2f\n", tuning.collisionRel.x);
        AppendLine(out, "collision_y=%.2f\n", tuning.collisionRel.y);
        AppendLine(out, "collision_w=%.2f\n", tuning.collisionRel.width);
        AppendLine(out, "collision_h=%.2f\n", tuning.collisionRel.height);
    }
    if (tuning.hasCapsule)
    {
        AppendLine(out, "capsule_radius=%.2f\n",      tuning.capsuleRadius);
        AppendLine(out, "capsule_half_height=%.2f\n", tuning.capsuleHalfHeight);
        AppendLine(out, "capsule_offset_x=%.2f\n",    tuning.capsuleOffset.x);
        AppendLine(out, "capsule_offset_y=%.2f\n",    tuning.capsuleOffset.y);
    }
    if (tuning.hasAttackBox)
    {
        AppendLine(out, "attackbox_w=%.2f\n",  tuning.attackBoxWidth);
        AppendLine(out, "attackbox_h=%.2f\n",  tuning.attackBoxHeight);
        AppendLine(out, "attackbox_ox=%.2f\n", tuning.attackBoxOffsetX);
        AppendLine(out, "attackbox_oy=%.2f\n", tuning.attackBoxOffsetY);
    }
    if (tuning.hasHurtPad)
    {
        AppendLine(out, "hurt_pad_x=%.2f\n", tuning.hurtPadX);
        AppendLine(out, "hurt_pad_y=%.2f\n", tuning.hurtPadY);
    }
    for (int i = 0; i < CharacterTuning::kMaxAnims; i++)
        if (tuning.animFrameTime[i] > 0.f)
            AppendLine(out, "anim_frametime_%d=%.4f\n", i, tuning.animFrameTime[i]);

    for (int i = 0; i < CharacterTuning::kMaxAnims; i++)
    {
        if (tuning.animBody[i].set)
        {
            AppendLine(out, "anim_%d_body_x=%.2f\n", i, tuning.animBody[i].x);
            AppendLine(out, "anim_%d_body_y=%.2f\n", i, tuning.animBody[i].y);
            AppendLine(out, "anim_%d_body_r=%.2f\n", i, tuning.animBody[i].radius);
        }
        if (tuning.animMelee[i].set)
        {
            AppendLine(out, "anim_%d_melee_x=%.2f\n", i, tuning.animMelee[i].rect.x);
            AppendLine(out, "anim_%d_melee_y=%.2f\n", i, tuning.animMelee[i].rect.y);
            AppendLine(out, "anim_%d_melee_w=%.2f\n", i, tuning.animMelee[i].rect.width);
            AppendLine(out, "anim_%d_melee_h=%.2f\n", i, tuning.animMelee[i].rect.height);
        }
        if (tuning.animDraw[i].set)
        {
            AppendLine(out, "anim_%d_draw_x=%.2f\n", i, tuning.animDraw[i].x);
            AppendLine(out, "anim_%d_draw_y=%.2f\n", i, tuning.animDraw[i].y);
        }
        if (tuning.animHit[i].set)
        {
            AppendLine(out, "anim_%d_hit_start=%d\n", i, tuning.animHit[i].start);
            AppendLine(out, "anim_%d_hit_end=%d\n", i, tuning.animHit[i].end);
        }
        const int eventCount = std::min((int)tuning.animEvents[i].size(), CharacterTuning::kMaxEventsPerAnim);
        for (int n = 0; n < eventCount; n++)
        {
            const CharacterTuning::AnimEvent& event = tuning.animEvents[i][n];
            const int kind = std::clamp((int)event.kind, 0, kEventKindCount - 1);
            out += "anim_" + std::to_string(i) + "_event_" + std::to_string(n) + "=" +
                   kEventKindNames[kind] + ":" + std::to_string(event.frame) + ":" +
                   Number(event.x) + ":" + Number(event.y) + ":" + Number(event.size) + ":" +
                   (event.fx.empty() ? std::string(kNoFx) : event.fx) + "\n";
        }
    }
    return out;
}

void Save(const std::string& characterName, const CharacterTuning& tuning)
{
    FILE* f = fopen(FilePath(characterName).c_str(), "w");
    if (!f)
        return;

    const std::string text = Serialize(tuning);
    fwrite(text.data(), 1, text.size(), f);
    fclose(f);

    Reload(characterName);
}

void Reload(const std::string& characterName)
{
    s_cache.erase(characterName);
}

void ReloadAll()
{
    s_cache.clear();
}

bool Delete(const std::string& characterName)
{
    bool removed = (remove(FilePath(characterName).c_str()) == 0);
    Reload(characterName);
    return removed;
}

}   // namespace CharacterTuningStore
