#include "DayNight.h"
#include "GameBalance.h"

#include <cmath>
#include <iterator>

namespace DayNight
{
    namespace
    {
        constexpr float kPlainTolerance = 0.001f;   // this close to "unchanged" counts as unchanged
        constexpr float kChannelMax     = 255.f;

        float Wrap(float hours)
        {
            const float day = Balance::DayNight::kHoursPerDay;
            const float wrapped = std::fmod(hours, day);
            return wrapped < 0.f ? wrapped + day : wrapped;
        }

        float Lerp(float a, float b, float t) { return a + (b - a) * t; }

        // Smoothstep: a look eases out of one keyframe and into the next.
        float Ease(float t) { return t * t * (3.f - 2.f * t); }

        void ToUnit(const Balance::DayNight::Rgb& rgb, float out[3])
        {
            out[0] = rgb.r / kChannelMax;
            out[1] = rgb.g / kChannelMax;
            out[2] = rgb.b / kChannelMax;
        }

        Look FromKeyframe(const Balance::DayNight::Keyframe& keyframe)
        {
            Look look;
            ToUnit(keyframe.tint, look.tint);
            look.saturation = keyframe.saturation;
            look.lights = keyframe.lights;
            ToUnit(keyframe.haze, look.haze);
            look.hazeAmount = keyframe.hazeAmount;
            return look;
        }

        Look Blend(const Look& a, const Look& b, float t)
        {
            Look look;
            for (int i = 0; i < 3; ++i)
            {
                look.tint[i] = Lerp(a.tint[i], b.tint[i], t);
                look.haze[i] = Lerp(a.haze[i], b.haze[i], t);
            }
            look.saturation = Lerp(a.saturation, b.saturation, t);
            look.lights = Lerp(a.lights, b.lights, t);
            look.hazeAmount = Lerp(a.hazeAmount, b.hazeAmount, t);
            return look;
        }
    }

    bool Look::IsPlain() const
    {
        for (int i = 0; i < 3; ++i)
            if (tint[i] < 1.f - kPlainTolerance) return false;
        return saturation >= 1.f - kPlainTolerance && lights <= kPlainTolerance &&
               hazeAmount <= kPlainTolerance;
    }

    float AdvanceClock(float hours, float seconds, float speed)
    {
        return Wrap(hours + seconds * speed * Balance::DayNight::kHoursPerDay /
                            Balance::DayNight::kDayLengthSeconds);
    }

    Look LookAt(float hours)
    {
        using Balance::DayNight::kLooks;
        const int count = (int)std::size(kLooks);
        hours = Wrap(hours);

        // The last keyframe at or before this hour; before the first one, it is
        // the last one, from the evening before.
        int previous = count - 1;
        for (int i = 0; i < count; ++i)
            if (kLooks[i].hour <= hours) previous = i;
        const int next = (previous + 1) % count;

        const float day = Balance::DayNight::kHoursPerDay;
        const float from = kLooks[previous].hour;
        float to = kLooks[next].hour;
        float at = hours;
        if (to <= from) to += day;   // the span runs past midnight
        if (at < from) at += day;
        const float span = to - from;
        const float t = span > 0.f ? Ease((at - from) / span) : 0.f;
        return Blend(FromKeyframe(kLooks[previous]), FromKeyframe(kLooks[next]), t);
    }

    bool IsOutdoorBiome(Biome biome)
    {
        switch (biome)
        {
        case Biome::Forest:
        case Biome::Graveyard:
        case Biome::Jungle:
        case Biome::LostCity:
        case Biome::Wastelands:
            return true;
        case Biome::AncientCastle:
        case Biome::Caverns:
        case Biome::DemonsInsides:
        case Biome::DreamRealm:
        case Biome::TheSanctuary:
            return false;
        }
        return false;
    }
}
