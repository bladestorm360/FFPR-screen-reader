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
#include <atomic>
#include <map>
#include <unordered_set>
#include "entity_translator.h"
#include "event_labels.h"
#include "field_api.h"
#include "field_internal.h"
#include "map_names.h"
#include "object_cache.h"
#include "path_search.h"
#include "baked_nav.h"
#include "../game.h"
#include "../background.h"
#include "../frame.h"
#include "../system/system.h"
#include "map_state.h"
#include "../hooks.h"
#include "../il2cpp.h"
#include "../log.h"
#include "../modtext.h"
#include "../settings.h"
#include "../text.h"
#include "../ui/preferences.h"

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

/// PathfindingFilter (OnCycle), three ways since 2026-10-11 (user): "Current floor" lists only
/// what walking reaches on this sub-map; "Current map with vehicles" adds what a vehicle the
/// party has here reaches; "Current map" lists everything, and a target cut off on this floor
/// is routed through another floor. The old on/off setting maps to floor / map.
int pathfinding_mode()
{
    int mode = settings::get_int(ui::prefs::kPathfindingFilterMode, -1);
    if (mode < 0) mode = settings::get_bool("PathfindingFilter", false) ? ui::prefs::kFilterCurrentFloor : ui::prefs::kFilterCurrentMap;
    return std::clamp(mode, 0, ui::prefs::kPathfindingFilterModeMax);
}
bool pathfinding_enabled() { return pathfinding_mode() != ui::prefs::kFilterCurrentMap; }

bool pathfinding_passes(const EntityPtr& e, const FilterContext& ctx)
{
    if (!e || !e->is_alive()) return false;
    // Another floor's entry exists only under "Current map", which lists everything.
    if (e->remote()) return pathfinding_mode() == ui::prefs::kFilterCurrentMap;
    // A vehicle-only trigger hidden for the current vehicle is not a walking target: it is
    // reached by boarding its vehicle, so the walking path test does not apply.
    if (e->game && entity_is_hidden_by_vehicle(e->game)) return true;
    if (!ctx.player_controller) return false;
    // FF5 PathSearchMode.Quick: the filter runs once per entity and never pays for breadcrumb
    // chaining, so a target past the game's search window can be filtered out yet still be
    // routable with the announce key.
    PathInfo p = find_path_to_quick(ctx.player_position, e->position(), ctx.map_handle, ctx.field_player,
                                    TargetKind::Infer, pathfinding_mode() == ui::prefs::kFilterCurrentMapWithVehicles);
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
    e->converted_name = name;
    return e;
}

/// The game module's placeholder list (game::Flags::placeholder_entity), asked with the raw
/// property name, or the object name when the property has none or says "NPC": FF2's
/// IsPlaceholderEntity, applied where its EntityScanner applied it (NPCs, event triggers and
/// interactive objects). No list in the other games.
bool placeholder(Context& c)
{
    bool (*fn)(const wchar_t*) = game::flags().placeholder_entity;
    if (!fn) return false;
    std::wstring raw = c.property() ? ref::rd_str(c.property(), r::fPropName) : std::wstring();
    if (text::is_blank(raw) || raw == L"NPC") raw = c.go_name;
    return fn(raw.c_str());
}

EntityPtr make_event(const Context& c, const std::wstring& type)
{
    EntityPtr e = make(Kind::Event, c, safe_event_label(c.fe, type));
    e->event_type = type;
    return e;
}

EntityPtr make_vehicle(const Context& c, const std::wstring& name, int type, const std::wstring& message_id = L"",
                       int transportation_id = 0)
{
    EntityPtr e = make(Kind::Vehicle, c, name);
    e->transport_id = type;
    e->transportation_id = transportation_id;
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
    int transportation_id = 0;
    std::wstring message_id;
    if (!vehicle_type_of(c.fe, type, transportation_id, message_id)) return not_handled();
    // The canoe is an auto-used key item (summoned at canals), not a walk-to vehicle.
    if (type == kCanoe) return skip();
    std::wstring name = vehicle_display_name(transportation_id, type, message_id);
    if (name.empty()) name = T(L"Vehicle");   // FF2's table has no name for this type
    return detected(make_vehicle(c, name, type, message_id, transportation_id));
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
        return detected(make_event(c, L"Warp Tile"));
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
    // FF2's vehicle spawn NPCs (the Dreadnought flying over the world map, the airship) have an
    // action but no script or message; the game's CanAction cannot tell (user, 2026-10-10).
    if (placeholder(c)) return skip();

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
    return detected(make_event(c, L"ToLayer"));
}

