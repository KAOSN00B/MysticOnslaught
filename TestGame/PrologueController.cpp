#include "PrologueController.h"

#include <algorithm>

namespace
{
    constexpr const char* kFirstPoeDialogue[] = {
        "Hm. You're stronger than you look. Could've been stronger, with proper guidance.",
        "Guess you'll do.",
        "These monsters have kept me very busy. Every body they leave behind becomes my problem.",
        "I can bring you back. In exchange, you're going to thin their numbers for me.",
        "Do try to last longer next time.",
    };
}

bool ShouldPlayPrologue(PrologueEntryMode mode)
{
    return mode == PrologueEntryMode::NewGame;
}

const char* GetPrologueBasicAttackName(PlayerClass playerClass)
{
    switch (playerClass)
    {
    case PlayerClass::Mage:    return "Arcane Staff Bolt";
    case PlayerClass::Warrior: return "Sword Slash";
    case PlayerClass::Hunter:  return "Bow Shot";
    case PlayerClass::Rogue:   return "Dagger Strike";
    case PlayerClass::Paladin: return "Shielded Slash";
    case PlayerClass::Warlock: return "Shadow Staff Bolt";
    default:                   return "Basic Attack";
    }
}

int GetFirstPoeDialogueLineCount()
{
    return (int)(sizeof(kFirstPoeDialogue) / sizeof(kFirstPoeDialogue[0]));
}

const char* GetFirstPoeDialogueLine(int index)
{
    index = std::clamp(index, 0, GetFirstPoeDialogueLineCount() - 1);
    return kFirstPoeDialogue[index];
}

void PrologueController::Begin()
{
    _phase = ProloguePhase::BasicAttack;
    _requiredActionComplete = false;
    _lastStandHits = 0;
}

void PrologueController::Reset()
{
    _phase = ProloguePhase::Inactive;
    _requiredActionComplete = false;
    _lastStandHits = 0;
}

void PrologueController::Update(const PrologueInput& input)
{
    switch (_phase)
    {
    case ProloguePhase::BasicAttack:
        _requiredActionComplete = _requiredActionComplete || input.basicAttackPressed;
        if (_requiredActionComplete && input.roomCleared)
        {
            _phase = ProloguePhase::Ability;
            _requiredActionComplete = false;
        }
        break;
    case ProloguePhase::Ability:
        _requiredActionComplete = _requiredActionComplete || input.abilityPressed;
        if (_requiredActionComplete && input.roomCleared)
        {
            _phase = ProloguePhase::Dash;
            _requiredActionComplete = false;
        }
        break;
    case ProloguePhase::Dash:
        if (input.dashPressed)
            _phase = ProloguePhase::LastStand;
        break;
    case ProloguePhase::LastStand:
        if (input.playerHit)
        {
            _lastStandHits = std::min(_lastStandHits + 1, 3);
            if (_lastStandHits >= 3)
                _phase = ProloguePhase::FirstDeath;
        }
        break;
    default:
        break;
    }
}

void PrologueController::BeginPoeDialogue()
{
    if (_phase == ProloguePhase::FirstDeath)
        _phase = ProloguePhase::PoeDialogue;
}

void PrologueController::Complete()
{
    _phase = ProloguePhase::Complete;
}

bool PrologueController::ShouldSuppressRewards() const
{
    return _phase != ProloguePhase::Inactive && _phase != ProloguePhase::Complete;
}

bool PrologueController::ShouldAutoRestoreOnLethalHit() const
{
    return _phase == ProloguePhase::BasicAttack ||
           _phase == ProloguePhase::Ability ||
           _phase == ProloguePhase::Dash;
}
