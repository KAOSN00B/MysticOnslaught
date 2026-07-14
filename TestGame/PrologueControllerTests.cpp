#include "PrologueController.h"

#include <cassert>
#include <string>

int main()
{
    assert(ShouldPlayPrologue(PrologueEntryMode::NewGame));
    assert(!ShouldPlayPrologue(PrologueEntryMode::Continue));

    assert(std::string(GetPrologueBasicAttackName(PlayerClass::Mage)) == "Arcane Staff Bolt");
    assert(std::string(GetPrologueBasicAttackName(PlayerClass::Warrior)) == "Sword Slash");
    assert(std::string(GetPrologueBasicAttackName(PlayerClass::Hunter)) == "Bow Shot");
    assert(std::string(GetPrologueBasicAttackName(PlayerClass::Rogue)) == "Dagger Strike");
    assert(std::string(GetPrologueBasicAttackName(PlayerClass::Paladin)) == "Shielded Slash");
    assert(std::string(GetPrologueBasicAttackName(PlayerClass::Warlock)) == "Shadow Staff Bolt");

    assert(GetFirstPoeDialogueLineCount() == 5);
    assert(std::string(GetFirstPoeDialogueLine(0)) ==
           "Hm. You're stronger than you look. Could've been stronger, with proper guidance.");
    assert(std::string(GetFirstPoeDialogueLine(2)).find("kept me very busy") != std::string::npos);
    assert(std::string(GetFirstPoeDialogueLine(4)) == "Do try to last longer next time.");

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
