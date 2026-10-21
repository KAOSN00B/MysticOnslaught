#include "Credits.h"

#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>

namespace
{
    // The name line under a heading, before the next heading.
    bool CreditedUnder(const char* heading, const char* name)
    {
        bool inSection = false;
        for (int i = 0; i < kCreditLineCount; ++i)
        {
            const CreditLine& line = kCreditLines[i];
            if (line.kind == CreditLine::Kind::Heading) inSection = std::strcmp(line.text, heading) == 0;
            else if (inSection && line.kind == CreditLine::Kind::Name && std::strcmp(line.text, name) == 0) return true;
        }
        return false;
    }
}

int main()
{
    assert(kCreditLineCount > 0 && kCreditLines[0].kind == CreditLine::Kind::Title);

    // ElvGames' license requires credit for every pack we ship from them.
    assert(CreditedUnder("SOUND EFFECTS", "ElvGames"));
    assert(CreditedUnder("ART", "ElvGames"));
    // Ovani asks for credit (appreciated, not required); keep it.
    assert(CreditedUnder("MUSIC", "Ovani Sound"));
    assert(CreditedUnder("SOUND EFFECTS", "Ovani Sound"));

    // Every non-gap line has text, and the default raylib font draws only ASCII.
    for (int i = 0; i < kCreditLineCount; ++i)
    {
        const CreditLine& line = kCreditLines[i];
        if (line.kind != CreditLine::Kind::Gap) assert(line.text && *line.text);
        for (const char* c = line.text; *c; ++c) assert((unsigned char)*c >= 32 && (unsigned char)*c < 127);
    }
    std::puts("Credits tests passed");
    return 0;
}
