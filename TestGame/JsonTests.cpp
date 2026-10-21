#include "Json.h"

#include <cfloat>
#include <cmath>
#include <cstdio>
#include <limits>
#include <string>

#define CHECK(x) do { if (!(x)) { std::fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #x); return 1; } } while (0)

namespace
{
    bool Parses(const std::string& text, const JsonLimits& limits = JsonLimits())
    {
        JsonValue value;
        std::string error;
        return ParseJson(text, value, error, limits);
    }

    std::string Write(const JsonValue& value, bool pretty = false)
    {
        std::string out;
        return WriteJson(value, out, pretty) ? out : std::string("<unwritable>");
    }

    std::string Nested(int depth)
    {
        return std::string((std::size_t)depth, '[') + std::string((std::size_t)depth, ']');
    }
}

int main()
{
    // ── Round trip: exact numbers, escapes, Unicode, insertion order ─────────
    JsonValue doc = JsonValue::MakeObject();
    doc.Set("schema", JsonValue::MakeInt(1));
    doc.Set("max", JsonValue::MakeInt(std::numeric_limits<long long>::max()));
    doc.Set("min", JsonValue::MakeInt(std::numeric_limits<long long>::min()));
    doc.Set("health", JsonValue::MakeFloat(7.3f));
    doc.Set("third", JsonValue::MakeFloat(1.0f / 3.0f));
    doc.Set("tiny", JsonValue::MakeFloat(std::numeric_limits<float>::denorm_min()));
    doc.Set("huge", JsonValue::MakeFloat(FLT_MAX));
    doc.Set("time", JsonValue::MakeDouble(1234.5678901234567));
    doc.Set("label", JsonValue::MakeString("Line one\nback\\slash \"quoted\"\ttab\x01 \xC3\xA9 \xF0\x9F\x94\xA5"));
    doc.Set("flag", JsonValue::MakeBool(true));
    doc.Set("none", JsonValue());
    JsonValue& list = doc.Set("relics", JsonValue::MakeArray());
    list.Push(JsonValue::MakeString("LastStand"));
    list.Push(JsonValue::MakeInt(-3));
    doc.Set("empty", JsonValue::MakeObject());
    doc.Set("health", JsonValue::MakeFloat(7.25f));   // Set replaces in place

    const std::string compact = Write(doc);
    CHECK(compact.rfind("{\"schema\":1,\"max\":", 0) == 0);   // order kept, compact
    JsonValue back;
    std::string error;
    CHECK(ParseJson(compact, back, error));
    CHECK(Write(back) == compact);                          // deterministic
    CHECK(ParseJson(Write(doc, true), back, error) && Write(back) == compact);   // pretty reads back the same

    long long whole = 0;
    float f = 0.f;
    double d = 0.0;
    CHECK(back.Find("max")->GetInt(whole) && whole == std::numeric_limits<long long>::max());
    CHECK(back.Find("min")->GetInt(whole) && whole == std::numeric_limits<long long>::min());
    CHECK(back.Find("health")->GetFloat(f) && f == 7.25f);
    CHECK(back.Find("third")->GetFloat(f) && f == 1.0f / 3.0f);
    CHECK(back.Find("tiny")->GetFloat(f) && f == std::numeric_limits<float>::denorm_min());
    CHECK(back.Find("huge")->GetFloat(f) && f == FLT_MAX);
    CHECK(back.Find("time")->GetDouble(d) && d == 1234.5678901234567);
    CHECK(back.Find("label")->GetString() == doc.Find("label")->GetString());
    CHECK(back.Find("relics")->Size() == 2 && back.Find("relics")->At(0).GetString() == "LastStand");
    CHECK(back.Find("none")->IsNull() && back.Find("empty")->IsObject() && back.Find("empty")->Size() == 0);
    CHECK(Write(JsonValue::MakeFloat(7.3f)) == "7.3");       // shortest form, not 7.30000019

    // \u escapes, surrogate pairs, a byte-order mark and number forms.
    CHECK(ParseJson("\xEF\xBB\xBF {\"a\" : \"\\u00e9\\ud83d\\udd25\\/\"} ", back, error));
    CHECK(back.Find("a")->GetString() == "\xC3\xA9\xF0\x9F\x94\xA5/");
    CHECK(ParseJson("[-0, 1E+2, 0.5e-3, 12]", back, error));
    CHECK(back.At(0).GetInt(whole) && whole == 0);
    CHECK(!back.At(1).GetInt(whole) && back.At(1).GetDouble(d) && d == 100.0);
    CHECK(back.At(3).GetInt(whole) && whole == 12);

    // ── Strictness ───────────────────────────────────────────────────────────
    for (const char* bad : { "", "   ", "{", "[1,]", "{\"a\":1,}", "{\"a\":1 \"b\":2}", "NaN", "Infinity",
                             "-Infinity", "01", "1.", ".5", "+1", "1e", "--1", "\"\\x\"", "\"a\nb\"", "'a'",
                             "{\"a\":1}{}", "// c\n{}", "{\"a\":1,\"a\":2}", "\"\\u0000\"", "\"\\ud800\"",
                             "\"\\udc00\"", "\"\\ud800\\u0041\"", "\"\xC0\xAF\"", "\"\xED\xA0\x80\"",
                             "\"\xF4\x90\x80\x80\"", "\"\xE2\x82\"", "tru", "nul", "[1 2]", "{1:2}", "\"\\u12\"" })
        CHECK(!Parses(bad));

    // Numbers that are valid JSON but do not fit the asked-for type.
    CHECK(ParseJson("[1.0, 1e2, 9223372036854775808, 1e39, 1e999, 3.4028235e38]", back, error));
    CHECK(!back.At(0).GetInt(whole) && !back.At(1).GetInt(whole) && !back.At(2).GetInt(whole));
    CHECK(!back.At(3).GetFloat(f) && back.At(3).GetDouble(d));
    CHECK(!back.At(4).GetDouble(d) && !back.At(4).GetFloat(f));
    CHECK(back.At(5).GetFloat(f) && f == FLT_MAX);
    bool flag = false;
    CHECK(!back.At(0).GetBool(flag));

    // Unwritable documents: non-finite numbers, invalid UTF-8, NUL characters.
    std::string out;
    CHECK(!WriteJson(JsonValue::MakeFloat(std::numeric_limits<float>::quiet_NaN()), out));
    CHECK(!WriteJson(JsonValue::MakeDouble(std::numeric_limits<double>::infinity()), out));
    {
        JsonValue arr = JsonValue::MakeArray();
        arr.Push(JsonValue::MakeString("ok"));
        arr.Push(JsonValue::MakeFloat(-std::numeric_limits<float>::infinity()));
        CHECK(!WriteJson(arr, out));
    }
    CHECK(!WriteJson(JsonValue::MakeString("\xC0\xAF"), out));
    CHECK(!WriteJson(JsonValue::MakeString(std::string("a\0b", 3)), out));

    // ── Limits ───────────────────────────────────────────────────────────────
    JsonLimits limits;
    CHECK(Parses(Nested(limits.maxDepth), limits));
    CHECK(!Parses(Nested(limits.maxDepth + 1), limits));
    limits.maxStringBytes = 8;
    CHECK(Parses("\"12345678\"", limits) && !Parses("\"123456789\"", limits));
    CHECK(!Parses("{\"123456789\":1}", limits));
    limits = JsonLimits();
    limits.maxContainerItems = 3;
    CHECK(Parses("[1,2,3]", limits) && !Parses("[1,2,3,4]", limits) && !Parses("{\"a\":1,\"b\":2,\"c\":3,\"d\":4}", limits));
    limits = JsonLimits();
    limits.maxTotalValues = 4;
    CHECK(Parses("[1,2,3]", limits) && !Parses("[1,2,3,4]", limits));
    CHECK(!Parses(std::string(65, '1')) && Parses(std::string(64, '1')));

    // ── JsonReader: types, ranges, paths, fallbacks, first error kept ────────
    CHECK(ParseJson("{\"a\":{\"b\":[1,2,300]},\"name\":\"Hero03\",\"on\":true,\"x\":1.5,\"m\":{\"Ogre\":2,\"Cyclops\":5}}", back, error));
    {
        JsonReadErrors errors;
        JsonReader root(back, errors);
        CHECK(root.String("name", 16) == "Hero03" && root.Bool("on") && root.Float("x", 0.f, 2.f) == 1.5f);
        CHECK(root.IntOr("missing", 42, 0, 100) == 42 && root.BoolOr("missing", true));
        JsonReader m = root.Map("m", 8);
        CHECK(m.Count() == 2 && m.KeyAt(1) == "Cyclops" && m.ValueAt(1).AsInt(0, 10) == 5);
        JsonReader b = root.Object("a").Array("b", 3);
        CHECK(b.Count() == 3 && b.Item(1).AsInt(0, 10) == 2);
        CHECK(errors.Ok());
        b.Item(2).AsInt(0, 10);
        CHECK(!errors.Ok() && errors.first == "$.a.b[2]: out of range");
        root.Int("missing", 0, 1);
        CHECK(errors.first == "$.a.b[2]: out of range");   // the first problem is kept
    }
    {
        JsonReadErrors errors;
        JsonReader root(back, errors);
        root.Int("name", 0, 1);
        CHECK(errors.first == "$.name: expected a number");
    }
    {
        JsonReadErrors errors;
        JsonReader root(back, errors);
        root.Float("missing", 0.f, 1.f);
        CHECK(errors.first == "$.missing: missing");
    }
    {
        JsonReadErrors errors;
        JsonReader root(back, errors);
        root.Object("a").Array("b", 2);
        CHECK(errors.first == "$.a.b: too many items");
    }
    {
        JsonReadErrors errors;
        JsonReader root(back, errors);
        JsonReader absent;
        CHECK(!root.OptionalObject("nothing", absent) && errors.Ok());
        CHECK(!root.OptionalArray("name", 4, absent) && errors.first == "$.name: expected an array");
    }
    {
        JsonReadErrors errors;
        JsonReader root(back, errors);
        CHECK(root.IntOr("x", 0, 0, 10) == 0 && errors.first == "$.x: not a whole number");
    }

    std::puts("JSON tests passed: exact round trips, deterministic output, strict grammar, UTF-8, limits, validating reader");
    return 0;
}