/// EventTriggerDetector (120): every other EventTriggerEntity, scenery excepted.
Detection event_trigger_detector(Context& c)
{
    if (!c.is_event_trigger) return not_handled();
    if (entity_is_scenery(c.fe) || placeholder(c)) return skip();
    return detected(make_event(c, L"Event"));
}

/// InteractiveEntityDetector (130): FieldMapObjectDefault and any IInteractiveEntity left.
Detection interactive_detector(Context& c)
{
    if (entity_is_scenery(c.fe) || placeholder(c)) return skip();

    if (r::kFieldMapObjectDefault.is(c.fe)) {
        if (!call_virtual_bool(r::mMapObjectGetCanAction, c.fe)) return skip();
        return detected(make_event(c, L"Interactive Object"));
    }

    if (implements(c.fe, r::kIInteractiveEntity)) {
        // ObjectType 0 + PointIn = entry points (not useful to navigate to)
        if (c.object_type() == 0 && contains(c.go_lower, L"pointin")) return skip();
        return detected(make_event(c, L"Interactive"));
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

// The other floors' entries (baked data), built once per map while the filter is "Current
// map" (user, 2026-10-11); appended after the live entities.
std::vector<EntityPtr> g_remote;
int g_remote_map_id = -1;

// Selected entity identity, to keep focus across re-sorts
EntityPtr g_sel;
bool g_sel_has = false;
Vec3 g_sel_pos{};
Category g_sel_category = Category::All;
std::wstring g_sel_name;
int g_sel_remote = 0;            // another floor's entry: its floor and cell identify it
int g_sel_cx = 0, g_sel_cy = 0;

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
/// an unresolved destination (<= 0) are kept individually. This is also FF5's
/// MapExitGroupingStrategy (same key, same "MapExitFilter" setting, nearest member as the
/// representative); FF5 only added "(N exits)" to the grouped name, which FF1 does not say.
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

/// Names end in the developers' per-map object number ("パラメキア兵7" -> "Palamecian Soldier
/// 7", "ボーゲン11" -> "Borghen 11"), kept by the translator as " N". The user's rule
/// (2026-10-10): an NPC is numbered only when several NPCs share the same name, 1..n within
/// that group; a unique name carries no number. Groups keep the developers' order (then map
/// position) so a number never changes as the player moves. Re-derived from the converted
/// name on every scan, so an entity leaving the list renumbers its group.
std::wstring strip_instance_number(const std::wstring& name, long long& number)
{
    number = -1;
    size_t end = name.size();
    while (end > 0 && is_decimal_digit(name[end - 1])) --end;
    if (end == 0 || end == name.size()) return name;   // all digits, or no number
    size_t base_end = end;
    while (base_end > 0 && name[base_end - 1] == L' ') --base_end;
    if (base_end == 0) return name;
    number = 0;
    for (size_t i = end; i < name.size(); ++i) {
        wchar_t c = name[i];
        number = number * 10 + ((c >= 0xFF10 && c <= 0xFF19) ? c - 0xFF10 : c - L'0');
    }
    return name.substr(0, base_end);
}

void renumber_npcs(std::vector<EntityPtr>& list)
{
    struct Member { EntityPtr e; long long number; };
    std::map<std::wstring, std::vector<Member>> groups;
    for (auto& e : list) {
        if (e->kind != Kind::NPC || e->remote()) continue;   // other floors are numbered per floor (build_remote)
        long long number = -1;
        std::wstring base = strip_instance_number(e->converted_name, number);
        groups[base].push_back({e, number});
    }
    for (auto& kv : groups) {
        auto& members = kv.second;
        if (members.size() == 1) {
            members[0].e->name = kv.first;
            continue;
        }
        std::stable_sort(members.begin(), members.end(), [](const Member& a, const Member& b) {
            if (a.number != b.number) {
                if (a.number < 0) return false;   // unnumbered after the numbered
                if (b.number < 0) return true;
                return a.number < b.number;
            }
            if (a.e->pos.y != b.e->pos.y) return a.e->pos.y > b.e->pos.y;   // top to bottom
            return a.e->pos.x < b.e->pos.x;
        });
        int k = 0;
        for (auto& m : members) m.e->name = kv.first + L" " + std::to_wstring(++k);
    }
}

/// The entries of the other floors (user, 2026-10-11: "destinations from other floors need to
/// be visible ... current map should include all dungeon floors, town floors etc"): the talk
/// targets, chests, save points and outward exits of every sub-map connected to this one by
/// exits and warps without crossing a world map, from the baked data
/// (baked_nav::connected_objects). Named as the live list names them, the floor's title after
/// the name ("Potion (Kashuan Keep 2F)"); NPCs are numbered within their floor as on this one.
/// Story groups and collected chests are projected from a copied native flag snapshot.
/// Formatting reads native names on the main thread in bounded batches.
struct RemoteBuilder {
    std::vector<baked_nav::RemoteObject> objs;
    size_t next = 0;
    struct Member { EntityPtr e; long long number; };
    std::map<std::wstring, std::vector<Member>> npc_groups;   // "floor\nbase name"
    std::unordered_set<std::wstring> seen;
    bool (*placeholder_fn)(const wchar_t*) = game::flags().placeholder_entity;
    explicit RemoteBuilder(std::vector<baked_nav::RemoteObject> objects) : objs(std::move(objects)) { g_remote.clear(); }
    bool step() {
        const double until = frame::now() + 0.002;
        int count = 0;
        for (; next < objs.size() && count++ < 16 && frame::now() < until; ++next) {
            const auto& r = objs[next];
            auto e = std::make_shared<Entity>();
            e->remote_map_id = r.map_id;
            e->cell_layer = r.cell.layer;
            e->cell_x = r.cell.x;
            e->cell_y = r.cell.y;
            e->floor_name = resolve_map_name(r.map_id);
            long long number = -1;
            std::wstring base;
            switch (r.type) {
            case baked::ObjType::NPC:
            case baked::ObjType::ShopNPC: {
                if (placeholder_fn && placeholder_fn(r.raw_name.c_str())) continue;
                std::wstring name = r.raw_name.empty() ? std::wstring() : translate_entity_name(r.raw_name);
                if (name.empty() || name == L"NPC") name = T(L"NPC");
                e->kind = Kind::NPC;
                e->name = name;
                e->shop = r.shop;
                base = strip_instance_number(name, number);
                break;
            }
            case baked::ObjType::TreasureBox: {
                std::wstring item = r.content_id > 0 ? resolve_item_name(r.content_id) : std::wstring();
                e->kind = Kind::TreasureChest;
                e->name = item.empty() ? T(L"Treasure Chest") : item;
                break;
            }
            case baked::ObjType::SavePoint:
                e->kind = Kind::SavePoint;
                e->name = T(L"Save Point");
                break;
            case baked::ObjType::GotoMap: {
                std::wstring dest_name = resolve_map_name(r.dest_map_id);
                e->kind = Kind::MapExit;
                e->dest_map_id = r.dest_map_id;
                e->dest_name = dest_name;
                e->name = !dest_name.empty() ? TF(L"Exit to {0}", {dest_name}) : T(L"Exit");
                break;
            }
            default:
                continue;
            }
            e->converted_name = e->name;
            // One entry per name and cell (a floor's story variants repeat its people), one exit per
            // destination per floor.
            const std::wstring where = e->kind == Kind::MapExit ? std::to_wstring(r.dest_map_id)
                                                                : std::to_wstring(r.cell.x) + L"," + std::to_wstring(r.cell.y);
            const std::wstring key = e->floor_name + L"\n" + std::to_wstring(static_cast<int>(e->kind)) + L"\n" +
                                     (e->kind == Kind::NPC ? base : e->name) + L"\n" + where;
            if (!seen.insert(key).second) continue;
            if (e->kind == Kind::NPC) npc_groups[e->floor_name + L"\n" + base].push_back({e, number});
            g_remote.push_back(e);
        }
        if (next < objs.size()) return true;
        for (auto& kv : npc_groups) {
            auto& members = kv.second;
            const std::wstring base = kv.first.substr(kv.first.find(L'\n') + 1);
            if (members.size() == 1) {
                members[0].e->name = base;
                continue;
            }
            std::stable_sort(members.begin(), members.end(), [](const Member& a, const Member& b) {
                if (a.number != b.number) {
                    if (a.number < 0) return false;   // unnumbered after the numbered
                    if (b.number < 0) return true;
                    return a.number < b.number;
                }
                if (a.e->cell_y != b.e->cell_y) return a.e->cell_y < b.e->cell_y;   // top to bottom
                return a.e->cell_x < b.e->cell_x;
            });
            int k = 0;
            for (auto& m : members) m.e->name = base + L" " + std::to_wstring(++k);
        }
        LOGF("[EntityScanner] other floors: %zu entries from %zu baked objects", g_remote.size(), objs.size());
        return false;
    }
};
void build_remote(void* map_handle)
{
    RemoteBuilder builder(baked_nav::connected_objects(map_handle));
    while (builder.step()) {} // explicit manual refresh; automatic transitions use bounded steps
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
    if (g_sel_remote > 0) {
        // Another floor's entry: the same floor and cell (a rebuilt list has new addresses).
        for (size_t i = 0; i < g_filtered.size(); ++i) {
            const auto& e = g_filtered[i];
            if (e->remote_map_id == g_sel_remote && e->cell_x == g_sel_cx && e->cell_y == g_sel_cy) return static_cast<int>(i);
        }
        return -1;
    }
    for (size_t i = 0; i < g_filtered.size(); ++i) {
        const auto& e = g_filtered[i];
        if (!e->remote() && e->category() == g_sel_category && vec_distance(e->position(), g_sel_pos) < 0.5f)
            return static_cast<int>(i);
    }
    if (!g_sel_name.empty()) {
        for (size_t i = 0; i < g_filtered.size(); ++i) {
            const auto& e = g_filtered[i];
            if (!e->remote() && e->category() == g_sel_category && e->name == g_sel_name) return static_cast<int>(i);
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
        // The other floors' entries have no position here: they follow the live ones, floor by
        // floor in the order they were built (the nearest floor first).
        for (size_t i = 0; i < g_filtered.size(); ++i) {
            const auto& e = g_filtered[i];
            keyed.emplace_back(e->remote() ? 1.0e9f + static_cast<float>(i) : vec_distance(e->position(), player_pos), e);
        }
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
    g_sel_remote = 0;
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
    g_sel_remote = e->remote_map_id;
    g_sel_cx = e->cell_x;
    g_sel_cy = e->cell_y;
}

/// A route that went on through the transition (baked_nav::pending_goal): its destination is
/// selected on the new floor (a live entity now, or still another floor's entry), so the
/// directions key and the beacon keep leading to it (user, 2026-10-11: the destination was
/// lost after "take the exit to ...").
void select_pending_destination(int map_id)
{
    baked_nav::MapGoal goal;
    TargetKind kind = TargetKind::Infer;
    std::wstring name;
    if (!baked_nav::pending_goal(goal, kind, name)) return;
    FilterContext ctx;
    for (size_t i = 0; i < g_filtered.size(); ++i) {
        const auto& e = g_filtered[i];
        bool hit = false;
        if (e->remote()) {
            hit = e->remote_map_id == goal.map_id && e->cell_x == goal.cell.x && e->cell_y == goal.cell.y;
        } else if (goal.map_id == map_id) {
            baked_route::Cell c;
            hit = baked_nav::cell_at(ctx.map_handle, e->position(), c) && c.x == goal.cell.x && c.y == goal.cell.y;
        }
        if (!hit) continue;
        g_index = static_cast<int>(i);
        save_selected_identifier();
        LOGF("[EntityScanner] destination kept through the transition: %ls", e->name.c_str());
        return;
    }
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

namespace {
std::atomic<uint64_t> g_scan_generation{0};
bool g_scan_running = false;
void finish_scan(int map_id, bool remote_ready = false)
{
    const bool map_changed = map_id != g_last_scanned_map_id;
    g_entities.clear();
    for (auto& kv : g_entity_map) g_entities.push_back(kv.second);
    renumber_npcs(g_entities);
    if (pathfinding_mode() == ui::prefs::kFilterCurrentMap) {
        if (g_remote_map_id != map_id && !remote_ready) {
            FilterContext ctx;
            build_remote(ctx.map_handle);
            g_remote_map_id = map_id;
        }
        g_entities.insert(g_entities.end(), g_remote.begin(), g_remote.end());
    } else if (g_remote_map_id != -1) {
        g_remote.clear();
        g_remote_map_id = -1;
    }
    g_last_scanned_map_id = map_id;
    apply_filter();
    if (g_index >= static_cast<int>(g_filtered.size())) g_index = 0;
    if (map_changed) select_pending_destination(map_id);
}
}

void cancel_scan()
{
    ++g_scan_generation;
    g_scan_running = false;
    frame::cancel_label("EntityScanner.Scan");
}
void invalidate_map()
{
    cancel_scan();
    g_entity_map.clear();
    g_remote_map_id = -1;
    g_remote.clear();
    g_entities.clear();
    g_filtered.clear();
    g_index = 0;
    g_last_scanned_map_id = -1;
}
bool needs_scan(int map_id) { return map_id != g_last_scanned_map_id && !g_scan_running; }

void schedule_scan()
{
    cancel_scan();
    const uint64_t generation = g_scan_generation;
    g_scan_running = true;
    struct Scan {
        int map_id = 0;
        size_t next = 0;
        std::wstring asset;
        std::vector<void*> entities;
        double started = 0;
    };
    auto scan = std::make_shared<Scan>();
    frame::every_frame([scan, generation, frames = 0]() mutable {
        if (generation != g_scan_generation) return false;
        if (++frames >= 300) { g_scan_running = false; LOGF("[EntityScanner] preparation not ready within frame budget"); return false; }
        if (!ffpr::system::is_field_navigation_ready()) { g_scan_running = false; return false; }
        if (!baked_nav::prepared()) return true; // atomic readiness only, never parse/wait
        if (!scan->map_id) {
            scan->map_id = current_map_id_or_user_data();
            if (scan->map_id <= 0) { g_scan_running = false; return false; }
            scan->started = frame::now();
            scan->asset = current_map_asset_name();
            scan->entities = all_field_entities();
            std::unordered_set<void*> current(scan->entities.begin(), scan->entities.end());
            g_entity_map.erase(std::remove_if(g_entity_map.begin(), g_entity_map.end(), [&](const auto& kv) {
                return !current.count(kv.first) || !kv.second->is_alive();
            }), g_entity_map.end());
        }
        if (scan->map_id != current_map_id_or_user_data()) { g_scan_running = false; return false; }
        // Unity/IL2CPP reads must remain on their owner thread. Spread conversions
        // across frames, capped at 16 entities or 2 ms of work per dispatch.
        const double until = frame::now() + 0.002;
        int count = 0;
        while (scan->next < scan->entities.size() && count++ < 16 && frame::now() < until) {
            void* fe = scan->entities[scan->next++];
            if (std::any_of(g_entity_map.begin(), g_entity_map.end(), [fe](const auto& kv) { return kv.first == fe; })) continue;
            if (!scan->asset.empty() && !entity_on_current_map(fe, scan->asset)) continue;
            auto entity = hooks::guarded_r("EntityScanner.Convert", EntityPtr(), [fe] { return convert(fe); });
            if (entity) g_entity_map.emplace_back(fe, std::move(entity));
        }
        if (scan->next < scan->entities.size()) return true;
        if (pathfinding_mode() == ui::prefs::kFilterCurrentMap && g_remote_map_id != scan->map_id) {
            map_state::Resolver state; // Copy live flags here; worker receives no game pointers.
            const int map_id = scan->map_id;
            const double started = scan->started;
            if (background::post([map_id, started, generation, state = std::move(state)]() mutable {
                if (generation != g_scan_generation) return; // obsolete queued work owns no live objects
                auto objects = baked_nav::connected_objects_snapshot(map_id, state);
                frame::post([map_id, started, generation, objects = std::move(objects)]() mutable {
                    if (generation != g_scan_generation) return;
                    if (map_id != current_map_id_or_user_data()) { g_scan_running = false; return; }
                    if (!ffpr::system::is_field_navigation_ready()) { g_scan_running = false; return; }
                    auto builder = std::make_shared<RemoteBuilder>(std::move(objects));
                    frame::every_frame([map_id, started, generation, builder, frames = 0]() mutable {
                        if (generation != g_scan_generation) return false;
                        if (++frames >= 1800 || map_id != current_map_id_or_user_data()) { g_scan_running = false; return false; }
                        if (!ffpr::system::is_field_navigation_ready()) { g_scan_running = false; return false; }
                        bool failed = true;
                        const bool more = hooks::guarded_r("EntityScanner.FormatRemote", false, [&] {
                            const bool result = builder->step(); failed = false; return result;
                        });
                        if (failed) { g_scan_running = false; return false; }
                        if (more) return true;
                        g_remote_map_id = map_id;
                        finish_scan(map_id, true);
                        g_scan_running = false;
                        LOGF("[EntityScanner] ready map %d: %zu live, %zu remote; %.1f ms elapsed across frames",
                             map_id, g_entity_map.size(), g_remote.size(), (frame::now() - started) * 1000);
                        return false;
                    }, "EntityScanner.ScanRemote", 1800);
                });
            })) return false;
        }
        // Worker creation failure must not put projection back on the game thread.
        finish_scan(scan->map_id, true);
        g_scan_running = false;
        LOGF("[EntityScanner] ready map %d: %zu live; %.1f ms elapsed across frames",
             scan->map_id, g_entity_map.size(), (frame::now() - scan->started) * 1000);
        return false;
    }, "EntityScanner.Scan", 300);
}

void scan_entities()
{
    if (g_scan_running) return;
    if (!baked_nav::prepared()) { schedule_scan(); return; }
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

        finish_scan(map_id);
    });
}

void force_rescan()
{
    if (g_scan_running) return;
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
        if (e->kind == Kind::MapExit && !e->remote()) out.push_back(e->pos);
    return out;
}

} // namespace scanner

std::vector<unity::Vec3> map_exit_positions() { return scanner::map_exit_positions(); }

} // namespace field
