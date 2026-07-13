#include "PrologueController.h"

#include <cassert>

int main()
{
    PrologueController prologue;
    prologue.Begin();
    assert(prologue.GetPhase() == ProloguePhase::BasicAttack);
    assert(prologue.ShouldSuppressRewards());
    assert(prologue.ShouldAutoRestoreOnLethalHit());

    prologue.Update({ true, false, false, false, false });
    assert(prologue.GetPhase() == ProloguePhase::BasicAttack);
    prologue.Update({ false, false, false, true, false });
    assert(prologue.GetPhase() == ProloguePhase::Ability);

    prologue.Update({ false, true, false, false, false });
    assert(prologue.GetPhase() == ProloguePhase::Ability);
    prologue.Update({ false, false, false, true, false });
    assert(prologue.GetPhase() == ProloguePhase::Dash);
    prologue.Update({ false, false, true, false, false });
    assert(prologue.GetPhase() == ProloguePhase::LastStand);
    assert(!prologue.ShouldAutoRestoreOnLethalHit());

    prologue.Update({ false, false, false, false, true });
    prologue.Update({ false, false, false, false, true });
    assert(!prologue.IsScriptedDeathReady());
    prologue.Update({ false, false, false, false, true });
    assert(prologue.IsScriptedDeathReady());
    assert(prologue.GetPhase() == ProloguePhase::FirstDeath);

    prologue.BeginPoeDialogue();
    assert(prologue.GetPhase() == ProloguePhase::PoeDialogue);
    prologue.Complete();
    assert(prologue.GetPhase() == ProloguePhase::Complete);
    assert(!prologue.ShouldSuppressRewards());
    return 0;
}
