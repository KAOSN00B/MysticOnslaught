
#pragma once

#include "raylib.h"
#include "GameTypes.h"
#include "KeyBindings.h"
#include "Character.h"
#include "Prop.h"
#include "Enemy.h"
#include "Cyclops.h"
#include "Ogre.h"
#include "Molarbeast.h"
#include "MainMenu.h"
#include "PauseAndGameOver.h"
#include "Pickup.h"
#include "HealPickup.h"
#include "GoldPickup.h"
#include "SpreadProjectile.h"
#include "CyclopsLaserProjectile.h"
#include "LavaBallProjectile.h"
#include "TouchControls.h"
#include "NavigationGrid.h"
#include "VFXManager.h"
#include "ShopManager.h"
#include "DebugPanel.h"
#include "WorldConfig.h"
#include "AudioManager.h"
#include "RunStateController.h"
#include "CombatDirector.h"
#include "OverlayRenderer.h"
#include "CutsceneManager.h"
#include "WorldMapManager.h"
#include "SettingsManager.h"
#include "VirtualCanvas.h"
#include "TileMapper.h"
#include "NineSliceEditor.h"
#include "DungeonGen.h"
#include "TileDefs.h"
#include "RoomLayout.h"
#include "TileRenderer.h"
#include "GamepadInput.h"

#include <vector>
#include <string>
#include <memory>
#include <unordered_map>
#include <functional>


class Engine
{
public:
    Engine();
    ~Engine();

    void Run();
    void RunFrame();

private:
    enum class DungeonDoorSide;

    void Init();

    void Update(float dt);
    void Draw();

    void UpdateGamePlay(float dt);
    void SpawnWave();
    void SpawnEnemies();
    void HandleCollisions();
    void UpdateEnemyCount(float dt);
    Enemy* SpawnBasicEnemy(Vector2 pos);
    Enemy* SpawnCyclops(Vector2 pos);
    Enemy* SpawnOgre(Vector2 pos);
    void SpawnMolarbeast(Vector2 pos);
    void UpdateCyclopsLasers(float dt);
    void UpdateLavaBallProjectiles(float dt);
    void TriggerScreenShake(float strength, float duration);
    void DrawCyclopsLasers(Vector2 worldOffset);
    void DrawHUD();
    void DebugStartRun();
    void DebugRestartRoomAs(RoomType type);
    void DebugRestartDungeonRoomAs(RoomType type);
    void DebugSetEliteMechanic(int mechanic);
    void DrawHowToPlay();
    void DrawAbilityBar();   // unified 1-2-3-4 slot HUD
    void DrawWaveIntro();
    void HandlePlayerMeleeDamage();
    Rectangle GetTreasureChestRect() const;
    void OpenTreasureChest();
    void HandlePlayerCastRequest();
    void SpawnSpreadBurst(AbilityType element);
    void SpawnBolt(AbilityType element);
    void SpawnUltimateBurst(AbilityType element);
    void UpdateUltimateBlasts(float dt);
    void DrawUltimateBlasts(Vector2 worldOffset);

    // Cinematic ultimate sequence
    void TriggerUltimateSequence(AbilityType element);
    void UpdateUltimateSequence(float dt);
    void DrawUltimateSequence();
    void ApplyUltimateImpact();
    void GenerateStartingAbilityOptions();
    void DrawStartingAbilityChoice();
    void EnterDungeonRoom(int roomIdx, DungeonDoorSide entryDoorSide, Vector2 playerSpawnPos, bool resetRoomStates);
    Vector2 GetDungeonBottomSpawnPos() const;
    void EnterDungeonShopIfNeeded(const DungeonRoom& room);
    void UpdateSpreadProjectiles(float dt);
    void SpawnEnemyDrop(Vector2 worldPos, bool isOgre, bool isBoss);
    void SpawnTimedPickup();
    void SpawnBossSupportAdds();
    void UpdateBossSupportRespawns(float dt);
    void ClearBossSupportAdds();
    const char* GetBiomeName(Biome biome) const;
    void LoadTilesetForBiome(Biome biome);
    void ApplyBiome(Biome biome);
    void PopulatePropsForBiome(Biome biome);
    void UpdateBiomeTransition(float dt);
    float GetBiomeTransitionAlpha() const;

    void DrawLevelUpChoice();
    void GenerateLevelUpOptions(LevelUpOfferContext context = LevelUpOfferContext::NormalLevel);
    void DrawAbilityChoice();
    void GenerateAbilityChoiceOptions();
    void ResetRunState();


    // ── EXP Tally screen ─────────────────────────────────────────────────────
    void UpdateExpTally(float dt);
    void DrawExpTally();

    // ── Demo end screen ───────────────────────────────────────────────────────
    void DrawDemoEnd();

