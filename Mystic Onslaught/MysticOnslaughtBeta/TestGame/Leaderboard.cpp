#include "Leaderboard.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <vector>

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
        std::vector<std::string> tokens;
        std::string token;
        while (ss >> token)
            tokens.push_back(token);

        if (tokens.size() < 3)
            continue;

        e.wave = std::stoi(tokens[0]);

        size_t nameIndex = 2;
        if (tokens.size() >= 4 && tokens[1].find('.') != std::string::npos)
            nameIndex = 3;

        e.kills = std::stoi(tokens[nameIndex - 1]);
        for (size_t i = nameIndex; i < tokens.size(); ++i)
        {
            if (!e.name.empty())
                e.name += " ";
            e.name += tokens[i];
        }
        _entries.push_back(e);
    }
}

void Leaderboard::Save(const std::string& path) const
{
    std::ofstream file(path);
    for (const auto& e : _entries)
        file << e.wave << " " << e.kills << " " << e.name << "\n";
}

void Leaderboard::AddEntry(int wave, int kills, const std::string& name)
{
    _entries.push_back({ wave, kills, name });

    std::sort(_entries.begin(), _entries.end(), [](const LeaderboardEntry& a, const LeaderboardEntry& b)
    {
        if (a.wave  != b.wave)  return a.wave  > b.wave;
        if (a.kills != b.kills) return a.kills > b.kills;
        return a.name < b.name;
    });

    if ((int)_entries.size() > MAX_ENTRIES)
        _entries.resize(MAX_ENTRIES);
}
