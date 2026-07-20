#include "ContentDocument.h"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <algorithm>

// -- Small local string helpers -----------------------------------------------

// Trims spaces, tabs, and carriage returns from both ends of a string.
static std::string TrimWhitespace(const std::string& text)
{
    size_t first = text.find_first_not_of(" \t\r\n");
    if (first == std::string::npos)
        return "";
    size_t last = text.find_last_not_of(" \t\r\n");
    return text.substr(first, last - first + 1);
}

std::string ContentError::ToString() const
{
    std::ostringstream stream;
    if (!filePath.empty())
        stream << filePath;
    if (lineNumber > 0)
        stream << "(" << lineNumber << ")";
    if (!filePath.empty() || lineNumber > 0)
        stream << ": ";
    stream << message;
    return stream.str();
}

// -- ContentSection typed getters ---------------------------------------------

const ContentEntry* ContentSection::FindEntry(const std::string& key) const
{
    for (const ContentEntry& entry : entries)
    {
        if (entry.key == key)
        {
            entry.consumed = true;
            return &entry;
        }
    }
    return nullptr;
}

std::string ContentSection::GetStringOr(const std::string& key, const std::string& fallback) const
{
    const ContentEntry* entry = FindEntry(key);
    return entry ? entry->value : fallback;
}

float ContentSection::GetFloatOr(const std::string& key, float fallback) const
{
    const ContentEntry* entry = FindEntry(key);
    if (!entry)
        return fallback;
    char* parseEnd = nullptr;
    float parsed = std::strtof(entry->value.c_str(), &parseEnd);
    return (parseEnd && parseEnd != entry->value.c_str()) ? parsed : fallback;
}

int ContentSection::GetIntOr(const std::string& key, int fallback) const
{
    const ContentEntry* entry = FindEntry(key);
    if (!entry)
        return fallback;
    char* parseEnd = nullptr;
    long parsed = std::strtol(entry->value.c_str(), &parseEnd, 10);
    return (parseEnd && parseEnd != entry->value.c_str()) ? (int)parsed : fallback;
}

bool ContentSection::GetBoolOr(const std::string& key, bool fallback) const
{
    const ContentEntry* entry = FindEntry(key);
    if (!entry)
        return fallback;
    if (entry->value == "true"  || entry->value == "1") return true;
    if (entry->value == "false" || entry->value == "0") return false;
    return fallback;
}

bool ContentSection::RequireString(const std::string& key, std::string& outValue, ContentError& error) const
{
    const ContentEntry* entry = FindEntry(key);
    if (!entry)
    {
        error.message    = "missing required field '" + key + "' in section [" + name + "]";
        error.lineNumber = lineNumber;
        return false;
    }
    outValue = entry->value;
    return true;
}

bool ContentSection::RequireFloat(const std::string& key, float& outValue, ContentError& error) const
{
    const ContentEntry* entry = FindEntry(key);
    if (!entry)
    {
        error.message    = "missing required field '" + key + "' in section [" + name + "]";
        error.lineNumber = lineNumber;
        return false;
    }
    char* parseEnd = nullptr;
    float parsed = std::strtof(entry->value.c_str(), &parseEnd);
    if (!parseEnd || parseEnd == entry->value.c_str())
    {
        error.message    = "field '" + key + "' has non-numeric value '" + entry->value + "'";
        error.lineNumber = entry->lineNumber;
        return false;
    }
    outValue = parsed;
    return true;
}

bool ContentSection::RequireInt(const std::string& key, int& outValue, ContentError& error) const
{
    float parsedFloat = 0.f;
    if (!RequireFloat(key, parsedFloat, error))
        return false;
    outValue = (int)parsedFloat;
    return true;
}

// -- ContentDocument parsing ---------------------------------------------------

bool ContentDocument::ParseFromText(const std::string& text, ContentError& error)
{
    _sections.clear();
    _sections.push_back(ContentSection{});   // implicit global section ""

    std::istringstream stream(text);
    std::string rawLine;
    int currentLineNumber = 0;

    while (std::getline(stream, rawLine))
    {
        ++currentLineNumber;
        std::string line = TrimWhitespace(rawLine);

        // Blank lines and comment lines (# or ;) are ignored.
        if (line.empty() || line[0] == '#' || line[0] == ';')
            continue;

        if (line.front() == '[')
        {
            // Section header. Must be a matched [name] pair on its own line.
            if (line.back() != ']' || line.size() < 3)
            {
                error.message    = "malformed section header '" + line + "'";
                error.lineNumber = currentLineNumber;
                return false;
            }
            std::string headerBody = TrimWhitespace(line.substr(1, line.size() - 2));

            ContentSection section;
            section.lineNumber = currentLineNumber;

            // Split a trailing ".N" list index off the section name, but only
            // when everything after the final dot is digits ("timeline.3").
            size_t dotPosition = headerBody.rfind('.');
            bool hasNumericSuffix = dotPosition != std::string::npos && dotPosition + 1 < headerBody.size();
            if (hasNumericSuffix)
            {
                for (size_t i = dotPosition + 1; i < headerBody.size(); ++i)
                    if (!std::isdigit((unsigned char)headerBody[i]))
                        hasNumericSuffix = false;
            }
            if (hasNumericSuffix)
            {
                section.name      = headerBody.substr(0, dotPosition);
                section.listIndex = std::atoi(headerBody.c_str() + dotPosition + 1);
            }
            else
            {
                section.name = headerBody;
            }

            if (section.name.empty())
            {
                error.message    = "section header has an empty name";
                error.lineNumber = currentLineNumber;
                return false;
            }
            _sections.push_back(section);
            continue;
        }

        // Ordinary key=value entry.
        size_t equalsPosition = line.find('=');
        if (equalsPosition == std::string::npos)
        {
            error.message    = "expected 'key=value' but found '" + line + "'";
            error.lineNumber = currentLineNumber;
            return false;
        }

        ContentEntry entry;
        entry.key        = TrimWhitespace(line.substr(0, equalsPosition));
        entry.value      = TrimWhitespace(line.substr(equalsPosition + 1));
        entry.lineNumber = currentLineNumber;
        if (entry.key.empty())
        {
            error.message    = "entry has an empty key";
            error.lineNumber = currentLineNumber;
            return false;
        }
        _sections.back().entries.push_back(entry);
    }
    return true;
}