    // ── Room-based run progression ────────────────────────────────────────────
    void StartNextRoom(RoomType type);    // internal setup helper (biome + wave intro)
    void GenerateActMap();                // builds the full act node graph
    void EnterMapRoom(int nodeIdx);       // called when the player clicks a map node
    void CompleteCurrentMapNode();        // marks the current node complete and unlocks next nodes
    void HandleRoomContinueAction();      // shared continue path for cleared/completed rooms
    void DrawMap();                       // Slay-the-Spire–style act map screen
    std::vector<Vector2> GetCyclopsLaserEndpoints(const CyclopsLaserProjectile& laser) const;
    bool SegmentHitsRect(Vector2 start, Vector2 end, float thickness, const Rectangle& rect) const;

    // ── Settings screen ───────────────────────────────────────────────────────
    void UpdateSettings(float dt);
    void DrawSettings() const;
    void DrawSettingsKeybindings(float contentY, float panelX, float panelW, Vector2 mouse) const;
    void ApplySfxVolume();   // push current SFX scale to all owned Sound objects

    void UpdateMusicSystem();
    void EnsureAudioInitialized();
    void StartVictoryMusic(MusicCue cue);
    void ResetMusicState();

    Biome GetBiomeForAct(int act) const;

    // Touch-mode helpers
    void UpdateTouchControls();
    void DrawTouchAbilityArc();
    void ScanAbilityArcTaps();
    void EnterTouchButtonMapping();
    int  DrawTouchButtonMapping();     // returns 0=none 1=save 2=back 3=default
    void ApplyTouchCustomLayout();
    float GetTouchMappingRadius(int idx) const;
    Rectangle GetDebugToggleTabRect() const;
    bool HandleDebugToggleTabInput();
    void DrawDebugToggleTab();
    void SaveKeybindings();
    void LoadKeybindings();
    int GetActiveEnemyCount() const;
    bool IsBossFightActive() const;
    bool TryGetPooledEnemySpawn(Vector2 pos);
    bool TryGetPooledCyclopsSpawn(Vector2 pos);
    bool TryGetPooledOgreSpawn(Vector2 pos);
    bool TryGetPooledMolarbeastSpawn(Vector2 pos);
    int GetCyclopsSpawnCountForWave(int wave) const;
    int GetOgreSpawnCountForWave(int wave) const;
    int GetEnemyPowerLevelForWave(int wave) const;
    void ConfigureSpawnedEnemy(Enemy& enemy);

    Vector2 GetRandomPropPosition();
    bool IsSpawnPositionValid(Vector2 pos);
    void RespawnOutOfBoundsEnemies();
    bool TryGetFarSpawnPosition(Vector2& pos, float minPlayerDistance);

private:
    // ── Act map node — one room on the Slay-the-Spire–style map ──────────────
    struct MapNode
    {
        int      row       = 0;               // 0 = entry, 5 = boss
        float    normX     = 0.5f;            // 0–1 horizontal position in row
        RoomType type      = RoomType::Standard;
        bool     completed = false;           // player has cleared this room
        bool     available = false;           // player can click this node now
        std::vector<int> nextNodes;           // indices into _actMap
        Vector2  drawPos{};                   // screen-space position (computed in GenerateActMap)
    };

    // ── Virtual canvas + settings ─────────────────────────────────────────────
    RenderTexture2D _virtualCanvas{};
    Texture2D       _settingsBorderTex{};
    SettingsManager _settingsMgr;
    GameState       _stateBeforeSettings = GameState::Menu;
    // Settings screen UI state
    int   _settingsTab          = 0;   // 0=Display  1=Audio  2=Keybindings
    int   _settingsDragSlider   = -1;  // -1=none  0=master  1=music  2=sfx
    float _settingsDragStartX   = 0.f;
    float _settingsDragStartVal = 0.f;
    int   _settingsRebindSlot   = -1;  // -1=none; slot index 0-9 when waiting for key (M&K)
    int   _keybindSubTab        = 0;   // 0=M&K  1=Gamepad  (sub-tab inside KEYBINDINGS tab)
    int   _settingsGpRebindSlot = -1;  // -1=none; 0=attack 1=dash 2-5=ability[0-3] 6=pause
    GamepadBindings _gamepadBindingsEdit;

    RunStateController _runState;
    GameState& _gameState;
    GameState& _howToPlayFrom;
    int& _htpTab;
    float& _htpSlideOffset;

    bool       _audioInitialised  = false;
    bool       _shouldExit        = false;
    GameState  _stateBeforePause  = GameState::Play;  // so Pause ESC returns to the right state
    bool _waveStarting = true;
    bool _wave1LevelUpDone = false; // ensures forced level-up after wave 1 only fires once
    bool _playerDying = false;
    bool _shouldClose = false;
    bool _awaitingStartingAbility = false;

