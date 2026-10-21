#include "SaveStore.h"

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

#define CHECK(x) do { if (!(x)) { std::fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #x); return 1; } } while (0)

namespace
{
    constexpr std::size_t kCap = 4096;
    constexpr int kMaxValue = 1000;

    std::string ReadAll(const std::string& path)
    {
        std::ifstream file(path, std::ios::binary);
        return std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    }

    void WriteAll(const std::string& path, const std::string& bytes)
    {
        std::ofstream(path, std::ios::binary | std::ios::trunc) << bytes;
    }

    bool Exists(const std::string& path) { return std::filesystem::exists(path); }

    void Clean(const SaveFile& file)
    {
        for (const std::string& p : { file.Path(), file.BackupPath(), file.TempPath(), file.RejectedPath() })
        {
            std::error_code error;
            std::filesystem::remove_all(p, error);
        }
    }

    JsonValue Doc(long long value)
    {
        JsonValue doc = JsonValue::MakeObject();
        doc.Set("schema", JsonValue::MakeInt(1));
        doc.Set("value", JsonValue::MakeInt(value));
        return doc;
    }

    // Accepts {"schema":1,"value":0..1000} and stores the value.
    SaveFile::Validator Accept(long long& out)
    {
        return [&out](const JsonValue& document, std::string& error)
        {
            JsonReadErrors errors;
            JsonReader root(document, errors);
            root.Int("schema", 1, 1);
            const long long value = root.Int("value", 0, kMaxValue);
            if (!errors.Ok()) { error = errors.first; return false; }
            out = value;
            return true;
        };
    }

    long long Decoded(const std::string& path, SaveKind kind)
    {
        JsonValue doc;
        std::string error;
        long long value = -1;
        if (!SaveFile::ReadDocument(path, kind, kCap, doc, error)) return -1;
        return doc.Find("value") && doc.Find("value")->GetInt(value) ? value : -1;
    }
}

