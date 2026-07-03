#pragma once
#include "raylib.h"
#include "WebGamepad.h"
#include "Character.h"
#include "AbilityType.h"
#include "InputPrompts.h"
#include "MetaProgression.h"
#include <vector>
#include <string>

// ── ShopManager ───────────────────────────────────────────────────────────────
// Owns all Zeph shop state: inventory, NPC position, dialogue, reroll cost.
// Engine holds the textures; ShopManager keeps non-owning pointers.
// Engine calls:
//   _shop.Init(tex)                  once after textures are loaded
//   _shop.Enter(npcPos, player)      when entering a Store room
//   _shop.UpdateNpc(player)          every frame from UpdateGamePlay (Store room)
//                                    → returns true  = player just opened the shop
//   _shop.Update(player)             every frame when GameState == Shop
//                                    → returns true  = player clicked Leave
//   _shop.Draw(player)               every frame when GameState == Shop
//   _shop.DrawNpc(worldOffset)       every frame from DrawWorld  (Store room)
//   _shop.GenerateInventory(player)  on Enter and on reroll
// ─────────────────────────────────────────────────────────────────────────────

struct ShopTextures
{
    Texture2D* border          = nullptr;
    Texture2D* zephIdle        = nullptr;
    Texture2D* upgradeAttack   = nullptr;   // AttackPower and related
    Texture2D* upgradeRange    = nullptr;   // AttackRange and related
    Texture2D* upgradeHealth   = nullptr;   // MaxHealth and related
    Texture2D* upgradeMagic    = nullptr;   // MaxMana and related
    Texture2D* upgradeDefense  = nullptr;   // Defense and related
    Texture2D* upgradeSpeed    = nullptr;   // MoveSpeed and related
    Texture2D* abilityFire     = nullptr;
    Texture2D* abilityIce      = nullptr;
    Texture2D* abilityElectric = nullptr;
};

class ShopManager
{
public:
    // ── Setup ────────────────────────────────────────────────────────────────
    void Init(const ShopTextures& tex);

    // Non-owning pointer so GenerateInventory can hide abilities that are still
    // locked behind Legacy Altar (meta progression) purchases.
    void SetMetaProgression(const MetaProgressionManager* meta) { _meta = meta; }

    // Reset shop state and generate fresh inventory for this Store room.
    // act: 1-indexed act number, used to scale prices for inflation.
    void Enter(Vector2 npcWorldPos, Character& player, int act = 1);

    // ── Per-frame (Store room, GameState::Play) ───────────────────────────
    // Handles NPC collision push and proximity. Returns true if the player
    // pressed E to open the shop this frame.
    bool UpdateNpc(Character& player, Vector2 worldOffset, bool touchMode, bool gamepadInteractPressed = false);

    // Draw the NPC sprite at its world position.
    void DrawNpc(Vector2 worldOffset) const;

    // ── Per-frame (GameState::Shop) ───────────────────────────────────────
    // Returns true when the player clicks Leave (engine transitions to Play).
    bool Update(Character& player, bool debugActive = false);

    // Render the full shop screen.
    void Draw(const Character& player, bool debugActive = false) const;

    // Gamepad cursor navigation for the shop grid.
    // Call every frame while GameState == Shop, before Update().
    // Returns true if B / Circle was pressed (leave shop).
    bool UpdateGamepadNav(float dt, const Character& player);

    // ── Inventory ─────────────────────────────────────────────────────────
    void GenerateInventory(const Character& player);

    // ── Queries ───────────────────────────────────────────────────────────
    bool        IsNearNpc()    const { return _nearNpc; }
    Vector2     GetNpcPos()    const { return _npcPos;  }
    Rectangle   GetNpcTouchBtnRect(float sx, float sy) const;
    void        SetPromptMode(InputPromptMode mode) { _promptMode = mode; }

private:
    struct ShopItem
    {
        bool        isAbility   = false;
        UpgradeType upgradeType = UpgradeType::AttackPower;
        AbilityType abilityType = AbilityType::None;
        int         price       = 0;
        bool        purchased   = false;
    };

    std::vector<ShopItem> _inventory;
    const MetaProgressionManager* _meta = nullptr;   // set once by Engine after Init
    int         _tab            = 0;    // 0 = wares, 1 = abilities
    std::string _dialogue;
    Vector2     _npcPos         = {};
    bool        _nearNpc        = false;
    bool        _touchPromptMode = false;
    InputPromptMode _promptMode = InputPromptMode::KeyboardMouse;
    bool        _npcTouchHeld   = false;
    int         _rerollCost     = 20;
    int         _act            = 1;    // current act, set on Enter()
    int         _dailyDealIndex = -1;   // index of discounted item (-1 = none)

    ShopTextures _tex;

