// =============================================================================
// EngineCrossroads.cpp — the room between domains (replaces the world map).
//
// After a domain's boss the player walks into a quiet room of the domain just
// cleared. Its west, north and east doors each lead to a different unplayed
// domain, signed with where it leads, the guardian waiting there and a reward
// for arriving. Walking through a door commits the choice; nothing else has to
// be clicked. Before the final domain only the north door opens.
//
// The room is a small dungeon graph (DungeonGen::GenerateCrossroads): the
// crossroads plus a "gateway" room behind each door, so door art, collision and
// door-walking all come from the normal dungeon code. The door threshold check
// in UpdateDungeonRun turns "walk into a gateway" into ChooseCrossroadsDoor.
// =============================================================================

#include "Engine.h"
#include "WorldMapManager.h"

#include <algorithm>
#include <string>
#include <vector>

namespace
{
    // Domains a crossroads can lead to: the Forest opens every run and Demon's
    // Insides ends it, so neither is offered.
    constexpr Biome kCrossroadsPool[] = {
        Biome::AncientCastle, Biome::DreamRealm, Biome::Caverns,      Biome::Graveyard,
        Biome::Jungle,        Biome::LostCity,   Biome::TheSanctuary, Biome::Wastelands,
    };

    // Door order in _crossroadsDoors: 0 = west, 1 = north, 2 = east.
    constexpr int kDoorDr[3] = { 0, -1, 0 };
    constexpr int kDoorDc[3] = { -1, 0, 1 };

    // The door openings, as in Engine.cpp (GetDungeonDoorStartCol/Row).
    constexpr int kDoorSpanCols = 5;
    constexpr int kDoorSpanRows = 3;
    constexpr int kDoorStartCol = RoomLayout::kCols / 2 - kDoorSpanCols / 2;
    constexpr int kDoorStartRow = RoomLayout::kRows / 2 - kDoorSpanRows / 2;
}

const char* Engine::GetBiomeGuardianName(Biome biome)
{
    // Mirrors SpawnBossForBiome.
    switch (biome)
    {
    case Biome::Forest:        return "Werewolf";
    case Biome::Jungle:        return "Chomp Bug";
    case Biome::Graveyard:     return "Pumpkin Jack";
    case Biome::AncientCastle: return "Minotaur";
    case Biome::LostCity:      return "Osiris";
    case Biome::TheSanctuary:  return "Titan Guard";
    case Biome::Wastelands:    return "Toxic Vermin";
    case Biome::DreamRealm:    return "Ancient Bear";
    case Biome::Caverns:       return "Abyss Slime";
    case Biome::DemonsInsides: return "Molarbeast";
    default:                   return "Unknown";
    }
}

int Engine::PathBoonAmount(PathBoon boon, int domainIndex)
{
    const int d = std::clamp(domainIndex, 0, RunProgression::kFinalZone);
    switch (boon)
    {
    case PathBoon::Gold:   return 50 + 25 * d;
    case PathBoon::Echoes: return 15 + 10 * d;
    default:               return 0;
    }
}

std::string Engine::DescribePathBoon(PathBoon boon, int domainIndex)
{
    switch (boon)
    {
    case PathBoon::Relic:  return "a relic of your choice";
    case PathBoon::Gold:   return TextFormat("+%d gold", PathBoonAmount(boon, domainIndex));
    case PathBoon::Echoes: return TextFormat("+%d Echoes", PathBoonAmount(boon, domainIndex));
    case PathBoon::Vigor:  return "a full heal";
    default:               return "";
    }
}

