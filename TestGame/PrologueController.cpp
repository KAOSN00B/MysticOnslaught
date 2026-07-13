#include "PrologueController.h"

#include <algorithm>

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
