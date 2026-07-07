#pragma warning(disable: 4996)   // fopen/fprintf are fine here; paths are internal

#include "AttackEditor.h"
#include "AbilityType.h"
#include "AttackTuning.h"
#include "SpreadProjectile.h"
#include "AssetPaths.h"
#include "VirtualCanvas.h"

#include <cstdio>
#include <cmath>
#include <algorithm>
#include <filesystem>

namespace fs = std::filesystem;

namespace
{
    // Which class owns each ability, by enum order (the enum is grouped 9-per-class).
    const char* OwnerForAbility(int idx)
    {
        if (idx < 9)  return "Mage";
        if (idx < 18) return "Warrior";
        if (idx < 27) return "Rogue";
        if (idx < 36) return "Ranger";
        if (idx < 45) return "Paladin";
        return "Warlock";
    }

    // Every boss and its distinct attacks (no FX strips — bosses use their own
    // sprites/projectiles; the box is what you tune). Names just need to be
    // distinguishable per boss.
    // Per-move: name, circle? , size (w/h, or diameter for circles). Sensible
    // starting hitboxes only — Robert tunes them. Circle = ranged/AoE, rect = melee.
    struct BossMove  { const char* name; bool circle; float w, h; };
    struct BossMoves { const char* boss; std::vector<BossMove> moves; };
    const std::vector<BossMoves>& BossTable()
    {
        static const std::vector<BossMoves> table = {
            { "Molarbeast",  { {"Dash Charge",false,200,120}, {"Ranged Volley",true,80,80}, {"Melee",false,160,140} } },
            { "Werewolf",    { {"Swipe Combo",false,210,160}, {"Pounce",true,180,180}, {"Blood Howl",true,500,500} } },
            { "ChompBug",    { {"Dive Bomb",true,160,160}, {"Acid Spit Fan",true,80,80}, {"Orbit Contact",true,140,140} } },
            { "Osiris",      { {"Judgement Nova",true,440,440}, {"Wrath Volley",true,72,72}, {"Sand Step",true,120,120}, {"Melee",false,150,140} } },
            { "TitanGuard",  { {"Bomb Lob",true,140,140}, {"Bulwark Slam",true,400,400}, {"Melee",false,170,150} } },
            { "ToxicVermin", { {"Eruption",true,240,240}, {"Toxic Spit Fan",true,80,80}, {"Poison Pool",true,180,180} } },
            { "AncientBear", { {"Crushing Slam",true,360,360}, {"Dream Pull",true,600,600}, {"Contact",false,180,160} } },
            { "AbyssSlime",  { {"Jump Slam",true,300,300}, {"Summon",true,120,120}, {"Melee",false,160,150} } },
            { "PumpkinJack", { {"Volley",true,72,72}, {"Summon",true,120,120}, {"Teleport Strike",false,150,140} } },
            { "Minotaur",    { {"Rush",false,220,140}, {"Stomp",true,320,320}, {"Melee",false,170,150} } },
            { "Cyclops",     { {"Laser Sweep",false,480,60}, {"Scatter Shot",true,80,80} } },
            { "Ogre",        { {"Charge",false,200,130}, {"Ground Pound",true,300,300} } },
        };
        return table;
    }

    std::string Sanitise(const std::string& s)
    {
        std::string out;
        for (char c : s)
            out += (c == ' ' || c == '/' || c == '\\' || c == ':') ? '_' : c;
        return out;
    }

    std::string TuningPath(const std::string& key) { return "attacktuning_" + key + ".txt"; }
}

void AttackEditor::Init()
{
    BuildList();
    ScanFxCatalog();
    _screen      = Screen::Select;
    _wantsToExit = false;
    _selectedIdx = -1;
    _scroll      = 0.f;
    _fxPickerOpen = false;
    _helpOpen     = false;
    _status.clear();
}