void Engine::RollCrossroadsDoors()
{
    for (CrossroadsDoor& door : _crossroadsDoors) door = CrossroadsDoor{};

    // Only the final descent is left after the fifth domain.
    if (_worldZone >= RunProgression::kFinalZone - 1)
    {
        _crossroadsDoors[1] = CrossroadsDoor{ Biome::DemonsInsides, PathBoon::Vigor, true };
        return;
    }

    std::vector<Biome> pool;
    for (Biome biome : kCrossroadsPool)
        if (biome != _currentBiome &&
            std::find(_worldCompletedBiomes.begin(), _worldCompletedBiomes.end(), biome) == _worldCompletedBiomes.end())
            pool.push_back(biome);
    for (int i = (int)pool.size() - 1; i > 0; --i)
        std::swap(pool[(size_t)i], pool[(size_t)GetRandomValue(0, i)]);

    PathBoon boons[] = { PathBoon::Relic, PathBoon::Gold, PathBoon::Echoes, PathBoon::Vigor };
    for (int i = 3; i > 0; --i)
        std::swap(boons[i], boons[GetRandomValue(0, i)]);

    for (int i = 0; i < 3 && i < (int)pool.size(); ++i)
        _crossroadsDoors[i] = CrossroadsDoor{ pool[(size_t)i], boons[i], true };
    _runSessionData.MarkWorldMapGenerated();
}

void Engine::OpenCrossroads()
{
    _atCrossroads = true;
    _crossroadsLeaving = false;
    _dungeonGen.GenerateCrossroads(_crossroadsDoors[0].open, _crossroadsDoors[1].open, _crossroadsDoors[2].open);
    const int start = _dungeonGen.GetStartIndex();
    for (int door = 0; door < 3; ++door)
        _crossroadsGateway[door] = _crossroadsDoors[door].open
            ? _dungeonGen.GetNeighborIndex(start, kDoorDr[door], kDoorDc[door]) : -1;

    // Its art: an authored room of the domain just cleared with exactly these doors.
    _handcraftedDungeonRoomIds.clear();
    if (_useHandcraftedDungeonRooms)
    {
        const DungeonRoom& room = _dungeonGen.GetRooms()[(size_t)start];
        const unsigned char mask = RoomDoorMask(room.hasNorth, room.hasSouth, room.hasEast, room.hasWest);
        std::vector<const RoomBlueprint*> candidates = _roomLibrary.PlaytestCandidates(_currentBiome, RoomType::Standard, mask);
        if (!candidates.empty())
            _handcraftedDungeonRoomIds.assign(_dungeonGen.GetRooms().size(),
                                              candidates[(size_t)GetRandomValue(0, (int)candidates.size() - 1)]->id);
    }

    const float cellH = (float)kVirtualHeight / (float)RoomLayout::kRows;
    EnterDungeonRoom(start, DungeonDoorSide::None,
                     Vector2{ (float)kVirtualWidth * 0.5f, (RoomLayout::kRows - 3) * cellH }, true);
    _gameState = GameState::DungeonRun;
}

void Engine::ChooseCrossroadsDoor(int door)
{
    if (!_atCrossroads || _crossroadsLeaving || door < 0 || door > 2 || !_crossroadsDoors[door].open) return;
    _crossroadsLeaving = true;
    _pendingPathBoon = _crossroadsDoors[door].boon;
    if (_crossroadsDoors[door].biome == Biome::DemonsInsides)
    {
        if (!QueueJourney("The final descent", { [this] { EnterFinalDomain(); } },
                          [this] { OpenCrossroads(); }))
            _crossroadsLeaving = false;
        return;
    }
    CommitWorldMapChoice(_crossroadsDoors[door].biome, door);
}

void Engine::ApplyPendingPathBoon()
{
    const PathBoon boon = _pendingPathBoon;
    _pendingPathBoon = PathBoon::Count;
    switch (boon)
    {
    case PathBoon::Relic:  ++_pendingRelicChoices; break;   // the pick opens at Zeph's camp
    case PathBoon::Gold:   _player.AddGold(PathBoonAmount(boon, _worldZone)); break;
    case PathBoon::Echoes: _player.AddCells(PathBoonAmount(boon, _worldZone)); break;   // banked at the camp
    case PathBoon::Vigor:  _player.Heal(9999); break;
    default:               return;
    }
    _contractToastText  = "ROAD'S REWARD: " + DescribePathBoon(boon, _worldZone);
    _contractToastTimer = 4.f;
}

