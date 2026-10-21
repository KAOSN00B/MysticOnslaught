#include "Json.h"

#include <charconv>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <set>
#include <system_error>

namespace
{
    constexpr std::size_t kMaxNumberLiteral = 64;   // sign, digits, point and exponent
    constexpr std::size_t kNumberTextBuffer = 64;   // to_chars output for any float/double
    constexpr int         kPrettyIndent     = 2;
    constexpr unsigned char kFirstPrintable = 0x20; // bytes below this are control characters

    // RFC 3629 UTF-8 forms: lead-byte mask and tag, sequence length, and the
    // smallest code point that form may encode (anything lower is overlong).
    struct Utf8Form { unsigned char mask; unsigned char tag; int length; std::uint32_t minCodePoint; };
    constexpr Utf8Form kUtf8Forms[] = {
        { 0x80, 0x00, 1, 0x0 },
        { 0xE0, 0xC0, 2, 0x80 },
        { 0xF0, 0xE0, 3, 0x800 },
        { 0xF8, 0xF0, 4, 0x10000 },
    };
    constexpr unsigned char kContinuationMask = 0xC0;
    constexpr unsigned char kContinuationTag  = 0x80;
    constexpr int           kContinuationBits = 6;
    constexpr std::uint32_t kMaxCodePoint     = 0x10FFFF;
    constexpr std::uint32_t kSurrogateFirst   = 0xD800;   // UTF-16 high surrogates start here
    constexpr std::uint32_t kLowSurrogateFirst = 0xDC00;
    constexpr std::uint32_t kSurrogateLast    = 0xDFFF;
    constexpr std::uint32_t kSupplementaryBase = 0x10000;
    constexpr int           kSurrogateShift   = 10;
    constexpr int           kHexDigitsPerEscape = 4;
    constexpr int           kHexDigitBits     = 4;
    constexpr unsigned char kUtf8Bom[] = { 0xEF, 0xBB, 0xBF };

    bool NextCodePoint(const std::string& text, std::size_t& at, std::uint32_t& codePoint)
    {
        const unsigned char lead = (unsigned char)text[at];
        for (const Utf8Form& form : kUtf8Forms)
        {
            if ((lead & form.mask) != form.tag) continue;
            if (at + form.length > text.size()) return false;
            codePoint = lead & (unsigned char)~form.mask;
            for (int k = 1; k < form.length; ++k)
            {
                const unsigned char next = (unsigned char)text[at + k];
                if ((next & kContinuationMask) != kContinuationTag) return false;
                codePoint = (codePoint << kContinuationBits) | (next & (unsigned char)~kContinuationMask);
            }
            if (codePoint < form.minCodePoint || codePoint > kMaxCodePoint ||
                (codePoint >= kSurrogateFirst && codePoint <= kSurrogateLast))
                return false;
            at += form.length;
            return true;
        }
        return false;
    }

    bool IsValidUtf8(const std::string& text)
    {
        std::uint32_t codePoint = 0;
        for (std::size_t at = 0; at < text.size();)
            if (!NextCodePoint(text, at, codePoint)) return false;
        return true;
    }

    void AppendUtf8(std::string& out, std::uint32_t codePoint)
    {
        int form = 0;
        while (form + 1 < (int)std::size(kUtf8Forms) && codePoint >= kUtf8Forms[form + 1].minCodePoint) ++form;
        const int length = kUtf8Forms[form].length;
        const int leadShift = kContinuationBits * (length - 1);
        out += (char)(kUtf8Forms[form].tag | (codePoint >> leadShift));
        for (int k = length - 2; k >= 0; --k)
            out += (char)(kContinuationTag | ((codePoint >> (kContinuationBits * k)) & (unsigned char)~kContinuationMask));
    }

    int HexValue(char c)
    {
        constexpr int kDecimalDigits = 10;
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + kDecimalDigits;
        if (c >= 'A' && c <= 'F') return c - 'A' + kDecimalDigits;
        return -1;
    }

    bool IsDigit(char c) { return c >= '0' && c <= '9'; }

