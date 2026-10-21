#pragma once

// In-game credits (Main Menu > Credits). Only attributions verified from a
// license file or the publisher's published terms belong here; the full record,
// including what is still unconfirmed, is docs/CREDITS.md. ElvGames' license
// requires credit, so CreditsTests fails if their entries go missing.
struct CreditLine
{
    enum class Kind { Title, Heading, Name, Detail, Gap };
    Kind        kind;
    const char* text;
};

inline constexpr CreditLine kCreditLines[] = {
    { CreditLine::Kind::Title,   "MYSTIC ONSLAUGHT" },
    { CreditLine::Kind::Detail,  "A game by Robert Blandon" },
    { CreditLine::Kind::Gap,     "" },

    { CreditLine::Kind::Heading, "MUSIC" },
    { CreditLine::Kind::Name,    "Ovani Sound" },
    { CreditLine::Kind::Detail,  "Fantasy Music Pack Vol. 7" },
    { CreditLine::Kind::Detail,  "Mystwood Reverie, Feysong Fields, Tomb of Kings," },
    { CreditLine::Kind::Detail,  "Enemy Grounds, Black Doves, Above, Denmark," },
    { CreditLine::Kind::Detail,  "Stormborn Destiny, Elder Awakening" },
    { CreditLine::Kind::Gap,     "" },

    { CreditLine::Kind::Heading, "SOUND EFFECTS" },
    { CreditLine::Kind::Name,    "ElvGames" },
    { CreditLine::Kind::Detail,  "Footsteps Sound FX Asset Pack" },
    { CreditLine::Kind::Detail,  "Game Sounds FX Pack I and II" },
    { CreditLine::Kind::Name,    "Ovani Sound" },
    { CreditLine::Kind::Detail,  "Gothic Fantasy Sound FX Pack Vol. 1" },
    { CreditLine::Kind::Detail,  "Inventory Sound FX Pack Vol. 2" },
    { CreditLine::Kind::Detail,  "Medieval Fantasy Sound FX Pack Vol. 4" },
    { CreditLine::Kind::Gap,     "" },

    { CreditLine::Kind::Heading, "ART" },
    { CreditLine::Kind::Name,    "ElvGames" },
    { CreditLine::Kind::Detail,  "Rogue Adventure: village tileset and buildings" },
    { CreditLine::Kind::Gap,     "" },

    { CreditLine::Kind::Heading, "BUILT WITH" },
    { CreditLine::Kind::Name,    "raylib" },
    { CreditLine::Kind::Detail,  "by Ramon Santamaria and contributors" },
    { CreditLine::Kind::Gap,     "" },
    { CreditLine::Kind::Gap,     "" },

    { CreditLine::Kind::Heading, "THANK YOU FOR PLAYING" },
};

inline constexpr int kCreditLineCount = (int)(sizeof(kCreditLines) / sizeof(kCreditLines[0]));
