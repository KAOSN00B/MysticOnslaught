#pragma once
#include "raylib.h"
#include "InputPrompts.h"
#include <string>
#include <vector>

// ── DialogueBox ───────────────────────────────────────────────────────────────
// Holds all layout values for the cutscene dialogue panel.
// Every value is editable live in the debug Dialogue Box designer (F11 in
// debug mode), so you can position and resize everything without recompiling.
//
// The panel uses the shop's nine-slice border texture and srcCorner/dstCorner
// values so it matches the existing shop art style.
// ─────────────────────────────────────────────────────────────────────────────

struct DialogueBox
{
    // ── Panel ─────────────────────────────────────────────────────────────────
    Rectangle panelRect     = {};      // filled in by ApplyScreenDefaults()
    float     srcCorner     = 16.f;    // nine-slice source corner size in pixels
    float     dstCorner     = 28.f;    // nine-slice corner size on screen in pixels

    // ── Portrait (Zeph image, left of panel) ─────────────────────────────────
    // Set showPortrait = true once the animated portrait is ready.
    bool  showPortrait    = false;
    float portraitX       = 0.f;    // screen-space centre X of the portrait
    float portraitY       = 0.f;    // screen-space centre Y of the portrait
    float portraitScale   = 1.0f;   // uniform scale (1.0 = native texture size)

    // ── Text ──────────────────────────────────────────────────────────────────
    float textInsetLeft = 45.5f;  // px from left edge of panel to text start
    float textInsetTop  = 34.0f;  // px from top edge of panel to first text row
    int   speakerFontSize = 47;
    int   bodyFontSize    = 32;
    Color speakerColor    = { 255, 220, 100, 255 };  // gold
    Color bodyColor       = WHITE;

    // ── Overlay ───────────────────────────────────────────────────────────────
    Color dimColor        = { 0, 0, 0, 140 };        // full-screen dim behind panel

    // Call once after the window is open; sets screen-relative default positions.
    void ApplyScreenDefaults();
    bool defaultsApplied = false;

    // Returns the char index (exclusive) where each page ends, given the full
    // dialogue text. Call this whenever the text or layout changes to get the
    // page break list. Pass the result to CutsceneManager for pagination.
    std::vector<int> ComputePageBreaks(const std::string& text) const;

    // ── Draw ──────────────────────────────────────────────────────────────────
    // borderTex    — shop border texture (_shopBorderTex in Engine)
    // portraitTex  — Zeph idle texture   (_shopZephTex in Engine); ignored if showPortrait=false
    // speaker      — name label (nullptr = no label row)
    // visibleText  — text for the current page, revealed so far by the typewriter
    // showContinue — blinks the "Press E" prompt when the page is fully shown
    void Draw(Texture2D borderTex,
              Texture2D portraitTex,
              const char* speaker,
              const std::string& visibleText,
              bool showContinue, InputPromptMode promptMode = InputPromptMode::KeyboardMouse) const;
};