void AttackEditor::ScanFxCatalog()
{
    _fxCatalog.clear();
    _fxCatalog.push_back("");   // "(none)" — clears the FX for an attack
    std::string dir = AssetFolderPath("PowerUps");
    std::error_code ec;
    if (!fs::exists(dir, ec)) return;
    for (const auto& entry : fs::directory_iterator(dir, ec))
    {
        if (!entry.is_regular_file()) continue;
        std::string name = entry.path().filename().string();
        if (name.rfind("FX_", 0) != 0) continue;                 // must start FX_
        if (name.size() < 8 || name.substr(name.size() - 4) != ".png") continue;
        _fxCatalog.push_back(name.substr(3, name.size() - 3 - 4)); // strip FX_ and .png
    }
    std::sort(_fxCatalog.begin() + 1, _fxCatalog.end());
}

void AttackEditor::BuildList()
{
    _items.clear();

    // ── Player abilities: auto-enumerate the whole AbilityType enum ──
    for (int i = 0; i < (int)AbilityType::Count; i++)
    {
        AttackItem it;
        it.owner      = OwnerForAbility(i);
        it.name       = GetAbilityName((AbilityType)i);
        it.abilityIdx = i;
        it.isElemental = (i < 9);   // Fire/Ice/Electric Spread/Bolt/Ultimate
        // Elemental spells render their real projectile/blast sprite (not an FX
        // strip); everything else uses its FX_<stem>.png overlay.
        it.fxStem = it.isElemental ? "" : (GetAbilityIconStem((AbilityType)i) ? GetAbilityIconStem((AbilityType)i) : "");
        it.key    = Sanitise(it.owner + "_" + it.name);
        it.isBoss = false;
        // Elemental spread(0-2)/bolt(3-5) = circle; ultimate(6-8) + class = rect.
        if (i <= 2)      { it.circleHit = true;  it.defW = it.defH = 112.f; }   // spread r56
        else if (i <= 5) { it.circleHit = true;  it.defW = it.defH = 52.f;  }   // bolt r26
        else if (i <= 8) { it.circleHit = false; it.defW = 360.f; it.defH = 300.f; } // ult
        else             { it.circleHit = false; it.defW = it.defH = 140.f; }   // class ability
        _items.push_back(std::move(it));
    }

    // ── Boss attacks ──
    for (const auto& b : BossTable())
        for (const BossMove& mv : b.moves)
        {
            AttackItem it;
            it.owner     = b.boss;
            it.name      = mv.name;
            it.fxStem    = "";
            it.key       = Sanitise(std::string(b.boss) + "_" + mv.name);
            it.isBoss    = true;
            it.circleHit = mv.circle;
            it.defW      = mv.w;
            it.defH      = mv.h;
            _items.push_back(std::move(it));
        }
}

void AttackEditor::ReloadFx()
{
    if (_fx.id != 0 && _fxOwned) UnloadTexture(_fx);   // never unload borrowed elemental textures
    _fx = Texture2D{};
    _fxOwned = true;
    _fxFrames = 0; _frameW = 0.f;
    _frame = 0; _frameTimer = 0.f;
    if (_selectedIdx < 0) return;
    const AttackItem& it = _items[_selectedIdx];

    // An explicit FX-strip override (from the FX picker) always wins.
    if (!it.fxStem.empty())
    {
        _fx = LoadTexture(AssetPath(TextFormat("PowerUps/FX_%s.png", it.fxStem.c_str())).c_str());
        _fxOwned = true;
        if (_fx.id != 0 && _fx.height > 0)
        {
            _fxFrames = _fx.width / 64;                 // 64px cells
            _frameW   = _fxFrames > 0 ? (float)_fx.width / _fxFrames : (float)_fx.width;
        }
        return;
    }

    // Elemental spells: borrow the real projectile / blast sprite (owned by
    // SpreadProjectile's static cache — do NOT unload it).
    if (it.isElemental)
    {
        AbilityType ab = (AbilityType)it.abilityIdx;
        _fx       = SpreadProjectile::GetAnimTexture(ab);
        _fxOwned  = false;
        _fxFrames = SpreadProjectile::GetFrameCountFor(ab);
        _frameW   = (float)SpreadProjectile::GetFrameWFor(ab);
    }
}

