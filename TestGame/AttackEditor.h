#pragma once

#include "raylib.h"
#include <string>
#include <vector>

// ─────────────────────────────────────────────────────────────────────────────
// AttackEditor — dev tool (opened from the main menu with the K key), sibling to
// the Character Animator. It lists EVERY player ability (auto-enumerated from the
// AbilityType enum, grouped by class) and every boss attack, plays the attack's
// FX strip, and lets you drag-resize the attack's damage hitbox — then saves it
// to attacktuning_<key>.txt (key=value, next to the exe, same pattern as the
// character tuning files, auto-loaded by gameplay once wired in Phase 2).
//
// The tool never changes your numbers on its own — it seeds a default box and you
// resize as you work.
//
//   Screen 1  — attack list (Up/Down or mouse wheel to scroll, click to open).
//   Screen 2  — editor:
//     SPACE   pause / play the FX          , .   step one frame
//     Left-drag a corner handle            resize the hitbox
//     Left-drag the box body               move the hitbox
//     R       reset box to default         S    save        ESC  back
// ─────────────────────────────────────────────────────────────────────────────
class AttackEditor
{
public:
    void Init();
    void Update();
    void Draw();
    void Unload();
    bool WantsToExit() const { return _wantsToExit; }

private:
    enum class Screen { Select, Edit };

    struct AttackItem
    {
        std::string owner;    // "Mage" / "Warrior" / ... / "Werewolf" / ...
        std::string name;     // display name
        std::string key;      // save key: "<owner>_<name>" sanitised
        std::string fxStem;   // FX_<stem>.png (empty = no strip, projectile/blast)
        bool        isBoss = false;
        bool        isElemental = false;  // Fire/Ice/Electric Spread/Bolt/Ultimate
        int         abilityIdx  = -1;     // (int)AbilityType for player abilities
        bool        circleHit = false;    // default hitbox shape
        float       defX = 0.f;           // default centre x, relative to attack origin
        float       defY = 0.f;           // default centre y, relative to attack origin
        float       defW = 140.f;         // default box width  (or diameter for circle)
        float       defH = 140.f;         // default box height
        int         previewKind = 0;       // 0 FX strip, 1 lava, 2 fire bolt, 3 spit, 4 laser
        float       previewScale = 4.f;    // editor-only draw scale
    };

    struct HitBox { float x, y, w, h; };   // centre-relative, authored facing right

    void BuildList();
    void ScanFxCatalog();     // list every PowerUps/FX_*.png stem (for the picker)
    void OpenAttack(int index);
    void CloseAttack();
    void ReloadFx();          // (re)load _fx from the current item's fxStem
    void SaveCurrent();
    void LoadCurrent();
    void UpdateSelect();
    void UpdateEdit();
    void UpdateFxPicker();
    void DrawSelect();
    void DrawEdit();
    void DrawFxPicker();

    HitBox CurrentDefaultBox() const;

    std::vector<AttackItem> _items;
    Screen _screen        = Screen::Select;
    bool   _wantsToExit   = false;
    int    _selectedIdx   = -1;
    float  _scroll        = 0.f;
    std::string _status;

    // ── Edit-screen state ──
    Texture2D _fx{};            // FX strip / projectile sprite for the current attack
    bool      _fxOwned   = true;  // true = editor LoadTexture'd it; false = borrowed (elemental)
    float     _frameW    = 0.f;   // width of one animation frame
    float     _frameH    = 0.f;   // height of one animation frame
    bool      _circleHit = false; // projectile hitboxes are circles, not rects
    int       _fxFrames  = 0;
    int       _frame     = 0;
    float     _frameTimer = 0.f;
    bool      _paused    = false;
    HitBox    _box       = { 0.f, 0.f, 140.f, 140.f };
    Vector2   _fxOffset  = { 0.f, 0.f }; // visual FX centre relative to attacker
    bool      _fxOffsetSet = false;
    bool      _editFxPosition = false;
    int       _dragHandle = -1;         // -1 none, 0..3 corners, 4 body
    Vector2   _dragMouseStart{};
    HitBox    _boxAtDragStart{};

    // ── FX picker (assign any FX_*.png sprite to the current attack) ──
    std::vector<std::string> _fxCatalog;   // available FX stems, scanned once
    bool  _fxPickerOpen   = false;
    float _fxPickerScroll = 0.f;

    // ── Controls / help overlay ──
    bool  _helpOpen = false;
    void  DrawHelp();
};
