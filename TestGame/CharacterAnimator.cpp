#include "CharacterAnimator.h"
#include "VirtualCanvas.h"
#include "PlayerPreview.h"
#include "PlayerClass.h"
#include "SkeletonArcher.h"
#include "FlameWisp.h"
#include "SlimeEnemy.h"
#include "Sporeling.h"
#include "Shieldbearer.h"
#include "Phantom.h"
#include "BomberImp.h"
#include "Warchief.h"
#include "LivingBlade.h"
#include "AbyssSlime.h"
#include "PumpkinJack.h"
#include "Minotaur.h"
#include "Werewolf.h"
#include "ChompBug.h"
#include "Osiris.h"
#include "TitanGuard.h"
#include "ToxicVermin.h"
#include "AncientBear.h"
#include "Cyclops.h"
#include "Ogre.h"
#include "Molarbeast.h"
#include "Bonechill.h"
#include "Infernal.h"
#include "Stormclub.h"
#include "Venomfang.h"
#include "VfxGlow.h"
#include "EliteMoveDefs.h"
#include "AssetPaths.h"
#include "raymath.h"

#include <algorithm>
#include <cmath>
#include <filesystem>

namespace
{
    // Handle ids. The body circle uses Centre + Radius; the melee box uses the
    // four corners + Centre.
    constexpr int kHandleTopLeft     = 0;
    constexpr int kHandleTopRight    = 1;
    constexpr int kHandleBottomLeft  = 2;
    constexpr int kHandleBottomRight = 3;
    constexpr int kHandleCentre      = 4;
    constexpr int kHandleRadius      = 5;
    constexpr int kHandleFxOrigin    = 6;
    constexpr int kHandlePadX        = 7;   // hurtbox padding, right edge
    constexpr int kHandlePadY        = 8;   // hurtbox padding, bottom edge
    constexpr int kHandleInner       = 9;   // a ring move's inner radius
    constexpr int kHandleLaneEnd     = 10;  // a lane move's length
    constexpr int kHandleLaneSide    = 11;  // a lane move's width

    constexpr float kHandleSize = 14.f;

    // ── Layout (1920x1080 canvas) ────────────────────────────────────────────
    constexpr float kPad            = 20.f;
    constexpr float kGap            = 8.f;
    constexpr float kPanelWidth     = 400.f;    // left panel
    constexpr float kRowHeight      = 44.f;
    constexpr float kButtonHeight   = 38.f;
    constexpr float kSmallButton    = 44.f;     // the - / + beside a value
    constexpr float kHeaderGap      = 24.f;     // section header to its first row
    constexpr float kTitleY         = 16.f;
    constexpr float kSubtitleY      = 56.f;
    constexpr float kTabsY          = 96.f;
    constexpr float kTabHeight      = 40.f;
    constexpr float kSizeHeaderY    = 156.f;
    constexpr float kSpeedHeaderY   = 290.f;
    constexpr float kAnimHeaderY    = 424.f;
    constexpr float kListRowHeight  = 34.f;
    constexpr float kListRowStep    = 38.f;
    constexpr float kSaveY          = 906.f;
    constexpr float kHelpY          = 1004.f;
    constexpr float kHelpLineStep   = 24.f;

    constexpr float kTimelineTop    = 868.f;    // bottom bar, between the side panels
    constexpr float kCaptionY       = 876.f;
    constexpr float kTransportY     = 902.f;
    constexpr float kTransportSize  = 44.f;
    constexpr float kHitLaneY       = 954.f;    // the HIT FRAMES lane under the frames
    constexpr float kEventLaneY     = 990.f;    // the EVENTS lane
    constexpr float kLaneHeight     = 28.f;
    constexpr float kLaneHintY      = 1030.f;
    constexpr float kMarkerSize     = 10.f;     // an event's mark in its lane
    constexpr float kToolbarY       = 12.f;     // view controls over the stage
    constexpr float kToolbarHeight  = 36.f;
    constexpr float kRightTabY      = 16.f;
    constexpr float kRightTabHeight = 36.f;
    constexpr float kRightContentY  = 64.f;
    constexpr float kFrameCellMax   = 64.f;
    constexpr float kHeroToggleW    = 80.f;
    constexpr float kSnapToggleW    = 110.f;
    constexpr float kFlipW          = 70.f;
    constexpr float kZoomLabelW     = 80.f;

    constexpr float kRightPanelWidth = 444.f;   // moves / attack / events panel
    constexpr float kRightPad        = 16.f;
    constexpr float kAttackSelectY   = 64.f;
    constexpr float kAttackPreviewY  = 112.f;
    constexpr float kAttackFileY     = 160.f;
    constexpr float kProjectileHeaderY = 186.f;
    constexpr float kLabHeaderY      = 424.f;
    constexpr float kFieldRowHeight  = 34.f;
    constexpr float kFieldRowStep    = 40.f;
    constexpr float kAttackInfoY     = 864.f;
    constexpr float kInfoLineStep    = 22.f;

    constexpr float kSelectTitleY    = 70.f;
    constexpr float kSelectSubtitleY = 140.f;
    constexpr float kSelectButtonsY  = 196.f;
    constexpr float kSelectButtonW   = 430.f;
    constexpr float kSelectButtonH   = 62.f;
    constexpr float kSelectTop       = 300.f;
    constexpr float kSelectColumnW   = 520.f;
    constexpr float kSelectColumnStep = 560.f;
    constexpr float kSelectRowHeight = 48.f;
    constexpr float kSelectRowStep   = 54.f;
    constexpr float kSelectHeaderStep = 36.f;
    constexpr float kSelectGroupGap  = 18.f;
    constexpr int   kSelectColumns   = 3;

    constexpr int kTitleFont   = 32;
    constexpr int kBigFont     = 60;
    constexpr int kHeaderFont  = 18;
    constexpr int kRowFont     = 22;
    constexpr int kButtonFont  = 20;
    constexpr int kSmallFont   = 16;
    constexpr int kCaptionFont = 18;
    constexpr float kTextInset = 12.f;

    // ── View ─────────────────────────────────────────────────────────────────
    constexpr float kMinZoom  = 0.25f;
    constexpr float kMaxZoom  = 4.f;
    constexpr float kFitMaxZoom  = 3.f;         // Fit never blows small sprites up further
    constexpr float kFitShare    = 0.6f;        // share of the stage the fitted group fills
    constexpr float kFitLabelRoom = 150.f;      // world px kept right of the bracket for its label
    constexpr float kZoomStep = 1.15f;          // per wheel notch / button
    constexpr float kSnapStep = 4.f;            // world px: one pixel of hero art
    constexpr float kFineStep = 1.f;            // Shift: no snapping, 1 px nudges

    // ── Size + speed controls ────────────────────────────────────────────────
    constexpr float kSizeStep       = 0.05f;    // - / + in multiples of the hero's height
    constexpr float kSizeDragPerPx  = 0.004f;
    constexpr float kMinSizeVsHero  = 0.25f;
    constexpr float kMaxSizeVsHero  = 8.f;
    constexpr float kScaleEpsilon   = 0.001f;
    constexpr float kMinFrameTime   = 0.02f;
    constexpr float kMaxFrameTime   = 0.6f;
    constexpr float kFrameTimePerPx = 0.0006f;
    constexpr float kFpsStep        = 1.f;
    constexpr float kWheelPixels    = 10.f;     // a wheel notch on a value row = this much drag
    constexpr float kDefaultFrameTime = 1.f / 8.f;   // when a class reports no speed

    // ── Hero reference + measuring ───────────────────────────────────────────
    constexpr float kHeroGap        = 48.f;     // world px between the hero and the character
    constexpr float kBracketGap     = 28.f;     // the size bracket beside the character
    constexpr float kBracketTick    = 10.f;
    constexpr float kLabelLift      = 26.f;     // screen px above a head for its label
    constexpr int   kMeasureCanvas  = 1024;     // off-screen square for measuring drawn sprites
    constexpr unsigned char kMeasureAlpha = 128;   // ignores soft shadows

    // ── Lineup (screen px unless noted; layout converts them at the fitted zoom) ──
    constexpr float kLineupGap        = 24.f;   // between characters
    constexpr float kLineupLabelSlot  = 150.f;  // narrowest slot, so names never overlap
    constexpr float kLineupLabelSpace = 84.f;   // the three label lines under a line
    constexpr float kLineupHeaderSpace = 34.f;  // a row's header above its first line
    constexpr float kLineupMargin     = 50.f;   // around the fitted lineup
    constexpr float kLineupTop        = 90.f;   // reserved for the title
    constexpr float kLineupMaxZoom    = 1.5f;
    constexpr float kLineupLabelGap   = 10.f;
    constexpr float kLineupLineStep   = 20.f;
    constexpr float kLineupHeaderLift = 28.f;
    constexpr int   kLineupFitPasses  = 4;      // layout and zoom settle together
    constexpr float kStatusBottomGap  = 90.f;   // the status message on full-screen pages

    // ── Dialog ───────────────────────────────────────────────────────────────
    constexpr float kDialogWidth   = 680.f;
    constexpr float kDialogHeight  = 230.f;
    constexpr float kDialogButtonW = 180.f;

    // ── Misc ─────────────────────────────────────────────────────────────────
    constexpr size_t kMaxUndo       = 200;
    constexpr size_t kUiReserve     = 320;
    constexpr float  kStatusSeconds = 2.5f;
    constexpr float  kTooltipDelay  = 0.35f;
    constexpr float  kTooltipWidth  = 380.f;
    constexpr float  kTooltipOffset = 22.f;
    constexpr int    kCheckerCell   = 64;
    constexpr float  kRoundness     = 0.25f;
    constexpr int    kSegments      = 6;
    constexpr int    kCircleDashes  = 24;
    constexpr float  kMinBodyRadius = 12.f;    // fallback circle never smaller than this
    constexpr float  kMinMeleeSize  = 8.f;
    constexpr float  kDummyRadius   = 22.f;
    constexpr float  kDummySpread   = 0.55f;
    constexpr float  kFxRingRadius  = 18.f;
    constexpr float  kFireDotRadius = 7.f;
    constexpr float  kMinPreviewShot = 6.f;
    constexpr float  kMinAimRange   = 40.f;
    constexpr float  kMinCadence    = 0.05f;

    // ── Events + moves ───────────────────────────────────────────────────────
    constexpr float kEventFramePerPx  = 0.05f;   // dragging an event's frame row
    constexpr float kEventSizePerPx   = 0.02f;
    constexpr float kMinEventSize     = 0.5f;
    constexpr float kMaxFxSize        = 12.f;
    constexpr float kMaxShake         = 30.f;
    constexpr float kDefaultFxForward = 60.f;    // a new FX event lands in front when there is no melee box
    constexpr float kFxGhostAlpha     = 0.5f;    // the selected FX event's first frame on the stage
    constexpr float kShakeJiggleHz    = 40.f;    // a Shake event's preview wobble
    constexpr float kMovePerPx        = 1.f;     // move sizes, world px per dragged px
    constexpr float kMinMoveSize      = 4.f;
    constexpr float kMaxMoveSize      = 2400.f;
    constexpr float kMinMoveOffset    = -2400.f;
    constexpr unsigned kFlagAuthored  = 8u;      // a hit-lane cell set in the animator (not the code's)
    constexpr float kLiveFillAlpha    = 0.22f;   // the melee box while its hit window is live
    constexpr float kHitLaneSetAlpha  = 0.75f;   // hit-lane cells set here
    constexpr float kHitLaneCodeAlpha = 0.3f;    // hit-lane cells from the code
    constexpr float kMoveFillAlpha    = 0.12f;
    constexpr float kHurtIdleAlpha    = 0.35f;   // the hurtbox while another mode is active
    constexpr float kDashLength       = 8.f;     // the hurtbox's dashed outline
    constexpr float kDashWidth        = 1.5f;
    constexpr int   kRingSegments     = 48;
    const Color kHurtColor = { 255, 225, 90, 255 };   // yellow
    const Color kSoundColor = { 255, 230, 120, 255 };
    const Color kShakeColor = { 255, 150, 60, 255 };

    // Colours
    const Color kBodyColor   = { 90, 220, 255, 255 };   // cyan
    const Color kMeleeColor  = { 255, 110, 110, 255 };  // red
    const Color kAttackColor = { 255, 180, 70, 255 };   // orange
    const Color kSpriteColor = { 220, 120, 255, 255 };  // magenta
    const Color kFxColor     = { 90, 235, 210, 255 };
    const Color kGoodColor   = { 140, 235, 160, 255 };
    const Color kWarnColor   = { 255, 170, 70, 255 };
    const Color kHeroColor   = { 255, 214, 120, 255 };
    const Color kPanelFill   = { 18, 18, 24, 235 };
    const Color kRowFill     = { 56, 56, 70, 255 };
    const Color kRowHot      = { 76, 76, 98, 255 };
    const Color kRowActive   = { 92, 92, 124, 255 };
    const Color kRowOutline  = { 96, 96, 118, 255 };
    const Color kTextDim     = { 170, 170, 185, 255 };
    const Color kCheckerA    = { 40, 40, 48, 255 };
    const Color kCheckerB    = { 46, 46, 56, 255 };
    const Color kDummyFill   = { 210, 75, 75, 255 };
    const Color kDummyLine   = { 235, 105, 105, 230 };
    const Color kTooltipFill = { 12, 12, 16, 245 };

    // The player attack panel's value rows: label, field, drag rate per px,
    // limits, whole numbers only, help text.
    struct AttackField
    {
        const char* label;
        float AttackTuning::* field;
        float perPixel;
        float minValue;
        float maxValue;
        bool  whole;
        const char* tooltip;
    };
    constexpr int kProjectileFieldCount = 5;
    const AttackField kAttackFields[] = {
        { "Scale",         &AttackTuning::projScale,      0.01f,  0.1f,   6.f,    false, "Projectile draw size (a multiplier). Drag sideways or scroll." },
        { "Radius",        &AttackTuning::projRadius,     0.25f,  1.f,    500.f,  false, "Projectile hit radius in world px. The ring's handle drags it too." },
        { "Speed",         &AttackTuning::projSpeed,      5.f,    0.f,    4000.f, false, "Travel speed in px per second." },
        { "Life",          &AttackTuning::projLifetime,   0.02f,  0.05f,  20.f,   false, "Seconds before the projectile despawns." },
        { "Cooldown",      &AttackTuning::cooldown,       0.01f,  0.f,    20.f,   false, "Seconds between shots (the basic attack's cadence)." },
        { "Aim range",     &AttackTuning::aimRange,       2.f,    40.f,   2400.f, false, "How far the ability can be aimed." },
        { "Area radius",   &AttackTuning::areaRadius,     0.5f,   10.f,   800.f,  false, "Radius of the ability's area." },
        { "Effect length", &AttackTuning::effectLength,   1.f,    20.f,   1800.f, false, "Length of a line or cone effect." },
        { "Effect width",  &AttackTuning::effectWidth,    0.4f,   10.f,   600.f,  false, "Width of a line or cone effect." },
        { "Duration",      &AttackTuning::effectDuration, 0.02f,  0.1f,   30.f,   false, "Seconds a zone or trap lasts." },
        { "Tick time",     &AttackTuning::tickInterval,   0.005f, 0.05f,  5.f,    false, "Seconds between a zone's damage ticks." },
        { "Chain range",   &AttackTuning::chainRange,     1.f,    20.f,   1200.f, false, "How far a chain jumps between targets." },
        { "Max targets",   &AttackTuning::maxTargets,     0.03f,  1.f,    20.f,   true,  "The most targets one cast can hit." },
        { "Move distance", &AttackTuning::moveDistance,   1.f,    0.f,    1600.f, false, "How far a dash or leap moves you." },
        { "Preview angle", &AttackTuning::previewAngle,   0.5f,   -180.f, 180.f,  false, "Editor only: which way the preview aims." },
    };
    constexpr int kAttackFieldCount = (int)(sizeof(kAttackFields) / sizeof(kAttackFields[0]));

    // A special move's editable values, by shape: label, its field in the
    // move's attacktuning file, limits.
    struct MoveFieldSpec
    {
        const char* label;
        float AttackTuning::* field;
        float minValue;
        float maxValue;
    };

    std::vector<MoveFieldSpec> MoveFields(const Enemy::MoveInfo& info)
    {
        using Shape = Enemy::MoveShape;
        switch (info.shape)
        {
        case Shape::Circle:
        {
            std::vector<MoveFieldSpec> fields{ { "Radius", &AttackTuning::hitRadius, kMinMoveSize, kMaxMoveSize } };
            if (info.forward != 0.f)
                fields.push_back({ "Ahead", &AttackTuning::hitForward, kMinMoveOffset, kMaxMoveSize });
            return fields;
        }
        case Shape::Ring:
            return { { "Inner radius", &AttackTuning::hitInner, 0.f, kMaxMoveSize },
                     { "Outer radius", &AttackTuning::hitRadius, kMinMoveSize, kMaxMoveSize } };
        case Shape::Lane:
            return { { "Length", &AttackTuning::hitLength, kMinMoveSize, kMaxMoveSize },
                     { "Half width", &AttackTuning::hitWidth, kMinMoveSize, kMaxMoveSize } };
        case Shape::Box:
            return { { "Centre X", &AttackTuning::hitX, kMinMoveOffset, kMaxMoveSize },
                     { "Centre Y", &AttackTuning::hitY, kMinMoveOffset, kMaxMoveSize },
                     { "Width",    &AttackTuning::hitW, kMinMoveSize, kMaxMoveSize },
                     { "Height",   &AttackTuning::hitH, kMinMoveSize, kMaxMoveSize } };
        }
        return {};
    }

    // Every hit-area field, for copying a move's coded size into its file.
    float AttackTuning::* const kAllMoveFields[] = {
        &AttackTuning::hitRadius, &AttackTuning::hitInner, &AttackTuning::hitForward,
        &AttackTuning::hitLength, &AttackTuning::hitWidth,
        &AttackTuning::hitX, &AttackTuning::hitY, &AttackTuning::hitW, &AttackTuning::hitH,
    };

    // The move's coded value for one of its fields (what an unsaved move uses).
    float MoveDefault(const Enemy::MoveInfo& info, float AttackTuning::* field)
    {
        if (field == &AttackTuning::hitRadius)  return info.radius;
        if (field == &AttackTuning::hitInner)   return info.inner;
        if (field == &AttackTuning::hitForward) return info.forward;
        if (field == &AttackTuning::hitLength)  return info.length;
        if (field == &AttackTuning::hitWidth)   return info.width;
        if (field == &AttackTuning::hitX)       return info.box.x + info.box.width * 0.5f;
        if (field == &AttackTuning::hitY)       return info.box.y + info.box.height * 0.5f;
        if (field == &AttackTuning::hitW)       return info.box.width;
        if (field == &AttackTuning::hitH)       return info.box.height;
        return 0.f;
    }

    // What the game uses: the saved hit area, else the coded size.
    float MoveValue(const AttackTuning& tuning, const Enemy::MoveInfo& info, float AttackTuning::* field)
    {
        return tuning.hasHitArea ? tuning.*field : MoveDefault(info, field);
    }

    // A box move's rect, top-left, relative to the character, facing right.
    Rectangle MoveBoxRel(const AttackTuning& tuning, const Enemy::MoveInfo& info)
    {
        const float width  = MoveValue(tuning, info, &AttackTuning::hitW);
        const float height = MoveValue(tuning, info, &AttackTuning::hitH);
        return Rectangle{ MoveValue(tuning, info, &AttackTuning::hitX) - width * 0.5f,
                          MoveValue(tuning, info, &AttackTuning::hitY) - height * 0.5f, width, height };
    }

    const char* ShapeName(Enemy::MoveShape shape)
    {
        switch (shape)
        {
        case Enemy::MoveShape::Circle: return "CIRCLE";
        case Enemy::MoveShape::Ring:   return "RING";
        case Enemy::MoveShape::Lane:   return "LANE";
        case Enemy::MoveShape::Box:    return "BOX";
        }
        return "";
    }

    // Event kinds as the panel shows them, in CharacterTuning::EventKind order.
    const char* kEventKindLabels[] = { "FX", "Sound", "Shake" };
    constexpr int kEventKindCount = (int)(sizeof(kEventKindLabels) / sizeof(kEventKindLabels[0]));

    Color EventColor(CharacterTuning::EventKind kind)
    {
        switch (kind)
        {
        case CharacterTuning::EventKind::Fx:    return kFxColor;
        case CharacterTuning::EventKind::Sound: return kSoundColor;
        case CharacterTuning::EventKind::Shake: return kShakeColor;
        }
        return WHITE;
    }

    // An event's size as the game plays it (0 = the kind's default).
    float EventSizeOf(const CharacterTuning::AnimEvent& event)
    {
        if (event.size > 0.f)
            return event.size;
        return (event.kind == CharacterTuning::EventKind::Shake) ? Balance::AnimEvents::kDefaultShake
                                                                 : Balance::AnimEvents::kDefaultFxSize;
    }

    // A rectangle's outline in dashes (the hurtbox reads as derived, not drawn).
    void DrawDashedRect(Rectangle rect, Color color)
    {
        const Vector2 corners[4] = {
            { rect.x, rect.y }, { rect.x + rect.width, rect.y },
            { rect.x + rect.width, rect.y + rect.height }, { rect.x, rect.y + rect.height } };
        for (int side = 0; side < 4; side++)
        {
            const Vector2 from = corners[side];
            const Vector2 to = corners[(side + 1) % 4];
            const float length = Vector2Distance(from, to);
            const Vector2 step = Vector2Scale(Vector2Normalize(Vector2Subtract(to, from)), kDashLength);
            Vector2 at = from;
            for (float done = 0.f; done < length; done += kDashLength * 2.f)
            {
                const Vector2 end = (done + kDashLength < length) ? Vector2Add(at, step) : to;
                DrawLineEx(at, end, kDashWidth, color);
                at = Vector2Add(end, step);
            }
        }
    }

    // Flags shown beside each animation in the list.
    constexpr unsigned kFlagBody   = 1u;
    constexpr unsigned kFlagMelee  = 2u;
    constexpr unsigned kFlagSprite = 4u;
    constexpr float    kFlagSize   = 10.f;

    template <class T>
    Enemy* MakeInstance()
    {
        auto* enemy = new T(Vector2{ 0.f, 0.f });
        enemy->Init();
        return enemy;
    }

    void DrawCheckerboard()
    {
        for (int gy = 0; gy <= kVirtualHeight / kCheckerCell; gy++)
            for (int gx = 0; gx <= kVirtualWidth / kCheckerCell; gx++)
                DrawRectangle(gx * kCheckerCell, gy * kCheckerCell, kCheckerCell, kCheckerCell,
                    ((gx + gy) % 2 == 0) ? kCheckerA : kCheckerB);
    }

