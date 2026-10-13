// check-sources: field/map_state_program.cpp field/baked_map.cpp field/baked_router.cpp json.cpp text.cpp
// Real extracted initializer/bake consistency, without loading a game or a native hook.
#include "field/map_state.h"
#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>
#include <chrono>
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
        const auto decoding_started = std::chrono::steady_clock::now();
        std::vector<const baked::Grids*> decoded;
        for (int i = 0; i < baked::submap_count(); ++i) decoded.push_back(&baked::grids(*baked::submap_at(i)));
        for (int i = 0; i < baked::submap_count(); ++i)
            expect(decoded[static_cast<size_t>(i)] == &baked::grids(*baked::submap_at(i)),
                   "prepared grids retain immutable cache identities");
        const double decode_ms = std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - decoding_started).count();
        json::Value records;
        expect(json::parse(read(root + "mapstate.json"), records), "navigation decisions parse");
        map_state::Flags flags;
        for (auto& v : flags.scenario) v = 0;
        for (auto& v : flags.treasure) v = 0;
        for (auto& v : flags.global) v = 0;
        map_state::Resolver resolver(flags, records);
        int known = 0, unknown = 0, solid = 0, known_chests = 0, unknown_chests = 0;
        for (int i = 0; i < baked::submap_count(); ++i) {
            const auto& sm = *baked::submap_at(i);
            resolver.known(sm) ? ++known : ++unknown;
            baked_route::Request request;
            request.submap = &sm;
            map_state::add_obstacles(resolver, sm, request);
            for (const auto& o : baked::grids(sm).objects) {
                if (o.type == baked::ObjType::TreasureBox) {
                    const auto opened = resolver.treasure_opened(sm, o);
                    if (opened) {
                        ++known_chests;
                        expect(!*opened, "zero saved flag means unopened chest");
                        const auto* record = records.get(baked::name_of(sm));
                        const auto* chests = record ? record->get(L"treasures") : nullptr;
                        bool matched = false;
                        if (chests) for (const auto& row : chests->arr) {
                            const auto& r = row.arr;
                            if (r.size() < 3 || static_cast<int>(r[0].n) != o.entity_id() || r[1].s != baked::str(o.group)) continue;
                            const int bit = static_cast<int>(r[2].n);
                            if (bit < 0 || bit >= 1024) continue;
                            auto changed = flags; changed.treasure[static_cast<size_t>(bit)] = 1;
                            map_state::Resolver collected(changed, records);
                            expect(collected.treasure_opened(sm, o) == std::optional<bool>(true),
                                   "matching copied saved bit means opened chest on any floor");
                            changed.treasure[static_cast<size_t>(bit)].reset();
                            map_state::Resolver missing(changed, records);
                            expect(!missing.treasure_opened(sm, o).has_value(),
                                   "unavailable saved bit remains unknown rather than unopened");
                            matched = true; break;
                        }
                        expect(matched, "known chest state is backed by matching entity and story-group flag record");
                    } else ++unknown_chests;
                }
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
        expect(known_chests > 0, "each game has verified saved chest flags");
        std::printf("FF%d: %d saved chest mappings verified; %d unmapped chests remain unknown\n", n, known_chests, unknown_chests);
        std::printf("FF%d: %d known, %d conservative maps; %d solid NPC checks; all grids decoded %.1f ms offline\n",
                    n, known, unknown, solid, decode_ms);
    }
    std::printf("%s: %d failures\n", failures ? "FAIL" : "PASS", failures);
    return failures ? 1 : 0;
}
