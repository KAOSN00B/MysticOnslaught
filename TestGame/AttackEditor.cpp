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
        if (idx < 36) return "Hunter";
        if (idx < 45) return "Paladin";
        return "Warlock";
    }

    // Every boss and its distinct attacks (no FX strips — bosses use their own
    // sprites/projectiles; the box is what you tune). Names just need to be
    // distinguishable per boss.
    // Per-move: name, circle? , size (w/h, or diameter for circles). Sensible
    // starting hitboxes only — Robert tunes them. Circle = ranged/AoE, rect = melee.
    enum PreviewKind { PreviewFx = 0, PreviewLava = 1, PreviewFireBolt = 2, PreviewSpit = 3, PreviewLaser = 4 };
    struct BossMove  { const char* name; bool circle; float w, h; const char* fx; int preview; float scale; };
    struct BossMoves { const char* boss; std::vector<BossMove> moves; };
    const std::vector<BossMoves>& BossTable()
    {
        static const std::vector<BossMoves> table = {
            { "Molarbeast",  { {"Dash Charge",false,220,130,"BossDashDust",PreviewFx,4.2f}, {"Ranged Volley",true,72,72,"",PreviewLava,4.4f}, {"Melee",false,170,145,"BossClawSwipe",PreviewFx,3.9f} } },
            { "Werewolf",    { {"Swipe Combo",false,220,165,"BossClawSwipe",PreviewFx,4.0f}, {"Pounce",true,190,190,"BossPounceImpact",PreviewFx,4.4f}, {"Blood Howl",true,500,500,"BossBloodHowl",PreviewFx,5.0f} } },
            { "ChompBug",    { {"Dive Bomb",true,170,170,"BossDiveImpact",PreviewFx,4.5f}, {"Acid Spit Fan",true,52,52,"",PreviewSpit,3.8f}, {"Orbit Contact",true,140,140,"BossChitinBurst",PreviewFx,3.8f} } },
            { "Osiris",      { {"Judgement Nova",true,52,52,"",PreviewFireBolt,4.6f}, {"Wrath Volley",true,52,52,"",PreviewFireBolt,4.6f}, {"Sand Step",true,130,130,"BossSandStep",PreviewFx,4.0f}, {"Melee",false,160,145,"BossDivineSlash",PreviewFx,4.0f} } },
            { "TitanGuard",  { {"Bomb Lob",true,82,82,"",PreviewLava,4.4f}, {"Bulwark Slam",true,420,420,"BossBulwarkSlam",PreviewFx,5.0f}, {"Melee",false,180,155,"BossHeavyStrike",PreviewFx,4.2f} } },
            { "ToxicVermin", { {"Eruption",true,250,250,"BossToxicEruption",PreviewFx,4.8f}, {"Toxic Spit Fan",true,52,52,"",PreviewSpit,3.8f}, {"Poison Pool",true,190,190,"BossPoisonPool",PreviewFx,4.5f} } },
            { "AncientBear", { {"Crushing Slam",true,380,380,"BossCrushingSlam",PreviewFx,5.0f}, {"Dream Pull",true,620,620,"BossDreamPull",PreviewFx,5.4f}, {"Contact",false,190,165,"BossClawSwipe",PreviewFx,4.0f} } },
            { "AbyssSlime",  { {"Jump Slam",true,320,320,"BossSlimeSlam",PreviewFx,4.8f}, {"Summon",true,130,130,"BossAbyssSummon",PreviewFx,4.4f}, {"Melee",false,165,150,"BossSlimeSplash",PreviewFx,4.0f} } },
            { "PumpkinJack", { {"Volley",true,52,52,"",PreviewFireBolt,4.6f}, {"Summon",true,130,130,"BossPumpkinSummon",PreviewFx,4.5f}, {"Teleport Strike",false,165,145,"BossTeleportStrike",PreviewFx,4.2f} } },
            { "Minotaur",    { {"Rush",false,240,150,"BossDashDust",PreviewFx,4.4f}, {"Stomp",true,340,340,"BossCrushingSlam",PreviewFx,5.0f}, {"Melee",false,180,155,"BossHeavyStrike",PreviewFx,4.2f} } },
            { "Cyclops",     { {"Laser Sweep",false,520,56,"",PreviewLaser,1.0f}, {"Scatter Shot",true,64,64,"",PreviewLaser,1.0f} } },
            { "Ogre",        { {"Charge",false,220,140,"BossDashDust",PreviewFx,4.2f}, {"Ground Pound",true,320,320,"BossCrushingSlam",PreviewFx,4.9f} } },
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

    void ForwardBox(bool& circle, float& x, float& y, float& w, float& h, float length, float height)
    {
        circle = false;
        x = length * 0.5f;
        y = 0.f;
        w = length;
        h = height;
    }

    void RadialBox(bool& circle, float& x, float& y, float& w, float& h, float radius)
    {
        circle = false;
        x = 0.f;
        y = 0.f;
        w = radius * 2.f;
        h = radius * 2.f;
    }

    void CircleAt(bool& circle, float& x, float& y, float& w, float& h, float cx, float cy, float diameter)
    {
        circle = true;
        x = cx;
        y = cy;
        w = diameter;
        h = diameter;
    }

    void ApplyAbilityDefault(AbilityType ability, bool& circle, float& x, float& y, float& w, float& h)
    {
        switch (ability)
        {
        case AbilityType::FireSpread: case AbilityType::IceSpread: case AbilityType::ElectricSpread:
            CircleAt(circle, x, y, w, h, 0.f, 0.f, 112.f); break;
        case AbilityType::FireBolt: case AbilityType::IceBolt: case AbilityType::ElectricBolt:
            CircleAt(circle, x, y, w, h, 280.f, 0.f, 52.f); break;
        case AbilityType::FireUltimate: case AbilityType::IceUltimate: case AbilityType::ElectricUltimate:
            RadialBox(circle, x, y, w, h, 600.f); break;
        case AbilityType::WarCleave: case AbilityType::Smite: ForwardBox(circle, x, y, w, h, 210.f, 200.f); break;
        case AbilityType::Whirlwind: RadialBox(circle, x, y, w, h, 170.f); break;
        case AbilityType::ThrowingAxe: case AbilityType::Volley: ForwardBox(circle, x, y, w, h, 520.f, 90.f); break;
        case AbilityType::Rend: case AbilityType::Backstab: ForwardBox(circle, x, y, w, h, 150.f, 140.f); break;
        case AbilityType::ShieldBash: ForwardBox(circle, x, y, w, h, 150.f, 150.f); break;
        case AbilityType::WarCry: RadialBox(circle, x, y, w, h, 160.f); break;
        case AbilityType::GroundSlam: RadialBox(circle, x, y, w, h, 340.f); break;
        case AbilityType::Rampage: RadialBox(circle, x, y, w, h, 180.f); break;
        case AbilityType::Earthshatter: ForwardBox(circle, x, y, w, h, 640.f, 130.f); break;
        case AbilityType::FanOfKnives: ForwardBox(circle, x, y, w, h, 360.f, 240.f); break;
        case AbilityType::Shadowstep: ForwardBox(circle, x, y, w, h, 300.f, 110.f); break;
        case AbilityType::PoisonVial: CircleAt(circle, x, y, w, h, 200.f, 0.f, 260.f); break;
        case AbilityType::SmokeBomb: RadialBox(circle, x, y, w, h, 240.f); break;
        case AbilityType::Eviscerate: ForwardBox(circle, x, y, w, h, 210.f, 130.f); break;
        case AbilityType::DeathMark: RadialBox(circle, x, y, w, h, 1200.f); break;
        case AbilityType::BladeDance: RadialBox(circle, x, y, w, h, 230.f); break;
        case AbilityType::RainOfBlades: ForwardBox(circle, x, y, w, h, 680.f, 320.f); break;
        case AbilityType::PiercingShot: ForwardBox(circle, x, y, w, h, 600.f, 70.f); break;
        case AbilityType::Multishot: ForwardBox(circle, x, y, w, h, 380.f, 260.f); break;
        case AbilityType::FrostTrap: CircleAt(circle, x, y, w, h, 0.f, 0.f, 300.f); break;
        case AbilityType::ExplosiveArrow: CircleAt(circle, x, y, w, h, 320.f, 0.f, 340.f); break;
        case AbilityType::Roll: CircleAt(circle, x, y, w, h, 240.f, 0.f, 80.f); break;
        case AbilityType::ArrowStorm: RadialBox(circle, x, y, w, h, 1000.f); break;
        case AbilityType::Deadeye: RadialBox(circle, x, y, w, h, 180.f); break;
        case AbilityType::PiercingBarrage: ForwardBox(circle, x, y, w, h, 900.f, 150.f); break;
        case AbilityType::Consecrate: CircleAt(circle, x, y, w, h, 0.f, 0.f, 340.f); break;
        case AbilityType::ShieldOfFaith: RadialBox(circle, x, y, w, h, 160.f); break;
        case AbilityType::HolyBolt: ForwardBox(circle, x, y, w, h, 560.f, 80.f); break;
        case AbilityType::HammerThrow: ForwardBox(circle, x, y, w, h, 480.f, 110.f); break;
        case AbilityType::LayOnHands: RadialBox(circle, x, y, w, h, 150.f); break;
        case AbilityType::DivineStorm: RadialBox(circle, x, y, w, h, 320.f); break;
        case AbilityType::AvengingWrath: RadialBox(circle, x, y, w, h, 180.f); break;
        case AbilityType::HammerOfJustice: ForwardBox(circle, x, y, w, h, 660.f, 150.f); break;
        case AbilityType::ShadowBolt: ForwardBox(circle, x, y, w, h, 560.f, 80.f); break;
        case AbilityType::DrainLife: ForwardBox(circle, x, y, w, h, 220.f, 130.f); break;
        case AbilityType::Curse: ForwardBox(circle, x, y, w, h, 260.f, 200.f); break;
        case AbilityType::CorruptionPool: CircleAt(circle, x, y, w, h, 200.f, 0.f, 300.f); break;
        case AbilityType::Hellfire: RadialBox(circle, x, y, w, h, 260.f); break;
        case AbilityType::SoulSiphon: RadialBox(circle, x, y, w, h, 230.f); break;
        case AbilityType::Cataclysm: RadialBox(circle, x, y, w, h, 1200.f); break;
        case AbilityType::DemonForm: RadialBox(circle, x, y, w, h, 190.f); break;
        case AbilityType::ShadowNova: ForwardBox(circle, x, y, w, h, 680.f, 200.f); break;
        default: ForwardBox(circle, x, y, w, h, 140.f, 140.f); break;
        }
    }
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
        ApplyAbilityDefault((AbilityType)i, it.circleHit, it.defX, it.defY, it.defW, it.defH);
        _items.push_back(std::move(it));
    }

    // ── Boss attacks ──
    for (const auto& b : BossTable())
        for (const BossMove& mv : b.moves)
        {
            AttackItem it;
            it.owner     = b.boss;
            it.name      = mv.name;
            it.fxStem    = mv.fx ? mv.fx : "";
            it.key       = Sanitise(std::string(b.boss) + "_" + mv.name);
            it.isBoss    = true;
            it.circleHit = mv.circle;
            it.defW      = mv.w;
            it.defH      = mv.h;
            it.previewKind = mv.preview;
            it.previewScale = mv.scale;
            _items.push_back(std::move(it));
        }
}

