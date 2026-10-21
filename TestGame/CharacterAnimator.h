#pragma once

#include "raylib.h"
#include "Enemy.h"
#include "CharacterTuning.h"
#include "AttackTuning.h"
#include "AttackEditor.h"

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

// ─────────────────────────────────────────────────────────────────────────────
// Character Animator (main menu dev tool). Every character's size, animation
// speed, sprite alignment and per-animation hitboxes, plus the player's attack
// fire points. Saves charactertuning_<Name>.txt / attacktuning_<key>.txt, which
// the game reads at spawn. Unsaved edits are tracked against the last save,
// with undo/redo, and the Size Lineup shows everyone at their in-game size.
// The timeline holds each animation's hit window (when its melee box is live)
// and its events (FX, sound, shake on a frame); the MOVES tab sizes a boss's
// or elite's special-move hit areas.
// ─────────────────────────────────────────────────────────────────────────────
class CharacterAnimator
{
public:
    void Init();
    void Update();
    void Draw();
    void Unload();
    bool WantsToExit() const { return _wantsToExit; }
    void OpenAttackLibrary();

    // ── Flow self-test hooks (EngineFlowSelfTest phase 8). They run the same
    // code as the controls and never write a file. ──
    int         TestEntryCount() const { return (int)_entries.size(); }
    std::string TestEntryName(int index) const { return _entries[index].displayName; }
    bool        TestOpen(const std::string& name);
    Enemy*      TestCharacter() const { return _enemy.get(); }
    float       TestSizeVsHero() const { return SizeVsHero(); }
    void        TestSetSizeVsHero(float multiple);
    bool        TestHasUnsavedChanges() const { return HasUnsavedChanges(); }
    void        TestUndo() { Undo(); }
    void        TestRedo() { Redo(); }
    void        TestSelectAnim(int index) { SelectAnim(index); }
    void        TestStepFrame(int delta) { StepFrame(delta); }
    void        TestRequestBack() { RequestBack(); }
    bool        TestDialogOpen() const { return _dialog != Dialog::None; }
    void        TestDialogDiscard() { AnswerDialog(DialogAnswer::Discard); }
    void        TestDialogCancel() { AnswerDialog(DialogAnswer::Cancel); }
    bool        TestOnSelectScreen() const { return _screen == Screen::Select; }
    void        TestOpenLineup() { OpenLineup(); }
    int         TestLineupCount() const { return (int)_lineup.size(); }
    void        TestSetHeroShown(bool shown) { _showHero = shown; }
    // The hero reference stands left of the character, feet on the same ground.
    bool        TestHeroPlaced() const;
    // Feet measured below the sprite's centre (a sanity check on measuring).
    bool        TestMeasuredFeetBelowCentre() const { return _charBounds.height > 0.f && _groundRel > 0.f; }
    void        TestSetHitWindow(int start, int end) { _enemy->SetAnimHit(_animIndex, start, end); }
    bool        TestMeleeLiveNow() const { return MeleeLiveNow(); }
    bool        TestIsMeleeAnim(int slot) const { return IsMeleeAnim(slot); }
    void        TestAddEvent(CharacterTuning::EventKind kind, int frame) { AddEvent(kind, frame); }
    int         TestEventCount() const { return (int)CurrentEvents().size(); }
    int         TestMoveCount() const { return (int)_moves.size(); }
    std::string TestMoveKey(int index) const { return _moves[index].info.key; }
    void        TestSelectMove(int index) { SelectMove(index); }
    void        TestSetMoveRadius(float radius);
    void        TestSetHurtPad(float x, float y) { _enemy->SetHurtPad(x, y); }
    void        TestSaveAll() { CommitIfChanged(); SaveAll(); }

private:
    enum class Screen { Select, Edit, Lineup, AttackLibrary };
    enum class EditTarget { Sprite, Body, Melee, Attack, Move, Event };
    enum class RightTab { Moves, Event };   // Moves = the player's attack for the Player
    enum class Category { Hero, Enemy, Elite, Boss };
    enum class Dialog { None, Unsaved, ConfirmReset };
    enum class DialogAnswer { Save, Discard, Cancel };
    enum class AfterDialog { None, BackToSelect, OpenEntry, SwitchAttack };

    struct CharacterEntry
    {
        std::string displayName;
        Category    category = Category::Enemy;
        int         meleeSlot = -1;   // animation with a melee box; -1 = ranged/contact only
        std::function<Enemy*()> createInstance;
    };

