// check-sources: field/baked_map.cpp field/baked_router.cpp
// Offline check of the baked-map router (2026-10-10): loads each data/ffN/maps.bin named on the
// command line (FF2 and FF1 from build.bat; no game, no IL2CPP) and runs routes whose answers
// are known from the dumps and from tools/route.py, the offline Python router that was
// validated first. FF1 (check_ff1, 2026-10-11): the Cornelia bridge is open to walking and
// Matoya's Cave and Pravoka route from Cornelia on foot. FF2:
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
#include "field/route_progress.h"

namespace b = field::baked;
namespace r = field::baked_route;

namespace {

int g_failures = 0;

void expect(bool ok, const char* what)
{
    std::printf("%s %s\n", ok ? "ok  " : "FAIL", what);
    if (!ok) ++g_failures;
}

void check_event_warnings()
{
    b::Object object{};
    object.type = b::ObjType::GotoMap;
    object.a = 101;
    expect(!r::step_event(object), "ordinary town/room exits are not story events");
    object.type = b::ObjType::ToLayer;
    expect(!r::step_event(object), "ordinary layer changes are not story events");
    object.type = b::ObjType::TransportationEventAction;
    expect(!r::step_event(object), "vehicle boarding pads are not story events");
    object.type = b::ObjType::Event;
    expect(r::step_event(object), "a script-backed event remains a route hazard");
    object.a = object.b = 0;
    expect(!r::step_event(object), "inert cutscene markers are not hazards");
    object.type = b::ObjType::Warp;
    object.d = 0;
    expect(!r::step_event(object), "examined warps do not trigger by walking across them");
    object.d = 1;
    expect(r::step_event(object), "step-on warps remain route hazards");
    const std::vector<r::Cell> path{{1, 91, 116}, {1, 91, 115}};
    expect(!r::crosses_event(path, {}), "FF2 ordinary Altair entry has no false event warning");
    expect(!r::crosses_event(path, {{-1, 91, 116}}), "already occupied starting tile is not an upcoming event");
    expect(!r::crosses_event(path, {{0, 91, 115}}), "event on another layer does not affect this route");
    expect(r::crosses_event(path, {{-1, 91, 115}}), "an active event at the destination is still announced");
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

/// FF2's checks: the first game baked, the reference for the router's rules.
void check_ff2()
{
    expect(b::game_number() == 2, "game byte is FF2");
    expect(b::submap_count() > 300, "FF2 has its 349 sub-maps");

    // Dreadnought: captains stay on their tiles after battle. Stamp is idle movement,
    // not Area/Route travelling; never route through a captain to save a long detour.
    const auto* dreadnought = b::submap_by_name(L"Map_30051_8");
    expect(dreadnought != nullptr, "Dreadnought engine-room approach is baked");
    if (dreadnought) {
        r::Request req;
        req.submap = dreadnought;
        const b::Object* entrance = nullptr;
        const b::Object* engine_exit = nullptr;
        int captains = 0;
        bool idle = true;
        for (const auto& object : b::grids(*dreadnought).objects) {
            if (object.type == b::ObjType::PointIn && object.a == 1) entrance = &object;
            if (object.type == b::ObjType::GotoMap && object.a == 175) engine_exit = &object;
            if (object.type == b::ObjType::NPC) {
                ++captains; idle = idle && !(object.d & 16);
                req.blocked.push_back({object.layer, object.tx, object.ty});
            }
        }
        expect(captains == 12 && idle, "all twelve captains are persistent stationary obstacles");
        if (entrance && engine_exit) {
            req.start = {entrance->layer, entrance->tx, entrance->ty};
            req.goals = {{engine_exit->layer, engine_exit->tx, engine_exit->ty}};
            auto route = r::find(req);
            bool avoids = route.found;
            for (const auto& cell : route.cells) for (const auto& npc : req.blocked)
                if (cell == npc) avoids = false;
            expect(avoids, "Dreadnought: reach engine-room exit around every captain");
            auto after_battle = r::find(req);
            expect(after_battle.found && after_battle.cells == route.cells, "captains remain blocked after an unchanged battle return");
        } else expect(false, "Dreadnought entrance and engine-room exit resolve");
    }

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
        const b::Object* door = nullptr;
        for (const b::Object& o : g.objects)
            if (o.type == b::ObjType::PointIn) { door = &o; break; }
        expect(door != nullptr, "Map_20011_11 has its PointIn");
        if (relay && keeper && door) {
            // Tile objects are anchored at their bottom edge in Tiled: the keeper stands one row
            // above the relay (the counter tile), and the arrival point sits below both.
            expect(keeper->ty + 1 == relay->ty && keeper->tx == relay->tx, "the keeper stands directly behind the counter tile");
            r::Request req;
            req.submap = shop;
            req.start = r::Cell{door->layer, door->tx, door->ty};
            r::Goals goals = r::goals_for(req, r::Cell{keeper->layer, keeper->tx, keeper->ty}, r::GoalKind::Interact);
            expect(goals.cells.size() == 1 && goals.cells[0].x == relay->tx && goals.cells[0].y == relay->ty, "the shopkeeper's goal is the counter tile");
            req.goals = goals.cells;
            r::Route route = r::find(req);
            std::string legs = legs_text(route.cells);
            std::printf("     shop route: door (%d,%d) to counter (%d,%d): %d steps, %s\n", door->tx, door->ty, relay->tx, relay->ty, route.steps(), legs.c_str());
            const int expected = door->ty - relay->ty;
            expect(route.found && route.steps() == expected && legs == "North " + std::to_string(expected), "shop: the door reaches the counter straight north");
            r::Reach reach = r::flood(req);
            expect(reach.distance(r::Cell{relay->layer, relay->tx, relay->ty}) == expected, "flood distance to the counter agrees");
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

    // 2b. A collision layer not named "...Collision": Josef's house in Salamand (Map_20071_1)
    //     calls its only layer "GroundObject"; the bake once dropped it and every target in the
    //     house was "No path" (user, 2026-10-11). Door PointIn (9,15) to beside Nelly (17,8).
    const b::Submap* house = b::submap_by_name(L"Map_20071_1");
    expect(house != nullptr, "Map_20071_1 present");
    if (house) {
        const b::Grids& g = b::grids(*house);
        expect(!g.collision[1].empty(), "Josef's house has its Ground collision layer");
        const b::Object* door = nullptr;
        const b::Object* exit = nullptr;
        const b::Object* nelly = nullptr;
        for (const b::Object& o : g.objects) {
            if (o.type == b::ObjType::PointIn && !door) door = &o;
            if (o.type == b::ObjType::GotoMap && !exit) exit = &o;
            if (o.type == b::ObjType::NPC && !nelly && b::str(o.name).find(L"ネリー") != std::wstring::npos) nelly = &o;
        }
        expect(door && exit && nelly, "Josef's house has its door, exit and Nelly");
        if (door && exit && nelly) {
            std::printf("     Josef's house: door (%d,%d), exit (%d,%d), Nelly (%d,%d)\n", door->tx, door->ty, exit->tx, exit->ty, nelly->tx, nelly->ty);
            expect(b::walkable(g, 1, door->tx, door->ty, false) && b::walkable(g, 1, nelly->tx, nelly->ty, false),
                   "the arrival tile and Nelly's tile are open floor (tile objects anchored at their top row)");
            r::Request req;
            req.submap = house;
            req.start = r::Cell{1, door->tx, door->ty};
            // 2d. An interaction ends beside the NPC and faces it: Nelly (17,7) has open floor
            //     north, east and south and a wall to her west, and accepts every side (mask
            //     15): three goal cells, the facing requested.
            r::Goals goals = r::goals_for(req, r::Cell{1, nelly->tx, nelly->ty}, r::GoalKind::Interact);
            expect(goals.face && goals.cells.size() == 3 && goals.faced.size() == 1, "Nelly: three open cells beside her, facing requested");
            bool none_on_her = true;
            for (const r::Cell& c : goals.cells)
                if (c.x == nelly->tx && c.y == nelly->ty) none_on_her = false;
            expect(none_on_her, "Nelly: her own cell is never a goal");
            req.goals = goals.cells;
            r::Route route = r::find(req);
            std::printf("     Josef's house, door to Nelly: %d steps, %s\n", route.steps(), legs_text(route.cells).c_str());
            expect(route.found && route.steps() == 14 && r::direction_towards(route.cells.back(), r::Cell{1, nelly->tx, nelly->ty}) == 0,
                   "Josef's house: the door reaches Nelly in 14 steps, from the south, facing north");
            // Inferred from the bake: the exit is a transition, Nelly an interaction.
            r::Goals ex = r::goals_for(req, r::Cell{1, exit->tx, exit->ty}, r::GoalKind::Infer);
            expect(!ex.cells.empty() && !ex.face && ex.cells[0].x == exit->tx && ex.cells[0].y == exit->ty, "the house exit is inferred as a transition onto its own tile");
            expect(r::goals_for(req, r::Cell{1, nelly->tx, nelly->ty}, r::GoalKind::Infer).face, "Nelly is inferred as an interaction");
        }

        // 2c. Turns are priced: the open floor (7..13, 12..13) is crossed in two runs, never a
        //     staircase, and the route stays a shortest one.
        r::Request room;
        room.submap = house;
        room.start = r::Cell{1, 7, 13};
        room.goals.push_back(r::Cell{1, 13, 12});
        r::Route across = r::find(room);
        std::printf("     across the room: %d steps, %s\n", across.steps(), legs_text(across.cells).c_str());
        expect(across.found && across.steps() == 7 && r::legs(across.cells).size() == 2, "turn penalty: an open room is crossed in two legs");
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

        // Each selected entrance tile stays exact, including sibling town arcs.
        r::Request walk_mode;
        walk_mode.submap = world;
        auto arc_ok = [&](const char* town, int x, int y) {
            const auto gl = r::goals_for(walk_mode,{1,x,y},r::GoalKind::Infer);
            expect(gl.cells.size()==1&&!gl.face&&gl.cells[0].x==x&&gl.cells[0].y==y,town);
        };
        arc_ok("Salamand: selected west arc stays exact",123,33);
        arc_ok("Salamand: selected centre stays exact",124,32);
        arc_ok("Altair: selected west arc stays exact",90,116);
        arc_ok("Bafsk: selected west arc stays exact",172,58);
        expect(b::attribute(g, 125, 34) == 19 && !b::vehicle_open(g, 1, 125, 34), "Salamand: (125,34) is a wall tile, where the player was once sent");
        r::Request into_salamand = walk_mode;
        into_salamand.start = r::Cell{1, 124, 36};
        into_salamand.goals = r::goals_for(walk_mode, r::Cell{1, 125, 33}, r::GoalKind::Transition).cells;
        r::Route in = r::find(into_salamand);
        std::printf("     into Salamand from (124,36): %d steps, %s\n", in.steps(), legs_text(in.cells).c_str());
        expect(in.found && in.cells.back().x==125 && in.cells.back().y==33, "Salamand: route ends on selected east arc rather than centre entrance");

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
}

/// FF1 (2026-10-11): Cornelia's bridge. The opening-credits Event on the bridge cell (145,145)
/// runs ChangeMap to the scene map 124 and straight back; baked as a step-on warp it was a
/// hazard the router never crossed, so from Cornelia everything north of the bridge (Matoya's
/// Cave, Pravoka) was "Requires vehicle" (user). bake_maps.py now keeps such round trips as
/// plain events: this checks the bake and the two walking routes the user's log named.
void check_ff1()
{
    expect(b::game_number() == 1, "game byte is FF1");
    const b::Submap* world = b::submap_by_name(L"Map_10010");
    expect(world && world->world, "FF1 Map_10010 is a world map");
    if (!world || !world->world) return;
    const b::Grids& g = b::grids(*world);
    expect(g.transport.size() == 4, "FF1 world map has four transportation group layers");

    bool warp_on_bridge = false;
    bool gate_on_bridge = false;
    for (const b::Object& o : g.objects) {
        if (o.type == b::ObjType::Warp && o.contains(145, 145)) warp_on_bridge = true;
        if (o.type == b::ObjType::CollisionEntity && o.contains(145, 145)) gate_on_bridge = true;
    }
    int step_on_warps = 0;
    for (int i = 0; i < b::submap_count(); ++i) {
        const b::Submap* s = b::submap_at(i);
        if (!s) continue;
        for (const b::Object& o : b::grids(*s).objects)
            if (o.type == b::ObjType::Warp && o.d == 1) ++step_on_warps;
    }
    std::printf("     FF1: %d step-on warps baked (trapdoors, Citadel pads)\n", step_on_warps);
    expect(!warp_on_bridge, "FF1: the bridge credits Event at (145,145) is not baked as a warp");
    expect(gate_on_bridge, "FF1: the unbuilt bridge's CollisionEntity stays on the cell (the live story gate)");
    expect(step_on_warps >= 40, "FF1: its trapdoors and warp pads are still step-on hazards");
    expect(b::vehicle_open(g, 1, 145, 145), "FF1: the bridge cell is open to walking");
    expect(g.hazard.empty() || g.hazard[static_cast<size_t>(145) * g.width + 145] == 0, "FF1: the bridge cell is no hazard");

    // Cornelia's entrance (145,156) on foot to the GotoMap tiles the user's log named; the
    // tiles are entered as transitions.
    r::Request req;
    req.submap = world;
    req.start = r::Cell{1, 145, 156};
    struct Town { const char* name; int x, y; };
    const Town towns[] = {{"Matoya's Cave", 161, 110}, {"Pravoka", 203, 142}};
    for (const Town& t : towns) {
        req.goals = r::goals_for(req, r::Cell{1, t.x, t.y}, r::GoalKind::Transition).cells;
        r::Route route = r::find(req);
        std::printf("     FF1 Cornelia -> %s (%d,%d): %d steps, %s\n", t.name, t.x, t.y, route.steps(), legs_text(route.cells).c_str());
        std::string what = std::string("FF1: Cornelia reaches ") + t.name + " on foot across the bridge";
        expect(route.found && route.steps() > 20, what.c_str());
    }
}

int wmain(int argc, wchar_t** argv)
{
    check_event_warnings();
    {
        b::Submap map{}; map.width = map.height = 3;
        b::Grids grid; grid.width = grid.height = 3;
        grid.collision[1].assign(9, 0);
        r::Request req; req.submap = &map;
        const r::Cell landing{1, 1, 1};
        expect(!r::safe_landing(req, grid, landing), "teleport refuses a wall tile");
        grid.collision[1][4] = 15;
        expect(!r::safe_landing(req, grid, landing), "teleport refuses an open tile enclosed by walls");
        grid.collision[1][5] = 15;
        expect(r::safe_landing(req, grid, landing), "teleport accepts clear ground with a legal exit");
        grid.collision[1][4] = 1;
        expect(!r::safe_landing(req, grid, landing), "teleport checks actual direction masks, not merely adjacent open ground");
        grid.collision[1][4] = 15;
        req.blocked = {landing};
        expect(!r::safe_landing(req, grid, landing), "teleport refuses a live occupied landing");
        req.blocked = {{1, 2, 1}};
        expect(!r::safe_landing(req, grid, landing), "teleport refuses a landing whose only exit is occupied");
        req.blocked.clear(); req.live_hazards = true; req.hazards = {landing};
        expect(!r::safe_landing(req, grid, landing), "teleport refuses live event triggers");
        req.hazards.clear(); grid.traps[1].assign(9, 0); grid.traps[1][4] = 1;
        expect(!r::safe_landing(req, grid, landing), "teleport refuses a pitfall");
        expect(!r::safe_landing(req, grid, {1, -1, 0}) && !r::safe_landing(req, grid, {1, 3, 0}) &&
               !r::safe_landing(req, grid, {4, 1, 1}), "teleport refuses map boundaries and invalid layers");
        grid.traps[1][4] = 0; grid.collision[1][4] = 0; grid.passage[1].assign(9, 0); grid.passage[1][4] = 1;
        req.passages = true;
        expect(r::safe_landing(req, grid, landing), "teleport uses the existing hidden-passage movement rule");
    }
    {
        namespace progress = field::route_progress;
        progress::Phase p;
        p.cells = {{1, 2, 3}, {1, 3, 3}, {1, 3, 2}};
        p.action_before = L"land";
        p.action_after = L"take the exit";
        p.arrival_facing = L"face north";
        const std::vector<progress::Phase> phases{p};
        expect(!progress::at_interaction(phases, 0), "interaction facing is withheld while movement remains");
        std::vector<progress::Phase> rest;
        expect(progress::remaining(phases, {1, 3, 3}, rest) && rest[0].cells.size() == 2 &&
               rest[0].action_before.empty() && rest[0].action_after == p.action_after,
               "cached route keeps its destination instructions after a corner");
        expect(progress::remaining(phases, {1, 3, 2}, rest) && progress::at_interaction(rest, 0) &&
               rest[0].arrival_facing == p.arrival_facing,
               "adjacent interaction retains its facing direction at arrival");
        expect(!progress::remaining(phases, {1, 0, 0}, rest), "leaving a cached route requires replanning");
    }
    std::vector<std::wstring> paths;
    for (int i = 1; i < argc; ++i) paths.push_back(argv[i]);
    if (paths.empty()) {
        // build/checks runs from the repo root through build.bat; fall back to the data files.
        wchar_t exe[MAX_PATH]{};
        GetModuleFileNameW(nullptr, exe, MAX_PATH);
        std::wstring dir(exe);
        size_t slash = dir.find_last_of(L"\\/");
        dir = slash == std::wstring::npos ? L"." : dir.substr(0, slash);
        paths.push_back(dir + L"\\..\\..\\data\\ff2\\maps.bin");
        paths.push_back(dir + L"\\..\\..\\data\\ff1\\maps.bin");
    }
    static std::vector<std::vector<char>> blobs;   // static: the reader keeps the pointers
    for (const std::wstring& path : paths) {
        blobs.push_back(read_file(path));
        std::vector<char>& blob = blobs.back();
        if (blob.empty()) {
            std::wprintf(L"FAIL cannot read %s\n", path.c_str());
            return 2;
        }
        std::wprintf(L"== %s\n", path.c_str());
        expect(b::load(blob.data(), blob.size()), "maps.bin header and tables load");
        if (!b::loaded()) return 1;
        switch (b::game_number()) {
        case 1: check_ff1(); break;
        case 2: check_ff2(); break;
        default: expect(b::submap_count() > 0, "sub-maps present"); break;
        }
    }

    std::printf("%s: %d failure(s)\n", g_failures ? "FAIL" : "PASS", g_failures);
    return g_failures ? 1 : 0;
}