void AttackEditor::OpenAttack(int index)
{
    if (index < 0 || index >= (int)_items.size()) return;
    _selectedIdx = index;
    _paused = false;
    _dragHandle = -1;
    _fxPickerOpen = false;

    // Per-attack default shape + size (projectile/AoE = circle, melee = rect).
    const AttackItem& it = _items[index];
    _circleHit = it.circleHit;
    _box = HitBox{ 0.f, 0.f, it.defW, it.defH };

    LoadCurrent();     // may override _box AND this item's fxStem
    ReloadFx();        // load the (possibly overridden) sprite
    _screen = Screen::Edit;
    _status.clear();
}

void AttackEditor::CloseAttack()
{
    if (_fx.id != 0 && _fxOwned) UnloadTexture(_fx);
    _fx = Texture2D{}; _fxFrames = 0;
    _screen   = Screen::Select;
}

void AttackEditor::LoadCurrent()
{
    if (_selectedIdx < 0) return;
    FILE* f = fopen(TuningPath(_items[_selectedIdx].key).c_str(), "r");
    if (!f) return;
    char line[256];
    while (fgets(line, sizeof(line), f))
    {
        char key[64]; char sval[128]; float val;
        if (sscanf(line, "%63[^=]=%127s", key, sval) == 2 && std::string(key) == "fx")
        {
            _items[_selectedIdx].fxStem = (std::string(sval) == "none") ? "" : sval;
            continue;
        }
        if (sscanf(line, "%63[^=]=%f", key, &val) == 2)
        {
            std::string k = key;
            if      (k == "box_x") _box.x = val;
            else if (k == "box_y") _box.y = val;
            else if (k == "box_w") _box.w = val;
            else if (k == "box_h") _box.h = val;
        }
    }
    fclose(f);
}

void AttackEditor::SaveCurrent()
{
    if (_selectedIdx < 0) return;
    FILE* f = fopen(TuningPath(_items[_selectedIdx].key).c_str(), "w");
    if (!f) { _status = "SAVE FAILED"; return; }
    const std::string& stem = _items[_selectedIdx].fxStem;
    fprintf(f, "fx=%s\n", stem.empty() ? "none" : stem.c_str());
    fprintf(f, "box_x=%.2f\n", _box.x);
    fprintf(f, "box_y=%.2f\n", _box.y);
    fprintf(f, "box_w=%.2f\n", _box.w);
    fprintf(f, "box_h=%.2f\n", _box.h);
    fclose(f);
    AttackTuningStore::Reload(_items[_selectedIdx].key);   // so gameplay picks it up live
    _status = "Saved " + _items[_selectedIdx].key + ".txt";
}

void AttackEditor::Unload()
{
    if (_fx.id != 0 && _fxOwned) UnloadTexture(_fx);
    _fx = Texture2D{};
    _items.clear();
}

void AttackEditor::Update()
{
    if (_screen == Screen::Select) UpdateSelect();
    else                          UpdateEdit();
}

void AttackEditor::UpdateSelect()
{
    if (_helpOpen) { if (IsKeyPressed(KEY_H) || IsKeyPressed(KEY_ESCAPE)) _helpOpen = false; return; }
    if (IsKeyPressed(KEY_H)) { _helpOpen = true; return; }
    if (IsKeyPressed(KEY_ESCAPE)) { _wantsToExit = true; return; }

    _scroll -= GetMouseWheelMove() * 48.f;
    float rowH = 34.f;
    float maxScroll = std::max(0.f, _items.size() * rowH - (kVirtualHeight - 160.f));
    _scroll = std::clamp(_scroll, 0.f, maxScroll);

    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
    {
        Vector2 m = GetVirtualMousePos();
        float listX = 80.f, listY = 120.f, listW = kVirtualWidth - 160.f;
        for (int i = 0; i < (int)_items.size(); i++)
        {
            float y = listY + i * rowH - _scroll;
            if (y < listY - rowH || y > kVirtualHeight) continue;
            Rectangle row{ listX, y, listW, rowH - 4.f };
            if (CheckCollisionPointRec(m, row)) { OpenAttack(i); return; }
        }
    }
}