    int _wave        = 0;
    int  _enemiesKilled      = 0;
    int  _goldDroughtCounter = 0;  // kills since last Five-or-better drop; resets at 5
    int  _bossesDefeated     = 0;  // how many Molarbeasts have been killed this run
    bool _demoCompleted      = false; // true after 2 boss kills OR secret code — unlocks debug

    float _shakeTimer = 0.f;
    float _shakeStrength = 0.f;
    float _gameTimer = 0.f;
    float _pickupSpawnTimer = 0.f;

    Vector2 _shakeOffset = { 0.f, 0.f };
    Vector2 _cameraPos   = { 0.f, 0.f };

    const int _windowWidth = 1920;
    const int _windowHeight = 1080;

    float _waveIntroTimer = 0.f;
    float _waveIntroDuration = 2.5f;

    float _gameOverTimer  = 0.f;
    float _gameOverDelay  = 2.f;
    float _fadeInTimer    = 0.f;
    float _fadeInDuration = 2.f;  // set alongside _fadeInTimer; controls fade speed
    float _bossWarningTimer = 0.f;
    float _levelUpOpenTimer = 0.f;  // blocks card clicks briefly after panel opens

    // ── Act / room progression ────────────────────────────────────────────────
    // _wave keeps its name but now means "total rooms entered this run"
    // (used for enemy scaling via GetEnemyPowerLevelForWave).
    // _currentAct / _currentRoom drive display and encounter selection.
    int      _currentAct      = 1;                  // 1-indexed; advances after each boss clear
    int      _currentRoom     = 0;                  // 1-5 normal + 6 boss within the act
    RoomType _currentRoomType = RoomType::Standard; // drives SpawnEnemies and reward logic
    bool     _pendingRoomChoice  = false; // after AbilityChoice (boss clear), show new-act map
    bool     _roomClearPending   = false; // combat finished — waiting for player to click Continue
    float    _roomClearTimer     = 0.f;  // non-combat rooms wait before advancing (Rest/Store)

    // EXP tally state
    float _pendingExp             = 0.f;   // EXP accumulated during combat, drained during tally
    float _expTallyAccum          = 0.f;   // fractional drain accumulator
    bool  _expTallyDone           = false; // bar fully drained — show dismiss hint
    int   _tallyStartLevel        = 1;     // player level when tally begins, to count level-ups
    int   _tallyLevelUpsRemaining = 0;     // level-up choices still to show after tally
    bool  _tallyChoiceChaining    = false; // true once player pressed Continue to start chain

    // Act map state (Slay-the-Spire–style node graph)
    std::vector<MapNode> _actMap;
    int   _currentMapNodeIdx  = -1;         // index of the node currently in / last completed
    int   _mapKeySelectedIdx  = -1;         // keyboard-highlighted node on the map screen (-1 = none)
    float _mapOpenTimer       = 0.f;        // brief block after the map opens to prevent accidental clicks
    bool  _startBiomeDungeon  = true;       // fallback only; sequence takes precedence
    static constexpr int kTotalActs = 5;
    std::vector<Biome> _biomeSequence;      // 5 randomly chosen biomes per run

    // ── World map (biome selection) ───────────────────────────────────────
    WorldMapManager       _worldMap;
    int                   _worldZone = 0;          // 0=Caverns, 1-4=picked zones, 5=DemonsInsides
    std::vector<Biome>    _worldCompletedBiomes;   // biomes played, in order (after Caverns)
    std::vector<int>      _worldChosenNodeIndices; // 0/1/2 tierIdx chosen at each completed tier

    // ── Map screen right-panel debug editor ───────────────────────────────
    bool  _mapEditorActive  = false;
    int   _mapEditorSelIdx  = 0;
    float _mapJourneyX      = 0.725f;  // 0  right panel X as fraction of sw
    float _mapPad           = 21.f;    // 1  padding inside journey panel
    float _mapTitleFs       = 43.f;    // 2  "JOURNEY" title font
    float _mapRoomsFs       = 44.f;    // 3  Rooms / Gold row font
    float _mapNoRoomsFs     = 30.f;    // 4  "No rooms cleared yet" font
    float _mapBiomeTitleFs  = 43.f;    // 5  "BIOMES" label font
    float _mapBiomeLabelFs  = 0.350f;  // 6  diamond text size as fraction of half-size
    float _mapSqSz          = 96.f;    // 7  visited-room square size
    float _mapSqGap         = 33.f;    // 8  gap between visited-room squares
    float _mapIconSz        = 87.f;    // 9  map node icon size
    float _mapDiamondSz     = 56.f;    // 10 biome diamond half-size in pixels
    float _mapHeaderX       = 0.525f;  // 11 top Act header center X as fraction of sw
    float _mapHeaderY       = 10.f;    // 12 top Act header Y
    float _mapHeaderFs      = 55.f;    // 13 top Act header font size
    float _mapSubX          = 0.535f;  // 14 subtitle center X as fraction of sw
    float _mapSubY          = 61.f;    // 15 subtitle Y
    float _mapSubFs         = 30.f;    // 16 subtitle font size
    float _mapHintY         = 35.f;    // 17 bottom hint distance from screen bottom
    float _mapHintFs        = 34.f;    // 18 bottom hint font size

