// Ports FF1 Core/EntityNavigationManager.cs, Core/CategoryManager.cs and, from
// Core/FFI_ScreenReaderMod.cs: CycleNext, CyclePrevious, AnnounceCurrentEntity,
// AnnounceEntityOnly, ForceEntityRescan, CycleNextCategory, CyclePreviousCategory,
// ResetToAllCategory, GetSelectedEntity, GetCurrentMapId (navigation.cpp), AnnounceCurrentMap,
// the filter reactions of TogglePathfindingFilter / ToggleMapExitFilter / ToggleToLayerFilter,
// and OnSceneLoaded (the scene-loaded event; see scene_events.h).
//
// NavigationTargetTracker lives in src/core/fieldnav; this file reaches it through the actions
// "NavigationTargetTracker.MarkEntity" and "NavigationTargetTracker.Clear".
//
// Actions defined: Mod.CycleNext, Mod.CyclePrevious, Mod.AnnounceCurrentEntity,
// Mod.AnnounceEntityOnly, Mod.ForceEntityRescan, Mod.CycleNextCategory,
// Mod.CyclePreviousCategory, Mod.ResetToAllCategory, Mod.AnnounceCurrentMap,
// Settings.Changed.PathfindingFilter, Settings.Changed.MapExitFilter,
// Settings.Changed.ToLayerFilter.
#include "baked_nav.h"
#include "field_api.h"
#include "field_internal.h"
#include "map_names.h"
#include "object_cache.h"
#include "scene_events.h"
#include "../core.h"
#include "../frame.h"
#include "../fieldnav/autowalk.h"
#include "../fieldnav/audio_loops.h"
#include "../ui/preferences.h"
#include "../hooks.h"
#include "../log.h"
#include "../modtext.h"
#include "../say.h"
#include "../text.h"

