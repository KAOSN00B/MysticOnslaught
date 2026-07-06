#include "CharacterAnimator.h"
#include "VirtualCanvas.h"
#include "PlayerPreview.h"
#include "SkeletonArcher.h"
#include "FlameWisp.h"
#include "SlimeEnemy.h"
#include "Sporeling.h"
#include "Shieldbearer.h"
#include "Phantom.h"
#include "BomberImp.h"
#include "Warchief.h"
#include "LivingBlade.h"
#include "AbyssSlime.h"
#include "PumpkinJack.h"
#include "Minotaur.h"
#include "Werewolf.h"
#include "ChompBug.h"
#include "Osiris.h"
#include "TitanGuard.h"
#include "ToxicVermin.h"
#include "AncientBear.h"
#include "raymath.h"

#include <algorithm>
#include <cmath>

namespace
{
    // Handle ids. The body circle uses Centre + Radius; the melee box uses the
    // four corners + Centre.
    constexpr int kHandleTopLeft     = 0;
    constexpr int kHandleTopRight    = 1;
    constexpr int kHandleBottomLeft  = 2;
    constexpr int kHandleBottomRight = 3;
    constexpr int kHandleCentre      = 4;
    constexpr int kHandleRadius      = 5;

    constexpr float kHandleSize = 14.f;
    constexpr float kPanelWidth = 380.f;
    constexpr float kRowHeight  = 44.f;

    const Color kBodyColor  = { 90, 220, 255, 255 };   // cyan
    const Color kMeleeColor = { 255, 110, 110, 255 };  // red

    void DrawCheckerboard()
    {
        const int cell = 64;
        for (int gy = 0; gy <= kVirtualHeight / cell; gy++)
            for (int gx = 0; gx <= kVirtualWidth / cell; gx++)
                DrawRectangle(gx * cell, gy * cell, cell, cell,
                    ((gx + gy) % 2 == 0) ? Color{ 40, 40, 48, 255 } : Color{ 46, 46, 56, 255 });
    }
}

// =============================================================================
void CharacterAnimator::Init()
{
    _entries.clear();
    // The player first — authors charactertuning_Player.txt (body + melee), which
    // the live Character applies. Melee box lives on the Swing anim (slot 2).
    _entries.push_back({ "Player", 2, []() -> Enemy* { auto* p = new PlayerPreview({ 0.f, 0.f }); p->Init(); return p; } });
    // meleeSlot: which animation carries a melee box (2 = the attack anim for
    // every current melee character); -1 marks ranged / contact-only characters.
    _entries.push_back({ "SkeletonArcher", -1, []() -> Enemy* { auto* e = new SkeletonArcher({ 0.f, 0.f }); e->Init(); return e; } });
    _entries.push_back({ "SlimeBig",        2, []() -> Enemy* { auto* e = new SlimeEnemy({ 0.f, 0.f }, SlimeSize::Big); e->Init(); return e; } });
    _entries.push_back({ "SlimeSmall",      2, []() -> Enemy* { auto* e = new SlimeEnemy({ 0.f, 0.f }, SlimeSize::Small); e->Init(); return e; } });
    _entries.push_back({ "FlameWisp",      -1, []() -> Enemy* { auto* e = new FlameWisp({ 0.f, 0.f }); e->Init(); return e; } });
    _entries.push_back({ "Sporeling",       2, []() -> Enemy* { auto* e = new Sporeling({ 0.f, 0.f }); e->Init(); return e; } });
    _entries.push_back({ "Shieldbearer",    2, []() -> Enemy* { auto* e = new Shieldbearer({ 0.f, 0.f }); e->Init(); return e; } });
    _entries.push_back({ "Phantom",        -1, []() -> Enemy* { auto* e = new Phantom({ 0.f, 0.f }); e->Init(); return e; } });
    _entries.push_back({ "BomberImp",      -1, []() -> Enemy* { auto* e = new BomberImp({ 0.f, 0.f }); e->Init(); return e; } });
    _entries.push_back({ "Warchief",        2, []() -> Enemy* { auto* e = new Warchief({ 0.f, 0.f }); e->Init(); return e; } });
    _entries.push_back({ "LivingBlade",    -1, []() -> Enemy* { auto* e = new LivingBlade({ 0.f, 0.f }); e->Init(); return e; } });
    _entries.push_back({ "AbyssSlime",      2, []() -> Enemy* { auto* e = new AbyssSlime({ 0.f, 0.f }); e->Init(); return e; } });
    _entries.push_back({ "PumpkinJack",     2, []() -> Enemy* { auto* e = new PumpkinJack({ 0.f, 0.f }); e->Init(); return e; } });
    _entries.push_back({ "Minotaur",        2, []() -> Enemy* { auto* e = new Minotaur({ 0.f, 0.f }); e->Init(); return e; } });
    _entries.push_back({ "Werewolf",        2, []() -> Enemy* { auto* e = new Werewolf({ 0.f, 0.f }); e->Init(); return e; } });
    _entries.push_back({ "ChompBug",        2, []() -> Enemy* { auto* e = new ChompBug({ 0.f, 0.f }); e->Init(); return e; } });
    _entries.push_back({ "Osiris",          2, []() -> Enemy* { auto* e = new Osiris({ 0.f, 0.f }); e->Init(); return e; } });
    _entries.push_back({ "TitanGuard",      2, []() -> Enemy* { auto* e = new TitanGuard({ 0.f, 0.f }); e->Init(); return e; } });
    _entries.push_back({ "ToxicVermin",     2, []() -> Enemy* { auto* e = new ToxicVermin({ 0.f, 0.f }); e->Init(); return e; } });
    _entries.push_back({ "AncientBear",     2, []() -> Enemy* { auto* e = new AncientBear({ 0.f, 0.f }); e->Init(); return e; } });

    _screen      = Screen::Select;
    _selectedIdx = -1;
    _wantsToExit = false;
    _enemy.reset();
    _statusTimer = 0.f;
}

