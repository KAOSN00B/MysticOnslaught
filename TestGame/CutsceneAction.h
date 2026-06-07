#pragma once
#include "raylib.h"

// ── CutsceneAction ────────────────────────────────────────────────────────────
// A single step in a cutscene timeline. Cutscenes are just plain arrays of
// these structs, so adding a new scene is as simple as writing a new array.
//
// Example:
//   static const CutsceneAction myScene[] =
//   {
//       FadeIn(1.0f),
//       Dialogue("Zeph", "Welcome back."),
//       EndCutscene()
//   };
// ─────────────────────────────────────────────────────────────────────────────

struct CutsceneAction
{
    enum class Type
    {
        FadeIn,            // fade from black to clear
        FadeOut,           // fade from clear to black
        Dialogue,          // show one line and wait for player to press E
        Wait,              // pause for a fixed number of seconds (no input needed)
        MoveActor,         // slide an actor to a new world position
        OpenAbilitySelect, // pause cutscene; engine opens the ability pick screen
        UnlockDoor,        // signal engine to open the north exit door
        EndCutscene        // mark the end; manager stops and hands control back
    };

    Type        type     = Type::EndCutscene;
    float       duration = 0.f;      // FadeIn / FadeOut / Wait / MoveActor
    const char* speaker  = nullptr;  // Dialogue: name shown above the text box
    const char* text     = nullptr;  // Dialogue: the spoken line
    int         actorIdx = 0;        // MoveActor: 0 = player, 1 = shopkeeper NPC
    Vector2     target   = {};       // MoveActor: destination in world/screen space
};

// ── Factory helpers ───────────────────────────────────────────────────────────
// These keep cutscene arrays readable. Each function just fills a
// CutsceneAction with the right type and fields — nothing fancy.

inline CutsceneAction FadeIn(float seconds)
{
    CutsceneAction a;
    a.type     = CutsceneAction::Type::FadeIn;
    a.duration = seconds;
    return a;
}

inline CutsceneAction FadeOut(float seconds)
{
    CutsceneAction a;
    a.type     = CutsceneAction::Type::FadeOut;
    a.duration = seconds;
    return a;
}

// speaker can be nullptr if you want no name label above the text.
inline CutsceneAction Dialogue(const char* speaker, const char* text)
{
    CutsceneAction a;
    a.type    = CutsceneAction::Type::Dialogue;
    a.speaker = speaker;
    a.text    = text;
    return a;
}

inline CutsceneAction Wait(float seconds)
{
    CutsceneAction a;
    a.type     = CutsceneAction::Type::Wait;
    a.duration = seconds;
    return a;
}

// actorIdx: 0 = player, 1 = shopkeeper NPC
inline CutsceneAction MoveActor(int actorIdx, Vector2 targetPos, float seconds)
{
    CutsceneAction a;
    a.type     = CutsceneAction::Type::MoveActor;
    a.duration = seconds;
    a.actorIdx = actorIdx;
    a.target   = targetPos;
    return a;
}

inline CutsceneAction OpenAbilitySelect()
{
    CutsceneAction a;
    a.type = CutsceneAction::Type::OpenAbilitySelect;
    return a;
}

inline CutsceneAction UnlockDoor()
{
    CutsceneAction a;
    a.type = CutsceneAction::Type::UnlockDoor;
    return a;
}

inline CutsceneAction EndCutscene()
{
    CutsceneAction a;
    a.type = CutsceneAction::Type::EndCutscene;
    return a;
}
