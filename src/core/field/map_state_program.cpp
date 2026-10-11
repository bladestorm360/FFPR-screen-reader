// Read-only projection of the navigation subset of a native map initializer.
#include "map_state.h"
#include <vector>

namespace field::map_state {
namespace {
int number(const json::Value& v) { return static_cast<int>(v.n); }
using Key = std::pair<int, int>;
std::optional<int> value(int category, int index, const Flags& flags, const std::map<Key, std::optional<int>>& shadow)
{
    if (category == -1) return index;
    if (auto it = shadow.find({category, index}); it != shadow.end()) return it->second;
    if (index < 0) return {};
    if (category >= 0 && category < 8 && index < 256) {
        const size_t bit = static_cast<size_t>((category % 4) * 256 + index);
        return category < 4 ? flags.scenario[bit] : flags.treasure[bit];
    }
    if (category == 8 && index < 16) return flags.global[static_cast<size_t>(index)];
    // Locals from another floor are unknown unless that initializer assigns them.
    return {};
}
std::optional<bool> compare(int op, int a, int b)
{
    switch (op) {
    case 0: return a == b; case 1: return a != b; case 2: return a < b;
    case 3: return a > b; case 4: return a <= b; case 5: return a >= b;
    default: return {};
    }
}
}

Selection project(const json::Value& record, const Flags& flags)
{
    const auto* code = record.get(L"code");
    if (!code || code->arr.empty()) return {};
    const auto* entry = record.get(L"entry");
    struct Machine {
        int pc; std::wstring group;
        std::vector<int> stack;
        std::map<Key, std::optional<int>> shadow;
    };
    std::vector<Machine> branches{{entry ? number(*entry) : 0, {}, {}, {}}};
    std::optional<std::wstring> agreed;
    int budget = 8192;
    size_t explored = 0;
    while (!branches.empty()) {
        if (++explored > 64) return {false, {}};
        Machine state = std::move(branches.back()); branches.pop_back();
        bool finished = false;
        while (!finished) {
            if (--budget <= 0 || state.pc < 0 || static_cast<size_t>(state.pc) >= code->arr.size()) return {false, {}};
            const auto& row = code->arr[static_cast<size_t>(state.pc++)].arr;
            if (row.empty()) return {false, {}};
            auto i = [&](size_t n) { return n < row.size() ? number(row[n]) : -2; };
            switch (i(0)) {
            case 0: break;
            case 1:
                if (row.size() < 2 || !row[1].is_string()) return {false, {}};
                state.group = row[1].s; break;
            case 2: {
                if (i(3) < 0 || i(3) > 5) return {false, {}};
                const auto lhs = value(i(1), i(2), flags, state.shadow), rhs = value(i(4), i(5), flags, state.shadow);
                if (!lhs || !rhs) {
                    // An unknown local may only control animation. Accept the selection
                    // only when every possible branch ends with the same entity group.
                    branches.push_back(state);
                    state.pc = i(6);
                } else {
                    const auto test = compare(i(3), *lhs, *rhs);
                    if (!test) return {false, {}};
                    // Native reaction jumps to iValues[2] when comparison result == 1.
                    if (*test) state.pc = i(6);
                }
                break;
            }
            case 3: state.pc = i(1); break;
            case 4:
                if (state.stack.size() >= 16) return {false, {}};
                state.stack.push_back(state.pc); state.pc = i(1); break;
            case 5:
                if (state.stack.empty()) finished = true;
                else { state.pc = state.stack.back(); state.stack.pop_back(); }
                break;
            case 6: finished = true; break;
            case 7: state.shadow[{i(1), i(2)}] = i(3); break;
            case 8: state.shadow[{i(1), i(2)}] = value(i(3), i(4), flags, state.shadow); break;
            default: return {false, {}};
            }
        }
        if (agreed && *agreed != state.group) return {false, {}};
        agreed = state.group;
    }
    return {true, agreed.value_or(L"")};
}

Selection Resolver::selection(const baked::Submap& sm)
{
    const int id = baked::submap_index(&sm);
    if (auto it = cache_.find(id); it != cache_.end()) return it->second;
    const auto* record = records_.get(baked::name_of(sm));
    auto selected = record ? project(*record, flags_) : Selection{};
    cache_[id] = selected;
    return selected;
}
bool Resolver::active(const baked::Submap& sm, const baked::Object& object, bool obstacle)
{
    const auto group = baked::str(object.group);
    if (group.empty() || group == L"entity_default" || group == L"default") return true;
    const auto selected = selection(sm);
    return selected.known ? group == selected.group : obstacle;
}
bool Resolver::known(const baked::Submap& sm) { return selection(sm).known; }
bool Resolver::collected(const baked::Submap& sm, const baked::Object& object) const
{
    if (object.type != baked::ObjType::TreasureBox) return false;
    const auto* record = records_.get(baked::name_of(sm));
    const auto* chests = record ? record->get(L"treasures") : nullptr;
    if (!chests) return false;
    for (const auto& v : chests->arr) {
        const auto& r = v.arr;
        if (r.size() < 3 || number(r[0]) != object.entity_id() || r[1].s != baked::str(object.group)) continue;
        // Native MapUIManager.GetFlag uses flag_id/256 and flag_id%256 directly.
        const int bit = number(r[2]);
        return bit >= 0 && bit < 1024 && flags_.treasure[static_cast<size_t>(bit)].value_or(0) != 0;
    }
    return false;
}
void add_obstacles(Resolver& resolver, const baked::Submap& sm, baked_route::Request& request, bool gates)
{
    request.live_hazards = true;
    for (const auto& o : baked::grids(sm).objects) {
        if (!resolver.active(sm, o, true)) continue;
        const bool npc = o.type == baked::ObjType::NPC || o.type == baked::ObjType::ShopNPC;
        const bool solid = o.type == baked::ObjType::TreasureBox ||
            (npc && (o.d & 1)) ||
            (gates && ((o.type == baked::ObjType::Entity || o.type == baked::ObjType::AnimEntity) && (o.d & 1)));
        const bool collider = gates && o.type == baked::ObjType::CollisionEntity;
        const bool trigger = o.type == baked::ObjType::Event || (o.type == baked::ObjType::Warp && o.d == 1);
        if (!solid && !collider && !trigger) continue;
        for (int y = o.ty; y < o.ty + o.th; ++y) for (int x = o.tx; x < o.tx + o.tw; ++x) {
            baked_route::Cell cell{sm.world || (npc && (o.d & 2)) ? -1 : o.layer, x, y};
            if (solid) request.blocked.push_back(cell);
            if (collider) {
                if (o.a == 6 || o.a == 7) request.directional.push_back({cell, o.a == 6 ? 1 : 2});
                else request.blocked.push_back(cell);
            }
            if (trigger) request.hazards.push_back(cell);
        }
    }
}
}
