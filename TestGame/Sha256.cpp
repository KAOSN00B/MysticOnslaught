#include "Sha256.h"

#include <cstring>

namespace
{
    // FIPS 180-4 §4.2.2: the first 32 bits of the fractional parts of the
    // cube roots of the first 64 primes.
    constexpr int kRounds = 64;
    constexpr std::uint32_t kRoundConstants[kRounds] = {
        0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
        0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
        0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
        0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
        0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
        0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
        0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
        0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2,
    };

    // FIPS 180-4 §5.3.3: the initial hash value.
    constexpr std::uint32_t kInitialState[8] = {
        0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19,
    };

    constexpr int           kWordBits        = 32;
    constexpr int           kMessageWords    = 16;               // words per 64-byte block
    constexpr std::uint8_t  kPaddingMarker   = 0x80;             // the "1" bit after the message
    constexpr std::size_t   kLengthFieldSize = 8;                // message length in bits, big-endian
    constexpr std::uint8_t  kHmacInnerPad    = 0x36;             // RFC 2104 ipad
    constexpr std::uint8_t  kHmacOuterPad    = 0x5c;             // RFC 2104 opad
    constexpr int           kBitsPerByte     = 8;

    std::uint32_t RotateRight(std::uint32_t value, int bits) { return (value >> bits) | (value << (kWordBits - bits)); }

    // FIPS 180-4 §4.1.2. The rotation and shift amounts are the definitions of
    // these four functions.
    std::uint32_t BigSigma0(std::uint32_t x)   { return RotateRight(x, 2) ^ RotateRight(x, 13) ^ RotateRight(x, 22); }
    std::uint32_t BigSigma1(std::uint32_t x)   { return RotateRight(x, 6) ^ RotateRight(x, 11) ^ RotateRight(x, 25); }
    std::uint32_t SmallSigma0(std::uint32_t x) { return RotateRight(x, 7) ^ RotateRight(x, 18) ^ (x >> 3); }
    std::uint32_t SmallSigma1(std::uint32_t x) { return RotateRight(x, 17) ^ RotateRight(x, 19) ^ (x >> 10); }
    std::uint32_t Choose(std::uint32_t x, std::uint32_t y, std::uint32_t z)   { return (x & y) ^ (~x & z); }
    std::uint32_t Majority(std::uint32_t x, std::uint32_t y, std::uint32_t z) { return (x & y) ^ (x & z) ^ (y & z); }

    std::uint32_t ReadBigEndian32(const std::uint8_t* bytes)
    {
        std::uint32_t value = 0;
        for (int i = 0; i < (int)sizeof(std::uint32_t); ++i) value = (value << kBitsPerByte) | bytes[i];
        return value;
    }

    void WriteBigEndian(std::uint64_t value, std::uint8_t* out, std::size_t size)
    {
        for (std::size_t i = 0; i < size; ++i)
            out[size - 1 - i] = (std::uint8_t)(value >> (kBitsPerByte * i));
    }
}

namespace Sha256
{
    Hasher::Hasher()
    {
        std::memcpy(_state, kInitialState, sizeof(_state));
    }

    void Hasher::Compress(const std::uint8_t* block)
    {
        std::uint32_t w[kRounds];
        for (int i = 0; i < kMessageWords; ++i) w[i] = ReadBigEndian32(block + i * sizeof(std::uint32_t));
        for (int i = kMessageWords; i < kRounds; ++i)
            w[i] = SmallSigma1(w[i - 2]) + w[i - 7] + SmallSigma0(w[i - 15]) + w[i - 16];

        std::uint32_t a = _state[0], b = _state[1], c = _state[2], d = _state[3];
        std::uint32_t e = _state[4], f = _state[5], g = _state[6], h = _state[7];
        for (int i = 0; i < kRounds; ++i)
        {
            const std::uint32_t t1 = h + BigSigma1(e) + Choose(e, f, g) + kRoundConstants[i] + w[i];
            const std::uint32_t t2 = BigSigma0(a) + Majority(a, b, c);
            h = g; g = f; f = e; e = d + t1;
            d = c; c = b; b = a; a = t1 + t2;
        }
        _state[0] += a; _state[1] += b; _state[2] += c; _state[3] += d;
        _state[4] += e; _state[5] += f; _state[6] += g; _state[7] += h;
    }

