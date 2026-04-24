#include "Molarbeast.h"
#include "AssetPaths.h"

#include "Character.h"
#include "raymath.h"

#include <cmath>

Texture2D Molarbeast::_sharedIdleAnim{};
Texture2D Molarbeast::_sharedMeleeAnim{};
Texture2D Molarbeast::_sharedRangedAnim{};
Texture2D Molarbeast::_sharedDashAnim{};
Texture2D Molarbeast::_sharedHurtAnim{};
Texture2D Molarbeast::_sharedDeathAnim{};
Texture2D Molarbeast::_sharedSpawnFireballTex{};
Sound Molarbeast::_sharedAttackSound{};
Sound Molarbeast::_sharedHurtSound{};
Sound Molarbeast::_sharedDeathSound{};
Sound Molarbeast::_sharedWallHitSound{};
bool Molarbeast::_sharedResourcesLoaded = false;

Molarbeast::Molarbeast(Vector2 pos)
    : Enemy(pos)
{
}

Molarbeast::~Molarbeast()
{
}

void Molarbeast::Init()
{
    EnsureSharedResourcesLoaded();

    _idleAnim = _sharedIdleAnim;
    _walkAnim = _sharedIdleAnim;
    _attackAnim = _sharedMeleeAnim;
    _takeDamageAnim = _sharedHurtAnim;
    _deathAnim = _sharedDeathAnim;
    _attackSound = _sharedAttackSound;
    _hurtSound = _sharedHurtSound;
    _deathSound = _sharedDeathSound;

    // Lock in stable collision dimensions from the idle sheet. These never
    // change again so every collision rect stays consistent across all
    // animation states — melee, hurt, dash, ranged sheets can all differ in
    // size without making the hurtbox jump around.
    _stableFrameW = _idleAnim.width / (float)_sheetFrameCount;
    _stableFrameH = _idleAnim.height;

    BuildBehaviourTree();
    ResetForSpawn(_worldPos);
}

void Molarbeast::ResetForSpawn(Vector2 pos)
{
    _worldPos = pos;
    _worldPosLastFrame = pos;
    _homePos = pos;
    _velocity = Vector2Zero();
    _isActive = true;
    _dying = false;
    _takingDamage = false;
    _attacking = false;
    _damageApplied = false;
    _freezeTimer = 0.f;
    _pendingBurns.clear();
    _launchVisualTimer = 0.f;
    _launchHoldingHurtPose = false;
    _runningTime = 0.f;
    _frame = 0;
    _scale = _bossScale;
    _speed = _moveSpeed;
    _expValue = _bossBaseExpValue;
    _rightLeft = 1.f;
    _health = 18.f;
    _maxHealth = 18.f;
    _attackPower = 1.f;
    _attackRange = _attackRangeBase;
    _state = State::Chasing;
    _specialTimer = 10.0f;
    _meleeCooldown = 0.f;
    _contactCooldown = 0.f;
    _chargeTimer = 0.f;
    _chargeDuration = 0.f;
    _recoveryTimer = 0.f;
    _volleyShotTimer = 0.f;
    _volleyShotsRemaining = 0;
    _pendingLavaBallShot = false;
    _impactShakeRequested = false;
    _dashDirection = Vector2{ 1.f, 0.f };
    _queuedLavaBallTarget = _worldPos;
    _dashedEnemies.clear();
    _stuckTimer = 0.f;
    _stuckCheckPos = _worldPos;
    _lockedNavTarget = _worldPos;
    _hasLockedNavTarget = false;
    _navTargetLockTimer = 0.f;
    _escapeDirection = Vector2Zero();
    _escapeTimer = 0.f;
    _deathTimer = 0.6f;
    // Start each spawn at a random orbit angle so multiple Molarbeasts
    // don't circle in sync.
    _orbitAngle = (float)GetRandomValue(0, 628) / 100.f;
    SetIdleAnimation(true);
}

void Molarbeast::Update(float dt, Vector2 heroWorldPos, Vector2 navigationTarget, bool hasNavigationTarget,
    const std::vector<std::unique_ptr<Enemy>>& enemies, const std::vector<Vector2>& propCenters)
{
    (void)heroWorldPos;

    _navTarget    = navigationTarget;
    _hasNavTarget = hasNavigationTarget;

    if (!_isActive)
        return;

    _worldPosLastFrame = _worldPos;
    ApplyVelocity(dt);
    UpdateHit(dt);
    UpdateLaunchVisual(dt);

    if (_dying)
    {
        HandleAnimation(dt);
        return;
    }

    if (_target == nullptr)
        return;

    _specialTimer    -= dt;
    _meleeCooldown   -= dt;
    _contactCooldown -= dt;

    // Make frame-scoped data available to BT leaf lambdas.
    _cachedEnemies     = &enemies;
    _cachedPropCenters = &propCenters;

    _behaviorTreeRoot->Tick(dt);

    _cachedEnemies     = nullptr;
    _cachedPropCenters = nullptr;

    TryDealContactDamage();
    HandleAnimation(dt);
}

