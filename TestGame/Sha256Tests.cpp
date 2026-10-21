#include "Sha256.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#define CHECK(x) do { if (!(x)) { std::fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #x); return 1; } } while (0)

namespace
{
    std::string HashHex(const std::string& text) { return Sha256::ToHex(Sha256::Hash(text.data(), text.size())); }

    std::string HmacHex(const std::vector<unsigned char>& key, const std::string& data)
    {
        return Sha256::ToHex(Sha256::HmacDigest(key.data(), key.size(), data.data(), data.size()));
    }

    std::vector<unsigned char> Repeat(unsigned char byte, std::size_t count) { return std::vector<unsigned char>(count, byte); }
    std::string RepeatText(char c, std::size_t count) { return std::string(count, c); }
}

int main()
{
    // FIPS 180-2 / NIST CAVP example messages.
    CHECK(HashHex("") == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    CHECK(HashHex("abc") == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    CHECK(HashHex("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq") ==
          "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
    CHECK(HashHex("abcdefghbcdefghicdefghijdefghijkefghijklfghijklmghijklmnhijklmnoijklmnopjklmnopqklmnopqrlmnopqrsmnopqrstnopqrstu") ==
          "cf5b16a778af8380036ce59e7b0492370b249b11e8f07a51afac45037afee9d1");
    const std::string million = RepeatText('a', 1000000);
    CHECK(HashHex(million) == "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");

    // Feeding the same bytes in uneven pieces gives the same digest, including
    // pieces that straddle the 64-byte block and the 56-byte padding boundary.
    for (std::size_t piece : { (std::size_t)1, (std::size_t)55, (std::size_t)56, (std::size_t)63,
                               (std::size_t)64, (std::size_t)65, (std::size_t)1000 })
    {
        Sha256::Hasher hasher;
        for (std::size_t at = 0; at < million.size(); at += piece)
            hasher.Update(million.data() + at, std::min(piece, million.size() - at));
        CHECK(Sha256::ToHex(hasher.Finish()) == HashHex(million));
    }
    for (std::size_t length = 50; length < 70; ++length)   // every padding branch
    {
        const std::string text = RepeatText('q', length);
        Sha256::Hasher split;
        split.Update(text.data(), length / 2);
        split.Update(text.data() + length / 2, length - length / 2);
        CHECK(Sha256::ToHex(split.Finish()) == HashHex(text));
    }

    // RFC 4231 HMAC-SHA256 test cases 1-4, 6, 7 and the truncated case 5.
    CHECK(HmacHex(Repeat(0x0b, 20), "Hi There") ==
          "b0344c61d8db38535ca8afceaf0bf12b881dc200c9833da726e9376c2e32cff7");
    CHECK(HmacHex({ 'J', 'e', 'f', 'e' }, "what do ya want for nothing?") ==
          "5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843");
    {
        const std::vector<unsigned char> data = Repeat(0xdd, 50);
        CHECK(HmacHex(Repeat(0xaa, 20), std::string(data.begin(), data.end())) ==
              "773ea91e36800e46854db8ebd09181a72959098b3ef8c122d9635514ced565fe");
    }
    {
        std::vector<unsigned char> key;
        for (unsigned char b = 0x01; b <= 0x19; ++b) key.push_back(b);
        const std::vector<unsigned char> data = Repeat(0xcd, 50);
        CHECK(HmacHex(key, std::string(data.begin(), data.end())) ==
              "82558a389a443c0ea4cc819899f2083a85f0faa3e578f8077a2e3ff46729665b");
    }
    CHECK(HmacHex(Repeat(0x0c, 20), "Test With Truncation").substr(0, 32) == "a3b6167473100ee06e0c796c2955552b");
    CHECK(HmacHex(Repeat(0xaa, 131), "Test Using Larger Than Block-Size Key - Hash Key First") ==
          "60e431591ee0b67f0d8a26aacbf5b77f8e0bc6213728c5140546040f0ee37f54");
    CHECK(HmacHex(Repeat(0xaa, 131),
                  "This is a test using a larger than block-size key and a larger than block-size data. "
                  "The key needs to be hashed before being used by the HMAC algorithm.") ==
          "9b09ffa71b942fcb27635fbcd5b0e944bfdc63644f0713938a7f51535c3a35e2");

    // Incremental HMAC matches the one-shot form.
    {
        const std::vector<unsigned char> key = Repeat(0x42, 32);
        const std::string header = "MOSV-header", payload = "{\"schema\":1}";
        Sha256::Hmac hmac(key.data(), key.size());
        hmac.Update(header.data(), header.size());
        hmac.Update(payload.data(), payload.size());
        CHECK(Sha256::ToHex(hmac.Finish()) == HmacHex(key, header + payload));
    }

    // Constant-time comparison still compares correctly.
    const Sha256::Digest a = Sha256::Hash("a", 1), b = Sha256::Hash("b", 1);
    CHECK(Sha256::ConstantTimeEqual(a.data(), a.data(), a.size()));
    CHECK(!Sha256::ConstantTimeEqual(a.data(), b.data(), a.size()));

    std::puts("SHA-256 tests passed: NIST digests, chunked updates, RFC 4231 HMAC vectors, incremental HMAC");
    return 0;
}