void AttackEditor::ReloadFx()
{
    if (_fx.id != 0 && _fxOwned) UnloadTexture(_fx);   // never unload borrowed elemental textures
    _fx = Texture2D{};
    _fxOwned = true;
    _fxFrames = 0; _frameW = 0.f; _frameH = 0.f;
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
            _frameH   = 64.f;
        }
        return;
    }

    // Boss projectile previews use the real projectile art where gameplay has one.
    if (it.previewKind == PreviewLava)
    {
        _fx = LoadTexture(AssetPath("Bosses/Lavaball.png").c_str());
        _fxOwned = true;
        _fxFrames = (_fx.id != 0) ? 8 : 0;
        _frameW = (_fxFrames > 0) ? (float)_fx.width / _fxFrames : 0.f;
        _frameH = 64.f;
        return;
    }
    if (it.previewKind == PreviewFireBolt)
    {
        _fx = LoadTexture(AssetPath("PowerUps/Fireball.png").c_str());
        _fxOwned = true;
        _fxFrames = (_fx.id != 0) ? std::max(1, _fx.width / 32) : 0;
        _frameW = 32.f;
                _frameH = 32.f;
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
        _frameH   = (float)SpreadProjectile::GetFrameHFor(ab);
    }
}

AttackEditor::HitBox AttackEditor::CurrentDefaultBox() const
{
    if (_selectedIdx < 0 || _selectedIdx >= (int)_items.size())
        return HitBox{ 0.f, 0.f, 140.f, 140.f };

    const AttackItem& it = _items[_selectedIdx];
    return HitBox{ it.defX, it.defY, it.defW, it.defH };
}
void AttackEditor::OpenAttack(int index)
{
    if (index < 0 || index >= (int)_items.size()) return;
    _selectedIdx = index;
    _paused = false;
    _dragHandle = -1;
    _fxPickerOpen = false;
    _fxOffset = Vector2{ 0.f, 0.f };
    _fxOffsetSet = false;
    _editFxPosition = false;

    // Per-attack default shape + size (projectile/AoE = circle, melee = rect).
    const AttackItem& it = _items[index];
    _circleHit = it.circleHit;
    _box = CurrentDefaultBox();

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
            else if (k == "fx_forward") { _fxOffset.x = val; _fxOffsetSet = true; }
            else if (k == "fx_height")  { _fxOffset.y = val; _fxOffsetSet = true; }
        }
    }
    fclose(f);
}

