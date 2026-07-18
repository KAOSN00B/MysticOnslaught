#pragma once

#include "raylib.h"

enum class DebugActionKind
{
    None,
    GrantInvuln,
    ClearEnemiesContinue,
    RestartRoom,
    SetEliteMechanic,
    SetEliteType,         // value = EliteArchetype index; -1 = random
    ForceEliteSignature,  // trigger the active elite's signature move now
    ForceElitePhaseTwo,   // drop the active elite to its 50% escalation
    SpawnGrunt,
    SpawnCyclops,
    SpawnOgre,
    SpawnBoss,
    SpawnNewEnemy,   // value = enemy index (see kDebugEnemyList in Engine)
    SpawnNewBoss,    // value = boss index  (see kDebugBossList  in Engine)
    GrantRandomRelic,
    GrantAllRelics,
    UnlockAscension,
    Heal,
    RestoreMana,
    AddGold,
    SpawnLoot,      // drop a pile of gold + echo pickups near the player (feel test)
    ForceLevelUp,   // instant level-up (see the power-gain flash)
    AddExp,
    TreasureCards,
    EliteReward,
    AbilityReward,
    ApplyUpgrade
};

struct DebugCommand
{
    DebugActionKind action = DebugActionKind::None;
    int             value  = 0;
    bool            issued = false;
};

class DebugPanel
{
public:
    void Activate();
    void Deactivate();

    bool IsActive()               const { return _active;              }
    bool IsOpen()                 const { return _open;                }
    bool IsGodMode()              const { return _godMode;             }
    int  GetForcedEliteMechanic() const { return _forcedEliteMechanic; }
    void SetForcedEliteMechanic(int v)  { _forcedEliteMechanic = v;   }
    int  GetForcedEliteType()     const { return _forcedEliteType; }
    void SetForcedEliteType(int v)      { _forcedEliteType = v;    }
    void ToggleOpen()                  { _open = !_open;              }
    void SetOpen(bool open)            { _open = open;                }

    // Handles keyboard toggle, scroll, close button, and internal god-mode toggle.
    // Returns a command when a button requires Engine-side action; otherwise issued=false.
    DebugCommand Update();
    void         Draw(int act, int room, const char* roomTypeName) const;

private:
    bool  _active              = false;
    bool  _open                = false;
    bool  _godMode             = false;
    float _scrollY             = 0.f;
    int   _forcedEliteMechanic = -1;
    int   _forcedEliteType     = -1;
};
