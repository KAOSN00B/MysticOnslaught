#include "GoldPickup.h"
#include "VirtualCanvas.h"
#include "AssetPaths.h"
#include "VirtualCanvas.h"
#include "Character.h"
#include "VirtualCanvas.h"

Texture2D GoldPickup::_texSingle{};
Texture2D GoldPickup::_texFive{};
Texture2D GoldPickup::_texTen{};
bool      GoldPickup::_texturesLoaded = false;

void GoldPickup::Init(Vector2 spawnPos, GoldDenomination denomination)
{
    EnsureTexturesLoaded();
    _worldPos     = spawnPos;
    _denomination = denomination;
    _isActive     = true;
}

void GoldPickup::Draw(Vector2 worldOffset)
{
    if (!_isActive)
        return;

    EnsureTexturesLoaded();

    Texture2D& tex =
        (_denomination == GoldDenomination::Ten)  ? _texTen  :
        (_denomination == GoldDenomination::Five) ? _texFive : _texSingle;

    Vector2 screenPos{ _worldPos.x + worldOffset.x, _worldPos.y + worldOffset.y };
    screenPos.x += kVirtualWidth  / 2.f;
    screenPos.y += kVirtualHeight / 2.f;

    const float scale = 4.0f;
    Rectangle source{ 0.f, 0.f, (float)tex.width, (float)tex.height };
    Rectangle dest{
        screenPos.x,
        screenPos.y,
        tex.width  * scale,
        tex.height * scale
    };
    DrawTexturePro(tex, source, dest,
        Vector2{ dest.width * 0.5f, dest.height * 0.5f }, 0.f, WHITE);
}

void GoldPickup::OnCollect(Character& player)
{
    player.AddGoldFromDrop((int)_denomination);   // applies Midas Touch relic
    _isActive = false;
}

Rectangle GoldPickup::GetCollisionRec() const
{
    return Rectangle{ _worldPos.x - 18.f, _worldPos.y - 18.f, 36.f, 36.f };
}

void GoldPickup::UnloadSharedResources()
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

void GoldPickup::EnsureTexturesLoaded()
{
    if (_texturesLoaded)
        return;

    _texSingle = LoadTexture(AssetPath("TileSet/SingleGold.png").c_str());
    _texFive   = LoadTexture(AssetPath("TileSet/FiveGold.png").c_str());
    _texTen    = LoadTexture(AssetPath("TileSet/TenGold.png").c_str());
    _texturesLoaded = true;
}