    void Hasher::Update(const void* data, std::size_t size)
    {
        const std::uint8_t* bytes = static_cast<const std::uint8_t*>(data);
        _totalBytes += size;
        while (size > 0)
        {
            if (_buffered == 0 && size >= kBlockSize)
            {
                Compress(bytes);
                bytes += kBlockSize;
                size -= kBlockSize;
                continue;
            }
            const std::size_t take = (kBlockSize - _buffered) < size ? (kBlockSize - _buffered) : size;
            std::memcpy(_buffer + _buffered, bytes, take);
            _buffered += take;
            bytes += take;
            size -= take;
            if (_buffered == kBlockSize)
            {
                Compress(_buffer);
                _buffered = 0;
            }
        }
    }

    Digest Hasher::Finish()
    {
        const std::uint64_t totalBits = _totalBytes * kBitsPerByte;
        _buffer[_buffered++] = kPaddingMarker;
        if (_buffered > kBlockSize - kLengthFieldSize)
        {
            std::memset(_buffer + _buffered, 0, kBlockSize - _buffered);
            Compress(_buffer);
            _buffered = 0;
        }
        std::memset(_buffer + _buffered, 0, kBlockSize - kLengthFieldSize - _buffered);
        WriteBigEndian(totalBits, _buffer + kBlockSize - kLengthFieldSize, kLengthFieldSize);
        Compress(_buffer);

        Digest digest{};
        for (int i = 0; i < kStateWords; ++i)
            WriteBigEndian(_state[i], digest.data() + i * sizeof(std::uint32_t), sizeof(std::uint32_t));
        return digest;
    }

    Hmac::Hmac(const void* key, std::size_t keySize)
    {
        // Keys longer than a block are hashed first (RFC 2104 §2).
        std::uint8_t block[kBlockSize] = {};
        if (keySize > kBlockSize)
        {
            const Digest hashed = Hash(key, keySize);
            std::memcpy(block, hashed.data(), hashed.size());
        }
        else if (keySize > 0)
            std::memcpy(block, key, keySize);

        std::uint8_t innerKey[kBlockSize];
        for (std::size_t i = 0; i < kBlockSize; ++i)
        {
            innerKey[i] = block[i] ^ kHmacInnerPad;
            _outerKey[i] = block[i] ^ kHmacOuterPad;
        }
        _inner.Update(innerKey, kBlockSize);
    }

    Digest Hmac::Finish()
    {
        const Digest innerDigest = _inner.Finish();
        Hasher outer;
        outer.Update(_outerKey, kBlockSize);
        outer.Update(innerDigest.data(), innerDigest.size());
        return outer.Finish();
    }

    Digest Hash(const void* data, std::size_t size)
    {
        Hasher hasher;
        hasher.Update(data, size);
        return hasher.Finish();
    }

    Digest HmacDigest(const void* key, std::size_t keySize, const void* data, std::size_t size)
    {
        Hmac hmac(key, keySize);
        hmac.Update(data, size);
        return hmac.Finish();
    }

    bool ConstantTimeEqual(const std::uint8_t* a, const std::uint8_t* b, std::size_t size)
    {
        std::uint8_t difference = 0;
        for (std::size_t i = 0; i < size; ++i) difference |= (std::uint8_t)(a[i] ^ b[i]);
        return difference == 0;
    }

    std::string ToHex(const Digest& digest)
    {
        static const char kHexDigits[] = "0123456789abcdef";
        constexpr int kNibbleBits = 4;
        constexpr std::uint8_t kNibbleMask = 0x0f;
        constexpr std::size_t kHexCharsPerByte = 2;
        std::string out;
        out.reserve(digest.size() * kHexCharsPerByte);
        for (std::uint8_t byte : digest)
        {
            out += kHexDigits[byte >> kNibbleBits];
            out += kHexDigits[byte & kNibbleMask];
        }
        return out;
    }
}
