#include "Osiris.h"
#include "VirtualCanvas.h"
#include "AssetPaths.h"
#include "Character.h"
#include "raymath.h"
#include <algorithm>
#include <cmath>

Texture2D Osiris::_sharedIdleAnim{};
Texture2D Osiris::_sharedWalkAnim{};
Texture2D Osiris::_sharedMeleeAnim{};
Texture2D Osiris::_sharedMagicAnim{};
Texture2D Osiris::_sharedHurtAnim{};
Texture2D Osiris::_sharedDeathAnim{};
Sound     Osiris::_sharedAttackSound{};
Sound     Osiris::_sharedHurtSound{};
Sound     Osiris::_sharedDeathSound{};
Sound     Osiris::_sharedCastSound{};
bool      Osiris::_sharedResourcesLoaded = false;

Osiris::Osiris(Vector2 pos) : Enemy(pos) {}
Osiris::~Osiris() {}

void Osiris::Init()
{
    EnsureSharedResourcesLoaded();
    _attackSound = _sharedAttackSound;
    _hurtSound   = _sharedHurtSound;
    _deathSound  = _sharedDeathSound;
    ResetForSpawn(_worldPos);
}

void Osiris::ResetForSpawn(Vector2 pos)
{
    _worldPos = pos; _worldPosLastFrame = pos; _homePos = pos;
    _velocity = Vector2Zero();
    _isActive = true;

    _stableFrameW = (float)_sharedIdleAnim.width / (float)_sheetFrameCount;
    _stableFrameH = (float)_sharedIdleAnim.height;

    SetAnimation(_sharedIdleAnim, 1.f / 8.f, true);
    _scale = _bossScale;

    _health = 58.f; _maxHealth = 58.f;
    _attackPower = 1.f;
    _speed = _moveSpeed;
    _expValue = _bossBaseExpValue;

    _hitTimer = 0.f; _deathTimer = 0.7f; _freezeTimer = 0.f;
    _isCharged = false; _chargeNextStunTime = 0.f; _electricChargeTotalTimer = 0.f;
    _isEliteMiniboss = false; _isInvulnerable = false; _leapInvulnerable = false;
    _takingDamage = false; _attacking = false; _dying = false;

    _state = State::Stalking;
    _stateTimer = 0.f;
    _meleeCooldown = 1.2f;
    _contactCooldown = 1.0f;
    _novaCooldown = 3.5f;
    _volleyCooldown = 2.2f;
    _teleportCooldown = 1.8f;
    _pendingNova = false;
    _novaBoltCount = 8;
    _pendingVolley = false;
    _volleyDirection = Vector2Zero();
    _volleyBoltCount = 3;
    _teleportTarget = pos;

    _forcedPushActive = false; _forcedPushDirection = Vector2Zero(); _forcedPushSpeed = 0.f;
    _pendingBurns.clear();
    _waypoints.clear(); _waypointIndex = 0;

    ResetTuningState();
    ApplyStoredTuning();
}

void Osiris::SetAnimation(const Texture2D& sheet, float frameTime, bool resetFrame)
{
    _texture = sheet;
    _width  = (float)sheet.width / (float)_sheetFrameCount;
    _height = (float)sheet.height;
    _updateTime = frameTime;
    _maxFrames  = _sheetFrameCount;
    if (resetFrame) { _frame = 0; _runningTime = 0.f; }
}

int Osiris::GetCurrentAnimSlot() const
{
    if (_texture.id == _sharedIdleAnim.id)  return 0;
    if (_texture.id == _sharedWalkAnim.id)  return 1;
    if (_texture.id == _sharedMeleeAnim.id) return 2;
    if (_texture.id == _sharedMagicAnim.id) return 3;
    if (_texture.id == _sharedHurtAnim.id)  return 4;
    if (_texture.id == _sharedDeathAnim.id) return 5;
    return 0;
}