void CharacterAnimator::Unload()
{
    _enemy.reset();
}

// =============================================================================
void CharacterAnimator::OpenCharacter(int index)
{
    if (index < 0 || index >= (int)_entries.size())
        return;

    _selectedIdx = index;

    // A real, live instance — every hitbox getter and animation sheet behaves
    // exactly like it will in the game. The factory lambda already calls Init()
    // (which loads textures and applies any saved tuning).
    _enemy.reset(_entries[index].createInstance());

    _enemy->Teleport({ 0.f, 0.f });
    _enemy->SetEditorFacing(1.f);

    _animIndex  = 0;
    _animPaused = false;
    _target     = EditTarget::Body;
    _dragHandle = -1;
    _rowDrag    = -1;
    _spriteDragActive = false;
    _enemy->PlayEditorAnim(_animIndex);

    _screen = Screen::Edit;
}

void CharacterAnimator::CloseCharacter()
{
    _enemy.reset();
    _screen = Screen::Select;
}

// =============================================================================
// Effective values with fallbacks so the overlays always show something
// grabbable even before an animation has explicit data.
// =============================================================================
void CharacterAnimator::GetEffectiveBodyCircle(Vector2& outOffset, float& outRadius) const
{
    if (_enemy->GetAnimBodySet(_animIndex))
    {
        outOffset = _enemy->GetAnimBodyOffset(_animIndex);
        outRadius = _enemy->GetAnimBodyRadius(_animIndex);
        return;
    }

    // Fall back to the class capsule (facing right, so no mirroring needed).
    Capsule2D capsule = _enemy->GetCapsule();
    outOffset = Vector2{ capsule.center.x - _enemy->GetWorldPos().x,
                         capsule.center.y - _enemy->GetWorldPos().y };
    outRadius = std::max(12.f, capsule.radius + capsule.halfHeight * 0.5f);
}

bool CharacterAnimator::MeleeEditableNow() const
{
    int meleeSlot = _entries[_selectedIdx].meleeSlot;
    return meleeSlot >= 0 && _animIndex == meleeSlot;
}

Rectangle CharacterAnimator::GetEffectiveMeleeRel() const
{
    int meleeSlot = _entries[_selectedIdx].meleeSlot;
    if (_enemy->GetAnimMeleeSet(meleeSlot))
        return _enemy->GetAnimMeleeRelRect(meleeSlot);

    // Fall back to the legacy attack-box fields (grunts) or a sensible default
    // in front of the character (bosses).
    float w = _enemy->GetAttackBoxWidth();
    float h = _enemy->GetAttackBoxHeight();
    if (w > 4.f && h > 4.f)
        return Rectangle{ _enemy->GetAttackBoxOffsetX() - w * 0.5f,
                          _enemy->GetAttackBoxOffsetY() - h * 0.5f, w, h };
    return Rectangle{ 40.f, -50.f, 130.f, 110.f };
}