void Molarbeast::SetWaveScale(int wave)
{
    // Fixed first-boss profile. Boss progression now follows the same global
    // enemy power-level system as the rest of the cast so we avoid stacking
    // a per-5-wave stat jump on top of the slower every-10-wave scaling.
    // Wave only affects cadence/readability, not raw boss stats.
    _expValue = _bossBaseExpValue;
    _health      = 44.f;
    _maxHealth   = _health;
    _speed       = _moveSpeed;
    _attackPower = 2.f;

    _attackRange = _attackRangeBase;
    ResetSpecialCooldown();
    ResetMeleeCooldown();
}

void Molarbeast::DrawEnemy(Vector2 cameraRef)
{
    if (!_isActive)
        return;

    Vector2 screenPos = Vector2Subtract(_worldPos, cameraRef);
    screenPos.x += GetScreenWidth() * 0.5f;
    screenPos.y += GetScreenHeight() * 0.5f;

    // The boss uses the same bottom-centred draw anchor across every state so
    // its oversized sheets still feel planted in one world position.
    float drawWidth = _width * _scale;
    float drawHeight = _height * _scale;

    Color tint = WHITE;
    if (_takingDamage)
        tint = Color{ 255, 220, 220, 255 };

    if (_state == State::DashCharging)
    {
        float chargeRatio = (_chargeDuration > 0.01f)
            ? 1.f - (_chargeTimer / _chargeDuration)
            : 1.f;
        if (chargeRatio < 0.f) chargeRatio = 0.f;
        if (chargeRatio > 1.f) chargeRatio = 1.f;
        tint.r = 255;
        tint.g = (unsigned char)(255.f - _chargeTintStrength * chargeRatio);
        tint.b = (unsigned char)(255.f - _chargeTintStrength * chargeRatio);
    }

    Vector2 drawJitter = Vector2Zero();
    if (_state == State::RangedCharging)
    {
        drawJitter.x = (float)GetRandomValue(-4, 4);
        drawJitter.y = (float)GetRandomValue(-4, 4);
    }

    Rectangle source{ _frame * _width, 0.f, _rightLeft * _width, _height };
    Rectangle dest{
        screenPos.x + drawJitter.x,
        screenPos.y + drawJitter.y,
        drawWidth,
        drawHeight
    };

    DrawTexturePro(_texture, source, dest, Vector2{ drawWidth * 0.5f, drawHeight * 0.78f }, 0.f, tint);

    if (_state == State::RangedCharging)
    {
        // The spawn-fireball sheet loops in front of the boss while it charges
        // so the player gets a clear read on the projectile attack without a
        // new HUD icon or ground decal.
        const int columns = 3;
        const int rows = 2;
        const int frameCount = 6;
        const float frameWidth = _sharedSpawnFireballTex.width / (float)columns;
        const float frameHeight = _sharedSpawnFireballTex.height / (float)rows;
        const int spawnFrame = ((int)std::floor(GetTime() * 14.0)) % frameCount;
        const int spawnCol = spawnFrame % columns;
        const int spawnRow = spawnFrame / columns;

        Rectangle spawnSource{
            spawnCol * frameWidth,
            spawnRow * frameHeight,
            frameWidth,
            frameHeight
        };

        Vector2 spawnWorld = GetLavaBallSpawnPos();
        Vector2 spawnScreen = Vector2Subtract(spawnWorld, cameraRef);
        spawnScreen.x += GetScreenWidth() * 0.5f;
        spawnScreen.y += GetScreenHeight() * 0.5f;

        const float spawnScale = 1.6f;
        Rectangle spawnDest{
            spawnScreen.x,
            spawnScreen.y,
            frameWidth * spawnScale,
            frameHeight * spawnScale
        };
        DrawTexturePro(_sharedSpawnFireballTex, spawnSource, spawnDest,
            Vector2{ spawnDest.width * 0.5f, spawnDest.height * 0.5f }, 0.f, WHITE);
    }

    DrawHealthBar(screenPos, drawWidth, drawHeight);
}

void Molarbeast::TakeDamage(int damage, Vector2 attackerPos)
{
    (void)attackerPos;
    if (_dying)
        return;

    // Keep only a tiny duplicate-hit guard. The older 0.1s blanket gate made
    // the boss effectively stop taking damage once it entered its faster
    // low-health pattern, because many legitimate melee/projectile hits landed
    // during the guard window and were discarded.
    if (_hitTimer > 0.f)
        return;

    int appliedDamage = damage;
    if (IsInReducedDamageState())
        appliedDamage = std::max(1, (int)std::ceil(damage * 0.5f));

    _health -= (float)appliedDamage;

    if (_health > 0.f)
        PlayHurtSound();

    if (_health <= 0.f)
    {
        _health = 0.f;
        _dying = true;
        _takingDamage = false;
        _attacking = false;
        _pendingLavaBallShot = false;
        _volleyShotsRemaining = 0;
        _state = State::Recovery;
        _deathTimer = 0.7f;
        SetDeathAnimation(true);
        PlayDeathSound();
        return;
    }

    // The boss should react visually to hits, but never get knocked back.
    _velocity = Vector2Zero();
    if (IsInReducedDamageState())
    {
        // Charging states still take reduced damage, but the player should get
        // visible hit feedback when the strike connects. Keep the charge state
        // intact and let HandleAnimation restore the correct charge sheet after
        // the brief hurt reaction finishes.
        _takingDamage = true;
        _hitTimer = 0.02f;
        SetHurtAnimation(true);
        return;
    }

    _takingDamage = true;
    // Short per-hit guard only — prevents the same overlapping hitbox from
    // registering twice in consecutive frames. _takingDamage stays true
    // independently until the hurt animation finishes, so the visual hold
    // is not tied to this timer.
    _hitTimer = 0.02f;
    SetHurtAnimation(true);
}