    bool ShiftDown() { return IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT); }
    bool CtrlDown()  { return IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL); }

    // Splits text into lines no wider than maxWidth at fontSize.
    std::vector<std::string> WrapText(const std::string& text, int fontSize, float maxWidth)
    {
        std::vector<std::string> lines;
        std::string line;
        size_t start = 0;
        while (start <= text.size())
        {
            size_t end = text.find(' ', start);
            if (end == std::string::npos)
                end = text.size();
            const std::string word = text.substr(start, end - start);
            const std::string candidate = line.empty() ? word : line + " " + word;
            if (!line.empty() && MeasureText(candidate.c_str(), fontSize) > maxWidth)
            {
                lines.push_back(line);
                line = word;
            }
            else
                line = candidate;
            start = end + 1;
        }
        if (!line.empty())
            lines.push_back(line);
        return lines;
    }

    // Pure self-buffs (damage/lifesteal/reflect/heal only): the game reads no
    // geometry, projectile, zone, or movement tuning for these, so listing them in
    // the attack editor would only offer dead sliders.
    bool AbilityHasTunableAttack(AbilityType ability)
    {
        switch (ability)
        {
        case AbilityType::WarCry:
        case AbilityType::Rampage:
        case AbilityType::Deadeye:
        case AbilityType::ShieldOfFaith:
        case AbilityType::LayOnHands:
        case AbilityType::AvengingWrath:
        case AbilityType::DemonForm:
            return false;
        default:
            return true;
        }
    }

    void ApplyAbilityLabDefaults(AbilityType ability, AttackTuning& tuning)
    {
        auto setDirection = [&](float range, float length, float width, float moveDistance = 0.f) {
            tuning.aimRange = range;
            tuning.effectLength = length;
            tuning.effectWidth = width;
            if (moveDistance > 0.f) tuning.moveDistance = moveDistance;
        };
        auto setGround = [&](float range, float radius) {
            tuning.aimRange = range;
            tuning.areaRadius = radius;
        };
        auto setZoneTiming = [&](float duration, float tick) {
            tuning.effectDuration = duration;
            tuning.tickInterval = tick;
        };

        switch (ability)
        {
        case AbilityType::FireSpread:       setGround(560.f, 45.f); tuning.effectLength = 420.f; tuning.effectWidth = 90.f; setZoneTiming(5.f, 0.45f); break;
        case AbilityType::IceSpread:        tuning.areaRadius = 210.f; break;
        case AbilityType::ElectricSpread:   setDirection(430.f, 430.f, 100.f, 430.f); tuning.areaRadius = 95.f; break;
        case AbilityType::FireBolt:         setDirection(900.f, 900.f, 90.f); tuning.areaRadius = 135.f; break;
        case AbilityType::IceBolt:          setDirection(1050.f, 1050.f, 65.f); break;
        case AbilityType::ElectricBolt:     setDirection(800.f, 800.f, 80.f); tuning.chainRange = 280.f; tuning.maxTargets = 5.f; break;
        case AbilityType::FireUltimate:     setGround(800.f, 250.f); setZoneTiming(4.8f, 0.55f); break;
        case AbilityType::IceUltimate:      setGround(720.f, 300.f); setZoneTiming(7.f, 0.55f); break;
        case AbilityType::ElectricUltimate: setDirection(850.f, 850.f, 230.f); setZoneTiming(7.f, 0.42f); break;

        // Self-centred blasts: the game reads only Area radius for these; defaults
        // mirror the live hardcoded fallbacks so the preview matches the game.
        case AbilityType::Whirlwind:       tuning.areaRadius = 170.f; break;
        case AbilityType::GroundSlam:      tuning.areaRadius = 340.f; break;
        case AbilityType::SmokeBomb:       tuning.areaRadius = 240.f; break;
        case AbilityType::DeathMark:       tuning.areaRadius = 1200.f; break;
        case AbilityType::BladeDance:      tuning.areaRadius = 230.f; break;
        case AbilityType::DivineStorm:     tuning.areaRadius = 320.f; break;
        case AbilityType::Hellfire:        tuning.areaRadius = 260.f; break;
        case AbilityType::SoulSiphon:      tuning.areaRadius = 230.f; break;
        case AbilityType::Cataclysm:       tuning.areaRadius = 1200.f; break;

        case AbilityType::WarCleave:       setDirection(210.f, 210.f, 200.f); break;
        case AbilityType::ThrowingAxe:     setDirection(520.f, 520.f, 90.f); break;
        case AbilityType::Rend:            setDirection(270.f, 150.f, 140.f, 120.f); break;
        case AbilityType::ShieldBash:      setDirection(240.f, 150.f, 150.f, 90.f); break;
        case AbilityType::Earthshatter:    setDirection(640.f, 640.f, 130.f); break;

        case AbilityType::FanOfKnives:     setDirection(360.f, 360.f, 240.f); break;
        case AbilityType::Shadowstep:      setDirection(300.f, 300.f, 110.f, 300.f); break;
        case AbilityType::PoisonVial:      setGround(500.f, 130.f); setZoneTiming(3.5f, 0.4f); break;
        case AbilityType::Backstab:        setDirection(150.f, 150.f, 120.f); break;
        case AbilityType::Eviscerate:      setDirection(210.f, 210.f, 130.f); break;
        case AbilityType::RainOfBlades:    setGround(680.f, 220.f); break;

        case AbilityType::PiercingShot:    setDirection(600.f, 600.f, 70.f); break;
        case AbilityType::Multishot:       setDirection(380.f, 380.f, 260.f); break;
        case AbilityType::FrostTrap:       setGround(500.f, 175.f); tuning.effectDuration = 15.f; break;   // armed lifetime
        case AbilityType::ExplosiveArrow:  setGround(500.f, 195.f); tuning.effectDuration = 15.f; break;   // armed lifetime
        case AbilityType::Roll:            setDirection(240.f, 240.f, 100.f, 240.f); break;
        case AbilityType::Volley:          setDirection(660.f, 660.f, 84.f); break;
        case AbilityType::ArrowStorm:      setGround(700.f, 360.f); break;
        case AbilityType::PiercingBarrage: setDirection(900.f, 900.f, 150.f); break;

        case AbilityType::Smite:           setDirection(210.f, 210.f, 200.f); break;
        case AbilityType::Consecrate:      setGround(420.f, 170.f); setZoneTiming(4.f, 0.45f); break;
        case AbilityType::HolyBolt:        setDirection(560.f, 560.f, 80.f); break;
        case AbilityType::HammerThrow:     setDirection(480.f, 480.f, 110.f); break;
        case AbilityType::HammerOfJustice: setDirection(660.f, 660.f, 150.f); break;

        case AbilityType::ShadowBolt:     setDirection(560.f, 560.f, 80.f); break;
        case AbilityType::DrainLife:      setDirection(220.f, 220.f, 130.f); break;
        case AbilityType::Curse:          setDirection(260.f, 260.f, 200.f); break;
        case AbilityType::CorruptionPool: setGround(500.f, 150.f); setZoneTiming(4.f, 0.4f); break;
        case AbilityType::ShadowNova:     setDirection(680.f, 680.f, 200.f); break;
        default: break;
        }
    }
}

// =============================================================================
void CharacterAnimator::Init()
{
    _entries.clear();
    auto add = [this](const char* name, Category category, int meleeSlot, std::function<Enemy*()> create)
    {
        _entries.push_back(CharacterEntry{ name, category, meleeSlot, std::move(create) });
    };

    // The player first — authors charactertuning_Player.txt (body + melee), which
    // the live Character applies. Melee box lives on the Swing anim (slot 2).
    add("Player", Category::Hero, 2, []() -> Enemy* { auto* p = new PlayerPreview({ 0.f, 0.f }); p->Init(); return p; });
    // meleeSlot: which animation carries a melee box (2 = the attack anim for
    // every current melee character); -1 marks ranged / contact-only characters.
    add("Grunt",          Category::Enemy, 2,  &MakeInstance<Enemy>);
    add("SkeletonArcher", Category::Enemy, -1, &MakeInstance<SkeletonArcher>);
    add("SlimeBig",       Category::Enemy, 2,  []() -> Enemy* { auto* e = new SlimeEnemy({ 0.f, 0.f }, SlimeSize::Big); e->Init(); return e; });
    add("SlimeSmall",     Category::Enemy, 2,  []() -> Enemy* { auto* e = new SlimeEnemy({ 0.f, 0.f }, SlimeSize::Small); e->Init(); return e; });
    add("FlameWisp",      Category::Enemy, -1, &MakeInstance<FlameWisp>);
    add("Sporeling",      Category::Enemy, 2,  &MakeInstance<Sporeling>);
    add("Shieldbearer",   Category::Enemy, 2,  &MakeInstance<Shieldbearer>);
    add("Phantom",        Category::Enemy, -1, &MakeInstance<Phantom>);
    add("BomberImp",      Category::Enemy, -1, &MakeInstance<BomberImp>);
    add("Warchief",       Category::Enemy, 2,  &MakeInstance<Warchief>);
    add("LivingBlade",    Category::Enemy, -1, &MakeInstance<LivingBlade>);

    add("Cyclops",        Category::Elite, -1, &MakeInstance<Cyclops>);
    add("Ogre",           Category::Elite, -1, &MakeInstance<Ogre>);
    add("Bonechill",      Category::Elite, 2,  &MakeInstance<Bonechill>);
    add("Infernal",       Category::Elite, 2,  &MakeInstance<Infernal>);
    add("Stormclub",      Category::Elite, 2,  &MakeInstance<Stormclub>);
    add("Venomfang",      Category::Elite, 2,  &MakeInstance<Venomfang>);

    add("Molarbeast",     Category::Boss, 2, &MakeInstance<Molarbeast>);
    add("AbyssSlime",     Category::Boss, 2, &MakeInstance<AbyssSlime>);
    add("PumpkinJack",    Category::Boss, 2, &MakeInstance<PumpkinJack>);
    add("Minotaur",       Category::Boss, 2, &MakeInstance<Minotaur>);
    add("Werewolf",       Category::Boss, 2, &MakeInstance<Werewolf>);
    add("ChompBug",       Category::Boss, -1, &MakeInstance<ChompBug>);   // its dive is a special move
    add("Osiris",         Category::Boss, 2, &MakeInstance<Osiris>);
    add("TitanGuard",     Category::Boss, 2, &MakeInstance<TitanGuard>);
    add("ToxicVermin",    Category::Boss, 2, &MakeInstance<ToxicVermin>);
    add("AncientBear",    Category::Boss, 2, &MakeInstance<AncientBear>);

    _screen      = Screen::Select;
    _selectedIdx = -1;
    _wantsToExit = false;
    _lineup.clear();
    _enemy.reset();
    _hero.reset(_entries[0].createInstance());
    _hero->SetEditorFacing(1.f);
    _heroBounds = Rectangle{};
    _dialog      = Dialog::None;
    _statusTimer = 0.f;
    _undo.clear();
    _redo.clear();
    ResetView();
    ScanFxCatalog();
    BuildSelectUi();
}

void CharacterAnimator::Unload()
{
    _attackEditor.Unload();
    _lineup.clear();
    _enemy.reset();
    _hero.reset();
    for (auto& entry : _fxSheets)
        if (entry.second.id != 0)
            UnloadTexture(entry.second);
    _fxSheets.clear();
    _fxPreviews.clear();
    if (_measureTarget.id != 0)
    {
        UnloadRenderTexture(_measureTarget);
        _measureTarget = RenderTexture2D{};
    }
}

void CharacterAnimator::OpenAttackLibrary()
{
    CloseLineup();
    _enemy.reset();
    _attackEditor.Init();
    _screen = Screen::AttackLibrary;
}

int CharacterAnimator::FindEntry(const std::string& name) const
{
    for (int i = 0; i < (int)_entries.size(); i++)
        if (_entries[i].displayName == name)
            return i;
    return -1;
}

bool CharacterAnimator::IsPlayer() const
{
    return _enemy && _selectedIdx >= 0 && _selectedIdx < (int)_entries.size() &&
           _entries[_selectedIdx].category == Category::Hero;
}

// =============================================================================
// Character lifetime
// =============================================================================
void CharacterAnimator::OpenCharacter(int index)
{
    if (index < 0 || index >= (int)_entries.size())
        return;

    CloseLineup();   // the lineup may still be showing the previous character
    _selectedIdx = index;

    // A real, live instance — every hitbox getter and animation sheet behaves
    // exactly like it will in the game. The factory lambda already calls Init()
    // (which loads textures and applies any saved tuning).
    _enemy.reset(_entries[index].createInstance());
    _enemy->Teleport({ 0.f, 0.f });
    _enemy->SetEditorFacing(1.f);

    // Each animation's own speed and length, so "Default speed" can restore it
    // and the list can show it without playing every sheet again.
    const int animCount = std::min(_enemy->GetEditorAnimCount(), CharacterTuning::kMaxAnims);
    for (int i = 0; i < CharacterTuning::kMaxAnims; i++)
    {
        _classFrameTime[i] = kDefaultFrameTime;
        _animFrameCount[i] = 0;
    }
    for (int i = 0; i < animCount; i++)
    {
        const float saved = _enemy->GetEditorAnimFrameTime(i);
        _enemy->SetEditorAnimFrameTime(i, 0.f);
        _enemy->PlayEditorAnim(i);
        if (_enemy->GetEditorPlaybackFrameTime() > 0.f)
            _classFrameTime[i] = _enemy->GetEditorPlaybackFrameTime();
        _animFrameCount[i] = _enemy->GetEditorFrameCount();
        _enemy->SetEditorAnimFrameTime(i, saved);
    }

    // The game sizes characters by their idle art (Balance::Visual).
    _enemy->PlayEditorAnim(0);
    _idleArtHeight = Enemy::MeasureVisibleHeight(_enemy->GetEditorSheet(), _enemy->GetEditorFrameWidth());

    _animIndex  = 0;
    _animPaused = false;
    _target     = EditTarget::Sprite;
    _dragHandle = -1;
    _rowDrag    = Control::None;
    _spriteDragActive = false;
    _scrubbing  = false;
    _panning    = false;
    PlayAnimInEditor(0);
    BuildAttackList();
    ResetView();
    _measureDirty = true;
    _fitPending = true;
    _dialog = Dialog::None;
    _screen = Screen::Edit;
    _rightTab = RightTab::Moves;
    _eventIndex = -1;
    _hitDragging = false;
    _eventDragging = false;
    _fxPreviews.clear();
    _previewSlot = _previewFrame = -1;
    _shakeTimer = 0.f;
    LoadMoves();

    _saved = CaptureSnapshot();
    ResetHistory();
}

void CharacterAnimator::CloseCharacter()
{
    CloseLineup();
    _enemy.reset();
    _undo.clear();
    _redo.clear();
    _screen = Screen::Select;
}

void CharacterAnimator::RequestOpenCharacter(int index)
{
    if (_enemy && index == _selectedIdx)
    {
        // Already open: just go back to editing it.
        CloseLineup();
        _screen = Screen::Edit;
        return;
    }
    if (_enemy && HasUnsavedChanges())
    {
        OpenDialog(Dialog::Unsaved, AfterDialog::OpenEntry, index);
        return;
    }
    OpenCharacter(index);
}

void CharacterAnimator::RequestBack()
{
    if (HasUnsavedChanges())
        OpenDialog(Dialog::Unsaved, AfterDialog::BackToSelect, -1);
    else
        CloseCharacter();
}

bool CharacterAnimator::TestOpen(const std::string& name)
{
    const int index = FindEntry(name);
    if (index < 0)
        return false;
    OpenCharacter(index);
    return _enemy != nullptr;
}

void CharacterAnimator::TestSetSizeVsHero(float multiple)
{
    ApplyScale(ScaleForSizeVsHero(multiple));
    CommitIfChanged();
}

bool CharacterAnimator::TestHeroPlaced() const
{
    if (_heroBounds.height <= 0.f || _charBounds.height <= 0.f)
        return false;
    const float heroFeet  = _heroPos.y + _heroBounds.y + _heroBounds.height;
    const float heroRight = _heroPos.x + _heroBounds.x + _heroBounds.width;
    return fabsf(heroFeet - _groundRel) < 1.f && heroRight <= _charBounds.x;
}

// =============================================================================
// Animation
// =============================================================================
void CharacterAnimator::SelectAnim(int index)
{
    if (!_enemy)
        return;
    const int count = _enemy->GetEditorAnimCount();
    if (count <= 0)
        return;
    const int previous = _animIndex;
    _animIndex = ((index % count) + count) % count;
    PlayAnimInEditor(_animIndex);
    _dragHandle = -1;
    if (_animIndex != previous)
        _eventIndex = -1;
    if (_target == EditTarget::Melee && !MeleeEditableNow())
        _target = EditTarget::Body;
}

void CharacterAnimator::PlayAnimInEditor(int index)
{
    _enemy->PlayEditorAnim(index);
    // Shared playback keeps the previous sheet's speed when this one has no
    // saved speed, so use the speed recorded for it when the character opened.
    if (index >= 0 && index < CharacterTuning::kMaxAnims && _enemy->GetEditorAnimFrameTime(index) <= 0.f)
        _enemy->SetEditorPlaybackFrameTime(_classFrameTime[index]);
}

void CharacterAnimator::ReplayCurrentAnimKeepingFrame()
{
    const int frame = _enemy->GetEditorFrame();
    PlayAnimInEditor(_animIndex);
    _enemy->SetEditorFrame(frame);
}

void CharacterAnimator::StepFrame(int delta)
{
    if (!_enemy)
        return;
    _animPaused = true;
    _enemy->SetEditorFrame(_enemy->GetEditorFrame() + delta);
}

float CharacterAnimator::CurrentFrameTime() const
{
    const float saved = _enemy->GetEditorAnimFrameTime(_animIndex);
    return saved > 0.f ? saved : _enemy->GetEditorPlaybackFrameTime();
}

void CharacterAnimator::SetCurrentFrameTime(float frameTime)
{
    frameTime = std::clamp(frameTime, kMinFrameTime, kMaxFrameTime);
    _enemy->SetEditorAnimFrameTime(_animIndex, frameTime);
    _enemy->SetEditorPlaybackFrameTime(frameTime);
}

// =============================================================================
// Size. Shown and set as a multiple of the hero's height, the same measure the
// game uses to size standard enemies (Balance::Visual).
// =============================================================================
float CharacterAnimator::SizeVsHero() const
{
    if (!_enemy || _idleArtHeight <= 0.f)
        return 0.f;
    return _idleArtHeight * _enemy->GetDrawScale() / Balance::Visual::kPlayerVisibleHeight;
}

float CharacterAnimator::ScaleForSizeVsHero(float multiple) const
{
    if (_idleArtHeight <= 0.f)
        return _enemy ? _enemy->GetDrawScale() : 1.f;
    multiple = std::clamp(multiple, kMinSizeVsHero, kMaxSizeVsHero);
    return multiple * Balance::Visual::kPlayerVisibleHeight / _idleArtHeight;
}

CharacterTuning CharacterAnimator::ScaledTuning(const CharacterTuning& tuning, float factor)
{
    // Hitboxes and offsets are world px, so they grow with the sprite.
    CharacterTuning out = tuning;
    out.scale *= factor;
    out.collisionRel = Rectangle{ tuning.collisionRel.x * factor, tuning.collisionRel.y * factor,
                                  tuning.collisionRel.width * factor, tuning.collisionRel.height * factor };
    out.capsuleRadius     *= factor;
    out.capsuleHalfHeight *= factor;
    out.capsuleOffset      = Vector2Scale(tuning.capsuleOffset, factor);
    out.attackBoxWidth    *= factor;
    out.attackBoxHeight   *= factor;
    out.attackBoxOffsetX  *= factor;
    out.attackBoxOffsetY  *= factor;
    for (int i = 0; i < CharacterTuning::kMaxAnims; i++)
    {
        out.animBody[i].x      *= factor;
        out.animBody[i].y      *= factor;
        out.animBody[i].radius *= factor;
        const Rectangle m = tuning.animMelee[i].rect;
        out.animMelee[i].rect = Rectangle{ m.x * factor, m.y * factor, m.width * factor, m.height * factor };
        out.animDraw[i].x *= factor;
        out.animDraw[i].y *= factor;
        for (CharacterTuning::AnimEvent& event : out.animEvents[i])
        {
            event.x *= factor;
            event.y *= factor;
        }
    }
    out.hurtPadX *= factor;
    out.hurtPadY *= factor;
    return out;
}

void CharacterAnimator::ApplyScale(float newScale)
{
    if (!_enemy || IsPlayer())
        return;
    const CharacterTuning before = CaptureCharacter();
    if (before.scale <= 0.f)
        return;
    _enemy->SetDrawScale(newScale);
    const float applied = _enemy->GetDrawScale();   // the enemy keeps a minimum scale
    if (fabsf(applied - before.scale) < kScaleEpsilon)
        return;
    RestoreCharacter(ScaledTuning(before, applied / before.scale));
}

// =============================================================================
// Saving, undo and unsaved changes. A snapshot is the character's tuning (and
// the player's attack tuning); their saved text decides "changed".
// =============================================================================
CharacterTuning CharacterAnimator::CaptureCharacter() const
{
    CharacterTuning tuning;
    if (!_enemy)
        return tuning;

    tuning.hasScale = true;
    tuning.scale    = _enemy->GetDrawScale();
    // Store the size only when it differs from what the game picks by itself,
    // so automatic sizing stays automatic until the Size control is used.
    tuning.sizeSet  = !IsPlayer() && fabsf(tuning.scale - _enemy->GetAutoScale()) > kScaleEpsilon;

    const int animCount = std::min(_enemy->GetEditorAnimCount(), CharacterTuning::kMaxAnims);
    for (int i = 0; i < animCount; i++)
    {
        tuning.animFrameTime[i] = _enemy->GetEditorAnimFrameTime(i);

        if (_enemy->GetAnimBodySet(i))
        {
            tuning.animBody[i].set    = true;
            tuning.animBody[i].x      = _enemy->GetAnimBodyOffset(i).x;
            tuning.animBody[i].y      = _enemy->GetAnimBodyOffset(i).y;
            tuning.animBody[i].radius = _enemy->GetAnimBodyRadius(i);
        }
        if (_enemy->GetAnimMeleeSet(i))
        {
            tuning.animMelee[i].set  = true;
            tuning.animMelee[i].rect = _enemy->GetAnimMeleeRelRect(i);
        }
        if (_enemy->GetAnimDrawSet(i))
        {
            tuning.animDraw[i].set = true;
            tuning.animDraw[i].x   = _enemy->GetAnimDrawOffsetValue(i).x;
            tuning.animDraw[i].y   = _enemy->GetAnimDrawOffsetValue(i).y;
        }
        if (_enemy->GetAnimHitSet(i))
            tuning.animHit[i] = CharacterTuning::AnimHit{ true, _enemy->GetAnimHitStart(i), _enemy->GetAnimHitEnd(i) };
        tuning.animEvents[i] = _enemy->GetAnimEvents(i);
    }
    if (_enemy->GetHurtPadSet())
    {
        tuning.hasHurtPad = true;
        tuning.hurtPadX = _enemy->GetHurtPadX();
        tuning.hurtPadY = _enemy->GetHurtPadY();
    }
    return tuning;
}

CharacterAnimator::Snapshot CharacterAnimator::CaptureSnapshot() const
{
    Snapshot snapshot;
    snapshot.character     = CaptureCharacter();
    snapshot.characterText = CharacterTuningStore::Serialize(snapshot.character);
    snapshot.moves         = MoveTunings();
    snapshot.movesText     = MovesText(snapshot.moves);
    if (IsPlayer())
    {
        snapshot.attack     = _attackTuning;
        snapshot.attackText = AttackTuningStore::Serialize(_attackTuning);
    }
    return snapshot;
}

void CharacterAnimator::RestoreCharacter(const CharacterTuning& tuning)
{
    _enemy->SetDrawScale(tuning.scale);
    const int animCount = std::min(_enemy->GetEditorAnimCount(), CharacterTuning::kMaxAnims);
    for (int i = 0; i < animCount; i++)
    {
        _enemy->SetEditorAnimFrameTime(i, tuning.animFrameTime[i]);

        const CharacterTuning::AnimBody& body = tuning.animBody[i];
        if (body.set) _enemy->SetAnimBody(i, Vector2{ body.x, body.y }, body.radius);
        else          _enemy->ClearAnimBody(i);

        if (tuning.animMelee[i].set) _enemy->SetAnimMelee(i, tuning.animMelee[i].rect);
        else                         _enemy->ClearAnimMelee(i);

        const CharacterTuning::AnimDraw& draw = tuning.animDraw[i];
        if (draw.set) _enemy->SetAnimDrawOffset(i, Vector2{ draw.x, draw.y });
        else          _enemy->ClearAnimDrawOffset(i);

        const CharacterTuning::AnimHit& hit = tuning.animHit[i];
        if (hit.set) _enemy->SetAnimHit(i, hit.start, hit.end);
        else         _enemy->ClearAnimHit(i);
        _enemy->SetAnimEvents(i, tuning.animEvents[i]);
    }
    if (tuning.hasHurtPad) _enemy->SetHurtPad(tuning.hurtPadX, tuning.hurtPadY);
    else                   _enemy->ClearHurtPad();
    if (_eventIndex >= (int)_enemy->GetAnimEvents(_animIndex).size())
        _eventIndex = -1;
    // The playing sheet picks up its (possibly restored) speed without restarting.
    const float saved = tuning.animFrameTime[_animIndex];
    _enemy->SetEditorPlaybackFrameTime(saved > 0.f ? saved : _classFrameTime[_animIndex]);
    _measureDirty = true;
}

void CharacterAnimator::RestoreSnapshot(const Snapshot& snapshot)
{
    RestoreCharacter(snapshot.character);
    if (IsPlayer())
        _attackTuning = snapshot.attack;
    for (size_t i = 0; i < _moves.size() && i < snapshot.moves.size(); i++)
        _moves[i].tuning = snapshot.moves[i];
}

void CharacterAnimator::ResetHistory()
{
    _undo.clear();
    _redo.clear();
    _committed = CaptureSnapshot();
}

void CharacterAnimator::CommitIfChanged()
{
    // A drag is one edit: it's recorded when the mouse lets go.
    if (!_enemy || IsGestureActive())
        return;
    Snapshot now = CaptureSnapshot();
    if (now.characterText == _committed.characterText && now.attackText == _committed.attackText &&
        now.movesText == _committed.movesText)
        return;
    _undo.push_back(std::move(_committed));
    if (_undo.size() > kMaxUndo)
        _undo.erase(_undo.begin());
    _redo.clear();
    _committed = std::move(now);
    _measureDirty = true;
}

void CharacterAnimator::Undo()
{
    if (!_enemy || IsGestureActive())
        return;
    if (_undo.empty())
    {
        SetStatus("Nothing to undo");
        return;
    }
    _redo.push_back(_committed);
    _committed = _undo.back();
    _undo.pop_back();
    RestoreSnapshot(_committed);
    SetStatus("Undo");
}

void CharacterAnimator::Redo()
{
    if (!_enemy || IsGestureActive())
        return;
    if (_redo.empty())
    {
        SetStatus("Nothing to redo");
        return;
    }
    _undo.push_back(_committed);
    _committed = _redo.back();
    _redo.pop_back();
    RestoreSnapshot(_committed);
    SetStatus("Redo");
}

bool CharacterAnimator::CharacterDirty() const
{
    return _enemy && CharacterTuningStore::Serialize(CaptureCharacter()) != _saved.characterText;
}

bool CharacterAnimator::AttackDirty() const
{
    return IsPlayer() && AttackTuningStore::Serialize(_attackTuning) != _saved.attackText;
}

void CharacterAnimator::SaveAll()
{
    if (!_enemy)
        return;
    const bool character = CharacterDirty();
    const bool attack    = AttackDirty();
    const bool moves     = MovesDirty();
    if (!character && !attack && !moves)
    {
        SetStatus("Nothing to save");
        return;
    }

    std::string saved;
    if (character)
    {
        CharacterTuningStore::Save(_entries[_selectedIdx].displayName, CaptureCharacter());
        saved = _entries[_selectedIdx].displayName;
    }
    if (attack)
    {
        AttackTuningStore::Save(CurrentAttackKey(), _attackTuning);
        saved += (saved.empty() ? "" : " + ") + CurrentAttackKey();
    }
    for (size_t i = 0; i < _moves.size() && i < _saved.moves.size(); i++)
    {
        if (AttackTuningStore::Serialize(_moves[i].tuning) == AttackTuningStore::Serialize(_saved.moves[i]))
            continue;
        AttackTuningStore::Save(_moves[i].info.key, _moves[i].tuning);
        saved += (saved.empty() ? "" : " + ") + std::string(_moves[i].info.name);
    }
    _saved = CaptureSnapshot();
    SetStatus("Saved " + saved);
}

void CharacterAnimator::RevertToSaved()
{
    if (!HasUnsavedChanges())
    {
        SetStatus("Nothing to revert");
        return;
    }
    RestoreSnapshot(_saved);
    SetStatus("Back to the last save (Ctrl+Z brings the edits back)");
}