void Engine::DrawCrossroadsSigns(Vector2 worldOffset) const
{
    const float sw = (float)kVirtualWidth, sh = (float)kVirtualHeight;
    const float cellW = sw / (float)RoomLayout::kCols, cellH = sh / (float)RoomLayout::kRows;
    const Vector2 toScreen{ worldOffset.x + sw * 0.5f, worldOffset.y + sh * 0.5f };
    const float pulse = 0.5f + 0.5f * sinf((float)GetTime() * 2.4f);
    const int doorStartC = kDoorStartCol, doorStartR = kDoorStartRow;

    for (int door = 0; door < 3; ++door)
    {
        const CrossroadsDoor& d = _crossroadsDoors[door];
        if (!d.open) continue;
        const Color tint = WorldMapManager::GetBiomeColor(d.biome);
        // Sign text runs brighter than the biome colour so dark domains
        // (Graveyard, Demon's Insides) still read on the dark panel.
        const Color bright{ (unsigned char)(tint.r + (255 - tint.r) * 0.45f), (unsigned char)(tint.g + (255 - tint.g) * 0.45f),
                            (unsigned char)(tint.b + (255 - tint.b) * 0.45f), 255 };

        // A glow in the doorway, in the colour of where it leads.
        Rectangle opening = door == 1
            ? Rectangle{ doorStartC * cellW, 0.f, kDoorSpanCols * cellW, cellH * 1.6f }
            : door == 0 ? Rectangle{ 0.f, doorStartR * cellH, cellW * 1.6f, kDoorSpanRows * cellH }
                        : Rectangle{ sw - cellW * 1.6f, doorStartR * cellH, cellW * 1.6f, kDoorSpanRows * cellH };
        opening.x += toScreen.x;
        opening.y += toScreen.y;
        const Color glow = Fade(tint, 0.20f + 0.15f * pulse), clear = Fade(tint, 0.f);
        if (door == 1)
            DrawRectangleGradientV((int)opening.x, (int)opening.y, (int)opening.width, (int)opening.height, glow, clear);
        else if (door == 0)
            DrawRectangleGradientH((int)opening.x, (int)opening.y, (int)opening.width, (int)opening.height, glow, clear);
        else
            DrawRectangleGradientH((int)opening.x, (int)opening.y, (int)opening.width, (int)opening.height, clear, glow);

        // The sign: where it leads, who guards it, what arriving gives.
        std::string name = GetBiomeName(d.biome);
        for (char& c : name) c = (char)toupper((unsigned char)c);
        const std::string guardian = std::string("Guardian: ") + GetBiomeGuardianName(d.biome);
        const std::string reward = "Arrive with " + DescribePathBoon(d.boon, _worldZone + 1);
        const int nameFs = 34, lineFs = 22, rewardFs = 24;
        const float textW = (float)std::max({ MeasureText(name.c_str(), nameFs), MeasureText(guardian.c_str(), lineFs),
                                              MeasureText(reward.c_str(), rewardFs) });
        const float panelW = textW + 40.f, panelH = 124.f;
        float px, py;
        if (door == 1)      { px = sw * 0.5f - panelW * 0.5f; py = cellH * 3.4f; }   // below the room banner
        else if (door == 0) { px = cellW * 1.9f;              py = sh * 0.5f - panelH * 0.5f; }
        else                { px = sw - cellW * 1.9f - panelW; py = sh * 0.5f - panelH * 0.5f; }
        px += toScreen.x;
        py += toScreen.y;
        DrawRectangleRounded({ px, py, panelW, panelH }, 0.18f, 8, Fade(Color{ 16, 14, 22, 255 }, 0.82f));
        DrawRectangleRoundedLinesEx({ px, py, panelW, panelH }, 0.18f, 8, 2.f, Fade(bright, 0.85f));
        DrawText(name.c_str(), (int)(px + panelW * 0.5f - MeasureText(name.c_str(), nameFs) * 0.5f), (int)(py + 12.f), nameFs, bright);
        DrawText(guardian.c_str(), (int)(px + panelW * 0.5f - MeasureText(guardian.c_str(), lineFs) * 0.5f), (int)(py + 54.f),
                 lineFs, Color{ 210, 205, 220, 255 });
        DrawText(reward.c_str(), (int)(px + panelW * 0.5f - MeasureText(reward.c_str(), rewardFs) * 0.5f), (int)(py + 86.f),
                 rewardFs, Color{ 255, 210, 110, 255 });
    }
}
