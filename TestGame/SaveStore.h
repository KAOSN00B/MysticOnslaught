#pragma once

#include "Json.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>

// ─────────────────────────────────────────────────────────────────────────────
// SAVE STORE — every file the game saves goes through SaveFile.
//
// Signed files (profile, run checkpoint, run log) are sealed in an envelope:
//   "MOSV" | envelope version | file kind | 2 reserved bytes | 16-byte nonce |
//   HMAC-SHA256 tag | payload (the JSON, XOR'd with a keystream from the nonce)
// The tag covers the header and the scrambled payload, and the file kind sits
// in the header, so a run file can't be passed off as a profile. The key is
// built into the game: this stops casual editing, not a determined cheater.
// Plain files (settings, keybindings) are readable, pretty-printed JSON.
//
// Loading: size cap from the file's metadata, before anything is read ->
// envelope and signature -> strict JSON -> the caller's schema validation. A
// file that fails any of these is renamed to <name>.rejected (kept for
// inspection, never read) and the backup is tried; a good backup is copied
// back as the primary.
//
// Saving: the document is checked first (finite numbers, valid text, and the
// caller's validation of exactly what would be read back), written to
// <name>.tmp and flushed to disk; the current primary, if it still verifies,
// becomes <name>.bak; then the temp file atomically replaces the primary. A
// failed save returns false and leaves every existing file as it was.
// ─────────────────────────────────────────────────────────────────────────────

enum class SaveKind : std::uint8_t
{
    Profile       = 1,
    RunCheckpoint = 2,
    RunLog        = 3,
    Settings      = 4,   // plain JSON
    Keybindings   = 5,   // plain JSON
};

enum class SaveLoadResult
{
    Loaded,           // the primary file was good
    RestoredBackup,   // the primary was missing or rejected; the backup was good and is the primary again
    Missing,          // neither file exists: start fresh
    Rejected,         // a file existed but none was usable: start fresh (bad files kept as .rejected)
};

class SaveFile
{
public:
    // Validates a whole document. Load's validator applies the data only once
    // everything has passed; Save's validator only checks.
    using Validator = std::function<bool(const JsonValue& document, std::string& error)>;

    SaveFile(std::string path, SaveKind kind, std::size_t maxBytes);

    SaveLoadResult Load(const Validator& accept);
    bool Save(const JsonValue& document, const Validator& verify = Validator());

    const std::string& LastError() const { return _lastError; }
    const std::string& Path() const { return _path; }
    std::string BackupPath() const   { return _path + ".bak"; }
    std::string RejectedPath() const { return _path + ".rejected"; }
    std::string TempPath() const     { return _path + ".tmp"; }
    bool IsSigned() const { return IsSignedKind(_kind); }

    static bool IsSignedKind(SaveKind kind) { return kind == SaveKind::Profile || kind == SaveKind::RunCheckpoint || kind == SaveKind::RunLog; }

    // ── Envelope ─────────────────────────────────────────────────────────────
    static constexpr std::size_t kNonceSize  = 16;
    static constexpr std::size_t kHeaderSize = 56;   // magic, version, kind, reserved, nonce, tag
    static std::string Seal(SaveKind kind, const std::string& json);   // fresh random nonce
    static std::string Seal(SaveKind kind, const std::string& json, const std::uint8_t (&nonce)[kNonceSize]);
    static bool Open(SaveKind kind, const std::string& sealed, std::string& json, std::string& error);

    // ── Files ────────────────────────────────────────────────────────────────
    enum class ReadStatus { Ok, Missing, TooLarge, Empty, Unreadable };
    // Checks the size from the file's metadata before reading anything, and
    // never reads more than maxBytes + 1 even if the file grows meanwhile.
    static ReadStatus ReadBounded(const std::string& path, std::size_t maxBytes, std::string& out);
    // Reads one file (no backup, no renaming): size cap, envelope, JSON. For
    // tools and flow checks that inspect what is on disk.
    static bool ReadDocument(const std::string& path, SaveKind kind, std::size_t maxBytes, JsonValue& out, std::string& error);

    // Test hook: while set, every Save fails before touching the disk.
    static void SetFailWritesForTesting(bool fail);

private:
    enum class Attempt { Accepted, Missing, Unreadable, Rejected };
    Attempt TryLoad(const std::string& path, const Validator& accept, std::string& bytes, std::string& why) const;
    bool Decode(const std::string& bytes, JsonValue& out, std::string& error) const;
    bool PrimaryVerifies() const;
    bool Fail(const std::string& why) { _lastError = why; return false; }

    std::string _path;
    SaveKind _kind;
    std::size_t _maxBytes;
    std::string _lastError;
};
