#include "DialogueBox.h"
#include "VirtualCanvas.h"
#include "NineSlice.h"
#include "VirtualCanvas.h"
#include <string>
#include <vector>
#include <algorithm>

// ── WordWrap ──────────────────────────────────────────────────────────────────
// Splits text into lines that fit within maxWidth at fontSize.
// A single word wider than maxWidth gets its own line rather than clipping.
static std::vector<std::string> WordWrap(
    const std::string& text, int fontSize, float maxWidth)
{
    std::vector<std::string> lines;
    int n   = (int)text.size();
    int pos = 0;

    while (pos < n)
    {
        std::string current;

        while (pos < n)
        {
            // Skip spaces (naturally skipped at start of a new line)
            int spaceStart = pos;
            while (pos < n && text[pos] == ' ') pos++;
            if (pos >= n) break;

            // Read next word
            int wordStart = pos;
            while (pos < n && text[pos] != ' ') pos++;
            std::string word = text.substr(wordStart, pos - wordStart);

            std::string candidate = current.empty() ? word : (current + " " + word);

            if (MeasureText(candidate.c_str(), fontSize) <= (int)maxWidth || current.empty())
            {
                current = candidate;
            }
            else
            {
                // Word doesn't fit — rewind before the spaces and break this line
                pos = spaceStart;
                break;
            }
        }

        if (!current.empty()) lines.push_back(current);
    }

    return lines;
}

// ── ComputeLineEnds ───────────────────────────────────────────────────────────
// Same word-wrap as above, but also returns the source-text char index (exclusive)
// at which each line ends. Used by ComputePageBreaks to know where to cut.
static std::vector<int> ComputeLineEnds(
    const std::string& text, int fontSize, float maxWidth)
{
    std::vector<int> ends;
    int n   = (int)text.size();
    int pos = 0;

    while (pos < n)
    {
        std::string current;
        int lineEnd = pos;   // tracks end of the last word that fit on this line

        while (pos < n)
        {
            int spaceStart = pos;
            while (pos < n && text[pos] == ' ') pos++;
            if (pos >= n) break;

            int wordStart = pos;
            while (pos < n && text[pos] != ' ') pos++;
            int wordEnd = pos;
            std::string word = text.substr(wordStart, wordEnd - wordStart);

            std::string candidate = current.empty() ? word : (current + " " + word);

            if (MeasureText(candidate.c_str(), fontSize) <= (int)maxWidth || current.empty())
            {
                current = candidate;
                lineEnd = wordEnd;
            }
            else
            {
                pos = spaceStart;   // rewind to before the spaces
                break;
            }
        }

        if (!current.empty())
            ends.push_back(lineEnd);
        else
            break;  // no word fit and nothing in current — avoid infinite loop
    }

    return ends;
}

// ── ApplyScreenDefaults ───────────────────────────────────────────────────────
void DialogueBox::ApplyScreenDefaults()
{
    if (defaultsApplied) return;
    defaultsApplied = true;

    float sw = (float)kVirtualWidth;
    float sh = (float)kVirtualHeight;

    // Panel: tuned via F11 dialogue editor — centred-ish, ~68% wide, lower third.
    panelRect = { sw * 0.159f, sh * 0.687f, sw * 0.683f, sh * 0.238f };

    // Portrait defaults (unused until showPortrait = true).
    portraitX     = 160.f;
    portraitY     = panelRect.y - 40.f;
    portraitScale = 1.0f;
}

// ── ComputePageBreaks ─────────────────────────────────────────────────────────
// Returns the exclusive end-char index for each page.
// Example: "Hello world..." splits at char 50 into two pages → {50, <text.size()>}.
// If everything fits on one page, returns {text.size()}.
std::vector<int> DialogueBox::ComputePageBreaks(const std::string& text) const
{
    // Match the same widths used in Draw().
    float inset    = showPortrait ? 300.f : textInsetLeft;
    float textMaxW = panelRect.width - inset - 20.f;

    // Available vertical space: panel height minus top padding, speaker row, E-prompt space.
    float textY0   = panelRect.y + textInsetTop + (float)(speakerFontSize + 8);
    float availH   = (panelRect.y + panelRect.height) - textY0 - 30.f;
    int   linesPerPage = std::max(1, (int)(availH / (float)(bodyFontSize + 4)));

    std::vector<int> lineEnds = ComputeLineEnds(text, bodyFontSize, textMaxW);

    std::vector<int> breaks;
    // Push the last line of each full page.
    for (int i = linesPerPage - 1; i < (int)lineEnds.size(); i += linesPerPage)
        breaks.push_back(lineEnds[i]);

    // Always end at the last character (covers partial last page or empty text).
    if (breaks.empty() || breaks.back() < (int)text.size())
        breaks.push_back((int)text.size());

    return breaks;
}

// ── Draw ──────────────────────────────────────────────────────────────────────
void DialogueBox::Draw(Texture2D borderTex,
                       Texture2D portraitTex,
                       const char* speaker,
                       const std::string& visibleText,
                       bool showContinue) const
{
    float sw = (float)kVirtualWidth;
    float sh = (float)kVirtualHeight;

    // ── Full-screen dim ───────────────────────────────────────────────────────
    DrawRectangle(0, 0, (int)sw, (int)sh, dimColor);

    // ── Portrait (disabled until animated version is ready) ───────────────────
    if (showPortrait && portraitTex.id > 0)
    {
        float texW = (float)portraitTex.width  * portraitScale;
        float texH = (float)portraitTex.height * portraitScale;
        Rectangle src  = { 0.f, 0.f, (float)portraitTex.width, (float)portraitTex.height };
        Rectangle dest = { portraitX - texW * 0.5f, portraitY - texH * 0.5f, texW, texH };
        DrawTexturePro(portraitTex, src, dest, {}, 0.f, WHITE);
    }

    // ── Border panel ──────────────────────────────────────────────────────────
    if (borderTex.id > 0)
        DrawNineSlice(borderTex, srcCorner, dstCorner, panelRect, WHITE);
    else
        DrawRectangleRec(panelRect, Color{ 20, 14, 30, 230 });

    // ── Text area ─────────────────────────────────────────────────────────────
    float inset    = showPortrait ? 300.f : textInsetLeft;
    float textX    = panelRect.x + inset;
    float textMaxW = panelRect.width - inset - 20.f;
    float textY    = panelRect.y + textInsetTop;

    // Speaker name row
    if (speaker && speaker[0] != '\0')
    {
        DrawText(speaker, (int)textX, (int)textY, speakerFontSize, speakerColor);
        textY += speakerFontSize + 8.f;
    }

    // Word-wrapped body text — visibleText is already clipped to the current page
    // by CutsceneManager, so every line here is guaranteed to fit vertically.
    std::vector<std::string> lines = WordWrap(visibleText, bodyFontSize, textMaxW);
    for (const std::string& line : lines)
    {
        DrawText(line.c_str(), (int)textX, (int)textY, bodyFontSize, bodyColor);
        textY += bodyFontSize + 4.f;
    }

    // ── "Press E to continue" prompt ──────────────────────────────────────────
    if (showContinue)
    {
        const char* prompt = "[ E ] Continue";
        int promptW = MeasureText(prompt, 16);
        float px = panelRect.x + panelRect.width - (float)promptW - 14.f;
        float py = panelRect.y + panelRect.height - 26.f;
        DrawText(prompt, (int)px, (int)py, 16, Fade(WHITE, 0.75f));
    }
}