const char* Osiris::GetEditorAnimName(int index) const
{
    static const char* kNames[6] = { "Idle", "Walk", "Melee", "Magic", "Hurt", "Death" };
    return (index >= 0 && index < 6) ? kNames[index] : "";
}

void Osiris::PlayEditorAnim(int index)
{
    const Texture2D* sheets[6] = {
        &_sharedIdleAnim, &_sharedWalkAnim, &_sharedMeleeAnim,
        &_sharedMagicAnim, &_sharedHurtAnim, &_sharedDeathAnim
    };
    if (index < 0 || index > 5) return;
    float frameTimeOverride = _editorAnimFrameTimes[index];
    SetAnimation(*sheets[index], (frameTimeOverride > 0.f) ? frameTimeOverride : 1.f / 8.f, true);
}

void Osiris::Update(float dt, Vector2 heroWorldPos, Vector2, bool,
    const std::vector<std::unique_ptr<Enemy>>&, const std::vector<Vector2>&)
{
    if (!_isActive) return;
    _worldPosLastFrame = _worldPos;

    UpdateHit(dt);
    UpdateBurns(dt);
    UpdateElectricCharge(dt);

    if (_freezeTimer > 0.f)     _freezeTimer -= dt;
    if (_meleeCooldown > 0.f)   _meleeCooldown -= dt;
    if (_contactCooldown > 0.f) _contactCooldown -= dt;
    if (_state == State::Stalking)
    {
        if (_novaCooldown > 0.f)     _novaCooldown -= dt;
        if (_volleyCooldown > 0.f)   _volleyCooldown -= dt;
        if (_teleportCooldown > 0.f) _teleportCooldown -= dt;
    }

    if (!_dying && _target != nullptr)
    {
        bool controlled = IsFrozen() || IsElectroStunned();
        Vector2 toPlayer = Vector2Subtract(heroWorldPos, _worldPos);
        float dist = Vector2Length(toPlayer);

        switch (_state)
        {
        case State::Stalking:
        {
            if (controlled) break;

            if (toPlayer.x < -20.f) _rightLeft = -1.f;
            if (toPlayer.x >  20.f) _rightLeft =  1.f;

            // Sand Step away whenever the player gets too close.
            if (dist < _panicDistance && _teleportCooldown <= 0.f)
            {
                BeginTeleport();
                break;
            }

            // Cornered melee if teleport is still recharging.
            if (dist < _meleeRange && _meleeCooldown <= 0.f)
            {
                _state = State::MeleeAttacking;
                _damageApplied = false;
                SetAnimation(_sharedMeleeAnim, 1.f / 11.f, true);
                PlaySound(_attackSound);
                break;
            }

            if (_novaCooldown <= 0.f)
            {
                _state = State::NovaCasting; _stateTimer = 0.f;
                SetAnimation(_sharedMagicAnim, _novaCastDuration / (float)_sheetFrameCount, true);
                PlaySound(_sharedCastSound);
                break;
            }
            if (_volleyCooldown <= 0.f && dist > 260.f)
            {
                _state = State::VolleyCasting; _stateTimer = 0.f;
                SetAnimation(_sharedMagicAnim, _volleyCastDuration / (float)_sheetFrameCount, true);
                PlaySound(_sharedCastSound);
                break;
            }

            // Slow regal drift that keeps the comfortable casting gap.
            Vector2 dir = (dist > 0.01f) ? Vector2Scale(toPlayer, 1.f / dist) : Vector2{ 1.f, 0.f };
            float wanted = (dist > 520.f) ? 1.f : (dist < 340.f ? -0.8f : 0.f);
            if (wanted != 0.f)
            {
                _worldPos = Vector2Add(_worldPos, Vector2Scale(dir, _moveSpeed * wanted * dt));
                if (_texture.id != _sharedWalkAnim.id && !_takingDamage)
                    SetAnimation(_sharedWalkAnim, 1.f / 9.f, true);
            }
            else if (_texture.id != _sharedIdleAnim.id && !_takingDamage)
            {
                SetAnimation(_sharedIdleAnim, 1.f / 8.f, true);
            }
            break;
        }

        case State::MeleeAttacking:
            _velocity = Vector2Zero();
            if (!_damageApplied && _frame >= 3)
            {
                Rectangle attackRec;
                if (!GetAnimMeleeRectWorld(2, attackRec))
                {
                    attackRec = GetBodyContactRec();
                    attackRec.width += 100.f;
                    if (_rightLeft < 0.f) attackRec.x -= 100.f;
                    attackRec.y -= 20.f; attackRec.height += 40.f;
                }
                if (CheckCollisionRecs(attackRec, _target->GetCollisionRec()))
                {
                    _target->TakeFractionalDamage(_bossDamagePerHit, _worldPos);
                    _target->StartForcedPush(GetPushDirectionToPlayer(), _bossPushSpeed);
                }
                _damageApplied = true;
            }
            if (_frame >= _maxFrames - 1)
            {
                _state = State::Recovery; _stateTimer = 0.f;
                _meleeCooldown = _meleeCooldownBase;
                SetAnimation(_sharedIdleAnim, 1.f / 8.f, true);
            }
            break;

        case State::NovaCasting:
            _velocity = Vector2Zero();
            _stateTimer += dt;
            if (_stateTimer >= _novaCastDuration)
            {
                _pendingNova = true;
                _novaBoltCount = IsEnraged() ? 12 : 8;
                _state = State::Recovery; _stateTimer = 0.f;
                _novaCooldown = IsEnraged() ? _novaCooldownBase * 0.7f : _novaCooldownBase;
                SetAnimation(_sharedIdleAnim, 1.f / 8.f, true);
            }
            break;

        case State::VolleyCasting:
            _velocity = Vector2Zero();
            _stateTimer += dt;
            if (_stateTimer >= _volleyCastDuration)
            {
                Vector2 aim = Vector2Subtract(_target->GetFeetWorldPos(), _worldPos);
                _pendingVolley = true;
                _volleyDirection = (Vector2LengthSqr(aim) > 0.0001f) ? Vector2Normalize(aim) : Vector2{ 1.f, 0.f };
                _volleyBoltCount = IsEnraged() ? 5 : 3;
                _state = State::Recovery; _stateTimer = 0.f;
                _volleyCooldown = IsEnraged() ? _volleyCooldownBase * 0.7f : _volleyCooldownBase;
                SetAnimation(_sharedIdleAnim, 1.f / 8.f, true);
            }
            break;

        case State::TeleportOut:
            _velocity = Vector2Zero();
            _stateTimer += dt;
            if (_stateTimer >= _teleportOutDuration)
            {
                _worldPos = _teleportTarget;
                _state = State::TeleportIn; _stateTimer = 0.f;
            }
            break;

        case State::TeleportIn:
            _velocity = Vector2Zero();
            _stateTimer += dt;
            if (_stateTimer >= _teleportInDuration)
            {
                _teleportCooldown = IsEnraged() ? _teleportCooldownBase * 0.7f : _teleportCooldownBase;
                // Arriving with a spiteful volley keeps the pressure on.
                if (_volleyCooldown <= 1.5f)
                {
                    _state = State::VolleyCasting; _stateTimer = 0.f;
                    SetAnimation(_sharedMagicAnim, _volleyCastDuration / (float)_sheetFrameCount, true);
                    PlaySound(_sharedCastSound);
                }
                else
                {
                    _state = State::Stalking; _stateTimer = 0.f;
                }
            }
            break;

        case State::Recovery:
            _stateTimer += dt;
            if (_stateTimer >= _recoveryDuration)
            {
                _state = State::Stalking; _stateTimer = 0.f;
            }
            break;
        }

        if (_state != State::TeleportOut && _state != State::TeleportIn)
            TryDealContactDamage();

        _worldPos.x = std::clamp(_worldPos.x, 190.f, (float)kVirtualWidth  - 190.f);
        _worldPos.y = std::clamp(_worldPos.y, 190.f, (float)kVirtualHeight - 190.f);
    }

    HandleAnimation(dt);
}