void Molarbeast::PlayHurtSound()
{
    SetSoundPitch(_sharedHurtSound, 0.28f);
    SetSoundVolume(_sharedHurtSound, 0.5f);
    StopSound(_sharedHurtSound);
    PlaySound(_sharedHurtSound);
}

void Molarbeast::ApplyBurn(float delay, int damage, Vector2 sourcePos)
{
    (void)delay;
    (void)damage;
    (void)sourcePos;
}

void Molarbeast::ApplyFreeze(float duration)
{
    (void)duration;
}

Rectangle Molarbeast::GetCollisionRec() const
{
    // Use _stableFrameW/H (set once from the idle sheet in Init) so this rect
    // never changes shape when animations switch mid-fight.
    float width  = _stableFrameW * _scale * _collisionWidthScale;
    float height = _stableFrameH * _scale * _collisionHeightScale;
    return Rectangle{
        _worldPos.x - width * 0.5f,
        _worldPos.y - height * 0.5f + (_stableFrameH * _scale * _collisionYOffsetScale),
        width,
        height
    };
}

Rectangle Molarbeast::GetBodyContactRec() const
{
    // Contact damage uses a trimmed version of the boss hurtbox so the player
    // only gets punished when truly overlapping the creature, not just when
    // standing near the edges of the large sprite.
    Rectangle bodyRec = GetCollisionRec();
    bodyRec.x += _bodyContactInset;
    bodyRec.y += _bodyContactInset;
    bodyRec.width = std::max(1.0f, bodyRec.width - _bodyContactInset * 2.f);
    bodyRec.height = std::max(1.0f, bodyRec.height - _bodyContactInset * 2.f);
    return bodyRec;
}

Rectangle Molarbeast::GetThreatCollisionRec() const
{
    Rectangle bodyRec = GetCollisionRec();
    bodyRec.width += _attackFrontPadding;
    if (_rightLeft > 0.f)
        bodyRec.x -= 0.f;
    else
        bodyRec.x -= _attackFrontPadding;
    return bodyRec;
}

Vector2 Molarbeast::GetLavaBallSpawnPos() const
{
    return Vector2Add(_worldPos, Vector2{ _rightLeft * (_width * _scale * 0.28f), -_height * _scale * 0.1f });
}

void Molarbeast::OnLavaBallSpawned()
{
    _pendingLavaBallShot = false;
}

void Molarbeast::OnDashBlocked()
{
    if (_state != State::Dashing)
        return;

    UndoMovement();
    SetSoundVolume(_sharedWallHitSound, 0.6f);
    StopSound(_sharedWallHitSound);
    PlaySound(_sharedWallHitSound);
    FinishDash(true);
}

bool Molarbeast::ConsumeImpactShakeRequest()
{
    bool requested = _impactShakeRequested;
    _impactShakeRequested = false;
    return requested;
}

void Molarbeast::EnsureSharedResourcesLoaded()
{
    if (_sharedResourcesLoaded)
        return;

    _sharedIdleAnim = LoadTexture(AssetPath("Bosses/MolarbeastIdle.png").c_str());
    _sharedMeleeAnim = LoadTexture(AssetPath("Bosses/MolarbeastMeleeAttack.png").c_str());
    _sharedRangedAnim = LoadTexture(AssetPath("Bosses/MolarbeastRangedAttack.png").c_str());
    _sharedDashAnim = LoadTexture(AssetPath("Bosses/MolarbeastDashAttack.png").c_str());
    _sharedHurtAnim = LoadTexture(AssetPath("Bosses/MolarbeastHurt.png").c_str());
    _sharedDeathAnim = LoadTexture(AssetPath("Bosses/MolarbeastDeath.png").c_str());
    _sharedSpawnFireballTex = LoadTexture(AssetPath("Bosses/Spawn_Fireball.png").c_str());

    // The demo boss currently reuses the ogre sound family so its actions read
    // clearly without requiring a full bespoke audio pass yet.
    _sharedAttackSound = LoadSound(AssetPath("Sounds/OgreChargeHit.ogg").c_str());
    _sharedHurtSound = LoadSound(AssetPath("Sounds/PlayerHurt.ogg").c_str());
    _sharedDeathSound = LoadSound(AssetPath("Sounds/MonsterDeath.ogg").c_str());
    _sharedWallHitSound = LoadSound(AssetPath("Sounds/OgreHitWall.ogg").c_str());
    _sharedResourcesLoaded = true;
}

