#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

// ─────────────────────────────────────────────────────────────────────────────
// SHA-256 (FIPS 180-4) and HMAC-SHA256 (RFC 2104), used to sign save files.
//
// Self-contained so the game and the standalone test suites need no extra
// library. Sha256Tests.cpp checks it against the NIST and RFC 4231 vectors.
// ─────────────────────────────────────────────────────────────────────────────

namespace Sha256
{
    constexpr std::size_t kDigestSize = 32;
    constexpr std::size_t kBlockSize  = 64;
    using Digest = std::array<std::uint8_t, kDigestSize>;

    class Hasher
    {
    public:
        Hasher();
        void   Update(const void* data, std::size_t size);
        Digest Finish();   // the hasher must not be reused afterwards

    private:
        static constexpr int kStateWords = 8;
        void Compress(const std::uint8_t* block);

        std::uint32_t _state[kStateWords];
        std::uint8_t  _buffer[kBlockSize] = {};
        std::size_t   _buffered = 0;
        std::uint64_t _totalBytes = 0;
    };

    // Incremental HMAC, so a header and a payload can be signed together
    // without first concatenating them.
    class Hmac
    {
    public:
        Hmac(const void* key, std::size_t keySize);
        void   Update(const void* data, std::size_t size) { _inner.Update(data, size); }
        Digest Finish();

    private:
        Hasher _inner;
        std::uint8_t _outerKey[kBlockSize];
    };

    Digest Hash(const void* data, std::size_t size);
    Digest HmacDigest(const void* key, std::size_t keySize, const void* data, std::size_t size);

    // Compares every byte regardless of where the first difference is.
    bool ConstantTimeEqual(const std::uint8_t* a, const std::uint8_t* b, std::size_t size);
    std::string ToHex(const Digest& digest);
}
