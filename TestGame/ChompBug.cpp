#include "ChompBug.h"
#include "VirtualCanvas.h"
#include "AssetPaths.h"
#include "Character.h"
#include "raymath.h"
#include <algorithm>
#include <cmath>

Texture2D ChompBug::_sharedIdleAnim{};
Texture2D ChompBug::_sharedFlyAnim{};
Texture2D ChompBug::_sharedMeleeAnim{};
Texture2D ChompBug::_sharedSpitAnim{};
Texture2D ChompBug::_sharedHurtAnim{};
Texture2D ChompBug::_sharedDeathAnim{};
Sound     ChompBug::_sharedAttackSound{};
Sound     ChompBug::_sharedHurtSound{};
Sound     ChompBug::_sharedDeathSound{};
Sound     ChompBug::_sharedSpitSound{};
bool      ChompBug::_sharedResourcesLoaded = false;

ChompBug::ChompBug(Vector2 pos) : Enemy(pos) {}
ChompBug::~ChompBug() {}

void ChompBug::Init()
{
    EnsureSharedResourcesLoaded();
    _attackSound = _sharedAttackSound;
    _hurtSound   = _sharedHurtSound;
    _deathSound  = _sharedDeathSound;
    ResetForSpawn(_worldPos);
}

void ChompBug::ResetForSpawn(Vector2 pos)
{
    _worldPos = pos; _worldPosLastFrame = pos; _homePos = pos;
    _velocity = Vector2Zero();
    _isActive = true;

    _stableFrameW = (float)_sharedIdleAnim.width / (float)_sheetFrameCount;
    _stableFrameH = (float)_sharedIdleAnim.height;

    SetAnimation(_sharedFlyAnim, 1.f / 12.f, true);
    _scale = _bossScale;

    _health = 55.f; _maxHealth = 55.f;
    _attackPower = 1.f;
    _speed = 260.f;
    _expValue = _bossBaseExpValue;

    _hitTimer = 0.f; _deathTimer = 0.7f; _freezeTimer = 0.f;
    _isCharged = false; _chargeNextStunTime = 0.f; _electricChargeTotalTimer = 0.f;
    _isEliteMiniboss = false; _isInvulnerable = false; _leapInvulnerable = false;
    _takingDamage = false; _attacking = false; _dying = false;

    _state = State::Orbiting;
    _stateTimer = 0.f;
    _contactCooldown = 1.0f;
    _diveCooldown = 2.6f;
    _spitCooldown = 4.0f;
    _orbitAngle = (float)GetRandomValue(0, 628) / 100.f;
    _orbitSign = (GetRandomValue(0, 1) == 0) ? -1.f : 1.f;
    _diveTimer = 0.f;
    _diveHitApplied = false;
    _pendingSpit = false;
    _spitDirection = Vector2Zero();
    _spitCount = 3;
    _buzzTimer = 0.f;

    _forcedPushActive = false; _forcedPushDirection = Vector2Zero(); _forcedPushSpeed = 0.f;
    _pendingBurns.clear();
    _waypoints.clear(); _waypointIndex = 0;

    ResetTuningState();
    ApplyStoredTuning();
}

void ChompBug::SetAnimation(const Texture2D& sheet, float frameTime, bool resetFrame)
{
    _texture = sheet;
    _width  = (float)sheet.width / (float)_sheetFrameCount;
    _height = (float)sheet.height;
    _updateTime = frameTime;
    _maxFrames  = _sheetFrameCount;
    if (resetFrame) { _frame = 0; _runningTime = 0.f; }
}

int ChompBug::GetCurrentAnimSlot() const
{
    if (_texture.id == _sharedIdleAnim.id)  return 0;
    if (_texture.id == _sharedFlyAnim.id)   return 1;
    if (_texture.id == _sharedMeleeAnim.id) return 2;
    if (_texture.id == _sharedSpitAnim.id)  return 3;
    if (_texture.id == _sharedHurtAnim.id)  return 4;
    if (_texture.id == _sharedDeathAnim.id) return 5;
    return 0;
}

const char* ChompBug::GetEditorAnimName(int index) const
{
    static const char* kNames[6] = { "Idle", "Fly", "Chomp", "Spit", "Hurt", "Death" };
    return (index >= 0 && index < 6) ? kNames[index] : "";
}

