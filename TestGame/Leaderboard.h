#pragma once
#include <vector>
#include <string>

struct LeaderboardEntry
{
    int         wave  = 0;
    float       time  = 0.f;
    int         kills = 0;
    std::string name;
};

class Leaderboard
{
public:
    void Load(const std::string& path);
    void Save(const std::string& path) const;
    void AddEntry(int wave, float time, int kills, const std::string& name);

    const std::vector<LeaderboardEntry>& GetEntries() const { return _entries; }

private:
    static constexpr int MAX_ENTRIES = 10;
    std::vector<LeaderboardEntry> _entries;
};