void AttackEditor::SaveCurrent()
{
    if (_selectedIdx < 0) return;

    // Character Animator owns fire point/projectile/cooldown fields in this
    // same file. Merge our FX + damage-box edits into the existing record so
    // moving between the two views never erases the other view's work.
    AttackTuning merged{};
    if (const AttackTuning* existing = AttackTuningStore::Get(_items[_selectedIdx].key))
        merged = *existing;

    const std::string& stem = _items[_selectedIdx].fxStem;
    merged.hasFx = true;
    merged.fxStem = stem;
    if (_fxOffsetSet)
    {
        merged.hasFxOffset = true;
        merged.fxForward = _fxOffset.x;
        merged.fxHeight = _fxOffset.y;
    }
    merged.hasBox = true;
    merged.x = _box.x;
    merged.y = _box.y;
    merged.w = _box.w;
    merged.h = _box.h;
    if (!AttackTuningStore::Save(_items[_selectedIdx].key, merged))
    {
        _status = "SAVE FAILED";
        return;
    }
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
    if (IsKeyPressed(KEY_X))      { _editFxPosition = !_editFxPosition; _dragHandle = -1; }
    if (IsKeyPressed(KEY_SPACE))  _paused = !_paused;
    if (IsKeyPressed(KEY_R))
    {
        if (_editFxPosition) { _fxOffset = Vector2{ 0.f, 0.f }; _fxOffsetSet = true; _status = "FX origin reset to player"; }
        else { _box = CurrentDefaultBox(); _status = "Hitbox reset to default"; }
    }
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

    if (_editFxPosition && !_items[_selectedIdx].isBoss)
    {
        Vector2 fxHandle{ originX + _fxOffset.x, originY + _fxOffset.y };
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) &&
            CheckCollisionPointCircle(m, fxHandle, 24.f))
            _dragHandle = 5;
        if (IsMouseButtonDown(MOUSE_LEFT_BUTTON) && _dragHandle == 5)
        {
            _fxOffset = Vector2{ m.x - originX, m.y - originY };
            _fxOffsetSet = true;
        }
        if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) _dragHandle = -1;
        return;
    }

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
        { "X, then drag FX ORIGIN", "move the animation relative to the player" },
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

    const char* title = "ATTACK FX + HITBOXES";
    DrawText(title, (int)(sw * 0.5f - MeasureText(title, 52) * 0.5f), 40, 52, Color{ 235, 210, 140, 255 });
    const char* sub = "Character Animator library: every player ability and boss attack.";
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
    const AttackItem& it = _items[_selectedIdx];
    ClearBackground(Color{ 18, 20, 26, 255 });

    // Reference grid + origin (the attack source, facing right).
    const float originX = sw * 0.5f, originY = sh * 0.55f;
    Vector2 fxPos{ originX + _fxOffset.x, originY + _fxOffset.y };
    for (float gx = 0.f; gx < sw; gx += 64.f) DrawLine((int)gx, 0, (int)gx, (int)sh, Color{ 30, 32, 40, 255 });
    for (float gy = 0.f; gy < sh; gy += 64.f) DrawLine(0, (int)gy, (int)sw, (int)gy, Color{ 30, 32, 40, 255 });
    DrawLine((int)originX, (int)(originY - 16.f), (int)originX, (int)(originY + 16.f), Color{ 90, 100, 120, 255 });
    DrawLine((int)(originX - 16.f), (int)originY, (int)(originX + 16.f), (int)originY, Color{ 90, 100, 120, 255 });
    DrawText("origin (attack source) -> facing right", (int)(originX + 22.f), (int)(originY - 8.f), 16, Color{ 90, 100, 120, 255 });
    Color attackerColor = it.isBoss ? Color{ 210, 90, 90, 255 } : Color{ 120, 170, 255, 255 };
    if (it.owner == "Mage") attackerColor = Color{ 120, 150, 255, 255 };
    else if (it.owner == "Warrior") attackerColor = Color{ 220, 70, 60, 255 };
    else if (it.owner == "Rogue") attackerColor = Color{ 160, 85, 220, 255 };
    else if (it.owner == "Hunter") attackerColor = Color{ 90, 190, 100, 255 };
    else if (it.owner == "Paladin") attackerColor = Color{ 245, 210, 110, 255 };
    else if (it.owner == "Warlock") attackerColor = Color{ 135, 75, 190, 255 };

    DrawCircleV({ originX - 18.f, originY - 34.f }, 16.f, Fade(attackerColor, 0.95f));
    DrawRectangleRounded({ originX - 42.f, originY - 18.f, 48.f, 62.f }, 0.3f, 8, Fade(attackerColor, 0.78f));
    DrawLineEx({ originX - 4.f, originY - 4.f }, { originX + 32.f, originY - 2.f }, 6.f, Fade(attackerColor, 0.95f));
    DrawCircleV({ originX + 36.f, originY - 2.f }, 6.f, GOLD);
    DrawText(it.owner.c_str(), (int)(originX - 70.f), (int)(originY + 54.f), 18, Fade(RAYWHITE, 0.85f));

    if (!it.isBoss)
    {
        Color fxOriginColor = _editFxPosition ? GOLD : Color{ 90, 235, 210, 255 };
        DrawLineEx({ originX, originY }, fxPos, 2.f, Fade(fxOriginColor, 0.55f));
        DrawCircleV(fxPos, _editFxPosition ? 11.f : 8.f, fxOriginColor);
        DrawCircleLines((int)fxPos.x, (int)fxPos.y, 18.f, fxOriginColor);
        DrawText("FX ORIGIN", (int)fxPos.x + 20, (int)fxPos.y - 28, 18, fxOriginColor);
    }

    bool travelPreview = it.previewKind == PreviewLava || it.previewKind == PreviewFireBolt ||
                         it.previewKind == PreviewSpit || it.previewKind == PreviewLaser ||
                         fabsf(_box.x) > 96.f || (!_circleHit && _box.w > 260.f);
    if (travelPreview)
    {
        Vector2 start = fxPos;
        Vector2 end{ originX + _box.x, originY + _box.y };
        DrawLineEx(start, end, 3.f, Fade(Color{ 120, 210, 255, 255 }, 0.5f));
        for (int i = 1; i <= 6; ++i)
        {
            float t = (float)i / 6.f;
            Vector2 p{ start.x + (end.x - start.x) * t, start.y + (end.y - start.y) * t };
            DrawCircleV(p, 5.f, Fade(Color{ 120, 210, 255, 255 }, 0.35f + 0.08f * i));
        }
        DrawText("travel path", (int)(start.x + 18.f), (int)(start.y - 26.f), 16, Color{ 120, 210, 255, 210 });
    }

    // FX strip playing at the origin.
    if (_fx.id != 0 && _fxFrames > 0)
    {
        float fw = (_frameW > 0.f) ? _frameW : (float)_fx.width;
        float fh = (_frameH > 0.f) ? _frameH : (float)_fx.height;
        float scale = it.previewScale;
        Rectangle src{ _frame * fw, 0.f, fw, fh };
        Rectangle dst{ fxPos.x, fxPos.y, fw * scale, fh * scale };
        DrawTexturePro(_fx, src, dst, Vector2{ dst.width * 0.5f, dst.height * 0.5f }, 0.f, Fade(WHITE, 0.9f));
    }

    if (it.previewKind == PreviewSpit)
    {
        for (int i = 1; i <= 3; ++i)
            DrawCircleV({ originX - i * 28.f, originY }, 18.f - i * 4.f, Fade(Color{ 110, 200, 60, 255 }, 0.42f - i * 0.09f));
        DrawCircleV({ originX, originY }, 34.f, Color{ 80, 160, 40, 255 });
        DrawCircleV({ originX, originY }, 22.f, Color{ 140, 230, 80, 255 });
        DrawCircleV({ originX - 7.f, originY - 7.f }, 7.f, Fade(WHITE, 0.6f));
    }
    else if (it.previewKind == PreviewLaser)
    {
        const float len = (it.name == "Scatter Shot") ? 460.f : 780.f;
        const int beams = (it.name == "Scatter Shot") ? 5 : 1;
        for (int i = 0; i < beams; ++i)
        {
            float t = beams == 1 ? 0.5f : (float)i / (float)(beams - 1);
            float ang = (t - 0.5f) * 70.f * DEG2RAD;
            Vector2 end{ originX + cosf(ang) * len, originY + sinf(ang) * len };
            DrawLineEx({ originX, originY }, end, beams == 1 ? 42.f : 25.f, Fade(Color{ 255, 80, 70, 255 }, 0.24f));
            DrawLineEx({ originX, originY }, end, beams == 1 ? 22.f : 12.f, Color{ 255, 105, 90, 230 });
            DrawLineEx({ originX, originY }, end, beams == 1 ? 7.f : 4.f, Color{ 255, 245, 235, 220 });
        }
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
    DrawRectangle(0, 0, (int)sw, 40, Fade(BLACK, 0.75f));
    DrawText(TextFormat("%s  -  %s", it.owner.c_str(), it.name.c_str()), 12, 8, 26, GOLD);
    if (it.isElemental)
        DrawText("(elemental - real projectile / blast sprite)", (int)(sw - 480.f), 12, 20, Color{ 130, 200, 160, 255 });
    else if (it.fxStem.empty())
        DrawText("(boss visual - Phase 3; tune the box for now)", (int)(sw - 500.f), 12, 20, Color{ 200, 160, 120, 255 });

    DrawRectangle(0, (int)(sh - 64.f), (int)sw, 64, Fade(BLACK, 0.78f));
    const char* dims = _editFxPosition
        ? TextFormat("FX origin   x %.0f   y %.0f   (relative to player)", _fxOffset.x, _fxOffset.y)
        : _circleHit
        ? TextFormat("circle   x %.0f   y %.0f   radius %.0f", _box.x, _box.y, _box.w * 0.5f)
        : TextFormat("rect   x %.0f   y %.0f   w %.0f   h %.0f", _box.x, _box.y, _box.w, _box.h);
    DrawText(dims, 12, (int)(sh - 56.f), 24, RAYWHITE);
    DrawText("X: FX position   Drag: move/resize   SPACE: pause   F: choose FX   R: reset   S: save   [H] Help   ESC: back",
             12, (int)(sh - 28.f), 20, Color{ 175, 170, 190, 255 });
    if (!_status.empty())
        DrawText(_status.c_str(), (int)(sw - MeasureText(_status.c_str(), 22) - 16.f), (int)(sh - 54.f), 22, Color{ 150, 230, 150, 255 });

    if (_fxPickerOpen) DrawFxPicker();
}
