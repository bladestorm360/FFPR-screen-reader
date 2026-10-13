// check-sources: field/baked_map.cpp field/baked_router.cpp
#include "../src/core/field/baked_router.h"
#include <cstdio>
#include <fstream>
#include <string>
#include <algorithm>
namespace b = field::baked;
namespace r = field::baked_route;
int main() {
    int failures=0, south_figaro=0;
    for (int game = 1; game <= 6; ++game) {
        std::ifstream file("data/ff" + std::to_string(game) + "/maps.bin", std::ios::binary);
        std::string bytes((std::istreambuf_iterator<char>(file)), {});
        if (!b::load(bytes.data(), bytes.size())) return 1;
        int total = 0, unreachable = 0;
        for (int i = 0; i < b::submap_count(); ++i) {
            const auto* sm = b::submap_at(i);
            const auto& g = b::grids(*sm);
            if (sm->world) {
                r::Request movement;movement.submap=sm;
                for(const auto& transport:b::transports()) if(transport.type==5) movement.auto_groups.push_back(transport.group);
                int candidates=0,enterable=0;
                for(const auto& object:g.objects) {
                    if(object.type!=b::ObjType::GotoMap) continue;
                    ++candidates;
                    auto request=movement;
                    if(!r::transition_enterable(request,g,object.tx,object.ty)) continue;
                    request.goals={{1,object.tx,object.ty}};
                    bool found=false;
                    for(const auto& d:{r::Cell{0,0,-1},r::Cell{0,1,0},r::Cell{0,0,1},r::Cell{0,-1,0}}) {
                        request.start={1,(object.tx+d.x+g.width)%g.width,(object.ty+d.y+g.height)%g.height};
                        if(!r::open(request,g,request.start)) continue;
                        const auto path=r::find(request);
                        if(path.found&&path.cells.back()==request.goals[0]) { found=true;break; }
                    }
                    if(found) ++enterable;
                }
                if(candidates) std::printf("ff%d %ls: %d world trigger candidates, %d have a legal walking/owned-content approach; remaining candidates stay subject to reachability/state pruning\n",game,b::name_of(*sm).c_str(),candidates,enterable);
                continue;
            }
            r::Request req; req.submap = sm; req.passages = true; req.ignore_hazards = true;
            std::vector<r::Cell> starts;
            for (const auto& o : g.objects) if (o.type == b::ObjType::PointIn)
                starts.push_back({o.layer, o.tx, o.ty});
            const auto reach = r::flood(req, starts);
            for (const auto& o : g.objects) {
                if (o.type != b::ObjType::GotoMap) continue;
                ++total;
                const auto goals = r::goals_for(req, {o.layer,o.tx,o.ty}, r::GoalKind::Transition);
                req.goals=goals.cells;
                const bool found = r::transition_distance(req,reach)>=0;
                const auto name=b::name_of(*sm);
                const bool south=game==6 && (name==L"Map_20070" || name==L"Map_20360" ||
                    name.starts_with(L"Map_20071_") || name.starts_with(L"Map_20361_"));
                if (south) { ++south_figaro; if (!found) ++failures; }
                if (game==5 && name==L"Map_20050" && !found) ++failures;
                if (!found) {
                    ++unreachable;
                    std::printf("UNREACHABLE ff%d %ls layer%d (%d,%d) ->%d starts%zu\n",
                        game,b::name_of(*sm).c_str(),o.layer,o.tx,o.ty,o.a,starts.size());
                }
            }
        }
        std::printf("ff%d: %d floor exits, %d without an arrival-connected approach (ungated static audit)\n",game,total,unreachable);
        if (game==5) {
            const auto* castle=b::submap_by_name(L"Map_20050");
            if (!castle || castle->width!=64 || castle->height!=64 || castle->layer_mask!=3) ++failures;
        }
        if (game==6) {
            const auto* room=b::submap_by_name(L"Map_20071_11");
            if (!room || room->width!=27 || room->height!=20) ++failures;
        }
    }
    if (south_figaro<100) ++failures;
    std::printf("South Figaro: %d floor exits checked; native room bounds, stair and Karnak regressions: %d failures\n",south_figaro,failures);
    return failures ? 1 : 0;
}