    template <typename T>
    bool ParseFloating(const std::string& text, T& out)
    {
        const char* end = text.data() + text.size();
        const auto result = std::from_chars(text.data(), end, out, std::chars_format::general);
        return result.ec == std::errc() && result.ptr == end && std::isfinite(out);
    }

    template <typename T>
    std::string ShortestText(T value)
    {
        char buffer[kNumberTextBuffer];
        const auto result = std::to_chars(buffer, buffer + sizeof(buffer), value);
        return std::string(buffer, result.ptr);
    }
}

// ── Values ───────────────────────────────────────────────────────────────────

JsonValue JsonValue::MakeBool(bool value)   { JsonValue v; v._type = Type::Bool; v._bool = value; return v; }
JsonValue JsonValue::MakeInt(long long value) { JsonValue v; v._type = Type::Number; v._text = std::to_string(value); return v; }
JsonValue JsonValue::MakeString(std::string value) { JsonValue v; v._type = Type::String; v._text = std::move(value); return v; }
JsonValue JsonValue::MakeArray()  { JsonValue v; v._type = Type::Array; return v; }
JsonValue JsonValue::MakeObject() { JsonValue v; v._type = Type::Object; return v; }

JsonValue JsonValue::MakeFloat(float value)
{
    JsonValue v;
    v._type = Type::Number;
    v._finite = std::isfinite(value);
    v._text = v._finite ? ShortestText(value) : "0";
    return v;
}

JsonValue JsonValue::MakeDouble(double value)
{
    JsonValue v;
    v._type = Type::Number;
    v._finite = std::isfinite(value);
    v._text = v._finite ? ShortestText(value) : "0";
    return v;
}

bool JsonValue::GetBool(bool& out) const
{
    if (_type != Type::Bool) return false;
    out = _bool;
    return true;
}

bool JsonValue::GetInt(long long& out) const
{
    if (_type != Type::Number || _text.find_first_of(".eE") != std::string::npos) return false;
    const char* end = _text.data() + _text.size();
    const auto result = std::from_chars(_text.data(), end, out);
    return result.ec == std::errc() && result.ptr == end;
}

bool JsonValue::GetDouble(double& out) const { return _type == Type::Number && ParseFloating(_text, out); }
bool JsonValue::GetFloat(float& out) const   { return _type == Type::Number && ParseFloating(_text, out); }

JsonValue& JsonValue::Push(JsonValue value)
{
    _items.push_back(std::move(value));
    return _items.back();
}

const JsonValue* JsonValue::Find(const std::string& key) const
{
    for (std::size_t i = 0; i < _keys.size(); ++i)
        if (_keys[i] == key) return &_items[i];
    return nullptr;
}

JsonValue& JsonValue::Set(const std::string& key, JsonValue value)
{
    for (std::size_t i = 0; i < _keys.size(); ++i)
        if (_keys[i] == key) return _items[i] = std::move(value);
    _keys.push_back(key);
    _items.push_back(std::move(value));
    return _items.back();
}

bool JsonValue::Remove(const std::string& key)
{
    for (std::size_t i = 0; i < _keys.size(); ++i)
        if (_keys[i] == key)
        {
            _keys.erase(_keys.begin() + (std::ptrdiff_t)i);
            _items.erase(_items.begin() + (std::ptrdiff_t)i);
            return true;
        }
    return false;
}

// ── Parser ───────────────────────────────────────────────────────────────────

class JsonParser
{
public:
    JsonParser(const std::string& text, const JsonLimits& limits) : _text(text), _limits(limits) {}

    bool Parse(JsonValue& out, std::string& error)
    {
        if (_text.size() >= sizeof(kUtf8Bom) && _text.compare(0, sizeof(kUtf8Bom), (const char*)kUtf8Bom, sizeof(kUtf8Bom)) == 0)
            _pos = sizeof(kUtf8Bom);
        JsonValue value;
        SkipWhitespace();
        bool ok = ParseValue(value, 0);
        if (ok)
        {
            SkipWhitespace();
            if (_pos != _text.size()) ok = Fail("unexpected text after the value");
        }
        if (!ok)
        {
            error = _error + " at byte " + std::to_string(_pos);
            return false;
        }
        out = std::move(value);
        return true;
    }

private:
    bool Fail(const char* what) { if (_error.empty()) _error = what; return false; }
    bool AtEnd() const { return _pos >= _text.size(); }
    char Peek() const { return AtEnd() ? '\0' : _text[_pos]; }

