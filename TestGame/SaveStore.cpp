#if defined(_MSC_VER)
#pragma warning(disable: 4996)   // fopen: plain C I/O so the write can be committed (_commit)
#endif
#include "SaveStore.h"
#include "Sha256.h"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <random>
#include <thread>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define NOGDI
#define NOUSER
#include <windows.h>
#include <io.h>
#endif

namespace
{
    // Built-in master key. Changing it makes every existing signed save
    // unreadable (a clean slate), so only change it deliberately.
    constexpr std::uint8_t kMasterKey[] = {
        0xdf, 0x99, 0xb8, 0x4b, 0x31, 0x5b, 0x85, 0x96, 0xda, 0xde, 0xf8, 0xbc, 0xf5, 0x41, 0x81, 0x7f,
        0xad, 0xf9, 0x46, 0x18, 0xa8, 0x0e, 0x9e, 0xf5, 0x4b, 0xe8, 0xb8, 0xe0, 0x25, 0x08, 0x88, 0xfb,
    };
    // Separate keys for signing and scrambling, derived from the master key.
    constexpr char kSignatureLabel[] = "MysticOnslaught/save-signature/v1";
    constexpr char kScrambleLabel[]  = "MysticOnslaught/save-scramble/v1";

    // Envelope layout (see SaveStore.h).
    constexpr char         kMagic[] = { 'M', 'O', 'S', 'V' };
    constexpr std::uint8_t kEnvelopeVersion = 1;
    constexpr std::size_t  kVersionOffset   = sizeof(kMagic);
    constexpr std::size_t  kKindOffset      = kVersionOffset + sizeof(kEnvelopeVersion);
    constexpr std::size_t  kReservedOffset  = kKindOffset + sizeof(SaveKind);
    constexpr std::size_t  kReservedSize    = 2;
    constexpr std::size_t  kNonceOffset     = kReservedOffset + kReservedSize;
    constexpr std::size_t  kTagOffset       = kNonceOffset + SaveFile::kNonceSize;
    constexpr std::size_t  kPayloadOffset   = kTagOffset + Sha256::kDigestSize;
    static_assert(kPayloadOffset == SaveFile::kHeaderSize, "envelope layout and kHeaderSize disagree");

    constexpr int kCounterBytes = 4;   // keystream block counter, big-endian
    constexpr int kBitsPerByte  = 8;

    // Antivirus scanners and sync clients (OneDrive) briefly hold files they
    // just saw change; the copy and the final rename retry for a moment.
    constexpr int kFileOpAttempts     = 5;
    constexpr int kFileOpRetryDelayMs = 25;

    template <typename Op>
    bool RetryFileOp(Op op)
    {
        for (int attempt = 0; attempt < kFileOpAttempts; ++attempt)
        {
            if (op()) return true;
            std::this_thread::sleep_for(std::chrono::milliseconds(kFileOpRetryDelayMs));
        }
        return false;
    }

    std::atomic<bool> g_failWritesForTesting{ false };

    struct DerivedKeys
    {
        Sha256::Digest signature;
        Sha256::Digest scramble;
    };

    const DerivedKeys& Keys()
    {
        static const DerivedKeys keys = {
            Sha256::HmacDigest(kMasterKey, sizeof(kMasterKey), kSignatureLabel, sizeof(kSignatureLabel) - 1),
            Sha256::HmacDigest(kMasterKey, sizeof(kMasterKey), kScrambleLabel, sizeof(kScrambleLabel) - 1),
        };
        return keys;
    }