void ChompBug::PlayEditorAnim(int index)
{
    const Texture2D* sheets[6] = {
        &_sharedIdleAnim, &_sharedFlyAnim, &_sharedMeleeAnim,
        &_sharedSpitAnim, &_sharedHurtAnim, &_sharedDeathAnim
    };
    if (index < 0 || index > 5) return;
    float frameTimeOverride = _editorAnimFrameTimes[index];
    SetAnimation(*sheets[index], (frameTimeOverride > 0.f) ? frameTimeOverride : 1.f / 8.f, true);
}

void ChompBug::Update(float dt, Vector2 heroWorldPos, Vector2, bool,
    const std::vector<std::unique_ptr<Enemy>>&, const std::vector<Vector2>&)
{
    if (!_isActive) return;
    _worldPosLastFrame = _worldPos;
    _buzzTimer += dt;

    UpdateHit(dt);
    UpdateBurns(dt);
    UpdateElectricCharge(dt);

    if (_freezeTimer > 0.f)     _freezeTimer -= dt;
    if (_contactCooldown > 0.f) _contactCooldown -= dt;
    if (_state == State::Orbiting)
    {
        if (_diveCooldown > 0.f) _diveCooldown -= dt;
        if (_spitCooldown > 0.f) _spitCooldown -= dt;
    }

    if (!_dying && _target != nullptr)
    {
        bool controlled = IsFrozen() || IsElectroStunned();

        switch (_state)
        {
        case State::Orbiting:
        {
            if (controlled) break;

            // Constant buzzing orbit around the player at mid range.
            float orbitSpeed = IsEnraged() ? _orbitSpeed * 1.35f : _orbitSpeed;
            _orbitAngle += orbitSpeed * _orbitSign * dt;
            float radius = IsEnraged() ? _orbitRadius * 0.82f : _orbitRadius;
            Vector2 desired{
                heroWorldPos.x + cosf(_orbitAngle) * radius,
                heroWorldPos.y + sinf(_orbitAngle) * radius * 0.72f
            };
            desired.x = std::clamp(desired.x, 170.f, (float)kVirtualWidth  - 170.f);
            desired.y = std::clamp(desired.y, 170.f, (float)kVirtualHeight - 170.f);
            Vector2 toDesired = Vector2Subtract(desired, _worldPos);
            _worldPos = Vector2Add(_worldPos, Vector2Scale(toDesired, std::min(1.f, 4.5f * dt)));
            _worldPos.y += sinf(_buzzTimer * 7.f) * 14.f * dt;

            if (GetRandomValue(0, 240) == 0)
                _orbitSign = -_orbitSign;

            // Specials.
            if (_diveCooldown <= 0.f)
            {
                _state = State::DiveAiming; _stateTimer = 0.f;
                SetAnimation(_sharedIdleAnim, 1.f / 12.f, true);
            }
            else if (_spitCooldown <= 0.f)
            {
                _state = State::SpitCasting; _stateTimer = 0.f;
                SetAnimation(_sharedSpitAnim, _spitCastDuration / (float)_sheetFrameCount, true);
                PlaySound(_sharedSpitSound);
            }
            break;
        }

        case State::DiveAiming:
        {
            _stateTimer += dt;
            // Line locks through the player with overshoot on both ends.
            Vector2 toPlayer = Vector2Subtract(_target->GetFeetWorldPos(), _worldPos);
            if (Vector2LengthSqr(toPlayer) > 0.0001f)
            {
                Vector2 dir = Vector2Normalize(toPlayer);
                _diveStart = _worldPos;
                _diveEnd = Vector2Add(_target->GetFeetWorldPos(), Vector2Scale(dir, _diveOvershoot));
                _diveEnd.x = std::clamp(_diveEnd.x, 150.f, (float)kVirtualWidth  - 150.f);
                _diveEnd.y = std::clamp(_diveEnd.y, 150.f, (float)kVirtualHeight - 150.f);
            }
            float aimDuration = IsEnraged() ? _diveAimDuration * 0.7f : _diveAimDuration;
            if (_stateTimer >= aimDuration)
            {
                _state = State::Diving;
                _diveTimer = 0.f;
                _diveHitApplied = false;
                SetAnimation(_sharedMeleeAnim, _diveTravelDuration / (float)_sheetFrameCount, true);
                PlaySound(_attackSound);
            }
            break;
        }

        case State::Diving:
        {
            _diveTimer += dt;
            float t = std::min(1.f, _diveTimer / _diveTravelDuration);
            _worldPos = Vector2Lerp(_diveStart, _diveEnd, t);

            if (!_diveHitApplied && _target->IsAlive() &&
                CheckCollisionRecs(GetBodyContactRec(), _target->GetCollisionRec()))
            {
                _diveHitApplied = true;
                _target->TakeFractionalDamage(1.0f, _worldPos);
                _target->StartForcedPush(GetPushDirectionToPlayer(), _bossPushSpeed);
            }

            if (t >= 1.f)
            {
                _state = State::Recovery; _stateTimer = 0.f;
                _diveCooldown = IsEnraged() ? _diveCooldownBase * 0.5f : _diveCooldownBase;
                SetAnimation(_sharedFlyAnim, 1.f / 12.f, true);
            }
            break;
        }

        case State::SpitCasting:
            _stateTimer += dt;
            if (_stateTimer >= _spitCastDuration)
            {
                Vector2 toPlayer = Vector2Subtract(_target->GetFeetWorldPos(), _worldPos);
                _pendingSpit = true;
                _spitDirection = (Vector2LengthSqr(toPlayer) > 0.0001f)
                    ? Vector2Normalize(toPlayer) : Vector2{ 1.f, 0.f };
                _spitCount = IsEnraged() ? 5 : 3;
                _state = State::Recovery; _stateTimer = 0.f;
                _spitCooldown = _spitCooldownBase;
                SetAnimation(_sharedFlyAnim, 1.f / 12.f, true);
            }
            break;

        case State::Recovery:
            // Post-dive hover — the melee punish window.
            _stateTimer += dt;
            if (_stateTimer >= _recoveryDuration)
            {
                _state = State::Orbiting; _stateTimer = 0.f;
            }
            break;
        }

        if (_state != State::Diving)
            TryDealContactDamage();

        Vector2 toPlayerFacing = Vector2Subtract(heroWorldPos, _worldPos);
        if (toPlayerFacing.x < -20.f) _rightLeft = -1.f;
        if (toPlayerFacing.x >  20.f) _rightLeft =  1.f;
    }

    HandleAnimation(dt);
}

