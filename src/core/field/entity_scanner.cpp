// Ports FF1 Field/EntityScanner.cs, Field/EntityDetectors/*.cs (the detector chain, in priority
// order), Field/FilterContext.cs and Core/Filters/*.cs (IEntityFilter, PathfindingFilter,
// ToLayerFilter).
//
// From the later FF2-FF5 mods: vehicle trigger pads are merged to their nearest tile when
// game::flags().has_vehicle_pads (FF2 EntityScanner.DeduplicateVehiclePads, used by FF2, FF4,
// FF5). Scenery and vehicle-hidden entities are FF1's own (FieldEntityState).
//
// EntityDetectionContext.TypeName is the entity's runtime class name. (In the C# mod it was
// the Il2CppInterop wrapper's type, usually the static type FieldEntity.)
#include <algorithm>
#include <unordered_set>
#include "field_api.h"
#include "field_internal.h"
#include "map_names.h"
#include "object_cache.h"
#include "../game.h"
#include "../hooks.h"
#include "../il2cpp.h"
#include "../log.h"
#include "../modtext.h"
#include "../settings.h"
#include "../text.h"

namespace field {

// =========================================================================================
// FilterContext
// =========================================================================================
FilterContext::FilterContext()
{
    player_controller = cache::get(r::kFieldPlayerController);
    if (!player_controller) player_controller = cache::refresh(r::kFieldPlayerController);
    if (!player_controller) return;
    field_player = ref::rd<void*>(player_controller, r::fFieldPlayer);
    map_handle = ref::rd<void*>(player_controller, r::fMapHandle);
    void* tr = field_player ? unity::transform(field_player) : nullptr;
    if (tr) player_position = unity::local_position(tr);   // localPosition for pathfinding
}

namespace {

using Vec3 = unity::Vec3;
constexpr int kCanoe = 5;   // FF1Constants.TransportTypes.CANOE (the Content slot)
constexpr int kObjTransportationEventAction = 21;

bool contains(const std::wstring& s, const wchar_t* part) { return s.find(part) != std::wstring::npos; }

// =========================================================================================
// Filters (Core/Filters)
// =========================================================================================

/// PathfindingFilter: entities with a valid path from the player (OnCycle). Its IsEnabled is
/// the "PathfindingFilter" setting.
bool pathfinding_enabled() { return settings::get_bool("PathfindingFilter", false); }

bool pathfinding_passes(const EntityPtr& e, const FilterContext& ctx)
{
    if (!e || !e->is_alive()) return false;
    // A vehicle-only trigger hidden for the current vehicle is not a walking target: it is
    // reached by boarding its vehicle, so the walking path test does not apply.
    if (e->game && entity_is_hidden_by_vehicle(e->game)) return true;
    if (!ctx.player_controller) return false;
    PathInfo p = find_path_to(ctx.player_position, e->position(), ctx.map_handle, ctx.field_player);
    return p.success;
}

/// ToLayerFilter: hides layer transitions (OnAdd). Its IsEnabled is the "ToLayerFilter" setting.
bool to_layer_enabled() { return settings::get_bool("ToLayerFilter", false); }

bool to_layer_passes(const EntityPtr& e) { return !(e->kind == Kind::Event && e->event_type == L"ToLayer"); }

// =========================================================================================
// Detection (EntityDetectors)
// =========================================================================================

/// EntityDetectionContext: shared data for one detection pass; the property and its object
/// type are read lazily.
struct Context {
    void* fe = nullptr;
    Vec3 position{};
    std::wstring type_name;
    std::wstring go_name;
    std::wstring go_lower;
    bool is_active = true;
    bool is_event_trigger = false;