    // XORs `data` with HMAC(scrambleKey, nonce || counter) blocks. The same
    // call scrambles and unscrambles.
    void Scramble(std::string& data, const std::uint8_t* nonce)
    {
        const Sha256::Digest& key = Keys().scramble;
        std::uint32_t counter = 0;
        for (std::size_t at = 0; at < data.size(); at += Sha256::kDigestSize, ++counter)
        {
            std::uint8_t counterBytes[kCounterBytes];
            for (int i = 0; i < kCounterBytes; ++i)
                counterBytes[i] = (std::uint8_t)(counter >> (kBitsPerByte * (kCounterBytes - 1 - i)));
            Sha256::Hmac block(key.data(), key.size());
            block.Update(nonce, SaveFile::kNonceSize);
            block.Update(counterBytes, sizeof(counterBytes));
            const Sha256::Digest stream = block.Finish();
            for (std::size_t i = 0; i < stream.size() && at + i < data.size(); ++i)
                data[at + i] = (char)((std::uint8_t)data[at + i] ^ stream[i]);
        }
    }

    Sha256::Digest Sign(const std::string& headerAndPayload)
    {
        // Everything except the tag itself: header fields before it, payload after it.
        const Sha256::Digest& key = Keys().signature;
        Sha256::Hmac hmac(key.data(), key.size());
        hmac.Update(headerAndPayload.data(), kTagOffset);
        hmac.Update(headerAndPayload.data() + kPayloadOffset, headerAndPayload.size() - kPayloadOffset);
        return hmac.Finish();
    }

    void FillNonce(std::uint8_t (&nonce)[SaveFile::kNonceSize])
    {
        std::random_device source;   // the OS cryptographic generator on MSVC
        for (std::size_t i = 0; i < SaveFile::kNonceSize; i += sizeof(unsigned int))
        {
            const unsigned int value = source();
            for (std::size_t b = 0; b < sizeof(unsigned int) && i + b < SaveFile::kNonceSize; ++b)
                nonce[i + b] = (std::uint8_t)(value >> (kBitsPerByte * b));
        }
    }

    JsonLimits SaveJsonLimits() { return JsonLimits(); }