// =============================================================================
void CharacterAnimator::SaveCurrentTuning()
{
    if (!_enemy)
        return;

    CharacterTuning tuning;
    tuning.hasScale = true;
    tuning.scale    = _enemy->GetDrawScale();

    int animCount = std::min(_enemy->GetEditorAnimCount(), CharacterTuning::kMaxAnims);
    for (int i = 0; i < animCount; i++)
    {
        tuning.animFrameTime[i] = _enemy->GetEditorAnimFrameTime(i);

        if (_enemy->GetAnimBodySet(i))
        {
            tuning.animBody[i].set    = true;
            tuning.animBody[i].x      = _enemy->GetAnimBodyOffset(i).x;
            tuning.animBody[i].y      = _enemy->GetAnimBodyOffset(i).y;
            tuning.animBody[i].radius = _enemy->GetAnimBodyRadius(i);
        }
        if (_enemy->GetAnimMeleeSet(i))
        {
            tuning.animMelee[i].set  = true;
            tuning.animMelee[i].rect = _enemy->GetAnimMeleeRelRect(i);
        }
        if (_enemy->GetAnimDrawSet(i))
        {
            tuning.animDraw[i].set = true;
            tuning.animDraw[i].x   = _enemy->GetAnimDrawOffsetValue(i).x;
            tuning.animDraw[i].y   = _enemy->GetAnimDrawOffsetValue(i).y;
        }
    }

    CharacterTuningStore::Save(_entries[_selectedIdx].displayName, tuning);
    _statusText  = "Saved " + _entries[_selectedIdx].displayName + " tuning";
    _statusTimer = 2.5f;
}

void CharacterAnimator::DeleteCurrentTuning()
{
    if (_selectedIdx < 0)
        return;

    CharacterTuningStore::Delete(_entries[_selectedIdx].displayName);
    // Respawn the preview so it snaps back to code defaults.
    int index = _selectedIdx;
    OpenCharacter(index);
    _statusText  = "Tuning deleted - back to code defaults";
    _statusTimer = 2.5f;
}