void CharacterAnimator::ResetToCodeDefaults()
{
    if (_selectedIdx < 0)
        return;
    CharacterTuningStore::Delete(_entries[_selectedIdx].displayName);
    // Respawn the preview so it snaps back to code defaults.
    OpenCharacter(_selectedIdx);
    SetStatus("Tuning file deleted - back to code defaults");
}

void CharacterAnimator::CopyBodyAndSpriteToAll()
{
    // Copies this animation's body circle + draw offset to every animation —
    // fastest way to author a character whose body barely moves between anims.
    Vector2 bodyOffset;
    float bodyRadius;
    GetEffectiveBodyCircle(bodyOffset, bodyRadius);
    const Vector2 drawOffset = _enemy->GetAnimDrawOffsetValue(_animIndex);
    const bool hasDrawOffset = _enemy->GetAnimDrawSet(_animIndex);

    const int animCount = _enemy->GetEditorAnimCount();
    for (int i = 0; i < animCount; i++)
    {
        _enemy->SetAnimBody(i, bodyOffset, bodyRadius);
        if (hasDrawOffset)
            _enemy->SetAnimDrawOffset(i, drawOffset);
    }
    SetStatus("Copied the body circle and sprite offset to every animation");
}

void CharacterAnimator::SetStatus(const std::string& text)
{
    _statusText  = text;
    _statusTimer = kStatusSeconds;
}

// =============================================================================
// Dialog: unsaved changes (Save / Don't save / Cancel) and the reset confirm.
// =============================================================================
void CharacterAnimator::OpenDialog(Dialog dialog, AfterDialog after, int param)
{
    _dialog     = dialog;
    _after      = after;
    _afterParam = param;
    _dragHandle = -1;
    _rowDrag    = Control::None;
    _spriteDragActive = false;
    _scrubbing  = false;
    _panning    = false;
    BuildDialogUi();
}

void CharacterAnimator::CloseDialog()
{
    _dialog = Dialog::None;
    _dialogUi.clear();
}

void CharacterAnimator::AnswerDialog(DialogAnswer answer)
{
    if (_dialog == Dialog::None)
        return;
    const Dialog dialog = _dialog;
    CloseDialog();
    if (answer == DialogAnswer::Cancel)
        return;

    if (dialog == Dialog::ConfirmReset)
    {
        ResetToCodeDefaults();
        return;
    }
    if (answer == DialogAnswer::Save)
        SaveAll();
    RunAfterDialog();
}

void CharacterAnimator::RunAfterDialog()
{
    const AfterDialog after = _after;
    _after = AfterDialog::None;
    switch (after)
    {
    case AfterDialog::BackToSelect: CloseCharacter(); break;
    case AfterDialog::OpenEntry:    OpenCharacter(_afterParam); break;
    case AfterDialog::SwitchAttack: SelectAttack(_afterParam); ResetHistory(); break;
    default: break;
    }
}

void CharacterAnimator::BuildDialogUi()
{
    _dialogUi.clear();
    const Rectangle box{ (kVirtualWidth - kDialogWidth) * 0.5f, (kVirtualHeight - kDialogHeight) * 0.5f,
                         kDialogWidth, kDialogHeight };
    const float buttonY = box.y + box.height - kPad - kRowHeight;

    auto addButton = [this](Control id, float x, float y, const char* label, const char* tooltip, bool primary)
    {
        UiItem item;
        item.id = id;
        item.kind = UiKind::Button;
        item.rect = Rectangle{ x, y, kDialogButtonW, kRowHeight };
        item.label = label;
        item.tooltip = tooltip;
        item.active = primary;
        item.accent = kGoodColor;
        _dialogUi.push_back(item);
    };

    const float right = box.x + box.width - kPad;
    if (_dialog == Dialog::ConfirmReset)
    {
        addButton(Control::DialogCancel, right - kDialogButtonW, buttonY, "Cancel", "Keep everything (Esc).", false);
        addButton(Control::DialogSave, right - kDialogButtonW * 2.f - kGap, buttonY, "Delete file", "Delete the tuning file and go back to code defaults (Enter).", true);
    }
    else
    {
        addButton(Control::DialogCancel, right - kDialogButtonW, buttonY, "Cancel", "Stay here (Esc).", false);
        addButton(Control::DialogDiscard, right - kDialogButtonW * 2.f - kGap, buttonY, "Don't save", "Throw the changes away (N).", false);
        addButton(Control::DialogSave, right - kDialogButtonW * 3.f - kGap * 2.f, buttonY, "Save", "Save, then continue (Enter).", true);
    }
}

void CharacterAnimator::DrawDialog()
{
    DrawRectangle(0, 0, kVirtualWidth, kVirtualHeight, Fade(BLACK, 0.6f));
    const Rectangle box{ (kVirtualWidth - kDialogWidth) * 0.5f, (kVirtualHeight - kDialogHeight) * 0.5f,
                         kDialogWidth, kDialogHeight };
    DrawRectangleRounded(box, kRoundness * 0.3f, kSegments, kPanelFill);
    DrawRectangleRoundedLines(box, kRoundness * 0.3f, kSegments, kWarnColor);

    std::string title;
    std::string message;
    if (_dialog == Dialog::ConfirmReset)
    {
        title   = "Reset to code defaults?";
        message = "This deletes charactertuning_" + _entries[_selectedIdx].displayName +
                  ".txt. It can't be undone.";
    }
    else
    {
        title = "Unsaved changes";
        const std::string what = (_after == AfterDialog::SwitchAttack) ? std::string(CurrentAttackLabel())
                                                                       : _entries[_selectedIdx].displayName;
        message = what + " has changes that aren't saved yet.";
    }
    DrawText(title.c_str(), (int)(box.x + kPad), (int)(box.y + kPad), kTitleFont, RAYWHITE);
    float y = box.y + kPad + kTitleFont + kGap * 2.f;
    for (const std::string& line : WrapText(message, kRowFont, box.width - kPad * 2.f))
    {
        DrawText(line.c_str(), (int)(box.x + kPad), (int)y, kRowFont, kTextDim);
        y += kRowFont + kGap;
    }
    DrawUi(_dialogUi);
}

// =============================================================================
// Player attack
// =============================================================================
void CharacterAnimator::BuildAttackList()
{
    _attackOptions.clear();
    const char* classNames[] = { "Mage", "Warrior", "Hunter", "Rogue", "Paladin", "Warlock" };
    for (int i = 0; i < (int)PlayerClass::Count; ++i)
        _attackOptions.push_back(AttackOption{ std::string(classNames[i]) + " Basic", AttackTuningKeyForBasic(i), true, AbilityType::None });
    for (AbilityType ability : kAllAbilities)
    {
        if (!AbilityHasTunableAttack(ability))
            continue;
        std::string label = std::string(AttackOwnerForAbility((int)ability)) + " " + GetAbilityName(ability);
        _attackOptions.push_back(AttackOption{ label, AttackTuningKeyForAbility(ability), false, ability });
    }
    SelectAttack(0);
}

void CharacterAnimator::SelectAttack(int index)
{
    if (_attackOptions.empty()) return;
    _attackIndex = (index + (int)_attackOptions.size()) % (int)_attackOptions.size();
    LoadCurrentAttackTuning();
    _attackPreviewActive = false;
    _attackPreviewTimer = 0.f;
    _attackPreviewNextShot = 0.f;
    _attackPreviewShots.clear();
}

void CharacterAnimator::RequestSelectAttack(int index)
{
    if (AttackDirty())
    {
        OpenDialog(Dialog::Unsaved, AfterDialog::SwitchAttack, index);
        return;
    }
    SelectAttack(index);
    ResetHistory();
}

std::string CharacterAnimator::CurrentAttackKey() const
{
    if (_attackIndex < 0 || _attackIndex >= (int)_attackOptions.size()) return "Mage_Basic";
    return _attackOptions[_attackIndex].key;
}

const char* CharacterAnimator::CurrentAttackLabel() const
{
    if (_attackIndex < 0 || _attackIndex >= (int)_attackOptions.size()) return "Mage Basic";
    return _attackOptions[_attackIndex].label.c_str();
}

void CharacterAnimator::LoadCurrentAttackTuning()
{
    _attackTuning = AttackTuning{};
    _attackTuning.hasFirePoint = true;
    _attackTuning.fireForward = 50.f;
    _attackTuning.fireHeight = 0.f;
    _attackTuning.hasProjectile = true;
    _attackTuning.projScale = 1.f;
    _attackTuning.projRadius = (_attackIndex >= 0 && _attackIndex < (int)_attackOptions.size() && _attackOptions[_attackIndex].basic) ? 26.f : 56.f;
    _attackTuning.projSpeed = 650.f;
    _attackTuning.projLifetime = (_attackIndex >= 0 && _attackIndex < (int)_attackOptions.size() && _attackOptions[_attackIndex].basic) ? 1.4f : 5.f;
    _attackTuning.hasCooldown = true;
    _attackTuning.cooldown = 0.f;
    _attackTuning.hasAbility = true;
    _attackTuning.aimRange = 600.f;
    _attackTuning.areaRadius = 150.f;
    _attackTuning.effectLength = 400.f;
    _attackTuning.effectWidth = 90.f;
    _attackTuning.effectDuration = 4.f;
    _attackTuning.tickInterval = 0.5f;
    _attackTuning.chainRange = 280.f;
    _attackTuning.maxTargets = 4.f;
    _attackTuning.moveDistance = 400.f;
    _attackTuning.previewAngle = 0.f;

    if (_attackIndex >= 0 && _attackIndex < (int)_attackOptions.size())
        ApplyAbilityLabDefaults(_attackOptions[_attackIndex].ability, _attackTuning);

    if (const AttackTuning* saved = AttackTuningStore::Get(CurrentAttackKey()))
        _attackTuning = *saved;

    _attackTuning.hasFirePoint = true;
    _attackTuning.hasProjectile = true;
    _attackTuning.hasCooldown = true;
    _attackTuning.hasAbility = true;
    if (_attackTuning.projScale <= 0.f) _attackTuning.projScale = 1.f;
    if (_attackTuning.projRadius <= 0.f) _attackTuning.projRadius = 26.f;
    if (_attackTuning.projSpeed <= 0.f) _attackTuning.projSpeed = 650.f;
    if (_attackTuning.projLifetime <= 0.f) _attackTuning.projLifetime = 1.4f;

    // What just loaded is the "saved" attack the unsaved check compares against.
    _saved.attack     = _attackTuning;
    _saved.attackText = AttackTuningStore::Serialize(_attackTuning);
}

void CharacterAnimator::UpdateAttackPreview(float dt)
{
    if (!_attackPreviewActive)
        return;
    _attackPreviewTimer += dt;
    for (float& age : _attackPreviewShots)
        age += dt;

    const float lifetime = std::max(kMinCadence, _attackTuning.projLifetime);
    _attackPreviewShots.erase(std::remove_if(_attackPreviewShots.begin(), _attackPreviewShots.end(),
        [lifetime](float age) { return age > lifetime; }), _attackPreviewShots.end());

    const float cadence = std::max(kMinCadence, _attackTuning.cooldown);
    while (_attackPreviewTimer >= _attackPreviewNextShot)
    {
        _attackPreviewShots.push_back(0.f);
        _attackPreviewNextShot += cadence;
    }
}

// =============================================================================
// Update
// =============================================================================
void CharacterAnimator::Update()
{
    const float dt = GetFrameTime();
    if (_statusTimer > 0.f)
        _statusTimer -= dt;

    if (_screen == Screen::AttackLibrary)
    {
        _attackEditor.Update();
        if (_attackEditor.WantsToExit())
        {
            _attackEditor.Unload();
            _screen = Screen::Select;
        }
        return;
    }

    const Vector2 mouse = GetVirtualMousePos();
    if (_dialog != Dialog::None)
    {
        BuildDialogUi();
        UpdateTooltip(_dialogUi, mouse, dt);
        if (IsKeyPressed(KEY_ESCAPE))                                   AnswerDialog(DialogAnswer::Cancel);
        else if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) AnswerDialog(DialogAnswer::Save);
        else if (IsKeyPressed(KEY_N) && _dialog == Dialog::Unsaved)     AnswerDialog(DialogAnswer::Discard);
        else if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        {
            if (const UiItem* hit = HitUi(_dialogUi, mouse))
            {
                const UiItem item = *hit;
                ClickControl(item);
            }
        }
        if (_screen == Screen::Edit)
            BuildEditUi();
        _lastMouse = mouse;
        return;
    }

    if (_screen == Screen::Select)
        UpdateSelect();
    else if (_screen == Screen::Lineup)
        UpdateLineup(dt);
    else
        UpdateEdit(dt);
    _lastMouse = GetVirtualMousePos();
}

void CharacterAnimator::UpdateSelect()
{
    if (IsKeyPressed(KEY_ESCAPE))
    {
        _wantsToExit = true;
        return;
    }
    const Vector2 mouse = GetVirtualMousePos();
    BuildSelectUi();
    UpdateTooltip(_ui, mouse, GetFrameTime());
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        if (const UiItem* hit = HitUi(_ui, mouse))
        {
            const UiItem item = *hit;
            ClickControl(item);
        }
}

void CharacterAnimator::UpdateEdit(float dt)
{
    if (!_enemy)
    {
        _screen = Screen::Select;
        return;
    }
    if (_measureDirty)
        MeasureCharacter();

    const Vector2 mouse = GetVirtualMousePos();
    BuildEditUi();
    UpdateTooltip(_ui, mouse, dt);

    HandleEditKeys();
    if (_screen != Screen::Edit || _dialog != Dialog::None || !_enemy)
        return;

    // A drag that started on the stage keeps the mouse even over a panel.
    const bool stageGesture = _dragHandle != -1 || _spriteDragActive || _panning;
    if (stageGesture || !HandleUiMouse(mouse))
        HandleStageMouse(mouse);
    if (_screen != Screen::Edit || !_enemy)
        return;

    if (!_animPaused)
        _enemy->TickEditorAnimation(dt);
    if (_hero)
        _hero->TickEditorAnimation(dt);
    UpdateAttackPreview(dt);
    UpdateFxPreviews(dt);

    CommitIfChanged();
    if (_measureDirty)
        MeasureCharacter();
    if (_fitPending)
    {
        FitView();
        _fitPending = false;
    }
    BuildEditUi();   // labels for Draw reflect this frame's edits
}

void CharacterAnimator::HandleEditKeys()
{
    if (IsKeyPressed(KEY_ESCAPE))
    {
        RequestBack();
        return;
    }
    if (IsGestureActive())
        return;   // mid-drag: shortcuts wait

    const bool shift = ShiftDown();
    if (CtrlDown())
    {
        if (IsKeyPressed(KEY_Z)) { if (shift) Redo(); else Undo(); }
        if (IsKeyPressed(KEY_Y)) Redo();
        if (IsKeyPressed(KEY_S)) SaveAll();
        return;
    }

    const bool player = IsPlayer();
    if (IsKeyPressed(KEY_Q)) SelectAnim(_animIndex - 1);
    if (IsKeyPressed(KEY_E)) SelectAnim(_animIndex + 1);
    if (IsKeyPressed(KEY_SPACE))
    {
        if (_target == EditTarget::Attack && player)
        {
            _attackPreviewActive = !_attackPreviewActive;
            _attackPreviewTimer = 0.f;
            _attackPreviewNextShot = 0.f;
            _attackPreviewShots.clear();
        }
        else
            _animPaused = !_animPaused;
    }
    if (IsKeyPressed(KEY_COMMA)  || IsKeyPressedRepeat(KEY_COMMA))  StepFrame(-1);
    if (IsKeyPressed(KEY_PERIOD) || IsKeyPressedRepeat(KEY_PERIOD)) StepFrame(1);
    if (IsKeyPressed(KEY_F))
    {
        _enemy->SetEditorFacing(_enemy->GetEditorFacing() >= 0.f ? -1.f : 1.f);
        _measureDirty = true;
    }

    // TAB cycles Sprite / Body / Melee (on the melee animation) / Attack (player).
    if (IsKeyPressed(KEY_TAB))
    {
        if (_target == EditTarget::Sprite)
            _target = EditTarget::Body;
        else if (_target == EditTarget::Body && MeleeEditableNow())
            _target = EditTarget::Melee;
        else if (_target != EditTarget::Attack && player)
            _target = EditTarget::Attack;
        else
            _target = EditTarget::Sprite;
        _dragHandle = -1;
    }

    if (IsKeyPressed(KEY_C))      CopyBodyAndSpriteToAll();
    if (IsKeyPressed(KEY_S))      SaveAll();
    if (IsKeyPressed(KEY_DELETE))
    {
        if (_rightTab == RightTab::Event && _eventIndex >= 0)
            DeleteEvent(_eventIndex);
        else if (CharacterTuningStore::Get(_entries[_selectedIdx].displayName) != nullptr)
            OpenDialog(Dialog::ConfirmReset, AfterDialog::None, -1);
    }
    if (IsKeyPressed(KEY_H) && !player) _showHero = !_showHero;
    if (IsKeyPressed(KEY_G))      _snap = !_snap;
    if (IsKeyPressed(KEY_HOME))   FitView();

    if (player)
    {
        if (IsKeyPressed(KEY_A))
        {
            _target = EditTarget::Attack;
            _dragHandle = -1;
        }
        if (IsKeyPressed(KEY_LEFT_BRACKET))  RequestSelectAttack(_attackIndex - 1);
        if (IsKeyPressed(KEY_RIGHT_BRACKET)) RequestSelectAttack(_attackIndex + 1);
    }

    // Arrow keys nudge whatever the current mode edits.
    const float step = shift ? kFineStep : kSnapStep;
    auto pressed = [](int key) { return IsKeyPressed(key) || IsKeyPressedRepeat(key); };
    if (pressed(KEY_LEFT))  Nudge(Vector2{ -step, 0.f });
    if (pressed(KEY_RIGHT)) Nudge(Vector2{ step, 0.f });
    if (pressed(KEY_UP))    Nudge(Vector2{ 0.f, -step });
    if (pressed(KEY_DOWN))  Nudge(Vector2{ 0.f, step });
}

void CharacterAnimator::Nudge(Vector2 delta)
{
    // Arrow keys move on screen; hitboxes are authored facing right.
    const float facing = (_enemy->GetEditorFacing() >= 0.f) ? 1.f : -1.f;
    const Vector2 authored{ delta.x * facing, delta.y };
    switch (_target)
    {
    case EditTarget::Sprite:
    {
        const Vector2 offset = _enemy->GetAnimDrawOffsetValue(_animIndex);
        _enemy->SetAnimDrawOffset(_animIndex, Vector2Add(offset, authored));
        break;
    }
    case EditTarget::Body:
    {
        Vector2 bodyOffset;
        float bodyRadius;
        GetEffectiveBodyCircle(bodyOffset, bodyRadius);
        _enemy->SetAnimBody(_animIndex, Vector2Add(bodyOffset, authored), bodyRadius);
        break;
    }
    case EditTarget::Melee:
    {
        if (!MeleeEditableNow())
            break;
        Rectangle rect = GetEffectiveMeleeRel();
        rect.x += authored.x;
        rect.y += authored.y;
        _enemy->SetAnimMelee(_animIndex, rect);
        break;
    }
    case EditTarget::Attack:
        if (!IsPlayer())
            break;
        _attackTuning.fireForward += delta.x;
        _attackTuning.fireHeight  += delta.y;
        _attackTuning.hasFirePoint = true;
        break;
    case EditTarget::Move:
    {
        // Boxes move; a circle placed ahead of the character slides along.
        if (_moveIndex < 0 || _moveIndex >= (int)_moves.size())
            break;
        MoveEdit& move = _moves[_moveIndex];
        MakeMoveCustom(move);
        if (move.info.shape == Enemy::MoveShape::Box)
        {
            move.tuning.hitX += authored.x;
            move.tuning.hitY += authored.y;
        }
        else if (move.info.shape == Enemy::MoveShape::Circle && move.info.forward != 0.f)
            move.tuning.hitForward += authored.x;
        break;
    }
    case EditTarget::Event:
    {
        std::vector<CharacterTuning::AnimEvent> events = CurrentEvents();
        if (_eventIndex < 0 || _eventIndex >= (int)events.size())
            break;
        events[_eventIndex].x += authored.x;
        events[_eventIndex].y += authored.y;
        SetCurrentEvents(events);
        break;
    }
    }
}

bool CharacterAnimator::IsGestureActive() const
{
    return _dragHandle != -1 || _rowDrag != Control::None || _spriteDragActive || _scrubbing ||
           _hitDragging || _eventDragging;
}