    // ── Clickable controls. Rebuilt every frame from the current state, so
    // input and drawing always agree on where everything is. ──
    enum class Control
    {
        None,
        // Select screen
        EntryRow, OpenLineup, OpenAttackLibrary,
        // Edit: left panel
        Tab, SizeRow, SizeMinus, SizePlus, SizeAuto, Lineup,
        SpeedRow, SpeedMinus, SpeedPlus, SpeedDefault,
        AnimRow, CopyToAll, Save, Revert, Reset, Back,
        // Edit: timeline bar
        StepBack, PlayPause, StepForward, FrameCell,
        HeroToggle, SnapToggle, Flip, ZoomOut, ZoomReset, ZoomIn,
        // Edit: timeline lanes
        HitCell, EventCell,
        // Edit: right panel
        RightTabMoves, RightTabEvent,
        // Edit: player attack panel
        AttackPrev, AttackNext, AttackPreview, AttackField,
        // Edit: special moves
        MoveRow, MoveField, MoveCodeSize, EliteField,
        // Edit: events
        EventRow, EventAdd, EventKindTab, EventField, EventFxPrev, EventFxNext, EventDelete,
        // Dialog
        DialogSave, DialogDiscard, DialogCancel,
        // Lineup
        LineupBack
    };
    enum class UiKind { Button, Toggle, Tab, ValueRow, ListRow, FrameCell, HitCell, EventCell, Label };

    struct UiItem
    {
        Control     id = Control::None;
        int         index = 0;
        UiKind      kind = UiKind::Button;
        Rectangle   rect{};
        std::string label;
        std::string detail;          // right-aligned extra text (list rows)
        std::string tooltip;
        Color       accent = WHITE;
        bool        active = false;  // selected tab/row, toggle on, current frame
        bool        enabled = true;
        bool        detailAccent = false;   // detail text in the accent colour
        unsigned    flags = 0u;      // animation rows: body / melee / sprite marks
    };

    // One undoable state: the character's tuning, (player only) the attack's,
    // and the special moves' hit areas.
    struct Snapshot
    {
        CharacterTuning           character{};
        AttackTuning              attack{};
        std::vector<AttackTuning> moves;
        std::string               characterText;
        std::string               attackText;
        std::string               movesText;
    };

    // A special move being edited: what the class says it is, and its file.
    struct MoveEdit
    {
        Enemy::MoveInfo info;
        AttackTuning    tuning;
    };

    // An event's FX playing on the stage (the editor's own preview).
    struct FxPreview
    {
        std::string stem;
        Vector2     rel{};
        float       size = 0.f;
        float       time = 0.f;
    };

    struct LineupItem
    {
        int                    entry = -1;      // -1 = the hero reference at a row start
        Enemy*                 instance = nullptr;
        std::unique_ptr<Enemy> owned;            // null for the character being edited
        Rectangle              bounds{};         // drawn pixels, relative to its position
        Vector2                position{};
        float                  sizeVsHero = 0.f;
        float                  heightPx = 0.f;
        int                    row = 0;          // 0 hero + enemies, 1 elites, 2 bosses
        bool                   firstInRow = false;
        float                  ground = 0.f;     // its line's feet line
        float                  lineTop = 0.f;    // top of the tallest on its line
    };

    // ── Screens ──
    void UpdateSelect();
    void UpdateEdit(float dt);
    void UpdateLineup(float dt);
    void DrawSelectScreen();
    void DrawEditScreen();
    void DrawLineupScreen();

    // ── Character lifetime ──
    void OpenCharacter(int index);
    void CloseCharacter();
    void RequestOpenCharacter(int index);
    void RequestBack();
    void OpenLineup();
    void LayoutLineup(float zoom);
    void CloseLineup();
    int  FindEntry(const std::string& name) const;
    bool IsPlayer() const;

    // ── Animation ──
    void SelectAnim(int index);
    void PlayAnimInEditor(int index);
    void ReplayCurrentAnimKeepingFrame();
    void StepFrame(int delta);
    float CurrentFrameTime() const;
    void SetCurrentFrameTime(float frameTime);

    // ── Size ──
    float SizeVsHero() const;
    float ScaleForSizeVsHero(float multiple) const;
    void  ApplyScale(float newScale);
    static CharacterTuning ScaledTuning(const CharacterTuning& tuning, float factor);

