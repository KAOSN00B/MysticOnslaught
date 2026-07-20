#pragma once

// ContentDocument -- the shared section-based text parser for every authored
// content file (.gasset / .projectile / .enemy). One parser owns section
// handling, typed conversion, source line numbers, unknown-field tracking, and
// error messages so the individual type serializers never run their own ad hoc
// sscanf loops (authoring engine design doc, "File Format And Safe
// Persistence").
//
// The format is the project's familiar ini-style layout:
//
//     version=1
//     id=ice_totem
//
//     [visual]
//     sprite=PowerUps/Hazard_IceTotem.png
//
//     [timeline.0]
//     action=Wait
//     duration=1.5
//
// Keys that appear before the first [section] header live in an implicit
// global section whose name is the empty string. Numbered sections such as
// [timeline.3] keep their list order and expose the trailing index.

#include <string>
#include <vector>

// A structured error produced by parsing, validation, or saving. The line
// number is 0 when the error is not tied to a specific source line.
struct ContentError
{
    std::string message;      // human-readable description of what went wrong
    std::string filePath;     // file the error came from (empty for in-memory text)
    int         lineNumber = 0;

    // Formats "file(line): message" for console output and test assertions.
    std::string ToString() const;
};

// One key=value pair inside a section, remembering where it came from.
struct ContentEntry
{
    std::string  key;
    std::string  value;
    int          lineNumber = 0;
    mutable bool consumed   = false;   // serializers mark fields they understood
};

// One [name] or [name.index] block, in file order.
struct ContentSection
{
    std::string               name;         // "visual", "timeline", "" for globals
    int                       listIndex = -1; // trailing .N index, -1 when absent
    int                       lineNumber = 0;
    std::vector<ContentEntry> entries;

    // -- Typed getters -----------------------------------------------------
    // Each getter marks the entry consumed when found. The *Or variants fall
    // back to a default instead of failing, which is how optional fields are
    // read. The Require variants append a ContentError when the key is absent
    // or the value cannot convert.
    const ContentEntry* FindEntry(const std::string& key) const;

    std::string GetStringOr(const std::string& key, const std::string& fallback) const;
    float       GetFloatOr (const std::string& key, float fallback) const;
    int         GetIntOr   (const std::string& key, int fallback) const;
    bool        GetBoolOr  (const std::string& key, bool fallback) const;

    bool RequireString(const std::string& key, std::string& outValue, ContentError& error) const;
    bool RequireFloat (const std::string& key, float& outValue, ContentError& error) const;
    bool RequireInt   (const std::string& key, int& outValue, ContentError& error) const;
};

class ContentDocument
{
public:
    // Parses ini-style text. Returns false and fills the error (with a line
    // number) on malformed section headers or entries.
    bool ParseFromText(const std::string& text, ContentError& error);

    // Reads the file then parses it; the error carries the file path.
    bool LoadFromFile(const std::string& filePath, ContentError& error);

    // Serializes the document back to canonical text (globals first, then
    // sections in stored order). Round-trips cleanly through ParseFromText.
    std::string SerializeToText() const;

    // Writes SerializeToText() to disk. Returns false on I/O failure.
    bool SaveToFile(const std::string& filePath, ContentError& error) const;

    // -- Reading -----------------------------------------------------------
    const ContentSection* FindSection(const std::string& name) const;      // first match, ignores listIndex
    const ContentSection& GlobalSection() const;                            // the implicit "" section
    // All [name.N] sections sorted by their listIndex (for timelines/lists).
    std::vector<const ContentSection*> FindNumberedSections(const std::string& name) const;

    // Entries no serializer consumed -- reported as warnings by validation so
    // typos like "duratoin=1.5" are surfaced instead of silently ignored.
    std::vector<const ContentEntry*> CollectUnconsumedEntries() const;

    // -- Writing (used by the save path) -----------------------------------
    ContentSection& AddSection(const std::string& name, int listIndex = -1);
    ContentSection& EditGlobalSection();
    static void AppendEntry(ContentSection& section, const std::string& key, const std::string& value);
    // Without this overload, a bare `const char*` (as returned by every
    // *Name() enum-to-string helper) prefers the bool overload below --
    // pointer-to-bool is a standard conversion and beats the user-defined
    // conversion to std::string in overload resolution.
    static void AppendEntry(ContentSection& section, const std::string& key, const char* value);
    static void AppendEntry(ContentSection& section, const std::string& key, float value);
    static void AppendEntry(ContentSection& section, const std::string& key, int value);
    static void AppendEntry(ContentSection& section, const std::string& key, bool value);

    const std::string& SourceFilePath() const { return _sourceFilePath; }

private:
    // _sections[0] is always the implicit global section.
    std::vector<ContentSection> _sections;
    std::string                 _sourceFilePath;
};