// Returns true when the mouse belongs to the UI this frame (a control, a value
// drag, a scrub, or just a panel), so the stage underneath must ignore it.
bool CharacterAnimator::HandleUiMouse(Vector2 mouse)
{
    if (_rowDrag != Control::None)
    {
        if (!IsMouseButtonDown(MOUSE_LEFT_BUTTON))
            _rowDrag = Control::None;
        else
            UpdateValueDrag(mouse);
        return true;
    }
    if (_scrubbing)
    {
        if (!IsMouseButtonDown(MOUSE_LEFT_BUTTON))
        {
            _scrubbing = false;
            return true;
        }
        // Follow the cursor across the strip; only x matters, so a loose
        // vertical hand still scrubs.
        for (const UiItem& item : _ui)
            if (item.id == Control::FrameCell && mouse.x >= item.rect.x && mouse.x < item.rect.x + item.rect.width)
                _enemy->SetEditorFrame(item.index);
        return true;
    }

    // A hit-window drag paints the range across the lane; an event drag moves
    // the selected event to the frame under the cursor. Only x matters.
    if (_hitDragging || _eventDragging)
    {
        if (!IsMouseButtonDown(MOUSE_LEFT_BUTTON))
        {
            _hitDragging = _eventDragging = false;
            return true;
        }
        for (const UiItem& item : _ui)
        {
            if (item.id != Control::HitCell || mouse.x < item.rect.x || mouse.x >= item.rect.x + item.rect.width)
                continue;
            if (_hitDragging)
                _enemy->SetAnimHit(_animIndex, std::min(_hitAnchor, item.index), std::max(_hitAnchor, item.index));
            else
            {
                std::vector<CharacterTuning::AnimEvent> events = CurrentEvents();
                if (_eventIndex >= 0 && _eventIndex < (int)events.size() && events[_eventIndex].frame != item.index)
                {
                    events[_eventIndex].frame = item.index;
                    SetCurrentEvents(events);
                }
            }
            _enemy->SetEditorFrame(item.index);
        }
        return true;
    }

    const UiItem* hit = HitUi(_ui, mouse);
    const float wheel = GetMouseWheelMove();
    if (hit != nullptr && hit->enabled && wheel != 0.f && hit->kind == UiKind::ValueRow)
    {
        const UiItem item = *hit;
        StepValue(item, wheel);
        return true;
    }
    if (hit != nullptr && hit->enabled && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
    {
        const UiItem item = *hit;
        if (item.kind == UiKind::ValueRow)
            BeginValueDrag(item, mouse);
        else if (item.kind == UiKind::FrameCell)
        {
            _scrubbing  = true;
            _animPaused = true;
            _enemy->SetEditorFrame(item.index);
        }
        else if (item.kind == UiKind::HitCell)
        {
            _hitDragging = true;
            _hitAnchor   = item.index;
            _animPaused  = true;
            _enemy->SetEditorFrame(item.index);
            _enemy->SetAnimHit(_animIndex, item.index, item.index);
            _target = EditTarget::Melee;
        }
        else if (item.kind == UiKind::EventCell)
        {
            _animPaused = true;
            _enemy->SetEditorFrame(item.index);
            const std::vector<CharacterTuning::AnimEvent> events = CurrentEvents();
            // Click a frame with events: select the next one there (and drag it);
            // an empty frame gets a new FX event.
            int pick = -1;
            for (int i = 0; i < (int)events.size(); i++)
                if (events[i].frame == item.index && (pick < 0 || i > _eventIndex))
                {
                    pick = i;
                    if (i > _eventIndex) break;
                }
            if (pick >= 0)
            {
                SelectEvent(pick);
                _eventDragging = true;
            }
            else
                AddEvent(CharacterTuning::EventKind::Fx, item.index);
        }
        else
            ClickControl(item);
        return true;
    }
    if (hit != nullptr && hit->enabled && IsMouseButtonPressed(MOUSE_RIGHT_BUTTON))
    {
        // Right-click: a hit lane goes back to the code's frames; an event
        // frame loses its selected (or first) event.
        if (hit->kind == UiKind::HitCell)
        {
            _enemy->ClearAnimHit(_animIndex);
            SetStatus("Back to the code's hit frames");
            return true;
        }
        if (hit->kind == UiKind::EventCell)
        {
            const std::vector<CharacterTuning::AnimEvent> events = CurrentEvents();
            int victim = -1;
            for (int i = 0; i < (int)events.size(); i++)
                if (events[i].frame == hit->index && (victim < 0 || i == _eventIndex))
                    victim = i;
            if (victim >= 0)
                DeleteEvent(victim);
            return true;
        }
    }
    // Panels swallow everything so nothing behind them gets grabbed.
    return hit != nullptr || !CheckCollisionPointRec(mouse, StageRect());
}

void CharacterAnimator::HandleStageMouse(Vector2 mouse)
{
    const bool overStage = CheckCollisionPointRec(mouse, StageRect());

    // Wheel zooms at the cursor; middle-drag pans.
    const float wheel = GetMouseWheelMove();
    if (wheel != 0.f && overStage)
        ZoomAt(mouse, _zoom * powf(kZoomStep, wheel));
    if (IsMouseButtonPressed(MOUSE_MIDDLE_BUTTON) && overStage)
        _panning = true;
    if (_panning)
    {
        if (!IsMouseButtonDown(MOUSE_MIDDLE_BUTTON))
            _panning = false;
        else
            _pan = Vector2Add(_pan, Vector2Subtract(mouse, _lastMouse));
    }

    // Right-drag: per-animation sprite draw offset.
    if (IsMouseButtonPressed(MOUSE_RIGHT_BUTTON) && overStage)
    {
        _spriteDragActive      = true;
        _spriteDragStartMouse  = mouse;
        _spriteDragStartOffset = _enemy->GetAnimDrawOffsetValue(_animIndex);
    }
    if (_spriteDragActive)
    {
        if (!IsMouseButtonDown(MOUSE_RIGHT_BUTTON))
            _spriteDragActive = false;
        else
        {
            // Offsets are authored facing right — compensate when flipped.
            const float facing = (_enemy->GetEditorFacing() >= 0.f) ? 1.f : -1.f;
            const Vector2 delta = Vector2Scale(Vector2Subtract(mouse, _spriteDragStartMouse), 1.f / _zoom);
            _enemy->SetAnimDrawOffset(_animIndex, Vector2{
                Snap(_spriteDragStartOffset.x + delta.x * facing),
                Snap(_spriteDragStartOffset.y + delta.y) });
        }
    }

    if (overStage || _dragHandle != -1)
        UpdateHandleDrag(mouse);
}

void CharacterAnimator::ClickControl(const UiItem& item)
{
    switch (item.id)
    {
    // ── Select screen ──
    case Control::EntryRow:          RequestOpenCharacter(item.index); break;
    case Control::OpenLineup:        OpenLineup(); break;
    case Control::OpenAttackLibrary: OpenAttackLibrary(); break;

    // ── Edit: left panel ──
    case Control::Tab:
    {
        static const EditTarget kTargets[] = { EditTarget::Sprite, EditTarget::Body, EditTarget::Melee, EditTarget::Attack };
        _target = kTargets[item.index];
        if (_target == EditTarget::Melee && !MeleeEditableNow())
        {
            // Jump to the first animation that has a melee box.
            for (int slot = 0; slot < _enemy->GetEditorAnimCount(); slot++)
                if (IsMeleeAnim(slot)) { SelectAnim(slot); break; }
        }
        _target = kTargets[item.index];
        _dragHandle = -1;
        break;
    }
    case Control::SizeMinus:
    case Control::SizePlus:
        StepValue(UiItem{ Control::SizeRow }, item.id == Control::SizePlus ? 1.f : -1.f);
        break;
    case Control::SizeAuto:
        ApplyScale(_enemy->GetAutoScale());
        SetStatus(TextFormat("Size back to automatic: %.2fx hero", SizeVsHero()));
        break;
    case Control::Lineup:     OpenLineup(); break;
    case Control::SpeedMinus:
    case Control::SpeedPlus:
        StepValue(UiItem{ Control::SpeedRow }, item.id == Control::SpeedPlus ? 1.f : -1.f);
        break;
    case Control::SpeedDefault:
        _enemy->SetEditorAnimFrameTime(_animIndex, 0.f);
        _enemy->SetEditorPlaybackFrameTime(_classFrameTime[_animIndex]);
        break;
    case Control::AnimRow:   SelectAnim(item.index); break;
    case Control::CopyToAll: CopyBodyAndSpriteToAll(); break;
    case Control::Save:      SaveAll(); break;
    case Control::Revert:    RevertToSaved(); break;
    case Control::Reset:     OpenDialog(Dialog::ConfirmReset, AfterDialog::None, -1); break;
    case Control::Back:      RequestBack(); break;

    // ── Edit: timeline bar ──
    case Control::StepBack:    StepFrame(-1); break;
    case Control::StepForward: StepFrame(1); break;
    case Control::PlayPause:   _animPaused = !_animPaused; break;
    case Control::HeroToggle:  _showHero = !_showHero; break;
    case Control::SnapToggle:  _snap = !_snap; break;
    case Control::Flip:
        _enemy->SetEditorFacing(_enemy->GetEditorFacing() >= 0.f ? -1.f : 1.f);
        _measureDirty = true;
        break;
    case Control::ZoomOut:
    case Control::ZoomIn:
    {
        const Rectangle stage = StageRect();
        const Vector2 centre{ stage.x + stage.width * 0.5f, stage.y + stage.height * 0.5f };
        ZoomAt(centre, item.id == Control::ZoomIn ? _zoom * kZoomStep : _zoom / kZoomStep);
        break;
    }
    case Control::ZoomReset: FitView(); break;

    // ── Edit: player attack panel ──
    case Control::AttackPrev: RequestSelectAttack(_attackIndex - 1); break;
    case Control::AttackNext: RequestSelectAttack(_attackIndex + 1); break;
    case Control::AttackPreview:
        _target = EditTarget::Attack;
        _attackPreviewActive = !_attackPreviewActive;
        _attackPreviewTimer = 0.f;
        _attackPreviewNextShot = 0.f;
        _attackPreviewShots.clear();
        break;

    // ── Dialog ──
    case Control::DialogSave:    AnswerDialog(DialogAnswer::Save); break;
    case Control::DialogDiscard: AnswerDialog(DialogAnswer::Discard); break;
    case Control::DialogCancel:  AnswerDialog(DialogAnswer::Cancel); break;

    // ── Edit: right panel ──
    case Control::RightTabMoves:
        _rightTab = RightTab::Moves;
        if (!IsPlayer() && _moveIndex >= 0)
            _target = EditTarget::Move;
        break;
    case Control::RightTabEvent:
        _rightTab = RightTab::Event;
        break;
    case Control::MoveRow:
        SelectMove(item.index);
        break;
    case Control::MoveCodeSize:
        if (_moveIndex >= 0 && _moveIndex < (int)_moves.size())
        {
            _moves[_moveIndex].tuning.hasHitArea = false;
            SetStatus("Back to the coded size");
        }
        break;
    case Control::EventRow:
        SelectEvent(item.index);
        break;
    case Control::EventAdd:
        AddEvent((CharacterTuning::EventKind)item.index, _enemy->GetEditorFrame());
        break;
    case Control::EventKindTab:
    {
        std::vector<CharacterTuning::AnimEvent> events = CurrentEvents();
        if (_eventIndex < 0 || _eventIndex >= (int)events.size())
            break;
        CharacterTuning::AnimEvent& event = events[_eventIndex];
        event.kind = (CharacterTuning::EventKind)item.index;
        event.size = 0.f;   // the new kind's default size
        if (event.kind == CharacterTuning::EventKind::Fx && event.fx.empty() && !_fxCatalog.empty())
            event.fx = _fxCatalog.front();
        SetCurrentEvents(events);
        break;
    }
    case Control::EventFxPrev:
    case Control::EventFxNext:
    {
        std::vector<CharacterTuning::AnimEvent> events = CurrentEvents();
        if (_eventIndex < 0 || _eventIndex >= (int)events.size() || _fxCatalog.empty())
            break;
        const int count = (int)_fxCatalog.size();
        int at = -1;
        for (int i = 0; i < count; i++)
            if (_fxCatalog[i] == events[_eventIndex].fx)
                at = i;
        const int step = (item.id == Control::EventFxNext) ? 1 : -1;
        at = (at < 0) ? 0 : (((at + step) % count) + count) % count;
        events[_eventIndex].fx = _fxCatalog[at];
        SetCurrentEvents(events);
        break;
    }
    case Control::EventDelete:
        if (_eventIndex >= 0)
            DeleteEvent(_eventIndex);
        break;


    // ── Lineup ──
    case Control::LineupBack:
        CloseLineup();
        _screen = (_lineupReturn == Screen::Edit && _enemy) ? Screen::Edit : Screen::Select;
        break;

    default: break;
    }
}

void CharacterAnimator::BeginValueDrag(const UiItem& item, Vector2 mouse)
{
    _rowDrag       = item.id;
    _rowDragIndex  = item.index;
    _rowDragStartX = mouse.x;
    switch (item.id)
    {
    case Control::SizeRow:
        _rowDragStartValue  = SizeVsHero();
        _rowDragStartTuning = CaptureCharacter();
        break;
    case Control::SpeedRow:
        _rowDragStartValue = CurrentFrameTime();
        break;
    case Control::AttackField:
        _target = EditTarget::Attack;
        _rowDragStartValue = _attackTuning.*kAttackFields[item.index].field;
        break;
    case Control::MoveField:
    {
        if (_moveIndex < 0) { _rowDrag = Control::None; break; }
        MoveEdit& move = _moves[_moveIndex];
        MakeMoveCustom(move);
        _target = EditTarget::Move;
        _rowDragStartValue = move.tuning.*MoveFields(move.info)[item.index].field;
        break;
    }
    case Control::EliteField:
    {
        if (_moveIndex < 0) { _rowDrag = Control::None; break; }
        float values[EliteMoves::kRowCount];
        EliteMoves::Values(_moves[_moveIndex].info.key, &_moves[_moveIndex].tuning, values);
        _rowDragStartValue = values[item.index];
        break;
    }
    case Control::EventField:
    {
        const std::vector<CharacterTuning::AnimEvent> events = CurrentEvents();
        if (_eventIndex < 0 || _eventIndex >= (int)events.size()) { _rowDrag = Control::None; break; }
        _rowDragStartValue = (item.index == 0) ? (float)events[_eventIndex].frame : EventSizeOf(events[_eventIndex]);
        break;
    }
    default:
        _rowDrag = Control::None;
        break;
    }
}

void CharacterAnimator::UpdateValueDrag(Vector2 mouse)
{
    const float deltaX = mouse.x - _rowDragStartX;
    switch (_rowDrag)
    {
    case Control::SizeRow:
    {
        // Scale from the drag's start so hitboxes never drift from rounding.
        const float multiple = std::clamp(_rowDragStartValue + deltaX * kSizeDragPerPx, kMinSizeVsHero, kMaxSizeVsHero);
        _enemy->SetDrawScale(ScaleForSizeVsHero(multiple));
        const float applied = _enemy->GetDrawScale();
        if (_rowDragStartTuning.scale > 0.f)
            RestoreCharacter(ScaledTuning(_rowDragStartTuning, applied / _rowDragStartTuning.scale));
        break;
    }
    case Control::SpeedRow:
        SetCurrentFrameTime(_rowDragStartValue + deltaX * kFrameTimePerPx);
        break;
    case Control::AttackField:
    {
        const AttackField& field = kAttackFields[_rowDragIndex];
        float value = std::clamp(_rowDragStartValue + deltaX * field.perPixel, field.minValue, field.maxValue);
        if (field.whole)
            value = roundf(value);
        _attackTuning.*field.field = value;
        _attackTuning.hasProjectile = true;
        _attackTuning.hasCooldown = true;
        _attackTuning.hasAbility = true;
        break;
    }
    case Control::MoveField:
    case Control::EliteField:
    case Control::EventField:
        SetPanelValue(_rowDrag, _rowDragIndex, _rowDragStartValue + deltaX * PanelValuePerPixel(_rowDrag, _rowDragIndex));
        break;
    default: break;
    }
}

void CharacterAnimator::StepValue(const UiItem& item, float steps)
{
    switch (item.id)
    {
    case Control::SizeRow:
    {
        // Land on whole steps so - / + give tidy values.
        const float current = roundf(SizeVsHero() / kSizeStep) * kSizeStep;
        ApplyScale(ScaleForSizeVsHero(current + steps * kSizeStep));
        break;
    }
    case Control::SpeedRow:
    {
        const float fps = roundf(1.f / CurrentFrameTime()) + steps * kFpsStep;
        if (fps > 0.f)
            SetCurrentFrameTime(1.f / fps);
        break;
    }
    case Control::AttackField:
    {
        const AttackField& field = kAttackFields[item.index];
        float value = std::clamp(_attackTuning.*field.field + steps * field.perPixel * kWheelPixels,
                                 field.minValue, field.maxValue);
        if (field.whole)
            value = roundf(value);
        _attackTuning.*field.field = value;
        _attackTuning.hasProjectile = true;
        _attackTuning.hasCooldown = true;
        _attackTuning.hasAbility = true;
        break;
    }
    case Control::MoveField:
    case Control::EliteField:
    case Control::EventField:
    {
        const float step = (item.id == Control::EventField && item.index == 0)
            ? 1.f : PanelValuePerPixel(item.id, item.index) * kWheelPixels;
        SetPanelValue(item.id, item.index, PanelValue(item.id, item.index) + steps * step);
        break;
    }
    default: break;
    }
}

// =============================================================================
// Effective values with fallbacks so the overlays always show something
// grabbable even before an animation has explicit data.
// =============================================================================
void CharacterAnimator::GetEffectiveBodyCircle(Vector2& outOffset, float& outRadius) const
{
    if (_enemy->GetAnimBodySet(_animIndex))
    {
        outOffset = _enemy->GetAnimBodyOffset(_animIndex);
        outRadius = _enemy->GetAnimBodyRadius(_animIndex);
        return;
    }

    // Fall back to the class capsule (facing right, so no mirroring needed).
    Capsule2D capsule = _enemy->GetCapsule();
    outOffset = Vector2{ capsule.center.x - _enemy->GetWorldPos().x,
                         capsule.center.y - _enemy->GetWorldPos().y };
    outRadius = std::max(kMinBodyRadius, capsule.radius + capsule.halfHeight * 0.5f);
}

bool CharacterAnimator::MeleeEditableNow() const
{
    return IsMeleeAnim(_animIndex);
}

Rectangle CharacterAnimator::GetEffectiveMeleeRel() const
{
    // This animation's own box, else the attack animation's (the game falls
    // back the same way), else the legacy attack box or a default in front.
    if (_enemy->GetAnimMeleeSet(_animIndex))
        return _enemy->GetAnimMeleeRelRect(_animIndex);
    if (_enemy->GetAnimMeleeSet(Enemy::kAnimSlotAttack))
        return _enemy->GetAnimMeleeRelRect(Enemy::kAnimSlotAttack);

    const float w = _enemy->GetAttackBoxWidth();
    const float h = _enemy->GetAttackBoxHeight();
    if (w > kMinMeleeSize * 0.5f && h > kMinMeleeSize * 0.5f)
        return Rectangle{ _enemy->GetAttackBoxOffsetX() - w * 0.5f,
                          _enemy->GetAttackBoxOffsetY() - h * 0.5f, w, h };
    return Rectangle{ 40.f, -50.f, 130.f, 110.f };
}

// =============================================================================
// Melee per animation + hit windows. An animation has a melee box only where
// the game reads one (the class's GetDefaultHitWindow), so the editor never
// offers a box the game ignores.
// =============================================================================
bool CharacterAnimator::IsMeleeAnim(int slot) const
{
    int start = 0, end = 0;
    return _enemy && _selectedIdx >= 0 && _entries[_selectedIdx].meleeSlot >= 0 &&
           slot >= 0 && slot < CharacterTuning::kMaxAnims && _enemy->GetDefaultHitWindow(slot, start, end);
}

bool CharacterAnimator::CodeHitWindow(int slot, int& start, int& end) const
{
    if (!IsMeleeAnim(slot) || !_enemy->GetDefaultHitWindow(slot, start, end))
        return false;
    // "To the end" becomes this sheet's last frame.
    const int last = std::max(0, _animFrameCount[slot] - 1);
    start = std::clamp(start, 0, last);
    end   = std::clamp(end, start, last);
    return true;
}

bool CharacterAnimator::HitWindowFor(int slot, int& start, int& end, bool& authored) const
{
    authored = _enemy->GetAnimHitSet(slot);
    if (authored)
    {
        start = _enemy->GetAnimHitStart(slot);
        end   = _enemy->GetAnimHitEnd(slot);
        return true;
    }
    return CodeHitWindow(slot, start, end);
}

bool CharacterAnimator::MeleeLiveNow() const
{
    int start = 0, end = 0;
    bool authored = false;
    if (!IsMeleeAnim(_animIndex) || !HitWindowFor(_animIndex, start, end, authored))
        return false;
    const int frame = _enemy->GetEditorFrame();
    return frame >= start && frame <= end;
}

// =============================================================================
// Handle dragging. Everything is edited relative to the character's position;
// the view's zoom and pan only change where it shows on screen.
// =============================================================================
void CharacterAnimator::UpdateHandleDrag(Vector2 mouse)
{
    // Hitboxes, moves and events are authored facing right; with the preview
    // flipped their handles mirror, and dragging works in facing-right space.
    const float facing = (_enemy->GetEditorFacing() >= 0.f) ? 1.f : -1.f;
    auto toScreen = [&](Vector2 rel) { return ToScreen(Vector2{ rel.x * facing, rel.y }); };
    const Vector2 screenRel = ToRel(mouse);
    const Vector2 rawRel{ screenRel.x * facing, screenRel.y };
    const Vector2 mouseRel{ Snap(rawRel.x), Snap(rawRel.y) };

    Vector2 bodyOffset;
    float bodyRadius;
    GetEffectiveBodyCircle(bodyOffset, bodyRadius);

    const Rectangle meleeRel = MeleeEditableNow() ? GetEffectiveMeleeRel() : Rectangle{};
    // The player's attack overlay keeps its own convention (as on screen).
    const Vector2 attackFire = ToScreen(Vector2{ _attackTuning.fireForward, _attackTuning.fireHeight });
    const Vector2 attackRadiusHandle{ attackFire.x + std::max(1.f, _attackTuning.projRadius) * _zoom, attackFire.y };
    const Vector2 attackFxOrigin = ToScreen(Vector2{ _attackTuning.fxForward, _attackTuning.fxHeight });
    const Vector2 spriteHandle = toScreen(_enemy->GetAnimDrawOffsetValue(_animIndex));

    // What the player's attacks hit, padded past the body square. The
    // character stands at the origin, so its world rects are relative ones.
    const Rectangle hurt = _enemy->GetHitCollisionRec();
    const Rectangle body = _enemy->GetCollisionRec();
    const bool padding = _enemy->UsesHurtPadding();
    const Vector2 padXHandle = ToScreen(Vector2{ hurt.x + hurt.width, hurt.y + hurt.height * 0.5f });
    const Vector2 padYHandle = ToScreen(Vector2{ hurt.x + hurt.width * 0.5f, hurt.y + hurt.height });

    MoveEdit* move = (!IsPlayer() && _moveIndex >= 0 && _moveIndex < (int)_moves.size()) ? &_moves[_moveIndex] : nullptr;
    std::vector<CharacterTuning::AnimEvent> events = CurrentEvents();
    const bool fxEventSelected = _eventIndex >= 0 && _eventIndex < (int)events.size() &&
                                 events[_eventIndex].kind == CharacterTuning::EventKind::Fx;

    auto rectHandles = [&](Rectangle rel, Vector2* outPositions)
    {
        outPositions[kHandleTopLeft]     = { rel.x,             rel.y };
        outPositions[kHandleTopRight]    = { rel.x + rel.width, rel.y };
        outPositions[kHandleBottomLeft]  = { rel.x,             rel.y + rel.height };
        outPositions[kHandleBottomRight] = { rel.x + rel.width, rel.y + rel.height };
        outPositions[kHandleCentre]      = { rel.x + rel.width * 0.5f, rel.y + rel.height * 0.5f };
    };
    // A box's handle under the mouse; corners remember the opposite corner.
    auto tryBoxHandles = [&](Rectangle rel) -> bool
    {
        Vector2 positions[5];
        rectHandles(rel, positions);
        for (int i = 0; i < 5; i++)
        {
            if (!HandleHit(mouse, toScreen(positions[i])))
                continue;
            _dragHandle = i;
            if (i == kHandleTopLeft)     _dragAnchor = positions[kHandleBottomRight];
            if (i == kHandleTopRight)    _dragAnchor = positions[kHandleBottomLeft];
            if (i == kHandleBottomLeft)  _dragAnchor = positions[kHandleTopRight];
            if (i == kHandleBottomRight) _dragAnchor = positions[kHandleTopLeft];
            return true;
        }
        return false;
    };
    // A box dragged by its centre or a corner.
    auto dragBox = [&](Rectangle rel) -> Rectangle
    {
        if (_dragHandle == kHandleCentre)
        {
            rel.x = Snap(rawRel.x - rel.width * 0.5f);
            rel.y = Snap(rawRel.y - rel.height * 0.5f);
            return rel;
        }
        return Rectangle{ std::min(mouseRel.x, _dragAnchor.x), std::min(mouseRel.y, _dragAnchor.y),
                          std::max(kMinMeleeSize, fabsf(mouseRel.x - _dragAnchor.x)),
                          std::max(kMinMeleeSize, fabsf(mouseRel.y - _dragAnchor.y)) };
    };

    // ── Begin drag ────────────────────────────────────────────────────────────
    // Clicking ANY overlay's handle selects that target automatically.
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && _dragHandle == -1)
    {
        auto tryBegin = [&](EditTarget target) -> bool
        {
            switch (target)
            {
            case EditTarget::Sprite:
                if (HandleHit(mouse, spriteHandle))
                    _dragHandle = kHandleCentre;
                break;
            case EditTarget::Body:
            {
                const Vector2 centreHandle = toScreen(bodyOffset);
                const Vector2 radiusHandle{ centreHandle.x + bodyRadius * _zoom, centreHandle.y };
                if      (HandleHit(mouse, radiusHandle))             _dragHandle = kHandleRadius;
                else if (HandleHit(mouse, centreHandle))             _dragHandle = kHandleCentre;
                else if (padding && HandleHit(mouse, padXHandle))    _dragHandle = kHandlePadX;
                else if (padding && HandleHit(mouse, padYHandle))    _dragHandle = kHandlePadY;
                break;
            }
            case EditTarget::Melee:
                if (MeleeEditableNow())
                    tryBoxHandles(meleeRel);
                break;
            case EditTarget::Attack:
                if (!PlayerAttackEditorAvailable())
                    break;
                if (HandleHit(mouse, attackFxOrigin))
                    _dragHandle = kHandleFxOrigin;
                else if (HandleHit(mouse, attackRadiusHandle))
                    _dragHandle = kHandleRadius;
                else if (HandleHit(mouse, attackFire))
                    _dragHandle = kHandleCentre;
                break;
            case EditTarget::Move:
            {
                if (move == nullptr || _rightTab != RightTab::Moves)
                    break;
                const Enemy::MoveInfo& info = move->info;
                auto value = [&](float AttackTuning::* field) { return MoveValue(move->tuning, info, field); };
                switch (info.shape)
                {
                case Enemy::MoveShape::Circle:
                {
                    const float ahead = value(&AttackTuning::hitForward);
                    if (HandleHit(mouse, toScreen(Vector2{ ahead + value(&AttackTuning::hitRadius), 0.f })))
                        _dragHandle = kHandleRadius;
                    else if (info.forward != 0.f && HandleHit(mouse, toScreen(Vector2{ ahead, 0.f })))
                        _dragHandle = kHandleCentre;
                    break;
                }
                case Enemy::MoveShape::Ring:
                    if (HandleHit(mouse, toScreen(Vector2{ value(&AttackTuning::hitRadius), 0.f })))
                        _dragHandle = kHandleRadius;
                    else if (HandleHit(mouse, toScreen(Vector2{ value(&AttackTuning::hitInner), 0.f })))
                        _dragHandle = kHandleInner;
                    break;
                case Enemy::MoveShape::Lane:
                {
                    const float length = value(&AttackTuning::hitLength);
                    if (HandleHit(mouse, toScreen(Vector2{ length, 0.f })))
                        _dragHandle = kHandleLaneEnd;
                    else if (HandleHit(mouse, toScreen(Vector2{ length * 0.5f, value(&AttackTuning::hitWidth) })))
                        _dragHandle = kHandleLaneSide;
                    break;
                }
                case Enemy::MoveShape::Box:
                    tryBoxHandles(MoveBoxRel(move->tuning, info));
                    break;
                }
                if (_dragHandle != -1)
                    MakeMoveCustom(*move);
                break;
            }
            case EditTarget::Event:
                if (fxEventSelected && _rightTab == RightTab::Event &&
                    HandleHit(mouse, toScreen(Vector2{ events[_eventIndex].x, events[_eventIndex].y })))
                    _dragHandle = kHandleCentre;
                break;
            }
            return _dragHandle != -1;
        };

        // The active target's handles win where they overlap.
        const EditTarget order[] = { _target, EditTarget::Event, EditTarget::Move, EditTarget::Sprite,
                                     EditTarget::Body, EditTarget::Melee, EditTarget::Attack };
        for (EditTarget target : order)
        {
            if (tryBegin(target))
            {
                _target = target;
                break;
            }
        }
    }

    if (_dragHandle == -1)
        return;
    if (!IsMouseButtonDown(MOUSE_LEFT_BUTTON))
    {
        _dragHandle = -1;
        return;
    }

    // ── Apply drag ────────────────────────────────────────────────────────────
    switch (_target)
    {
    case EditTarget::Sprite:
        _enemy->SetAnimDrawOffset(_animIndex, mouseRel);
        return;

    case EditTarget::Attack:
    {
        if (!PlayerAttackEditorAvailable())
            return;
        const Vector2 onScreen{ Snap(screenRel.x), Snap(screenRel.y) };
        if (_dragHandle == kHandleFxOrigin)
        {
            _attackTuning.fxForward = onScreen.x;
            _attackTuning.fxHeight = onScreen.y;
            _attackTuning.hasFxOffset = true;
        }
        else if (_dragHandle == kHandleCentre)
        {
            _attackTuning.fireForward = onScreen.x;
            _attackTuning.fireHeight = onScreen.y;
            _attackTuning.hasFirePoint = true;
        }
        else if (_dragHandle == kHandleRadius)
        {
            _attackTuning.projRadius = std::max(1.f, Snap(fabsf(screenRel.x - _attackTuning.fireForward)));
            _attackTuning.hasProjectile = true;
        }
        return;
    }

    case EditTarget::Body:
        if (_dragHandle == kHandleCentre)
            _enemy->SetAnimBody(_animIndex, mouseRel, bodyRadius);
        else if (_dragHandle == kHandleRadius)
            _enemy->SetAnimBody(_animIndex, bodyOffset, Snap(fabsf(rawRel.x - bodyOffset.x)));
        else if (_dragHandle == kHandlePadX || _dragHandle == kHandlePadY)
        {
            // Padding is the same on both sides: how far past the body square.
            const float centreX = body.x + body.width * 0.5f;
            const float centreY = body.y + body.height * 0.5f;
            float padX = _enemy->GetHurtPadX();
            float padY = _enemy->GetHurtPadY();
            if (_dragHandle == kHandlePadX)
                padX = Snap(std::max(0.f, fabsf(screenRel.x - centreX) - body.width * 0.5f));
            else
                padY = Snap(std::max(0.f, fabsf(screenRel.y - centreY) - body.height * 0.5f));
            _enemy->SetHurtPad(padX, padY);
        }
        return;

    case EditTarget::Melee:
        _enemy->SetAnimMelee(_animIndex, dragBox(meleeRel));
        return;

    case EditTarget::Move:
    {
        if (move == nullptr)
            return;
        AttackTuning& tuning = move->tuning;
        switch (move->info.shape)
        {
        case Enemy::MoveShape::Circle:
            if (_dragHandle == kHandleCentre)
                tuning.hitForward = mouseRel.x;
            else
                tuning.hitRadius = std::max(kMinMoveSize, Snap(Vector2Distance(rawRel, Vector2{ tuning.hitForward, 0.f })));
            break;
        case Enemy::MoveShape::Ring:
        {
            const float distance = Snap(Vector2Length(rawRel));
            if (_dragHandle == kHandleInner)
                tuning.hitInner = std::clamp(distance, 0.f, tuning.hitRadius);
            else
                tuning.hitRadius = std::max(std::max(kMinMoveSize, tuning.hitInner), distance);
            break;
        }
        case Enemy::MoveShape::Lane:
            if (_dragHandle == kHandleLaneEnd)
                tuning.hitLength = std::max(kMinMoveSize, mouseRel.x);
            else
                tuning.hitWidth = std::max(kMinMoveSize, fabsf(mouseRel.y));
            break;
        case Enemy::MoveShape::Box:
        {
            const Rectangle box = dragBox(MoveBoxRel(tuning, move->info));
            tuning.hitX = box.x + box.width * 0.5f;
            tuning.hitY = box.y + box.height * 0.5f;
            tuning.hitW = box.width;
            tuning.hitH = box.height;
            break;
        }
        }
        return;
    }

    case EditTarget::Event:
        if (fxEventSelected)
        {
            events[_eventIndex].x = mouseRel.x;
            events[_eventIndex].y = mouseRel.y;
            SetCurrentEvents(events);
        }
        return;
    }
}

