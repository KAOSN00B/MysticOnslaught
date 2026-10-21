#include "DayNight.h"
#include "GameBalance.h"

#include <cassert>
#include <cmath>
#include <iterator>

namespace
{
    constexpr float kTolerance = 0.001f;
    // Neighbouring minutes must look almost the same: no jumps anywhere in the
    // day, past midnight included.
    constexpr float kMinute = 1.f / 60.f;
    constexpr float kMaxStepPerMinute = 0.02f;

    bool Near(float a, float b, float tolerance = kTolerance) { return std::fabs(a - b) <= tolerance; }

    float Difference(const DayNight::Look& a, const DayNight::Look& b)
    {
        float most = 0.f;
        for (int i = 0; i < 3; ++i)
        {
            most = std::fmax(most, std::fabs(a.tint[i] - b.tint[i]));
            most = std::fmax(most, std::fabs(a.haze[i] - b.haze[i]));
        }
        most = std::fmax(most, std::fabs(a.saturation - b.saturation));
        most = std::fmax(most, std::fabs(a.lights - b.lights));
        most = std::fmax(most, std::fabs(a.hazeAmount - b.hazeAmount));
        return most;
    }
}

int main()
{
    using namespace Balance::DayNight;

    // The clock: one day of play is one full turn; it always wraps into [0, 24).
    assert(Near(DayNight::AdvanceClock(8.f, kDayLengthSeconds), 8.f));
    assert(Near(DayNight::AdvanceClock(8.f, kDayLengthSeconds * 0.5f), 20.f));
    assert(Near(DayNight::AdvanceClock(23.f, kDayLengthSeconds / kHoursPerDay * 2.f), 1.f));
    assert(Near(DayNight::AdvanceClock(6.f, kDayLengthSeconds / kHoursPerDay, 3.f), 9.f));   // fast-forward
    for (int i = 0; i < 1000; ++i)
    {
        const float hour = DayNight::AdvanceClock(23.9f, i * 7.3f, 17.f);
        assert(hour >= 0.f && hour < kHoursPerDay);
    }

    // The keyframes are in order and inside the day.
    const int count = (int)std::size(kLooks);
    assert(count > 0);
    for (int i = 0; i < count; ++i)
    {
        assert(kLooks[i].hour >= 0.f && kLooks[i].hour < kHoursPerDay);
        if (i > 0) assert(kLooks[i].hour > kLooks[i - 1].hour);
    }

    // Each keyframe's hour shows exactly that keyframe.
    for (int i = 0; i < count; ++i)
    {
        const DayNight::Look look = DayNight::LookAt(kLooks[i].hour);
        assert(Near(look.saturation, kLooks[i].saturation));
        assert(Near(look.lights, kLooks[i].lights));
        assert(Near(look.tint[0], kLooks[i].tint.r / 255.f));
    }

    // Midday is the art as drawn (no pass at all); midnight is dark and lit.
    assert(DayNight::LookAt(12.f).IsPlain());
    const DayNight::Look midnight = DayNight::LookAt(0.f);
    assert(!midnight.IsPlain());
    assert(midnight.lights > 0.99f);
    assert(midnight.tint[0] < 1.f && midnight.tint[2] > midnight.tint[0]);   // blue, not just dark
    assert(DayNight::LookAt(24.f + 12.f).IsPlain());                        // hours wrap
    assert(DayNight::LookAt(-1.f).lights > 0.99f);                          // 23:00

    // Smooth all day long, past midnight too.
    for (float hour = 0.f; hour < kHoursPerDay; hour += kMinute)
        assert(Difference(DayNight::LookAt(hour), DayNight::LookAt(hour + kMinute)) <= kMaxStepPerMinute);

    // Outdoors: the open-sky biomes. Indoors keep their own art.
    assert(DayNight::IsOutdoorBiome(Biome::Forest));
    assert(DayNight::IsOutdoorBiome(Biome::Graveyard));
    assert(DayNight::IsOutdoorBiome(Biome::Jungle));
    assert(DayNight::IsOutdoorBiome(Biome::LostCity));
    assert(DayNight::IsOutdoorBiome(Biome::Wastelands));
    assert(!DayNight::IsOutdoorBiome(Biome::AncientCastle));
    assert(!DayNight::IsOutdoorBiome(Biome::Caverns));
    assert(!DayNight::IsOutdoorBiome(Biome::DemonsInsides));
    assert(!DayNight::IsOutdoorBiome(Biome::DreamRealm));
    assert(!DayNight::IsOutdoorBiome(Biome::TheSanctuary));
    return 0;
}