void ChompBug::TryDealContactDamage()
{
    if (_target == nullptr || !_target->IsAlive()) return;
    if (_target->IsBeingForcedPushed()) return;
    if (_contactCooldown > 0.f) return;
    if (!CheckCollisionRecs(GetBodyContactRec(), _target->GetCollisionRec())) return;

    _target->TakeFractionalDamage(_bossDamagePerHit, _worldPos);
    _target->StartForcedPush(GetPushDirectionToPlayer(), _bossPushSpeed);
    _contactCooldown = _contactCooldownBase;
}

Rectangle ChompBug::GetBodyContactRec() const
{
    Rectangle bodyRec = GetCollisionRec();
    bodyRec.x += 22.f; bodyRec.y += 18.f;
    bodyRec.width  = std::max(1.f, bodyRec.width  - 44.f);
    bodyRec.height = std::max(1.f, bodyRec.height - 36.f);
    return bodyRec;
}

Vector2 ChompBug::GetPushDirectionToPlayer() const
{
    if (_target == nullptr) return Vector2{ 1.f, 0.f };
    Vector2 away = Vector2Subtract(_target->GetWorldPos(), _worldPos);
    return (Vector2Length(away) > 0.01f) ? Vector2Normalize(away) : Vector2{ 1.f, 0.f };
}

void ChompBug::OnSpitFired()
{
    _pendingSpit = false;
    _spitDirection = Vector2Zero();
}