    // Temp file, flushed and committed, then renamed over `path`. Never
    // leaves a partial `path`; removes the temp file on failure.
    bool WriteReplace(const std::string& path, const std::string& temporary, const std::string& bytes,
                      const std::string* backupFrom, const std::string& backupPath, std::string& why)
    {
        std::error_code error;
        const std::filesystem::path parent = std::filesystem::path(path).parent_path();
        if (!parent.empty()) std::filesystem::create_directories(parent, error);

        FILE* file = std::fopen(temporary.c_str(), "wb");
        if (!file) { why = "could not create " + temporary; return false; }
        bool ok = std::fwrite(bytes.data(), 1, bytes.size(), file) == bytes.size() && std::fflush(file) == 0;
#if defined(_WIN32)
        if (ok) ok = _commit(_fileno(file)) == 0;
#endif
        if (std::fclose(file) != 0) ok = false;
        if (!ok) { std::remove(temporary.c_str()); why = "could not write " + temporary; return false; }

        if (backupFrom)
        {
#if defined(_WIN32)
            const bool copied = RetryFileOp([&] { return CopyFileA(backupFrom->c_str(), backupPath.c_str(), FALSE) != 0; });
#else
            std::filesystem::copy_file(*backupFrom, backupPath, std::filesystem::copy_options::overwrite_existing, error);
            const bool copied = !error;
#endif
            if (!copied) { std::remove(temporary.c_str()); why = "could not back up " + *backupFrom; return false; }
        }

#if defined(_WIN32)
        const bool replaced = RetryFileOp([&] {
            return MoveFileExA(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
        });
#else
        const bool replaced = std::rename(temporary.c_str(), path.c_str()) == 0;
#endif
        if (!replaced) { std::remove(temporary.c_str()); why = "could not replace " + path; return false; }
        return true;
    }

    void Quarantine(const std::string& path, const std::string& rejectedPath)
    {
#if defined(_WIN32)
        MoveFileExA(path.c_str(), rejectedPath.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
#else
        std::rename(path.c_str(), rejectedPath.c_str());
#endif
    }

    const char* ReadStatusText(SaveFile::ReadStatus status)
    {
        switch (status)
        {
        case SaveFile::ReadStatus::TooLarge:   return "file is larger than the size limit";
        case SaveFile::ReadStatus::Empty:      return "file is empty";
        case SaveFile::ReadStatus::Unreadable: return "file could not be read";
        default:                               return "file is missing";
        }
    }
}

SaveFile::SaveFile(std::string path, SaveKind kind, std::size_t maxBytes)
    : _path(std::move(path)), _kind(kind), _maxBytes(maxBytes)
{
}

void SaveFile::SetFailWritesForTesting(bool fail) { g_failWritesForTesting = fail; }

// ── Envelope ─────────────────────────────────────────────────────────────────

std::string SaveFile::Seal(SaveKind kind, const std::string& json)
{
    std::uint8_t nonce[kNonceSize];
    FillNonce(nonce);
    return Seal(kind, json, nonce);
}

std::string SaveFile::Seal(SaveKind kind, const std::string& json, const std::uint8_t (&nonce)[kNonceSize])
{
    std::string sealed(kHeaderSize, '\0');
    std::memcpy(&sealed[0], kMagic, sizeof(kMagic));
    sealed[kVersionOffset] = (char)kEnvelopeVersion;
    sealed[kKindOffset] = (char)kind;
    std::memcpy(&sealed[kNonceOffset], nonce, kNonceSize);
    std::string payload = json;
    Scramble(payload, nonce);
    sealed += payload;
    const Sha256::Digest tag = Sign(sealed);
    std::memcpy(&sealed[kTagOffset], tag.data(), tag.size());
    return sealed;
}

bool SaveFile::Open(SaveKind kind, const std::string& sealed, std::string& json, std::string& error)
{
    json.clear();
    if (sealed.size() < kHeaderSize) { error = "file is too short"; return false; }
    if (std::memcmp(sealed.data(), kMagic, sizeof(kMagic)) != 0) { error = "not a Mystic Onslaught save file"; return false; }
    if ((std::uint8_t)sealed[kVersionOffset] != kEnvelopeVersion) { error = "unsupported save envelope version"; return false; }
    if ((std::uint8_t)sealed[kKindOffset] != (std::uint8_t)kind) { error = "wrong kind of save file"; return false; }
    for (std::size_t i = 0; i < kReservedSize; ++i)
        if (sealed[kReservedOffset + i] != 0) { error = "bad save header"; return false; }
    const Sha256::Digest expected = Sign(sealed);
    if (!Sha256::ConstantTimeEqual(expected.data(), (const std::uint8_t*)sealed.data() + kTagOffset, expected.size()))
    {
        error = "signature mismatch (edited or damaged)";
        return false;
    }
    json = sealed.substr(kPayloadOffset);
    Scramble(json, (const std::uint8_t*)sealed.data() + kNonceOffset);
    return true;
}

// ── Files ────────────────────────────────────────────────────────────────────

SaveFile::ReadStatus SaveFile::ReadBounded(const std::string& path, std::size_t maxBytes, std::string& out)
{
    out.clear();
    std::error_code error;
    const std::filesystem::file_status status = std::filesystem::status(path, error);
    if (error || !std::filesystem::exists(status)) return ReadStatus::Missing;
    if (!std::filesystem::is_regular_file(status)) return ReadStatus::Unreadable;
    const std::uintmax_t size = std::filesystem::file_size(path, error);
    if (error) return ReadStatus::Unreadable;
    if (size == 0) return ReadStatus::Empty;
    if (size > maxBytes) return ReadStatus::TooLarge;

    std::ifstream file(path, std::ios::binary);
    if (!file) return ReadStatus::Unreadable;
    std::string bytes((std::size_t)size, '\0');
    if (!file.read(&bytes[0], (std::streamsize)size)) return ReadStatus::Unreadable;
    char extra = 0;
    if (file.read(&extra, 1)) return ReadStatus::TooLarge;   // grew since the size check
    out = std::move(bytes);
    return ReadStatus::Ok;
}

bool SaveFile::Decode(const std::string& bytes, JsonValue& out, std::string& error) const
{
    std::string json;
    if (IsSigned())
    {
        if (!Open(_kind, bytes, json, error)) return false;
    }
    else
        json = bytes;
    std::string parseError;
    if (!ParseJson(json, out, parseError, SaveJsonLimits())) { error = "invalid JSON: " + parseError; return false; }
    return true;
}

bool SaveFile::ReadDocument(const std::string& path, SaveKind kind, std::size_t maxBytes, JsonValue& out, std::string& error)
{
    std::string bytes;
    const ReadStatus status = ReadBounded(path, maxBytes, bytes);
    if (status != ReadStatus::Ok) { error = ReadStatusText(status); return false; }
    return SaveFile(path, kind, maxBytes).Decode(bytes, out, error);
}

SaveFile::Attempt SaveFile::TryLoad(const std::string& path, const Validator& accept, std::string& bytes, std::string& why) const
{
    const ReadStatus status = ReadBounded(path, _maxBytes, bytes);
    if (status == ReadStatus::Missing) return Attempt::Missing;
    if (status == ReadStatus::Unreadable) { why = ReadStatusText(status); return Attempt::Unreadable; }
    if (status != ReadStatus::Ok) { why = ReadStatusText(status); return Attempt::Rejected; }
    JsonValue document;
    if (!Decode(bytes, document, why)) return Attempt::Rejected;
    std::string invalid;
    if (!accept(document, invalid)) { why = "invalid content: " + invalid; return Attempt::Rejected; }
    return Attempt::Accepted;
}

SaveLoadResult SaveFile::Load(const Validator& accept)
{
    _lastError.clear();
    std::string bytes, why;
    const Attempt primary = TryLoad(_path, accept, bytes, why);
    if (primary == Attempt::Accepted) return SaveLoadResult::Loaded;
    if (primary != Attempt::Missing) _lastError = _path + ": " + why;
    // A file with bad content is set aside; one we merely could not open
    // (locked by another program) is left alone.
    if (primary == Attempt::Rejected) Quarantine(_path, RejectedPath());

    std::string backupWhy;
    const Attempt backup = TryLoad(BackupPath(), accept, bytes, backupWhy);
    if (backup == Attempt::Accepted)
    {
        std::string restoreWhy;
        if (primary != Attempt::Unreadable && !WriteReplace(_path, TempPath(), bytes, nullptr, std::string(), restoreWhy))
            _lastError += (_lastError.empty() ? "" : "; ") + restoreWhy;
        return SaveLoadResult::RestoredBackup;
    }
    if (backup != Attempt::Missing)
        _lastError += (_lastError.empty() ? "" : "; ") + BackupPath() + ": " + backupWhy;
    return (primary == Attempt::Missing && backup == Attempt::Missing) ? SaveLoadResult::Missing : SaveLoadResult::Rejected;
}

bool SaveFile::PrimaryVerifies() const
{
    std::string bytes;
    if (ReadBounded(_path, _maxBytes, bytes) != ReadStatus::Ok) return false;
    JsonValue document;
    std::string error;
    return Decode(bytes, document, error);
}

bool SaveFile::Save(const JsonValue& document, const Validator& verify)
{
    _lastError.clear();
    std::string json;
    if (!WriteJson(document, json, !IsSigned())) return Fail("document holds a non-finite number or invalid text");
    const std::size_t total = json.size() + (IsSigned() ? kHeaderSize : 0);
    if (total > _maxBytes) return Fail("document is larger than the size limit");
    if (verify)
    {
        // Check exactly what a load would see.
        JsonValue readBack;
        std::string error;
        if (!ParseJson(json, readBack, error, SaveJsonLimits())) return Fail("document would not parse: " + error);
        if (!verify(readBack, error)) return Fail("document would not load: " + error);
    }
    if (g_failWritesForTesting) return Fail("write failure injected for testing");

    const std::string bytes = IsSigned() ? Seal(_kind, json) : json;
    // Keep the last good generation: a damaged primary never overwrites the backup.
    const bool backup = PrimaryVerifies();
    std::string why;
    if (!WriteReplace(_path, TempPath(), bytes, backup ? &_path : nullptr, BackupPath(), why)) return Fail(why);
    return true;
}