    // ── Gamepad cursor navigation ──────────────────────────────────────────
    int   _gamepadCursorIdx      = 0;   // 0-5, item grid cursor
    bool  _gamepadNavActive      = false;
    bool  _gamepadConfirmPending = false;
    float _gamepadNavCooldown    = 0.f;
    // Bottom section (potions / reroll / leave): 0=HP Pot 1=MP Pot 2=Reroll 3=Leave
    bool  _gamepadBottomActive   = false;
    int   _gamepadBottomIdx      = 0;
    bool  _gamepadRerollPending  = false;
    bool  _gamepadHPotPending    = false;
    bool  _gamepadMPotPending    = false;
    // Tab-row cursor (highlight tab first, A to switch)
    bool  _gamepadTabActive      = false;
    int   _gamepadTabCursor      = 0;      // 0=Wares 1=Abilities
    // Left-panel ability Upg/Rem cursor
    bool  _gamepadLPActive       = false;
    int   _gamepadLPIdx          = 0;      // flat index: slot0_upg=0, slot0_rem=1, slot1_upg=2, ...
    int   _gamepadLPUpgSlot      = -1;     // pending upgrade: slot index (-1=none)
    int   _gamepadLPRemSlot      = -1;     // pending remove:  slot index (-1=none)

    // ── Shop intro animation ───────────────────────────────────────────────
    enum class IntroPhase { Off, PanelSettle, ZephReveal, ContentReveal, Done };
    IntroPhase _introPhase        = IntroPhase::Done;
    float      _introTimer        = 0.f;
    bool       _introFirstEntry   = true;
    bool       _introFull         = true;
    float      _introSkipLock     = 0.f;
    bool       _introTouchWasDown = false;

    // Computed by Update(), read by Draw()
    float _introFullAlpha     = 0.f;
    float _introZephAlpha     = 0.f;
    float _introContentAlpha  = 0.f;
    float _introZephScaleMult = 1.f;
    float _introPanelYOff     = 0.f;

    static constexpr float kIntroPanelDur       = 1.32f;
    static constexpr float kIntroShortDur        = 0.15f;
    static constexpr float kIntroZephDur         = 0.84f;
    static constexpr float kIntroContentDur      = 0.84f;
    static constexpr float kIntroPanelStartYOff  = 28.f;
    static constexpr float kIntroZephScaleStart  = 0.75f;
    static constexpr float kIntroOverlayAlpha    = 0.85f;

    // ── UI Editor (debug-only) ─────────────────────────────────────────────
    bool  _isUIEditorActive      = false;
    int   _uiEditorSelectedIndex = 0;

    float _uiPad             = 18.0f;   // 0  outer padding
    float _uiLeftPanelW     = 0.23f;   // 1  left panel width as screen-width multiplier
    float _uiTitleFs        = 35.0f;   // 2  "PLAYER" / "ZEPH'S WARES" font size
    float _uiStatFs         = 42.0f;   // 3  stat row font size
    float _uiSlotFs         = 42.0f;   // 4  ability name in slot
    float _uiSlotBtnFs      = 32.0f;   // 5  Upg/Rem button text
    float _uiHpFs           = 47.0f;   // 6  HP/MP bar label
    float _uiTabH           = 57.0f;   // 7  Wares/Abilities tab height
    float _uiBuyBtnH        = 59.0f;   // 8  buy button height
    float _uiItemNameFs     = 41.0f;   // 9  shop item name font size
    float _uiItemDescFs     = 38.0f;   // 10 shop item description font size
    float _uiItemTextOffsetY = 20.0f;  // 11 Y gap between icon bottom and item name/desc
    float _uiPriceFs        = 45.0f;   // 12 buy button price text font size
    float _uiDialNameFs     = 45.0f;   // 13 "Zeph:" name tag font size
    float _uiDialTextFs     = 31.0f;   // 14 dialogue text font size
    float _uiPotionH        = 63.0f;   // 15 potion strip button height
    float _uiPotionFs       = 31.0f;   // 16 potion button text font size
    float _uiAbilTitleFs    = 38.0f;   // 17 "ABILITIES" section header font size
    float _uiBtnH           = 75.0f;   // 18 Leave / Reroll button height
    float _uiLeaveW         = 201.0f;  // 19 Leave button width
    float _uiRerollW        = 249.0f;  // 20 Reroll button width
    float _uiBtnFs          = 26.0f;   // 21 Leave / Reroll text (auto-shrinks to fit)
    float _uiRarityFs       = 22.0f;   // 22 Rarity label font size
    float _uiRarityPad      = 9.0f;    // 23 Rarity label offset from card corner
    float _uiZephScale      = 1.4f;    // 24 Zeph portrait uniform scale (1.0 = 180px tall)
    float _uiZephPosX       = 545.f;   // 25 Zeph portrait X center (screen space)
    float _uiZephPosY       = 780.f;   // 26 Zeph portrait Y center (screen space)
    float _uiDialPosX       = 620.f;   // 27 Dialogue text block X start (screen space)
    float _uiDialPosY       = 850.f;   // 28 Dialogue text block Y center (screen space)

    // NPC touch button (appears above Zeph in touch mode when near)
    float _uiNpcBtnW        = 220.f;   // 29 button width
    float _uiNpcBtnH        = 75.f;    // 30 button height
    float _uiNpcBtnOffsetX  = 0.f;     // 31 X offset from Zeph screen centre
    float _uiNpcBtnOffsetY  = -165.f;  // 32 Y offset (negative = above Zeph)
    float _uiNpcBtnFs       = 34.f;    // 33 button label font size
};