    // ── In-game HUD debug editor ──────────────────────────────────────────
    struct HUDConfig
    {
        // HP / MP bars (0–4)
        float barW         = 534.f;   // 0  bar width
        float barH         = 30.f;    // 1  bar height
        float barGap       = 28.f;    // 2  gap between HP and MP bars
        float barTopPad    = 16.f;    // 3  Y of HP bar from top
        float barLabelFs   = 15.f;    // 4  font inside bars
        // Gold label (5–7)
        float goldX        = 21.f;    // 5  gold X
        float goldY        = 16.f;    // 6  gold Y
        float goldFs       = 48.f;    // 7  gold font size
        // Enemies left (8–10)
        float enemiesX     = 22.f;    // 8  enemies label X
        float enemiesY     = 90.f;    // 9  enemies label Y
        float enemiesFs    = 45.f;    // 10 enemies label font size
        // Act / Room label (11–13)
        float actOffsetX   = 42.f;    // 11 offset from right edge
        float actY         = 30.f;    // 12 act label Y
        float actFs        = 43.f;    // 13 act label font size
        // Minimap (14–22)
        float miniX        = 16.f;    // 14 minimap origin X
        float miniY        = 164.f;   // 15 minimap origin Y
        float miniW        = 271.f;   // 16 minimap width (height auto-derived)
        float miniDotBoss  = 6.f;     // 17 boss dot base radius
        float miniDotElite = 2.f;     // 18 cyclops/ogre dot base radius
        float miniDotEnemy = 0.f;     // 19 regular enemy dot base radius
        float miniDotProp  = 3.f;     // 20 prop dot base radius
        float miniDotPickup= 2.f;     // 21 pickup dot base radius
        float miniDotPlayer= 4.f;     // 22 player dot base radius
        // PC ability bar (23–27)
        float slotSz       = 103.f;   // 23 ability slot size (square)
        float slotGap      = 108.f;    // 24 gap between slots
        float slotBotPad   = 19.f;    // 25 slot Y from bottom
        float slotKeyFs    = 15.f;    // 26 keybind label font size
        float slotNameFs   = 21.f;    // 27 ability name font size
        // Touch buttons (28–38)
        float touchJoyR       = 90.f;   // 28 joystick radius
        float touchAtkR       = 113.f;  // 29 ATK button radius
        float touchDashR      = 111.f;  // 30 DASH button radius
        float touchAtkPadR    = 176.f;  // 31 ATK centre from right
        float touchAtkPadB    = 214.f;  // 32 ATK centre from bottom
        float touchDashOffset = 302.f;  // 33 DASH left-offset from ATK (ATK.x - DASH.x)
        float touchDashBotPad = 214.f;  // 33b DASH centre Y from bottom (-1 = share ATK pad)
        float touchPauseW     = 130.f;  // 34 pause button width
        float touchPauseH     = 67.f;   // 35 pause button height
        float touchPausePad   = 85.f;   // 36 pause button edge padding
        float touchAtkFs      = 46.f;   // 37 ATK button text size
        float touchDashFs     = 57.f;   // 38 DASH button text size
        // Touch ability slots (39–42)
        float touchSlotSz      = 130.f;  // 39 ability slot square size
        float touchSlotGap     = 20.f;   // 40 gap between slots
        float touchSlotRightPad= 27.f;   // 41 right-edge padding for slot row
        float touchSlotYOff    = 29.f;   // 42 gap between slot bottom and ATK top
        // EXP bar (43-45)
        float expBarH    = 27.f;   // 43 height of the EXP bar
        float expBarGap  = 19.f;   // 44 gap between MP bar bottom and EXP bar top
        float expLabelFs = 16.f;   // 45 font size of the level / exp label
        // HP / MP orbs (46-50)
        float hpOrbX     = 513.f;  // 46 HP orb centre X
        float hpOrbY     = 999.f;  // 47 HP orb centre Y
        float mpOrbX     = 1408.f; // 48 MP orb centre X
        float mpOrbY     = 992.f;  // 49 MP orb centre Y
        float statOrbR   = 64.f;   // 50 shared HP/MP orb radius
        // Armour icons (51-53)
        float armourX    = 590.f;  // 51 first armour icon X
        float armourY    = 853.f;  // 52 armour icon Y
        float armourSize = 55.f;   // 53 armour icon height
    };
    HUDConfig _hudCfg;
    bool  _hudEditorActive  = false;
    int   _hudEditorSelIdx  = 0;

