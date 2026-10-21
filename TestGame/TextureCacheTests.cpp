#include "TextureCache.h"

#include <cstdio>
#include <cstdlib>
#include <map>
#include <filesystem>
#include <set>
#include <string>

// Stub raylib: count GPU uploads/frees and let the test "edit" files on disk.
static std::map<std::string, long> modTimes;
static std::set<unsigned int> live;
static unsigned int nextId = 1;
static int loads = 0, unloads = 0, doubleFrees = 0;

extern "C" {
long GetFileModTime(const char* path) { return modTimes[path]; }
Texture2D LoadTexture(const char* path)
{
    Texture2D t{};
    if (std::string(path) == "missing.png") return t;
    t.id = nextId++;
    live.insert(t.id);
    ++loads;
    return t;
}
void UnloadTexture(Texture2D t)
{
    if (!live.erase(t.id)) ++doubleFrees;
    ++unloads;
}
}

static void check(bool ok, const char* message)
{
    if (!ok) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
}

int main()
{
    TextureCache& cache = TextureCache::Get();

    // Two renderers (active room + incoming scroll room) share the catalog.
    Texture2D a1 = cache.Acquire("forest.png");
    Texture2D g1 = cache.Acquire("ground.png");
    Texture2D a2 = cache.Acquire("forest.png");
    Texture2D g2 = cache.Acquire("ground.png");
    check(loads == 2, "shared sheets upload once");
    check(a1.id == a2.id && g1.id == g2.id, "shared handles are identical");

    Texture2D alias = cache.Acquire("./folder/../forest.png");
    check(alias.id == a1.id && loads == 2, "lexical aliases share one texture");
    cache.Release(alias);
    alias = cache.Acquire(std::filesystem::absolute("forest.png").string());
    check(alias.id == a1.id && loads == 2, "absolute and relative paths share one texture");
    cache.Release(alias);

    // Scroll renderer unloads after the door; the active renderer keeps drawing.
    cache.Release(a2);
    cache.Release(g2);
    check(unloads == 0 && live.count(a1.id), "releasing one holder keeps texture resident");

    // Warm door crossing: re-acquire before releasing (Init order) -> no uploads.
    for (int door = 0; door < 50; ++door)
    {
        Texture2D next = cache.Acquire("forest.png");
        cache.Release(a1);
        a1 = next;
    }
    check(loads == 2 && unloads == 0, "warm traversal performs no uploads");

    // Editor saves the image: next Acquire gets fresh pixels, old holder stays valid.
    modTimes["forest.png"] = 99;
    Texture2D edited = cache.Acquire("forest.png");
    check(edited.id != a1.id && loads == 3, "edited file reloads");
    check(live.count(a1.id) == 1, "old texture survives while still held");
    cache.Release(a1);
    check(live.count(a1.id) == 0, "old texture freed once last holder releases");

    // Missing files are not cached and releasing them is harmless.
    Texture2D missing = cache.Acquire("missing.png");
    check(missing.id == 0, "missing file yields id 0");
    cache.Release(missing);

    cache.Release(edited);
    cache.Release(g1);
    check(cache.ResidentCount() == 0 && live.empty(), "everything freed at the end");
    check(doubleFrees == 0, "no texture is freed twice");

    std::puts("Texture cache tests passed: sharing, warm traversal, edit reload, lifetime, missing files.");
    return 0;
}