void AttackEditor::UpdateEdit()
{
    if (_helpOpen) { if (IsKeyPressed(KEY_H) || IsKeyPressed(KEY_ESCAPE)) _helpOpen = false; return; }
    if (_fxPickerOpen) { UpdateFxPicker(); return; }
    if (IsKeyPressed(KEY_H))       { _helpOpen = true; return; }
    if (IsKeyPressed(KEY_ESCAPE)) { CloseAttack(); return; }
    if (IsKeyPressed(KEY_F))      { _fxPickerOpen = true; _fxPickerScroll = 0.f; return; }
    if (IsKeyPressed(KEY_SPACE))  _paused = !_paused;
    if (IsKeyPressed(KEY_R))      { _box = DefaultBox(); _status = "Reset to default"; }
    if (IsKeyPressed(KEY_S))      SaveCurrent();

    // FX playback
    if (_fxFrames > 1)
    {
        if (IsKeyPressed(KEY_PERIOD)) { _frame = (_frame + 1) % _fxFrames; _paused = true; }
        if (IsKeyPressed(KEY_COMMA))  { _frame = (_frame + _fxFrames - 1) % _fxFrames; _paused = true; }
        if (!_paused)
        {
            _frameTimer += GetFrameTime();
            if (_frameTimer >= 1.f / 14.f) { _frameTimer = 0.f; _frame = (_frame + 1) % _fxFrames; }
        }
    }

    // Hitbox drag/resize
    const float originX = kVirtualWidth * 0.5f;
    const float originY = kVirtualHeight * 0.55f;
    float cx = originX + _box.x, cy = originY + _box.y;
    Vector2 corners[4] = {
        { cx - _box.w * 0.5f, cy - _box.h * 0.5f }, { cx + _box.w * 0.5f, cy - _box.h * 0.5f },
        { cx - _box.w * 0.5f, cy + _box.h * 0.5f }, { cx + _box.w * 0.5f, cy + _box.h * 0.5f },
    };
    Vector2 m = GetVirtualMousePos();

    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
    {
        _dragHandle = -1;
        for (int i = 0; i < 4; i++)
            if (CheckCollisionPointCircle(m, corners[i], 16.f)) { _dragHandle = i; break; }
        if (_dragHandle < 0)
        {
            Rectangle body{ cx - _box.w * 0.5f, cy - _box.h * 0.5f, _box.w, _box.h };
            if (CheckCollisionPointRec(m, body)) _dragHandle = 4;
        }
        _dragMouseStart = m;
        _boxAtDragStart = _box;
    }
    if (IsMouseButtonDown(MOUSE_LEFT_BUTTON) && _dragHandle >= 0)
    {
        if (_dragHandle == 4)   // move
        {
            _box.x = _boxAtDragStart.x + (m.x - _dragMouseStart.x);
            _box.y = _boxAtDragStart.y + (m.y - _dragMouseStart.y);
        }
        else if (_circleHit)    // radius = distance from centre (stays round)
        {
            float dx = m.x - cx, dy = m.y - cy;
            _box.w = _box.h = std::max(8.f, sqrtf(dx * dx + dy * dy) * 2.f);
        }
        else                    // resize rect symmetrically about the centre
        {
            _box.w = std::max(8.f, fabsf(m.x - cx) * 2.f);
            _box.h = std::max(8.f, fabsf(m.y - cy) * 2.f);
        }
    }
    if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) _dragHandle = -1;
}