    // Per-slot drag offsets (set interactively when HUD editor is open)
    Vector2 _touchSlotOffset[4]{};
    int     _touchSlotDragIdx        = -1;
    Vector2 _touchSlotDragMouseStart{};
    Vector2 _touchSlotDragOffsetStart{};

    // Touch button mapping screen
    // Indices: 0=ATK  1=DASH  2..5=ability slots 0..3
    bool    _touchLayoutCustom   = false;
    Vector2 _touchCustomPos[6]{};      // saved absolute centres (session only)
    Vector2 _touchMappingPos[6]{};     // working positions during mapping
    int     _touchMappingDragIdx  = -1;
    Vector2 _touchMappingDragStart{};
    Vector2 _touchMappingPosAtDrag{};

    // Snapshot of the original baked values — captured once on first mapping screen open.
    // Default always restores to these, regardless of how many saves have been made.
    struct TouchDefaults
    {
        float   atkPadR, atkPadB, dashOffset, dashBotPad;
        Vector2 slotOffset[4];
        Vector2 pos[6];
        bool    captured = false;
    } _touchDefaults;

    // Level-up choice state
    UpgradeType _levelUpOptions[3] = { UpgradeType::AttackPower, UpgradeType::AttackRange, UpgradeType::MaxHealth };
    UpgradeType _levelUpUltimateOptions[3] = { UpgradeType::LearnFireUltimate, UpgradeType::LearnIceUltimate, UpgradeType::LearnElectricUltimate };
    GameState   _levelUpReturnState  = GameState::Play;
    LevelUpOfferContext _levelUpOfferContext = LevelUpOfferContext::NormalLevel;
    bool        _showUltimateRow     = false;
    bool        _ultimateRowPicked   = false;
    bool        _regularRowPicked    = false;
    bool        _eliteRewardGranted  = false;
    UpgradeType _startingAbilityOptions[6] = {
        UpgradeType::LearnFireSpread, UpgradeType::LearnIceSpread, UpgradeType::LearnElectricSpread,
        UpgradeType::LearnFireBolt,   UpgradeType::LearnIceBolt,   UpgradeType::LearnElectricBolt
    };
    bool        _startingAbilitySelected[6] = {};
    int         _startingAbilityPickCount = 0;
    bool        _starterAbilityGiftClaimed = false;

    // ── Elite-room state (all reset in StartNextRoom) ─────────────────────
    // Active mechanic index: 0=Cage, 1=Bodyguard, 2=Enrage, 3=Leap, 4=Hazards; -1=none
    int     _eliteMechanic            = -1;
    Enemy*  _eliteMinibossPtr         = nullptr;  // non-owning ptr into _enemies
    Vector2 _eliteCageCenter          = {};
    float   _eliteCageRadius          = 0.f;
    float   _eliteCageDamageTimer     = 0.f;
    float   _eliteEnrageWarningTimer  = 0.f;
    bool    _eliteIsLeaping           = false;
    Vector2 _eliteLeapStartPos        = {};
    Vector2 _eliteLeapTarget          = {};
    float   _eliteLeapCooldown        = 0.f;
    float   _eliteLeapTimer           = 0.f;
    float   _eliteHazardSpawnTimer    = 0.f;

    // ── Elite constants ───────────────────────────────────────────────────
    static constexpr float kEliteCageRadius             = 500.f;
    static constexpr float kEliteCageDamageInterval     = 0.5f;
    static constexpr float kEliteEnrageWarningDuration  = 4.0f;
    static constexpr float kLeapInterval                = 8.0f;
    static constexpr float kLeapDuration                = 1.5f;
    static constexpr float kLeapAoERadius               = 90.f;
    static constexpr int   kLeapAoEDamage               = 3;
    static constexpr float kHazardVolleyMinInterval     = 0.55f;
    static constexpr float kHazardVolleyMaxInterval     = 0.95f;
    static constexpr int   kHazardVolleyMinCount        = 3;
    static constexpr int   kHazardVolleyMaxCount        = 6;

    // ── Biome-modifier room state ─────────────────────────────────────────
    // Wastelands: warning circles that detonate on the player after a delay
    struct WastelandHazard
    {
        Vector2 pos;
        float   warningTimer;   // counts down; on zero, explodes and sets exploded = true
        float   activeTimer;    // counts down while the sprite is playing
        float   dmgCooldown;    // per-hazard cooldown so the player isn't hit every frame
        bool    exploded;
    };
    std::vector<WastelandHazard> _wastelandHazards;
    float _wastelandHazardTimer = 0.f;

