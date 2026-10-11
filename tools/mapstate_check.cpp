// check-sources: field/map_state_program.cpp field/baked_map.cpp field/baked_router.cpp json.cpp text.cpp
// Real extracted initializer/bake consistency, without loading a game or a native hook.
#include "field/map_state.h"
#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>
using namespace field;
int failures = 0;
void expect(bool pass, const char* message) { if (!pass) { ++failures; std::printf("FAIL %s\n", message); } }
std::string read(const std::string& path)
{
    std::ifstream f(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()};
}
int main()
{
    for (int n = 1; n <= 6; ++n) {
        const std::string root = "data/ff" + std::to_string(n) + "/";
        const std::string bytes = read(root + "maps.bin");
        expect(baked::load(bytes.data(), bytes.size()), "matching bake loads");
        json::Value records;
        expect(json::parse(read(root + "mapstate.json"), records), "navigation decisions parse");
        map_state::Flags flags;
        for (auto& v : flags.scenario) v = 0;
        for (auto& v : flags.treasure) v = 0;
        for (auto& v : flags.global) v = 0;
        map_state::Resolver resolver(flags, records);
        int known = 0, unknown = 0, solid = 0;
        for (int i = 0; i < baked::submap_count(); ++i) {
            const auto& sm = *baked::submap_at(i);
            resolver.known(sm) ? ++known : ++unknown;
            baked_route::Request request;
            request.submap = &sm;
            map_state::add_obstacles(resolver, sm, request);
            for (const auto& o : baked::grids(sm).objects) {
                if (!resolver.active(sm, o, true)) continue;
                if ((o.type != baked::ObjType::NPC && o.type != baked::ObjType::ShopNPC) || !(o.d & 1)) continue;
                ++solid;
                bool blocked = false;
                for (const auto& c : request.blocked) if (c.x == o.tx && c.y == o.ty) blocked = true;
                expect(blocked, "every applicable solid NPC blocks future-floor routes");
            }
        }
        if (n == 2) {
            const auto* room = baked::submap_by_name(L"Map_30051_8");
            expect(room != nullptr, "Dreadnought engine approach exists");
            if (room) {
                baked_route::Request req; req.submap = room;
                for (const auto& o : baked::grids(*room).objects) {
                    if (o.type == baked::ObjType::PointIn && o.a == 1) req.start = {o.layer,o.tx,o.ty};
                    if (o.type == baked::ObjType::GotoMap && o.a == 175) req.goals = {{o.layer,o.tx,o.ty}};
                }
                map_state::add_obstacles(resolver, *room, req);
                const auto before = baked_route::find(req);
                expect(before.found, "projected Dreadnought route uses the captain detour");
                const auto after = baked_route::find(req);
                expect(before.cells == after.cells, "a battle leaves permanent captain obstacles intact");
            }
        }
        std::printf("FF%d: %d known, %d conservative map variants; %d solid NPC checks\n", n, known, unknown, solid);
    }
    std::printf("%s: %d failures\n", failures ? "FAIL" : "PASS", failures);
    return failures ? 1 : 0;
}
