#include "CharacterTuning.h"
#include "AttackTuning.h"

#include <cassert>
#include <cmath>
#include <string>

// The Character Animator's file formats: what Save writes, Parse reads back,
// and the text doubles as the animator's "has this changed?" check. Also the
// rule for when an animation event fires.
namespace
{
    constexpr float kTolerance = 0.01f;   // the files keep two or three decimals

    bool Near(float a, float b) { return std::fabs(a - b) <= kTolerance; }

    CharacterTuning Sample()
    {
        CharacterTuning tuning;
        tuning.hasScale = true;
        tuning.scale    = 5.25f;
        tuning.sizeSet  = true;
        tuning.animFrameTime[2] = 0.125f;
        tuning.animBody[0]  = { true, 4.f, 36.f, 44.f };
        tuning.animMelee[2] = { true, Rectangle{ 60.f, -8.f, 52.f, 76.f } };
        tuning.animDraw[3]  = { true, -12.f, 8.f };
        tuning.animHit[2]   = { true, 3, 5 };
        CharacterTuning::AnimEvent fx;
        fx.kind = CharacterTuning::EventKind::Fx; fx.frame = 3; fx.x = 80.f; fx.y = -20.f; fx.size = 4.5f; fx.fx = "BossClawSwipe";
        CharacterTuning::AnimEvent shake;
        shake.kind = CharacterTuning::EventKind::Shake; shake.frame = 4; shake.size = 7.f;
        CharacterTuning::AnimEvent sound;
        sound.kind = CharacterTuning::EventKind::Sound; sound.frame = 2;
        tuning.animEvents[2] = { fx, shake, sound };
        tuning.hasHurtPad = true;
        tuning.hurtPadX = 20.f;
        tuning.hurtPadY = 12.f;
        return tuning;
    }
}