void AttackEditor::UpdateFxPicker()
{
    if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_F)) { _fxPickerOpen = false; return; }

    float rowH = 30.f, panelH = kVirtualHeight - 200.f;
    _fxPickerScroll -= GetMouseWheelMove() * 40.f;
    float maxScroll = std::max(0.f, _fxCatalog.size() * rowH - panelH);
    _fxPickerScroll = std::clamp(_fxPickerScroll, 0.f, maxScroll);

    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
    {
        Vector2 m = GetVirtualMousePos();
        float px = kVirtualWidth * 0.5f - 250.f, py = 130.f, pw = 500.f;
        for (int i = 0; i < (int)_fxCatalog.size(); i++)
        {
            float y = py + i * rowH - _fxPickerScroll;
            if (y < py - rowH || y > py + panelH) continue;
            Rectangle row{ px, y, pw, rowH - 3.f };
            if (CheckCollisionPointRec(m, row))
            {
                _items[_selectedIdx].fxStem = _fxCatalog[i];
                ReloadFx();
                _status = _fxCatalog[i].empty() ? "FX cleared (save to keep)"
                                                : ("FX -> " + _fxCatalog[i] + " (save to keep)");
                _fxPickerOpen = false;
                return;
            }
        }
    }
}

void AttackEditor::Draw()
{
    if (_screen == Screen::Select) DrawSelect();
    else                          DrawEdit();
    if (_helpOpen) DrawHelp();
}

void AttackEditor::DrawHelp()
{
    const float sw = (float)kVirtualWidth, sh = (float)kVirtualHeight;
    DrawRectangle(0, 0, (int)sw, (int)sh, Fade(BLACK, 0.72f));
    float pw = 760.f, ph = 560.f, px = sw * 0.5f - pw * 0.5f, py = sh * 0.5f - ph * 0.5f;
    DrawRectangleRounded({ px, py, pw, ph }, 0.04f, 8, Color{ 26, 24, 34, 255 });
    DrawRectangleRoundedLines({ px, py, pw, ph }, 0.04f, 8, Color{ 235, 210, 140, 255 });

    const char* title = "ATTACK EDITOR - CONTROLS";
    DrawText(title, (int)(sw * 0.5f - MeasureText(title, 34) * 0.5f), (int)(py + 26.f), 34, Color{ 235, 210, 140, 255 });

    struct Row { const char* keys; const char* what; };
    const Row rows[] = {
        { "-- Attack list --", "" },
        { "Mouse wheel",   "scroll the list" },
        { "Click a row",   "open that attack" },
        { "",              "" },
        { "-- Editor --",  "" },
        { "SPACE",         "play / pause the animation" },
        { ", and .",       "step one frame back / forward" },
        { "Drag a corner handle", "resize the hitbox (round for projectiles)" },
        { "Drag the box body",    "move the hitbox" },
        { "F",             "change / assign an FX sprite" },
        { "R",             "reset the box to its default" },
        { "S",             "save  ->  attacktuning_<attack>.txt" },
        { "ESC",           "back to the list" },
        { "",              "" },
        { "-- Anywhere --", "" },
        { "H",             "open / close this help" },
        { "ESC (on list)", "exit the editor" },
    };
    float y = py + 84.f;
    for (const Row& r : rows)
    {
        bool header = (r.keys[0] == '-');
        DrawText(r.keys, (int)(px + 40.f), (int)y, header ? 22 : 22,
                 header ? Color{ 150, 200, 255, 255 } : GOLD);
        if (!header) DrawText(r.what, (int)(px + 320.f), (int)y, 22, Color{ 210, 208, 222, 255 });
        y += 27.f;
    }
    const char* close = "H or ESC to close";
    DrawText(close, (int)(sw * 0.5f - MeasureText(close, 20) * 0.5f), (int)(py + ph - 34.f), 20, Color{ 170, 165, 185, 255 });
}