void Molarbeast::UnloadSharedResources()
{
    if (!_sharedResourcesLoaded)
        return;

    UnloadTexture(_sharedIdleAnim);
    UnloadTexture(_sharedMeleeAnim);
    UnloadTexture(_sharedRangedAnim);
    UnloadTexture(_sharedDashAnim);
    UnloadTexture(_sharedHurtAnim);
    UnloadTexture(_sharedDeathAnim);
    UnloadTexture(_sharedSpawnFireballTex);
    UnloadSound(_sharedAttackSound);
    UnloadSound(_sharedHurtSound);
    UnloadSound(_sharedDeathSound);
    UnloadSound(_sharedWallHitSound);

    _sharedIdleAnim = Texture2D{};
    _sharedMeleeAnim = Texture2D{};
    _sharedRangedAnim = Texture2D{};
    _sharedDashAnim = Texture2D{};
    _sharedHurtAnim = Texture2D{};
    _sharedDeathAnim = Texture2D{};
    _sharedSpawnFireballTex = Texture2D{};
    _sharedAttackSound = Sound{};
    _sharedHurtSound = Sound{};
    _sharedDeathSound = Sound{};
    _sharedWallHitSound = Sound{};
    _sharedResourcesLoaded = false;
}

void Molarbeast::SetIdleAnimation(bool resetFrame)
{
    _texture = _idleAnim;
    _width = _idleAnim.width / (float)_sheetFrameCount;
    _height = _idleAnim.height;
    _updateTime = 1.f / 8.f;
    _maxFrames = _sheetFrameCount;

    if (resetFrame)
    {
        _frame = 0;
        _runningTime = 0.f;
    }
}

void Molarbeast::BuildBehaviourTree()
{
    // ── Node A: Melee Branch (Sequence) ──────────────────────────────────────
    // Fires when the player enters the threat zone with the melee cooldown
    // ready. Stays RUNNING until the attack animation completes naturally.
    // Only triggers from Chasing so a dash or ranged charge cannot be
    // interrupted mid-wind-up.
    auto meleeSeq = std::make_unique<BTSequence>();

    meleeSeq->AddChild(std::make_unique<BTCondition>([this]() -> bool
    {
        if (_target == nullptr) return false;
        if (_state == State::MeleeAttacking) return true;   // already swinging — hold branch
        if (_state != State::Chasing)        return false;  // never interrupt a special
        if (_meleeCooldown > 0.f)            return false;
        float dist = Vector2Distance(_worldPos, _target->GetFeetWorldPos());
        return CheckCollisionRecs(GetThreatCollisionRec(), _target->GetCollisionRec())
            || dist <= _attackRange;
    }));

    meleeSeq->AddChild(std::make_unique<BTAction>([this](float) -> BTStatus
    {
        if (_state != State::MeleeAttacking)
        {
            _state         = State::MeleeAttacking;
            _attacking     = true;
            _damageApplied = false;
            _velocity      = Vector2Zero();
            SetMeleeAnimation(true);
            ResetMeleeCooldown();
            SetSoundVolume(_attackSound, 0.35f);
            StopSound(_attackSound);
            PlaySound(_attackSound);
        }
        HandleMelee();
        return (_state == State::MeleeAttacking) ? BTStatus::RUNNING : BTStatus::SUCCESS;
    }));

    // ── Node B: Special Attack Selector (Selector) ───────────────────────────
    // Covers the full special lifecycle in priority order:
    //   B1. Continue an active dash  (DashCharging → Dashing)
    //   B2. Continue an active ranged attack  (RangedCharging → RangedVolley)
    //   B3. Ride out post-special recovery
    //   B4. Trigger a brand-new special when the cooldown expires
    // Returning FAILURE from every branch lets Node C (Chasing) run.
    auto specialSel = std::make_unique<BTSelector>();

    // B1 — Dash
    auto dashSeq = std::make_unique<BTSequence>();
    dashSeq->AddChild(std::make_unique<BTCondition>([this]() -> bool {
        return _state == State::DashCharging || _state == State::Dashing;
    }));
    dashSeq->AddChild(std::make_unique<BTAction>([this](float dt) -> BTStatus
    {
        // Run one phase per frame — matches original switch behaviour where
        // DashCharging and Dashing were separate cases that never ran together.
        if (_state == State::DashCharging)
        {
            HandleDashCharge(dt);
            return BTStatus::RUNNING;
        }
        HandleDash(dt, *_cachedEnemies);
        return BTStatus::RUNNING;
    }));

    // B2 — Ranged
    auto rangedSeq = std::make_unique<BTSequence>();
    rangedSeq->AddChild(std::make_unique<BTCondition>([this]() -> bool {
        return _state == State::RangedCharging || _state == State::RangedVolley;
    }));
    rangedSeq->AddChild(std::make_unique<BTAction>([this](float dt) -> BTStatus
    {
        if (_state == State::RangedCharging)
        {
            HandleRangedCharge(dt);
            return BTStatus::RUNNING;
        }
        HandleRangedVolley(dt);
        return BTStatus::RUNNING;
    }));

    // B3 — Recovery
    auto recoverySeq = std::make_unique<BTSequence>();
    recoverySeq->AddChild(std::make_unique<BTCondition>([this]() -> bool {
        return _state == State::Recovery;
    }));
    recoverySeq->AddChild(std::make_unique<BTAction>([this](float dt) -> BTStatus
    {
        HandleRecovery(dt);
        return (_state == State::Recovery) ? BTStatus::RUNNING : BTStatus::SUCCESS;
    }));

    // B4 — Special trigger: returns FAILURE while the timer is still positive
    // (allowing Node C to run), fires BeginRandomSpecial when it expires.
    // To swap "Dash" for a new move later: replace the BeginRandomSpecial call
    // here or add a new branch above B4.
    auto triggerSpecial = std::make_unique<BTAction>([this](float) -> BTStatus
    {
        if (_specialTimer > 0.f) return BTStatus::FAILURE;
        BeginRandomSpecial();
        return BTStatus::RUNNING;
    });

    specialSel->AddChild(std::move(dashSeq));
    specialSel->AddChild(std::move(rangedSeq));
    specialSel->AddChild(std::move(recoverySeq));
    specialSel->AddChild(std::move(triggerSpecial));

    // ── Node C: Orbit / Movement (Action) ────────────────────────────────────
    // Default fallback — circles the player, handles A* nav and stuck recovery.
    // Runs only when Nodes A and B both return FAILURE.
    auto chaseAction = std::make_unique<BTAction>([this](float dt) -> BTStatus
    {
        HandleChasing(dt, *_cachedPropCenters);
        return BTStatus::RUNNING;
    });

    // ── Root: Selector ────────────────────────────────────────────────────────
    auto root = std::make_unique<BTSelector>();
    root->AddChild(std::move(meleeSeq));
    root->AddChild(std::move(specialSel));
    root->AddChild(std::move(chaseAction));

    _behaviorTreeRoot = std::move(root);
}

