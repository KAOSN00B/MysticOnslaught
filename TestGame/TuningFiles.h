#pragma once

#include <string>

// Where the tuning files (charactertuning_*.txt, attacktuning_*.txt) are read
// and written: "" = the working directory (the source folder in dev builds,
// next to the exe in a package). The flow self-test points it at its sandbox
// so it never touches the real files; reload the stores after changing it.
namespace TuningFiles
{
    inline std::string& Directory()
    {
        static std::string directory;
        return directory;
    }

    inline std::string Path(const std::string& fileName)
    {
        const std::string& directory = Directory();
        return directory.empty() ? fileName : directory + "/" + fileName;
    }
}
