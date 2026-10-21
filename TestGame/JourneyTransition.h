#pragma once

#include <algorithm>
#include <cmath>
#include <exception>
#include <functional>
#include <string>
#include <utility>
#include <vector>

// GPU work stays on the main thread. Each preparation stage runs only after
// a fully covered frame was presented, and yields another frame before the next.
class JourneyTransition
{
public:
    using Step = std::function<void()>;

    // Restore the execution flag even if destination recovery itself throws.
    static Step GuardStep(bool& executing, Step action, Step onFailure)
    {
        return [&executing, action = std::move(action), onFailure = std::move(onFailure)] {
            struct RestoreFlag
            {
                bool& flag;
                bool previous;
                ~RestoreFlag() { flag = previous; }
            } restore{executing, executing};
            executing = true;
            try { if (action) action(); }
            catch (...) { if (onFailure) onFailure(); throw; }
        };
    }

    bool Begin(std::string label, std::vector<Step> steps, Step onFinished = {})
    {
        if (Active() || steps.empty()) return false;
        _label = std::move(label);
        _steps = std::move(steps);
        _onFinished = std::move(onFinished);
        _next = 0;
        _elapsed = 0.f;
        _presented = false;
        _error.clear();
        _phase = Phase::Cover;
        return true;
    }

    void Update(float dt)
    {
        if (!std::isfinite(dt) || dt < 0.f) dt = 0.f;
        dt = std::min(dt, 0.1f); // a loading spike must not skip the reveal
        if (_phase == Phase::Cover)
        {
            _elapsed = std::min(_elapsed + dt, kCoverSeconds);
            if (_elapsed >= kCoverSeconds) _phase = Phase::Preparing;
        }
        else if (_phase == Phase::Preparing && _presented)
        {
            _presented = false;
            // Move before invoking: no dangling reference if a callback tries
            // to start a second journey (Begin rejects while this is active).
            Step step = std::move(_steps[_next++]);
            try { if (step) step(); }
            catch (const std::exception& e) { _error = *e.what() ? e.what() : "Unable to prepare the destination."; }
            catch (...) { _error = "Unable to prepare the destination."; }
            if (_next == _steps.size() || !_error.empty())
            {
                _steps.clear();
                Step finish = std::move(_onFinished);
                try { if (finish) finish(); }
                catch (const std::exception& e) { if (_error.empty()) _error = *e.what() ? e.what() : "Unable to finish the destination."; }
                catch (...) { if (_error.empty()) _error = "Unable to finish the destination."; }
                _phase = Phase::Reveal;
                _elapsed = 0.f;
            }
        }
        else if (_phase == Phase::Reveal)
        {
            _elapsed = std::min(_elapsed + dt, kRevealSeconds);
            if (_elapsed >= kRevealSeconds) _phase = Phase::Idle;
        }
    }

    void Presented() { if (_phase == Phase::Preparing) _presented = true; }
    bool Active() const { return _phase != Phase::Idle; }
    bool Revealing() const { return _phase == Phase::Reveal; }
    bool Preparing() const { return _phase == Phase::Preparing; }
    float Alpha() const
    {
        if (_phase == Phase::Idle) return 0.f;
        if (_phase == Phase::Preparing) return 1.f;
        const float t = _elapsed / (_phase == Phase::Cover ? kCoverSeconds : kRevealSeconds);
        const float eased = t * t * (3.f - 2.f * t);
        return _phase == Phase::Cover ? eased : 1.f - eased;
    }
    const std::string& Label() const { return _label; }
    const std::string& Error() const { return _error; }

private:
    enum class Phase { Idle, Cover, Preparing, Reveal };
    static constexpr float kCoverSeconds = 0.20f;
    static constexpr float kRevealSeconds = 0.32f;
    Phase _phase = Phase::Idle;
    std::string _label, _error;
    std::vector<Step> _steps;
    Step _onFinished;
    std::size_t _next = 0;
    float _elapsed = 0.f;
    bool _presented = false;
};