    void SkipWhitespace()
    {
        while (!AtEnd() && (_text[_pos] == ' ' || _text[_pos] == '\t' || _text[_pos] == '\n' || _text[_pos] == '\r')) ++_pos;
    }

    bool CountValue()
    {
        return ++_values <= _limits.maxTotalValues || Fail("too many values");
    }

    bool ParseValue(JsonValue& out, int depth)
    {
        if (!CountValue()) return false;
        switch (Peek())
        {
        case '{': return ParseObject(out, depth + 1);
        case '[': return ParseArray(out, depth + 1);
        case '"': out._type = JsonValue::Type::String; return ParseString(out._text);
        case 't': out._type = JsonValue::Type::Bool; out._bool = true;  return ParseWord("true");
        case 'f': out._type = JsonValue::Type::Bool; out._bool = false; return ParseWord("false");
        case 'n': out._type = JsonValue::Type::Null; return ParseWord("null");
        default:  return ParseNumber(out);
        }
    }

    bool ParseWord(const std::string& word)
    {
        if (_text.compare(_pos, word.size(), word) != 0) return Fail("unknown literal");
        _pos += word.size();
        return true;
    }

    bool ParseObject(JsonValue& out, int depth)
    {
        if (depth > _limits.maxDepth) return Fail("nested too deeply");
        out._type = JsonValue::Type::Object;
        ++_pos;   // '{'
        SkipWhitespace();
        if (Peek() == '}') { ++_pos; return true; }
        std::set<std::string> seen;
        for (;;)
        {
            if (Peek() != '"') return Fail("expected a member name");
            std::string key;
            if (!ParseString(key)) return false;
            if (!seen.insert(key).second) return Fail("duplicate member name");
            SkipWhitespace();
            if (Peek() != ':') return Fail("expected ':'");
            ++_pos;
            SkipWhitespace();
            JsonValue value;
            if (!ParseValue(value, depth)) return false;
            if (out._items.size() >= _limits.maxContainerItems) return Fail("too many members");
            out._keys.push_back(std::move(key));
            out._items.push_back(std::move(value));
            SkipWhitespace();
            if (Peek() == ',') { ++_pos; SkipWhitespace(); continue; }
            if (Peek() == '}') { ++_pos; return true; }
            return Fail("expected ',' or '}'");
        }
    }

    bool ParseArray(JsonValue& out, int depth)
    {
        if (depth > _limits.maxDepth) return Fail("nested too deeply");
        out._type = JsonValue::Type::Array;
        ++_pos;   // '['
        SkipWhitespace();
        if (Peek() == ']') { ++_pos; return true; }
        for (;;)
        {
            JsonValue value;
            if (!ParseValue(value, depth)) return false;
            if (out._items.size() >= _limits.maxContainerItems) return Fail("too many items");
            out._items.push_back(std::move(value));
            SkipWhitespace();
            if (Peek() == ',') { ++_pos; SkipWhitespace(); continue; }
            if (Peek() == ']') { ++_pos; return true; }
            return Fail("expected ',' or ']'");
        }
    }

    bool ParseHexEscape(std::uint32_t& unit)
    {
        if (_pos + kHexDigitsPerEscape > _text.size()) return Fail("short \\u escape");
        unit = 0;
        for (int i = 0; i < kHexDigitsPerEscape; ++i)
        {
            const int digit = HexValue(_text[_pos++]);
            if (digit < 0) return Fail("bad \\u escape");
            unit = (unit << kHexDigitBits) | (std::uint32_t)digit;
        }
        return true;
    }

