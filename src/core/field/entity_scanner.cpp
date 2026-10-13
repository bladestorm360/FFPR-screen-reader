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
#include "npc_names.h"
#include "entity_filter.h"
#include "capture_budget.h"
#include "event_labels.h"
#include "field_api.h"
#include "field_internal.h"
#include "map_names.h"
#include "object_cache.h"
#include "path_search.h"
#include "baked_nav.h"
#include "../game.h"
#include "../background.h"
#include "../worker_job.h"
#include "../frame.h"
#include "../system/system.h"
#include "../say.h"
#include "../fieldnav/map_transition.h"
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
    if (!pathfinding_enabled()) return true;
    // A vehicle-only trigger hidden for the current vehicle is not a walking target: it is
    // reached by boarding its vehicle, so the walking path test does not apply.
    if (e->game && entity_is_hidden_by_vehicle(e->game)) return true;
    if (!ctx.player_controller) return false;
    // Read the completed baked reachability flood; no native search/window limit.
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
    std::wstring contents = ui::prefs::spoil_treasure_chest_contents_enabled() ? treasure_contents(c.property()) : std::wstring();
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
    if (placeholder(c)) {
        void* property = c.property();
        const bool scripted = property && r::kPropertyEvent.is(property) && ref::rd<int32_t>(property, r::fEventScriptId) > 0;
        const bool message = property && r::kPropertyTalk.is(property) && !ref::rd_str(property, r::fTalkMessageKey).empty();
        // A ready, native-actionable NPC with a script/message is a talk target,
        // including animals/monsters that share names with cutscene placeholders.
        if (!scripted && !message) return skip();
    }

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
std::array<std::vector<EntityPtr>, 6> g_category_views;
std::vector<Vec3> g_exit_positions; // worker-compiled unfiltered live exits, for wall tones
int g_index = 0;
Category g_category = Category::All;
std::vector<EntityPtr>& filtered_view() { return g_category_views[static_cast<size_t>(g_category)]; }
bool g_views_ready = false;
bool g_navigation_filtered = false;
int g_view_vehicle = 0;
std::optional<Vec3> g_view_origin;
double g_view_captured_at = 0;
int g_last_scanned_map_id = -1;
worker_job::Owner g_filter_work;
bool g_filter_pending = false;
uint64_t g_filter_generation = 0;

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

// Preserve the existing remote story-variant identity guard exactly. Naming cleanup is
// independent of entity inclusion: no new/broader deduplication is authorized here.
std::wstring remote_guard_base(const std::wstring& name)
{
    size_t end = name.size();
    while (end > 0 && is_decimal_digit(name[end - 1])) --end;
    if (end == 0 || end == name.size()) return name;
    size_t base_end = end;
    while (base_end > 0 && name[base_end - 1] == L' ') --base_end;
    return base_end == 0 ? name : name.substr(0, base_end);
}