int main()
{
    // ── Envelope ─────────────────────────────────────────────────────────────
    const std::string json = "{\"schema\":1,\"value\":42,\"name\":\"plain text marker\"}";
    std::uint8_t nonce[SaveFile::kNonceSize] = {};
    for (std::size_t i = 0; i < SaveFile::kNonceSize; ++i) nonce[i] = (std::uint8_t)i;
    const std::string sealed = SaveFile::Seal(SaveKind::Profile, json, nonce);
    CHECK(sealed.size() == SaveFile::kHeaderSize + json.size());
    CHECK(sealed == SaveFile::Seal(SaveKind::Profile, json, nonce));                // deterministic for a nonce
    CHECK(sealed != SaveFile::Seal(SaveKind::Profile, json));                       // a fresh nonce changes it
    CHECK(sealed.find("plain text marker") == std::string::npos && sealed.find("schema") == std::string::npos);
    std::string opened, error;
    CHECK(SaveFile::Open(SaveKind::Profile, sealed, opened, error) && opened == json);
    CHECK(SaveFile::Open(SaveKind::Profile, SaveFile::Seal(SaveKind::Profile, json), opened, error) && opened == json);

    // Any single changed byte, anywhere (header, nonce, tag or payload), is rejected.
    for (std::size_t at = 0; at < sealed.size(); ++at)
    {
        std::string tampered = sealed;
        tampered[at] = (char)(tampered[at] ^ 0x01);
        CHECK(!SaveFile::Open(SaveKind::Profile, tampered, opened, error));
    }
    for (std::size_t length = 0; length < sealed.size(); ++length)
        CHECK(!SaveFile::Open(SaveKind::Profile, sealed.substr(0, length), opened, error));
    CHECK(!SaveFile::Open(SaveKind::Profile, sealed + "x", opened, error));
    CHECK(!SaveFile::Open(SaveKind::RunCheckpoint, sealed, opened, error) && error == "wrong kind of save file");
    CHECK(!SaveFile::Open(SaveKind::Profile, json, opened, error));                // plain JSON is not a save

    // ── Save, backup, load ───────────────────────────────────────────────────
    SaveFile profile("Saves/profile.mosave", SaveKind::Profile, kCap);
    Clean(profile);
    long long value = -1;
    CHECK(profile.Load(Accept(value)) == SaveLoadResult::Missing && value == -1);
    CHECK(profile.Save(Doc(1), Accept(value)));                                     // creates Saves/
    CHECK(Exists(profile.Path()) && !Exists(profile.BackupPath()) && !Exists(profile.TempPath()));
    CHECK(profile.Save(Doc(2), Accept(value)));
    CHECK(Decoded(profile.BackupPath(), SaveKind::Profile) == 1 && Decoded(profile.Path(), SaveKind::Profile) == 2);
    CHECK(profile.Load(Accept(value)) == SaveLoadResult::Loaded && value == 2);

    // Tampered primary: rejected, set aside, the backup restored as the primary.
    {
        std::string bytes = ReadAll(profile.Path());
        bytes[bytes.size() - 3] ^= 0x20;
        WriteAll(profile.Path(), bytes);
        value = -1;
        CHECK(profile.Load(Accept(value)) == SaveLoadResult::RestoredBackup && value == 1);
        CHECK(profile.LastError().find("signature mismatch") != std::string::npos);
        CHECK(ReadAll(profile.RejectedPath()) == bytes);
        CHECK(ReadAll(profile.Path()) == ReadAll(profile.BackupPath()));
        CHECK(profile.Load(Accept(value)) == SaveLoadResult::Loaded && value == 1);
    }

    // Correctly signed but invalid content (out of range): rejected the same way.
    CHECK(profile.Save(Doc(3), Accept(value)));                                     // primary 3, backup 1
    WriteAll(profile.Path(), SaveFile::Seal(SaveKind::Profile, "{\"schema\":1,\"value\":5000}"));
    CHECK(profile.Load(Accept(value)) == SaveLoadResult::RestoredBackup && value == 1);
    CHECK(profile.LastError().find("out of range") != std::string::npos);

    // A run checkpoint copied over the profile is the wrong kind.
    CHECK(profile.Save(Doc(4), Accept(value)));
    WriteAll(profile.Path(), SaveFile::Seal(SaveKind::RunCheckpoint, "{\"schema\":1,\"value\":9}"));
    CHECK(profile.Load(Accept(value)) == SaveLoadResult::RestoredBackup && value != 9);
    CHECK(profile.LastError().find("wrong kind") != std::string::npos);

    // Oversized primary: rejected from its size alone.
    CHECK(profile.Save(Doc(5), Accept(value)));
    const long long backupValue = Decoded(profile.BackupPath(), SaveKind::Profile);
    WriteAll(profile.Path(), std::string(kCap + 1, 'x'));
    CHECK(profile.Load(Accept(value)) == SaveLoadResult::RestoredBackup && value == backupValue);
    CHECK(profile.LastError().find("size limit") != std::string::npos);
    {
        // A huge file is refused before it is read: nothing is allocated and it is instant.
        constexpr std::uintmax_t kHugeFile = 512ull * 1024 * 1024;
        constexpr long long kInstantMs = 200;
        const std::string huge = "Saves/huge.mosave";
        { std::ofstream(huge, std::ios::binary) << 'x'; }
        std::filesystem::resize_file(huge, kHugeFile);
        std::string bytes;
        const auto start = std::chrono::steady_clock::now();
        CHECK(SaveFile::ReadBounded(huge, kCap, bytes) == SaveFile::ReadStatus::TooLarge && bytes.empty());
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
        CHECK(elapsed < kInstantMs);
        std::filesystem::remove(huge);
    }

    // Truncated and empty primaries.
    CHECK(profile.Save(Doc(6), Accept(value)));
    WriteAll(profile.Path(), ReadAll(profile.Path()).substr(0, SaveFile::kHeaderSize + 4));
    CHECK(profile.Load(Accept(value)) == SaveLoadResult::RestoredBackup);
    WriteAll(profile.Path(), std::string());
    CHECK(profile.Load(Accept(value)) == SaveLoadResult::RestoredBackup);
    CHECK(profile.LastError().find("empty") != std::string::npos);

    // Both files bad: nothing usable, start fresh; bad files are kept, not deleted.
    {
        WriteAll(profile.Path(), "garbage");
        WriteAll(profile.BackupPath(), "more garbage");
        value = -1;
        CHECK(profile.Load(Accept(value)) == SaveLoadResult::Rejected && value == -1);
        CHECK(ReadAll(profile.RejectedPath()) == "garbage" && ReadAll(profile.BackupPath()) == "more garbage");
    }

    // Primary missing, backup good: restored.
    Clean(profile);
    CHECK(profile.Save(Doc(7), Accept(value)) && profile.Save(Doc(8), Accept(value)));
    std::filesystem::remove(profile.Path());
    CHECK(profile.Load(Accept(value)) == SaveLoadResult::RestoredBackup && value == 7 && Exists(profile.Path()));

    // ── Failed writes keep every existing file ───────────────────────────────
    Clean(profile);
    CHECK(profile.Save(Doc(10), Accept(value)) && profile.Save(Doc(11), Accept(value)));
    const std::string primaryBefore = ReadAll(profile.Path()), backupBefore = ReadAll(profile.BackupPath());
    auto unchanged = [&]() {
        return ReadAll(profile.Path()) == primaryBefore && ReadAll(profile.BackupPath()) == backupBefore &&
               !Exists(profile.TempPath());
    };
    SaveFile::SetFailWritesForTesting(true);
    CHECK(!profile.Save(Doc(12), Accept(value)) && unchanged());
    SaveFile::SetFailWritesForTesting(false);
    std::filesystem::create_directory(profile.TempPath());                          // the temp file can't be created
    CHECK(!profile.Save(Doc(12), Accept(value)) && ReadAll(profile.Path()) == primaryBefore);
    std::filesystem::remove(profile.TempPath());
    CHECK(!profile.Save(Doc(5000), Accept(value)) && unchanged());                  // would not load back
    CHECK(profile.LastError().find("would not load") != std::string::npos);
    {
        JsonValue nan = Doc(1);
        nan.Set("bad", JsonValue::MakeFloat(std::numeric_limits<float>::quiet_NaN()));
        CHECK(!profile.Save(nan) && unchanged());
    }
    {
        JsonValue big = Doc(1);
        big.Set("padding", JsonValue::MakeString(std::string(kCap, 'p')));
        CHECK(!profile.Save(big) && unchanged());                                   // over the size cap
    }

    // A damaged primary never overwrites the last good backup.
    {
        WriteAll(profile.Path(), "damaged");
        CHECK(profile.Save(Doc(13), Accept(value)));
        CHECK(ReadAll(profile.BackupPath()) == backupBefore && Decoded(profile.Path(), SaveKind::Profile) == 13);
    }

#if defined(_WIN32)
    // A primary another program holds open is not renamed away: the backup is
    // used for now and the primary is left for the next save to replace.
    {
        CHECK(profile.Save(Doc(14), Accept(value)));
        HANDLE locked = CreateFileA(profile.Path().c_str(), GENERIC_READ, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        CHECK(locked != INVALID_HANDLE_VALUE);
        std::filesystem::remove(profile.RejectedPath());
        value = -1;
        CHECK(profile.Load(Accept(value)) == SaveLoadResult::RestoredBackup && value == 13);
        CloseHandle(locked);
        CHECK(!Exists(profile.RejectedPath()) && Decoded(profile.Path(), SaveKind::Profile) == 14);
    }
#endif

    // ── Plain files: readable JSON, same backup and rejection rules ──────────
    SaveFile settings("Saves/settings.json", SaveKind::Settings, kCap);
    Clean(settings);
    CHECK(settings.Save(Doc(20), Accept(value)) && settings.Save(Doc(21), Accept(value)));
    const std::string text = ReadAll(settings.Path());
    CHECK(text.rfind("{\n", 0) == 0 && text.find("\"value\": 21") != std::string::npos);
    WriteAll(settings.Path(), "{\"schema\":1,\"value\":21,}");                       // a hand edit gone wrong
    CHECK(settings.Load(Accept(value)) == SaveLoadResult::RestoredBackup && value == 20);
    CHECK(settings.LastError().find("invalid JSON") != std::string::npos);

    Clean(profile);
    Clean(settings);
    std::puts("Save store tests passed: envelope tamper/truncation/kind checks, size cap before reading, "
              "backup restore and quarantine, failed writes keep files, locked files left alone, plain JSON");
    return 0;
}