    bool ParseString(std::string& out)
    {
        ++_pos;   // opening quote
        out.clear();
        for (;;)
        {
            if (AtEnd()) return Fail("unterminated string");
            const unsigned char c = (unsigned char)_text[_pos];
            if (c == '"') { ++_pos; break; }
            if (c < kFirstPrintable) return Fail("control character in string");
            if (c != '\\')
            {
                const std::size_t start = _pos;
                std::uint32_t codePoint = 0;
                if (!NextCodePoint(_text, _pos, codePoint)) return Fail("invalid UTF-8");
                out.append(_text, start, _pos - start);
            }
            else
            {
                ++_pos;
                if (AtEnd()) return Fail("unterminated escape");
                const char e = _text[_pos++];
                switch (e)
                {
                case '"':  out += '"';  break;
                case '\\': out += '\\'; break;
                case '/':  out += '/';  break;
                case 'b':  out += '\b'; break;
                case 'f':  out += '\f'; break;
                case 'n':  out += '\n'; break;
                case 'r':  out += '\r'; break;
                case 't':  out += '\t'; break;
                case 'u':
                {
                    std::uint32_t unit = 0;
                    if (!ParseHexEscape(unit)) return false;
                    if (unit >= kLowSurrogateFirst && unit <= kSurrogateLast) return Fail("lone low surrogate");
                    if (unit >= kSurrogateFirst && unit < kLowSurrogateFirst)
                    {
                        std::uint32_t low = 0;
                        if (_text.compare(_pos, 2, "\\u") != 0) return Fail("lone high surrogate");
                        _pos += 2;
                        if (!ParseHexEscape(low)) return false;
                        if (low < kLowSurrogateFirst || low > kSurrogateLast) return Fail("bad surrogate pair");
                        unit = kSupplementaryBase + (((unit - kSurrogateFirst) << kSurrogateShift) | (low - kLowSurrogateFirst));
                    }
                    if (unit == 0) return Fail("\\u0000 is not allowed");
                    AppendUtf8(out, unit);
                    break;
                }
                default: return Fail("unknown escape");
                }
            }
            if (out.size() > _limits.maxStringBytes) return Fail("string too long");
        }
        return true;
    }

    bool ParseNumber(JsonValue& out)
    {
        const std::size_t start = _pos;
        if (Peek() == '-') ++_pos;
        if (Peek() == '0') ++_pos;
        else if (IsDigit(Peek())) { while (IsDigit(Peek())) ++_pos; }
        else return Fail("expected a value");
        if (Peek() == '.')
        {
            ++_pos;
            if (!IsDigit(Peek())) return Fail("expected a digit after '.'");
            while (IsDigit(Peek())) ++_pos;
        }
        if (Peek() == 'e' || Peek() == 'E')
        {
            ++_pos;
            if (Peek() == '+' || Peek() == '-') ++_pos;
            if (!IsDigit(Peek())) return Fail("expected an exponent");
            while (IsDigit(Peek())) ++_pos;
        }
        if (_pos - start > kMaxNumberLiteral) return Fail("number too long");
        out._type = JsonValue::Type::Number;
        out._text = _text.substr(start, _pos - start);
        return true;
    }

    const std::string& _text;
    const JsonLimits& _limits;
    std::size_t _pos = 0;
    std::size_t _values = 0;
    std::string _error;
};

bool ParseJson(const std::string& text, JsonValue& out, std::string& error, const JsonLimits& limits)
{
    JsonParser parser(text, limits);
    return parser.Parse(out, error);
}

// ── Writer ───────────────────────────────────────────────────────────────────

namespace
{
    bool AppendEscaped(std::string& out, const std::string& text)
    {
        if (!IsValidUtf8(text)) return false;
        out += '"';
        for (char c : text)
        {
            switch (c)
            {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            case '\b': out += "\\b";  break;
            case '\f': out += "\\f";  break;
            default:
                if ((unsigned char)c < kFirstPrintable)
                {
                    if (c == '\0') return false;   // the parser refuses \u0000, so never write it
                    char escape[sizeof("\\u0000")];
                    std::snprintf(escape, sizeof(escape), "\\u%04x", (unsigned)(unsigned char)c);
                    out += escape;
                }
                else
                    out += c;
            }
        }
        out += '"';
        return true;
    }