void ChompBug::HandleAnimation(float dt)
{
    _runningTime += dt;
    if (_runningTime >= _updateTime)
    {
        _runningTime = 0.f;
        _frame++;
        if (_frame >= _maxFrames)
        {
            if (_dying || IsFrozen()) { _frame = _maxFrames - 1; return; }
            if (_takingDamage)
            {
                _takingDamage = false;
                SetAnimation(_sharedFlyAnim, 1.f / 12.f, true);
                return;
            }
            _frame = 0;
        }
    }
}

void ChompBug::DrawEnemy(Vector2 cameraRef)
{
    if (!_isActive) return;

    float drawWidth  = _stableFrameW * _scale;
    float drawHeight = _stableFrameH * _scale;

    Vector2 screenPos = Vector2Subtract(_worldPos, cameraRef);
    screenPos.x += kVirtualWidth  / 2.f;
    screenPos.y += kVirtualHeight / 2.f + sinf(_buzzTimer * 7.f) * 6.f;

    // Dive line telegraph.
    if (_state == State::DiveAiming)
    {
        Vector2 startScreen = screenPos;
        Vector2 endScreen = Vector2Subtract(_diveEnd, cameraRef);
        endScreen.x += kVirtualWidth / 2.f; endScreen.y += kVirtualHeight / 2.f;
        float pulse = sinf((float)GetTime() * 14.f) * 0.5f + 0.5f;
        DrawLineEx(startScreen, endScreen, 10.f, Fade(Color{ 120, 230, 60, 255 }, 0.14f + 0.08f * pulse));
        DrawLineEx(startScreen, endScreen, 4.f,  Fade(Color{ 160, 250, 90, 255 }, 0.5f + 0.2f * pulse));
    }

    // Flying shadow far below the body.
    DrawEllipse((int)screenPos.x, (int)(screenPos.y + drawHeight * 0.62f),
        drawWidth * 0.24f, drawHeight * 0.08f, Fade(BLACK, 0.30f));

    bool burning = !_pendingBurns.empty();
    Color tint = IsElectroStunned() ? Color{ 255, 255,  60, 255 } :
                 IsFrozen()         ? Color{ 140, 200, 255, 255 } :
                 burning            ? Color{ 255, 180, 180, 255 } :
                 IsEnraged()        ? Color{ 220, 255, 180, 255 } :
                                      WHITE;

    Vector2 animDrawOffset = GetCurrentAnimDrawOffset();
    Rectangle source{ _frame * _width, 0.f, _rightLeft * _width, _height };
    Rectangle dest{ screenPos.x - drawWidth / 2.f + animDrawOffset.x,
                    screenPos.y - drawHeight / 2.f + animDrawOffset.y, drawWidth, drawHeight };
    DrawTexturePro(_texture, source, dest, Vector2{}, 0.f, tint);

    DrawHealthBar(screenPos, drawWidth, drawHeight);
}

Rectangle ChompBug::GetCollisionRec() const
{
    Rectangle animBodyRect;
    if (GetAnimBodyRectWorld(animBodyRect))
        return animBodyRect;
    if (_hasTunedCollision)
        return GetTunedCollisionRec();

    float halfW = _stableFrameW * _scale * 0.5f;
    float halfH = _stableFrameH * _scale * 0.5f;
    return Rectangle{ _worldPos.x - halfW * 0.58f, _worldPos.y - halfH * 0.50f, halfW * 1.16f, halfH * 1.00f };
}

Capsule2D ChompBug::GetCapsule() const
{
    Capsule2D animBodyCapsule;
    if (GetAnimBodyCapsuleWorld(animBodyCapsule))
        return animBodyCapsule;
    if (_capsuleRadius > 0.f)
        return Capsule2D{ { _worldPos.x + _capsuleOffset.x, _worldPos.y + _capsuleOffset.y }, _capsuleHalfHeight, _capsuleRadius };
    return Capsule2D{ { _worldPos.x, _worldPos.y }, 0.f, _stableFrameW * _scale * 0.26f };
}

void ChompBug::DrawHealthBar(Vector2 screenPos, float w, float h)
{
    if (_health <= 0.f) return;
    float pct = _health / _maxHealth;
    float barWidth = w * 0.8f, barHeight = 8.f;
    float barX = screenPos.x - barWidth / 2.f;
    float barY = screenPos.y - h * 0.62f - 14.f;
    DrawRectangle((int)barX, (int)barY, (int)barWidth, (int)barHeight, RED);
    DrawRectangle((int)barX, (int)barY, (int)(barWidth * pct), (int)barHeight, GREEN);
}

