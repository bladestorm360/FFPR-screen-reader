// check-sources: field/baked_map.cpp field/baked_router.cpp
// Replay real baked routes through the production follower, without loading any game.
#include "fieldnav/autowalk_route.h"
#include "fieldnav/autowalk_transition.h"
#include "field/baked_map.h"
#include <cstdio>
#include <vector>
#include <string>

namespace a = ffpr::fieldnav::autowalk;
namespace b = field::baked;
namespace r = field::baked_route;
int failures = 0;
void expect(bool yes, const char* message) { if (!yes) { ++failures; std::printf("FAIL: %s\n", message); } }

void boundaries()
{
    a::Route route;
    a::Phase p;
    p.cells = {{1,2,3}, {1,2,2}, {2,3,2}};
    p.arrival_facing = L"face north";
    p.arrival_direction = 0;
    route.set({p}, 8, 8, false);
    const auto first = route.next({1,2,3});
    expect(first.y == 1 && first.face_direction == -1, "native north axis without early facing");
    expect(route.next({1,2,3}).y == 1, "no progress without native displacement");
    const auto turn = route.next({1,2,2});
    expect(turn.x == 1 && turn.y == 0 && turn.next.layer == 2, "turn and stair layer");
    const auto done = route.next({2,3,2});
    expect(done.result == a::Result::Arrived && done.face_direction == 0 && done.instruction == L"face north", "structured arrival facing");
    p.cells = {{1,0,0}};
    route.set({p}, 8, 8, false);
    expect(route.next({1,0,0}).result == a::Result::Arrived && route.next({1,0,0}).face_direction == 0,
           "adjacent zero movement interaction faces target");
    p.cells = {{1,0,0}, {1,7,0}};
    route.set({p}, 8, 8, true);
    expect(route.next({1,0,0}).x == -1, "wrapped world seam uses west");
    route.set({p}, 8, 8, false);
    expect(route.next({1,0,0}).result == a::Result::OffRoute, "nonadjacent geometry never drives");
    p.cells = {{1,1,1}, {1,2,1}, {1,1,1}, {1,1,2}};
    route.set({p}, 8, 8, false);
    expect(route.next({1,1,1}).x == 1 && route.next({1,2,1}).x == -1 &&
        route.next({1,1,1}).y == -1, "revisited cell does not skip route loop");
    expect(route.next({1,7,7}).result == a::Result::OffRoute, "unexpected displacement stops");
    p.cells = {{1,1,1}}; p.manual_after = true; p.action_after = L"examine the warp";
    route.set({p}, 8, 8, false);
    expect(route.next({1,1,1}).result == a::Result::Action, "examine warp requires player action");
    p.manual_after = false; p.action_after.clear();
    a::Phase riding; riding.cells = {{1,2,1}, {1,3,1}}; riding.action_before = L"board the Ship";
    route.set({p, riding}, 8, 8, false);
    expect(route.next({1,1,1}).result == a::Result::Action, "boarding boundary cannot be crossed on foot");
    p.cells = {{1,1,1}, {1,1,1}, {1,2,1}};
    route.set({p}, 8, 8, false);
    expect(route.next({1,1,1}).x == 1, "native same-cell points preserve movement");

    r::Goals goals;
    goals.face = true;
    const r::Cell from{1,4,4};
    const r::Cell sides[] = {{1,4,3}, {1,5,4}, {1,4,5}, {1,3,4}};
    for (int dir = 0; dir < 4; ++dir) {
        goals.faced = {sides[dir]};
        expect(r::facing_direction(from, goals) == dir, "all four native cardinal arrival headings");
    }
    goals.faced = {{1,4,1}, {1,4,2}};
    expect(r::facing_direction(from, goals) == 0, "NPC across counter faces nearest footprint");
    goals.face = false;
    expect(r::facing_direction(from, goals) == -1, "waypoints do not turn player");
}

