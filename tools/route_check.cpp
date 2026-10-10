// check-sources: field/baked_map.cpp field/baked_router.cpp
// Offline check of the baked-map router (2026-10-10): loads a game's data/ffN/maps.bin from
// disk (no game, no IL2CPP) and runs routes whose answers are known from the dumps and from
// tools/route.py, the offline Python router that was validated first:
//  * FF2 Map_20011_11 (a Fynn shop), Bottom layer, the entrance tile (8,15) to the counter
//    tile (8,8), the RelayInteraction that the shopkeeper's link_object_id names: "North 7";
//  * FF2 Map_20060, a map with a ToLayer at (34,25) Ground -> Upper: cells on the upper layer
//    are reachable from the ground layer through it;
//  * FF2 Map_10010 (the world map): Salamand's entrance (123,34) to Semitt Falls (107,44) on
//    foot (transportation group 1): found, at least the Manhattan distance; the snowcraft
//    (group 5) and the ship (group 4) have open cells and route between two of them.
// Exit code 0 = every expectation holds. Run by build.bat through out/checks/route_check.exe.
#include <windows.h>
#include <cstdio>
#include <string>
#include <vector>
#include "field/baked_map.h"
#include "field/baked_router.h"

namespace b = field::baked;
namespace r = field::baked_route;

namespace {

int g_failures = 0;

void expect(bool ok, const char* what)
{
    std::printf("%s %s\n", ok ? "ok  " : "FAIL", what);
    if (!ok) ++g_failures;
}

std::vector<char> read_file(const std::wstring& path)
{
    std::vector<char> out;
    HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    if (h == INVALID_HANDLE_VALUE) return out;
    LARGE_INTEGER size{};
    if (GetFileSizeEx(h, &size) && size.QuadPart > 0 && size.QuadPart < (1ll << 30)) {
        out.resize(static_cast<size_t>(size.QuadPart));
        DWORD got = 0;
        if (!ReadFile(h, out.data(), static_cast<DWORD>(out.size()), &got, nullptr) || got != out.size()) out.clear();
    }
    CloseHandle(h);
    return out;
}

std::string legs_text(const std::vector<r::Cell>& cells)
{
    static const char* kDir[] = {"North", "East", "South", "West"};
    std::string s;
    for (const r::Leg& leg : r::legs(cells)) {
        if (!s.empty()) s += ", ";
        s += kDir[leg.dir];
        s += " " + std::to_string(leg.count);
    }
    return s;
}

/// The first open cell of a world layer scanning from (x0, y0) outward in rows.
bool first_open(const b::Grids& g, int group, int x0, int y0, int& x, int& y)
{
    for (int radius = 0; radius < 64; ++radius) {
        for (int dy = -radius; dy <= radius; ++dy) {
            for (int dx = -radius; dx <= radius; ++dx) {
                if (std::abs(dx) != radius && std::abs(dy) != radius) continue;
                if (b::vehicle_open(g, group, x0 + dx, y0 + dy)) {
                    x = x0 + dx;
                    y = y0 + dy;
                    return true;
                }
            }
        }
    }
    return false;
}

} // namespace