    void* property()
    {
        if (!property_resolved_) { property_ = entity_property(fe); property_resolved_ = true; }
        return property_;
    }
    int object_type()
    {
        if (object_type_ == -2) object_type_ = object_type_of(property());
        return object_type_;
    }

private:
    void* property_ = nullptr;
    bool property_resolved_ = false;
    int object_type_ = -2;
};

enum class Result { NotHandled, Skip, Detected };

struct Detection {
    Result result = Result::NotHandled;
    EntityPtr entity;
};

Detection skip() { return {Result::Skip, nullptr}; }
Detection not_handled() { return {}; }
Detection detected(EntityPtr e) { return {Result::Detected, std::move(e)}; }

EntityPtr make(Kind kind, const Context& c, const std::wstring& name)
{
    auto e = std::make_shared<Entity>();
    e->kind = kind;
    e->game = c.fe;
    e->game_class = il2cpp::object_class(c.fe);
    e->pos = c.position;
    e->name = name;
    return e;
}

EntityPtr make_event(const Context& c, const std::wstring& name, const std::wstring& type)
{
    EntityPtr e = make(Kind::Event, c, name);
    e->event_type = type;
    return e;
}

EntityPtr make_vehicle(const Context& c, const std::wstring& name, int type, const std::wstring& message_id = L"")
{
    EntityPtr e = make(Kind::Vehicle, c, name);
    e->transport_id = type;
    e->message_id = message_id;
    return e;
}

EntityPtr make_exit(const Context& c, const std::wstring& name, int dest_id, const std::wstring& dest_name)
{
    EntityPtr e = make(Kind::MapExit, c, name);
    e->dest_map_id = dest_id;
    e->dest_name = dest_name;
    return e;
}

/// PlayerFilter (0): the player is never a target.
Detection player_filter(Context& c)
{
    if (contains(c.type_name, L"FieldPlayer") || contains(c.go_lower, L"player")) return skip();
    return not_handled();
}

/// VehicleDetector (10): vehicles from Transportation.ModelList (VehicleTypeMap). Before the
/// visual-effect filter, since vehicles use ResidentCharaEntity game objects.
Detection vehicle_detector(Context& c)
{
    int type = 0;
    std::wstring message_id;
    if (!vehicle_type_of(c.fe, type, message_id)) return not_handled();
    // The canoe is an auto-used key item (summoned at canals), not a walk-to vehicle.
    if (type == kCanoe) return skip();
    std::wstring name = vehicle_name(type, message_id);
    if (name.empty()) name = T(L"Vehicle");   // FF2's table has no name for this type
    return detected(make_vehicle(c, name, type, message_id));
}

/// VisualEffectFilter (20): party members, visual effects and inactive objects.
Detection visual_effect_filter(Context& c)
{
    const std::wstring& lower = c.go_lower;
    if (contains(lower, L"residentchara") || contains(lower, L"resident")) return skip();
    if (!c.is_event_trigger) {
        if (contains(lower, L"fieldeffect") || contains(lower, L"scrolldummy") || contains(lower, L"scroll") ||
            contains(lower, L"tileanim") || contains(lower, L"pointin") || contains(lower, L"opentrigger") ||
            (contains(lower, L"effect") && !contains(lower, L"object")))
            return skip();
    }
    if (!c.is_active) return skip();
    return not_handled();
}

EntityPtr create_map_exit(Context& c)
{
    int dest = goto_map_destination_id(c.fe);
    std::wstring dest_name = resolve_map_name(dest);
    std::wstring exit_name = !dest_name.empty() ? TF(L"Exit to {0}", {dest_name}) : T(L"Exit");
    return make_exit(c, exit_name, dest, dest_name);
}

/// GotoMapDetector (50): map exits (GotoMapEventEntity, ObjectType 3) and same-map warp tiles
/// (PropertyTelepoPoint).
Detection goto_map_detector(Context& c)
{
    if (r::kGotoMapEventEntity.is(c.fe)) return detected(create_map_exit(c));
    if (contains(c.go_lower, L"gotomap") || c.object_type() == 3) return detected(create_map_exit(c));

    void* prop = c.property();
    if (prop && r::kPropertyTelepoPoint.is(prop)) {
        std::wstring name = entity_name_from_property(c.fe);
        if (name.empty()) name = T(L"Warp Tile");
        return detected(make_event(c, name, L"Warp Tile"));
    }
    return not_handled();
}

/// TreasureChestDetector (60): FieldTresureBox (the game's spelling), then names.
Detection treasure_detector(Context& c)
{
    bool hit = r::kFieldTresureBox.is(c.fe);
    if (!hit) {
        const std::wstring& lower = c.go_lower;
        hit = contains(lower, L"treasure") || contains(lower, L"tresure") || contains(lower, L"chest") ||
              contains(c.type_name, L"Treasure") || contains(c.type_name, L"Tresure");
    }
    if (!hit) return not_handled();
    bool opened = treasure_opened(c.fe);
    std::wstring contents = treasure_contents(c.property());
    EntityPtr e = make(Kind::TreasureChest, c, !contents.empty() ? contents : T(L"Treasure Chest"));
    e->opened = opened;
    return detected(e);
}

/// NPCDetector (70): FieldNonPlayer; one that cannot be talked to (CanAction false) is skipped.
Detection npc_detector(Context& c)
{
    if (!r::kFieldNonPlayer.is(c.fe)) return not_handled();
    if (r::mNonPlayerGetCanAction.get() && !ref::call<bool>(r::mNonPlayerGetCanAction, c.fe)) return skip();

    std::wstring name = entity_name_from_property(c.fe);
    if (name.empty() || name == L"NPC") name = clean_object_name(c.go_name, T(L"NPC"));
    EntityPtr e = make(Kind::NPC, c, name);
    e->shop = contains(c.go_lower, L"shop") || contains(c.go_lower, L"merchant");
    return detected(e);
}

/// SavePointDetector (80).
Detection save_point_detector(Context& c)
{
    if (contains(c.go_lower, L"save") || contains(c.type_name, L"Save"))
        return detected(make(Kind::SavePoint, c, T(L"Save Point")));
    return not_handled();
}

Detection try_create_vehicle(Context& c)
{
    std::wstring name = vehicle_name_from_property(c.go_name, c.type_name);
    int type = vehicle_type_from_name(name);
    // Unknown types (0) and the canoe (an auto-used key item summoned out of bounds) are skipped.
    if (type == 0 || type == kCanoe) return skip();
    return detected(make_vehicle(c, name, type));
}

/// TransportDetector (90): PropertyTransportation, then names.
Detection transport_detector(Context& c)
{
    void* prop = entity_property(c.fe);
    if (prop && r::kPropertyTransportation.is(prop)) return try_create_vehicle(c);
    if (contains(c.type_name, L"Transport") || contains(c.go_lower, L"ship") || contains(c.go_lower, L"canoe") ||
        contains(c.go_lower, L"airship"))
        return try_create_vehicle(c);
    return not_handled();
}

/// DoorStairsDetector (100): doors, stairs, ladders and entrances as map exits.
Detection door_stairs_detector(Context& c)
{
    const std::wstring& lower = c.go_lower;
    if (contains(lower, L"door") || contains(lower, L"stairs") || contains(lower, L"ladder") || contains(lower, L"entrance")) {
        int dest = goto_map_destination_id(c.fe);
        std::wstring dest_name = resolve_map_name(dest);
        return detected(make_exit(c, T(L"Exit"), dest, dest_name));
    }
    return not_handled();
}

/// ToLayerDetector (110): SwitchLayerEventEntity / ObjectType 4 / toupper, tobottom, tolayer*.
Detection to_layer_detector(Context& c)
{
    bool layer = r::kSwitchLayerEventEntity.is(c.fe);
    if (!layer && c.object_type() == 4) layer = true;   // MapConstants.ObjectType.ToLayer
    if (!layer) {
        const std::wstring& lower = c.go_lower;
        if (lower == L"toupper" || lower == L"tobottom" || text::starts_with(lower, L"tolayer")) layer = true;
    }
    if (!layer) return not_handled();
    std::wstring name = entity_name_from_property(c.fe);
    if (name.empty()) name = c.go_name;
    return detected(make_event(c, name, L"ToLayer"));
}

/// EventTriggerDetector (120): every other EventTriggerEntity, scenery excepted.
Detection event_trigger_detector(Context& c)
{
    if (!c.is_event_trigger) return not_handled();
    if (entity_is_scenery(c.fe)) return skip();
    std::wstring name = entity_name_from_property(c.fe);
    if (name.empty() || name == L"GeneralEventObject") {
        name = classify_by_dialogue(c.fe);
        if (name.empty()) name = clean_object_name(c.go_name, T(L"Event Trigger"));
    }
    return detected(make_event(c, name, L"Event"));
}

/// InteractiveEntityDetector (130): FieldMapObjectDefault and any IInteractiveEntity left.
Detection interactive_detector(Context& c)
{
    if (entity_is_scenery(c.fe)) return skip();

    if (r::kFieldMapObjectDefault.is(c.fe)) {
        if (!call_virtual_bool(r::mMapObjectGetCanAction, c.fe)) return skip();
        std::wstring name = entity_name_from_property(c.fe);
        if (name.empty() || name == L"GeneralEventObject") {
            name = classify_by_dialogue(c.fe);
            if (name.empty()) name = interactive_object_name(c.property(), c.go_name);
        }
        return detected(make_event(c, name, L"Interactive Object"));
    }

    if (implements(c.fe, r::kIInteractiveEntity)) {
        // ObjectType 0 + PointIn = entry points (not useful to navigate to)
        if (c.object_type() == 0 && contains(c.go_lower, L"pointin")) return skip();
        std::wstring name = entity_name_from_property(c.fe);
        if (name.empty()) name = clean_object_name(c.go_name, T(L"Object"));
        return detected(make_event(c, name, L"Interactive"));
    }
    return not_handled();
}

using Detector = Detection (*)(Context&);
// Sorted by priority: 0, 10, 20, 50, 60, 70, 80, 90, 100, 110, 120, 130.
const Detector kDetectors[] = {
    &player_filter, &vehicle_detector, &visual_effect_filter, &goto_map_detector, &treasure_detector,
    &npc_detector, &save_point_detector, &transport_detector, &door_stairs_detector, &to_layer_detector,
    &event_trigger_detector, &interactive_detector,
};

/// ConvertToNavigableEntity: the first detector that skips or detects decides.
EntityPtr convert(void* fe)
{
    if (!live_entity(fe)) return nullptr;
    Context c;
    c.fe = fe;
    void* tr = unity::transform(fe);
    if (!tr) return nullptr;
    c.position = unity::local_position(tr);
    c.type_name = text::widen(il2cpp::object_class_name(fe));
    c.go_name = unity::name(fe);
    c.go_lower = text::lower(c.go_name);
    // Present in the scene, or hidden by the game only while the player is in another vehicle
    c.is_active = unity::active_in_hierarchy(fe) || entity_is_hidden_by_vehicle(fe);
    c.is_event_trigger = r::kEventTriggerEntity.is(fe);

    for (Detector d : kDetectors) {
        Detection det = d(c);
        if (det.result == Result::Skip) return nullptr;
        if (det.result == Result::Detected) return det.entity;
    }
    return nullptr;
}

// =========================================================================================
// EntityScanner state
// =========================================================================================

std::vector<std::pair<void*, EntityPtr>> g_entity_map;   // FieldEntity -> conversion, in insertion order
std::vector<EntityPtr> g_entities;
std::vector<EntityPtr> g_filtered;
int g_index = 0;
Category g_category = Category::All;
int g_last_scanned_map_id = -1;

// Selected entity identity, to keep focus across re-sorts
EntityPtr g_sel;
bool g_sel_has = false;
Vec3 g_sel_pos{};
Category g_sel_category = Category::All;
std::wstring g_sel_name;

bool player_local_position(Vec3& out)
{
    // GetPlayerPosition: the cached controller only (no refresh), as FF1.
    void* pc = cache::get(r::kFieldPlayerController);
    void* fp = pc ? ref::rd<void*>(pc, r::fFieldPlayer) : nullptr;
    void* tr = fp ? unity::transform(fp) : nullptr;
    if (!tr) return false;
    out = unity::local_position(tr);
    return true;
}

/// FF2 EntityScanner.VehiclePadName: the developer name of a vehicle trigger pad tile.
std::wstring vehicle_pad_name(const EntityPtr& e)
{
    if (e->kind != Kind::Event || !live_entity(e->game)) return L"";
    void* prop = entity_property(e->game);
    if (!prop || ref::rd<int32_t>(prop, r::fPropObjectType) != kObjTransportationEventAction) return L"";
    return ref::rd_str(prop, r::fPropName);
}

/// FF2 EntityScanner.DeduplicateVehiclePads: the nearest tile of each pad (list sorted by
/// distance). A TransportationEventAction covers a pad of tiles, each its own map object with
/// the same name; in the map data each such name is one place per map.
void dedupe_vehicle_pads(std::vector<EntityPtr>& list)
{
    std::vector<EntityPtr> out;
    out.reserve(list.size());
    std::unordered_set<std::wstring> seen;
    for (auto& e : list) {
        std::wstring pad = vehicle_pad_name(e);
        if (!pad.empty() && !seen.insert(pad).second) continue;
        out.push_back(e);
    }
    list.swap(out);
}

/// DeduplicateMapExits: the closest exit per destination (list sorted by distance); exits with
/// an unresolved destination (<= 0) are kept individually.
void dedupe_map_exits(std::vector<EntityPtr>& list)
{
    std::vector<EntityPtr> out;
    out.reserve(list.size());
    std::unordered_set<int> seen;
    for (auto& e : list) {
        if (e->kind == Kind::MapExit && e->dest_map_id > 0 && !seen.insert(e->dest_map_id).second) continue;
        out.push_back(e);
    }
    list.swap(out);
}

int find_entity_by_identifier()
{
    // Reference match: the same entity object persists across scans, so the cursor stays on
    // the selected entity even when it moves. Position/name recover a replaced object.
    if (g_sel) {
        for (size_t i = 0; i < g_filtered.size(); ++i)
            if (g_filtered[i] == g_sel) return static_cast<int>(i);
    }
    if (!g_sel_has) return -1;
    for (size_t i = 0; i < g_filtered.size(); ++i) {
        const auto& e = g_filtered[i];
        if (e->category() == g_sel_category && vec_distance(e->position(), g_sel_pos) < 0.5f)
            return static_cast<int>(i);
    }
    if (!g_sel_name.empty()) {
        for (size_t i = 0; i < g_filtered.size(); ++i) {
            const auto& e = g_filtered[i];
            if (e->category() == g_sel_category && e->name == g_sel_name) return static_cast<int>(i);
        }
    }
    return -1;
}

/// ApplyFilter: category, distance sort, (vehicle pads), map exits, layer transitions, then
/// the selection restored.
void apply_filter()
{
    if (g_category == Category::All) {
        g_filtered = g_entities;
    } else {
        g_filtered.clear();
        for (auto& e : g_entities)
            if (e->category() == g_category) g_filtered.push_back(e);
    }

    Vec3 player_pos{};
    if (player_local_position(player_pos)) {
        std::vector<std::pair<float, EntityPtr>> keyed;
        keyed.reserve(g_filtered.size());
        for (auto& e : g_filtered) keyed.emplace_back(vec_distance(e->position(), player_pos), e);
        std::stable_sort(keyed.begin(), keyed.end(),
                         [](const auto& a, const auto& b) { return a.first < b.first; });
        for (size_t i = 0; i < keyed.size(); ++i) g_filtered[i] = keyed[i].second;
    }

    // One entry per vehicle trigger pad (FF2, FF4, FF5; user 2026-10-03)
    if (game::flags().has_vehicle_pads) dedupe_vehicle_pads(g_filtered);

    if (settings::get_bool("MapExitFilter", false)) dedupe_map_exits(g_filtered);

    if (to_layer_enabled()) {
        std::vector<EntityPtr> kept;
        for (auto& e : g_filtered)
            if (to_layer_passes(e)) kept.push_back(e);
        g_filtered.swap(kept);
    }

    int restored = find_entity_by_identifier();
    if (restored >= 0) g_index = restored;
}

void clear_selected_identifier()
{
    g_sel.reset();
    g_sel_has = false;
    g_sel_name.clear();
}

void save_selected_identifier()
{
    EntityPtr e = scanner::current_entity();
    if (!e) return;
    g_sel = e;
    g_sel_has = true;
    g_sel_pos = e->position();
    g_sel_category = e->category();
    g_sel_name = e->name;
}

void ensure_correct_map()
{
    int id = current_map_id_or_user_data();
    if (id > 0 && id != g_last_scanned_map_id) scanner::force_rescan();
}

} // namespace

// =========================================================================================
// scanner API
// =========================================================================================
namespace scanner {

void scan_entities()
{
    hooks::guarded("EntityScanner.ScanEntities", [&] {
        int map_id = current_map_id_or_user_data();
        std::wstring asset = current_map_asset_name();

        std::vector<void*> field_entities = all_field_entities();
        std::unordered_set<void*> current(field_entities.begin(), field_entities.end());

        // Entities no longer in the game's lists, then ones deactivated/destroyed in the scene
        g_entity_map.erase(std::remove_if(g_entity_map.begin(), g_entity_map.end(),
                                          [&](const auto& kv) { return !current.count(kv.first); }),
                           g_entity_map.end());
        g_entity_map.erase(std::remove_if(g_entity_map.begin(), g_entity_map.end(),
                                          [](const auto& kv) { return !kv.second->is_alive(); }),
                           g_entity_map.end());

        std::unordered_set<void*> known;
        for (auto& kv : g_entity_map) known.insert(kv.first);

        // Only NEW entities are converted
        for (void* fe : field_entities) {
            if (!asset.empty() && !entity_on_current_map(fe, asset)) continue;
            if (known.count(fe)) continue;
            EntityPtr e = hooks::guarded_r("EntityScanner.Convert", EntityPtr(), [&] { return convert(fe); });
            if (e) {
                g_entity_map.emplace_back(fe, e);
                known.insert(fe);
            }
        }

        g_entities.clear();
        for (auto& kv : g_entity_map) g_entities.push_back(kv.second);
        g_last_scanned_map_id = map_id;
        apply_filter();

        if (g_index >= static_cast<int>(g_filtered.size())) g_index = 0;
    });
}

void force_rescan()
{
    g_entity_map.clear();
    scan_entities();
}

void next_entity()
{
    ensure_correct_map();
    if (g_filtered.empty()) {
        scan_entities();
        if (g_filtered.empty()) return;
    }
    int n = static_cast<int>(g_filtered.size());
    if (!pathfinding_enabled()) {
        g_index = (g_index + 1) % n;
        save_selected_identifier();
        return;
    }
    FilterContext ctx;
    int start = g_index;
    for (int attempts = 0; attempts < n; ++attempts) {
        g_index = (g_index + 1) % n;
        if (pathfinding_passes(g_filtered[static_cast<size_t>(g_index)], ctx)) {
            save_selected_identifier();
            return;
        }
    }
    g_index = start;
}

void previous_entity()
{
    ensure_correct_map();
    if (g_filtered.empty()) {
        scan_entities();
        if (g_filtered.empty()) return;
    }
    int n = static_cast<int>(g_filtered.size());
    if (!pathfinding_enabled()) {
        g_index = (g_index - 1 + n) % n;
        save_selected_identifier();
        return;
    }
    FilterContext ctx;
    int start = g_index;
    for (int attempts = 0; attempts < n; ++attempts) {
        g_index = (g_index - 1 + n) % n;
        if (pathfinding_passes(g_filtered[static_cast<size_t>(g_index)], ctx)) {
            save_selected_identifier();
            return;
        }
    }
    g_index = start;
}

bool no_reachable_entities()
{
    if (!pathfinding_enabled() || g_filtered.empty()) return false;
    FilterContext ctx;
    for (auto& e : g_filtered)
        if (pathfinding_passes(e, ctx)) return false;
    return true;
}

bool select_first_reachable()
{
    if (g_filtered.empty()) return false;
    if (!pathfinding_enabled()) return true;
    FilterContext ctx;
    int n = static_cast<int>(g_filtered.size());
    for (int i = 0; i < n; ++i) {
        int idx = (g_index + i) % n;
        if (pathfinding_passes(g_filtered[static_cast<size_t>(idx)], ctx)) {
            g_index = idx;
            save_selected_identifier();
            return true;
        }
    }
    return false;
}

void reapply_filter()
{
    apply_filter();
    if (g_index >= static_cast<int>(g_filtered.size())) g_index = 0;
}

void set_filter_to_layer(bool)
{
    // FilterToLayer's setter: the filter state is the setting itself; re-apply at once.
    apply_filter();
}

void set_category(Category c)
{
    if (g_category == c) return;
    g_category = c;
    clear_selected_identifier();
    apply_filter();
    g_index = 0;
}

Category category() { return g_category; }

EntityPtr current_entity()
{
    ensure_correct_map();
    if (g_filtered.empty() || g_index < 0 || g_index >= static_cast<int>(g_filtered.size())) return nullptr;
    return g_filtered[static_cast<size_t>(g_index)];
}

int current_index() { return g_index; }
int count() { return static_cast<int>(g_filtered.size()); }

std::vector<unity::Vec3> map_exit_positions()
{
    // GetMapExitPositions: every MapExitEntity of the unfiltered list (wall-tone suppression)
    std::vector<unity::Vec3> out;
    for (auto& e : g_entities)
        if (e->kind == Kind::MapExit) out.push_back(e->pos);
    return out;
}

} // namespace scanner

std::vector<unity::Vec3> map_exit_positions() { return scanner::map_exit_positions(); }

} // namespace field