// =============================================================================
void CharacterAnimator::Update()
{
    if (_screen == Screen::Select)
    {
        if (IsKeyPressed(KEY_ESCAPE))
        {
            _wantsToExit = true;
            return;
        }

        Vector2 mouse = GetVirtualMousePos();
        for (int i = 0; i < (int)_entries.size(); i++)
        {
            // Two-column layout: 10 rows on the left, the rest on the right.
            int column = i / 10;
            int rowInColumn = i % 10;
            Rectangle row{ kVirtualWidth * 0.5f - 480.f + column * 500.f,
                           250.f + rowInColumn * 74.f, 460.f, 62.f };
            if (CheckCollisionPointRec(mouse, row) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
            {
                OpenCharacter(i);
                return;
            }
        }
        return;
    }

    // ── Edit screen ───────────────────────────────────────────────────────────
    if (_statusTimer > 0.f)
        _statusTimer -= GetFrameTime();

    if (IsKeyPressed(KEY_ESCAPE))
    {
        CloseCharacter();
        return;
    }
    if (!_enemy)
        return;

    UpdateEditInput();

    if (!_animPaused)
        _enemy->TickEditorAnimation(GetFrameTime());
}

void CharacterAnimator::UpdateEditInput()
{
    int animCount = _enemy->GetEditorAnimCount();

    // Animation controls
    if (IsKeyPressed(KEY_Q)) { _animIndex = (_animIndex + animCount - 1) % animCount; _enemy->PlayEditorAnim(_animIndex); _dragHandle = -1; }
    if (IsKeyPressed(KEY_E)) { _animIndex = (_animIndex + 1) % animCount;             _enemy->PlayEditorAnim(_animIndex); _dragHandle = -1; }
    if (IsKeyPressed(KEY_SPACE)) _animPaused = !_animPaused;
    if (_animPaused)
    {
        if (IsKeyPressed(KEY_PERIOD)) _enemy->TickEditorAnimation(1.f);
        if (IsKeyPressed(KEY_COMMA))  _enemy->TickEditorAnimation(1.f);
    }
    if (IsKeyPressed(KEY_F))
        _enemy->SetEditorFacing(_enemy->GetEditorFacing() >= 0.f ? -1.f : 1.f);

    // TAB toggles Body / Melee (Melee only on its animation).
    if (IsKeyPressed(KEY_TAB))
    {
        _target = (_target == EditTarget::Body && MeleeEditableNow())
            ? EditTarget::Melee
            : EditTarget::Body;
        _dragHandle = -1;
    }

    // C copies this animation's body circle + draw offset to every animation —
    // fastest way to author a character whose body barely moves between anims.
    if (IsKeyPressed(KEY_C))
    {
        Vector2 bodyOffset;
        float bodyRadius;
        GetEffectiveBodyCircle(bodyOffset, bodyRadius);
        Vector2 drawOffset = _enemy->GetAnimDrawOffsetValue(_animIndex);
        bool hasDrawOffset = _enemy->GetAnimDrawSet(_animIndex);

        for (int i = 0; i < animCount; i++)
        {
            _enemy->SetAnimBody(i, bodyOffset, bodyRadius);
            if (hasDrawOffset)
                _enemy->SetAnimDrawOffset(i, drawOffset);
        }
        _statusText  = "Copied body circle to all animations";
        _statusTimer = 2.0f;
    }

    if (IsKeyPressed(KEY_S))      SaveCurrentTuning();
    if (IsKeyPressed(KEY_DELETE)) DeleteCurrentTuning();

    Vector2 mouse = GetVirtualMousePos();

    // ── Right-drag: per-animation sprite draw offset ──────────────────────────
    if (IsMouseButtonPressed(MOUSE_RIGHT_BUTTON))
    {
        _spriteDragActive     = true;
        _spriteDragStartMouse = mouse;
        _spriteDragStartOffset = _enemy->GetAnimDrawOffsetValue(_animIndex);
    }
    if (_spriteDragActive)
    {
        if (!IsMouseButtonDown(MOUSE_RIGHT_BUTTON))
        {
            _spriteDragActive = false;
        }
        else
        {
            // Offsets are authored facing right — compensate when flipped.
            float facing = (_enemy->GetEditorFacing() >= 0.f) ? 1.f : -1.f;
            _enemy->SetAnimDrawOffset(_animIndex, Vector2{
                _spriteDragStartOffset.x + (mouse.x - _spriteDragStartMouse.x) * facing,
                _spriteDragStartOffset.y + (mouse.y - _spriteDragStartMouse.y) });
        }
    }

    // ── Side panel value rows (Scale / FPS) — horizontal drag to nudge ───────
    // Positions must match DrawSidePanel exactly.
    Rectangle scaleRow{ 20.f, 470.f, kPanelWidth - 40.f, kRowHeight };
    Rectangle fpsRow{   20.f, 470.f + kRowHeight + 8.f, kPanelWidth - 40.f, kRowHeight };
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
    {
        if (CheckCollisionPointRec(mouse, scaleRow))
        {
            _rowDrag = 0;
            _rowDragStartX = mouse.x;
            _rowDragStartValue = _enemy->GetDrawScale();
        }
        else if (CheckCollisionPointRec(mouse, fpsRow))
        {
            _rowDrag = 1;
            _rowDragStartX = mouse.x;
            float current = _enemy->GetEditorAnimFrameTime(_animIndex);
            _rowDragStartValue = (current > 0.f) ? current : 1.f / 8.f;
        }
    }
    if (_rowDrag >= 0)
    {
        if (!IsMouseButtonDown(MOUSE_LEFT_BUTTON))
        {
            _rowDrag = -1;
        }
        else
        {
            float deltaX = mouse.x - _rowDragStartX;
            if (_rowDrag == 0)
            {
                _enemy->SetDrawScale(_rowDragStartValue + deltaX * 0.01f);
            }
            else
            {
                float frameTime = std::clamp(_rowDragStartValue + deltaX * 0.0006f, 0.02f, 0.6f);
                _enemy->SetEditorAnimFrameTime(_animIndex, frameTime);
                _enemy->PlayEditorAnim(_animIndex);   // re-apply so the preview updates
            }
            return;   // a row drag owns the mouse — no handle interaction
        }
    }

    UpdateHandleDrag(mouse);
}

// =============================================================================
// Handle dragging. Everything is edited in world-relative space (relative to
// the enemy worldPos, which sits at the screen centre in this tool). The
// preview always faces right while editing hitboxes.
// =============================================================================
void CharacterAnimator::UpdateHandleDrag(Vector2 mouse)
{
    Vector2 screenCenter{ kVirtualWidth * 0.5f, kVirtualHeight * 0.5f };
    Vector2 mouseRel{ mouse.x - screenCenter.x, mouse.y - screenCenter.y };

    Vector2 bodyOffset;
    float bodyRadius;
    GetEffectiveBodyCircle(bodyOffset, bodyRadius);

    Rectangle meleeRel = MeleeEditableNow() ? GetEffectiveMeleeRel() : Rectangle{};

    auto rectHandles = [&](Rectangle rel, Vector2* outPositions)
    {
        outPositions[kHandleTopLeft]     = { rel.x,             rel.y };
        outPositions[kHandleTopRight]    = { rel.x + rel.width, rel.y };
        outPositions[kHandleBottomLeft]  = { rel.x,             rel.y + rel.height };
        outPositions[kHandleBottomRight] = { rel.x + rel.width, rel.y + rel.height };
        outPositions[kHandleCentre]      = { rel.x + rel.width * 0.5f, rel.y + rel.height * 0.5f };
    };

    // ── Begin drag ────────────────────────────────────────────────────────────
    // Clicking ANY overlay's handle selects that target automatically.
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && _dragHandle == -1)
    {
        auto tryBeginBodyDrag = [&]() -> bool
        {
            Vector2 centreHandle{ screenCenter.x + bodyOffset.x, screenCenter.y + bodyOffset.y };
            Vector2 radiusHandle{ centreHandle.x + bodyRadius, centreHandle.y };

            if      (HandleHit(mouse, radiusHandle)) _dragHandle = kHandleRadius;
            else if (HandleHit(mouse, centreHandle)) _dragHandle = kHandleCentre;
            return _dragHandle != -1;
        };

        auto tryBeginMeleeDrag = [&]() -> bool
        {
            if (!MeleeEditableNow())
                return false;

            Vector2 positions[5];
            rectHandles(meleeRel, positions);
            for (int i = 0; i < 5; i++)
            {
                Vector2 screenPos{ screenCenter.x + positions[i].x, screenCenter.y + positions[i].y };
                if (HandleHit(mouse, screenPos))
                {
                    _dragHandle = i;
                    if (i == kHandleTopLeft)     _dragAnchor = positions[kHandleBottomRight];
                    if (i == kHandleTopRight)    _dragAnchor = positions[kHandleBottomLeft];
                    if (i == kHandleBottomLeft)  _dragAnchor = positions[kHandleTopRight];
                    if (i == kHandleBottomRight) _dragAnchor = positions[kHandleTopLeft];
                    return true;
                }
            }
            return false;
        };

        // Active target gets priority when handles overlap.
        bool hit = false;
        if (_target == EditTarget::Body)
        {
            hit = tryBeginBodyDrag();
            if (!hit && tryBeginMeleeDrag()) { _target = EditTarget::Melee; hit = true; }
        }
        else
        {
            hit = tryBeginMeleeDrag();
            if (!hit && tryBeginBodyDrag()) { _target = EditTarget::Body; hit = true; }
        }
        (void)hit;
    }

    if (_dragHandle == -1)
        return;
    if (!IsMouseButtonDown(MOUSE_LEFT_BUTTON))
    {
        _dragHandle = -1;
        return;
    }

    // ── Apply drag ────────────────────────────────────────────────────────────
    if (_target == EditTarget::Body)
    {
        if (_dragHandle == kHandleCentre)
            _enemy->SetAnimBody(_animIndex, mouseRel, bodyRadius);
        else if (_dragHandle == kHandleRadius)
            _enemy->SetAnimBody(_animIndex, bodyOffset, fabsf(mouseRel.x - bodyOffset.x));
        return;
    }

    // Melee box
    Rectangle newRel = meleeRel;
    if (_dragHandle == kHandleCentre)
    {
        newRel.x = mouseRel.x - meleeRel.width * 0.5f;
        newRel.y = mouseRel.y - meleeRel.height * 0.5f;
    }
    else
    {
        newRel.x      = std::min(mouseRel.x, _dragAnchor.x);
        newRel.y      = std::min(mouseRel.y, _dragAnchor.y);
        newRel.width  = std::max(8.f, fabsf(mouseRel.x - _dragAnchor.x));
        newRel.height = std::max(8.f, fabsf(mouseRel.y - _dragAnchor.y));
    }
    _enemy->SetAnimMelee(_entries[_selectedIdx].meleeSlot, newRel);
}

