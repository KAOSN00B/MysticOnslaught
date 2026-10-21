#include "Engine.h"
#include "TextureCache.h"
#include <cmath>
#include <utility>

bool Engine::QueueJourney(const std::string& label, std::vector<JourneyTransition::Step> steps,
                          std::function<void()> onFailure)
{
    if (_journey.Active() || steps.empty()) return false;
    std::vector<JourneyTransition::Step> guarded;
    for (auto& step : steps)
    {
        guarded.push_back(JourneyTransition::GuardStep(_journeyExecuting,
            [action = std::move(step), label] {
                const double started = GetTime();
                const int loadsBefore = TextureCache::Get().LoadCount();
                action();
                TraceLog(LOG_INFO, "JOURNEY [%s]: preparation stage %.1f ms, %d tile-sheet uploads", label.c_str(),
                         (GetTime() - started) * 1000.0, TextureCache::Get().LoadCount() - loadsBefore);
            },
            [this, onFailure] {
                _dungeonFadeState = DungeonFadeState::None;
                _dungeonFadePendingAction = nullptr;
                try { if (onFailure) onFailure(); else _gameState = GameState::Menu; }
                catch (...) { _gameState = GameState::Menu; throw; }
            }));
    }
    auto finish = [this] {
        // The coordinator now owns reveal and simulation gating. Never layer
        // the legacy fades or carried combat effects over a fresh arrival.
        _fadeInTimer = 0.f;
        _dungeonFadeState = DungeonFadeState::None;
        _dungeonFadeAlpha = 0.f;
        _dungeonFadePendingAction = nullptr;
        _hitStopTimer = 0.f;
        _slowMoTimer = 0.f;
        _shakeTimer = 0.f;
        _shakeOffset = {};
        _player.ConsumeCastRequest();
        _player.SetTouchDirection({0.f, 0.f});
    };
    return _journey.Begin(label, std::move(guarded), std::move(finish));
}

void Engine::DrawJourneyOverlay() const
{
    if (!_journey.Active()) return;
    const float alpha = _journey.Alpha();
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Fade(Color{8, 7, 14, 255}, alpha));
    if (alpha < 0.5f) return;
    const LetterboxTransform lb = GetLetterboxTransform();
    Camera2D ui{};
    ui.offset = {lb.offsetX, lb.offsetY};
    ui.zoom = lb.scale;
    BeginMode2D(ui);
    const float ink = (alpha - 0.5f) * 2.f;
    const int titleSize = 44;
    const char* title = _journey.Label().c_str();
    const Color gold = Fade(Color{220, 199, 147, 255}, ink);
    DrawText(title, (kVirtualWidth - MeasureText(title, titleSize)) / 2,
             kVirtualHeight / 2 - 36, titleSize, gold);
    DrawLine(kVirtualWidth / 2 - 110, kVirtualHeight / 2 + 28,
             kVirtualWidth / 2 + 110, kVirtualHeight / 2 + 28, Fade(gold, 0.4f * ink));
    for (int i = 0; i < 3; ++i)
    {
        const float pulse = 0.3f + 0.7f * (0.5f + 0.5f * std::sin((float)GetTime() * 3.f - i * 0.9f));
        DrawPoly({kVirtualWidth * 0.5f + (i - 1) * 22.f, kVirtualHeight * 0.5f + 60.f},
                 4, 3.f, 0.f, Fade(gold, ink * pulse));
    }
    EndMode2D();
}