void Osiris::BeginTeleport()
{
    _state = State::TeleportOut;
    _stateTimer = 0.f;
    _teleportTarget = PickTeleportSpot();
    PlaySound(_sharedCastSound);
}

Vector2 Osiris::PickTeleportSpot() const
{
    Vector2 playerPos = (_target != nullptr) ? _target->GetFeetWorldPos() : _worldPos;
    float angle  = (float)GetRandomValue(0, 628) / 100.f;
    float radius = (float)GetRandomValue(380, 520);
    Vector2 spot{ playerPos.x + cosf(angle) * radius, playerPos.y + sinf(angle) * radius };
    spot.x = std::clamp(spot.x, 200.f, (float)kVirtualWidth  - 200.f);
    spot.y = std::clamp(spot.y, 200.f, (float)kVirtualHeight - 200.f);
    return spot;
}

void Osiris::TryDealContactDamage()
{
    if (_target == nullptr || !_target->IsAlive()) return;
    if (_target->IsBeingForcedPushed()) return;
    if (_state == State::MeleeAttacking) return;
    if (_contactCooldown > 0.f) return;
    if (!CheckCollisionRecs(GetBodyContactRec(), _target->GetCollisionRec())) return;

    _target->TakeFractionalDamage(_bossDamagePerHit, _worldPos);
    _target->StartForcedPush(GetPushDirectionToPlayer(), _bossPushSpeed);
    _contactCooldown = _contactCooldownBase;
}

