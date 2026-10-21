#pragma once

#include "Json.h"

#include <set>
#include <string>

enum class AbilityType;
enum class Biome;
enum class Feat;
enum class MetaUnlockType;
enum class PlayerClass;
enum class RelicType;

// ─────────────────────────────────────────────────────────────────────────────
// SAVE IDS — the stable names save files use for game enums.
//
// Saves store these names, never enum numbers, so reordering or inserting
// enum values can't corrupt a save. A name, once shipped, must not change:
// renaming drops that entry from existing saves. Every enum value needs a
// name (SaveIds.cpp static_asserts each table against the enum's size). A
// name this build doesn't know (content removed since) reads as "not found".
// ─────────────────────────────────────────────────────────────────────────────

const char* SaveIdOf(AbilityType value);      // nullptr for None or an out-of-range value
const char* SaveIdOf(Biome value);
const char* SaveIdOf(Feat value);
const char* SaveIdOf(MetaUnlockType value);
const char* SaveIdOf(PlayerClass value);
const char* SaveIdOf(RelicType value);
const char* BestiarySaveId(int bestiaryIndex);   // the Bestiary entry's type name

bool SaveIdTo(const std::string& id, AbilityType& out);
bool SaveIdTo(const std::string& id, Biome& out);
bool SaveIdTo(const std::string& id, Feat& out);
bool SaveIdTo(const std::string& id, MetaUnlockType& out);
bool SaveIdTo(const std::string& id, PlayerClass& out);
bool SaveIdTo(const std::string& id, RelicType& out);
bool BestiaryIndexFromSaveId(const std::string& id, int& out);

// Reads a JSON array of ids: each item must be a string of at most maxLength
// bytes and appear once (a repeat rejects the file). Known names are passed to
// `apply`; unknown ones are skipped.
template <typename Enum, typename Apply>
void ReadSaveIdList(const JsonReader& list, std::size_t maxLength, Apply&& apply)
{
    std::set<std::string> seen;
    for (std::size_t i = 0; i < list.Count(); ++i)
    {
        const JsonReader item = list.Item(i);
        const std::string id = item.AsString(maxLength);
        if (!seen.insert(id).second) { item.Fail("listed twice"); return; }
        Enum value{};
        if (SaveIdTo(id, value)) apply(value);
    }
}