bool CharacterAnimator::HandleHit(Vector2 mouse, Vector2 screenPos) const
{
    return fabsf(mouse.x - screenPos.x) <= kHandleSize && fabsf(mouse.y - screenPos.y) <= kHandleSize;
}

// =============================================================================
// Drawing
// =============================================================================
void CharacterAnimator::Draw()
{
    DrawCheckerboard();

    if (_screen == Screen::Select)
        DrawSelectScreen();
    else
        DrawEditScreen();
}

void CharacterAnimator::DrawSelectScreen()
{
    const char* title = "CHARACTER ANIMATOR";
    DrawText(title, (int)(kVirtualWidth * 0.5f - MeasureText(title, 60) * 0.5f), 110, 60, RAYWHITE);
    const char* subtitle = "Pick a character to adjust its hitboxes and animations";
    DrawText(subtitle, (int)(kVirtualWidth * 0.5f - MeasureText(subtitle, 26) * 0.5f), 190, 26, LIGHTGRAY);

    Vector2 mouse = GetVirtualMousePos();
    for (int i = 0; i < (int)_entries.size(); i++)
    {
        const CharacterEntry& entry = _entries[i];
        // Two-column layout: must match the rects in Update exactly.
        int column = i / 10;
        int rowInColumn = i % 10;
        Rectangle row{ kVirtualWidth * 0.5f - 480.f + column * 500.f,
                       250.f + rowInColumn * 74.f, 460.f, 62.f };
        bool hovered = CheckCollisionPointRec(mouse, row);
        DrawRectangleRounded(row, 0.3f, 6, hovered ? Color{ 70, 70, 92, 255 } : Color{ 52, 52, 66, 255 });
        DrawRectangleRoundedLines(row, 0.3f, 6, hovered ? GOLD : Color{ 90, 90, 110, 255 });

        bool tuned = (CharacterTuningStore::Get(entry.displayName) != nullptr);
        const char* label = tuned ? TextFormat("%s   [tuned]", entry.displayName.c_str()) : entry.displayName.c_str();
        DrawText(label, (int)(row.x + 24.f), (int)(row.y + 17.f), 28, tuned ? Color{ 140, 235, 160, 255 } : RAYWHITE);
    }

    DrawText("ESC: back to menu", 24, kVirtualHeight - 44, 24, LIGHTGRAY);
}

