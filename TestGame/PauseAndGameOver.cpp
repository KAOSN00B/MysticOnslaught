#include "PauseAndGameOver.h"
#include "AssetPaths.h"
#include "NineSlice.h"

// ── 9-slice corner sizes ──────────────────────────────────────────────────────
static constexpr float BORDER_SRC_CORNER = 16.f;
static constexpr float BORDER_DST_CORNER = 32.f;
static constexpr float BTN_SRC_CORNER    = 8.f;
static constexpr float BTN_DST_CORNER    = 12.f;

// ── Shared helper: draw a textured button, return true if clicked ─────────────
static bool DrawButton(Texture2D& tex, const char* label, Rectangle btn, Color tint)
{
    Vector2 mouse  = GetMousePosition();
    bool hovered   = CheckCollisionPointRec(mouse, btn);
    Color drawTint = hovered ? Fade(tint, 0.75f) : tint;

    if (tex.id != 0)
        DrawNineSlice(tex, BTN_SRC_CORNER, BTN_DST_CORNER, btn, drawTint);
    else
    {
        DrawRectangleRounded(btn, 0.22f, 6, drawTint);
        DrawRectangleRoundedLines(btn, 0.22f, 6, Fade(WHITE, 0.35f));
    }

    int fontSize = 30;
    int textW    = MeasureText(label, fontSize);
    DrawText(label,
        (int)(btn.x + btn.width  / 2.f - textW    / 2.f),
        (int)(btn.y + btn.height / 2.f - fontSize / 2.f),
        fontSize, BLACK);

    return hovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
}

// ── Lifecycle ─────────────────────────────────────────────────────────────────

PauseAndGameOver::~PauseAndGameOver()
{
    Unload();
}

void PauseAndGameOver::Init()
{
    if (_borderTex.id == 0)
        _borderTex  = LoadTexture(AssetPath("UI/PauseBoarder.png").c_str());
    if (_btnTex.id == 0)
        _btnTex     = LoadTexture(AssetPath("UI/PlayButton.png").c_str());
    if (_htpBtnTex.id == 0)
        _htpBtnTex  = LoadTexture(AssetPath("UI/HowToPlayButton.png").c_str());
}

void PauseAndGameOver::Unload()
{
    if (_borderTex.id  != 0) { UnloadTexture(_borderTex);  _borderTex  = {}; }
    if (_btnTex.id     != 0) { UnloadTexture(_btnTex);     _btnTex     = {}; }
    if (_htpBtnTex.id  != 0) { UnloadTexture(_htpBtnTex);  _htpBtnTex  = {}; }
}

// ── Pause ─────────────────────────────────────────────────────────────────────
// Returns: 0=nothing  1=resume  2=howtoplay  3=quit
int PauseAndGameOver::DrawPause()
{
    float sw = (float)GetScreenWidth();
    float sh = (float)GetScreenHeight();

    // Dim overlay
    DrawRectangle(0, 0, (int)sw, (int)sh, Fade(BLACK, 0.65f));

    // Panel dimensions
    const float btnW   = sw * 0.22f;
    const float btnH   = sh * 0.074f;
    const float btnGap = sh * 0.018f;
    const float padV   = sh * 0.045f;
    const float titleH = sh * 0.075f;
    const float panelW = btnW + sw * 0.06f;
    const float panelH = padV + titleH + 4.f * (btnH + btnGap) + padV;

    float panelX = sw / 2.f - panelW / 2.f;
    float panelY = sh / 2.f - panelH / 2.f;

    // Draw border texture as the panel background (slightly oversized for visual frame)
    float borderPad = sw * 0.012f;
    if (_borderTex.id != 0)
        DrawNineSlice(_borderTex, BORDER_SRC_CORNER, BORDER_DST_CORNER,
            { panelX - borderPad, panelY - borderPad,
              panelW + borderPad * 2.f, panelH + borderPad * 2.f }, WHITE);
    else
    {
        DrawRectangleRounded({ panelX, panelY, panelW, panelH }, 0.08f, 8, Fade(BLACK, 0.92f));
        DrawRectangleRoundedLines({ panelX, panelY, panelW, panelH }, 0.08f, 8, Fade(WHITE, 0.35f));
    }

    // Title
    const char* title = "PAUSED";
    int titleSz = (int)(sh * 0.055f);
    int titleWpx = MeasureText(title, titleSz);
    DrawText(title, (int)(sw / 2.f - titleWpx / 2.f), (int)(panelY + padV), titleSz, BLACK);

    // Buttons
    float btnX = sw / 2.f - btnW / 2.f;
    float btnY = panelY + padV + titleH;

    int result = 0;

    // Resume — green tint on play button
    if (DrawButton(_btnTex, "Resume", { btnX, btnY, btnW, btnH }, Color{ 100, 210, 120, 255 }))
        result = 1;
    btnY += btnH + btnGap;

    // How To Play — use the HTP button texture
    if (DrawButton(_htpBtnTex, "How To Play", { btnX, btnY, btnW, btnH }, WHITE))
        result = 2;
    btnY += btnH + btnGap;

    // Keybindings — blue tint
    if (DrawButton(_btnTex, "Keybindings", { btnX, btnY, btnW, btnH }, Color{ 80, 150, 230, 255 }))
        result = 4;
    btnY += btnH + btnGap;

    // Quit — red tint
    if (DrawButton(_btnTex, "Quit Game", { btnX, btnY, btnW, btnH }, Color{ 230, 80, 80, 255 }))
        result = 3;

    return result;
}

