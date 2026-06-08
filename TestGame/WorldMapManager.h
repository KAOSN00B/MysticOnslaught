#pragma once

#include "GameTypes.h"
#include "raylib.h"
#include <vector>

class Character;

// Slay-the-Spire-style world map shown after every boss clear.
// 6 tiers total: tier 0 = Caverns (always), tiers 1-4 = player picks,
// tier 5 = DemonsInsides (always last). Circles only — no icons.
class WorldMapManager
{
public:
    // Regenerate the map. Call each time a boss is cleared.
    //   completedBiomes  — biomes actually played, in order (index 0 = first pick after Caverns)
    //   chosenIndices    — tier-index (0/1/2) of the node the player picked at each completed tier
    //   nextZone         — which zone the player is about to choose (1-4)
    void Generate(
        const std::vector<Biome>& completedBiomes,
        const std::vector<int>&   chosenIndices,
        int                       nextZone,
        int                       windowWidth,
        int                       windowHeight);

    // Returns true once the player has confirmed a pick AND the fade-out is complete.
    bool Update(float dt);

    // Full screen draw (background, panels, graph, confirm popup, fade overlay).
    void Draw(const Character& player) const;

    // The biome the player confirmed. Only valid after Update returns true.
    Biome GetSelectedBiome()   const { return _selectedBiome; }
    // The 0/1/2 tier-index of the chosen node (stored by Engine for next Generate call).
    int   GetSelectedTierIdx() const { return _selectedTierIdx; }

    void Reset();

    // Debug layout editor — toggle with KEY_NINE while WorldMap is active in debug mode.
    void ToggleEditor()        { _editorActive = !_editorActive; _editorSelIdx = 0; }
    bool IsEditorActive() const { return _editorActive; }
    void UpdateEditor();
    void DrawEditor() const;

    // Shared helpers used by Engine (e.g. for HUD, minimap labels).
    static Color       GetBiomeColor(Biome b);
    static const char* GetBiomeName(Biome b);

private:
    // One circle on the map.
    struct Node
    {
        Biome   biome      = Biome::Caverns;
        int     tier       = 0;      // 0 = Caverns row, 5 = DemonsInsides row
        int     tierIdx    = 0;      // position within the tier: 0/1/2
        Vector2 drawPos    = {};
        bool    isCompleted  = false; // player cleared this biome
        bool    isReachable  = false; // player can click this in the current tier
        bool    isLocked     = true;  // future tier — show "?"
        bool    hasBiome     = false; // real biome assigned (vs placeholder "?")
    };

    std::vector<Node> _nodes;
    int   _nextZone       = 1;   // which tier is the current choice (1-4)

    int   _hoveredIdx     = -1;
    int   _confirmIdx     = -1;
    bool  _confirmActive  = false;
    float _openTimer      = 0.3f; // brief block after opening to prevent mis-clicks

    // Fade-in (map appears from black) and fade-out (after confirm, before load).
    float _fadeAlpha      = 255.f; // 255 = fully black, 0 = fully visible
    bool  _fadingOut      = false;
    bool  _done           = false;

    Biome _selectedBiome   = Biome::Caverns;
    int   _selectedTierIdx = 0;

    // ── Layout params — all editor-tunable ───────────────────────────────────
    float _mapLeft        = 0.30f;   // graph area left edge as fraction of sw
    float _mapRight       = 0.76f;   // graph area right edge as fraction of sw
    float _mapTop         = 130.f;   // pixels from top of screen
    float _mapBotPad      = 110.f;   // pixels from bottom of screen
    float _nodeRad        = 28.f;    // circle radius
    float _lineThick      = 3.5f;    // connection line thickness
    float _labelFs        = 24.f;    // biome name font size under node
    float _confirmW       = 620.f;
    float _confirmH       = 210.f;
    float _confirmFs      = 36.f;
    float _btnH           = 58.f;
    float _headerX        = 0.525f;  // fraction of sw for header centre
    float _headerY        = 10.f;
    float _headerFs       = 50.f;
    float _subY           = 62.f;
    float _subFs          = 28.f;
    float _hintY          = 35.f;
    float _hintFs         = 32.f;

    // Left stats panel
    float _panelX         = 48.f;
    float _panelY         = 98.f;
    float _panelW         = 490.f;

    // Right journey panel
    float _journeyX       = 0.79f;   // fraction of sw
    float _journeyPad     = 21.f;
    float _journeyTitleFs = 43.f;
    float _journeyCircleR = 22.f;    // radius of completed-biome circles in the journey list
    float _journeyGap     = 12.f;    // gap between journey entries

    // Panel text font sizes (all editor-tunable)
    float _statTitleFs    = 32.f;   // "PLAYER STATS" and "ABILITIES" section headers
    float _statFs         = 28.f;   // stat row text
    float _abilFs         = 24.f;   // ability list entries
    float _journeyNameFs  = 22.f;   // biome name text in the right journey panel
    float _nodeBgPad      = 12.f;   // padding around each node for the semi-transparent bg box

    int   _lastTouchCount = 0;       // tracks touch-down edges for click detection

    // ── Debug editor ─────────────────────────────────────────────────────────
    bool  _editorActive   = false;
    int   _editorSelIdx   = 0;

    // ── Private helpers ───────────────────────────────────────────────────────

    // Fills outNext[3] with up to 3 tier-indices in the NEXT tier that
    // are connected from this node. -1 means no more connections.
    static void GetNextConnections(int tier, int tierIdx, int outNext[3]);

    // True if the given screen point falls within a node's clickable circle.
    static bool PointInNode(Vector2 point, Vector2 nodePos, float radius);

    void DrawLeftPanel(const Character& player) const;
    void DrawRightPanel() const;
    void DrawConfirmPopup(float sw, float sh) const;
};