    // Lost City: two long beams that rotate around fixed pivot points
    struct LostCityBeam
    {
        Vector2 center;
        float   angle;          // current angle in radians
        float   rotSpeed;       // rad/s (negative = counter-clockwise)
        float   length;
        float   damageCooldown; // per-beam cooldown so the player isn't hit every frame
    };
    std::vector<LostCityBeam> _lostCityBeams;

    // Sanctuary: debuff zones — player inside one is dash-locked and slowed
    struct SanctuaryZone { Vector2 pos; float radius; };
    std::vector<SanctuaryZone> _sanctuaryZones;

    // Demon Insides: pulsing red overlay timer
    float _demonPulseTimer = 0.f;

    // Biome-modifier constants
    static constexpr float kWastelandHazardInterval  = 2.8f;
    static constexpr float kWastelandWarningDuration = 1.8f;
    static constexpr float kWastelandExplosionRadius = 75.f;
    static constexpr int   kWastelandDamage          = 1;
    static constexpr float kWastelandActiveDuration  = 0.80f;
    static constexpr float kLostCityBeamLength       = 340.f;
    static constexpr float kLostCityBeamWidth        = 10.f;
    static constexpr float kLostCityBeamRotSpeed     = 0.9f;
    static constexpr float kLostCityBeamDmgCooldown  = 0.35f;
    static constexpr float kSanctuaryZoneRadius      = 130.f;
    static constexpr float kSanctuarySlowFactor      = 0.45f;

    // Ability choice state (shown after every 5th-wave boss clear)
    UpgradeType _abilityChoiceOptions[3] = { UpgradeType::LearnFireSpread, UpgradeType::LearnFireSpread, UpgradeType::LearnFireSpread };
    int         _abilityChoiceOptionCount = 0;
    bool        _abilityChoiceSwapPending = false;
    UpgradeType _abilityChoiceSwapTarget  = UpgradeType::AttackPower;
    int         _lastAbilityChoiceWave    = -1;
    float       _abilityChoiceOpenTimer   = 0.f;

    // Upgrade icon textures (loaded once, never reloaded)
    Texture2D _upgradeAttackPowerTex{};
    Texture2D _upgradeAttackRangeTex{};
    Texture2D _upgradeHealthTex{};
    Texture2D _upgradeMagicTex{};
    Texture2D _upgradeDefenseTex{};
    Texture2D _upgradeMoveSpeedTex{};

    Sound _pickupSound{};
    Sound _fireballCastSound{};
    Sound _explosionSound{};
    Sound _roomClearExplosionSound{};
    Sound _lavaBallImpactSound{};
    Sound _buttonPressSound{};

    Texture2D _map{};
    Texture2D _treeTex{};
    Texture2D _smallTreeTex{};
    Texture2D _rockTex{};
    Texture2D _bigRockTex{};
    Vector2 _mapPos{};
    // Map scale is now computed by _worldConfig — do not set this by hand.
    // Read/write it through _worldConfig.Recalculate() and GetScale().
    float      _mapScale    = 3.f;
    WorldConfig _worldConfig;          // owns all map-scale and camera logic
    int _maxActiveEnemies = 16;

    Texture2D _pillarTex{};
    Texture2D _torchTex{};       // Torch.png — 256x29, 8 frames of 32x29
    Texture2D _pillarTorchTex{}; // PillarTorch.png — 290x52, 8 frames of 32x52 (content offset x=17)
    Texture2D _fireballCastTex{};
    Texture2D _fireballHitTex{};
    Texture2D _genericHitTex{};   // Hit03.png — melee hit splat + electric impact sprite
    Texture2D _iceHitTex{};       // Ice_Shard_Hit.png — ice ability impact
    Texture2D _lightningCastTex{};
    Texture2D _healEffectTex{};
    Texture2D _roomClearExplosionTex{};

    // Ability card icons for the level-up / starting ability panels
    Texture2D _abilityIconFireTex{};
    Texture2D _abilityIconIceTex{};
    Texture2D _abilityIconElectricTex{};

    Texture2D _shopBorderTex{};
    Texture2D _shopZephTex{};
    Texture2D _htpBorderTex{};   // HowToPlayBorder.png - used for the How To Play content panel
    Texture2D _magicGemTex{};
    Texture2D _bossBarrierTex{};

    // Map node icons (TileSet/MapIcons/)
    Texture2D _mapIconNormal{};
    Texture2D _mapIconElite{};
    Texture2D _mapIconShop{};
    Texture2D _mapIconTreasure{};
    Texture2D _mapIconBoss{};
    Texture2D _mapIconRest{};

    Character _player;
    MainMenu _menu;
    PauseAndGameOver _pauseUI;
    KeyBindings _keybindingsEdit;

    std::vector<Prop> _props;
    std::vector<std::unique_ptr<Pickup>> _pickups;
    enum class UltimatePhase { None, WindUp, Cinematic, Impact, Release };