    // ── Saving, undo, unsaved changes ──
    CharacterTuning CaptureCharacter() const;
    Snapshot        CaptureSnapshot() const;
    void            RestoreCharacter(const CharacterTuning& tuning);
    void            RestoreSnapshot(const Snapshot& snapshot);
    void            ResetHistory();
    void            CommitIfChanged();
    void            Undo();
    void            Redo();
    bool            CharacterDirty() const;
    bool            AttackDirty() const;
    bool            HasUnsavedChanges() const { return CharacterDirty() || AttackDirty() || MovesDirty(); }
    void            SaveAll();
    void            RevertToSaved();
    void            ResetToCodeDefaults();
    void            CopyBodyAndSpriteToAll();
    void            SetStatus(const std::string& text);

    // ── Dialog ──
    void OpenDialog(Dialog dialog, AfterDialog after, int param);
    void AnswerDialog(DialogAnswer answer);
    void RunAfterDialog();
    void CloseDialog();
    void BuildDialogUi();
    void DrawDialog();

    // ── Player attack ──
    void BuildAttackList();
    void SelectAttack(int index);
    void RequestSelectAttack(int index);
    void LoadCurrentAttackTuning();
    std::string CurrentAttackKey() const;
    const char* CurrentAttackLabel() const;
    bool PlayerAttackEditorAvailable() const { return IsPlayer(); }
    void UpdateAttackPreview(float dt);
    void DrawAttackOverlay();

    // ── Edit input ──
    void BuildEditUi();
    void BuildSelectUi();
    void HandleEditKeys();
    bool HandleUiMouse(Vector2 mouse);
    void HandleStageMouse(Vector2 mouse);
    void ClickControl(const UiItem& item);
    void BeginValueDrag(const UiItem& item, Vector2 mouse);
    void UpdateValueDrag(Vector2 mouse);
    void StepValue(const UiItem& item, float steps);
    void UpdateHandleDrag(Vector2 mouse);
    void Nudge(Vector2 delta);
    bool IsGestureActive() const;
    const UiItem* HitUi(const std::vector<UiItem>& items, Vector2 mouse) const;
    void UpdateTooltip(const std::vector<UiItem>& items, Vector2 mouse, float dt);

    // ── View (zoom + pan on the stage) ──
    Rectangle StageRect() const;
    Vector2   StageOrigin() const;
    Vector2   ToScreen(Vector2 rel) const;
    Vector2   ToRel(Vector2 screen) const;
    Camera2D  StageCamera() const;
    float     Snap(float value) const;
    void      ZoomAt(Vector2 screenPoint, float newZoom);
    void      ResetView();
    void      FitView();

    // ── Measuring (drawn pixels, off screen) ──
    Rectangle MeasureDrawnBounds(Enemy& enemy);
    void      MeasureCharacter();
    void      PlaceHero();

    // ── Drawing helpers ──
    void DrawStage();
    void DrawOverlays();
    void DrawSizeGuide();
    void DrawUi(const std::vector<UiItem>& items) const;
    void DrawUiItem(const UiItem& item, bool hovered) const;
    void DrawTooltip() const;
    void DrawStatus() const;
    void DrawHandle(Vector2 screenPos, bool hot, Color color) const;
    bool HandleHit(Vector2 mouse, Vector2 screenPos) const;

    void GetEffectiveBodyCircle(Vector2& outOffset, float& outRadius) const;
    bool MeleeEditableNow() const;
    Rectangle GetEffectiveMeleeRel() const;

    // ── Melee per animation + hit windows ──
    bool IsMeleeAnim(int slot) const;
    bool CodeHitWindow(int slot, int& start, int& end) const;   // the class's own frames, on this sheet
    bool HitWindowFor(int slot, int& start, int& end, bool& authored) const;
    bool MeleeLiveNow() const;

    // ── Events ──
    std::vector<CharacterTuning::AnimEvent> CurrentEvents() const;
    void SetCurrentEvents(const std::vector<CharacterTuning::AnimEvent>& events);
    void AddEvent(CharacterTuning::EventKind kind, int frame);
    void DeleteEvent(int index);
    void SelectEvent(int index);
    void ScanFxCatalog();
    const Texture2D* FxSheet(const std::string& stem);
    void UpdateFxPreviews(float dt);
    void DrawFxPreviews();
    void DrawEventMarkers() const;
    void DrawEventOverlay();

