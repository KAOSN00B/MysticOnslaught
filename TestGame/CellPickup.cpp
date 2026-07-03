#include "CellPickup.h"
#include "VirtualCanvas.h"
#include "AssetPaths.h"
#include "Character.h"

#include <cmath>

Texture2D CellPickup::_texSingle{};
Texture2D CellPickup::_texFive{};
Texture2D CellPickup::_texTen{};
bool      CellPickup::_texturesLoaded = false;

void CellPickup::Init(Vector2 spawnPos, CellDenomination denomination)
{
    EnsureTexturesLoaded();
    _worldPos     = spawnPos;
    _denomination = denomination;
    _isActive     = true;
    _bobTimer     = (float)GetRandomValue(0, 628) / 100.f;   // desync the float cycle
}

void CellPickup::Draw(Vector2 worldOffset)
{
    if (!_isActive)
        return;

    EnsureTexturesLoaded();

    Texture2D& tex =
        (_denomination == CellDenomination::Ten)  ? _texTen  :
        (_denomination == CellDenomination::Five) ? _texFive : _texSingle;

    Vector2 screenPos{ _worldPos.x + worldOffset.x, _worldPos.y + worldOffset.y };
    screenPos.x += kVirtualWidth  / 2.f;
    screenPos.y += kVirtualHeight / 2.f;

    // Cells float and pulse so they read as magical, distinct from gold coins.
    _bobTimer += GetFrameTime() * 3.f;
    const float bobOffset = sinf(_bobTimer) * 4.f;
    const float scale     = 4.0f + sinf(_bobTimer * 0.7f) * 0.25f;

    Rectangle source{ 0.f, 0.f, (float)tex.width, (float)tex.height };
    Rectangle dest{
        screenPos.x,
        screenPos.y + bobOffset,
        tex.width  * scale,
        tex.height * scale
    };
    DrawTexturePro(tex, source, dest,
        Vector2{ dest.width * 0.5f, dest.height * 0.5f }, 0.f, WHITE);
}

void CellPickup::OnCollect(Character& player)
{
    player.AddCells((int)_denomination);
    _isActive = false;
}

Rectangle CellPickup::GetCollisionRec() const
{
    return Rectangle{ _worldPos.x - 18.f, _worldPos.y - 18.f, 36.f, 36.f };
}

const Texture2D& CellPickup::GetMediumTexture()
{
    EnsureTexturesLoaded();
    return _texFive;
}

void CellPickup::UnloadSharedResources()
{
    if (_texturesLoaded)
    {
        UnloadTexture(_texSingle);
        UnloadTexture(_texFive);
        UnloadTexture(_texTen);
        _texSingle = Texture2D{};
        _texFive   = Texture2D{};
        _texTen    = Texture2D{};
        _texturesLoaded = false;
    }
}

void CellPickup::EnsureTexturesLoaded()
{
    if (_texturesLoaded)
        return;

    _texSingle = LoadTexture(AssetPath("PowerUps/MysticCellSmall.png").c_str());
    _texFive   = LoadTexture(AssetPath("PowerUps/MysticCellMedium.png").c_str());
    _texTen    = LoadTexture(AssetPath("PowerUps/MysticCellLarge.png").c_str());
    _texturesLoaded = true;
}
