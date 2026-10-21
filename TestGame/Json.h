#pragma once

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

// ─────────────────────────────────────────────────────────────────────────────
// JSON for save files: a small document model, a strict parser with hard
// limits, a deterministic writer, and JsonReader for field-by-field validation.
//
// Numbers keep their literal text, so 64-bit integers stay exact and floats
// parse straight to float (Float() writes the shortest text that reads back
// to the same float). Objects keep insertion order, so the same document
// always writes the same bytes.
// ─────────────────────────────────────────────────────────────────────────────

struct JsonLimits
{
    static constexpr int         kDefaultMaxDepth          = 32;
    static constexpr std::size_t kDefaultMaxStringBytes    = 4096;
    static constexpr std::size_t kDefaultMaxContainerItems = 4096;
    static constexpr std::size_t kDefaultMaxTotalValues    = 262144;

    int         maxDepth          = kDefaultMaxDepth;           // nested arrays/objects
    std::size_t maxStringBytes    = kDefaultMaxStringBytes;     // per string or key, after unescaping
    std::size_t maxContainerItems = kDefaultMaxContainerItems;  // per array or object
    std::size_t maxTotalValues    = kDefaultMaxTotalValues;     // in the whole document
};

class JsonValue
{
public:
    enum class Type { Null, Bool, Number, String, Array, Object };

    JsonValue() = default;   // null
    static JsonValue MakeBool(bool value);
    static JsonValue MakeInt(long long value);
    static JsonValue MakeFloat(float value);     // non-finite values make the document unwritable
    static JsonValue MakeDouble(double value);   // likewise
    static JsonValue MakeString(std::string value);
    static JsonValue MakeArray();
    static JsonValue MakeObject();

    Type GetType()  const { return _type; }
    bool IsNull()   const { return _type == Type::Null; }
    bool IsBool()   const { return _type == Type::Bool; }
    bool IsNumber() const { return _type == Type::Number; }
    bool IsString() const { return _type == Type::String; }
    bool IsArray()  const { return _type == Type::Array; }
    bool IsObject() const { return _type == Type::Object; }

    // Typed reads: false when the type or the number's form does not fit.
    bool GetBool(bool& out) const;
    bool GetInt(long long& out) const;    // an integer literal (no fraction or exponent) within 64 bits
    bool GetDouble(double& out) const;    // any finite number
    bool GetFloat(float& out) const;      // any number that is finite as a float
    const std::string& GetString() const { return _text; }   // empty unless IsString()

    // Arrays and objects. Size() counts items or members.
    std::size_t Size() const { return _items.size(); }
    const JsonValue& At(std::size_t index) const { return _items[index]; }
    JsonValue& Push(JsonValue value);
    const std::string& KeyAt(std::size_t index) const { return _keys[index]; }
    const JsonValue* Find(const std::string& key) const;
    JsonValue& Set(const std::string& key, JsonValue value);   // replaces an existing member
    bool Remove(const std::string& key);                       // false when absent

private:
    friend class JsonParser;
    friend class JsonWriter;

    Type _type = Type::Null;
    bool _bool = false;
    bool _finite = true;              // false for a number made from NaN/infinity
    std::string _text;                // a string's value or a number's literal
    std::vector<JsonValue> _items;    // array items, or object member values
    std::vector<std::string> _keys;   // object member names (parallel to _items)
};

// Strict RFC 8259: exactly one value, no comments, no trailing commas, no
// NaN/Infinity, no duplicate keys, valid UTF-8, no "\u0000", and every limit.
// A leading UTF-8 byte-order mark is skipped (hand-edited settings files).
bool ParseJson(const std::string& text, JsonValue& out, std::string& error, const JsonLimits& limits = JsonLimits());

// False (and `out` unspecified) when the document holds a non-finite number.
bool WriteJson(const JsonValue& value, std::string& out, bool pretty = false);

// ─────────────────────────────────────────────────────────────────────────────
// JsonReader: validating access to a parsed document. Every accessor checks
// type and range; the first problem is kept with its path ("profile.classes
// .Warrior.rooms: out of range") and every later read returns its fallback, so
// a loader reads all fields and then checks Ok() once.
// ─────────────────────────────────────────────────────────────────────────────

struct JsonReadErrors
{
    std::string first;
    bool Ok() const { return first.empty(); }
};

class JsonReader
{
public:
    JsonReader() = default;
    JsonReader(const JsonValue& value, JsonReadErrors& errors, std::string path = "$")
        : _value(&value), _errors(&errors), _path(std::move(path)) {}

    bool Ok() const { return _errors && _errors->Ok(); }
    void Fail(const std::string& what) const;
    const std::string& Path() const { return _path; }
    bool IsPresent() const { return _value != nullptr; }
    const JsonValue* Raw() const { return _value; }   // the value itself (null when absent)

    // This reader's own value.
    long long   AsInt(long long min, long long max) const;
    double      AsDouble(double min, double max) const;
    float       AsFloat(float min, float max) const;
    bool        AsBool() const;
    std::string AsString(std::size_t maxBytes) const;

    // Object fields. The plain forms require the field; the *Or forms use the
    // fallback when it is missing (a present field of the wrong type still fails).
    bool        Has(const char* key) const;
    long long   Int(const char* key, long long min, long long max) const;
    long long   IntOr(const char* key, long long fallback, long long min, long long max) const;
    double      Double(const char* key, double min, double max) const;
    float       Float(const char* key, float min, float max) const;
    float       FloatOr(const char* key, float fallback, float min, float max) const;
    bool        Bool(const char* key) const;
    bool        BoolOr(const char* key, bool fallback) const;
    std::string String(const char* key, std::size_t maxBytes) const;
    std::string StringOr(const char* key, const std::string& fallback, std::size_t maxBytes) const;

    // Nested containers. Required forms fail when missing; Optional* forms
    // return false when missing. Arrays and maps also fail above maxCount.
    JsonReader Object(const char* key) const;
    bool OptionalObject(const char* key, JsonReader& out) const;
    JsonReader Array(const char* key, std::size_t maxCount) const;
    bool OptionalArray(const char* key, std::size_t maxCount, JsonReader& out) const;
    JsonReader Map(const char* key, std::size_t maxCount) const;   // an object read as name -> value
    bool OptionalMap(const char* key, std::size_t maxCount, JsonReader& out) const;

    // Array items, or object members as a map (name -> value).
    std::size_t Count() const;
    JsonReader Item(std::size_t index) const;
    const std::string& KeyAt(std::size_t index) const;
    JsonReader ValueAt(std::size_t index) const;
    bool IsObjectValue() const { return _value && _value->IsObject(); }
    bool IsArrayValue() const { return _value && _value->IsArray(); }

private:
    JsonReader Field(const char* key, bool required) const;
    JsonReader Child(const JsonValue* value, std::string path) const;
    JsonReader Container(const char* key, bool required, bool isArray, std::size_t maxCount) const;

    const JsonValue* _value = nullptr;
    JsonReadErrors* _errors = nullptr;
    std::string _path;
};