Rectangle Osiris::GetBodyContactRec() const
{
    Rectangle bodyRec = GetCollisionRec();
    bodyRec.x += 26.f; bodyRec.y += 20.f;
    bodyRec.width  = std::max(1.f, bodyRec.width  - 52.f);
    bodyRec.height = std::max(1.f, bodyRec.height - 40.f);
    return bodyRec;
}

Vector2 Osiris::GetPushDirectionToPlayer() const
{
    if (_target == nullptr) return Vector2{ 1.f, 0.f };
    Vector2 away = Vector2Subtract(_target->GetWorldPos(), _worldPos);
    return (Vector2Length(away) > 0.01f) ? Vector2Normalize(away) : Vector2{ 1.f, 0.f };
}

void Osiris::HandleAnimation(float dt)
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
                SetAnimation(_sharedIdleAnim, 1.f / 8.f, true);
                return;
            }
            _frame = 0;
        }
    }
}

void Osiris::DrawEnemy(Vector2 cameraRef)
{
    if (!_isActive) return;

    float drawWidth  = _stableFrameW * _scale;
    float drawHeight = _stableFrameH * _scale;

    Vector2 screenPos = Vector2Subtract(_worldPos, cameraRef);
    screenPos.x += kVirtualWidth  / 2.f;
    screenPos.y += kVirtualHeight / 2.f;

    // Sand Step mirage swirl + shrink.
    float teleportScale = 1.f;
    if (_state == State::TeleportOut)
        teleportScale = 1.f - (_stateTimer / _teleportOutDuration) * 0.95f;
    else if (_state == State::TeleportIn)
        teleportScale = 0.05f + (_stateTimer / _teleportInDuration) * 0.95f;

    if (_state == State::TeleportOut || _state == State::TeleportIn)
    {
        float swirl = (float)GetTime() * 10.f;
        for (int i = 0; i < 6; i++)
        {
            float angle = swirl + (float)i * (2.f * PI / 6.f);
            float radius = 26.f + (1.f - teleportScale) * 60.f;
            DrawCircleV(Vector2{ screenPos.x + cosf(angle) * radius,
                                 screenPos.y + sinf(angle) * radius * 0.55f },
                6.f + (1.f - teleportScale) * 5.f, Fade(Color{ 230, 200, 110, 255 }, 0.5f));
        }
    }

    // Nova charge: golden ring closing in — the "get ready to dodge" cue.
    if (_state == State::NovaCasting)
    {
        float t = _stateTimer / _novaCastDuration;
        DrawCircleLines((int)screenPos.x, (int)screenPos.y, 220.f * (1.f - t) + 40.f,
            Fade(Color{ 255, 210, 90, 255 }, 0.35f + t * 0.4f));
        DrawCircleV(screenPos, drawWidth * (0.3f + 0.1f * t), Fade(Color{ 255, 210, 90, 255 }, 0.18f));
    }
    if (_state == State::VolleyCasting)
    {
        float pulse = sinf((float)GetTime() * 14.f) * 0.5f + 0.5f;
        DrawCircleV(screenPos, drawWidth * (0.3f + 0.06f * pulse), Fade(Color{ 255, 170, 60, 255 }, 0.2f));
    }

    float drawW = drawWidth * teleportScale;
    float drawH = drawHeight * teleportScale;

    DrawEllipse((int)screenPos.x, (int)(screenPos.y + drawH * 0.36f),
        drawW * 0.26f, drawH * 0.09f, Fade(BLACK, 0.35f * teleportScale));

    bool burning = !_pendingBurns.empty();
    Color tint = IsElectroStunned() ? Color{ 255, 255,  60, 255 } :
                 IsFrozen()         ? Color{ 140, 200, 255, 255 } :
                 burning            ? Color{ 255, 180, 180, 255 } :
                                      WHITE;

    Vector2 animDrawOffset = GetCurrentAnimDrawOffset();
    Rectangle source{ _frame * _width, 0.f, _rightLeft * _width, _height };
    Rectangle dest{ screenPos.x - drawW / 2.f + animDrawOffset.x,
                    screenPos.y - drawH / 2.f + animDrawOffset.y, drawW, drawH };
    DrawTexturePro(_texture, source, dest, Vector2{}, 0.f, tint);

    DrawHealthBar(screenPos, drawWidth, drawHeight);
}

