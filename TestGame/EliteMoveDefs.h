#pragma once

#include "AttackTuning.h"
#include "GameBalance.h"

#include <string>
#include <vector>

// ─────────────────────────────────────────────────────────────────────────────
// Elite signature moves: their coded timing defaults and which timing values
// the game actually reads. Shared by the Attack Library and the Character
// Animator's moves panel, which both edit the sig_* values in
// attacktuning_<key>.txt.
// ─────────────────────────────────────────────────────────────────────────────
namespace EliteMoves
{
    // Defaults mirror the live values in Balance::Elite so an editor shows
    // exactly what an untuned move does. Travel applies only to movement moves.
    struct MoveDef
    {
        const char* owner; const char* name;
        float telegraph, active, recovery, cooldown, travel, phaseSpeed, phaseCooldown;
    };

    inline const std::vector<MoveDef>& Table()
    {
        namespace Elite = Balance::Elite;
        static const std::vector<MoveDef> table = {
            { "Infernal",  "Cinder March",    Elite::kInfernalMarchTelegraph, Elite::kInfernalMarchActive,
              Elite::kInfernalMarchRecovery,  Elite::kInfernalSignatureCooldown, 0.f, 1.f, 1.f },
            { "Infernal",  "Furnace Burst",   Elite::kInfernalBurstTelegraph, Elite::kInfernalBurstActive,
              Elite::kInfernalBurstRecovery,  Elite::kInfernalSignatureCooldown, 0.f, 1.f, 1.f },
            { "Bonechill", "Permafrost Slam", Elite::kBonechillSlamTelegraph, Elite::kBonechillSlamActive,
              Elite::kBonechillSlamRecovery,  Elite::kBonechillSignatureCooldown, 0.f,
              Elite::kBonechillPhaseSpeed,    Elite::kBonechillPhaseCooldownMult },
            { "Stormclub", "Thunder Leap",    Elite::kStormclubLeapTelegraph, 0.f,
              Elite::kStormclubHitRecovery,   Elite::kStormclubSignatureCooldown,
              Elite::kStormclubLeapRange, 1.f, 1.f },
            { "Venomfang", "Venom Pounce",    Elite::kVenomfangPounceTelegraph, 0.f,
              Elite::kVenomfangPounceRecovery, Elite::kVenomfangSignatureCooldown,
              Elite::kVenomfangPounceRange, 1.f, 1.f },
        };
        return table;
    }

    inline const MoveDef* Find(const std::string& key)
    {
        for (const MoveDef& moveDef : Table())
            if (AttackTuningKey(moveDef.owner, moveDef.name) == key)
                return &moveDef;
        // The Ogre's charge lives in the boss table (it doubles as a boss-support
        // add) but is elite-tunable through the same signature fields.
        static const MoveDef ogreCharge{ "Ogre", "Charge", 3.0f, 0.f, 0.70f, 6.0f, 0.f, 1.f, 1.f };
        if (key == "Ogre_Charge")
            return &ogreCharge;
        return nullptr;
    }

    // Which signature rows this move's RUNTIME actually reads — unsupported
    // rows are hidden entirely so an editor never appears to work while
    // gameplay ignores the value. Bit i = row i (see kRows order:
    // telegraph, active, recovery, cooldown, travel, p2 speed, p2 cooldown).
    inline unsigned SupportedRowMask(const std::string& key)
    {
        if (key == "Ogre_Charge")               return 0b0001101;  // telegraph, recovery(stun), cooldown
        if (key == "Infernal_Cinder_March")     return 0b0001011;  // telegraph, active(march time), cooldown
        if (key == "Infernal_Furnace_Burst")    return 0b0001101;  // telegraph, recovery, cooldown
        if (key == "Bonechill_Permafrost_Slam") return 0b1101101;  // + both phase-two multipliers
        if (key == "Stormclub_Thunder_Leap")    return 0b0011101;  // + travel (leap range)
        if (key == "Venomfang_Venom_Pounce")    return 0b0011101;  // + travel (pounce range)
        return 0b1111111;
    }