void Molarbeast::SetWalkAnimation(bool resetFrame)
{
    // The boss uses the idle sheet while chasing because the current asset set
    // does not include a dedicated walk animation. That keeps movement stable
    // without inventing placeholder art.
    SetIdleAnimation(resetFrame);
}

void Molarbeast::SetMeleeAnimation(bool resetFrame)
{
    _texture = _attackAnim;
    _width = _attackAnim.width / (float)_sheetFrameCount;
    _height = _attackAnim.height;
    // The boss melee should read slowly enough that the player can react and
    // dash away from the enlarged attack range.
    _updateTime = 1.f / 5.f;
    _maxFrames = _sheetFrameCount;

    if (resetFrame)
    {
        _frame = 0;
        _runningTime = 0.f;
    }
}

void Molarbeast::SetRangedAnimation(bool resetFrame)
{
    _texture = _sharedRangedAnim;
    _width = _sharedRangedAnim.width / (float)_sheetFrameCount;
    _height = _sharedRangedAnim.height;
    _updateTime = 1.f / 9.f;
    _maxFrames = _sheetFrameCount;

    if (resetFrame)
    {
        _frame = 0;
        _runningTime = 0.f;
    }
}

void Molarbeast::SetDashAnimation(bool resetFrame)
{
    _texture = _sharedDashAnim;
    _width = _sharedDashAnim.width / (float)_sheetFrameCount;
    _height = _sharedDashAnim.height;
    _updateTime = 1.f / 12.f;
    _maxFrames = _sheetFrameCount;

    if (resetFrame)
    {
        _frame = 0;
        _runningTime = 0.f;
    }
}

void Molarbeast::SetHurtAnimation(bool resetFrame)
{
    _texture = _takeDamageAnim;
    _width = _takeDamageAnim.width / (float)_sheetFrameCount;
    _height = _takeDamageAnim.height;
    _updateTime = 1.f / 12.f;
    _maxFrames = _sheetFrameCount;

    if (resetFrame)
    {
        _frame = 0;
        _runningTime = 0.f;
    }
}

void Molarbeast::SetDeathAnimation(bool resetFrame)
{
    _texture = _deathAnim;
    _width = _deathAnim.width / (float)_sheetFrameCount;
    _height = _deathAnim.height;
    _updateTime = 1.f / 8.f;
    _maxFrames = _sheetFrameCount;

    if (resetFrame)
    {
        _frame = 0;
        _runningTime = 0.f;
    }
}