Rectangle Osiris::GetCollisionRec() const
{
    Rectangle animBodyRect;
    if (GetAnimBodyRectWorld(animBodyRect))
        return animBodyRect;
    if (_hasTunedCollision)
        return GetTunedCollisionRec();

    float halfW = _stableFrameW * _scale * 0.5f;
    float halfH = _stableFrameH * _scale * 0.5f;
    return Rectangle{ _worldPos.x - halfW * 0.46f, _worldPos.y - halfH * 0.58f, halfW * 0.92f, halfH * 1.22f };
}

Capsule2D Osiris::GetCapsule() const
{
    Capsule2D animBodyCapsule;
    if (GetAnimBodyCapsuleWorld(animBodyCapsule))
        return animBodyCapsule;
    if (_capsuleRadius > 0.f)
        return Capsule2D{ { _worldPos.x + _capsuleOffset.x, _worldPos.y + _capsuleOffset.y }, _capsuleHalfHeight, _capsuleRadius };
    return Capsule2D{ { _worldPos.x, _worldPos.y + 14.f }, 20.f, _stableFrameW * _scale * 0.20f };
}

void Osiris::DrawHealthBar(Vector2 screenPos, float w, float h)
{
    if (_health <= 0.f) return;
    float pct = _health / _maxHealth;
    float barWidth = w * 0.8f, barHeight = 8.f;
    float barX = screenPos.x - barWidth / 2.f;
    float barY = screenPos.y - h * 0.62f - 14.f;
    DrawRectangle((int)barX, (int)barY, (int)barWidth, (int)barHeight, RED);
    DrawRectangle((int)barX, (int)barY, (int)(barWidth * pct), (int)barHeight, GREEN);
}