void AttackEditor::DrawFxPicker()
{
    const float sw = (float)kVirtualWidth, sh = (float)kVirtualHeight;
    DrawRectangle(0, 0, (int)sw, (int)sh, Fade(BLACK, 0.62f));
    float px = sw * 0.5f - 250.f, py = 130.f, pw = 500.f, ph = sh - 200.f;
    DrawRectangleRounded({ px - 16.f, py - 60.f, pw + 32.f, ph + 92.f }, 0.03f, 6, Color{ 26, 24, 34, 255 });
    const char* t = "PICK AN FX SPRITE";
    DrawText(t, (int)(sw * 0.5f - MeasureText(t, 30) * 0.5f), (int)(py - 50.f), 30, Color{ 235, 210, 140, 255 });

    Vector2 m = GetVirtualMousePos();
    float rowH = 30.f;
    BeginScissorMode((int)px, (int)py, (int)pw, (int)ph);
    for (int i = 0; i < (int)_fxCatalog.size(); i++)
    {
        float y = py + i * rowH - _fxPickerScroll;
        if (y < py - rowH || y > py + ph) continue;
        const std::string& stem = _fxCatalog[i];
        Rectangle row{ px, y, pw, rowH - 3.f };
        bool hov = CheckCollisionPointRec(m, row);
        bool cur = (_selectedIdx >= 0 && _items[_selectedIdx].fxStem == stem);
        DrawRectangleRec(row, hov ? Color{ 48, 44, 62, 255 } : Color{ 32, 30, 42, 220 });
        const char* label = stem.empty() ? "(none)" : stem.c_str();
        DrawText(label, (int)(px + 12.f), (int)(y + 5.f), 22, cur ? GOLD : (hov ? RAYWHITE : Color{ 190, 188, 205, 255 }));
    }
    EndScissorMode();

    const char* h = "Click to assign    Wheel: scroll    F / ESC: cancel";
    DrawText(h, (int)(sw * 0.5f - MeasureText(h, 20) * 0.5f), (int)(py + ph + 10.f), 20, Color{ 170, 165, 185, 255 });
}

void AttackEditor::DrawSelect()
{
    const float sw = (float)kVirtualWidth;
    ClearBackground(Color{ 20, 18, 28, 255 });

    const char* title = "ATTACK EDITOR";
    DrawText(title, (int)(sw * 0.5f - MeasureText(title, 52) * 0.5f), 40, 52, Color{ 235, 210, 140, 255 });
    const char* sub = "Every player ability + boss attack. Click one to view its FX and resize its hitbox.";
    DrawText(sub, (int)(sw * 0.5f - MeasureText(sub, 22) * 0.5f), 96, 22, Color{ 180, 175, 195, 255 });

    float rowH = 34.f, listX = 80.f, listY = 120.f, listW = sw - 160.f;
    Vector2 m = GetVirtualMousePos();
    std::string lastOwner;
    for (int i = 0; i < (int)_items.size(); i++)
    {
        float y = listY + i * rowH - _scroll;
        if (y < listY - rowH || y > kVirtualHeight - 40.f) continue;
        const AttackItem& it = _items[i];
        Rectangle row{ listX, y, listW, rowH - 4.f };
        bool hov = CheckCollisionPointRec(m, row);
        DrawRectangleRec(row, hov ? Color{ 46, 42, 60, 255 } : Color{ 30, 28, 40, 220 });
        Color ownerCol = it.isBoss ? Color{ 235, 130, 130, 255 } : Color{ 150, 200, 255, 255 };
        DrawText(it.owner.c_str(), (int)(listX + 12.f), (int)(y + 6.f), 22, ownerCol);
        DrawText(it.name.c_str(), (int)(listX + 240.f), (int)(y + 6.f), 22, hov ? GOLD : RAYWHITE);
        if (!it.fxStem.empty())
            DrawText(TextFormat("FX_%s", it.fxStem.c_str()), (int)(listX + listW - 320.f), (int)(y + 8.f), 18, Color{ 120, 150, 120, 255 });
    }
    const char* selHint = "Wheel: scroll    Click: open    [H] Controls    ESC: back";
    DrawText(selHint, (int)(sw * 0.5f - MeasureText(selHint, 22) * 0.5f),
             (int)(kVirtualHeight - 34.f), 22, Color{ 170, 165, 185, 255 });
}