void CharacterAnimator::DrawEditScreen()
{
    if (!_enemy)
        return;

    Vector2 screenCenter{ kVirtualWidth * 0.5f, kVirtualHeight * 0.5f };

    // Ground reference line so scale reads against something.
    DrawLineEx(Vector2{ screenCenter.x - 420.f, screenCenter.y + 170.f },
               Vector2{ screenCenter.x + 420.f, screenCenter.y + 170.f }, 2.f, Fade(WHITE, 0.15f));

    // The live enemy draws itself exactly like in-game (cameraRef 0 = centre).
    _enemy->DrawEnemy(Vector2{ 0.f, 0.f });

    DrawOverlays(screenCenter);
    DrawSidePanel();

    if (_statusTimer > 0.f)
    {
        int width = MeasureText(_statusText.c_str(), 30);
        DrawRectangleRounded(
            Rectangle{ screenCenter.x - width * 0.5f - 18.f, 30.f, width + 36.f, 50.f }, 0.3f, 6, Fade(BLACK, 0.65f));
        DrawText(_statusText.c_str(), (int)(screenCenter.x - width * 0.5f), 40, 30, Color{ 140, 235, 160, 255 });
    }
}

void CharacterAnimator::DrawOverlays(Vector2 screenCenter)
{
    // ── Body circle (cyan) — per animation ────────────────────────────────────
    {
        Vector2 bodyOffset;
        float bodyRadius;
        GetEffectiveBodyCircle(bodyOffset, bodyRadius);

        Vector2 centre{ screenCenter.x + bodyOffset.x, screenCenter.y + bodyOffset.y };
        bool active   = (_target == EditTarget::Body);
        bool explicitData = _enemy->GetAnimBodySet(_animIndex);
        Color color   = active ? kBodyColor : Fade(kBodyColor, 0.45f);

        // Dashed-look ring when this anim has no explicit circle yet (showing
        // the fallback) — solid ring once authored.
        if (explicitData)
        {
            DrawCircleLines((int)centre.x, (int)centre.y, bodyRadius, color);
            DrawCircleLines((int)centre.x, (int)centre.y, bodyRadius - 1.f, Fade(color, 0.6f));
        }
        else
        {
            for (int i = 0; i < 24; i++)
            {
                if (i % 2 == 1) continue;   // gaps make it read as provisional
                float a0 = (float)i / 24.f * 2.f * PI;
                float a1 = (float)(i + 1) / 24.f * 2.f * PI;
                DrawLineV(Vector2{ centre.x + cosf(a0) * bodyRadius, centre.y + sinf(a0) * bodyRadius },
                          Vector2{ centre.x + cosf(a1) * bodyRadius, centre.y + sinf(a1) * bodyRadius }, color);
            }
        }

        DrawHandle(centre, active && _dragHandle == kHandleCentre, active ? GOLD : Fade(kBodyColor, 0.45f));
        DrawHandle(Vector2{ centre.x + bodyRadius, centre.y },
            active && _dragHandle == kHandleRadius, active ? GOLD : Fade(kBodyColor, 0.45f));

        // The hurt box the game derives from this circle (bounding square).
        DrawRectangleLinesEx(Rectangle{ centre.x - bodyRadius, centre.y - bodyRadius,
            bodyRadius * 2.f, bodyRadius * 2.f }, 1.f, Fade(WHITE, 0.14f));
    }

    // ── Melee box (red) — only on the attack animation ────────────────────────
    if (MeleeEditableNow())
    {
        Rectangle rel = GetEffectiveMeleeRel();
        Rectangle screenRect{ screenCenter.x + rel.x, screenCenter.y + rel.y, rel.width, rel.height };
        bool active = (_target == EditTarget::Melee);
        Color color = active ? kMeleeColor : Fade(kMeleeColor, 0.5f);
        DrawRectangleLinesEx(screenRect, active ? 3.f : 1.5f, color);

        Vector2 corners[5] = {
            { screenRect.x, screenRect.y },
            { screenRect.x + screenRect.width, screenRect.y },
            { screenRect.x, screenRect.y + screenRect.height },
            { screenRect.x + screenRect.width, screenRect.y + screenRect.height },
            { screenRect.x + screenRect.width * 0.5f, screenRect.y + screenRect.height * 0.5f }
        };
        for (int i = 0; i < 5; i++)
            DrawHandle(corners[i], active && _dragHandle == i, active ? GOLD : Fade(kMeleeColor, 0.45f));
    }

    // World-pos crosshair.
    DrawLineV(Vector2{ screenCenter.x - 12.f, screenCenter.y }, Vector2{ screenCenter.x + 12.f, screenCenter.y }, Fade(WHITE, 0.5f));
    DrawLineV(Vector2{ screenCenter.x, screenCenter.y - 12.f }, Vector2{ screenCenter.x, screenCenter.y + 12.f }, Fade(WHITE, 0.5f));
}