void Osiris::TakeDamage(int damage, Vector2 attackerPos)
{
    (void)attackerPos;
    if (_dying || _hitTimer > 0.f) return;
    if (_state == State::TeleportOut || _state == State::TeleportIn) return;   // a mirage

    _health -= (float)damage;
    if (_health > 0.f) PlayHurtSound();

    if (_health <= 0.f)
    {
        _health = 0.f; _dying = true;
        _takingDamage = false; _attacking = false;
        _pendingNova = false; _pendingVolley = false;
        _state = State::Recovery;
        _deathTimer = 0.7f;
        SetAnimation(_sharedDeathAnim, 1.f / 6.f, true);
        PlayDeathSound();
        return;
    }

    _velocity = Vector2Zero();
    if (_state == State::Stalking)
    {
        _takingDamage = true;
        SetAnimation(_sharedHurtAnim, 1.f / 12.f, true);
    }
    _hitTimer = 0.02f;
}

void Osiris::ApplyFreeze(float duration)
{
    if (_dying || !IsAlive()) return;
    if (duration > _freezeTimer) _freezeTimer = duration;
}

void Osiris::SetWaveScale(int wave)
{
    (void)wave;
    _expValue = _bossBaseExpValue;
    _health = 58.f; _maxHealth = 58.f;
    _speed = _moveSpeed; _attackPower = 1.f;
}

void Osiris::EnsureSharedResourcesLoaded()
{
    if (_sharedResourcesLoaded) return;

    _sharedIdleAnim  = LoadTexture(AssetPath("Bosses/OsirisIdle.png").c_str());
    _sharedWalkAnim  = LoadTexture(AssetPath("Bosses/OsirisWalk.png").c_str());
    _sharedMeleeAnim = LoadTexture(AssetPath("Bosses/OsirisMeleeAttack.png").c_str());
    _sharedMagicAnim = LoadTexture(AssetPath("Bosses/OsirisMagicAttack.png").c_str());
    _sharedHurtAnim  = LoadTexture(AssetPath("Bosses/OsirisHurt.png").c_str());
    _sharedDeathAnim = LoadTexture(AssetPath("Bosses/OsirisDeath.png").c_str());
    _sharedAttackSound = LoadSound(AssetPath("Sounds/SwordSwipe2.ogg").c_str());
    _sharedHurtSound   = LoadSound(AssetPath("Sounds/SmallMonsterDamage.ogg").c_str());
    _sharedDeathSound  = LoadSound(AssetPath("Sounds/MonsterDeath.ogg").c_str());
    _sharedCastSound   = LoadSound(AssetPath("Sounds/GS1_Spell_Fire.ogg").c_str());
    _sharedResourcesLoaded = true;
}

void Osiris::UnloadSharedResources()
{
    if (!_sharedResourcesLoaded) return;

    UnloadTexture(_sharedIdleAnim);  UnloadTexture(_sharedWalkAnim);
    UnloadTexture(_sharedMeleeAnim); UnloadTexture(_sharedMagicAnim);
    UnloadTexture(_sharedHurtAnim);  UnloadTexture(_sharedDeathAnim);
    UnloadSound(_sharedAttackSound); UnloadSound(_sharedHurtSound);
    UnloadSound(_sharedDeathSound);  UnloadSound(_sharedCastSound);
    _sharedIdleAnim = Texture2D{};  _sharedWalkAnim = Texture2D{};
    _sharedMeleeAnim = Texture2D{}; _sharedMagicAnim = Texture2D{};
    _sharedHurtAnim = Texture2D{};  _sharedDeathAnim = Texture2D{};
    _sharedAttackSound = Sound{};   _sharedHurtSound = Sound{};
    _sharedDeathSound = Sound{};    _sharedCastSound = Sound{};
    _sharedResourcesLoaded = false;
}
