// check-sources: field/baked_map.cpp field/baked_router.cpp
// All real world bakes, representative far routes and floods. No game runtime.
#include "field/baked_map.h"
#include "field/baked_router.h"
#include <cstdio>
#include <fstream>
#include <chrono>
#include <thread>
int main() {
    using namespace field;
    int failures = 0, worlds = 0, routes = 0;
    double worst_flood = 0, worst_route = 0;
    for (int game = 1; game <= 6; ++game) {
        std::ifstream stream("data/ff" + std::to_string(game) + "/maps.bin", std::ios::binary);
        std::string bytes((std::istreambuf_iterator<char>(stream)), {});
        if (!baked::load(bytes.data(), bytes.size())) { ++failures; continue; }
        for (int i = 0; i < baked::submap_count(); ++i) {
            const auto* sm = baked::submap_at(i); if (!sm->world) continue;
            ++worlds;
            const auto& grid = baked::grids(*sm);
            baked_route::Request request; request.submap = sm; request.ignore_hazards = true;
            // Native world groups vary. Foot plus the game's automatic traversal
            // groups supplies representative large connected land regions.
            request.auto_groups = {2};
            bool start = false;
            for (int y = 0; y < grid.height && !start; ++y) for (int x = 0; x < grid.width; ++x)
                if (baked::vehicle_open(grid, 1, x, y)) { request.start = {1, x, y}; start = true; break; }
            if (!start) continue;
            std::thread worker([&] {
                auto at = std::chrono::steady_clock::now();
                auto reach = baked_route::flood(request);
                const auto flood_ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - at).count();
                worst_flood = std::max(worst_flood, flood_ms);
                int far = -1; baked_route::Cell target = request.start;
                for (int y = 0; y < grid.height; ++y) for (int x = 0; x < grid.width; ++x) {
                    const int distance = reach.distance({1, x, y});
                    if (distance > far) { far = distance; target = {1, x, y}; }
                }
                request.goals = {target};
                at = std::chrono::steady_clock::now();
                auto route = baked_route::find(request);
                const auto route_ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - at).count();
                worst_route = std::max(worst_route, route_ms);
                if (!route.found || reach.distance(target) < 0 || route.cells.front() != request.start || route.cells.back() != target) {
                    ++failures;
                    std::printf("FAIL FF%d world %d %dx%d: far %d, route found %d, cells %zu\n",
                        game, sm->first_map_id, grid.width, grid.height, far, route.found, route.cells.size());
                }
                else ++routes;
            }); worker.join();
        }
    }
    std::printf("Six-game world benchmark: %d worlds, %d far routes; worst flood %.3f ms, route %.3f ms; %d failures (offline)\n",
        worlds, routes, worst_flood, worst_route, failures);
    return failures || !routes ? 1 : 0;
}