    // ── Special moves ──
    void LoadMoves();
    std::string MovesText(const std::vector<AttackTuning>& moves) const;
    std::vector<AttackTuning> MoveTunings() const;
    bool MovesDirty() const;
    void SelectMove(int index);
    void MakeMoveCustom(MoveEdit& move) const;
    void DrawMoveOverlay();
    void DrawHurtbox();
    void BuildRightPanel(float x, float width);
    // The right panel's value rows (moves, elite timing, events): current
    // value, drag rate and setter, by control and row.
    float PanelValue(Control id, int index);
    float PanelValuePerPixel(Control id, int index) const;
    void  SetPanelValue(Control id, int index, float value);

    // ── State ──
    std::vector<CharacterEntry> _entries;
    Screen _screen = Screen::Select;
    Screen _lineupReturn = Screen::Select;
    int    _selectedIdx = -1;
    bool   _wantsToExit = false;

    std::unique_ptr<Enemy> _enemy;
    std::unique_ptr<Enemy> _hero;          // the player's look, the size reference
    AttackEditor _attackEditor;
    int  _animIndex = 0;
    bool _animPaused = false;
    EditTarget _target = EditTarget::Body;
    float _classFrameTime[CharacterTuning::kMaxAnims] = {};   // each anim's speed without an override
    int   _animFrameCount[CharacterTuning::kMaxAnims] = {};
    float _editFacing = 1.f;            // restored after the lineup borrows the character

    // Size + measuring
    float     _idleArtHeight = 0.f;     // visible idle art in sprite px (the game's sizing input)
    Rectangle _charBounds{};            // drawn pixels relative to the character's position
    float     _groundRel = 0.f;         // the character's feet line
    Rectangle _heroBounds{};
    float     _heroHeightPx = 0.f;      // the hero's visible art height in game
    Vector2   _heroPos{};
    bool      _measureDirty = true;
    bool      _fitPending = false;      // frame the character once it has been measured
    bool      _showHero = true;
    RenderTexture2D _measureTarget{};

    // View
    float   _zoom = 1.f;
    Vector2 _pan{};
    bool    _snap = true;
    bool    _panning = false;
    Vector2 _lastMouse{};

    // UI
    std::vector<UiItem> _ui;
    std::vector<UiItem> _dialogUi;
    Control _tooltipControl = Control::None;
    int     _tooltipIndex = -1;
    float   _tooltipTimer = 0.f;
    std::string _tooltipText;
    Vector2 _tooltipAt{};

    // Undo / unsaved
    Snapshot _committed;
    Snapshot _saved;
    std::vector<Snapshot> _undo;
    std::vector<Snapshot> _redo;

    // Dialog
    Dialog      _dialog = Dialog::None;
    AfterDialog _after = AfterDialog::None;
    int         _afterParam = -1;

    // Player attack
    struct AttackOption
    {
        std::string label;
        std::string key;
        bool basic = false;
        AbilityType ability = AbilityType::None;
    };
    std::vector<AttackOption> _attackOptions;
    int _attackIndex = 0;
    AttackTuning _attackTuning{};
    bool _attackPreviewActive = false;
    float _attackPreviewTimer = 0.f;
    float _attackPreviewNextShot = 0.f;
    std::vector<float> _attackPreviewShots;

    // Drags
    int     _dragHandle = -1;
    Vector2 _dragAnchor{};
    bool    _spriteDragActive = false;
    Vector2 _spriteDragStartMouse{};
    Vector2 _spriteDragStartOffset{};
    Control _rowDrag = Control::None;
    int     _rowDragIndex = 0;
    float   _rowDragStartX = 0.f;
    float   _rowDragStartValue = 0.f;
    CharacterTuning _rowDragStartTuning{};
    bool    _scrubbing = false;

    // Hit windows, events, moves
    RightTab _rightTab = RightTab::Moves;
    std::vector<MoveEdit> _moves;
    int      _moveIndex = -1;
    int      _eventIndex = -1;              // into the current animation's events
    std::vector<std::string> _fxCatalog;    // PowerUps/FX_*.png stems
    std::unordered_map<std::string, Texture2D> _fxSheets;   // loaded on first use
    std::vector<FxPreview> _fxPreviews;
    int      _previewSlot = -1;
    int      _previewFrame = -1;
    float    _shakeTimer = 0.f;             // a Shake event jiggles the stage
    float    _shakeStrength = 0.f;
    bool     _hitDragging = false;
    int      _hitAnchor = 0;
    bool     _eventDragging = false;

    // Lineup
    std::vector<LineupItem> _lineup;
    float   _lineupZoom = 1.f;
    Vector2 _lineupOrigin{};            // screen position of lineup world (0,0)
    int     _lineupHover = -1;

    float _statusTimer = 0.f;
    std::string _statusText;
};