// ── Keybindings ───────────────────────────────────────────────────────────────
// Returns true when Back is pressed. Modifies bindings in place.
bool PauseAndGameOver::DrawKeybindings(KeyBindings& bindings)
{
    float sw = (float)GetScreenWidth();
    float sh = (float)GetScreenHeight();

    // Dim overlay
    DrawRectangle(0, 0, (int)sw, (int)sh, Fade(BLACK, 0.75f));

    // Slot table: name, pointer to the key, clearable (KEY_NULL allowed)
    struct SlotDef { const char* name; KeyboardKey* key; bool clearable; };
    SlotDef slots[9] = {
        { "Move Up",    &bindings.moveUp,      false },
        { "Move Down",  &bindings.moveDown,    false },
        { "Move Left",  &bindings.moveLeft,    false },
        { "Move Right", &bindings.moveRight,   false },
        { "Dash",       &bindings.dash,        false },
        { "Attack Key", &bindings.attack,      true  },
        { "Fireball",   &bindings.ability[0],  true  },
        { "Sword Beam", &bindings.ability[1],  true  },
        { "Freeze",     &bindings.ability[2],  true  },
    };

    // Poll for a key press when rebinding
    if (_rebindingSlot >= 0 && _rebindingSlot < 9)
    {
        if (IsKeyPressed(KEY_BACKSPACE) && slots[_rebindingSlot].clearable)
        {
            *slots[_rebindingSlot].key = KEY_NULL;
            _rebindingSlot = -1;
        }
        else
        {
            int pressed = GetKeyPressed();
            if (pressed != 0)
            {
                KeyboardKey k = (KeyboardKey)pressed;
                if (k == KEY_ESCAPE)
                    _rebindingSlot = -1;
                else if (k != KEY_BACKSPACE)
                {
                    *slots[_rebindingSlot].key = k;
                    _rebindingSlot = -1;
                }
            }
        }
    }

    // Layout
    const float panelW      = sw * 0.42f;
    const float rowH        = sh * 0.058f;
    const float rowGap      = sh * 0.010f;
    const float padV        = sh * 0.036f;
    const float titleH      = sh * 0.065f;
    const float groupLabelH = sh * 0.030f;
    const float groupGap    = sh * 0.022f;
    const float backH       = sh * 0.062f;
    const float hintH       = sh * 0.022f;

    // 3 groups: 4 + 2 + 3 rows
    const float panelH = padV + titleH
        + groupLabelH + 4.f * (rowH + rowGap)
        + groupGap
        + groupLabelH + 2.f * (rowH + rowGap)
        + groupGap
        + groupLabelH + 3.f * (rowH + rowGap)
        + groupGap + hintH + rowGap + backH + padV;

    float panelX = sw / 2.f - panelW / 2.f;
    float panelY = sh / 2.f - panelH / 2.f;

    DrawRectangleRounded({ panelX, panelY, panelW, panelH }, 0.05f, 8, Fade(BLACK, 0.93f));
    DrawRectangleRoundedLines({ panelX, panelY, panelW, panelH }, 0.05f, 8, Fade(WHITE, 0.30f));

    // Title
    const char* title   = "KEYBINDINGS";
    int         titleSz = (int)(sh * 0.046f);
    DrawText(title,
        (int)(sw / 2.f - MeasureText(title, titleSz) / 2.f),
        (int)(panelY + padV), titleSz, ORANGE);

    int nameSz   = (int)(sh * 0.026f);
    int keySz    = (int)(sh * 0.029f);
    int groupSz  = (int)(sh * 0.022f);

    struct GroupDef { const char* name; int start; int count; };
    GroupDef groups[3] = {
        { "MOVEMENT",  0, 4 },
        { "ACTIONS",   4, 2 },
        { "ABILITIES", 6, 3 },
    };

    float rowY = panelY + padV + titleH;

    for (int g = 0; g < 3; g++)
    {
        if (g > 0) rowY += groupGap;

        // Group label
        DrawText(groups[g].name,
            (int)(panelX + panelW * 0.05f),
            (int)(rowY), groupSz, Fade(GOLD, 0.72f));
        rowY += groupLabelH;

        for (int ri = 0; ri < groups[g].count; ri++)
        {
            int slotIdx = groups[g].start + ri;
            float rowX     = panelX + panelW * 0.05f;
            float rowRight = panelX + panelW * 0.95f;

            DrawRectangleRounded({ rowX, rowY, panelW * 0.90f, rowH }, 0.2f, 4, Fade(WHITE, 0.06f));

            DrawText(slots[slotIdx].name,
                (int)(rowX + 12.f),
                (int)(rowY + rowH * 0.5f - nameSz * 0.5f),
                nameSz, LIGHTGRAY);

            float badgeW = sw * 0.095f;
            float badgeH = rowH * 0.70f;
            float badgeX = rowRight - badgeW - 4.f;
            float badgeY = rowY + rowH * 0.5f - badgeH * 0.5f;
            Rectangle badgeRect{ badgeX, badgeY, badgeW, badgeH };

            bool rebinding = (_rebindingSlot == slotIdx);
            bool hovered   = CheckCollisionPointRec(GetMousePosition(), badgeRect) && !rebinding;

            Color badgeCol = rebinding ? Fade(GOLD, 0.85f)
                           : hovered   ? Fade(SKYBLUE, 0.75f)
                                       : Fade(DARKGRAY, 0.80f);
            DrawRectangleRounded(badgeRect, 0.3f, 4, badgeCol);
            DrawRectangleRoundedLines(badgeRect, 0.3f, 4, Fade(WHITE, 0.4f));

            const char* keyLabel = rebinding ? "..." : GetKeyName(*slots[slotIdx].key);
            int labelW = MeasureText(keyLabel, keySz);
            DrawText(keyLabel,
                (int)(badgeX + badgeW / 2.f - labelW / 2.f),
                (int)(badgeY + badgeH / 2.f - keySz  / 2.f),
                keySz, rebinding ? BLACK : WHITE);

            if (hovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
                _rebindingSlot = slotIdx;

            rowY += rowH + rowGap;
        }
    }

    // Hint text
    rowY += groupGap;
    const char* hintText = (_rebindingSlot >= 0)
        ? (slots[_rebindingSlot].clearable
            ? "Press a key  (ESC cancels  |  BKSP to clear)"
            : "Press a key  (ESC cancels)")
        : "Click a badge to rebind";
    int hintSz = (int)(sh * 0.019f);
    DrawText(hintText,
        (int)(sw / 2.f - MeasureText(hintText, hintSz) / 2.f),
        (int)(rowY), hintSz, Fade(WHITE, 0.45f));

    // Back button
    rowY += hintH + rowGap;
    float backW = panelW * 0.40f;
    float backX = sw / 2.f - backW / 2.f;
    bool done   = DrawButton(_btnTex, "Back", { backX, rowY, backW, backH }, Fade(WHITE, 0.85f));

    return done;
}

// ── Game Over ─────────────────────────────────────────────────────────────────
// Returns: 0=nothing  1=play again  2=main menu  3=quit
int PauseAndGameOver::DrawGameOver(int wave, float gameTimer, int kills, const std::vector<LeaderboardEntry>& scores)
{
    float sw = (float)GetScreenWidth();
    float sh = (float)GetScreenHeight();

    ClearBackground(BLACK);

    // ── Left column: leaderboard ──────────────────────────────────────────────
    // Column X anchors (all relative to left panel region 0..sw*0.5)
    float lbLeft   = sw * 0.02f;   // left edge of the leaderboard block
    int lbHeaderSz = (int)(sh * 0.030f);
    int lbEntrySz  = (int)(sh * 0.024f);
    int lbY        = (int)(sh * 0.14f);

    // Column positions (x-start of each field)
    float colName  = lbLeft;
    float colWave  = lbLeft + sw * 0.14f;
    float colKills = lbLeft + sw * 0.20f;
    float colTime  = lbLeft + sw * 0.27f;

    // Header
    const char* hTitle = "-- TOP SCORES --";
    DrawText(hTitle, (int)colName, lbY, lbHeaderSz, WHITE);
    lbY += lbHeaderSz + 8;

    // Column header row
    DrawText("Name",  (int)colName,  lbY, lbHeaderSz, WHITE);
    DrawText("Wave",  (int)colWave,  lbY, lbHeaderSz, WHITE);
    DrawText("Kills", (int)colKills, lbY, lbHeaderSz, WHITE);
    DrawText("Time",  (int)colTime,  lbY, lbHeaderSz, WHITE);
    lbY += lbHeaderSz + 2;

    // Underline
    DrawRectangle((int)colName, lbY, (int)(colTime + sw * 0.07f - colName), 2, GRAY);
    lbY += 8;

    if (scores.empty())
    {
        DrawText("No scores yet", (int)colName, lbY, lbEntrySz, GRAY);
    }
    else
    {
        for (int i = 0; i < (int)scores.size(); i++)
        {
            Color entryColor = (i == 0) ? GOLD : LIGHTGRAY;
            const std::string& n = scores[i].name.empty() ? "???" : scores[i].name;
            DrawText(n.c_str(),                                         (int)colName,  lbY, lbEntrySz, entryColor);
            DrawText(TextFormat("%d",   scores[i].wave),                (int)colWave,  lbY, lbEntrySz, entryColor);
            DrawText(TextFormat("%d",   scores[i].kills),               (int)colKills, lbY, lbEntrySz, entryColor);
            DrawText(TextFormat("%.1fs", scores[i].time),               (int)colTime,  lbY, lbEntrySz, entryColor);
            lbY += lbEntrySz + 6;
        }
    }

    // ── Right column: title, stats, buttons ───────────────────────────────────
    float rightCX = sw * 0.72f;

    // Title
    const char* title = "GAME OVER";
    int titleSz = (int)(sw * 0.065f);
    DrawText(title, (int)(rightCX - MeasureText(title, titleSz) / 2.f), (int)(sh * 0.12f), titleSz, RED);

    // Current run stats
    int statSz = (int)(sh * 0.034f);
    const char* waveTxt  = TextFormat("Wave: %d", wave);
    const char* timeTxt  = TextFormat("Time: %.1f s", gameTimer);
    const char* killsTxt = TextFormat("Kills: %d", kills);
    int statY = (int)(sh * 0.32f);
    DrawText(waveTxt,  (int)(rightCX - MeasureText(waveTxt,  statSz) / 2.f), statY,                    statSz, YELLOW);
    DrawText(timeTxt,  (int)(rightCX - MeasureText(timeTxt,  statSz) / 2.f), statY + statSz + 10,      statSz, YELLOW);
    DrawText(killsTxt, (int)(rightCX - MeasureText(killsTxt, statSz) / 2.f), statY + (statSz + 10) * 2, statSz, YELLOW);

    // Buttons
    const float btnW   = sw * 0.20f;
    const float btnH   = sh * 0.074f;
    const float btnGap = sh * 0.022f;
    float       btnX   = rightCX - btnW / 2.f;
    float       btnY   = sh * 0.55f;

    struct { const char* label; Texture2D* tex; Color tint; } btns[3] = {
        { "Play Again", &_btnTex,    Color{ 100, 210, 120, 255 } },
        { "Main Menu",  &_htpBtnTex, WHITE                       },
        { "Quit Game",  &_btnTex,    Color{ 230, 80,  80,  255 } },
    };

    int result = 0;
    for (int i = 0; i < 3; i++)
    {
        if (DrawButton(*btns[i].tex, btns[i].label, { btnX, btnY, btnW, btnH }, btns[i].tint))
            result = i + 1;
        btnY += btnH + btnGap;
    }

    return result;
}

// ── Name Entry ────────────────────────────────────────────────────────────────
// Returns "" while typing, returns the confirmed name when Enter is pressed.
std::string PauseAndGameOver::DrawNameEntry(int wave, float gameTimer, int kills)
{
    float sw = (float)GetScreenWidth();
    float sh = (float)GetScreenHeight();

    ClearBackground(BLACK);

    // Title
    const char* title = "GAME OVER";
    int titleSz = (int)(sw * 0.065f);
    DrawText(title,
        (int)(sw / 2.f - MeasureText(title, titleSz) / 2.f),
        (int)(sh * 0.10f), titleSz, RED);

    // Stats
    int statSz = (int)(sh * 0.034f);
    const char* waveTxt  = TextFormat("Wave: %d",    wave);
    const char* timeTxt  = TextFormat("Time: %.1f s", gameTimer);
    const char* killsTxt = TextFormat("Kills: %d",   kills);
    int statY = (int)(sh * 0.28f);
    DrawText(waveTxt,  (int)(sw / 2.f - MeasureText(waveTxt,  statSz) / 2.f), statY,                     statSz, YELLOW);
    DrawText(timeTxt,  (int)(sw / 2.f - MeasureText(timeTxt,  statSz) / 2.f), statY + statSz + 10,       statSz, YELLOW);
    DrawText(killsTxt, (int)(sw / 2.f - MeasureText(killsTxt, statSz) / 2.f), statY + (statSz + 10) * 2, statSz, YELLOW);

    // Prompt
    int promptSz = (int)(sh * 0.038f);
    const char* prompt = "Enter your name:";
    DrawText(prompt,
        (int)(sw / 2.f - MeasureText(prompt, promptSz) / 2.f),
        (int)(sh * 0.50f), promptSz, WHITE);

    // Collect keyboard input
    static constexpr int MAX_NAME_LEN = 20;
    int ch = GetCharPressed();
    while (ch > 0)
    {
        if ((int)_nameBuffer.size() < MAX_NAME_LEN && ch >= 32 && ch < 127)
            _nameBuffer += (char)ch;
        ch = GetCharPressed();
    }
    if (IsKeyPressed(KEY_BACKSPACE) && !_nameBuffer.empty())
        _nameBuffer.pop_back();

    // Blinking cursor
    _cursorBlink += GetFrameTime();
    bool showCursor = (int)(_cursorBlink * 2.f) % 2 == 0;

    // Text box
    float boxW = sw * 0.32f;
    float boxH = sh * 0.072f;
    float boxX = sw / 2.f - boxW / 2.f;
    float boxY = sh * 0.57f;
    DrawRectangleRounded({ boxX, boxY, boxW, boxH }, 0.15f, 6, Color{ 30, 30, 30, 255 });
    DrawRectangleRoundedLines({ boxX, boxY, boxW, boxH }, 0.15f, 6, GRAY);

    int inputSz = (int)(sh * 0.038f);
    std::string display = _nameBuffer + (showCursor ? "|" : " ");
    int textW = MeasureText(display.c_str(), inputSz);
    DrawText(display.c_str(),
        (int)(boxX + boxW / 2.f - textW / 2.f),
        (int)(boxY + boxH / 2.f - inputSz / 2.f),
        inputSz, WHITE);

    // Hint
    int hintSz = (int)(sh * 0.025f);
    const char* hint = "Press ENTER to confirm";
    DrawText(hint,
        (int)(sw / 2.f - MeasureText(hint, hintSz) / 2.f),
        (int)(boxY + boxH + 14.f), hintSz, GRAY);

    // Confirm
    if (IsKeyPressed(KEY_ENTER) && !_nameBuffer.empty())
    {
        std::string confirmed = _nameBuffer;
        return confirmed;
    }

    return "";
}

void PauseAndGameOver::ResetNameEntry()
{
    _nameBuffer.clear();
    _cursorBlink = 0.f;
}

// ── Leaderboard Screen ────────────────────────────────────────────────────────
// Returns true when the Back button is clicked.
bool PauseAndGameOver::DrawLeaderboardScreen(const std::vector<LeaderboardEntry>& scores)
{
    float sw = (float)GetScreenWidth();
    float sh = (float)GetScreenHeight();

    ClearBackground(Color{ 10, 8, 20, 255 });

    // Title
    const char* title = "LEADERBOARD";
    int titleSz = (int)(sw * 0.055f);
    DrawText(title,
        (int)(sw / 2.f - MeasureText(title, titleSz) / 2.f),
        (int)(sh * 0.07f), titleSz, GOLD);

    // Column layout (centred block)
    float blockW  = sw * 0.70f;
    float blockX  = sw / 2.f - blockW / 2.f;
    float colName  = blockX;
    float colWave  = blockX + blockW * 0.36f;
    float colKills = blockX + blockW * 0.52f;
    float colTime  = blockX + blockW * 0.68f;

    int headerSz = (int)(sh * 0.032f);
    int entrySz  = (int)(sh * 0.027f);
    int lbY      = (int)(sh * 0.20f);

    // Column headers
    DrawText("#",     (int)(blockX - sw * 0.03f), lbY, headerSz, WHITE);
    DrawText("Name",  (int)colName,  lbY, headerSz, WHITE);
    DrawText("Wave",  (int)colWave,  lbY, headerSz, WHITE);
    DrawText("Kills", (int)colKills, lbY, headerSz, WHITE);
    DrawText("Time",  (int)colTime,  lbY, headerSz, WHITE);
    lbY += headerSz + 4;

    // Underline
    DrawRectangle((int)(blockX - sw * 0.03f), lbY, (int)(blockW + sw * 0.03f), 2, GRAY);
    lbY += 10;

    if (scores.empty())
    {
        const char* none = "No scores recorded yet. Play a game!";
        DrawText(none,
            (int)(sw / 2.f - MeasureText(none, entrySz) / 2.f),
            lbY, entrySz, GRAY);
    }
    else
    {
        for (int i = 0; i < (int)scores.size(); i++)
        {
            Color col = (i == 0) ? GOLD : (i == 1) ? LIGHTGRAY : Color{ 180, 130, 80, 255 };
            if (i > 2) col = LIGHTGRAY;

            const std::string& n = scores[i].name.empty() ? "???" : scores[i].name;
            DrawText(TextFormat("%d", i + 1),               (int)(blockX - sw * 0.03f), lbY, entrySz, col);
            DrawText(n.c_str(),                             (int)colName,               lbY, entrySz, col);
            DrawText(TextFormat("%d",   scores[i].wave),    (int)colWave,               lbY, entrySz, col);
            DrawText(TextFormat("%d",   scores[i].kills),   (int)colKills,              lbY, entrySz, col);
            DrawText(TextFormat("%.1fs", scores[i].time),   (int)colTime,               lbY, entrySz, col);
            lbY += entrySz + 8;
        }
    }

    // Back button (bottom centre)
    float btnW = sw * 0.16f;
    float btnH = sh * 0.065f;
    float btnX = sw / 2.f - btnW / 2.f;
    float btnY = sh * 0.88f;

    return DrawButton(_btnTex, "Back", { btnX, btnY, btnW, btnH }, Color{ 130, 110, 220, 255 });
}