bool ContentDocument::LoadFromFile(const std::string& filePath, ContentError& error)
{
    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open())
    {
        error.message  = "cannot open file";
        error.filePath = filePath;
        return false;
    }
    std::ostringstream buffer;
    buffer << file.rdbuf();

    if (!ParseFromText(buffer.str(), error))
    {
        error.filePath = filePath;
        return false;
    }
    _sourceFilePath = filePath;
    return true;
}

std::string ContentDocument::SerializeToText() const
{
    std::ostringstream out;
    for (size_t sectionIndex = 0; sectionIndex < _sections.size(); ++sectionIndex)
    {
        const ContentSection& section = _sections[sectionIndex];
        if (!section.name.empty())
        {
            out << "\n[" << section.name;
            if (section.listIndex >= 0)
                out << "." << section.listIndex;
            out << "]\n";
        }
        for (const ContentEntry& entry : section.entries)
            out << entry.key << "=" << entry.value << "\n";
    }
    return out.str();
}

bool ContentDocument::SaveToFile(const std::string& filePath, ContentError& error) const
{
    std::ofstream file(filePath, std::ios::binary | std::ios::trunc);
    if (!file.is_open())
    {
        error.message  = "cannot open file for writing";
        error.filePath = filePath;
        return false;
    }
    file << SerializeToText();
    file.close();
    if (file.fail())
    {
        error.message  = "write failed";
        error.filePath = filePath;
        return false;
    }
    return true;
}

// -- Reading helpers -----------------------------------------------------------

const ContentSection* ContentDocument::FindSection(const std::string& name) const
{
    for (const ContentSection& section : _sections)
        if (section.name == name)
            return &section;
    return nullptr;
}

const ContentSection& ContentDocument::GlobalSection() const
{
    return _sections.front();
}

std::vector<const ContentSection*> ContentDocument::FindNumberedSections(const std::string& name) const
{
    std::vector<const ContentSection*> matches;
    for (const ContentSection& section : _sections)
        if (section.name == name && section.listIndex >= 0)
            matches.push_back(&section);
    std::sort(matches.begin(), matches.end(),
        [](const ContentSection* left, const ContentSection* right)
        { return left->listIndex < right->listIndex; });
    return matches;
}

std::vector<const ContentEntry*> ContentDocument::CollectUnconsumedEntries() const
{
    std::vector<const ContentEntry*> unconsumed;
    for (const ContentSection& section : _sections)
        for (const ContentEntry& entry : section.entries)
            if (!entry.consumed)
                unconsumed.push_back(&entry);
    return unconsumed;
}

// -- Writing helpers -----------------------------------------------------------

ContentSection& ContentDocument::AddSection(const std::string& name, int listIndex)
{
    if (_sections.empty())
        _sections.push_back(ContentSection{});   // ensure global section exists
    ContentSection section;
    section.name      = name;
    section.listIndex = listIndex;
    _sections.push_back(section);
    return _sections.back();
}

ContentSection& ContentDocument::EditGlobalSection()
{
    if (_sections.empty())
        _sections.push_back(ContentSection{});
    return _sections.front();
}

void ContentDocument::AppendEntry(ContentSection& section, const std::string& key, const std::string& value)
{
    ContentEntry entry;
    entry.key   = key;
    entry.value = value;
    section.entries.push_back(entry);
}

void ContentDocument::AppendEntry(ContentSection& section, const std::string& key, const char* value)
{
    AppendEntry(section, key, std::string(value));
}

void ContentDocument::AppendEntry(ContentSection& section, const std::string& key, float value)
{
    // Print floats compactly but with enough precision to round-trip tuning
    // values (matches how the existing attacktuning files are written).
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "%g", value);
    AppendEntry(section, key, std::string(buffer));
}

void ContentDocument::AppendEntry(ContentSection& section, const std::string& key, int value)
{
    AppendEntry(section, key, std::to_string(value));
}

void ContentDocument::AppendEntry(ContentSection& section, const std::string& key, bool value)
{
    AppendEntry(section, key, std::string(value ? "true" : "false"));
}