void Molarbeast::HandleChasing(float dt, const std::vector<Vector2>& propCenters)
{
    // Melee trigger and special-timer trigger are now owned by the behaviour
    // tree (Node A and Node B). This function is pure orbit / navigation.

    Vector2 oldPos = _worldPos;
    Vector2 playerCenter = _target->GetFeetWorldPos();
    Vector2 toPlayer = Vector2Subtract(playerCenter, _worldPos);

    if (toPlayer.x < 0.f) _rightLeft = -1.f;
    if (toPlayer.x > 0.f) _rightLeft = 1.f;

    // Orbit the player at a preferred radius, spinning around them rather than
    // charging straight in. When a prop blocks the path, A* waypoints take
    // priority and are briefly locked so the boss commits to routing around
    // the obstacle instead of re-deciding every frame and pushing into corners.
    static constexpr float _orbitRadius = 220.f;
    static constexpr float _orbitSpeed  = 0.55f;

    float distToPlayer = Vector2Distance(_worldPos, playerCenter);

    // Advance the orbit angle only when we're close enough to be circling;
    // if still far away we close the gap first and only start the spin once nearby.
    if (distToPlayer < _orbitRadius * 1.6f)
        _orbitAngle += _orbitSpeed * dt;

    // Orbit target: a point on the circle around the player
    Vector2 orbitPoint = {
        playerCenter.x + cosf(_orbitAngle) * _orbitRadius,
        playerCenter.y + sinf(_orbitAngle) * _orbitRadius
    };

    if (_navTargetLockTimer > 0.f)
        _navTargetLockTimer -= dt;

    if (_hasNavTarget)
    {
        bool needsNewLock = !_hasLockedNavTarget ||
            _navTargetLockTimer <= 0.f ||
            Vector2Distance(_lockedNavTarget, _navTarget) > _navTargetRefreshDistance ||
            Vector2Distance(_worldPos, _lockedNavTarget) <= _navTargetReachDistance;

        if (needsNewLock)
        {
            _lockedNavTarget = _navTarget;
            _hasLockedNavTarget = true;
            _navTargetLockTimer = _navTargetLockDuration;
        }
    }
    else
    {
        _hasLockedNavTarget = false;
        _navTargetLockTimer = 0.f;
    }

    Vector2 moveTarget = (_hasLockedNavTarget ? _lockedNavTarget : orbitPoint);
    Vector2 toTarget   = Vector2Subtract(moveTarget, _worldPos);

    if (Vector2Length(toTarget) > 0.01f)
    {
        Vector2 moveDir = Vector2Normalize(toTarget);

        if (_escapeTimer > 0.f && Vector2Length(_escapeDirection) > 0.01f)
        {
            _escapeTimer -= dt;
            moveDir = Vector2Normalize(Vector2Add(
                Vector2Scale(moveDir, 1.f - _escapeBlendStrength),
                Vector2Scale(_escapeDirection, _escapeBlendStrength)));
        }

        _worldPos = Vector2Add(_worldPos, Vector2Scale(moveDir, _speed * dt));
        SetWalkAnimation(false);

        // Boss-specific stuck recovery: if progress stalls, choose a
        // deterministic tangent around the nearest prop and commit to it
        // briefly. This gives the large boss enough local steering to escape
        // corner pressure without replacing the engine's A* routing.
        _stuckTimer += dt;
        if (_stuckTimer >= _stuckThreshold)
        {
            float moved = Vector2Distance(_worldPos, _stuckCheckPos);
            if (moved < _stuckMinMove)
            {
                Vector2 bestEscape = Vector2Zero();
                float bestPropDist = _stuckPropSearchRadius;

                for (const Vector2& propCenter : propCenters)
                {
                    float propDist = Vector2Distance(_worldPos, propCenter);
                    if (propDist >= bestPropDist || propDist <= 0.01f)
                        continue;

                    Vector2 awayFromProp = Vector2Normalize(Vector2Subtract(_worldPos, propCenter));
                    Vector2 tangentA{ -awayFromProp.y, awayFromProp.x };
                    Vector2 tangentB{ awayFromProp.y, -awayFromProp.x };
                    bestEscape = (Vector2DotProduct(tangentA, moveDir) >= Vector2DotProduct(tangentB, moveDir))
                        ? tangentA
                        : tangentB;
                    bestPropDist = propDist;
                }

                if (Vector2Length(bestEscape) <= 0.01f)
                {
                    Vector2 radialFromPlayer = Vector2Subtract(_worldPos, playerCenter);
                    if (Vector2Length(radialFromPlayer) > 0.01f)
                        bestEscape = Vector2Normalize(Vector2{ -radialFromPlayer.y, radialFromPlayer.x });
                    else
                        bestEscape = Vector2{ -moveDir.y, moveDir.x };
                }

                _escapeDirection = Vector2Normalize(Vector2Add(
                    Vector2Scale(bestEscape, 0.85f),
                    Vector2Scale(moveDir, 0.35f)));
                _escapeTimer = _escapeDuration;
                _navTargetLockTimer = std::max(_navTargetLockTimer, _navTargetLockDuration);
            }

            _stuckTimer = 0.f;
            _stuckCheckPos = _worldPos;
        }
    }
    else
    {
        SetIdleAnimation(false);
        _stuckTimer = 0.f;
        _stuckCheckPos = _worldPos;
        _escapeTimer = 0.f;
    }

    if (Vector2Distance(_worldPos, oldPos) > 0.5f)
        _stuckCheckPos = _worldPos;
}