void CharacterAnimator::DrawSidePanel()
{
    DrawRectangle(0, 0, (int)kPanelWidth, kVirtualHeight, Fade(BLACK, 0.62f));

    const CharacterEntry& entry = _entries[_selectedIdx];
    float y = 24.f;

    DrawText(entry.displayName.c_str(), 20, (int)y, 40, RAYWHITE);
    y += 56.f;

    if (_target == EditTarget::Body)
        DrawText("Editing: BODY CIRCLE (this anim)", 20, (int)y, 24, kBodyColor);
    else
        DrawText("Editing: MELEE BOX (this anim)", 20, (int)y, 24, kMeleeColor);
    y += 40.f;

    // Animation list — melee anim gets a red marker, authored anims a dot.
    DrawText("Animations  [Q / E]", 20, (int)y, 22, LIGHTGRAY);
    y += 30.f;
    for (int i = 0; i < _enemy->GetEditorAnimCount(); i++)
    {
        bool current  = (i == _animIndex);
        bool authored = _enemy->GetAnimBodySet(i) || _enemy->GetAnimMeleeSet(i) || _enemy->GetAnimDrawSet(i);
        const char* meleeTag = (i == entry.meleeSlot) ? " [melee]" : "";
        DrawText(TextFormat("%s %s%s%s", current ? ">" : " ", _enemy->GetEditorAnimName(i),
                authored ? " *" : "", meleeTag),
            28, (int)y, 24, current ? GOLD : Color{ 190, 190, 200, 255 });
        y += 28.f;
    }

    // Value rows (Scale / FPS) — must match the rects in UpdateEditInput.
    Rectangle scaleRow{ 20.f, 470.f, kPanelWidth - 40.f, kRowHeight };
    Rectangle fpsRow{   20.f, 470.f + kRowHeight + 8.f, kPanelWidth - 40.f, kRowHeight };

    DrawRectangleRounded(scaleRow, 0.3f, 6, (_rowDrag == 0) ? Color{ 80, 80, 104, 255 } : Color{ 56, 56, 70, 255 });
    DrawText(TextFormat("Scale: %.2f   (drag)", _enemy->GetDrawScale()),
        (int)(scaleRow.x + 14.f), (int)(scaleRow.y + 11.f), 24, RAYWHITE);

    float frameTime = _enemy->GetEditorAnimFrameTime(_animIndex);
    DrawRectangleRounded(fpsRow, 0.3f, 6, (_rowDrag == 1) ? Color{ 80, 80, 104, 255 } : Color{ 56, 56, 70, 255 });
    if (frameTime > 0.f)
        DrawText(TextFormat("Anim speed: %.1f fps   (drag)", 1.f / frameTime),
            (int)(fpsRow.x + 14.f), (int)(fpsRow.y + 11.f), 24, RAYWHITE);
    else
        DrawText("Anim speed: default   (drag)",
            (int)(fpsRow.x + 14.f), (int)(fpsRow.y + 11.f), 24, Color{ 170, 170, 185, 255 });

    // Sprite draw offset readout for the current anim.
    Vector2 drawOffset = _enemy->GetAnimDrawOffsetValue(_animIndex);
    if (_enemy->GetAnimDrawSet(_animIndex))
        DrawText(TextFormat("Sprite offset: %.0f, %.0f   (right-drag)", drawOffset.x, drawOffset.y),
            20, (int)(fpsRow.y + kRowHeight + 14.f), 22, Color{ 220, 200, 140, 255 });
    else
        DrawText("Sprite offset: none   (right-drag)",
            20, (int)(fpsRow.y + kRowHeight + 14.f), 22, Color{ 150, 150, 165, 255 });

    // Ranged characters have no melee box to edit.
    if (entry.meleeSlot < 0)
        DrawText("(ranged - no melee box)", 20, (int)(fpsRow.y + kRowHeight + 44.f), 22, Color{ 150, 150, 165, 255 });

    // Help block pinned to the bottom.
    float helpY = kVirtualHeight - 330.f;
    DrawText("TAB    body / melee box",        20, (int)helpY, 22, LIGHTGRAY); helpY += 30.f;
    DrawText("SPACE  pause    , .  step",      20, (int)helpY, 22, LIGHTGRAY); helpY += 30.f;
    DrawText("F      flip facing",             20, (int)helpY, 22, LIGHTGRAY); helpY += 30.f;
    DrawText("C      copy body to all anims",  20, (int)helpY, 22, LIGHTGRAY); helpY += 30.f;
    DrawText("R-drag move sprite (this anim)", 20, (int)helpY, 22, LIGHTGRAY); helpY += 30.f;
    DrawText("S      save tuning file",        20, (int)helpY, 22, Color{ 140, 235, 160, 255 }); helpY += 30.f;
    DrawText("DEL    delete tuning file",      20, (int)helpY, 22, Color{ 255, 160, 140, 255 }); helpY += 30.f;
    DrawText("ESC    back",                    20, (int)helpY, 22, LIGHTGRAY);
}

void CharacterAnimator::DrawHandle(Vector2 screenPos, bool hot, Color color) const
{
    DrawRectangle((int)(screenPos.x - kHandleSize * 0.5f), (int)(screenPos.y - kHandleSize * 0.5f),
        (int)kHandleSize, (int)kHandleSize, hot ? WHITE : Fade(color, 0.9f));
    DrawRectangleLines((int)(screenPos.x - kHandleSize * 0.5f), (int)(screenPos.y - kHandleSize * 0.5f),
        (int)kHandleSize, (int)kHandleSize, BLACK);
}
