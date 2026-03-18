#include "Leaderboard.h"
#include <fstream>
#include <sstream>
#include <algorithm>

void Leaderboard::Load(const std::string& path)
{
    _entries.clear();
    std::ifstream file(path);
    if (!file.is_open()) return;

    std::string line;
    while (std::getline(file, line))
    {
        if (line.empty()) continue;
        LeaderboardEntry e;
        std::istringstream ss(line);
        if (!(ss >> e.wave >> e.time >> e.kills)) continue;
        std::getline(ss, e.name);
        if (!e.name.empty() && e.name.front() == ' ')
            e.name.erase(0, 1);
        _entries.push_back(e);
    }
}

void Leaderboard::Save(const std::string& path) const
{
    std::ofstream file(path);
    for (const auto& e : _entries)
        file << e.wave << " " << e.time << " " << e.kills << " " << e.name << "\n";
}

void Leaderboard::AddEntry(int wave, float time, int kills, const std::string& name)
{
    _entries.push_back({ wave, time, kills, name });

    std::sort(_entries.begin(), _entries.end(), [](const LeaderboardEntry& a, const LeaderboardEntry& b)
    {
        if (a.wave  != b.wave)  return a.wave  > b.wave;
        if (a.kills != b.kills) return a.kills > b.kills;
        return a.time < b.time;
    });

    if ((int)_entries.size() > MAX_ENTRIES)
        _entries.resize(MAX_ENTRIES);
}
