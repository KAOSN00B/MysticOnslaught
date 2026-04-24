#pragma once
#include "raylib.h"
#include "Character.h"
#include "AbilityType.h"
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

    // Reset shop state and generate fresh inventory for this Store room.
    // act: 1-indexed act number, used to scale prices for inflation.
    void Enter(Vector2 npcWorldPos, Character& player, int act = 1);

    // ── Per-frame (Store room, GameState::Play) ───────────────────────────
    // Handles NPC collision push and proximity. Returns true if the player
    // pressed E to open the shop this frame.
    bool UpdateNpc(Character& player);

    // Draw the NPC sprite at its world position.
    void DrawNpc(Vector2 worldOffset) const;

    // ── Per-frame (GameState::Shop) ───────────────────────────────────────
    // Returns true when the player clicks Leave (engine transitions to Play).
    bool Update(Character& player);

    // Render the full shop screen.
    void Draw(const Character& player) const;

    // ── Inventory ─────────────────────────────────────────────────────────
    void GenerateInventory(const Character& player);

    // ── Queries ───────────────────────────────────────────────────────────
    bool        IsNearNpc()    const { return _nearNpc; }
    Vector2     GetNpcPos()    const { return _npcPos;  }

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
    int         _tab            = 0;    // 0 = wares, 1 = abilities
    std::string _dialogue;
    Vector2     _npcPos         = {};
    bool        _nearNpc        = false;
    int         _rerollCost     = 20;
    int         _act            = 1;    // current act, set on Enter()
    int         _dailyDealIndex = -1;   // index of discounted item (-1 = none)

    ShopTextures _tex;
};