    // The timing rows, in signature-value order: telegraph, active, recovery,
    // cooldown, travel, phase-two speed x, phase-two cooldown x.
    struct RowSpec { const char* label; float perPixel; float minValue; float maxValue; };
    constexpr int kRowCount = 7;
    constexpr RowSpec kRows[kRowCount] = {
        { "Telegraph (s)",    0.01f, 0.10f,  6.f },
        { "Active (s)",       0.01f, 0.00f,  6.f },
        { "Recovery (s)",     0.01f, 0.10f,  4.f },
        { "Cooldown (s)",     0.02f, 0.50f, 20.f },
        { "Travel (px)",      1.00f, 0.00f, 1400.f },
        { "P2 Speed x",       0.005f, 0.50f, 2.f },
        { "P2 Cooldown x",    0.005f, 0.30f, 2.f },
    };

    // A move's seven timing values: coded defaults, with saved ones on top.
    inline void Values(const std::string& key, const AttackTuning* saved, float out[kRowCount])
    {
        for (int i = 0; i < kRowCount; i++)
            out[i] = 0.f;
        if (const MoveDef* moveDef = Find(key))
        {
            out[0] = moveDef->telegraph;   out[1] = moveDef->active;
            out[2] = moveDef->recovery;    out[3] = moveDef->cooldown;
            out[4] = moveDef->travel;      out[5] = moveDef->phaseSpeed;
            out[6] = moveDef->phaseCooldown;
        }
        if (saved != nullptr && saved->hasSignature)
        {
            const float values[kRowCount] = { saved->telegraphTime, saved->activeTime, saved->recoveryTime,
                                              saved->signatureCooldown, saved->travelDistance,
                                              saved->phaseSpeedMult, saved->phaseCooldownMult };
            for (int i = 0; i < kRowCount; i++)
                if (values[i] > 0.f)
                    out[i] = values[i];
        }
    }

    // ── Hit areas of the elite moves (Character Animator) ────────────────────
    // Each danger zone's size: the saved hit area when there is one, otherwise
    // Balance::Elite. The elite's warning and CombatDirector's zone both read
    // these, so what is warned is what hits.
    constexpr const char* kInfernalBurstKey     = "Infernal_Furnace_Burst";
    constexpr const char* kInfernalMarchKey     = "Infernal_Cinder_March";
    constexpr const char* kBonechillSlamKey     = "Bonechill_Permafrost_Slam";
    constexpr const char* kBonechillLanesKey    = "Bonechill_Ice_Lanes";
    constexpr const char* kStormclubLeapKey     = "Stormclub_Thunder_Leap";
    constexpr const char* kStormclubBranchesKey = "Stormclub_Lightning_Branches";
    constexpr const char* kOgreChargeKey        = "Ogre_Charge";
    constexpr const char* kVenomfangPounceKey   = "Venomfang_Venom_Pounce";

    inline float InfernalFissureLength() { return TunedHitLength(kInfernalBurstKey, Balance::Elite::kInfernalFissureLength); }
    inline float InfernalFissureWidth()  { return TunedHitWidth(kInfernalBurstKey, Balance::Elite::kInfernalFissureWidth); }
    inline float InfernalPatchRadius()   { return TunedHitRadius(kInfernalMarchKey, Balance::Elite::kInfernalPatchRadius); }
    inline float BonechillSlamRadius()   { return TunedHitRadius(kBonechillSlamKey, Balance::Elite::kBonechillSlamRadius); }
    inline float BonechillSlamReach()    { return TunedHitForward(kBonechillSlamKey, Balance::Elite::kBonechillSlamReach); }
    inline float BonechillLaneLength()   { return TunedHitLength(kBonechillLanesKey, Balance::Elite::kBonechillLaneLength); }
    inline float BonechillLaneWidth()    { return TunedHitWidth(kBonechillLanesKey, Balance::Elite::kBonechillLaneWidth); }
    inline float StormclubImpactRadius() { return TunedHitRadius(kStormclubLeapKey, Balance::Elite::kStormclubImpactRadius); }
    inline float StormclubBranchLength() { return TunedHitLength(kStormclubBranchesKey, Balance::Elite::kStormclubBranchLength); }
    inline float StormclubBranchWidth()  { return TunedHitWidth(kStormclubBranchesKey, Balance::Elite::kStormclubBranchWidth); }
}