/// The entries of the other floors (user, 2026-10-11: "destinations from other floors need to
/// be visible ... current map should include all dungeon floors, town floors etc"): the talk
/// targets, chests, save points and outward exits of every sub-map connected to this one by
/// exits and warps without crossing a world map, from the baked data
/// (baked_nav::connected_objects). Named as the live list names them, the floor's title after
/// the name ("Potion (Kashuan Keep 2F)"); NPCs are numbered within their floor as on this one.
/// Story groups and chest opened state are projected from a copied native flag snapshot.
/// Formatting reads native names on the main thread in bounded batches.
struct RemoteRow { baked_nav::RemoteObject object; std::wstring floor, name, destination; };
struct RemoteInput { std::vector<RemoteRow> rows; std::wstring locale; };
struct RemoteBuilder {
    std::vector<baked_nav::RemoteObject> objects;
    RemoteInput input;
    size_t next = 0;
    bool (*placeholder_fn)(const wchar_t*) = game::flags().placeholder_entity;
    explicit RemoteBuilder(std::vector<baked_nav::RemoteObject> values) : objects(std::move(values)) {
        input.locale = modtext::lang();
    }
    bool step() {
        CaptureBudget budget;
        for (; next < objects.size() && budget.available(); ++next) {
            const auto& object = objects[next];
            RemoteRow row; row.object = object; row.floor = resolve_map_name(object.map_id);
            switch (object.type) {
            case baked::ObjType::NPC:
            case baked::ObjType::ShopNPC:
                if (placeholder_fn && placeholder_fn(object.raw_name.c_str()) && !object.scripted) continue;
                row.name = object.raw_name.empty() ? std::wstring() : translate_entity_name(object.raw_name);
                break;
            case baked::ObjType::TreasureBox:
                if (ui::prefs::spoil_treasure_chest_contents_enabled() && object.content_id > 0) row.name = resolve_item_name(object.content_id);
                break;
            case baked::ObjType::GotoMap: row.destination = resolve_map_name(object.dest_map_id); break;
            case baked::ObjType::SavePoint: break;
            default: continue;
            }
            input.rows.push_back(std::move(row));
        }
        return next < objects.size();
    }
};
std::vector<EntityPtr> build_remote(RemoteInput& input) {
    modtext::LocaleScope locale(input.locale);
    std::vector<EntityPtr> result;
    std::unordered_set<std::wstring> seen;
    for (auto& row : input.rows) {
        const auto& object = row.object;
        auto entity = std::make_shared<Entity>(); // new owned mod data; no engine pointers
        entity->remote_map_id = object.map_id; entity->cell_layer = object.cell.layer;
        entity->cell_x = object.cell.x; entity->cell_y = object.cell.y; entity->floor_name = std::move(row.floor);
        std::wstring base;
        switch (object.type) {
        case baked::ObjType::NPC:
        case baked::ObjType::ShopNPC:
            entity->kind = Kind::NPC; entity->shop = object.shop;
            entity->name = row.name.empty() || row.name == L"NPC" ? T(L"NPC") : std::move(row.name);
            base = remote_guard_base(entity->name); break;
        case baked::ObjType::TreasureBox:
            entity->kind = Kind::TreasureChest;
            entity->opened_known = object.opened.has_value(); entity->opened = object.opened.value_or(false);
            entity->name = row.name.empty() ? T(L"Treasure Chest") : std::move(row.name); break;
        case baked::ObjType::SavePoint: entity->kind = Kind::SavePoint; entity->name = T(L"Save Point"); break;
        case baked::ObjType::GotoMap:
            entity->kind = Kind::MapExit; entity->dest_map_id = object.dest_map_id; entity->dest_name = std::move(row.destination);
            entity->name = entity->dest_name.empty() ? T(L"Exit") : TF(L"Exit to {0}", {entity->dest_name}); break;
        default: continue;
        }
        entity->converted_name = entity->name;
        const std::wstring where = entity->kind == Kind::MapExit ? std::to_wstring(object.dest_map_id)
            : std::to_wstring(object.cell.x) + L"," + std::to_wstring(object.cell.y);
        const std::wstring key = entity->floor_name + L"\n" + std::to_wstring(static_cast<int>(entity->kind)) + L"\n" +
            (entity->kind == Kind::NPC ? base : entity->name) + L"\n" + where;
        if (!seen.insert(key).second) continue; // unchanged story-variant inclusion guard
        result.push_back(std::move(entity));
    }
    return result;
}

// Owned row snapshots: native positions/pad labels are copied in bounded batches.
// No EntityPtr, engine object or owner callback is sent to the compute worker.
using namespace filter_worker;