    void NewLine(std::string& out, bool pretty, int depth)
    {
        if (!pretty) return;
        out += '\n';
        out.append((std::size_t)(depth * kPrettyIndent), ' ');
    }
}

class JsonWriter
{
public:
    static bool Write(const JsonValue& value, std::string& out, bool pretty, int depth)
    {
        switch (value._type)
        {
        case JsonValue::Type::Null:   out += "null"; return true;
        case JsonValue::Type::Bool:   out += value._bool ? "true" : "false"; return true;
        case JsonValue::Type::Number:
            // Either our own shortest form or a literal the parser validated.
            if (!value._finite) return false;
            out += value._text;
            return true;
        case JsonValue::Type::String: return AppendEscaped(out, value._text);
        case JsonValue::Type::Array:
        case JsonValue::Type::Object:
            break;
        }

        const bool isObject = value.IsObject();
        out += isObject ? '{' : '[';
        if (value._items.empty()) { out += isObject ? '}' : ']'; return true; }
        for (std::size_t i = 0; i < value._items.size(); ++i)
        {
            if (i > 0) out += ',';
            NewLine(out, pretty, depth + 1);
            if (isObject)
            {
                if (!AppendEscaped(out, value._keys[i])) return false;
                out += pretty ? ": " : ":";
            }
            if (!Write(value._items[i], out, pretty, depth + 1)) return false;
        }
        NewLine(out, pretty, depth);
        out += isObject ? '}' : ']';
        return true;
    }
};

bool WriteJson(const JsonValue& value, std::string& out, bool pretty)
{
    out.clear();
    if (!JsonWriter::Write(value, out, pretty, 0)) return false;
    if (pretty) out += '\n';
    return true;
}

// ── Reader ───────────────────────────────────────────────────────────────────

void JsonReader::Fail(const std::string& what) const
{
    if (_errors && _errors->first.empty()) _errors->first = _path + ": " + what;
}

JsonReader JsonReader::Child(const JsonValue* value, std::string path) const
{
    JsonReader child;
    child._value = value;
    child._errors = _errors;
    child._path = std::move(path);
    return child;
}

JsonReader JsonReader::Field(const char* key, bool required) const
{
    std::string path = _path + "." + key;
    if (!_value) return Child(nullptr, std::move(path));
    if (!_value->IsObject()) { Fail("expected an object"); return Child(nullptr, std::move(path)); }
    const JsonValue* field = _value->Find(key);
    JsonReader child = Child(field, std::move(path));
    if (!field && required) child.Fail("missing");
    return child;
}

bool JsonReader::Has(const char* key) const
{
    return _value && _value->IsObject() && _value->Find(key) != nullptr;
}

long long JsonReader::AsInt(long long min, long long max) const
{
    long long value = 0;
    if (!_value) { Fail("missing"); return min; }
    if (!_value->GetInt(value)) { Fail(_value->IsNumber() ? "not a whole number" : "expected a number"); return min; }
    if (value < min || value > max) { Fail("out of range"); return min; }
    return value;
}

double JsonReader::AsDouble(double min, double max) const
{
    double value = 0.0;
    if (!_value) { Fail("missing"); return min; }
    if (!_value->GetDouble(value)) { Fail(_value->IsNumber() ? "not a finite number" : "expected a number"); return min; }
    if (value < min || value > max) { Fail("out of range"); return min; }
    return value;
}

float JsonReader::AsFloat(float min, float max) const
{
    float value = 0.f;
    if (!_value) { Fail("missing"); return min; }
    if (!_value->GetFloat(value)) { Fail(_value->IsNumber() ? "not a finite number" : "expected a number"); return min; }
    if (value < min || value > max) { Fail("out of range"); return min; }
    return value;
}

bool JsonReader::AsBool() const
{
    bool value = false;
    if (!_value) { Fail("missing"); return false; }
    if (!_value->GetBool(value)) Fail("expected true or false");
    return value;
}