bool CharacterAnimator::HandleHit(Vector2 mouse, Vector2 screenPos) const
{
    return fabsf(mouse.x - screenPos.x) <= kHandleSize && fabsf(mouse.y - screenPos.y) <= kHandleSize;
}

// =============================================================================
// View: zoom + pan. The character's position sits at the stage's centre.
// =============================================================================
Rectangle CharacterAnimator::StageRect() const
{
    const float right = kVirtualWidth - kRightPanelWidth;
    return Rectangle{ kPanelWidth, 0.f, right - kPanelWidth, kTimelineTop };
}

Vector2 CharacterAnimator::StageOrigin() const
{
    const Rectangle stage = StageRect();
    return Vector2{ stage.x + stage.width * 0.5f + _pan.x, stage.y + stage.height * 0.5f + _pan.y };
}

Vector2 CharacterAnimator::ToScreen(Vector2 rel) const
{
    const Vector2 origin = StageOrigin();
    return Vector2{ origin.x + rel.x * _zoom, origin.y + rel.y * _zoom };
}

Vector2 CharacterAnimator::ToRel(Vector2 screen) const
{
    const Vector2 origin = StageOrigin();
    return Vector2{ (screen.x - origin.x) / _zoom, (screen.y - origin.y) / _zoom };
}

Camera2D CharacterAnimator::StageCamera() const
{
    // Characters draw world (0,0) at the canvas centre; the camera moves that
    // point to the stage origin and applies the zoom.
    Camera2D camera{};
    camera.target   = Vector2{ kVirtualWidth * 0.5f, kVirtualHeight * 0.5f };
    camera.offset   = StageOrigin();
    camera.zoom     = _zoom;
    camera.rotation = 0.f;
    return camera;
}

float CharacterAnimator::Snap(float value) const
{
    if (!_snap || ShiftDown())
        return value;
    return roundf(value / kSnapStep) * kSnapStep;
}

void CharacterAnimator::ZoomAt(Vector2 screenPoint, float newZoom)
{
    // Keep the point under the cursor where it is.
    const Vector2 rel = ToRel(screenPoint);
    _zoom = std::clamp(newZoom, kMinZoom, kMaxZoom);
    const Rectangle stage = StageRect();
    _pan = Vector2{ screenPoint.x - rel.x * _zoom - (stage.x + stage.width * 0.5f),
                    screenPoint.y - rel.y * _zoom - (stage.y + stage.height * 0.5f) };
}

void CharacterAnimator::ResetView()
{
    _zoom = 1.f;
    _pan  = Vector2{};
}

void CharacterAnimator::FitView()
{
    // Frame the character (and the hero beside it) so it fills the stage.
    if (_charBounds.height <= 0.f)
    {
        ResetView();
        return;
    }
    Rectangle group = _charBounds;
    if (_showHero && !IsPlayer() && _heroBounds.height > 0.f)
    {
        const float heroLeft = _heroPos.x + _heroBounds.x;
        const float heroTop  = _heroPos.y + _heroBounds.y;
        const float right    = group.x + group.width;
        const float bottom   = std::max(group.y + group.height, heroTop + _heroBounds.height);
        group.x      = std::min(group.x, heroLeft);
        group.y      = std::min(group.y, heroTop);
        group.width  = right - group.x;
        group.height = bottom - group.y;
    }
    group.width += kBracketGap + kFitLabelRoom;   // the size bracket and its label

    const Rectangle stage = StageRect();
    const float zoom = std::min(stage.width * kFitShare / group.width, stage.height * kFitShare / group.height);
    _zoom = std::clamp(zoom, kMinZoom, kFitMaxZoom);
    // The group's centre lands on the stage's centre.
    const Vector2 centre{ group.x + group.width * 0.5f, group.y + group.height * 0.5f };
    _pan = Vector2{ -centre.x * _zoom, -centre.y * _zoom };
}

// =============================================================================
// Measuring. Characters draw themselves in many different ways, so the only
// reliable feet line is the one they actually draw: render once off screen and
// find the opaque pixels.
// =============================================================================
Rectangle CharacterAnimator::MeasureDrawnBounds(Enemy& enemy)
{
    if (_measureTarget.id == 0)
        _measureTarget = LoadRenderTexture(kMeasureCanvas, kMeasureCanvas);

    const float half = kMeasureCanvas * 0.5f;
    const Vector2 world = enemy.GetWorldPos();
    // DrawEnemy puts (world - cameraRef) at the canvas centre; aim it at the
    // middle of the measuring texture instead.
    const Vector2 cameraRef{ world.x + kVirtualWidth * 0.5f - half, world.y + kVirtualHeight * 0.5f - half };
    BeginTextureMode(_measureTarget);
    ClearBackground(BLANK);
    enemy.DrawEnemy(cameraRef);
    EndTextureMode();
    Vfx::Glow().Discard();   // light recorded while measuring belongs to no frame

    Image image = LoadImageFromTexture(_measureTarget.texture);
    Color* pixels = LoadImageColors(image);
    int minX = image.width, minY = image.height, maxX = -1, maxY = -1;
    for (int y = 0; y < image.height; y++)
        for (int x = 0; x < image.width; x++)
            if (pixels[y * image.width + x].a >= kMeasureAlpha)
            {
                minX = std::min(minX, x);
                maxX = std::max(maxX, x);
                minY = std::min(minY, y);
                maxY = std::max(maxY, y);
            }
    UnloadImageColors(pixels);
    UnloadImage(image);
    if (maxX < 0)
        return Rectangle{};

    // Render textures are stored bottom-up.
    const float top    = (float)(kMeasureCanvas - 1 - maxY);
    const float bottom = (float)(kMeasureCanvas - minY);
    return Rectangle{ minX - half, top - half, (float)(maxX - minX + 1), bottom - top };
}

void CharacterAnimator::MeasureCharacter()
{
    _measureDirty = false;
    if (!_enemy)
        return;

    // Standing still: idle, first frame, at the origin.
    const int frame = _enemy->GetEditorFrame();
    _enemy->Teleport({ 0.f, 0.f });
    if (_animIndex != 0)
        PlayAnimInEditor(0);
    else
        _enemy->SetEditorFrame(0);
    _charBounds = MeasureDrawnBounds(*_enemy);
    if (_animIndex != 0)
        PlayAnimInEditor(_animIndex);
    _enemy->SetEditorFrame(frame);
    _groundRel = _charBounds.y + _charBounds.height;

    if (_hero && _heroBounds.height <= 0.f)
    {
        _hero->Teleport({ 0.f, 0.f });
        _hero->SetEditorFacing(1.f);
        _hero->PlayEditorAnim(0);
        _heroBounds = MeasureDrawnBounds(*_hero);
        _heroHeightPx = Enemy::MeasureVisibleHeight(_hero->GetEditorSheet(), _hero->GetEditorFrameWidth()) * _hero->GetDrawScale();
    }
    PlaceHero();
}

void CharacterAnimator::PlaceHero()
{
    // Left of the character, feet on its ground line.
    const float heroRight = _heroBounds.x + _heroBounds.width;
    _heroPos = Vector2{ _charBounds.x - kHeroGap - heroRight,
                        _groundRel - (_heroBounds.y + _heroBounds.height) };
}

// =============================================================================
// Lineup: everyone at their in-game size on one ground line per row, each row
// starting with the hero. The character being edited shows its unsaved state.
// =============================================================================
void CharacterAnimator::OpenLineup()
{
    _lineupReturn = (_enemy && _screen == Screen::Edit) ? Screen::Edit : Screen::Select;
    if (_enemy)
        _editFacing = _enemy->GetEditorFacing();
    _lineup.clear();

    // Rows: hero + enemies, elites, bosses; the elite and boss rows start with
    // the hero (the first row starts with the Player itself).
    const Category rows[][2] = {
        { Category::Hero,  Category::Enemy },
        { Category::Elite, Category::Elite },
        { Category::Boss,  Category::Boss },
    };
    for (int r = 0; r < (int)(sizeof(rows) / sizeof(rows[0])); r++)
    {
        if (rows[r][0] != Category::Hero)
        {
            LineupItem hero;
            hero.instance = _hero.get();
            hero.row = r;
            _lineup.push_back(std::move(hero));
        }
        for (int i = 0; i < (int)_entries.size(); i++)
        {
            if (_entries[i].category != rows[r][0] && _entries[i].category != rows[r][1])
                continue;
            LineupItem item;
            item.entry = i;
            item.row = r;
            if (_enemy && i == _selectedIdx)
                item.instance = _enemy.get();
            else
            {
                item.owned.reset(_entries[i].createInstance());
                item.instance = item.owned.get();
            }
            _lineup.push_back(std::move(item));
        }
    }

    // Measure each once, standing still at its in-game size.
    for (LineupItem& item : _lineup)
    {
        Enemy& enemy = *item.instance;
        enemy.Teleport({ 0.f, 0.f });
        enemy.SetEditorFacing(1.f);
        enemy.PlayEditorAnim(0);
        item.bounds = MeasureDrawnBounds(enemy);
        const float artHeight = Enemy::MeasureVisibleHeight(enemy.GetEditorSheet(), enemy.GetEditorFrameWidth());
        item.heightPx   = artHeight * enemy.GetDrawScale();
        item.sizeVsHero = item.heightPx / Balance::Visual::kPlayerVisibleHeight;
    }

    // Labels and gaps are screen px, so the layout depends on the zoom and the
    // zoom on the layout: a few passes settle both.
    _lineupZoom = 1.f;
    const float availW = kVirtualWidth - kLineupMargin * 2.f;
    const float availH = kVirtualHeight - kLineupTop - kLineupMargin * 2.f;
    float minX = 0.f, minY = 0.f, maxX = 0.f, maxY = 0.f;
    for (int pass = 0; pass < kLineupFitPasses; pass++)
    {
        LayoutLineup(_lineupZoom);
        minX = minY = 0.f;
        maxX = maxY = 0.f;
        for (const LineupItem& item : _lineup)
        {
            const float slot = std::max(item.bounds.width, kLineupLabelSlot / _lineupZoom);
            const float left = item.position.x + item.bounds.x + (item.bounds.width - slot) * 0.5f;
            minX = std::min(minX, left);
            maxX = std::max(maxX, left + slot);
            minY = std::min(minY, item.lineTop - kLineupHeaderSpace / _lineupZoom);
            maxY = std::max(maxY, item.ground + kLineupLabelSpace / _lineupZoom);
        }
        const float width  = std::max(1.f, maxX - minX);
        const float height = std::max(1.f, maxY - minY);
        _lineupZoom = std::clamp(std::min(availW / width, availH / height), kMinZoom, kLineupMaxZoom);
    }
    LayoutLineup(_lineupZoom);

    // Centre it under the title.
    const Vector2 centre{ (minX + maxX) * 0.5f, (minY + maxY) * 0.5f };
    _lineupOrigin = Vector2{ kVirtualWidth * 0.5f - centre.x * _lineupZoom,
                             kLineupTop + kLineupMargin + availH * 0.5f - centre.y * _lineupZoom };
    _lineupHover = -1;
    _panning = false;
    _screen = Screen::Lineup;
}

void CharacterAnimator::LayoutLineup(float zoom)
{
    // Lines wrap at the screen's width; each row starts a new line under its
    // header. Every character gets a slot wide enough for its label.
    const float maxLineWidth = (kVirtualWidth - kLineupMargin * 2.f) / zoom;
    const float gap          = kLineupGap / zoom;
    const float labelSlot    = kLineupLabelSlot / zoom;
    const float labelSpace   = kLineupLabelSpace / zoom;
    const float headerSpace  = kLineupHeaderSpace / zoom;

    float ground = 0.f;
    bool firstLine = true;
    size_t i = 0;
    while (i < _lineup.size())
    {
        // One line: as many of this row's characters as fit.
        const int row = _lineup[i].row;
        const bool rowStart = (i == 0 || _lineup[i - 1].row != row);
        size_t end = i;
        float width = 0.f;
        float tallest = 0.f;
        while (end < _lineup.size() && _lineup[end].row == row)
        {
            const float slot = std::max(_lineup[end].bounds.width, labelSlot);
            if (end > i && width + slot > maxLineWidth)
                break;
            width += slot + gap;
            tallest = std::max(tallest, _lineup[end].bounds.height);
            end++;
        }

        ground += tallest + (firstLine ? 0.f : labelSpace) + (rowStart && !firstLine ? headerSpace : 0.f);
        firstLine = false;
        float cursor = 0.f;
        for (size_t k = i; k < end; k++)
        {
            LineupItem& item = _lineup[k];
            const float slot = std::max(item.bounds.width, labelSlot);
            item.position = Vector2{ cursor + (slot - item.bounds.width) * 0.5f - item.bounds.x,
                                     ground - (item.bounds.y + item.bounds.height) };
            item.ground = ground;
            item.lineTop = ground - tallest;
            item.firstInRow = rowStart && k == i;
            cursor += slot + gap;
        }
        i = end;
    }
}

void CharacterAnimator::CloseLineup()
{
    if (_lineup.empty())
        return;
    _lineup.clear();
    _panning = false;
    // The character being edited goes back to the stage as it was.
    if (_enemy)
    {
        _enemy->Teleport({ 0.f, 0.f });
        _enemy->SetEditorFacing(_editFacing);
        ReplayCurrentAnimKeepingFrame();
    }
    if (_hero)
    {
        _hero->Teleport({ 0.f, 0.f });
        _hero->SetEditorFacing(1.f);
    }
    _measureDirty = true;
}

void CharacterAnimator::UpdateLineup(float dt)
{
    const Vector2 mouse = GetVirtualMousePos();
    _ui.clear();
    UiItem back;
    back.id = Control::LineupBack;
    back.rect = Rectangle{ kPad, kPad, kSelectButtonW * 0.5f, kButtonHeight };
    back.label = (_lineupReturn == Screen::Edit && _enemy) ? "< Back to editor" : "< Back";
    back.tooltip = "Leave the lineup (Esc).";
    _ui.push_back(back);
    UpdateTooltip(_ui, mouse, dt);

    if (IsKeyPressed(KEY_ESCAPE))
    {
        ClickControl(back);
        return;
    }

    auto toScreen = [this](Vector2 world) {
        return Vector2{ _lineupOrigin.x + world.x * _lineupZoom, _lineupOrigin.y + world.y * _lineupZoom };
    };

    // Which character is under the mouse.
    _lineupHover = -1;
    for (int i = 0; i < (int)_lineup.size(); i++)
    {
        const LineupItem& item = _lineup[i];
        const Vector2 topLeft = toScreen(Vector2{ item.position.x + item.bounds.x, item.position.y + item.bounds.y });
        const Rectangle rect{ topLeft.x, topLeft.y, item.bounds.width * _lineupZoom, item.bounds.height * _lineupZoom };
        if (item.entry >= 0 && CheckCollisionPointRec(mouse, rect))
            _lineupHover = i;
    }

    const UiItem* hit = HitUi(_ui, mouse);
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
    {
        if (hit != nullptr)
        {
            const UiItem item = *hit;
            ClickControl(item);
            return;
        }
        if (_lineupHover >= 0)
        {
            RequestOpenCharacter(_lineup[_lineupHover].entry);
            return;
        }
        _panning = true;   // drag empty space to pan
    }

    // Wheel zooms at the cursor; drag pans.
    const float wheel = GetMouseWheelMove();
    if (wheel != 0.f)
    {
        const Vector2 world{ (mouse.x - _lineupOrigin.x) / _lineupZoom, (mouse.y - _lineupOrigin.y) / _lineupZoom };
        _lineupZoom = std::clamp(_lineupZoom * powf(kZoomStep, wheel), kMinZoom, kMaxZoom);
        _lineupOrigin = Vector2{ mouse.x - world.x * _lineupZoom, mouse.y - world.y * _lineupZoom };
    }
    if (IsMouseButtonPressed(MOUSE_MIDDLE_BUTTON))
        _panning = true;
    if (_panning)
    {
        if (!IsMouseButtonDown(MOUSE_LEFT_BUTTON) && !IsMouseButtonDown(MOUSE_MIDDLE_BUTTON))
            _panning = false;
        else
            _lineupOrigin = Vector2Add(_lineupOrigin, Vector2Subtract(mouse, _lastMouse));
    }

    // Everyone idles; the hero appears on several rows but ticks once.
    if (_hero)
        _hero->TickEditorAnimation(dt);
    for (LineupItem& item : _lineup)
        if (item.instance != _hero.get())
            item.instance->TickEditorAnimation(dt);
}

// =============================================================================
// UI building
// =============================================================================
void CharacterAnimator::BuildSelectUi()
{
    _ui.clear();
    const float buttonsLeft = (kVirtualWidth - kSelectButtonW * 2.f - kPad) * 0.5f;

    UiItem lineup;
    lineup.id = Control::OpenLineup;
    lineup.rect = Rectangle{ buttonsLeft, kSelectButtonsY, kSelectButtonW, kSelectButtonH };
    lineup.label = "SIZE LINEUP";
    lineup.detail = "everyone at in-game size";
    lineup.tooltip = "Every character side by side at the size the game draws them, with the hero on each row for reference.";
    lineup.accent = kHeroColor;
    _ui.push_back(lineup);

    UiItem library;
    library.id = Control::OpenAttackLibrary;
    library.rect = Rectangle{ buttonsLeft + kSelectButtonW + kPad, kSelectButtonsY, kSelectButtonW, kSelectButtonH };
    library.label = "ATTACK FX + HITBOXES";
    library.detail = "all player abilities and bosses";
    library.tooltip = "The Attack Library: damage boxes, FX sprites and elite timings for every attack.";
    library.accent = kAttackColor;
    _ui.push_back(library);

    struct Group { Category category; const char* title; int column; };
    const Group groups[] = {
        { Category::Hero,  "HERO",    0 },
        { Category::Enemy, "ENEMIES", 0 },
        { Category::Elite, "ELITES",  1 },
        { Category::Boss,  "BOSSES",  2 },
    };
    const float left = (kVirtualWidth - kSelectColumnStep * (kSelectColumns - 1) - kSelectColumnW) * 0.5f;
    float columnY[kSelectColumns] = { kSelectTop, kSelectTop, kSelectTop };
    for (const Group& group : groups)
    {
        const float x = left + group.column * kSelectColumnStep;
        float& y = columnY[group.column];
        UiItem header;
        header.kind = UiKind::Label;
        header.rect = Rectangle{ x, y, kSelectColumnW, kSelectHeaderStep };
        header.label = group.title;
        _ui.push_back(header);
        y += kSelectHeaderStep;
        for (int i = 0; i < (int)_entries.size(); i++)
        {
            if (_entries[i].category != group.category)
                continue;
            UiItem row;
            row.id = Control::EntryRow;
            row.index = i;
            row.kind = UiKind::ListRow;
            row.rect = Rectangle{ x, y, kSelectColumnW, kSelectRowHeight };
            row.label = _entries[i].displayName;
            const bool tuned = CharacterTuningStore::Get(_entries[i].displayName) != nullptr;
            row.detail = tuned ? "tuned" : "";
            row.accent = kGoodColor;
            row.detailAccent = true;
            row.tooltip = tuned ? "Open in the editor. Has a saved tuning file."
                                : "Open in the editor. No tuning file yet: it plays with its code defaults.";
            _ui.push_back(row);
            y += kSelectRowStep;
        }
        y += kSelectGroupGap;
    }
}

