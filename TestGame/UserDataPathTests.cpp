#include "UserDataPath.h"
#include <cassert>
#include <cstdio>
#include <filesystem>

int main()
{
    const auto dir = std::filesystem::temp_directory_path() / "MysticOnslaught-path-test";
    const auto a = ResolveUserDataPath("metaprogress.cfg", dir.string(), true);
    const auto b = ResolveUserDataPath("settings.cfg", dir.string(), true);
    assert(std::filesystem::path(a).parent_path() == dir / "MysticOnslaught");
    assert(std::filesystem::path(b).parent_path() == std::filesystem::path(a).parent_path());
    assert(ResolveUserDataPath("metaprogress.cfg", dir.string(), false) == "metaprogress.cfg");
    assert(std::filesystem::path(a).filename() == "metaprogress.cfg");
    std::puts("User data path tests passed: full-game profile isolation and unchanged development paths.");
}