void Molarbeast::HandleMelee()
{
    // Boss melee should behave like the regular enemy swing: stop moving,
    // commit to the attack animation, then apply damage during the strike.
    _velocity = Vector2Zero();

    // The boss swing should only become active once the animation reaches its
    // fourth frame so the player gets a readable telegraph before the hitbox
    // actually applies damage and knockback.
    if (_damageApplied || _frame < 3)
        return;

    // GetThreatCollisionRec already extends the hurtbox forward by
    // _attackFrontPadding — use it directly as the melee swing hitbox.
    Rectangle attackRec = GetThreatCollisionRec();

    if (CheckCollisionRecs(attackRec, _target->GetCollisionRec()))
    {
        _target->TakeFractionalDamage(_bossDamagePerHit, _worldPos);
        _target->StartForcedPush(GetPushDirectionToPlayer(), _bossPushSpeed);
    }

    _damageApplied = true;
}

void Molarbeast::HandleDashCharge(float dt)
{
    _chargeTimer -= dt;
    Vector2 toPlayer = Vector2Subtract(_target->GetFeetWorldPos(), _worldPos);
    if (toPlayer.x < 0.f) _rightLeft = -1.f;
    if (toPlayer.x > 0.f) _rightLeft = 1.f;

    if (_chargeTimer > 0.f)
        return;

    _dashDirection = (Vector2Length(toPlayer) > 0.01f)
        ? Vector2Normalize(toPlayer)
        : Vector2{ _rightLeft, 0.f };
    _state = State::Dashing;
    _dashedEnemies.clear();
    SetDashAnimation(true);
}

void Molarbeast::HandleDash(float dt, const std::vector<std::unique_ptr<Enemy>>& enemies)
{
    _worldPos = Vector2Add(_worldPos, Vector2Scale(_dashDirection, _dashSpeed * dt));

    for (const auto& enemy : enemies)
    {
        if (enemy.get() == this || !enemy->IsActive() || !enemy->IsAlive() || enemy->IsBoss())
            continue;
        if (HasAlreadyDashedEnemy(enemy.get()))
            continue;

        if (CheckCollisionRecs(GetCollisionRec(), enemy->GetCollisionRec()))
        {
            ScatterEnemy(*enemy);
            enemy->PlayHurtSound();
            _dashedEnemies.push_back(enemy.get());
        }
    }

    if (_target != nullptr && _target->IsAlive() &&
        !_target->IsBeingForcedPushed() &&
        CheckCollisionRecs(GetCollisionRec(), _target->GetCollisionRec()))
    {
        _target->TakeFractionalDamage(_bossDamagePerHit, _worldPos);
        _target->StartForcedPush(_dashDirection, 1200.f);
        SetSoundVolume(_attackSound, 0.45f);
        StopSound(_attackSound);
        PlaySound(_attackSound);
        FinishDash(false);
    }
}

void Molarbeast::HandleRangedCharge(float dt)
{
    _chargeTimer -= dt;
    Vector2 toPlayer = Vector2Subtract(_target->GetFeetWorldPos(), _worldPos);
    if (toPlayer.x < 0.f) _rightLeft = -1.f;
    if (toPlayer.x > 0.f) _rightLeft = 1.f;

    if (_frame > 2)
        _frame = 2;

    if (_chargeTimer > 0.f)
        return;

    _state = State::RangedVolley;
    _volleyShotsRemaining = GetRandomValue(3, 4);
    _volleyShotTimer = 0.f;
}

void Molarbeast::HandleRangedVolley(float dt)
{
    _volleyShotTimer -= dt;

    if (_volleyShotsRemaining > 0 && _volleyShotTimer <= 0.f)
    {
        _queuedLavaBallTarget = _target->GetFeetWorldPos();
        _pendingLavaBallShot = true;
        _volleyShotTimer = _volleyShotSpacing;
        --_volleyShotsRemaining;
        SetSoundVolume(_attackSound, 0.35f);
        StopSound(_attackSound);
        PlaySound(_attackSound);
    }

    // Transition to Recovery is handled in HandleAnimation once the ranged
    // sheet plays through to the end after all shots are fired. This lets
    // the back half of the animation finish before the state changes.
}

void Molarbeast::HandleRecovery(float dt)
{
    _recoveryTimer -= dt;
    if (_recoveryTimer > 0.f)
        return;

    // Always reset the special cooldown on recovery so both dash and ranged
    // volleys restart the timer. Without this the ranged path left
    // _specialTimer expired and the boss immediately looped into another
    // special the moment it returned to Chasing.
    ResetSpecialCooldown();
    _state = State::Chasing;
    SetIdleAnimation(true);
}

void Molarbeast::HandleAnimation(float dt)
{
    _runningTime += dt;

    // The ranged charge should visibly freeze on frame three until the charge
    // finishes, then continue the rest of the attack sheet while the volley is
    // being released.
    if (_state == State::RangedCharging && _frame >= 2)
        return;

    if (_runningTime < _updateTime)
        return;

    _runningTime = 0.f;
    ++_frame;

    if (_frame < _maxFrames)
        return;

    if (_dying)
    {
        _frame = _maxFrames - 1;
        return;
    }

    if (IsFrozen())
    {
        _frame = _maxFrames - 1;
        return;
    }

    if (_takingDamage)
    {
        _takingDamage = false;
        if (_state == State::DashCharging)
            SetDashAnimation(true);
        else if (_state == State::RangedCharging)
            SetRangedAnimation(true);
        else
            SetIdleAnimation(true);
        return;
    }

    switch (_state)
    {
    case State::MeleeAttacking:
        _state = State::Chasing;
        _attacking = false;
        SetIdleAnimation(true);
        break;
    case State::RangedVolley:
        if (_volleyShotsRemaining <= 0)
        {
            // All shots fired and the sheet just played through — clean exit.
            ResetSpecialCooldown();
            _state = State::Recovery;
            _recoveryTimer = _recoveryDuration;
            SetIdleAnimation(true);
        }
        else
        {
            // Still firing — loop the ranged sheet so it keeps cycling.
            _frame = 0;
        }
        break;
    default:
        _frame = 0;
        break;
    }
}