void CharacterAnimator::BuildEditUi()
{
    _ui.clear();
    _ui.reserve(kUiReserve);
    if (!_enemy)
        return;

    const CharacterEntry& entry = _entries[_selectedIdx];
    const bool player = IsPlayer();
    auto add = [this](Control id, UiKind kind, Rectangle rect, const std::string& label, const std::string& tooltip) -> UiItem&
    {
        UiItem item;
        item.id = id;
        item.kind = kind;
        item.rect = rect;
        item.label = label;
        item.tooltip = tooltip;
        _ui.push_back(std::move(item));
        return _ui.back();
    };
    auto header = [&](float y, const char* text) { add(Control::None, UiKind::Label, Rectangle{ kPad, y, kPanelWidth - kPad * 2.f, kHeaderGap }, text, ""); };

    const float x = kPad;
    const float w = kPanelWidth - kPad * 2.f;
    const float half = (w - kGap) * 0.5f;

    // ── Mode tabs ─────────────────────────────────────────────────────────────
    struct TabDef { EditTarget target; const char* label; Color color; const char* tooltip; };
    const TabDef tabs[] = {
        { EditTarget::Sprite, "Sprite", kSpriteColor, "Move the picture only (Tab cycles modes). Hitboxes stay put, so use it to line the art up with the body. Right-drag also moves it." },
        { EditTarget::Body,   "Body",   kBodyColor,   "The body circle: what blocks movement and what gets hit. One per animation; drag the centre or the edge handle." },
        { EditTarget::Melee,  "Melee",  kMeleeColor,  "The melee swing box. It lives on the attack animation, so this jumps there." },
        { EditTarget::Attack, "Attack", kAttackColor, "The player's attack: fire point, FX origin and projectile (right panel). Shortcut: A." },
    };
    const int tabCount = player ? 4 : 3;
    const float tabW = (w - kGap * (tabCount - 1)) / tabCount;
    for (int i = 0; i < tabCount; i++)
    {
        UiItem& tab = add(Control::Tab, UiKind::Tab, Rectangle{ x + i * (tabW + kGap), kTabsY, tabW, kTabHeight }, tabs[i].label, tabs[i].tooltip);
        tab.index  = i;
        tab.accent = tabs[i].color;
        tab.active = (_target == tabs[i].target);
        if (tabs[i].target == EditTarget::Melee && entry.meleeSlot < 0)
        {
            tab.enabled = false;
            tab.tooltip = "Ranged or contact-only: this character has no melee swing box.";
        }
    }

    // ── Size ──────────────────────────────────────────────────────────────────
    header(kSizeHeaderY, "SIZE  (in game)");
    float y = kSizeHeaderY + kHeaderGap;
    const float rowW = w - (kSmallButton + kGap) * 2.f;
    const float heightPx = _idleArtHeight * _enemy->GetDrawScale();
    const char* heroFixed = "The hero is everyone's size reference, so its in-game size is fixed.";
    {
        UiItem& size = add(Control::SizeRow, UiKind::ValueRow, Rectangle{ x, y, rowW, kRowHeight },
            player ? "1.00x hero" : TextFormat("%.2fx hero", SizeVsHero()),
            player ? heroFixed : "How tall it stands in game, as a multiple of the hero's height. Drag sideways, scroll, or use - and +. Hitboxes and offsets grow with it.");
        size.detail  = TextFormat("%d px tall", (int)roundf(heightPx));
        size.enabled = !player;
    }
    add(Control::SizeMinus, UiKind::Button, Rectangle{ x + rowW + kGap, y, kSmallButton, kRowHeight }, "-",
        player ? heroFixed : "Smaller by 0.05x hero.").enabled = !player;
    add(Control::SizePlus, UiKind::Button, Rectangle{ x + rowW + kGap * 2.f + kSmallButton, y, kSmallButton, kRowHeight }, "+",
        player ? heroFixed : "Bigger by 0.05x hero.").enabled = !player;
    y += kRowHeight + kGap;
    const float autoVsHero = _idleArtHeight * _enemy->GetAutoScale() / Balance::Visual::kPlayerVisibleHeight;
    UiItem& autoSize = add(Control::SizeAuto, UiKind::Button, Rectangle{ x, y, half, kButtonHeight }, "Auto size",
        TextFormat("Back to the size the game picks by itself: %.2fx hero.", autoVsHero));
    autoSize.enabled = !player && fabsf(_enemy->GetDrawScale() - _enemy->GetAutoScale()) > kScaleEpsilon;
    add(Control::Lineup, UiKind::Button, Rectangle{ x + half + kGap, y, half, kButtonHeight }, "Size lineup",
        "Everyone side by side at their in-game size, this character's unsaved size included.");

    // ── Speed ─────────────────────────────────────────────────────────────────
    header(kSpeedHeaderY, "SPEED  (this animation)");
    y = kSpeedHeaderY + kHeaderGap;
    const bool speedSaved = _enemy->GetEditorAnimFrameTime(_animIndex) > 0.f;
    UiItem& speed = add(Control::SpeedRow, UiKind::ValueRow, Rectangle{ x, y, rowW, kRowHeight },
        TextFormat("%.1f fps", 1.f / CurrentFrameTime()),
        "Frames per second for this animation. Drag sideways, scroll, or use - and +.");
    speed.detail = speedSaved ? "custom" : "default";
    add(Control::SpeedMinus, UiKind::Button, Rectangle{ x + rowW + kGap, y, kSmallButton, kRowHeight }, "-", "One frame per second slower.");
    add(Control::SpeedPlus,  UiKind::Button, Rectangle{ x + rowW + kGap * 2.f + kSmallButton, y, kSmallButton, kRowHeight }, "+", "One frame per second faster.");
    y += kRowHeight + kGap;
    UiItem& speedDefault = add(Control::SpeedDefault, UiKind::Button, Rectangle{ x, y, half, kButtonHeight }, "Default speed",
        "Back to the speed the code gives this animation.");
    speedDefault.enabled = speedSaved;

    // ── Animations ────────────────────────────────────────────────────────────
    header(kAnimHeaderY, "ANIMATIONS  (Q / E)");
    y = kAnimHeaderY + kHeaderGap;
    const int animCount = std::min(_enemy->GetEditorAnimCount(), CharacterTuning::kMaxAnims);
    for (int i = 0; i < animCount; i++)
    {
        UiItem& row = add(Control::AnimRow, UiKind::ListRow, Rectangle{ x, y, w, kListRowHeight }, _enemy->GetEditorAnimName(i),
            "Play this animation. Coloured marks: body circle, melee box, sprite offset set for it.");
        row.index  = i;
        row.active = (i == _animIndex);
        row.detail = TextFormat("%d frames", _animFrameCount[i]);
        if (_enemy->GetAnimBodySet(i))  row.flags |= kFlagBody;
        if (_enemy->GetAnimMeleeSet(i)) row.flags |= kFlagMelee;
        if (_enemy->GetAnimDrawSet(i))  row.flags |= kFlagSprite;
        if (IsMeleeAnim(i))
            row.label += "  (melee)";
        y += kListRowStep;
    }
    add(Control::CopyToAll, UiKind::Button, Rectangle{ x, y + kGap, w, kButtonHeight }, "Copy body + sprite offset to all",
        "Copies this animation's body circle and sprite offset to every animation (C).");

    // ── Save / revert / reset / back ──────────────────────────────────────────
    const bool dirty = HasUnsavedChanges();
    UiItem& save = add(Control::Save, UiKind::Button, Rectangle{ x, kSaveY, w, kRowHeight }, dirty ? "Save changes  (Ctrl+S)" : "Saved",
        "Write the tuning file(s) the game reads. Only changed files are written.");
    save.active = dirty;
    save.accent = kGoodColor;
    save.enabled = dirty;
    const float third = (w - kGap * 2.f) / 3.f;
    const float bottomY = kSaveY + kRowHeight + kGap;
    UiItem& revert = add(Control::Revert, UiKind::Button, Rectangle{ x, bottomY, third, kButtonHeight }, "Revert",
        "Back to the last save. Undoable with Ctrl+Z.");
    revert.enabled = dirty;
    const bool hasFile = CharacterTuningStore::Get(entry.displayName) != nullptr;
    add(Control::Reset, UiKind::Button, Rectangle{ x + third + kGap, bottomY, third, kButtonHeight }, "Reset...",
        hasFile ? "Delete the tuning file and go back to code defaults (asks first). Del."
                : "No tuning file yet: this character already plays with its code defaults.").enabled = hasFile;
    add(Control::Back, UiKind::Button, Rectangle{ x + (third + kGap) * 2.f, bottomY, third, kButtonHeight }, "Back",
        "Back to the character list (Esc). Asks first if anything is unsaved.");

    // ── Timeline bar: transport, frames, and the HIT FRAMES / EVENTS lanes ────
    const Rectangle stage = StageRect();
    float tx = stage.x + kPad;
    add(Control::StepBack, UiKind::Button, Rectangle{ tx, kTransportY, kTransportSize, kTransportSize }, "",
        "Previous frame (,). Pauses.");
    tx += kTransportSize + kGap;
    UiItem& play = add(Control::PlayPause, UiKind::Toggle, Rectangle{ tx, kTransportY, kTransportSize, kTransportSize }, "",
        _animPaused ? "Play (Space)." : "Pause (Space).");
    play.active = !_animPaused;
    tx += kTransportSize + kGap;
    add(Control::StepForward, UiKind::Button, Rectangle{ tx, kTransportY, kTransportSize, kTransportSize }, "",
        "Next frame (.). Pauses.");
    tx += kTransportSize + kPad * 2.f;

    add(Control::None, UiKind::Label, Rectangle{ stage.x + kPad, kHitLaneY, tx - stage.x - kPad, kLaneHeight }, "HIT FRAMES", "");
    add(Control::None, UiKind::Label, Rectangle{ stage.x + kPad, kEventLaneY, tx - stage.x - kPad, kLaneHeight }, "EVENTS", "");

    int hitStart = 0, hitEnd = 0, codeStart = 0, codeEnd = 0;
    bool hitAuthored = false;
    const bool melee = IsMeleeAnim(_animIndex) && HitWindowFor(_animIndex, hitStart, hitEnd, hitAuthored);
    CodeHitWindow(_animIndex, codeStart, codeEnd);
    const std::string hitTip = melee
        ? std::string(TextFormat("When this animation's melee box is live. Drag across frames to set it; right-click goes back to the code's frames (%d to %d).",
                                 codeStart + 1, codeEnd + 1))
        : std::string("This animation deals no melee hit.");
    const char* eventTip = "Click a frame to add an FX event there (or to pick one already on it). Drag a mark to move it, right-click to delete. Edit it in the EVENTS tab.";

    const int frames = std::max(1, _enemy->GetEditorFrameCount());
    const float stripW = stage.x + stage.width - kPad - tx;
    const float cellW = std::min(kFrameCellMax, stripW / frames);
    for (int i = 0; i < frames; i++)
    {
        const float cellX = tx + i * cellW;
        UiItem& cell = add(Control::FrameCell, UiKind::FrameCell, Rectangle{ cellX, kTransportY, cellW - 2.f, kTransportSize },
            TextFormat("%d", i + 1), "Jump to this frame. Drag across the strip to scrub.");
        cell.index  = i;
        cell.active = (i == _enemy->GetEditorFrame());

        UiItem& hitCell = add(Control::HitCell, UiKind::HitCell, Rectangle{ cellX, kHitLaneY, cellW - 2.f, kLaneHeight }, "", hitTip);
        hitCell.index   = i;
        hitCell.enabled = melee;
        hitCell.active  = melee && i >= hitStart && i <= hitEnd;
        hitCell.flags   = hitAuthored ? kFlagAuthored : 0u;

        UiItem& eventCell = add(Control::EventCell, UiKind::EventCell, Rectangle{ cellX, kEventLaneY, cellW - 2.f, kLaneHeight }, "", eventTip);
        eventCell.index = i;
    }

    // ── View toolbar over the stage ───────────────────────────────────────────
    float vx = stage.x + kPad;
    auto addTool = [&](Control id, UiKind kind, float width, const std::string& label, const std::string& tooltip) -> UiItem&
    {
        UiItem& item = add(id, kind, Rectangle{ vx, kToolbarY, width, kToolbarHeight }, label, tooltip);
        vx += width + kGap;
        return item;
    };
    UiItem& hero = addTool(Control::HeroToggle, UiKind::Toggle, kHeroToggleW, "Hero",
        "Show the hero beside the character at true in-game size, feet on the same ground (H).");
    hero.active = _showHero && !player;
    if (player)
    {
        hero.enabled = false;
        hero.tooltip = "This is the hero.";
    }
    addTool(Control::SnapToggle, UiKind::Toggle, kSnapToggleW, "Snap 4px",
        "Drags and arrow keys move in 4 px steps, one pixel of hero art (G). Hold Shift for 1 px.").active = _snap;
    addTool(Control::Flip, UiKind::Toggle, kFlipW, "Flip",
        "Face the other way (F). Hitboxes are authored facing right and mirror in game, as here.").active = _enemy->GetEditorFacing() < 0.f;
    addTool(Control::ZoomOut, UiKind::Button, kSmallButton, "-", "Zoom out (mouse wheel over the stage).");
    addTool(Control::ZoomReset, UiKind::Button, kZoomLabelW, TextFormat("%d%%", (int)roundf(_zoom * 100.f)),
        "The zoom. Click to fit the character on screen again (Home). Middle-drag pans.");
    addTool(Control::ZoomIn, UiKind::Button, kSmallButton, "+", "Zoom in (mouse wheel over the stage).");

    // ── Right panel ───────────────────────────────────────────────────────────
    BuildRightPanel(kVirtualWidth - kRightPanelWidth + kRightPad, kRightPanelWidth - kRightPad * 2.f);
}

void CharacterAnimator::BuildRightPanel(float px, float pw)
{
    auto add = [this](Control id, UiKind kind, Rectangle rect, const std::string& label, const std::string& tooltip) -> UiItem&
    {
        UiItem item;
        item.id = id;
        item.kind = kind;
        item.rect = rect;
        item.label = label;
        item.tooltip = tooltip;
        _ui.push_back(std::move(item));
        return _ui.back();
    };
    auto label = [&](float y, const std::string& text) { add(Control::None, UiKind::Label, Rectangle{ px, y, pw, kHeaderGap }, text, ""); };
    auto note = [&](float& y, const std::string& text)
    {
        for (const std::string& line : WrapText(text, kHeaderFont, pw))
        {
            label(y, line);
            y += kHeaderGap;
        }
    };

    const bool player = IsPlayer();
    const std::vector<CharacterTuning::AnimEvent> events = CurrentEvents();
    const float halfTab = (pw - kGap) * 0.5f;
    UiItem& movesTab = add(Control::RightTabMoves, UiKind::Tab, Rectangle{ px, kRightTabY, halfTab, kRightTabHeight },
        player ? std::string("ATTACK") : std::string(TextFormat("MOVES (%d)", (int)_moves.size())),
        player ? "The player's attack fire points and projectiles."
               : "This character's special moves: how big each one's hit area is.");
    movesTab.active = _rightTab == RightTab::Moves;
    movesTab.accent = kAttackColor;
    UiItem& eventTab = add(Control::RightTabEvent, UiKind::Tab, Rectangle{ px + halfTab + kGap, kRightTabY, halfTab, kRightTabHeight },
        TextFormat("EVENTS (%d)", (int)events.size()), "FX, sound and shake events on this animation's frames.");
    eventTab.active = _rightTab == RightTab::Event;
    eventTab.accent = kFxColor;

    float y = kRightContentY;

    // ── Events on this animation ──────────────────────────────────────────────
    if (_rightTab == RightTab::Event)
    {
        label(y, TextFormat("ON %s", _enemy->GetEditorAnimName(_animIndex)));
        y += kHeaderGap;
        if (events.empty())
            note(y, "None yet. Add one below, or click a frame in the EVENTS lane.");
        for (int i = 0; i < (int)events.size(); i++)
        {
            const CharacterTuning::AnimEvent& event = events[i];
            UiItem& row = add(Control::EventRow, UiKind::ListRow, Rectangle{ px, y, pw, kListRowHeight },
                TextFormat("Frame %d   %s", event.frame + 1, kEventKindLabels[(int)event.kind]), "Select this event.");
            row.index  = i;
            row.active = (i == _eventIndex);
            row.accent = EventColor(event.kind);
            row.detailAccent = true;
            row.detail = (event.kind == CharacterTuning::EventKind::Fx)    ? event.fx
                       : (event.kind == CharacterTuning::EventKind::Shake) ? std::string(TextFormat("strength %.1f", EventSizeOf(event)))
                                                                           : std::string("attack sound");
            y += kListRowStep;
        }
        y += kGap;
        const float third = (pw - kGap * 2.f) / 3.f;
        static const char* kAddTips[] = {
            "Add an FX event on the current frame: an FX sheet plays at a spot you place.",
            "Add a sound event on the current frame: this character's attack sound.",
            "Add a shake event on the current frame: the screen shakes." };
        for (int k = 0; k < kEventKindCount; k++)
        {
            UiItem& button = add(Control::EventAdd, UiKind::Button, Rectangle{ px + k * (third + kGap), y, third, kButtonHeight },
                TextFormat("+ %s", kEventKindLabels[k]), kAddTips[k]);
            button.index = k;
        }
        y += kButtonHeight + kGap * 2.f;

        if (_eventIndex < 0 || _eventIndex >= (int)events.size())
            return;
        const CharacterTuning::AnimEvent& event = events[_eventIndex];
        label(y, "SELECTED EVENT");
        y += kHeaderGap;
        for (int k = 0; k < kEventKindCount; k++)
        {
            UiItem& kindTab = add(Control::EventKindTab, UiKind::Tab, Rectangle{ px + k * (third + kGap), y, third, kTabHeight },
                kEventKindLabels[k], "Change what this event does.");
            kindTab.index  = k;
            kindTab.active = ((int)event.kind == k);
            kindTab.accent = EventColor((CharacterTuning::EventKind)k);
        }
        y += kTabHeight + kGap;
        UiItem& frameRow = add(Control::EventField, UiKind::ValueRow, Rectangle{ px, y, pw, kRowHeight },
            TextFormat("Frame %d", event.frame + 1), "Which frame fires it. Drag sideways or scroll; or drag its mark in the EVENTS lane.");
        frameRow.index = 0;
        y += kRowHeight + kGap;
        if (event.kind == CharacterTuning::EventKind::Fx)
        {
            add(Control::EventFxPrev, UiKind::Button, Rectangle{ px, y, kSmallButton, kButtonHeight }, "<", "Previous FX sheet.");
            add(Control::None, UiKind::ListRow, Rectangle{ px + kSmallButton + kGap, y, pw - (kSmallButton + kGap) * 2.f, kButtonHeight },
                event.fx.empty() ? std::string("(no FX)") : event.fx, "The FX sheet it plays (PowerUps/FX_<name>.png). < and > pick another.");
            add(Control::EventFxNext, UiKind::Button, Rectangle{ px + pw - kSmallButton, y, kSmallButton, kButtonHeight }, ">", "Next FX sheet.");
            y += kButtonHeight + kGap;
            UiItem& sizeRow = add(Control::EventField, UiKind::ValueRow, Rectangle{ px, y, pw, kRowHeight },
                TextFormat("Size %.1f", EventSizeOf(event)), "How big the FX plays (its draw scale).");
            sizeRow.index = 1;
            y += kRowHeight + kGap;
            note(y, "Drag the cyan mark on the stage to place it.");
        }
        else if (event.kind == CharacterTuning::EventKind::Shake)
        {
            UiItem& strengthRow = add(Control::EventField, UiKind::ValueRow, Rectangle{ px, y, pw, kRowHeight },
                TextFormat("Strength %.1f", EventSizeOf(event)), "How hard the screen shakes.");
            strengthRow.index = 1;
            y += kRowHeight + kGap;
        }
        else
            note(y, "Plays this character's attack sound.");
        y += kGap;
        add(Control::EventDelete, UiKind::Button, Rectangle{ px, y, pw, kButtonHeight }, "Delete event",
            "Remove it (Del, or right-click its mark in the lane).");
        return;
    }

    // ── The player's attack ───────────────────────────────────────────────────
    if (player)
    {
        add(Control::AttackPrev, UiKind::Button, Rectangle{ px, kAttackSelectY, kSmallButton, kButtonHeight }, "<", "Previous attack ([).");
        UiItem& attackName = add(Control::None, UiKind::ListRow, Rectangle{ px + kSmallButton + kGap, kAttackSelectY, pw - (kSmallButton + kGap) * 2.f, kButtonHeight },
            CurrentAttackLabel(), "The attack being tuned. < and > (or [ and ]) switch.");
        attackName.active = AttackDirty();
        attackName.detail = attackName.active ? "unsaved" : "";
        attackName.accent = kWarnColor;
        attackName.detailAccent = true;
        add(Control::AttackNext, UiKind::Button, Rectangle{ px + pw - kSmallButton, kAttackSelectY, kSmallButton, kButtonHeight }, ">", "Next attack (]).");
        UiItem& preview = add(Control::AttackPreview, UiKind::Toggle, Rectangle{ px, kAttackPreviewY, pw, kButtonHeight }, "Preview shots",
            "Fire the projectile on its cooldown so speed, life and size read at a glance (Space in Attack mode).");
        preview.active = _attackPreviewActive;

        for (int i = 0; i < kAttackFieldCount; i++)
        {
            const bool projectile = i < kProjectileFieldCount;
            const float top = projectile ? kProjectileHeaderY : kLabHeaderY;
            const int   row = projectile ? i : i - kProjectileFieldCount;
            if (row == 0)
                add(Control::None, UiKind::Label, Rectangle{ px, top, pw, kHeaderGap }, projectile ? "PROJECTILE" : "ABILITY LAB", "");
            const AttackField& field = kAttackFields[i];
            UiItem& item = add(Control::AttackField, UiKind::ValueRow,
                Rectangle{ px, top + kHeaderGap + row * kFieldRowStep, pw, kFieldRowHeight },
                TextFormat("%s: %.2f", field.label, _attackTuning.*field.field), field.tooltip);
            item.index  = i;
            item.accent = kAttackColor;
            item.active = (_rowDrag == Control::AttackField && _rowDragIndex == i);
        }
        return;
    }

    // ── Special moves ─────────────────────────────────────────────────────────
    if (_moves.empty())
    {
        note(y, "No special moves on this character. Melee hits: set their frames in the HIT FRAMES lane under the timeline.");
        return;
    }
    for (int i = 0; i < (int)_moves.size(); i++)
    {
        const MoveEdit& move = _moves[i];
        UiItem& row = add(Control::MoveRow, UiKind::ListRow, Rectangle{ px, y, pw, kListRowHeight }, move.info.name,
            "Show this move's hit area on the stage.");
        row.index  = i;
        row.active = (i == _moveIndex);
        row.accent = kAttackColor;
        row.detailAccent = move.tuning.hasHitArea;
        row.detail = move.tuning.hasHitArea ? "custom" : "code size";
        y += kListRowStep;
    }
    if (_moveIndex < 0 || _moveIndex >= (int)_moves.size())
        return;

    const MoveEdit& move = _moves[_moveIndex];
    y += kGap;
    label(y, TextFormat("%s  HIT AREA", ShapeName(move.info.shape)));
    y += kHeaderGap;
    const std::vector<MoveFieldSpec> fields = MoveFields(move.info);
    for (int k = 0; k < (int)fields.size(); k++)
    {
        UiItem& row = add(Control::MoveField, UiKind::ValueRow, Rectangle{ px, y, pw, kRowHeight },
            TextFormat("%s: %.0f px", fields[k].label, MoveValue(move.tuning, move.info, fields[k].field)),
            "Drag sideways, scroll, or drag the orange handles on the stage. Saving makes the game use it, warnings included.");
        row.index  = k;
        row.accent = kAttackColor;
        y += kRowHeight + kGap;
    }
    add(Control::MoveCodeSize, UiKind::Button, Rectangle{ px, y, pw, kButtonHeight }, "Use the coded size",
        "Forget the custom size: the move goes back to its coded hit area.").enabled = move.tuning.hasHitArea;
    y += kButtonHeight + kGap;
    if (move.info.note[0] != 0)
        note(y, move.info.note);

    if (EliteMoves::Find(move.info.key) == nullptr)
        return;
    y += kGap;
    label(y, "TIMING (elite signature)");
    y += kHeaderGap;
    float values[EliteMoves::kRowCount];
    EliteMoves::Values(move.info.key, &move.tuning, values);
    const unsigned mask = EliteMoves::SupportedRowMask(move.info.key);
    for (int r = 0; r < EliteMoves::kRowCount; r++)
    {
        if ((mask & (1u << r)) == 0u)
            continue;
        UiItem& row = add(Control::EliteField, UiKind::ValueRow, Rectangle{ px, y, pw, kFieldRowHeight },
            TextFormat("%s: %.2f", EliteMoves::kRows[r].label, values[r]),
            "The move's timing, as the Attack Library edits it. Drag sideways or scroll.");
        row.index  = r;
        row.accent = kAttackColor;
        y += kFieldRowStep;
    }
}

// =============================================================================
// UI helpers
// =============================================================================
const CharacterAnimator::UiItem* CharacterAnimator::HitUi(const std::vector<UiItem>& items, Vector2 mouse) const
{
    for (auto it = items.rbegin(); it != items.rend(); ++it)
        if (it->kind != UiKind::Label && CheckCollisionPointRec(mouse, it->rect))
            return &*it;
    return nullptr;
}

void CharacterAnimator::UpdateTooltip(const std::vector<UiItem>& items, Vector2 mouse, float dt)
{
    const UiItem* hit = HitUi(items, mouse);
    if (hit == nullptr || hit->tooltip.empty() || IsGestureActive())
    {
        _tooltipControl = Control::None;
        _tooltipTimer = 0.f;
        _tooltipText.clear();
        return;
    }
    if (hit->id != _tooltipControl || hit->index != _tooltipIndex)
    {
        _tooltipControl = hit->id;
        _tooltipIndex = hit->index;
        _tooltipTimer = 0.f;
    }
    _tooltipTimer += dt;
    _tooltipText = hit->tooltip;
    _tooltipAt = mouse;
}

void CharacterAnimator::DrawUi(const std::vector<UiItem>& items) const
{
    const Vector2 mouse = GetVirtualMousePos();
    // The dialog owns the mouse while it's open.
    const bool dialogItems = !_dialogUi.empty() && &items == &_dialogUi;
    const UiItem* hot = (_dialog == Dialog::None || dialogItems) ? HitUi(items, mouse) : nullptr;
    for (const UiItem& item : items)
        DrawUiItem(item, &item == hot);
}

void CharacterAnimator::DrawUiItem(const UiItem& item, bool hovered) const
{
    const Rectangle r = item.rect;
    if (item.kind == UiKind::Label)
    {
        DrawText(item.label.c_str(), (int)r.x, (int)(r.y + (r.height - kHeaderFont) * 0.5f), kHeaderFont, kTextDim);
        return;
    }
    if (item.kind == UiKind::HitCell)
    {
        // Lit cells = frames the melee box is live: solid when set here, faint
        // with an outline when it is the code's own window.
        const bool authored = (item.flags & kFlagAuthored) != 0u;
        Color fill = Fade(kRowFill, item.enabled ? 0.8f : 0.3f);
        if (item.active)
            fill = Fade(kMeleeColor, authored ? kHitLaneSetAlpha : kHitLaneCodeAlpha);
        else if (hovered && item.enabled)
            fill = kRowHot;
        DrawRectangleRec(r, fill);
        if (item.active && !authored)
            DrawRectangleLinesEx(r, 1.f, Fade(kMeleeColor, kHitLaneSetAlpha));
        return;
    }
    if (item.kind == UiKind::EventCell)
    {
        DrawRectangleRec(r, hovered ? kRowHot : Fade(kRowFill, 0.8f));
        return;   // the event marks are drawn over the lane (DrawEventMarkers)
    }

    Color fill = kRowFill;
    Color outline = kRowOutline;
    if (item.active)
    {
        fill    = (item.kind == UiKind::Tab || item.kind == UiKind::Button) ? Fade(item.accent, 0.3f) : kRowActive;
        outline = (item.kind == UiKind::Toggle) ? kGoodColor : item.accent;
        if (item.kind == UiKind::Toggle)
            fill = Fade(kGoodColor, 0.25f);
    }
    else if (hovered && item.enabled)
        fill = kRowHot;
    if (!item.enabled)
        fill = Fade(kRowFill, 0.5f);

    DrawRectangleRounded(r, kRoundness, kSegments, fill);
    DrawRectangleRoundedLines(r, kRoundness, kSegments, hovered && item.enabled && !item.active ? RAYWHITE : outline);
    const Color text = item.enabled ? RAYWHITE : Fade(RAYWHITE, 0.35f);

    // Transport icons, drawn with shapes.
    const Vector2 c{ r.x + r.width * 0.5f, r.y + r.height * 0.5f };
    const float s = r.height * 0.22f;
    if (item.id == Control::PlayPause)
    {
        if (item.active)   // playing: show pause bars
        {
            DrawRectangleRec(Rectangle{ c.x - s, c.y - s, s * 0.7f, s * 2.f }, text);
            DrawRectangleRec(Rectangle{ c.x + s * 0.3f, c.y - s, s * 0.7f, s * 2.f }, text);
        }
        else
            DrawTriangle(Vector2{ c.x - s * 0.8f, c.y - s }, Vector2{ c.x - s * 0.8f, c.y + s }, Vector2{ c.x + s, c.y }, text);
        return;
    }
    if (item.id == Control::StepBack || item.id == Control::StepForward)
    {
        const float dir = (item.id == Control::StepForward) ? 1.f : -1.f;
        const Vector2 tip{ c.x + s * 0.6f * dir, c.y };
        const Vector2 a{ c.x - s * 0.8f * dir, c.y - s };
        const Vector2 b{ c.x - s * 0.8f * dir, c.y + s };
        if (dir > 0.f) DrawTriangle(a, b, tip, text);
        else           DrawTriangle(a, tip, b, text);
        DrawRectangleRec(Rectangle{ tip.x - (dir > 0.f ? 0.f : s * 0.35f), c.y - s, s * 0.35f, s * 2.f }, text);
        return;
    }

    if (item.kind == UiKind::ValueRow || item.kind == UiKind::ListRow)
    {
        const int font = (item.kind == UiKind::ValueRow) ? kRowFont : kButtonFont;
        float textX = r.x + kTextInset;
        if (item.flags != 0u)
        {
            // Body / melee / sprite marks.
            const Color marks[] = { kBodyColor, kMeleeColor, kSpriteColor };
            const unsigned bits[] = { kFlagBody, kFlagMelee, kFlagSprite };
            float mx = r.x + r.width - kTextInset - kFlagSize;
            for (int i = 2; i >= 0; i--)
            {
                if (item.flags & bits[i])
                    DrawRectangleRec(Rectangle{ mx, c.y - kFlagSize * 0.5f, kFlagSize, kFlagSize }, marks[i]);
                mx -= kFlagSize + kGap * 0.5f;
            }
        }
        DrawText(item.label.c_str(), (int)textX, (int)(c.y - font * 0.5f), font, text);
        if (!item.detail.empty())
        {
            const float flagsW = (item.flags != 0u) ? (kFlagSize + kGap * 0.5f) * 3.f + kGap : 0.f;
            const int detailW = MeasureText(item.detail.c_str(), kSmallFont);
            const Color detailColor = item.detailAccent ? item.accent : kTextDim;
            DrawText(item.detail.c_str(), (int)(r.x + r.width - kTextInset - flagsW - detailW),
                     (int)(c.y - kSmallFont * 0.5f), kSmallFont, detailColor);
        }
        if (item.kind == UiKind::ValueRow && item.enabled && item.detail.empty())
        {
            // A small "drag me" hint.
            const char* hint = "< >";
            DrawText(hint, (int)(r.x + r.width - kTextInset - MeasureText(hint, kSmallFont)), (int)(c.y - kSmallFont * 0.5f), kSmallFont, kTextDim);
        }
        return;
    }

    const int font = (item.kind == UiKind::FrameCell) ? kSmallFont : kButtonFont;
    const int labelW = MeasureText(item.label.c_str(), font);
    DrawText(item.label.c_str(), (int)(c.x - labelW * 0.5f), (int)(c.y - font * 0.5f), font, text);
    if (!item.detail.empty())
    {
        const int detailW = MeasureText(item.detail.c_str(), kSmallFont);
        DrawText(item.detail.c_str(), (int)(c.x - detailW * 0.5f), (int)(c.y + font * 0.6f), kSmallFont, kTextDim);
    }
}