void cancel_filter() {
    g_filter_work.cancel(); ++g_filter_generation; g_filter_pending = false;
    frame::cancel_label("EntityScanner.Filter");
}
void save_selected_identifier();
void apply_filter() {
    cancel_filter();
    const double captured_at = frame::now();
    g_filter_pending = true;
    const auto generation = g_filter_generation;
    const auto map_id = current_map_id_or_user_data();
    auto entities = std::make_shared<std::vector<EntityPtr>>(g_entities); // owner registry only
    auto input = std::make_shared<FilterInput>();
    input->category = g_category; input->sort = player_local_position(input->player);
    input->pads = game::flags().has_vehicle_pads; input->exits = settings::get_bool("MapExitFilter", false);
    input->navigation = baked_nav::begin_exit_capture(pathfinding_mode() == ui::prefs::kFilterCurrentMapWithVehicles,input->exits);
    input->world = input->navigation && baked_nav::exit_snapshot_world(*input->navigation);
    const bool navigation_filtered = input->navigation != nullptr;
    const int vehicle = input->navigation ? baked_nav::exit_snapshot_vehicle(*input->navigation) : 0;
    const auto epoch = g_filter_work.epoch(); const auto epoch_generation = epoch->load();
    input->cancelled = [epoch,epoch_generation] { return epoch->load() != epoch_generation; };
    input->layer = to_layer_enabled(); input->selected_has = g_sel_has;
    input->selected_position = g_sel_pos; input->selected_category = g_sel_category; input->selected_name = g_sel_name;
    input->selected_remote = g_sel_remote; input->selected_x = g_sel_cx; input->selected_y = g_sel_cy;
    baked_nav::MapGoal continued;
    TargetKind continued_kind;
    std::wstring continued_name;
    input->continued_goal = baked_nav::pending_goal(continued, continued_kind, continued_name);
    input->current_map = map_id; input->continued_map = continued.map_id;
    input->continued_x = continued.cell.x; input->continued_y = continued.cell.y;
    const int continued_map = input->continued_map, continued_x = input->continued_x, continued_y = input->continued_y;
    auto obstacles = std::make_shared<EntityCursor>(); // native pointers remain in this closure
    frame::every_frame([entities, input, obstacles, captured = false, next = size_t(0), generation, map_id, continued_map, continued_x, continued_y, captured_at, navigation_filtered, vehicle]() mutable {
        if (generation != g_filter_generation) return false;
        if (!ffpr::system::is_field_navigation_ready() || map_id != current_map_id_or_user_data()) { cancel_filter(); return false; }
        CaptureBudget budget;
        for (; next < entities->size() && budget.available(); ++next) {
            const auto& entity = (*entities)[next];
            FilterRow row;
            row.index = next; row.kind = entity->kind; row.category = entity->category();
            row.position = entity->position(); row.numbering_position = entity->pos;
            row.remote_map = entity->remote_map_id; row.cell_x = entity->cell_x; row.cell_y = entity->cell_y;
            row.cell_layer = entity->cell_layer;
            if (input->continued_goal && !row.remote_map && map_id == continued_map) {
                baked_route::Cell cell;
                row.cell_valid = baked_nav::cell_at(field::map_handle(), row.position, cell);
                if (row.cell_valid) { row.cell_x = cell.x; row.cell_y = cell.y; }
            }
            row.destination = entity->dest_map_id; row.name = entity->name; row.event_type = entity->event_type;
            if (input->pads) row.pad = vehicle_pad_name(entity);
            if (row.kind == Kind::NPC) row.label = npc_display_label(entity->converted_name);
            row.selected = entity == g_sel;
            input->rows.push_back(std::move(row));
        }
        if (next < entities->size()) return true;
        if (input->navigation && !captured) {
            bool more = true;
            while (budget.available()) {
                void* entity = nullptr;
                if (!(more = obstacles->next(entity))) break;
                if (entity) baked_nav::capture_exit_obstacle(*input->navigation,entity);
            }
            if (more) return true;
            baked_nav::finish_exit_capture(*input->navigation);
            captured = true;
        }
        const auto origin = input->sort ? std::optional<Vec3>(input->player) : std::nullopt;
        worker_job::start(g_filter_work, std::move(*input), &filter_worker::build_categories,
            [entities, generation, map_id, continued_map, continued_x, continued_y, origin, captured_at, navigation_filtered, vehicle](Categories filtered) {
                if (generation != g_filter_generation || map_id != current_map_id_or_user_data()) return;
                auto result = std::make_shared<Categories>(std::move(filtered));
                auto handles = std::make_shared<std::array<std::vector<EntityPtr>, 6>>(); // native owner registry only
                frame::every_frame([entities, result, handles, named = size_t(0), ordered = size_t(0), view = size_t(0),
                                    generation, map_id, continued_map, continued_x, continued_y, origin, captured_at, navigation_filtered, vehicle]() mutable {
                    if (generation != g_filter_generation) return false;
                    if (!ffpr::system::is_field_navigation_ready() || map_id != current_map_id_or_user_data()) {
                        cancel_filter(); return false;
                    }
                    CaptureBudget budget;
                    auto& all = result->views[0];
                    for (; named < all.names.size() && budget.available(); ++named)
                        (*entities)[named]->name = std::move(all.names[named]);
                    for (; view < handles->size() && budget.available(); ++view, ordered = 0) {
                        const auto& order = result->views[view].order;
                        for (; ordered < order.size() && budget.available(); ++ordered)
                            (*handles)[view].push_back((*entities)[order[ordered]]);
                        if (ordered < order.size()) return true;
                    }
                    if (named < all.names.size() || view < handles->size()) return true;
                    if (navigation_filtered) {
                        Vec3 at; const auto transport = field::current_vehicle();
                        if (!origin || !player_local_position(at) || at.x!=origin->x || at.y!=origin->y || at.z!=origin->z ||
                            (transport.valid ? transport.id : 0)!=vehicle) {
                            g_views_ready=false; apply_filter(); return false;
                        }
                    }
                    g_category_views.swap(*handles); g_exit_positions.swap(all.exits);
                    auto& current = result->views[static_cast<size_t>(g_category)];
                    // Input can move selection while this refresh is running. Keep
                    // the latest owner-side identity, not the capture's old index.
                    const auto selected = std::find(filtered_view().begin(), filtered_view().end(), g_sel);
                    if (g_sel && selected != filtered_view().end())
                        g_index = static_cast<int>(selected - filtered_view().begin());
                    else if (g_sel_has && current.restored >= 0) g_index = current.restored;
                    baked_nav::MapGoal goal; TargetKind kind; std::wstring name;
                    const bool continued_current = current.continued >= 0 && baked_nav::pending_goal(goal, kind, name)
                        && goal.map_id == continued_map && goal.cell.x == continued_x && goal.cell.y == continued_y;
                    if (continued_current) g_index = current.continued;
                    if (g_index >= static_cast<int>(filtered_view().size())) g_index = 0;
                    g_views_ready = true;
                    g_view_origin = origin;
                    g_navigation_filtered = navigation_filtered; g_view_vehicle = vehicle;
                    g_view_captured_at = captured_at;
                    g_filter_pending = false;
                    if (continued_current) {
                        save_selected_identifier();
                        LOGF("[EntityScanner] destination kept through the transition: %ls", name.c_str());
                    }
                    return false;
                }, "EntityScanner.FilterPublish", INT_MAX);
            }, background::Lane::Navigation);
        return false;
    }, "EntityScanner.FilterCapture", INT_MAX);
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
void when_filter_ready(std::function<void()> ready) {
    ensure_correct_map();
    if (g_views_ready && g_navigation_filtered) {
        Vec3 at; const auto transport=field::current_vehicle();
        if (!g_view_origin || !player_local_position(at) || at.x!=g_view_origin->x || at.y!=g_view_origin->y || at.z!=g_view_origin->z ||
            (transport.valid ? transport.id : 0)!=g_view_vehicle) {
            g_views_ready=false; apply_filter();
        }
    }
    const int map_id = current_map_id_or_user_data();
    auto attempt = [map_id, ready = std::move(ready)]() mutable {
        if (!ffpr::system::is_field_navigation_ready() || map_id != current_map_id_or_user_data()) return false;
        // A live refresh must not put each browse input behind a full map scan.
        // Map/settings invalidation clears readiness; ordinary refresh keeps it.
        if (!g_views_ready) return true;
        if (!pathfinding_enabled()) { ready(); return false; }
        if (!baked_nav::available(field::map_handle())) { ffpr::say(T(L"Navigation map data unavailable")); return false; }
        baked_nav::request_reachability(pathfinding_mode() == ui::prefs::kFilterCurrentMapWithVehicles, std::move(ready));
        return false;
    };
    if (attempt()) frame::every_frame(std::move(attempt), "EntityScanner.Ready", INT_MAX);
}

namespace {
std::atomic<uint64_t> g_scan_generation{0};
bool g_scan_running = false;
worker_job::Owner g_scan_work;
void finish_scan(int map_id, bool remote_ready = false)
{
    g_entities.clear();
    for (auto& kv : g_entity_map) g_entities.push_back(kv.second);
    if (pathfinding_mode() == ui::prefs::kFilterCurrentMap) {
        g_entities.insert(g_entities.end(), g_remote.begin(), g_remote.end());
    } else if (g_remote_map_id != -1) {
        g_remote.clear();
        g_remote_map_id = -1;
    }
    g_last_scanned_map_id = map_id;
    apply_filter();
    if (g_index >= static_cast<int>(filtered_view().size())) g_index = 0;
    // Selection is restored by the asynchronous filter publication.
}
}

void cancel_scan()
{
    ++g_scan_generation;
    g_scan_work.cancel();
    g_scan_running = false;
    frame::cancel_label("EntityScanner.Scan");
    frame::cancel_label("EntityScanner.ScanRemote");
}
void invalidate_map()
{
    cancel_scan();
    cancel_filter();
    g_entity_map.clear();
    g_remote_map_id = -1;
    g_remote.clear();
    g_entities.clear();
    for (auto& view : g_category_views) view.clear();
    g_views_ready = false;
    g_view_origin.reset();
    g_navigation_filtered=false; g_view_vehicle=0;
    g_view_captured_at = 0;
    g_exit_positions.clear();
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
        EntityCursor cursor;
        std::wstring asset;
        bool captured = false;
        size_t prune = 0;
        double started = 0;
    };
    auto scan = std::make_shared<Scan>();
    frame::every_frame([scan, generation]() mutable {
        if (generation != g_scan_generation) return false;
        if (!ffpr::system::is_field_navigation_ready()) { g_scan_running = false; return false; }
        if (ffpr::fieldnav::map_transition::is_screen_fading()) return true;
        if (!baked_nav::prepared()) return true; // atomic readiness only, never parse/wait
        if (!scan->map_id) {
            scan->map_id = current_map_id_or_user_data();
            if (scan->map_id <= 0) { g_scan_running = false; return false; }
            scan->started = frame::now();
            scan->asset = current_map_asset_name();
        }
        if (scan->map_id != current_map_id_or_user_data()) { g_scan_running = false; return false; }
        // Read one current native collection slot at a time. No eager list copy,
        // deferred stale list pointer or full native allocation precedes the batch.
        bool more = !scan->captured;
        CaptureBudget budget;
        for (; more && budget.available();) {
            void* fe = nullptr;
            if (!(more = scan->cursor.next(fe))) break;
            if (!fe) continue;
            auto known = std::find_if(g_entity_map.begin(), g_entity_map.end(), [fe](const auto& kv) { return kv.first == fe; });
            if (known != g_entity_map.end() && known->second->is_alive()) continue;
            if (!scan->asset.empty() && !entity_on_current_map(fe, scan->asset)) continue;
            auto entity = hooks::guarded_r("EntityScanner.Convert", EntityPtr(), [fe] { return convert(fe); });
            if (entity) {
                if (known != g_entity_map.end()) known->second = std::move(entity);
                else g_entity_map.emplace_back(fe, std::move(entity));
            }
        }
        if (more) return true;
        scan->captured = true;
        for (; scan->prune < g_entity_map.size() && budget.available(); ++scan->prune) {
            auto& entry = g_entity_map[scan->prune];
            if (!scan->cursor.seen.count(entry.first) || !entry.second->is_alive()) entry.second.reset();
        }
        if (scan->prune < g_entity_map.size()) return true;
        g_entity_map.erase(std::remove_if(g_entity_map.begin(), g_entity_map.end(), [](const auto& entry) {
            return !entry.second;
        }), g_entity_map.end());
        if (pathfinding_mode() == ui::prefs::kFilterCurrentMap && g_remote_map_id != scan->map_id) {
            map_state::Resolver state; // Copy live flags here; worker receives no game pointers.
            const int map_id = scan->map_id;
            const double started = scan->started;
            struct Projection { int map_id; map_state::Resolver state; };
            worker_job::start(g_scan_work, Projection{map_id, std::move(state)},
                [](Projection& input) { return baked_nav::connected_objects_snapshot(input.map_id, input.state); },
                [map_id, started, generation](std::vector<baked_nav::RemoteObject> objects) mutable {
                    if (generation != g_scan_generation) return;
                    if (map_id != current_map_id_or_user_data()) { g_scan_running = false; return; }
                    if (!ffpr::system::is_field_navigation_ready()) { g_scan_running = false; return; }
                    auto builder = std::make_shared<RemoteBuilder>(std::move(objects));
                    frame::every_frame([map_id, started, generation, builder]() mutable {
                        if (generation != g_scan_generation) return false;
                        if (map_id != current_map_id_or_user_data()) { g_scan_running = false; return false; }
                        if (!ffpr::system::is_field_navigation_ready()) { g_scan_running = false; return false; }
                        if (ffpr::fieldnav::map_transition::is_screen_fading()) return true;
                        bool failed = true;
                        const bool more = hooks::guarded_r("EntityScanner.FormatRemote", false, [&] {
                            const bool result = builder->step(); failed = false; return result;
                        });
                        if (failed) { g_scan_running = false; return false; }
                        if (more) return true;
                        worker_job::start(g_scan_work, std::move(builder->input), &build_remote,
                            [map_id, started, generation](std::vector<EntityPtr> remote) {
                                if (generation != g_scan_generation || map_id != current_map_id_or_user_data()) return;
                                g_remote = std::move(remote); g_remote_map_id = map_id;
                                finish_scan(map_id, true); g_scan_running = false;
                                LOGF("[EntityScanner] ready map %d: %zu live, %zu remote; %.1f ms elapsed across frames",
                                    map_id, g_entity_map.size(), g_remote.size(), (frame::now() - started) * 1000);
                            });
                        return false;
                    }, "EntityScanner.ScanRemote", INT_MAX);
                });
            return false;
        }
        // No remote projection is needed for this current filter.
        finish_scan(scan->map_id, true);
        g_scan_running = false;
        LOGF("[EntityScanner] ready map %d: %zu live; %.1f ms elapsed across frames",
             scan->map_id, g_entity_map.size(), (frame::now() - scan->started) * 1000);
        return false;
    }, "EntityScanner.Scan", INT_MAX);
}