void AttackEditor::DrawEdit()
{
    const float sw = (float)kVirtualWidth, sh = (float)kVirtualHeight;
    ClearBackground(Color{ 18, 20, 26, 255 });

    // Reference grid + origin (the attack source, facing right).
    const float originX = sw * 0.5f, originY = sh * 0.55f;
    for (float gx = 0.f; gx < sw; gx += 64.f) DrawLine((int)gx, 0, (int)gx, (int)sh, Color{ 30, 32, 40, 255 });
    for (float gy = 0.f; gy < sh; gy += 64.f) DrawLine(0, (int)gy, (int)sw, (int)gy, Color{ 30, 32, 40, 255 });
    DrawLine((int)originX, (int)(originY - 16.f), (int)originX, (int)(originY + 16.f), Color{ 90, 100, 120, 255 });
    DrawLine((int)(originX - 16.f), (int)originY, (int)(originX + 16.f), (int)originY, Color{ 90, 100, 120, 255 });
    DrawText("origin (player) -> facing right", (int)(originX + 22.f), (int)(originY - 8.f), 16, Color{ 90, 100, 120, 255 });

    // FX strip playing at the origin.
    if (_fx.id != 0 && _fxFrames > 0)
    {
        float fw = (_frameW > 0.f) ? _frameW : (float)_fx.width;
        float scale = 4.0f;
        Rectangle src{ _frame * fw, 0.f, fw, (float)_fx.height };
        Rectangle dst{ originX, originY, fw * scale, _fx.height * scale };
        DrawTexturePro(_fx, src, dst, Vector2{ dst.width * 0.5f, dst.height * 0.5f }, 0.f, Fade(WHITE, 0.9f));
    }

    // Hitbox (rect, or circle for projectiles) + corner handles.
    float cx = originX + _box.x, cy = originY + _box.y;
    Rectangle box{ cx - _box.w * 0.5f, cy - _box.h * 0.5f, _box.w, _box.h };
    if (_circleHit)
    {
        DrawCircleV({ cx, cy }, _box.w * 0.5f, Fade(Color{ 255, 70, 70, 255 }, 0.18f));
        DrawCircleLines((int)cx, (int)cy, _box.w * 0.5f, Color{ 255, 90, 90, 255 });
    }
    else
    {
        DrawRectangleRec(box, Fade(Color{ 255, 70, 70, 255 }, 0.18f));
        DrawRectangleLinesEx(box, 2.f, Color{ 255, 90, 90, 255 });
    }
    Vector2 corners[4] = {
        { box.x, box.y }, { box.x + box.width, box.y },
        { box.x, box.y + box.height }, { box.x + box.width, box.y + box.height },
    };
    for (int i = 0; i < 4; i++)
        DrawCircleV(corners[i], 8.f, (_dragHandle == i) ? GOLD : Color{ 255, 160, 90, 255 });

    // Header + values.
    const AttackItem& it = _items[_selectedIdx];
    DrawRectangle(0, 0, (int)sw, 40, Fade(BLACK, 0.75f));
    DrawText(TextFormat("%s  -  %s", it.owner.c_str(), it.name.c_str()), 12, 8, 26, GOLD);
    if (it.isElemental)
        DrawText("(elemental - real projectile / blast sprite)", (int)(sw - 480.f), 12, 20, Color{ 130, 200, 160, 255 });
    else if (it.fxStem.empty())
        DrawText("(boss visual - Phase 3; tune the box for now)", (int)(sw - 500.f), 12, 20, Color{ 200, 160, 120, 255 });

    DrawRectangle(0, (int)(sh - 64.f), (int)sw, 64, Fade(BLACK, 0.78f));
    const char* dims = _circleHit
        ? TextFormat("circle   x %.0f   y %.0f   radius %.0f", _box.x, _box.y, _box.w * 0.5f)
        : TextFormat("rect   x %.0f   y %.0f   w %.0f   h %.0f", _box.x, _box.y, _box.w, _box.h);
    DrawText(dims, 12, (int)(sh - 56.f), 24, RAYWHITE);
    DrawText("Drag: resize/move   SPACE: pause   , . step   F: FX   R: reset   S: save   [H] Controls   ESC: back",
             12, (int)(sh - 28.f), 20, Color{ 175, 170, 190, 255 });
    if (!_status.empty())
        DrawText(_status.c_str(), (int)(sw - MeasureText(_status.c_str(), 22) - 16.f), (int)(sh - 54.f), 22, Color{ 150, 230, 150, 255 });

    if (_fxPickerOpen) DrawFxPicker();
}