void CharacterAnimator::DrawTooltip() const
{
    if (_tooltipText.empty() || _tooltipTimer < kTooltipDelay)
        return;
    const std::vector<std::string> lines = WrapText(_tooltipText, kSmallFont, kTooltipWidth - kTextInset * 2.f);
    float width = 0.f;
    for (const std::string& line : lines)
        width = std::max(width, (float)MeasureText(line.c_str(), kSmallFont));
    width += kTextInset * 2.f;
    const float lineStep = kSmallFont + kGap * 0.5f;
    const float height = lines.size() * lineStep + kTextInset * 1.5f;

    // Below-right of the cursor, kept on screen.
    float x = std::min(_tooltipAt.x + kTooltipOffset, kVirtualWidth - width - kGap);
    float y = _tooltipAt.y + kTooltipOffset;
    if (y + height > kVirtualHeight - kGap)
        y = _tooltipAt.y - kTooltipOffset - height;
    const Rectangle box{ x, y, width, height };
    DrawRectangleRounded(box, kRoundness * 0.4f, kSegments, kTooltipFill);
    DrawRectangleRoundedLines(box, kRoundness * 0.4f, kSegments, kRowOutline);
    float ly = y + kTextInset * 0.75f;
    for (const std::string& line : lines)
    {
        DrawText(line.c_str(), (int)(x + kTextInset), (int)ly, kSmallFont, RAYWHITE);
        ly += lineStep;
    }
}

void CharacterAnimator::DrawStatus() const
{
    if (_statusTimer <= 0.f)
        return;
    const int width = MeasureText(_statusText.c_str(), kTitleFont - 2);
    const bool editing = _screen == Screen::Edit;
    const float top = editing ? kToolbarY + kToolbarHeight + kGap : kVirtualHeight - kStatusBottomGap;
    // Centred over the stage while editing, and kept inside it.
    float left = kVirtualWidth * 0.5f - width * 0.5f - kPad;
    if (editing)
    {
        const Rectangle stage = StageRect();
        left = stage.x + stage.width * 0.5f - width * 0.5f - kPad;
        left = std::max(stage.x + kGap, std::min(left, stage.x + stage.width - width - kPad * 2.f - kGap));
    }
    DrawRectangleRounded(Rectangle{ left, top, width + kPad * 2.f, kRowHeight + kGap },
        kRoundness, kSegments, Fade(BLACK, 0.7f));
    DrawText(_statusText.c_str(), (int)(left + kPad), (int)(top + kGap * 1.5f), kTitleFont - 2, kGoodColor);
}

// =============================================================================
// Drawing
// =============================================================================
void CharacterAnimator::Draw()
{
    if (_screen == Screen::AttackLibrary)
    {
        _attackEditor.Draw();
        return;
    }

    DrawCheckerboard();
    if (_screen == Screen::Select)
        DrawSelectScreen();
    else if (_screen == Screen::Lineup)
        DrawLineupScreen();
    else
        DrawEditScreen();

    DrawStatus();
    if (_dialog != Dialog::None)
        DrawDialog();
    DrawTooltip();
}

void CharacterAnimator::DrawSelectScreen()
{
    const char* title = "CHARACTER ANIMATOR";
    DrawText(title, (int)(kVirtualWidth * 0.5f - MeasureText(title, kBigFont) * 0.5f), (int)kSelectTitleY, kBigFont, RAYWHITE);
    const char* subtitle = "Size, animation speed, sprite alignment and hitboxes for every character";
    DrawText(subtitle, (int)(kVirtualWidth * 0.5f - MeasureText(subtitle, kRowFont + 4) * 0.5f), (int)kSelectSubtitleY, kRowFont + 4, LIGHTGRAY);
    DrawUi(_ui);
    DrawText("ESC: back to menu      Hover anything for help", (int)kPad, (int)(kVirtualHeight - kPad - kRowFont), kRowFont, LIGHTGRAY);
}

void CharacterAnimator::DrawEditScreen()
{
    if (!_enemy)
        return;

    DrawStage();
    DrawSizeGuide();
    DrawOverlays();

    // Panels around the stage: the left panel, the right panel, the timeline.
    const bool player = IsPlayer();
    const Rectangle stage = StageRect();
    DrawRectangle(0, 0, (int)kPanelWidth, kVirtualHeight, kPanelFill);
    DrawRectangle((int)(stage.x + stage.width), 0, (int)kRightPanelWidth, kVirtualHeight, kPanelFill);
    DrawRectangle((int)stage.x, (int)kTimelineTop, (int)stage.width, kVirtualHeight - (int)kTimelineTop, kPanelFill);

    // Title + save state.
    const CharacterEntry& entry = _entries[_selectedIdx];
    DrawText(entry.displayName.c_str(), (int)kPad, (int)kTitleY, kTitleFont, RAYWHITE);
    if (HasUnsavedChanges())
        DrawText("Unsaved changes", (int)kPad, (int)kSubtitleY, kCaptionFont, kWarnColor);
    else if (CharacterTuningStore::Get(entry.displayName) != nullptr)
        DrawText("Saved", (int)kPad, (int)kSubtitleY, kCaptionFont, kGoodColor);
    else
        DrawText("No tuning file yet (code defaults)", (int)kPad, (int)kSubtitleY, kCaptionFont, kTextDim);

    // Timeline caption, and what the hit lane means for this animation.
    const char* caption = TextFormat("%s     frame %d / %d     %.1f fps%s",
        _enemy->GetEditorAnimName(_animIndex), _enemy->GetEditorFrame() + 1, _enemy->GetEditorFrameCount(),
        1.f / CurrentFrameTime(), _animPaused ? "     paused" : "");
    DrawText(caption, (int)(stage.x + kPad), (int)kCaptionY, kCaptionFont, RAYWHITE);
    int hitStart = 0, hitEnd = 0;
    bool authored = false;
    const char* hint = (IsMeleeAnim(_animIndex) && HitWindowFor(_animIndex, hitStart, hitEnd, authored))
        ? TextFormat("Melee box live on frames %d-%d (%s)%s", hitStart + 1, hitEnd + 1,
                     authored ? "set here" : "the code's", MeleeLiveNow() ? "  -  live now" : "")
        : "No melee hit on this animation.";
    DrawText(hint, (int)(stage.x + kPad), (int)kLaneHintY, kSmallFont, kTextDim);

    DrawText("Hover any control for help.", (int)kPad, (int)kHelpY, kSmallFont, kTextDim);
    DrawText("Wheel zoom, middle-drag pan", (int)kPad, (int)(kHelpY + kHelpLineStep), kSmallFont, kTextDim);
    DrawText("Arrows nudge, Ctrl+Z undo", (int)kPad, (int)(kHelpY + kHelpLineStep * 2.f), kSmallFont, kTextDim);

    if (player && _rightTab == RightTab::Moves)
    {
        const float px = kVirtualWidth - kRightPanelWidth + kRightPad;
        DrawText(TextFormat("File: attacktuning_%s.txt", CurrentAttackKey().c_str()), (int)px, (int)kAttackFileY, kSmallFont - 1, kTextDim);
        float y = kAttackInfoY;
        DrawText(TextFormat("Fire point: %.0f, %.0f", _attackTuning.fireForward, _attackTuning.fireHeight), (int)px, (int)y, kSmallFont, kAttackColor);
        y += kInfoLineStep;
        DrawText(TextFormat("FX origin: %.0f, %.0f", _attackTuning.fxForward, _attackTuning.fxHeight), (int)px, (int)y, kSmallFont, kFxColor);
        y += kInfoLineStep;
        DrawText(_attackTuning.hasBox ? "Damage area: from the Attack Library" : "Damage area: not tuned yet", (int)px, (int)y, kSmallFont, Fade(kMeleeColor, 0.85f));
    }

    DrawUi(_ui);
    DrawEventMarkers();
}

void CharacterAnimator::DrawStage()
{
    const Rectangle stage = StageRect();
    const bool player = IsPlayer();

    // Ground line at the character's feet.
    const float groundY = ToScreen(Vector2{ 0.f, _groundRel }).y;
    if (_charBounds.height > 0.f)
        DrawLineEx(Vector2{ stage.x, groundY }, Vector2{ stage.x + stage.width, groundY }, 2.f, Fade(WHITE, 0.18f));

    Camera2D camera = StageCamera();
    if (_shakeTimer > 0.f)
    {
        // A Shake event wobbles the picture so its timing reads.
        const float wobble = _shakeStrength * (_shakeTimer / Balance::AnimEvents::kShakeSeconds);
        camera.offset.x += sinf((float)GetTime() * kShakeJiggleHz) * wobble;
    }
    BeginMode2D(camera);
    if (_showHero && !player && _hero && _heroBounds.height > 0.f)
    {
        _hero->Teleport(_heroPos);
        _hero->DrawEnemy(Vector2{ 0.f, 0.f });
    }
    // The live enemy draws itself exactly like in-game.
    _enemy->DrawEnemy(Vector2{ 0.f, 0.f });
    DrawFxPreviews();
    EndMode2D();
}

void CharacterAnimator::DrawSizeGuide()
{
    if (_charBounds.height <= 0.f)
        return;

    // A bracket beside the character from its feet to the top of its art: the
    // height the game sizes it by, against the hero's.
    const bool player = IsPlayer();
    const float heightPx = _idleArtHeight * _enemy->GetDrawScale();
    const float bracketX = ToScreen(Vector2{ _charBounds.x + _charBounds.width + kBracketGap, 0.f }).x;
    const float bottom = ToScreen(Vector2{ 0.f, _groundRel }).y;
    const float top = ToScreen(Vector2{ 0.f, _groundRel - heightPx }).y;
    const Color color = player ? kHeroColor : RAYWHITE;
    DrawLineEx(Vector2{ bracketX, top }, Vector2{ bracketX, bottom }, 2.f, Fade(color, 0.8f));
    DrawLineEx(Vector2{ bracketX - kBracketTick, top }, Vector2{ bracketX + kBracketTick, top }, 2.f, Fade(color, 0.8f));
    DrawLineEx(Vector2{ bracketX - kBracketTick, bottom }, Vector2{ bracketX + kBracketTick, bottom }, 2.f, Fade(color, 0.8f));
    const char* size = player ? "hero (reference)" : TextFormat("%.2fx hero", SizeVsHero());
    DrawText(size, (int)(bracketX + kBracketTick + kGap), (int)top, kRowFont, color);
    DrawText(TextFormat("%d px tall", (int)roundf(heightPx)), (int)(bracketX + kBracketTick + kGap), (int)(top + kRowFont + 2.f), kSmallFont, kTextDim);

    if (player || !_showHero || _heroBounds.height <= 0.f)
        return;

    // The hero's head height carried across, so a glance compares the two.
    const float heroTop = ToScreen(Vector2{ 0.f, _groundRel - _heroHeightPx }).y;
    const float heroLeft = ToScreen(Vector2{ _heroPos.x + _heroBounds.x, 0.f }).x;
    DrawLineEx(Vector2{ heroLeft, heroTop }, Vector2{ bracketX + kBracketTick, heroTop }, 1.f, Fade(kHeroColor, 0.45f));
    const float heroCentreX = ToScreen(Vector2{ _heroPos.x + _heroBounds.x + _heroBounds.width * 0.5f, 0.f }).x;
    const char* label = TextFormat("HERO  %d px", (int)roundf(_heroHeightPx));
    DrawText(label, (int)(heroCentreX - MeasureText(label, kSmallFont) * 0.5f), (int)(heroTop - kLabelLift), kSmallFont, kHeroColor);
}

void CharacterAnimator::DrawOverlays()
{
    const Vector2 origin = ToScreen(Vector2{ 0.f, 0.f });
    // Everything is authored facing right and mirrors with the preview.
    const float facing = (_enemy->GetEditorFacing() >= 0.f) ? 1.f : -1.f;
    auto toScreen = [&](Vector2 rel) { return ToScreen(Vector2{ rel.x * facing, rel.y }); };

    // Sprite-pixel anchor. This moves only the rendered animation; collision
    // stays fixed so visual offsets can be corrected without moving gameplay.
    {
        const Vector2 anchor = toScreen(_enemy->GetAnimDrawOffsetValue(_animIndex));
        const bool active = _target == EditTarget::Sprite;
        const Color color = active ? kSpriteColor : Fade(kSpriteColor, 0.38f);
        DrawLineEx(origin, anchor, 2.f, Fade(color, 0.55f));
        DrawHandle(anchor, active && _dragHandle == kHandleCentre, active ? GOLD : color);
        DrawText("SPRITE PIXELS", (int)anchor.x + 14, (int)anchor.y + 12, kSmallFont, color);
    }

    DrawHurtbox();

    // ── Body circle (cyan) — per animation ────────────────────────────────────
    {
        Vector2 bodyOffset;
        float bodyRadius;
        GetEffectiveBodyCircle(bodyOffset, bodyRadius);

        const Vector2 centre = toScreen(bodyOffset);
        const float radius = bodyRadius * _zoom;
        const bool active = (_target == EditTarget::Body);
        const bool explicitData = _enemy->GetAnimBodySet(_animIndex);
        const Color color = active ? kBodyColor : Fade(kBodyColor, 0.45f);

        // Dashed ring when this anim has no explicit circle yet (showing the
        // fallback) — solid ring once authored.
        if (explicitData)
        {
            DrawCircleLinesV(centre, radius, color);
            DrawCircleLinesV(centre, radius - 1.f, Fade(color, 0.6f));
        }
        else
        {
            for (int i = 0; i < kCircleDashes; i += 2)
            {
                const float a0 = (float)i / kCircleDashes * 2.f * PI;
                const float a1 = (float)(i + 1) / kCircleDashes * 2.f * PI;
                DrawLineV(Vector2{ centre.x + cosf(a0) * radius, centre.y + sinf(a0) * radius },
                          Vector2{ centre.x + cosf(a1) * radius, centre.y + sinf(a1) * radius }, color);
            }
        }

        DrawHandle(centre, active && _dragHandle == kHandleCentre, active ? GOLD : Fade(kBodyColor, 0.45f));
        DrawHandle(Vector2{ centre.x + radius, centre.y },
            active && _dragHandle == kHandleRadius, active ? GOLD : Fade(kBodyColor, 0.45f));
    }

    // ── Melee box (red) — on every animation the game reads one from; filled
    // while the hit window is live on this frame ──
    if (MeleeEditableNow())
    {
        const Rectangle rel = GetEffectiveMeleeRel();
        Rectangle mirrored = rel;
        if (facing < 0.f)
            mirrored.x = -(rel.x + rel.width);
        const Vector2 topLeft = ToScreen(Vector2{ mirrored.x, mirrored.y });
        const Rectangle screenRect{ topLeft.x, topLeft.y, rel.width * _zoom, rel.height * _zoom };
        const bool active = (_target == EditTarget::Melee);
        const bool live = MeleeLiveNow();
        const Color color = active ? kMeleeColor : Fade(kMeleeColor, 0.5f);
        if (live)
            DrawRectangleRec(screenRect, Fade(kMeleeColor, kLiveFillAlpha));
        DrawRectangleLinesEx(screenRect, active ? 3.f : 1.5f, live ? color : Fade(color, 0.45f));

        const Vector2 corners[5] = {
            toScreen(Vector2{ rel.x, rel.y }),
            toScreen(Vector2{ rel.x + rel.width, rel.y }),
            toScreen(Vector2{ rel.x, rel.y + rel.height }),
            toScreen(Vector2{ rel.x + rel.width, rel.y + rel.height }),
            toScreen(Vector2{ rel.x + rel.width * 0.5f, rel.y + rel.height * 0.5f })
        };
        for (int i = 0; i < 5; i++)
            DrawHandle(corners[i], active && _dragHandle == i, active ? GOLD : Fade(kMeleeColor, 0.45f));

        const bool own = _enemy->GetAnimMeleeSet(_animIndex);
        const char* label = live ? "MELEE  live this frame" : "MELEE  not live this frame";
        DrawText(label, (int)screenRect.x, (int)(screenRect.y - kSmallFont - kGap * 0.5f), kSmallFont, live ? kMeleeColor : Fade(kMeleeColor, 0.6f));
        if (!own && _animIndex != Enemy::kAnimSlotAttack && _enemy->GetAnimMeleeSet(Enemy::kAnimSlotAttack))
            DrawText(TextFormat("(using %s's box until you move it)", _enemy->GetEditorAnimName(Enemy::kAnimSlotAttack)),
                     (int)screenRect.x, (int)(screenRect.y + screenRect.height + kGap * 0.5f), kSmallFont, Fade(kMeleeColor, 0.6f));
    }

    if (PlayerAttackEditorAvailable())
        DrawAttackOverlay();
    DrawMoveOverlay();
    DrawEventOverlay();

    // World-pos crosshair.
    DrawLineV(Vector2{ origin.x - 12.f, origin.y }, Vector2{ origin.x + 12.f, origin.y }, Fade(WHITE, 0.5f));
    DrawLineV(Vector2{ origin.x, origin.y - 12.f }, Vector2{ origin.x, origin.y + 12.f }, Fade(WHITE, 0.5f));
}

void CharacterAnimator::DrawAttackOverlay()
{
    const Vector2 origin = ToScreen(Vector2{ 0.f, 0.f });
    const Vector2 fire = ToScreen(Vector2{ _attackTuning.fireForward, _attackTuning.fireHeight });
    const Vector2 fxOrigin = ToScreen(Vector2{ _attackTuning.fxForward, _attackTuning.fxHeight });
    const float radius = std::max(1.f, _attackTuning.projRadius) * _zoom;
    const float range = std::max(kMinAimRange, _attackTuning.aimRange) * _zoom;
    const float angle = _attackTuning.previewAngle * DEG2RAD;
    const Vector2 aimDir{ cosf(angle), sinf(angle) };
    const Vector2 aimEnd = Vector2Add(fire, Vector2Scale(aimDir, range));
    const bool active = _target == EditTarget::Attack;
    const Color color = active ? kAttackColor : Fade(kAttackColor, 0.42f);

    // Damage boxes are authored in the Attack FX + Hitboxes library but shown
    // here against the live character, exposing fire-point/hitbox mismatches.
    if (_attackTuning.hasBox && _attackTuning.w > 0.f && _attackTuning.h > 0.f)
    {
        const Vector2 topLeft = ToScreen(Vector2{ _attackTuning.x - _attackTuning.w * 0.5f, _attackTuning.y - _attackTuning.h * 0.5f });
        const Rectangle damageArea{ topLeft.x, topLeft.y, _attackTuning.w * _zoom, _attackTuning.h * _zoom };
        DrawRectangleRec(damageArea, Fade(kMeleeColor, active ? 0.14f : 0.07f));
        DrawRectangleLinesEx(damageArea, active ? 2.5f : 1.5f, active ? kMeleeColor : Fade(kMeleeColor, 0.4f));
        DrawText("SAVED DAMAGE AREA", (int)damageArea.x + 8, (int)damageArea.y + 8,
            kSmallFont - 1, active ? kMeleeColor : Fade(kMeleeColor, 0.5f));
    }

    DrawLineEx(origin, fire, 2.f, Fade(color, 0.55f));
    DrawLineEx(fire, aimEnd, 3.f, Fade(color, 0.35f));
    DrawCircleLinesV(fire, radius, color);
    DrawCircleV(fire, kFireDotRadius, color);
    DrawHandle(fire, active && _dragHandle == kHandleCentre, active ? GOLD : Fade(kAttackColor, 0.75f));
    DrawHandle(Vector2{ fire.x + radius, fire.y }, active && _dragHandle == kHandleRadius, active ? GOLD : Fade(kAttackColor, 0.75f));

    const Color fxColor = active ? kFxColor : Fade(kFxColor, 0.43f);
    DrawLineEx(origin, fxOrigin, 2.f, Fade(fxColor, 0.55f));
    DrawCircleLinesV(fxOrigin, kFxRingRadius, fxColor);
    DrawHandle(fxOrigin, active && _dragHandle == kHandleFxOrigin,
        active && _dragHandle == kHandleFxOrigin ? GOLD : fxColor);
    DrawText("FX ORIGIN", (int)fxOrigin.x + 18, (int)fxOrigin.y - 26, kSmallFont, fxColor);

    // Target geometry and three test dummies make range/area/chain tuning
    // readable without leaving the animator.
    const float area = _attackTuning.areaRadius * _zoom;
    DrawCircleV(aimEnd, area, Fade(color, 0.07f));
    DrawCircleLinesV(aimEnd, area, Fade(color, 0.55f));
    const Vector2 side{ -aimDir.y, aimDir.x };
    const float effectHalf = _attackTuning.effectLength * 0.5f * _zoom;
    DrawLineEx(Vector2Subtract(aimEnd, Vector2Scale(side, effectHalf)),
               Vector2Add(aimEnd, Vector2Scale(side, effectHalf)),
               std::max(2.f, _attackTuning.effectWidth * _zoom), Fade(color, 0.08f));
    const Vector2 dummies[3] = {
        Vector2Add(aimEnd, Vector2Scale(side, -area * kDummySpread)),
        aimEnd,
        Vector2Add(aimEnd, Vector2Scale(side, area * kDummySpread)) };
    for (int i = 0; i < 3; ++i)
    {
        DrawCircleV(dummies[i], kDummyRadius * _zoom, Fade(kDummyFill, 0.35f));
        DrawCircleLinesV(dummies[i], kDummyRadius * _zoom, kDummyLine);
        DrawText("DUMMY", (int)dummies[i].x - 27, (int)dummies[i].y - 9, kSmallFont - 2, RAYWHITE);
    }

    if (_attackPreviewActive)
    {
        const float visualR = std::max(kMinPreviewShot, _attackTuning.projRadius * std::max(0.1f, _attackTuning.projScale)) * _zoom;
        for (float age : _attackPreviewShots)
        {
            const float t = std::min(age, _attackTuning.projLifetime);
            const Vector2 p = Vector2Add(fire, Vector2Scale(aimDir, _attackTuning.projSpeed * t * _zoom));
            const float fadeT = 1.f - std::clamp(t / std::max(kMinCadence, _attackTuning.projLifetime), 0.f, 1.f);
            DrawCircleV(p, visualR, Fade(color, 0.12f + 0.2f * fadeT));
            DrawCircleLinesV(p, visualR, Fade(color, 0.35f + 0.55f * fadeT));
        }
    }

    DrawText("FIRE POINT", (int)fire.x + 14, (int)fire.y - 30, kCaptionFont, color);
}