void scan_entities()
{
    if (!g_scan_running) schedule_scan();
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
    if (filtered_view().empty()) {
        scan_entities();
        if (filtered_view().empty()) return;
    }
    int n = static_cast<int>(filtered_view().size());
    FilterContext ctx;
    int start = g_index;
    for (int attempts = 0; attempts < n; ++attempts) {
        g_index = (g_index + 1) % n;
        if (pathfinding_passes(filtered_view()[static_cast<size_t>(g_index)], ctx)) {
            save_selected_identifier();
            return;
        }
    }
    g_index = start;
}

void previous_entity()
{
    ensure_correct_map();
    if (filtered_view().empty()) {
        scan_entities();
        if (filtered_view().empty()) return;
    }
    int n = static_cast<int>(filtered_view().size());
    FilterContext ctx;
    int start = g_index;
    for (int attempts = 0; attempts < n; ++attempts) {
        g_index = (g_index - 1 + n) % n;
        if (pathfinding_passes(filtered_view()[static_cast<size_t>(g_index)], ctx)) {
            save_selected_identifier();
            return;
        }
    }
    g_index = start;
}

bool no_reachable_entities()
{
    if (!pathfinding_enabled() || filtered_view().empty()) return false;
    FilterContext ctx;
    for (auto& e : filtered_view())
        if (pathfinding_passes(e, ctx)) return false;
    return true;
}