    struct UltimateBlast
    {
        Vector2     worldPos{};
        AbilityType element  = AbilityType::FireUltimate;
        float       timer    = 0.f;
        float       lifetime = 0.f;
        float       rotation = 0.f;   // initial rotation angle (degrees)
    };

    static constexpr float _ultWindUpDuration    = 0.75f;
    static constexpr float _ultCinematicDuration = 1.5f;
    static constexpr float _ultImpactDuration    = 0.35f;
    static constexpr float _ultReleaseDuration   = 0.5f;

    UltimatePhase _ultimatePhase       = UltimatePhase::None;
    float         _ultimatePhaseTimer  = 0.f;
    float         _ultimateCircleAngle = 0.f;
    AbilityType   _ultimateElement     = AbilityType::FireUltimate;

    std::vector<UltimateBlast>       _ultimateBlasts;
    std::vector<SpreadProjectile>    _spreadProjectiles;
    VFXManager     _vfx;
    NavigationGrid _nav;
    ShopManager    _shop;
    std::vector<std::unique_ptr<Enemy>>   _enemies;
    std::vector<CyclopsLaserProjectile>   _cyclopsLasers;
    std::vector<LavaBallProjectile>       _lavaBalls;
    BossSupportState _bossCyclopsSupport;
    BossSupportState _bossOgreSupport;

    std::string _message = "Objective: Survive";

    Biome _currentBiome = Biome::Caverns;
    Biome _pendingBiome = Biome::Caverns;
    bool  _biomeTransitionActive = false;
    bool  _biomeTransitionSwapped = false;
    float _biomeTransitionTimer = 0.f;
    bool     _demoEndTouchHeld = false;
    AudioManager _audio;
    CombatDirector _combatDirector;
    OverlayRenderer _overlayRenderer;

    // ── Touch mode ───────────────────────────────────────────────────────────
    bool          _touchModeActive = false;
    TouchControls _touch;
    GamepadInput  _gamepad;
    // Touch IDs that have already triggered an ability cast this press.
    // Cleared each frame when the touch lifts; prevents repeat casts on hold.
    std::vector<int> _abilityTapSeenIds;

    DebugPanel      _debug;
    TileMapper      _tileMapper;
    NineSliceEditor _nineSliceEditor;
    DungeonGen   _dungeonGen;
    TileDefSet   _tileDefs;
    TileRenderer _tileRenderer;

    // Per-room persistent state tracked during a dungeon run session.
    struct DungeonRoomState { bool cleared = false; };

    // Dungeon run sub-state
    enum class DungeonView { Graph, Room, Play };
    DungeonView _dungeonView          = DungeonView::Graph;
    int        _dungeonRoomIdx = -1;
    RoomLayout _dungeonRoomLayout{};

    enum class DungeonDoorSide { None = -1, North, South, West, East };
    DungeonDoorSide _dungeonEntryDoorSide = DungeonDoorSide::None;

    struct DungeonClearEffect
    {
        Vector2 worldPos{};
        float   timer = 0.f;
        Rectangle glowRect{};
        bool hasGlow = false;
    };
    std::vector<DungeonClearEffect> _dungeonClearEffects;

    std::unordered_map<int, DungeonRoomState> _dungeonRoomStates;
    bool _dungeonEnemiesSpawned = false;

    bool _hasMagicGem = false;
    bool _magicGemSpawned = false;
    bool _magicGemCollected = false;
    bool _bossBarrierUnlocked = false;
    float _magicGemAnimTimer = 0.f;
    int   _magicGemFrame = 0;
    float _bossBarrierAnimTimer = 0.f;
    int   _bossBarrierFrame = 0;
    float _bossBarrierMessageTimer = 0.f;
    // Zelda-style room scroll transition
    bool       _dungeonScrolling      = false;
    float      _dungeonScrollT        = 0.f;
    Vector2    _dungeonScrollVec      = {};         // direction current room slides (unit, applied to sw/sh)
    RoomLayout _dungeonScrollNextLayout{};
    int        _dungeonScrollNextIdx  = -1;
    Vector2    _dungeonScrollSpawnPos = {};
    DungeonDoorSide _dungeonScrollNextEntryDoorSide = DungeonDoorSide::None;
    static constexpr float kDungeonScrollDur = 0.40f;

    // Full-screen fade used when entering the dungeon from the Store and after boss clears.
    enum class DungeonFadeState { None, FadingOut, FadingIn };
    DungeonFadeState      _dungeonFadeState  = DungeonFadeState::None;
    float                 _dungeonFadeAlpha  = 0.f;   // 0–255
    float                 _dungeonFadeTimer  = 0.f;
    std::function<void()> _dungeonFadePendingAction;
    static constexpr float kDungeonFadeDuration = 0.40f;