void CharacterAnimator::DrawLineupScreen()
{
    auto toScreen = [this](Vector2 world) {
        return Vector2{ _lineupOrigin.x + world.x * _lineupZoom, _lineupOrigin.y + world.y * _lineupZoom };
    };

    // A ground line under each line of characters.
    float lastGround = -1.f;
    for (const LineupItem& item : _lineup)
    {
        if (fabsf(item.ground - lastGround) < 1.f)
            continue;
        lastGround = item.ground;
        const float y = toScreen(Vector2{ 0.f, item.ground }).y;
        DrawLineEx(Vector2{ 0.f, y }, Vector2{ (float)kVirtualWidth, y }, 2.f, Fade(WHITE, 0.15f));
    }

    Camera2D camera{};
    camera.target = Vector2{ kVirtualWidth * 0.5f, kVirtualHeight * 0.5f };
    camera.offset = _lineupOrigin;
    camera.zoom   = _lineupZoom;
    BeginMode2D(camera);
    for (const LineupItem& item : _lineup)
    {
        item.instance->Teleport(item.position);
        item.instance->DrawEnemy(Vector2{ 0.f, 0.f });
    }
    EndMode2D();

    // Labels under each character, and a highlight on the one under the mouse.
    for (int i = 0; i < (int)_lineup.size(); i++)
    {
        const LineupItem& item = _lineup[i];
        const Vector2 topLeft = toScreen(Vector2{ item.position.x + item.bounds.x, item.position.y + item.bounds.y });
        const Rectangle rect{ topLeft.x, topLeft.y, item.bounds.width * _lineupZoom, item.bounds.height * _lineupZoom };
        const bool hero = item.entry < 0 || _entries[item.entry].category == Category::Hero;
        const bool editing = _enemy && item.instance == _enemy.get();
        if (i == _lineupHover)
            DrawRectangleLinesEx(rect, 2.f, GOLD);

        const float centreX = rect.x + rect.width * 0.5f;
        float y = rect.y + rect.height + kLineupLabelGap;
        const char* name = hero ? "HERO" : _entries[item.entry].displayName.c_str();
        const Color nameColor = hero ? kHeroColor : (editing ? kWarnColor : RAYWHITE);
        DrawText(name, (int)(centreX - MeasureText(name, kSmallFont) * 0.5f), (int)y, kSmallFont, nameColor);
        y += kLineupLineStep;
        const char* size = hero ? "reference" : TextFormat("%.2fx hero", item.sizeVsHero);
        DrawText(size, (int)(centreX - MeasureText(size, kSmallFont) * 0.5f), (int)y, kSmallFont, kTextDim);
        y += kLineupLineStep;
        const char* px = TextFormat("%d px", (int)roundf(item.heightPx));
        DrawText(px, (int)(centreX - MeasureText(px, kSmallFont) * 0.5f), (int)y, kSmallFont, kTextDim);
        if (editing && HasUnsavedChanges())
        {
            y += kLineupLineStep;
            DrawText("(unsaved)", (int)(centreX - MeasureText("(unsaved)", kSmallFont) * 0.5f), (int)y, kSmallFont, kWarnColor);
        }
    }

    // Row headers above each row's first line.
    const char* rowNames[] = { "HERO + ENEMIES", "ELITES", "BOSSES" };
    constexpr int kRowNameCount = (int)(sizeof(rowNames) / sizeof(rowNames[0]));
    for (const LineupItem& item : _lineup)
    {
        if (!item.firstInRow || item.row < 0 || item.row >= kRowNameCount)
            continue;
        const Vector2 at = toScreen(Vector2{ item.position.x + item.bounds.x, item.lineTop });
        DrawText(rowNames[item.row], (int)at.x, (int)(at.y - kLineupHeaderLift), kCaptionFont, kTextDim);
    }

    const char* title = "SIZE LINEUP - everyone at in-game size";
    DrawText(title, (int)(kVirtualWidth * 0.5f - MeasureText(title, kTitleFont) * 0.5f), (int)kPad, kTitleFont, RAYWHITE);
    const char* help = "Click a character to edit it.   Wheel zoom   Drag to pan   Esc back";
    DrawText(help, (int)(kVirtualWidth * 0.5f - MeasureText(help, kSmallFont) * 0.5f), (int)(kPad + kTitleFont + kGap), kSmallFont, kTextDim);
    DrawUi(_ui);
}

// =============================================================================
// Animation events (Unity "Animation Events" / Unreal "Anim Notifies"): an FX,
// the attack sound or a shake when the animation reaches a frame. They live on
// the character, so undo, saving and the unsaved check cover them.
// =============================================================================
std::vector<CharacterTuning::AnimEvent> CharacterAnimator::CurrentEvents() const
{
    if (!_enemy)
        return {};
    return _enemy->GetAnimEvents(_animIndex);
}

void CharacterAnimator::SetCurrentEvents(const std::vector<CharacterTuning::AnimEvent>& events)
{
    _enemy->SetAnimEvents(_animIndex, events);
}

void CharacterAnimator::AddEvent(CharacterTuning::EventKind kind, int frame)
{
    std::vector<CharacterTuning::AnimEvent> events = CurrentEvents();
    if ((int)events.size() >= CharacterTuning::kMaxEventsPerAnim)
    {
        SetStatus(TextFormat("An animation holds at most %d events", CharacterTuning::kMaxEventsPerAnim));
        return;
    }
    CharacterTuning::AnimEvent event;
    event.kind  = kind;
    event.frame = std::clamp(frame, 0, std::max(0, _enemy->GetEditorFrameCount() - 1));
    if (kind == CharacterTuning::EventKind::Fx)
    {
        // A new FX starts where a hit would land: the melee box's centre, or in front.
        Vector2 at{ kDefaultFxForward, 0.f };
        if (MeleeEditableNow())
        {
            const Rectangle melee = GetEffectiveMeleeRel();
            at = Vector2{ melee.x + melee.width * 0.5f, melee.y + melee.height * 0.5f };
        }
        event.x = at.x;
        event.y = at.y;
        if (!_fxCatalog.empty())
            event.fx = _fxCatalog.front();
    }
    events.push_back(event);
    SetCurrentEvents(events);
    SelectEvent((int)events.size() - 1);
    SetStatus(TextFormat("%s event added on frame %d", kEventKindLabels[(int)kind], event.frame + 1));
}

void CharacterAnimator::DeleteEvent(int index)
{
    std::vector<CharacterTuning::AnimEvent> events = CurrentEvents();
    if (index < 0 || index >= (int)events.size())
        return;
    events.erase(events.begin() + index);
    SetCurrentEvents(events);
    _eventIndex = events.empty() ? -1 : std::min(index, (int)events.size() - 1);
    SetStatus("Event deleted");
}

void CharacterAnimator::SelectEvent(int index)
{
    _eventIndex = index;
    _rightTab = RightTab::Event;
    _target = EditTarget::Event;
}

void CharacterAnimator::ScanFxCatalog()
{
    // Every PowerUps/FX_<name>.png sheet, by name.
    _fxCatalog.clear();
    const std::string folder = AssetFolderPath("PowerUps");
    std::error_code error;
    for (const auto& file : std::filesystem::directory_iterator(folder, error))
    {
        const std::string name = file.path().filename().string();
        const std::string prefix = "FX_";
        const std::string suffix = ".png";
        if (name.size() <= prefix.size() + suffix.size() || name.rfind(prefix, 0) != 0 ||
            name.compare(name.size() - suffix.size(), suffix.size(), suffix) != 0)
            continue;
        _fxCatalog.push_back(name.substr(prefix.size(), name.size() - prefix.size() - suffix.size()));
    }
    std::sort(_fxCatalog.begin(), _fxCatalog.end());
}

const Texture2D* CharacterAnimator::FxSheet(const std::string& stem)
{
    if (stem.empty())
        return nullptr;
    auto it = _fxSheets.find(stem);
    if (it == _fxSheets.end())
    {
        Texture2D sheet{};
        const std::string path = AssetPath(TextFormat("PowerUps/FX_%s.png", stem.c_str()));
        if (FileExists(path.c_str()))
            sheet = LoadTexture(path.c_str());
        it = _fxSheets.emplace(stem, sheet).first;   // a missing sheet is remembered too
    }
    return it->second.id != 0 ? &it->second : nullptr;
}

void CharacterAnimator::UpdateFxPreviews(float dt)
{
    // Previews play through their sheet once, like the game's.
    for (FxPreview& preview : _fxPreviews)
        preview.time += dt;
    _fxPreviews.erase(std::remove_if(_fxPreviews.begin(), _fxPreviews.end(), [this](const FxPreview& preview)
    {
        const Texture2D* sheet = FxSheet(preview.stem);
        const int frames = sheet ? sheet->width / Balance::AnimEvents::kFxCellSize : 0;
        return frames <= 0 || preview.time >= frames * Balance::AnimEvents::kFxFrameSeconds;
    }), _fxPreviews.end());
    if (_shakeTimer > 0.f)
        _shakeTimer = std::max(0.f, _shakeTimer - dt);

    // Fire the events the animation just reached, as the game would.
    const int frame = _enemy->GetEditorFrame();
    if (_animIndex == _previewSlot && frame == _previewFrame)
        return;
    const bool sameAnimation = (_animIndex == _previewSlot);
    const int lastFrame = _previewFrame;
    _previewSlot  = _animIndex;
    _previewFrame = frame;
    const float facing = (_enemy->GetEditorFacing() >= 0.f) ? 1.f : -1.f;
    for (const CharacterTuning::AnimEvent& event : CurrentEvents())
    {
        if (!AnimEvents::Reached(event.frame, sameAnimation, lastFrame, frame))
            continue;
        switch (event.kind)
        {
        case CharacterTuning::EventKind::Fx:
            _fxPreviews.push_back(FxPreview{ event.fx, Vector2{ event.x * facing, event.y }, EventSizeOf(event), 0.f });
            break;
        case CharacterTuning::EventKind::Sound:
            if (!_animPaused)
                _enemy->PlayAttackSound();
            break;
        case CharacterTuning::EventKind::Shake:
            _shakeTimer = Balance::AnimEvents::kShakeSeconds;
            _shakeStrength = EventSizeOf(event);
            break;
        }
    }
}

void CharacterAnimator::DrawFxPreviews()
{
    // Inside the stage camera: the character's position is the canvas centre.
    const Vector2 centre{ kVirtualWidth * 0.5f, kVirtualHeight * 0.5f };
    const float cell = (float)Balance::AnimEvents::kFxCellSize;
    for (const FxPreview& preview : _fxPreviews)
    {
        const Texture2D* sheet = FxSheet(preview.stem);
        if (sheet == nullptr)
            continue;
        const int frames = std::max(1, sheet->width / Balance::AnimEvents::kFxCellSize);
        const int frame = std::min(frames - 1, (int)(preview.time / Balance::AnimEvents::kFxFrameSeconds));
        const float width = cell * preview.size;
        const float height = sheet->height * preview.size;
        DrawTexturePro(*sheet, Rectangle{ frame * cell, 0.f, cell, (float)sheet->height },
                       Rectangle{ centre.x + preview.rel.x, centre.y + preview.rel.y, width, height },
                       Vector2{ width * 0.5f, height * 0.5f }, 0.f, WHITE);
    }
}

void CharacterAnimator::DrawEventMarkers() const
{
    // One small mark per event in its frame's EVENTS cell, side by side.
    const std::vector<CharacterTuning::AnimEvent> events = CurrentEvents();
    std::vector<int> drawnOnFrame(std::max(1, _enemy->GetEditorFrameCount()), 0);
    for (int i = 0; i < (int)events.size(); i++)
    {
        const CharacterTuning::AnimEvent& event = events[i];
        if (event.frame < 0 || event.frame >= (int)drawnOnFrame.size())
            continue;
        for (const UiItem& item : _ui)
        {
            if (item.id != Control::EventCell || item.index != event.frame)
                continue;
            const float x = item.rect.x + kGap * 0.5f + drawnOnFrame[event.frame] * (kMarkerSize + kGap * 0.5f);
            const float y = item.rect.y + (item.rect.height - kMarkerSize) * 0.5f;
            DrawRectangleRec(Rectangle{ x, y, kMarkerSize, kMarkerSize }, EventColor(event.kind));
            if (i == _eventIndex)
                DrawRectangleLinesEx(Rectangle{ x - 2.f, y - 2.f, kMarkerSize + 4.f, kMarkerSize + 4.f }, 2.f, WHITE);
            drawnOnFrame[event.frame]++;
            break;
        }
    }
}

void CharacterAnimator::DrawEventOverlay()
{
    // The selected FX event: its sheet's first frame where it will play.
    if (_rightTab != RightTab::Event)
        return;
    const std::vector<CharacterTuning::AnimEvent> events = CurrentEvents();
    if (_eventIndex < 0 || _eventIndex >= (int)events.size())
        return;
    const CharacterTuning::AnimEvent& event = events[_eventIndex];
    if (event.kind != CharacterTuning::EventKind::Fx)
        return;
    const float facing = (_enemy->GetEditorFacing() >= 0.f) ? 1.f : -1.f;
    const Vector2 at = ToScreen(Vector2{ event.x * facing, event.y });
    if (const Texture2D* sheet = FxSheet(event.fx))
    {
        const float cell = (float)Balance::AnimEvents::kFxCellSize;
        const float width = cell * EventSizeOf(event) * _zoom;
        const float height = sheet->height * EventSizeOf(event) * _zoom;
        DrawTexturePro(*sheet, Rectangle{ 0.f, 0.f, cell, (float)sheet->height },
                       Rectangle{ at.x, at.y, width, height }, Vector2{ width * 0.5f, height * 0.5f }, 0.f,
                       Fade(WHITE, kFxGhostAlpha));
    }
    const bool active = _target == EditTarget::Event;
    DrawHandle(at, active && _dragHandle == kHandleCentre, active ? GOLD : kFxColor);
    DrawText(TextFormat("FX %s  frame %d", event.fx.c_str(), event.frame + 1), (int)at.x + 14, (int)at.y + 12, kSmallFont, kFxColor);
}

// =============================================================================
// Special moves: each boss/elite move's hit area (attacktuning_<key>.txt). An
// unsaved move keeps its coded size; the first edit copies that in.
// =============================================================================
void CharacterAnimator::LoadMoves()
{
    _moves.clear();
    _moveIndex = -1;
    if (!_enemy || IsPlayer())
        return;
    for (int i = 0; i < _enemy->GetEditorMoveCount(); i++)
    {
        MoveEdit move;
        move.info = _enemy->GetEditorMove(i);
        if (move.info.key == nullptr || move.info.key[0] == 0)
            continue;
        if (const AttackTuning* saved = AttackTuningStore::Get(move.info.key))
            move.tuning = *saved;
        _moves.push_back(move);
    }
    _moveIndex = -1;   // nothing on the stage until a move is picked
}

std::vector<AttackTuning> CharacterAnimator::MoveTunings() const
{
    std::vector<AttackTuning> tunings;
    for (const MoveEdit& move : _moves)
        tunings.push_back(move.tuning);
    return tunings;
}

std::string CharacterAnimator::MovesText(const std::vector<AttackTuning>& moves) const
{
    std::string text;
    for (const AttackTuning& tuning : moves)
        text += AttackTuningStore::Serialize(tuning) + "--\n";
    return text;
}

bool CharacterAnimator::MovesDirty() const
{
    return !_moves.empty() && MovesText(MoveTunings()) != _saved.movesText;
}

void CharacterAnimator::SelectMove(int index)
{
    if (index < 0 || index >= (int)_moves.size())
        return;
    _moveIndex = index;
    _rightTab = RightTab::Moves;
    _target = EditTarget::Move;
    const int slot = _moves[index].info.animSlot;
    if (slot >= 0 && slot < _enemy->GetEditorAnimCount())
        SelectAnim(slot);
    _target = EditTarget::Move;
}

void CharacterAnimator::MakeMoveCustom(MoveEdit& move) const
{
    if (move.tuning.hasHitArea)
        return;
    move.tuning.hasHitArea = true;
    for (float AttackTuning::* field : kAllMoveFields)
        move.tuning.*field = MoveDefault(move.info, field);
}

void CharacterAnimator::DrawMoveOverlay()
{
    if (IsPlayer() || _rightTab != RightTab::Moves || _moveIndex < 0 || _moveIndex >= (int)_moves.size())
        return;
    const MoveEdit& move = _moves[_moveIndex];
    const float facing = (_enemy->GetEditorFacing() >= 0.f) ? 1.f : -1.f;
    auto toScreen = [&](Vector2 rel) { return ToScreen(Vector2{ rel.x * facing, rel.y }); };
    auto value = [&](float AttackTuning::* field) { return MoveValue(move.tuning, move.info, field); };
    const bool active = _target == EditTarget::Move;
    const Color line = active ? kAttackColor : Fade(kAttackColor, 0.6f);
    const Color fill = Fade(kAttackColor, kMoveFillAlpha);
    const Color handle = active ? GOLD : Fade(kAttackColor, 0.75f);
    Vector2 labelAt = ToScreen(Vector2{ 0.f, 0.f });

    switch (move.info.shape)
    {
    case Enemy::MoveShape::Circle:
    {
        const Vector2 centre = toScreen(Vector2{ value(&AttackTuning::hitForward), 0.f });
        const float radius = value(&AttackTuning::hitRadius);
        DrawCircleV(centre, radius * _zoom, fill);
        DrawCircleLinesV(centre, radius * _zoom, line);
        DrawHandle(toScreen(Vector2{ value(&AttackTuning::hitForward) + radius, 0.f }), active && _dragHandle == kHandleRadius, handle);
        if (move.info.forward != 0.f)
            DrawHandle(centre, active && _dragHandle == kHandleCentre, handle);
        labelAt = Vector2{ centre.x - radius * _zoom, centre.y - radius * _zoom };
        break;
    }
    case Enemy::MoveShape::Ring:
    {
        const Vector2 centre = ToScreen(Vector2{ 0.f, 0.f });
        const float inner = value(&AttackTuning::hitInner);
        const float outer = value(&AttackTuning::hitRadius);
        DrawRing(centre, inner * _zoom, outer * _zoom, 0.f, 360.f, kRingSegments, fill);
        DrawCircleLinesV(centre, inner * _zoom, line);
        DrawCircleLinesV(centre, outer * _zoom, line);
        DrawHandle(toScreen(Vector2{ outer, 0.f }), active && _dragHandle == kHandleRadius, handle);
        DrawHandle(toScreen(Vector2{ inner, 0.f }), active && _dragHandle == kHandleInner, handle);
        labelAt = Vector2{ centre.x - outer * _zoom, centre.y - outer * _zoom };
        break;
    }
    case Enemy::MoveShape::Lane:
    {
        const float length = value(&AttackTuning::hitLength);
        const float width = value(&AttackTuning::hitWidth);
        const Vector2 a = toScreen(Vector2{ 0.f, -width });
        const Vector2 b = toScreen(Vector2{ length, width });
        const Rectangle rect{ std::min(a.x, b.x), std::min(a.y, b.y), fabsf(b.x - a.x), fabsf(b.y - a.y) };
        DrawRectangleRec(rect, fill);
        DrawRectangleLinesEx(rect, 2.f, line);
        DrawHandle(toScreen(Vector2{ length, 0.f }), active && _dragHandle == kHandleLaneEnd, handle);
        DrawHandle(toScreen(Vector2{ length * 0.5f, width }), active && _dragHandle == kHandleLaneSide, handle);
        labelAt = Vector2{ rect.x, rect.y };
        break;
    }
    case Enemy::MoveShape::Box:
    {
        const Rectangle rel = MoveBoxRel(move.tuning, move.info);
        Rectangle mirrored = rel;
        if (facing < 0.f)
            mirrored.x = -(rel.x + rel.width);
        const Vector2 topLeft = ToScreen(Vector2{ mirrored.x, mirrored.y });
        const Rectangle rect{ topLeft.x, topLeft.y, rel.width * _zoom, rel.height * _zoom };
        DrawRectangleRec(rect, fill);
        DrawRectangleLinesEx(rect, 2.f, line);
        const Vector2 corners[5] = {
            toScreen(Vector2{ rel.x, rel.y }), toScreen(Vector2{ rel.x + rel.width, rel.y }),
            toScreen(Vector2{ rel.x, rel.y + rel.height }), toScreen(Vector2{ rel.x + rel.width, rel.y + rel.height }),
            toScreen(Vector2{ rel.x + rel.width * 0.5f, rel.y + rel.height * 0.5f }) };
        for (int i = 0; i < 5; i++)
            DrawHandle(corners[i], active && _dragHandle == i, handle);
        labelAt = Vector2{ rect.x, rect.y };
        break;
    }
    }
    DrawText(TextFormat("%s  (%s)", move.info.name, move.tuning.hasHitArea ? "custom" : "code size"),
             (int)labelAt.x, (int)(labelAt.y - kSmallFont - kGap * 0.5f), kSmallFont, line);
}

void CharacterAnimator::DrawHurtbox()
{
    // What the player's attacks hit: the exact rectangle the game tests, the
    // body square plus its padding. The character stands at the origin.
    const Rectangle hurt = _enemy->GetHitCollisionRec();
    const Vector2 topLeft = ToScreen(Vector2{ hurt.x, hurt.y });
    const Rectangle rect{ topLeft.x, topLeft.y, hurt.width * _zoom, hurt.height * _zoom };
    const bool active = _target == EditTarget::Body;
    const Color color = active ? kHurtColor : Fade(kHurtColor, kHurtIdleAlpha);
    DrawDashedRect(rect, color);
    const bool padding = _enemy->UsesHurtPadding();
    const char* label = padding ? TextFormat("HURTBOX  padding %.0f x %.0f", _enemy->GetHurtPadX(), _enemy->GetHurtPadY())
                                : "HURTBOX  (the hero's: Idle circle, no padding)";
    DrawText(label, (int)rect.x, (int)(rect.y + rect.height + kGap * 0.5f), kSmallFont, color);
    if (active && padding)
    {
        DrawHandle(Vector2{ rect.x + rect.width, rect.y + rect.height * 0.5f }, _dragHandle == kHandlePadX, kHurtColor);
        DrawHandle(Vector2{ rect.x + rect.width * 0.5f, rect.y + rect.height }, _dragHandle == kHandlePadY, kHurtColor);
    }
}

// =============================================================================
// The right panel's value rows: moves, elite timing, events.
// =============================================================================
float CharacterAnimator::PanelValue(Control id, int index)
{
    if ((id == Control::MoveField || id == Control::EliteField) && _moveIndex >= 0 && _moveIndex < (int)_moves.size())
    {
        const MoveEdit& move = _moves[_moveIndex];
        if (id == Control::MoveField)
        {
            const std::vector<MoveFieldSpec> fields = MoveFields(move.info);
            return (index >= 0 && index < (int)fields.size()) ? MoveValue(move.tuning, move.info, fields[index].field) : 0.f;
        }
        float values[EliteMoves::kRowCount];
        EliteMoves::Values(move.info.key, &move.tuning, values);
        return (index >= 0 && index < EliteMoves::kRowCount) ? values[index] : 0.f;
    }
    if (id == Control::EventField)
    {
        const std::vector<CharacterTuning::AnimEvent> events = CurrentEvents();
        if (_eventIndex >= 0 && _eventIndex < (int)events.size())
            return (index == 0) ? (float)events[_eventIndex].frame : EventSizeOf(events[_eventIndex]);
    }
    return 0.f;
}

float CharacterAnimator::PanelValuePerPixel(Control id, int index) const
{
    if (id == Control::MoveField)
        return kMovePerPx;
    if (id == Control::EliteField)
        return (index >= 0 && index < EliteMoves::kRowCount) ? EliteMoves::kRows[index].perPixel : 0.f;
    if (id == Control::EventField)
        return (index == 0) ? kEventFramePerPx : kEventSizePerPx;
    return 0.f;
}

void CharacterAnimator::SetPanelValue(Control id, int index, float value)
{
    if ((id == Control::MoveField || id == Control::EliteField) && _moveIndex >= 0 && _moveIndex < (int)_moves.size())
    {
        MoveEdit& move = _moves[_moveIndex];
        if (id == Control::MoveField)
        {
            const std::vector<MoveFieldSpec> fields = MoveFields(move.info);
            if (index < 0 || index >= (int)fields.size())
                return;
            MakeMoveCustom(move);
            move.tuning.*fields[index].field = std::clamp(roundf(value), fields[index].minValue, fields[index].maxValue);
            // A ring's inside never passes its outside.
            if (move.info.shape == Enemy::MoveShape::Ring)
                move.tuning.hitInner = std::min(move.tuning.hitInner, move.tuning.hitRadius);
            return;
        }
        if (index < 0 || index >= EliteMoves::kRowCount)
            return;
        // Timing saves all seven values together, like the Attack Library.
        float values[EliteMoves::kRowCount];
        EliteMoves::Values(move.info.key, &move.tuning, values);
        values[index] = std::clamp(value, EliteMoves::kRows[index].minValue, EliteMoves::kRows[index].maxValue);
        move.tuning.hasSignature      = true;
        move.tuning.telegraphTime     = values[0];
        move.tuning.activeTime        = values[1];
        move.tuning.recoveryTime      = values[2];
        move.tuning.signatureCooldown = values[3];
        move.tuning.travelDistance    = values[4];
        move.tuning.phaseSpeedMult    = values[5];
        move.tuning.phaseCooldownMult = values[6];
        return;
    }
    if (id == Control::EventField)
    {
        std::vector<CharacterTuning::AnimEvent> events = CurrentEvents();
        if (_eventIndex < 0 || _eventIndex >= (int)events.size())
            return;
        CharacterTuning::AnimEvent& event = events[_eventIndex];
        if (index == 0)
            event.frame = std::clamp((int)roundf(value), 0, std::max(0, _enemy->GetEditorFrameCount() - 1));
        else
            event.size = std::clamp(value, kMinEventSize,
                                    event.kind == CharacterTuning::EventKind::Shake ? kMaxShake : kMaxFxSize);
        SetCurrentEvents(events);
    }
}

void CharacterAnimator::TestSetMoveRadius(float radius)
{
    if (_moveIndex < 0 || _moveIndex >= (int)_moves.size())
        return;
    MakeMoveCustom(_moves[_moveIndex]);
    _moves[_moveIndex].tuning.hitRadius = radius;
    CommitIfChanged();
}

void CharacterAnimator::DrawHandle(Vector2 screenPos, bool hot, Color color) const
{
    DrawRectangle((int)(screenPos.x - kHandleSize * 0.5f), (int)(screenPos.y - kHandleSize * 0.5f),
        (int)kHandleSize, (int)kHandleSize, hot ? WHITE : Fade(color, 0.9f));
    DrawRectangleLines((int)(screenPos.x - kHandleSize * 0.5f), (int)(screenPos.y - kHandleSize * 0.5f),
        (int)kHandleSize, (int)kHandleSize, BLACK);
}