bool select_first_reachable()
{
    if (filtered_view().empty()) return false;
    FilterContext ctx;
    int n = static_cast<int>(filtered_view().size());
    for (int i = 0; i < n; ++i) {
        int idx = (g_index + i) % n;
        if (pathfinding_passes(filtered_view()[static_cast<size_t>(idx)], ctx)) {
            g_index = idx;
            save_selected_identifier();
            return true;
        }
    }
    return false;
}

void reapply_filter()
{
    g_views_ready = false;
    apply_filter();
    if (g_index >= static_cast<int>(filtered_view().size())) g_index = 0;
}

void set_filter_to_layer(bool)
{
    // FilterToLayer's setter: the filter state is the setting itself; re-apply at once.
    reapply_filter();
}

void set_category(Category c)
{
    if (g_category == c) return;
    g_category = c;
    clear_selected_identifier();
    // Every published generation already contains this view. If the map is still
    // being captured, let its pending worker prepare all views together.
    g_index = 0;
    Vec3 at;
    if (g_views_ready && player_local_position(at) && (!g_view_origin ||
        at.x != g_view_origin->x || at.y != g_view_origin->y || at.z != g_view_origin->z)) {
        // Preserve nearest-first category entry after movement. Refresh only the
        // small live row snapshot/sort, without waiting for a full entity scan.
        g_views_ready = false;
        apply_filter();
    }
}

Category category() { return g_category; }

EntityPtr current_entity()
{
    ensure_correct_map();
    if (filtered_view().empty() || g_index < 0 || g_index >= static_cast<int>(filtered_view().size())) return nullptr;
    auto entity = filtered_view()[static_cast<size_t>(g_index)];
    return entity && entity->is_alive() ? entity : nullptr;
}

int current_index() { return g_index; }
int count() { return static_cast<int>(filtered_view().size()); }
double captured_at() { return g_view_captured_at; }

std::vector<unity::Vec3> map_exit_positions()
{
    // GetMapExitPositions: every MapExitEntity of the unfiltered list (wall-tone suppression)
    return g_exit_positions;
}

} // namespace scanner

std::vector<unity::Vec3> map_exit_positions() { return scanner::map_exit_positions(); }

} // namespace field
