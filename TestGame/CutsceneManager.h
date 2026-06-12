#pragma once
#include "CutsceneAction.h"
#include "DialogueBox.h"
#include "InputPrompts.h"
#include <string>
#include <vector>

// ── CutsceneManager ───────────────────────────────────────────────────────────
// Drives a cutscene from a plain array of CutsceneActions.
//
// How to use:
//   1. Define a static CutsceneAction[] array somewhere (e.g. at the top of
//      Engine.cpp) using the factory helpers from CutsceneAction.h.
//   2. Call Play(array, count) to start the scene.
//   3. Each frame: call Update(dt, ...) and Draw(...) while IsActive() is true.
//   4. Call AdvanceOnInput() when the player presses E/click.
//   5. Check WantsAbilitySelect() and WantsDoorUnlock() for signals back to
//      the engine, then call OnAbilitySelected() / ConsumeDoorUnlock() after
//      the engine has handled them.
//
// Adding a new cutscene later:
//   Just define another static array and call Play() with it. No enum changes,
//   no new manager methods — the same system handles everything.
// ─────────────────────────────────────────────────────────────────────────────

class CutsceneManager
{
public:

    // Start playing a cutscene.
    // actions[] must stay alive (static storage or a member array) for the
    // entire duration — the manager only keeps a pointer, not a copy.
    void Play(const CutsceneAction* actions, int count);

    // Immediately stop and clean up (e.g. when the player pauses to the main menu).
    void Stop();

    // Call every frame while IsActive().
    // playerPos and npcPos are moved in-place by MoveActor actions.
    // Pass nullptr for either if that actor is not in use for this cutscene.
    void Update(float dt, Vector2* playerPos, Vector2* npcPos);

    // Call every frame inside BeginDrawing() while IsActive().
    // borderTex  — shop border texture   (_shopBorderTex in Engine)
    // portraitTex — Zeph idle texture    (_shopZephTex in Engine)
    void Draw(Texture2D borderTex, Texture2D portraitTex, InputPromptMode promptMode = InputPromptMode::KeyboardMouse) const;

    // Call this when the player presses E or clicks during a Dialogue action.
    // First press skips the typewriter to show all text; second press advances.
    void AdvanceOnInput();

    // ── Status ────────────────────────────────────────────────────────────────
    bool IsActive() const { return _isActive; }

    // ── Signals back to Engine ────────────────────────────────────────────────
    // Engine checks WantsAbilitySelect() and then opens the ability pick screen.
    // After the player picks, Engine calls OnAbilitySelected() to resume.
    bool WantsAbilitySelect() const { return _wantsAbilitySelect; }
    void OnAbilitySelected();

    // Engine checks WantsDoorUnlock() then calls ConsumeDoorUnlock() after
    // applying the tile change so the flag is only acted on once.
    bool WantsDoorUnlock()   const { return _wantsDoorUnlock; }
    void ConsumeDoorUnlock()       { _wantsDoorUnlock = false; }

    // ── Debug designer access ─────────────────────────────────────────────────
    // The dialogue box editor in debug mode edits this directly so changes
    // are visible live without recompiling.
    DialogueBox& GetLayout() { return _box; }

private:

    // Called whenever _current changes to set up the new action's initial state.
    void BeginCurrentAction(Vector2* playerPos, Vector2* npcPos);

    // Move to the next action and begin it.
    void Advance(Vector2* playerPos, Vector2* npcPos);

    // ── Timeline ──────────────────────────────────────────────────────────────
    const CutsceneAction* _actions  = nullptr;
    int                   _count    = 0;
    int                   _current  = 0;
    bool                  _isActive = false;

    // ── Dialogue state ────────────────────────────────────────────────────────
    bool        _waitingForInput = false;  // true once the current page is fully shown
    std::string _visibleText;              // text revealed so far on the current page
    float       _typewriterTimer = 0.f;   // chars revealed on the current page (resets each page)
    float       _blinkTimer      = 0.f;   // drives the "press E" blink

    // ── Pagination ────────────────────────────────────────────────────────────
    // Computed once per Dialogue action; maps page index → exclusive end-char in full text.
    std::vector<int> _pageBreaks;
    int              _pageIdx    = 0;   // which page we are currently on
    int              _pageStart  = 0;   // char index in full text where this page starts
    int              _pageLength = 0;   // number of chars on this page

    static constexpr float kCharsPerSecond = 38.f;

    // ── Fade state ────────────────────────────────────────────────────────────
    float _fadeAlpha    = 0.f;   // 0 = fully visible, 1 = fully black
    float _fadeTimer    = 0.f;
    float _fadeDuration = 0.f;
    bool  _fadingIn     = false; // true = black-to-clear, false = clear-to-black

    // ── MoveActor state ───────────────────────────────────────────────────────
    Vector2 _moveOrigin   = {};
    Vector2 _moveTarget   = {};
    int     _moveActorIdx = 0;
    float   _moveTimer    = 0.f;
    float   _moveDuration = 0.f;
    bool    _moveCaptured = false;  // true once the start position has been snapped

    // ── Wait state ────────────────────────────────────────────────────────────
    float _waitTimer = 0.f;

    // ── Signals ───────────────────────────────────────────────────────────────
    bool _wantsAbilitySelect = false;
    bool _wantsDoorUnlock    = false;

    // ── Rendering ─────────────────────────────────────────────────────────────
    DialogueBox _box;
};
