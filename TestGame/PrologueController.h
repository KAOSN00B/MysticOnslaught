#pragma once

#include "PlayerClass.h"

enum class PrologueEntryMode
{
    NewGame,
    Continue,
};

bool ShouldPlayPrologue(PrologueEntryMode mode);
const char* GetPrologueBasicAttackName(PlayerClass playerClass);
int GetFirstPoeDialogueLineCount();
const char* GetFirstPoeDialogueLine(int index);

enum class ProloguePhase
{
    Inactive,
    BasicAttack,
    Ability,
    Dash,
    LastStand,
    FirstDeath,
    PoeDialogue,
    Complete,
};

struct PrologueInput
{
    bool basicAttackPressed = false;
    bool abilityPressed = false;
    bool dashPressed = false;
    bool roomCleared = false;
    bool playerHit = false;
};

class PrologueController
{
public:
    void Begin();
    void Reset();
    void Update(const PrologueInput& input);
    void BeginPoeDialogue();
    void Complete();

    ProloguePhase GetPhase() const { return _phase; }
    bool ShouldSuppressRewards() const;
    bool ShouldAutoRestoreOnLethalHit() const;
    bool IsScriptedDeathReady() const { return _phase == ProloguePhase::FirstDeath; }
    int GetLastStandHits() const { return _lastStandHits; }

private:
    ProloguePhase _phase = ProloguePhase::Inactive;
    bool _requiredActionComplete = false;
    int _lastStandHits = 0;
};