int main()
{
    // Round trip: every field the animator writes comes back.
    const CharacterTuning original = Sample();
    const std::string text = CharacterTuningStore::Serialize(original);
    CharacterTuning read;
    CharacterTuningStore::Parse(text, read);
    assert(read.hasScale && Near(read.scale, original.scale));
    assert(read.sizeSet);
    assert(Near(read.animFrameTime[2], original.animFrameTime[2]));
    assert(read.animBody[0].set && Near(read.animBody[0].x, 4.f) && Near(read.animBody[0].y, 36.f) && Near(read.animBody[0].radius, 44.f));
    assert(read.animMelee[2].set && Near(read.animMelee[2].rect.x, 60.f) && Near(read.animMelee[2].rect.width, 52.f));
    assert(read.animDraw[3].set && Near(read.animDraw[3].x, -12.f) && Near(read.animDraw[3].y, 8.f));
    assert(!read.animBody[1].set && !read.animMelee[0].set && !read.animDraw[0].set);
    assert(read.animHit[2].set && read.animHit[2].start == 3 && read.animHit[2].end == 5 && !read.animHit[0].set);
    assert(read.animEvents[2].size() == 3 && read.animEvents[0].empty());
    const CharacterTuning::AnimEvent& fx = read.animEvents[2][0];
    assert(fx.kind == CharacterTuning::EventKind::Fx && fx.frame == 3 && Near(fx.x, 80.f) && Near(fx.y, -20.f) && Near(fx.size, 4.5f) && fx.fx == "BossClawSwipe");
    assert(read.animEvents[2][1].kind == CharacterTuning::EventKind::Shake && read.animEvents[2][1].frame == 4 && Near(read.animEvents[2][1].size, 7.f));
    assert(read.animEvents[2][2].kind == CharacterTuning::EventKind::Sound && read.animEvents[2][2].fx.empty());
    assert(read.hasHurtPad && Near(read.hurtPadX, 20.f) && Near(read.hurtPadY, 12.f));

    // Writing what was read gives the same text, so "unchanged" compares equal.
    assert(CharacterTuningStore::Serialize(read) == text);

    // Automatic size: no size_set line, and an old file without it reads as automatic.
    CharacterTuning automatic = original;
    automatic.sizeSet = false;
    const std::string automaticText = CharacterTuningStore::Serialize(automatic);
    assert(automaticText.find("size_set") == std::string::npos);
    assert(automaticText != text);
    CharacterTuning old;
    CharacterTuningStore::Parse("scale=4.000\nanim_0_body_r=30.00\n", old);
    assert(old.hasScale && !old.sizeSet && Near(old.scale, 4.f));
    assert(!old.hasHurtPad && !old.animHit[2].set && old.animEvents[2].empty());

    // Windows line endings and junk lines are tolerated.
    CharacterTuning crlf;
    CharacterTuningStore::Parse("scale=6.200\r\nsize_set=1\r\nnot a key\r\nanim_frametime_1=0.1000\r\n", crlf);
    assert(crlf.hasScale && Near(crlf.scale, 6.2f) && crlf.sizeSet && Near(crlf.animFrameTime[1], 0.1f));

    // Out-of-range animation slots and malformed events are ignored.
    CharacterTuning outOfRange;
    CharacterTuningStore::Parse("anim_99_body_x=1.00\nanim_frametime_42=0.5\nanim_2_event_0=bogus:1:0:0:0:-\n"
                                "anim_2_event_1=fx:1:2\nanim_2_event_2=fx:-3:0:0:0:-\n", outOfRange);
    for (int i = 0; i < CharacterTuning::kMaxAnims; i++)
        assert(!outOfRange.animBody[i].set && outOfRange.animFrameTime[i] == 0.f && outOfRange.animEvents[i].empty());

    // At most kMaxEventsPerAnim events per animation are kept.
    std::string many;
    for (int n = 0; n < CharacterTuning::kMaxEventsPerAnim + 3; n++)
        many += "anim_1_event_" + std::to_string(n) + "=sound:" + std::to_string(n) + ":0:0:0:-\n";
    CharacterTuning capped;
    CharacterTuningStore::Parse(many, capped);
    assert((int)capped.animEvents[1].size() == CharacterTuning::kMaxEventsPerAnim);

    // When events fire: a new animation (or a restart) fires from frame 0;
    // otherwise exactly the frames passed since last time, none skipped.
    assert(AnimEvents::Reached(0, false, 5, 0));                  // new animation, frame 0
    assert(AnimEvents::Reached(2, false, -1, 3));                 // new animation joined at frame 3
    assert(!AnimEvents::Reached(4, false, -1, 3));
    assert(AnimEvents::Reached(3, true, 2, 3));                   // stepped onto it
    assert(AnimEvents::Reached(4, true, 2, 5));                   // a long step does not skip it
    assert(!AnimEvents::Reached(2, true, 2, 5));                  // already fired at frame 2
    assert(AnimEvents::Reached(1, true, 5, 1));                   // the loop started over
    assert(!AnimEvents::Reached(4, true, 5, 1));

    // Attack tuning: a special move's hit area round-trips, and a file without
    // one keeps the coded size.
    AttackTuning area;
    area.hasHitArea = true;
    area.hitRadius = 240.f; area.hitInner = 90.f; area.hitForward = 150.f;
    area.hitLength = 460.f; area.hitWidth = 52.f;
    area.hitX = 30.f; area.hitY = -10.f; area.hitW = 120.f; area.hitH = 80.f;
    area.hasSignature = true; area.telegraphTime = 0.8f;
    const std::string areaText = AttackTuningStore::Serialize(area);
    AttackTuning areaRead;
    AttackTuningStore::Parse(areaText, areaRead);
    assert(areaRead.hasHitArea && Near(areaRead.hitRadius, 240.f) && Near(areaRead.hitInner, 90.f) && Near(areaRead.hitForward, 150.f));
    assert(Near(areaRead.hitLength, 460.f) && Near(areaRead.hitWidth, 52.f));
    assert(Near(areaRead.hitX, 30.f) && Near(areaRead.hitY, -10.f) && Near(areaRead.hitW, 120.f) && Near(areaRead.hitH, 80.f));
    assert(areaRead.hasSignature && Near(areaRead.telegraphTime, 0.8f));
    assert(AttackTuningStore::Serialize(areaRead) == areaText);
    AttackTuning noArea;
    AttackTuningStore::Parse("box_x=10.000\nbox_w=200.000\n", noArea);
    assert(!noArea.hasHitArea && noArea.hasBox);
    assert(AttackTuningStore::Serialize(noArea).find("hit_") == std::string::npos);

    return 0;
}