void transition_lifecycle()
{
    a::Transition transition;
    transition.set_next(2002);
    expect(!transition.can_resume(true), "ordinary walk does not resume a nonexistent transition");
    expect(transition.begin(10) && transition.waiting(), "native loading/fade keeps destination");
    expect(!transition.can_resume(true), "cannot resume before native map arrival");
    expect(!transition.map_ready(9999, 11), "unplanned map is rejected");
    expect(transition.map_ready(2002, 14) && !transition.can_resume(false), "input disabled on new floor still waits");
    expect(!transition.expired(18) && transition.can_resume(true), "slow fade resumes when native controls return");
    transition.set_next(2001);
    expect(!transition.waiting() && !transition.can_resume(true), "next leg clears old readiness");
    expect(transition.begin(20) && transition.map_ready(2001, 22) && transition.can_resume(true),
           "route can return to original floor towards retained destination");
    transition.clear();
    expect(!transition.begin(23) && !transition.map_ready(2001, 24) && !transition.can_resume(true),
           "manual or battle cancellation prevents late continuation");
    transition.set_next(2003);
    transition.begin(25);
    expect(!transition.expired(54) && transition.expired(56), "failed trigger/loading wait is bounded");
}

std::vector<char> read(const std::wstring& path)
{
    FILE* file = nullptr;
    _wfopen_s(&file, path.c_str(), L"rb");
    if (!file) return {};
    std::fseek(file, 0, SEEK_END); const auto length = std::ftell(file); std::rewind(file);
    std::vector<char> bytes(static_cast<size_t>(length));
    const auto count = std::fread(bytes.data(), 1, bytes.size(), file); std::fclose(file);
    return count == bytes.size() ? bytes : std::vector<char>{};
}

bool replay(const r::Route& path, const b::Submap& map, int facing = -1)
{
    a::Phase phase; phase.cells = path.cells; phase.arrival_direction = facing;
    a::Route follower; follower.set({phase}, map.width, map.height, map.world);
    for (size_t i = 0; i < path.cells.size(); ++i) {
        const auto step = follower.next(path.cells[i]);
        if (i + 1 == path.cells.size()) {
            if (step.result != a::Result::Arrived || step.face_direction != facing) return false;
        } else if (step.result != a::Result::Move || !(step.next == path.cells[i+1])) return false;
    }
    return !path.cells.empty();
}

// A real native map edge, followed by a route from its exact PointIn to an interactible.
// Live story state is tested by mapstate_check; these replays test geometry and handoff.
void cross_floor_route(int game)
{
    bool tested = false;
    for (int mi = 0; mi < b::submap_count() && !tested; ++mi) {
        const auto* from = b::submap_at(mi);
        if (from->world) continue;
        const auto& grid = b::grids(*from);
        for (const auto& exit : grid.objects) {
            if (exit.type != b::ObjType::GotoMap || exit.a <= 0) continue;
            const auto* dest = b::submap_by_map_id(exit.a);
            if (!dest || dest->world || dest == from) continue;
            const auto& dest_grid = b::grids(*dest);
            const b::Object* arrival = nullptr;
            for (const auto& object : dest_grid.objects)
                if (object.type == b::ObjType::PointIn && object.a == exit.b) { arrival = &object; break; }
            if (!arrival) continue;
            r::Request second; second.submap = dest; second.passages = true; second.ignore_hazards = true;
            second.start = {arrival->layer, arrival->tx, arrival->ty};
            r::Route second_path;
            int direction = -1;
            for (const auto& object : dest_grid.objects) {
                if (object.type != b::ObjType::NPC && object.type != b::ObjType::ShopNPC &&
                    object.type != b::ObjType::TreasureBox) continue;
                const auto goals = r::goals_for(second, {object.layer, object.tx, object.ty}, r::GoalKind::Interact);
                second.goals = goals.cells;
                second_path = r::find(second);
                if (!second_path.found) continue;
                direction = r::facing_direction(second_path.cells.back(), goals);
                if (direction >= 0) break;
            }
            if (!second_path.found || direction < 0) continue;
            r::Request first; first.submap = from; first.passages = true; first.ignore_hazards = true;
            for (int y = exit.ty; y < exit.ty + exit.th; ++y)
                for (int x = exit.tx; x < exit.tx + exit.tw; ++x) first.goals.push_back({exit.layer,x,y});
            r::Route first_path;
            for (const auto& object : grid.objects) {
                if (object.type != b::ObjType::PointIn) continue;
                first.start = {object.layer, object.tx, object.ty};
                first_path = r::find(first);
                if (first_path.found && first_path.cells.size() > 1) break;
            }
            if (!first_path.found || first_path.cells.size() <= 1) continue;
            a::Transition transition; transition.set_next(exit.a);
            expect(replay(first_path, *from), "real floor route reaches native transition cell");
            expect(transition.begin(1) && !transition.can_resume(false) &&
                transition.map_ready(exit.a, 3) && !transition.can_resume(false) && transition.can_resume(true),
                "real native PointIn handoff waits for controls");
            expect(replay(second_path, *dest, direction), "new floor route ends facing interactible");
            std::printf("FF%d: real floor transition and destination facing replayed\n", game);
            tested = true; break;
        }
    }
    expect(tested, "each game has a real floor transition regression");
}