void ChompBug::TakeDamage(int damage, Vector2 attackerPos)
{
    (void)attackerPos;
    if (_dying || _hitTimer > 0.f) return;

    // Half damage mid-dive: it's moving too fast for clean hits.
    int appliedDamage = damage;
    if (_state == State::Diving)
        appliedDamage = std::max(1, (int)std::ceil(damage * 0.5f));

    _health -= (float)appliedDamage;
    if (_health > 0.f) PlayHurtSound();

    if (_health <= 0.f)
    {
        _health = 0.f; _dying = true;
        _takingDamage = false; _attacking = false;
        _pendingSpit = false;
        _state = State::Recovery;
        _deathTimer = 0.7f;
        SetAnimation(_sharedDeathAnim, 1.f / 6.f, true);
        PlayDeathSound();
        return;
    }

    _velocity = Vector2Zero();
    if (_state == State::Orbiting)
    {
        _takingDamage = true;
        SetAnimation(_sharedHurtAnim, 1.f / 12.f, true);
    }
    _hitTimer = 0.02f;
}

void ChompBug::ApplyFreeze(float duration)
{
    if (_dying || !IsAlive()) return;
    if (_state == State::Diving) return;
    if (duration > _freezeTimer) _freezeTimer = duration;
}

void ChompBug::SetWaveScale(int wave)
{
    (void)wave;
    _expValue = _bossBaseExpValue;
    _health = 55.f; _maxHealth = 55.f;
    _speed = 260.f; _attackPower = 1.f;
}

void ChompBug::EnsureSharedResourcesLoaded()
{
    if (_sharedResourcesLoaded) return;

    _sharedIdleAnim  = LoadTexture(AssetPath("Bosses/ChompBugIdle.png").c_str());
    _sharedFlyAnim   = LoadTexture(AssetPath("Bosses/ChompBugFly.png").c_str());
    _sharedMeleeAnim = LoadTexture(AssetPath("Bosses/ChompBugMeleeAttack.png").c_str());
    _sharedSpitAnim  = LoadTexture(AssetPath("Bosses/ChompBugSpit.png").c_str());
    _sharedHurtAnim  = LoadTexture(AssetPath("Bosses/ChompBugHurt.png").c_str());
    _sharedDeathAnim = LoadTexture(AssetPath("Bosses/ChompBugDeath.png").c_str());
    _sharedAttackSound = LoadSound(AssetPath("Sounds/SwordSwipe2.ogg").c_str());
    _sharedHurtSound   = LoadSound(AssetPath("Sounds/SmallMonsterDamage.ogg").c_str());
    _sharedDeathSound  = LoadSound(AssetPath("Sounds/MonsterDeath.ogg").c_str());
    _sharedSpitSound   = LoadSound(AssetPath("Sounds/GS1_Spell_Fire.ogg").c_str());
    _sharedResourcesLoaded = true;
}

void ChompBug::UnloadSharedResources()
{
    if (!_sharedResourcesLoaded) return;

    UnloadTexture(_sharedIdleAnim);  UnloadTexture(_sharedFlyAnim);
    UnloadTexture(_sharedMeleeAnim); UnloadTexture(_sharedSpitAnim);
    UnloadTexture(_sharedHurtAnim);  UnloadTexture(_sharedDeathAnim);
    UnloadSound(_sharedAttackSound); UnloadSound(_sharedHurtSound);
    UnloadSound(_sharedDeathSound);  UnloadSound(_sharedSpitSound);
    _sharedIdleAnim = Texture2D{};  _sharedFlyAnim = Texture2D{};
    _sharedMeleeAnim = Texture2D{}; _sharedSpitAnim = Texture2D{};
    _sharedHurtAnim = Texture2D{};  _sharedDeathAnim = Texture2D{};
    _sharedAttackSound = Sound{};   _sharedHurtSound = Sound{};
    _sharedDeathSound = Sound{};    _sharedSpitSound = Sound{};
    _sharedResourcesLoaded = false;
}