void Molarbeast::TryDealContactDamage()
{
    // The boss should punish standing too close from any angle, not just when
    // its melee animation happens to be active. This makes spacing matter even
    // though the sprite is visually large and round.
    if (_target == nullptr || !_target->IsAlive())
        return;
    if (_target->IsBeingForcedPushed())
        return;
    // No contact damage during melee (its own hitbox handles damage), dash
    // (handled by HandleDash), or ranged volley (gives the player a close-range
    // dodge window — step in to avoid lavaballs without being simultaneously punished).
    if (_state == State::MeleeAttacking || _state == State::Dashing || _state == State::RangedVolley)
        return;
    if (_contactCooldown > 0.f)
        return;

    if (!CheckCollisionRecs(GetBodyContactRec(), _target->GetCollisionRec()))
        return;

    _target->TakeFractionalDamage(_bossDamagePerHit, _worldPos);
    _target->StartForcedPush(GetPushDirectionToPlayer(), _bossPushSpeed);
    ResetContactCooldown();
}

void Molarbeast::ScatterEnemy(Enemy& enemy) const
{
    Vector2 away = Vector2Subtract(enemy.GetWorldPos(), _worldPos);
    if (Vector2Length(away) <= 0.01f)
        away = Vector2{ 1.f, 0.f };
    away = Vector2Normalize(away);

    Vector2 impulse = Vector2Scale(away, _scatterImpulse);
    enemy.ApplyExternalImpulse(impulse, true);
}

bool Molarbeast::HasAlreadyDashedEnemy(const Enemy* enemy) const
{
    for (const Enemy* rushedEnemy : _dashedEnemies)
    {
        if (rushedEnemy == enemy)
            return true;
    }

    return false;
}

void Molarbeast::ResetSpecialCooldown()
{
    // Keep special timing expressed in seconds everywhere else in the boss
    // code. The x10 conversion is only for GetRandomValue, which takes ints.
    _specialTimer = GetRandomValue(
        (int)(GetSpecialCooldownMin() * 10.f),
        (int)(GetSpecialCooldownMax() * 10.f)) / 10.0f;
}

void Molarbeast::ResetMeleeCooldown()
{
    _meleeCooldown = _attackCooldownBase;
}

void Molarbeast::ResetContactCooldown()
{
    // Contact pressure exists to stop the player from standing inside the
    // boss. It is intentionally faster than the dedicated melee cadence so the
    // player is punished for crowding without preventing the real melee state
    // from firing on its own timer.
    _contactCooldown = _contactCooldownBase;
}

void Molarbeast::FinishDash(bool blockedByArena)
{
    _state = State::Recovery;
    _recoveryTimer = blockedByArena ? 0.8f : _recoveryDuration;
    _impactShakeRequested = true;
    _velocity = Vector2Zero();
    ResetSpecialCooldown();
    SetIdleAnimation(true);
}

void Molarbeast::BeginRandomSpecial()
{
    const bool useDash = GetRandomValue(0, 1) == 0;
    _chargeDuration = GetChargeDuration();
    _chargeTimer = _chargeDuration;

    if (useDash)
    {
        _state = State::DashCharging;
        SetDashAnimation(true);
    }
    else
    {
        _state = State::RangedCharging;
        SetRangedAnimation(true);
    }
}

bool Molarbeast::IsInReducedDamageState() const
{
    return _state == State::DashCharging || _state == State::RangedCharging;
}

Vector2 Molarbeast::GetPushDirectionToPlayer() const
{
    if (_target == nullptr)
        return Vector2{ _rightLeft, 0.f };

    // Push the player directly away from the boss body so both melee hits and
    // close-contact punishment create clear space. Falling back to facing keeps
    // the result stable even if the centers overlap exactly.
    Vector2 away = Vector2Subtract(_target->GetWorldPos(), _worldPos);
    if (Vector2Length(away) <= 0.01f)
        return Vector2{ _rightLeft, 0.f };

    return Vector2Normalize(away);
}

float Molarbeast::GetSpecialCooldownMin() const
{
    // Boss pressure should stay high, but the player still needs short read
    // windows between specials so the fight doesn't feel like uninterrupted
    // dash/fireball spam.
    return (_health <= _maxHealth * 0.3f) ? 3.5f : 4.5f;
}

float Molarbeast::GetSpecialCooldownMax() const
{
    return (_health <= _maxHealth * 0.3f) ? 4.0f : 5.5f;
}

float Molarbeast::GetChargeDuration() const
{
    return 1.25f;
}