int wmain(int argc, wchar_t** argv)
{
    std::wstring path = argc > 1 ? argv[1] : L"";
    if (path.empty()) {
        // build/checks runs from the repo root through build.bat; fall back to the data file.
        wchar_t exe[MAX_PATH]{};
        GetModuleFileNameW(nullptr, exe, MAX_PATH);
        std::wstring dir(exe);
        size_t slash = dir.find_last_of(L"\\/");
        dir = slash == std::wstring::npos ? L"." : dir.substr(0, slash);
        path = dir + L"\\..\\..\\data\\ff2\\maps.bin";
    }
    static std::vector<char> blob = read_file(path);   // static: the reader keeps the pointer
    if (blob.empty()) {
        std::wprintf(L"FAIL cannot read %s\n", path.c_str());
        return 2;
    }
    expect(b::load(blob.data(), blob.size()), "maps.bin header and tables load");
    if (!b::loaded()) return 1;
    expect(b::game_number() == 2, "game byte is FF2");
    expect(b::submap_count() > 300, "FF2 has its 349 sub-maps");

    // 1. The shop counter rule: entrance to the relay tile.
    const b::Submap* shop = b::submap_by_name(L"Map_20011_11");
    expect(shop != nullptr, "Map_20011_11 present");
    if (shop) {
        const b::Grids& g = b::grids(*shop);
        expect(g.width == 16 || g.width > 0, "shop grid decoded");
        const b::Object* relay = nullptr;
        const b::Object* keeper = nullptr;
        for (const b::Object& o : g.objects) {
            if (o.type == b::ObjType::RelayInteraction) relay = &o;
            if (o.type == b::ObjType::ShopNPC) keeper = &o;
        }
        expect(relay && keeper && keeper->a == relay->a, "shopkeeper link_object_id names the RelayInteraction");
        if (relay) {
            r::Request req;
            req.submap = shop;
            req.start = r::Cell{0, 8, 15};
            req.goals.push_back(r::Cell{relay->layer, relay->tx, relay->ty});
            r::Route route = r::find(req);
            std::string legs = legs_text(route.cells);
            std::printf("     shop route: %d steps, %s\n", route.steps(), legs.c_str());
            expect(route.found && route.steps() == 7 && legs == "North 7", "shop: entrance (8,15) to counter (8,8) = North 7");
            r::Reach reach = r::flood(req);
            expect(reach.distance(r::Cell{0, 8, 8}) == 7, "flood distance to the counter is 7");
            expect(!reach.reachable(r::Cell{0, 0, 0}), "a wall cell is unreachable");
        }
    }

    // 2. Layer change through a ToLayer object.
    const b::Submap* layered = b::submap_by_name(L"Map_20060");
    expect(layered != nullptr, "Map_20060 present");
    if (layered) {
        const b::Grids& g = b::grids(*layered);
        const b::Object* to_layer = nullptr;
        for (const b::Object& o : g.objects)
            if (o.type == b::ObjType::ToLayer) { to_layer = &o; break; }
        expect(to_layer != nullptr, "Map_20060 has a ToLayer object");
        if (to_layer) {
            // Start next to the ToLayer on its own layer, on an open cell.
            r::Cell start{to_layer->layer, to_layer->tx, to_layer->ty};
            r::Request req;
            req.submap = layered;
            req.start = start;
            r::Reach reach = r::flood(req);
            int upper_reachable = 0;
            const int target = to_layer->a;
            if (target >= 0 && target <= 2 && !reach.dist[target].empty())
                for (uint16_t d : reach.dist[target]) upper_reachable += d != 0xFFFF;
            std::printf("     ToLayer at (%d,%d) layer %d -> %d: %d cells reachable on the target layer\n",
                        to_layer->tx, to_layer->ty, to_layer->layer, target, upper_reachable);
            expect(upper_reachable > 0, "cells on the ToLayer's target layer are reachable through it");
        }
    }

    // 3. The world map: on foot and by vehicle group.
    const b::Submap* world = b::submap_by_name(L"Map_10010");
    expect(world && world->world, "Map_10010 is a world map");
    if (world && world->world) {
        const b::Grids& g = b::grids(*world);
        expect(g.transport.size() == 6, "FF2 world map has six transportation group layers");
        expect(!g.attribute.empty(), "world attribute grid decoded");
        // Semitt Falls sits in a pocket of five walkable cells that only the river touches
        // (the dumped Transportation01 layer, 2026-10-10): on foot alone there is no route,
        // with the canoe owned (group 2 joins walking automatically) there is one.
        r::Request req;
        req.submap = world;
        req.start = r::Cell{1, 123, 34};
        req.goals.push_back(r::Cell{1, 107, 44});
        r::Route on_foot = r::find(req);
        expect(!on_foot.found, "world: Salamand (123,34) to Semitt Falls (107,44) needs the canoe");
        req.auto_groups.push_back(2);
        r::Route by_canoe = r::find(req);
        std::printf("     Salamand -> Semitt Falls with the canoe: %d steps, %s\n", by_canoe.steps(), legs_text(by_canoe.cells).c_str());
        expect(by_canoe.found && by_canoe.steps() >= 26, "world: Salamand to Semitt Falls with the canoe owned");

        // A town tile is closed to walking (attribute 19) but is entered as the goal: the game
        // opens GotoMap cells for a route. Bafsk (172,59) from the field cell beside it.
        expect(!b::vehicle_open(g, 1, 172, 59), "Bafsk's town tile is closed to walking");
        r::Request town;
        town.submap = world;
        town.start = r::Cell{1, 172, 58};
        town.goals.push_back(r::Cell{1, 172, 59});
        r::Route into_town = r::find(town);
        expect(into_town.found && into_town.steps() == 1 && legs_text(into_town.cells) == "South 1", "world: a closed town tile is entered as the goal (South 1)");
        town.goals_open = false;
        expect(!r::find(town).found, "world: the same town tile is closed when goals are not opened");

        // The ship (group 4) cannot enter a town tile; the snowcraft (group 5) has its field
        // around the Snow Cavern (177,15), which only it reaches.
        expect(!b::vehicle_open(g, 4, 123, 34), "the ship cannot enter Salamand's tile");
        expect(b::vehicle_open(g, 5, 177, 15) && !b::vehicle_open(g, 1, 177, 15), "the Snow Cavern entrance is snowcraft-only");
        for (int group : {4, 5}) {
            int x1, y1, x2, y2;
            bool a = first_open(g, group, group == 5 ? 177 : 128, group == 5 ? 18 : 128, x1, y1);
            bool c = first_open(g, group, group == 5 ? 177 : 60, group == 5 ? 15 : 200, x2, y2);
            r::Request v;
            v.submap = world;
            v.vehicle_group = group;
            v.start = r::Cell{1, x1, y1};
            v.goals.push_back(r::Cell{1, x2, y2});
            r::Reach reach = r::flood(v);
            int open_cells = 0;
            for (uint16_t d : reach.dist[0]) open_cells += d != 0xFFFF;
            std::printf("     group %d: open cells reachable from (%d,%d): %d; (%d,%d) %s\n", group, x1, y1, open_cells,
                        x2, y2, reach.reachable(r::Cell{1, x2, y2}) ? "reachable" : "not reachable");
            expect(a && c && open_cells > 100, group == 5 ? "snowcraft group layer has a field" : "ship group layer has water");
        }
        // Auto vehicles: with the snowcraft owned, on-foot reach grows (snow cells join).
        r::Request walk;
        walk.submap = world;
        walk.start = r::Cell{1, 123, 34};
        r::Reach plain = r::flood(walk);
        walk.auto_groups.push_back(5);
        r::Reach with_snowcraft = r::flood(walk);
        int n1 = 0, n2 = 0;
        for (uint16_t d : plain.dist[0]) n1 += d != 0xFFFF;
        for (uint16_t d : with_snowcraft.dist[0]) n2 += d != 0xFFFF;
        std::printf("     reach from Salamand on foot: %d cells; with the snowcraft: %d\n", n1, n2);
        expect(n2 >= n1, "the snowcraft never shrinks the on-foot reach");
    }

    std::printf("%s: %d failure(s)\n", g_failures ? "FAIL" : "PASS", g_failures);
    return g_failures ? 1 : 0;
}