std::string JsonReader::AsString(std::size_t maxBytes) const
{
    if (!_value) { Fail("missing"); return std::string(); }
    if (!_value->IsString()) { Fail("expected a string"); return std::string(); }
    if (_value->GetString().size() > maxBytes) { Fail("string too long"); return std::string(); }
    return _value->GetString();
}

long long JsonReader::Int(const char* key, long long min, long long max) const { return Field(key, true).AsInt(min, max); }
double JsonReader::Double(const char* key, double min, double max) const { return Field(key, true).AsDouble(min, max); }
float JsonReader::Float(const char* key, float min, float max) const { return Field(key, true).AsFloat(min, max); }
bool JsonReader::Bool(const char* key) const { return Field(key, true).AsBool(); }
std::string JsonReader::String(const char* key, std::size_t maxBytes) const { return Field(key, true).AsString(maxBytes); }

long long JsonReader::IntOr(const char* key, long long fallback, long long min, long long max) const
{
    const JsonReader field = Field(key, false);
    return field.IsPresent() ? field.AsInt(min, max) : fallback;
}

float JsonReader::FloatOr(const char* key, float fallback, float min, float max) const
{
    const JsonReader field = Field(key, false);
    return field.IsPresent() ? field.AsFloat(min, max) : fallback;
}

bool JsonReader::BoolOr(const char* key, bool fallback) const
{
    const JsonReader field = Field(key, false);
    return field.IsPresent() ? field.AsBool() : fallback;
}

std::string JsonReader::StringOr(const char* key, const std::string& fallback, std::size_t maxBytes) const
{
    const JsonReader field = Field(key, false);
    return field.IsPresent() ? field.AsString(maxBytes) : fallback;
}

JsonReader JsonReader::Container(const char* key, bool required, bool isArray, std::size_t maxCount) const
{
    JsonReader field = Field(key, required);
    if (!field.IsPresent()) return field;
    if (isArray ? !field._value->IsArray() : !field._value->IsObject())
    {
        field.Fail(isArray ? "expected an array" : "expected an object");
        field._value = nullptr;
    }
    else if (field._value->Size() > maxCount)
    {
        field.Fail(isArray ? "too many items" : "too many entries");
        field._value = nullptr;
    }
    return field;
}

JsonReader JsonReader::Object(const char* key) const
{
    return Container(key, true, false, JsonLimits::kDefaultMaxContainerItems);
}

bool JsonReader::OptionalObject(const char* key, JsonReader& out) const
{
    out = Container(key, false, false, JsonLimits::kDefaultMaxContainerItems);
    return out.IsPresent();
}

JsonReader JsonReader::Array(const char* key, std::size_t maxCount) const { return Container(key, true, true, maxCount); }
JsonReader JsonReader::Map(const char* key, std::size_t maxCount) const   { return Container(key, true, false, maxCount); }

bool JsonReader::OptionalArray(const char* key, std::size_t maxCount, JsonReader& out) const
{
    out = Container(key, false, true, maxCount);
    return out.IsPresent();
}

bool JsonReader::OptionalMap(const char* key, std::size_t maxCount, JsonReader& out) const
{
    out = Container(key, false, false, maxCount);
    return out.IsPresent();
}

std::size_t JsonReader::Count() const
{
    return (_value && (_value->IsArray() || _value->IsObject())) ? _value->Size() : 0;
}

JsonReader JsonReader::Item(std::size_t index) const
{
    if (!_value || !_value->IsArray() || index >= _value->Size()) return Child(nullptr, _path + "[?]");
    return Child(&_value->At(index), _path + "[" + std::to_string(index) + "]");
}

const std::string& JsonReader::KeyAt(std::size_t index) const
{
    static const std::string kNoKey;
    if (!_value || !_value->IsObject() || index >= _value->Size()) return kNoKey;
    return _value->KeyAt(index);
}

JsonReader JsonReader::ValueAt(std::size_t index) const
{
    if (!_value || !_value->IsObject() || index >= _value->Size()) return Child(nullptr, _path + ".?");
    return Child(&_value->At(index), _path + "." + _value->KeyAt(index));
}