namespace field {

std::wstring category_name(Category c)
{
    switch (c) {
    case Category::All: return T(L"All");
    case Category::Chests: return T(L"Treasure Chests");
    case Category::NPCs: return T(L"NPCs");
    case Category::MapExits: return T(L"Map Exits");
    case Category::Events: return T(L"Events");
    case Category::Vehicles: return T(L"Vehicles");
    }
    return T(L"Unknown");
}

namespace {

void speak(const std::wstring& t) { ffpr::say(t, true); }

void mark_entity()
{
    if (actions::defined("NavigationTargetTracker.MarkEntity")) actions::run("NavigationTargetTracker.MarkEntity");
}

void clear_target()
{
    if (actions::defined("NavigationTargetTracker.Clear")) actions::run("NavigationTargetTracker.Clear");
}

// =========================================================================================
// EntityNavigationManager
// =========================================================================================
int g_nav_last_scanned_map_id = -1;

/// EnsureFieldContext: the player is on an active field map and no menu is open.
bool ensure_field_context()
{
    if (menu_state::any_active()) return false;

    void* map = cache::get_or_refresh(r::kFieldMap);
    if (!map || !unity::active_in_hierarchy(map)) {
        speak(T(L"Not on map"));
        return false;
    }
    void* pc = cache::get_or_refresh(r::kFieldPlayerController);
    if (!pc || !ref::rd<void*>(pc, r::fFieldPlayer)) {
        speak(T(L"Not on map"));
        return false;
    }
    return true;
}

/// RefreshEntitiesIfNeeded: an incremental scan on each key; a full rescan on a map change.
void refresh_entities_if_needed()
{
    int id = current_map_id();
    bool map_changed = id != g_nav_last_scanned_map_id && id > 0 && g_nav_last_scanned_map_id > 0;
    if (map_changed) {
        // Refresh (not Clear) the cached controller: an early return later in the action would
        // otherwise leave the entry empty and collapse the field context.
        cache::refresh(r::kFieldPlayerController);
        scanner::force_rescan();
        g_nav_last_scanned_map_id = id;
        clear_target();
    } else {
        scanner::scan_entities();
        if (g_nav_last_scanned_map_id <= 0 && id > 0) g_nav_last_scanned_map_id = id;
    }
}

/// FormatCurrentEntity: name, steps, direction, type and "(N of M)"; empty when none.
std::wstring format_current_entity()
{
    EntityPtr e = scanner::current_entity();
    if (!e) return L"";
    FilterContext ctx;
    std::wstring s = e->format_description(ctx.player_position);
    s += L" " + TF(L"({0} of {1})", {text::num(scanner::current_index() + 1), text::num(scanner::count())});
    return s;
}

/// Where a route to this entity ends (field_api.h TargetKind): beside and facing an NPC or
/// chest, on an exit, on a vehicle or save point; an event by what is baked at its cell.
TargetKind target_kind_of(const Entity& e)
{
    switch (e.kind) {
    case Kind::NPC:
    case Kind::TreasureChest: return TargetKind::Interact;
    case Kind::MapExit: return TargetKind::Transition;
    case Kind::SavePoint:
    case Kind::Vehicle: return TargetKind::Spot;
    default: return TargetKind::Infer;
    }
}

/// AnnounceCurrentEntity: the path to the selected entity.
void announce_current_entity(bool walking = false)
{
    if (!ensure_field_context()) return;
    refresh_entities_if_needed();

    EntityPtr e = scanner::current_entity();
    if (!e) {
        speak(T(L"No entity selected"));
        return;
    }
    mark_entity();

    FilterContext ctx;
    if (!ctx.field_player || !unity::transform(ctx.field_player)) {
        speak(T(L"Cannot determine directions"));
        return;
    }
    const TargetKind kind = target_kind_of(*e);
    PathInfo p;
    baked_nav::MapGoal walk_goal;
    bool have_walk_goal = false;
    if (e->remote()) {
        // Another floor (user, 2026-10-11): the first leg, and the destination is remembered so
        // that the next leg follows the transition (baked_nav::continue_pending).
        baked_nav::MapGoal goal;
        goal.map_id = e->remote_map_id;
        goal.cell = baked_route::Cell{e->cell_layer, e->cell_x, e->cell_y};
        walk_goal = goal;
        have_walk_goal = true;
        baked_nav::route_to_goal(ctx.map_handle, ctx.field_player, goal, kind, e->display_name() + L" (" + e->floor_name + L")", p);
    } else {
        p = find_path_to(ctx.player_position, e->position(), ctx.map_handle, ctx.field_player, kind);
        baked_route::Cell walk_cell;
        if (baked_nav::cell_at(ctx.map_handle, e->position(), walk_cell)) {
            walk_goal.map_id = current_map_id();
            walk_goal.cell = walk_cell;
            have_walk_goal = true;
        }
        // Cut off on this floor and routed through another one: remembered the same way.
        baked_route::Cell c;
        if (p.success && p.next_map_id > 0 && baked_nav::cell_at(ctx.map_handle, e->position(), c)) {
            baked_nav::MapGoal goal;
            goal.map_id = current_map_id();
            goal.cell = c;
            baked_nav::remember_destination(goal, kind, e->display_name());
        } else {
            baked_nav::clear_pending();
        }
    }
    // No route: the reason when the router knows it ("Story gated", "Blocked by an NPC",
    // "Requires Ship"; user, 2026-10-11), else "No path".
    if (walking) {
        ffpr::fieldnav::autowalk::start(p, have_walk_goal ? &walk_goal : nullptr, kind, e->display_name());
    } else {
        speak(p.success && !p.description.empty() ? p.description : (!p.reason.empty() ? p.reason : T(L"No path")));
    }
}

/// Shared keyboard/controller policy; autowalk speech remains its independent setting.
void navigate_to_entity()
{
    if (!ensure_field_context()) return;
    if (ffpr::fieldnav::autowalk::stop_requested()) return;
    mark_entity();
    const bool walking = ui::prefs::autowalk_enabled();
    if (ui::prefs::audio_beacons_enabled()) {
        if (walking || ui::prefs::beacon_with_directions_enabled())
            ffpr::fieldnav::audio_loops::restart_beacon();
        else { actions::run("Mod.RestartEntityBeacon"); return; }
    }
    announce_current_entity(walking);
}

void announce_entity_only()
{
    if (!ensure_field_context()) return;
    refresh_entities_if_needed();

    std::wstring announcement = format_current_entity();
    if (announcement.empty()) {
        std::wstring cat = category_name(scanner::category());
        if (scanner::count() == 0) speak(TF(L"No {0} found", {cat}));
        else speak(T(L"No entity selected"));
        return;
    }
    mark_entity();
    speak(announcement);
}

void cycle(bool next)
{
    if (!ensure_field_context()) return;
    refresh_entities_if_needed();
    if (next) scanner::next_entity();
    else scanner::previous_entity();
    baked_nav::clear_pending();   // a new target: a route that was going on to another floor is dropped

    if (scanner::no_reachable_entities()) {
        speak(T(L"No reachable entities"));
        return;
    }
    mark_entity();
    announce_entity_only();
}

// =========================================================================================
// CategoryManager
// =========================================================================================
Category g_current_category = Category::All;

void announce_category_change()
{
    std::wstring category_text = TF(L"Category: {0}", {category_name(g_current_category)});
    baked_nav::clear_pending();   // the selection moves to the category's first entity

    // Refresh so the new category's nearest entity is the current selection.
    refresh_entities_if_needed();

    // Obey the pathfinding filter: select the nearest reachable entity. None reachable (or an
    // empty category): the category name only ("No reachable entities" is for cycling).
    if (!scanner::select_first_reachable()) {
        speak(category_text);
        return;
    }
    std::wstring description = format_current_entity();
    if (description.empty()) {
        speak(category_text);
        return;
    }
    // The category and its first entity, which becomes the navigation target.
    mark_entity();
    speak(category_text + L", " + description);
}

void category_next()
{
    if (!ensure_field_context()) return;
    g_current_category = static_cast<Category>((static_cast<int>(g_current_category) + 1) % kCategoryCount);
    scanner::set_category(g_current_category);
    announce_category_change();
}

void category_previous()
{
    if (!ensure_field_context()) return;
    int prev = static_cast<int>(g_current_category) - 1;
    if (prev < 0) prev = kCategoryCount - 1;
    g_current_category = static_cast<Category>(prev);
    scanner::set_category(g_current_category);
    announce_category_change();
}

void category_reset_to_all()
{
    if (!ensure_field_context()) return;
    if (g_current_category == Category::All) {
        speak(T(L"Already in All category"));
        return;
    }
    g_current_category = Category::All;
    scanner::set_category(g_current_category);
    announce_category_change();
}

// =========================================================================================
// FFI_ScreenReaderMod
// =========================================================================================
void force_entity_rescan()
{
    scanner::force_rescan();
    speak(T(L"Entity scan complete"));
}

void announce_current_map()
{
    std::wstring name = hooks::guarded_r("Mod.AnnounceCurrentMap", std::wstring(), [] { return current_map_name(); });
    speak(name.empty() ? T(L"Unknown location") : name);
}

// --- OnSceneLoaded -----------------------------------------------------------------------
std::wstring g_scene_name;
std::wstring g_previous_scene_name;

ref::Class kSceneManager{"UnityEngine.SceneManagement", "SceneManager"};
ref::Method mInternalSceneLoaded{kSceneManager, "Internal_SceneLoaded", 2};
ref::Class kScene{"UnityEngine.SceneManagement", "Scene"};
ref::Method mSceneGetNameInternal{kScene, "GetNameInternal", 1};

// static void Internal_SceneLoaded(Scene scene, LoadSceneMode mode): Scene is { int m_Handle }.
using SceneLoaded_t = void (*)(int32_t scene, int32_t mode, const void* mi);
SceneLoaded_t o_SceneLoaded = nullptr;

void d_SceneLoaded(int32_t scene, int32_t mode, const void* mi)
{
    o_SceneLoaded(scene, mode, mi);   // the game's sceneLoaded handlers first
    hooks::guarded("Field.OnSceneLoaded", [&] {
        std::wstring name = ref::str(ref::call_static<void*>(mSceneGetNameInternal, scene));
        LOGF("[ComponentCache] Scene loaded: %ls", name.c_str());
        g_previous_scene_name = g_scene_name;
        g_scene_name = name;

        // Clear the old component cache
        cache::clear_all();

        // Everything else of FF1's OnSceneLoaded runs in the areas that own it, from this
        // event (fieldnav: audio stop and location tracker reset).
        events::fire(events::Kind::SceneLoaded);
    });
}

int install()
{
    actions::define("Mod.CycleNext", [] { cycle(true); });
    actions::define("Mod.CyclePrevious", [] { cycle(false); });
    actions::define("Mod.AnnounceCurrentEntity", [] { announce_current_entity(); });
    actions::define("Mod.NavigateToEntity", [] { navigate_to_entity(); });
    actions::define("Mod.AnnounceEntityOnly", [] { announce_entity_only(); });
    actions::define("Mod.ForceEntityRescan", [] { force_entity_rescan(); });
    actions::define("Mod.CycleNextCategory", [] { category_next(); });
    actions::define("Mod.CyclePreviousCategory", [] { category_previous(); });
    actions::define("Mod.ResetToAllCategory", [] { category_reset_to_all(); });
    actions::define("Mod.AnnounceCurrentMap", [] { announce_current_map(); });

    // The toggles store the setting; the scanner reads the filter settings live, so the
    // reactions only re-apply the list as FF1's setters did.
    actions::define("Settings.Changed.PathfindingFilter", [] {});   // applied at the next cycle
    actions::define("Settings.Changed.MapExitFilter", [] { scanner::reapply_filter(); });
    actions::define("Settings.Changed.ToLayerFilter", [] { scanner::set_filter_to_layer(true); });

    // A route that goes on past a transition (user, 2026-10-11): 0.8 s after "Entering X", the
    // next leg to the remembered destination from where the player arrived.
    events::on(events::Kind::MapChanged, [] {
        if (ffpr::fieldnav::autowalk::active()) return; // walker owns continuation and its speech setting
        frame::after_seconds(0.8, [] { hooks::guarded("BakedNav.ContinuePending", [] { baked_nav::continue_pending(); }); },
                             "BakedNav.ContinuePending");
    });
    events::on(events::Kind::BattleStart, [] {
        scanner::cancel_scan();
        frame::cancel_label("BakedNav.ContinuePending");
        baked_nav::invalidate();
    });
    events::on(events::Kind::SceneLoaded, [] {
        if (scene_name() == L"Title") scanner::invalidate_map();
    });

    int n = hooks::attach(mInternalSceneLoaded, &d_SceneLoaded, &o_SceneLoaded) ? 1 : 0;
    if (!n) LOGF("[Field] SceneManager.Internal_SceneLoaded not hooked: no scene-loaded event");
    return n;
}
core::Install s_install{"Field.EntityNavigation", &install, 100};

} // namespace

// --- scene_events.h --------------------------------------------------------------------------
const std::wstring& scene_name() { return g_scene_name; }
const std::wstring& previous_scene_name() { return g_previous_scene_name; }

// --- field_api.h selection -------------------------------------------------------------------

/// FFI_ScreenReaderMod.GetSelectedEntity: EntityScanner.CurrentEntity.
Target selected()
{
    Target t;
    hooks::guarded("Field.Selected", [&] {
        EntityPtr e = scanner::current_entity();
        if (!e) return;
        t.valid = true;
        t.position = e->position();
        t.name = e->name;
        t.identity = e->game ? e->game : static_cast<const void*>(e.get());
        t.category = static_cast<int>(e->category());
        t.remote_map_id = e->remote_map_id;
        t.cell_layer = e->cell_layer;
        t.cell_x = e->cell_x;
        t.cell_y = e->cell_y;
    });
    return t;
}

/// Event-bound, asynchronous scan after native field readiness.
void request_rescan()
{
    scanner::schedule_scan();
}

} // namespace field