    // Treasure room chest — spawns at screen centre after all enemies die.
    bool _treasureChestSpawned = false;
    bool _treasureChestBroken  = false;

    // Folder scanned by the TileMapper debug tool for PNG tilesets.
    static constexpr const char* kTilesheetFolder =
        "C:/Lasalle/Semester 4/2DGamesProgramming/ClassNotes/TestGame/MapTilesets";

    // Folder scanned by the 9-Slice Editor — top-level PNGs in UI/ only, no subfolders.
    static constexpr const char* kUIFolder =
        "C:/Lasalle/Semester 4/2DGamesProgramming/ClassNotes/TestGame/UI";

    // ── Hitbox debug editor (F12 while debug active) ─────────────────────────
    bool           _isHitboxEditorActive = false;
    BaseCharacter* _hitboxSelectedEntity = nullptr;
    bool           _hitboxEditAttack     = false;
    float          _hitboxNudgeAccum     = 0.f;
    static constexpr float kHitboxNudgeInitDelay  = 0.30f;
    static constexpr float kHitboxNudgeRepeatRate = 0.05f;

    void UpdateHitboxEditor();
    void DrawHitboxEditor();

    // ── Dialogue box designer (F11 while debug active) ───────────────────────
    // Lets you drag and resize the dialogue panel and text sizes live
    // so you can tweak layout without recompiling.
    bool  _isDlgEditorActive   = false;
    int   _dlgEditorHandle     = -1;   // panel handle being dragged (0-4), -1 = none

    // Font-size drag state (drag the label left/right to shrink/grow)
    bool  _dlgSpeakerFsDrag    = false;
    float _dlgSpeakerFsDragX   = 0.f;
    int   _dlgSpeakerFsDragVal = 0;
    bool  _dlgBodyFsDrag       = false;
    float _dlgBodyFsDragX      = 0.f;
    int   _dlgBodyFsDragVal    = 0;

    // Text-inset drag state (horizontal and vertical padding inside the panel)
    bool  _dlgInsetLeftDrag    = false;
    float _dlgInsetLeftDragX   = 0.f;
    float _dlgInsetLeftDragVal = 0.f;
    bool  _dlgInsetTopDrag     = false;
    float _dlgInsetTopDragX    = 0.f;
    float _dlgInsetTopDragVal  = 0.f;

    void UpdateDialogueBoxEditor();
    void DrawDialogueBoxEditor();

    // ── Cutscene system ───────────────────────────────────────────────────────
    CutsceneManager _cutscene;
    bool            _cutsceneIntroPlayed = false; // stays true after first-ever intro

    // Lock / unlock the Store room's north exit door tile directly.
    void SetStoreDoorTiles(TileType doorType);

    // World map (biome selection after each boss clear)
    void UpdateWorldMap(float dt);
    void DrawWorldMap();
    void OpenWorldMap();   // generates the map and switches state

    // Dungeon run
    void UpdateDungeonRun(float dt);
    void UpdateDreamFlicker(float dt);
    void InitBiomeModifierRoom();
    void UpdateBiomeModifiers(float dt);
    void DrawBiomeModifiers();
    void DrawDungeonRun();
    Rectangle GetDungeonRoomRect(int roomIdx) const;
    DungeonDoorSide GetBossBarrierSide() const;
    Rectangle GetBossBarrierRect(DungeonDoorSide side) const;
    Vector2 GetMagicGemWorldPos() const;
    void UpdateDungeonMagicGemAndBarrier(float dt);
    void DrawDungeonMagicGemAndBarrier() const;
    void DrawMagicGemHudIcon() const;

    // Dungeon run combat helpers
    Vector2   GetDungeonSpawnPos(float cellW, float cellH) const;
    void      SpawnDungeonRoomEnemies();
    void      ClearDungeonEnemies();

    // Door state helpers for tile-dungeon rooms.
    void ApplyDungeonRoomDoorState(RoomLayout& layout, int roomIdx, DungeonDoorSide entryDoorSide) const;
    bool IsDungeonDoorOpen(DungeonDoorSide side) const;
    DungeonDoorSide OppositeDungeonDoorSide(int dr, int dc) const;
    void SpawnDungeonDoorOpenEffects();
    void UpdateDungeonClearEffects(float dt);
    void DrawDungeonClearEffects() const;

    // Boss room exit door - sets tiles in _dungeonRoomLayout and returns the trigger rect.
    void      ApplyDungeonBossExitTiles(TileType doorType);
    Rectangle GetDungeonBossExitTrigger() const;

    // Rebuilds the nav grid for the current dungeon room layout, including wall tiles.
    void RebuildDungeonNav();

    // Resolves all active enemies out of tile walls and props. Ends forced pushes on impact.
    void ResolveDungeonEnemyCollisions();
};