int wmain()
{
    boundaries();
    transition_lifecycle();
    for (int game = 1; game <= 6; ++game) {
        auto bytes = read(L"data/ff" + std::to_wstring(game) + L"/maps.bin");
        if (bytes.empty() || !b::load(bytes.data(), bytes.size())) { expect(false, "map data load"); continue; }
        cross_floor_route(game);
        int tested = 0, no_movement = 0;
        for (int mi = 0; mi < b::submap_count(); ++mi) {
            const auto* sm = b::submap_at(mi);
            const auto& grids = b::grids(*sm);
            r::Request req; req.submap = sm; req.passages = true; req.ignore_hazards = true; req.goals_open = false;
            bool found = false;
            for (int layer = 0; layer < 3 && !found; ++layer)
                for (int y = 0; y < grids.height && !found; ++y)
                    for (int x = 0; x < grids.width && !found; ++x) {
                        r::Cell c{layer,x,y};
                        if (sm->world && layer != 1) continue;
                        if (r::open(req, grids, c)) { req.start = c; found = true; }
                    }
            if (!found) { ++no_movement; continue; }
            const auto reach = r::flood(req);
            int longest = 0; r::Cell goal = req.start;
            for (int layer = 0; layer < 3; ++layer)
                for (int y = 0; y < grids.height; ++y)
                    for (int x = 0; x < grids.width; ++x) {
                        r::Cell c{layer,x,y}; const int d = reach.distance(c);
                        if (d > longest) { longest = d; goal = c; }
                    }
            req.goals = {goal};
            const auto path = r::find(req);
            if (!path.found) { expect(false, "reachable sampled map route"); continue; }
            a::Phase phase; phase.cells = path.cells;
            a::Route follower; follower.set({phase}, grids.width, grids.height, sm->world);
            bool valid = true;
            for (size_t i = 0; i < path.cells.size(); ++i) {
                const auto step = follower.next(path.cells[i]);
                if (i + 1 == path.cells.size()) valid &= step.result == a::Result::Arrived;
                else {
                    valid &= step.result == a::Result::Move && step.next == path.cells[i+1];
                    int dx=path.cells[i+1].x-path.cells[i].x, dy=path.cells[i].y-path.cells[i+1].y;
                    if(sm->world) {
                        if(std::abs(dx)>grids.width/2) dx+=dx>0?-grids.width:grids.width;
                        if(std::abs(dy)>grids.height/2) dy+=dy>0?-grids.height:grids.height;
                    }
                    valid &= step.x == dx && step.y == dy;
                    const auto repeated = follower.next(path.cells[i]);
                    valid &= repeated.x == step.x && repeated.y == step.y;
                }
            }
            if (!valid) { std::wprintf(L"FF%d map %s: ", game, b::name_of(*sm).c_str()); expect(false, "route replay"); }
            ++tested;
        }
        std::printf("FF%d: %d sampled map routes replayed; %d maps without a walkable cell\n", game, tested, no_movement);
    }
    std::printf("Autowalk: %s (%d failures)\n", failures ? "FAIL" : "PASS", failures);
    return failures ? 1 : 0;
}
