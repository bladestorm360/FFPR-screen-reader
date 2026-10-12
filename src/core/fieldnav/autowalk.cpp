// Ordinary native input only: no transform writes, MoveTo calls or script execution.
#include "autowalk.h"
#include "autowalk_route.h"
#include "autowalk_transition.h"
#include "../core.h"
#include "../frame.h"
#include "../hooks.h"
#include "../input/input.h"
#include "../modtext.h"
#include "../say.h"
#include "../field/object_cache.h"
#include "../field/scene_events.h"
#include "../ui/preferences.h"
#include <cmath>

namespace ffpr::fieldnav::autowalk {
namespace {
ref::Class kInputDevice{"", "InputDevice"};
ref::Method mInputAxis{kInputDevice, "get_InputAxis", 0};
ref::Class kController{"Last.Map", "FieldPlayerController"};
ref::Class kKeyController{"Last.Map", "FieldPlayerKeyController"};
ref::Field fInputEnable{kController, "<InputEnable>k__BackingField"};
ref::Field fPlayer{kController, "fieldPlayer"};
ref::Class kEntity{"Last.Entity.Field", "FieldEntity"};
ref::Method mIsMoving{kEntity, "get_IsMoving", 0};
using AxisFn = unity::Vec2 (*)(void*, const void*);
AxisFn o_InputAxis = nullptr;
bool g_hooked = false, g_active = false, g_has_goal = false;
bool g_exit_approach = false;
Transition g_transition;
int g_map = 0, g_next_map = 0, g_width = 0, g_height = 0;
Route* g_route = nullptr;
field::baked_nav::MapGoal* g_goal = nullptr;
field::TargetKind g_kind = field::TargetKind::Spot;
std::wstring* g_name = nullptr;
double g_progress_at = 0;
Cell g_last_cell;
bool g_have_cell = false;
bool g_lt_was_held = false;
const void* g_entity_identity = nullptr; // identity only, never dereferenced
int g_entity_map = 0;

bool safe_context()
{
    return input::game_window_focused() && !input::suppress_game_input() && !menu_state::any_active()
        && !queries::is("BattleStateHelper.IsInBattle") && !queries::is("MessageWindowPatches.IsInDialogue")
        && !queries::is("PuzzleGameState.IsActive");
}

bool context_ok()
{
    return safe_context() && input::field_active() && queries::is("GameStatePatches.IsFieldNavigationReady")
        && !queries::is("MapTransitionPatches.IsScreenFading");
}

bool controller_ok(void* player)
{
    void* pc = field::cache::get_or_refresh(kController);
    return player && pc && unity::active_in_hierarchy(pc) && kKeyController.is(pc)
        && ref::rd<void*>(pc, fPlayer) == player && ref::rd<bool>(pc, fInputEnable);
}

bool player_cell(void* map, void* pl, Cell& cell)
{
    if (field::baked_nav::player_cell(map, pl, cell)) return true;
    // Native searcher points have no dependable layer. On an unbaked map compare x/y;
    // native collision and stairs own the actual layer, while the fallback carries -1.
    if (g_width <= 0 || g_height <= 0) return false;
    const auto pos = unity::local_position(unity::transform(pl));
    cell.x = static_cast<int>(std::floor(g_width * .5f + pos.x / 16.f));
    cell.y = static_cast<int>(std::floor(g_height * .5f - pos.y / 16.f));
    cell.layer = g_last_cell.layer;
    return true;
}

void begin(const field::PathInfo& path)
{
    if (!g_route) g_route = new Route();
    g_route->set(path.walk_phases, path.map_width, path.map_height, path.wraps);
    g_map = field::current_map_id(); g_next_map = path.next_map_id;
    g_transition.set_next(g_next_map);
    g_exit_approach = path.walk_phases.size() == 1 && path.walk_phases.front().cells.size() <= 1;
    g_width = path.map_width; g_height = path.map_height;
    g_have_cell = false;
    if (!path.walk_phases.empty() && !path.walk_phases.front().cells.empty())
        g_last_cell = path.walk_phases.front().cells.front();
    g_progress_at = frame::now();
    g_active = true;
    // The asynchronous scanner may finish after movement is ready. Keep its destination
    // token even on the final floor until arrival/cancellation, so it and the beacon select
    // the same target rather than the first entry on that floor.
    if (g_has_goal) field::baked_nav::remember_destination(*g_goal, g_kind, *g_name);
    if (ui::prefs::speak_autowalk_path_enabled())
        ffpr::say(path.full_description.empty() ? path.description : path.full_description, false);
}

unity::Vec2 axis(unity::Vec2 native)
{
    if (!g_active) return native;
    if (std::fabs(native.x) > .1f || std::fabs(native.y) > .1f) {
        cancel(true); return native; // native remaps/touch input take immediate priority
    }
    if (!safe_context()) { cancel(); return native; }
    if (g_transition.waiting()) return native;
    if (!context_ok()) {
        if (g_has_goal && g_exit_approach && g_transition.begin(frame::now())) return native;
        cancel(); return native;
    }
    void* pl = field::player();
    void* map = field::map_handle();
    if (pl && map && unity::alive(pl) && field::current_map_id() == g_map && !controller_ok(pl)
        && g_has_goal && g_exit_approach && g_transition.begin(frame::now())) return native;
    if (!pl || !map || !unity::alive(pl) || !controller_ok(pl) || field::current_map_id() != g_map) {
        cancel(); return native;
    }
    if (g_has_goal && g_goal->map_id == g_map && g_entity_map == g_map && g_entity_identity) {
        const auto target = field::selected();
        Cell cell;
        if (target.valid && !target.remote_map_id && target.identity == g_entity_identity &&
            field::baked_nav::cell_at(map, target.position, cell) &&
            (cell.x != g_goal->cell.x || cell.y != g_goal->cell.y)) {
            cancel(); ffpr::say(T(L"Autowalk stopped. Destination moved.")); return native;
        }
    }
    // A move may take several frames. Wait until the actual native move has finished before
    // comparing cells or turning; crossing a cell boundary mid-animation is not arrival.
    if (ref::call<bool>(mIsMoving, pl)) {
        g_progress_at = frame::now();
        return {};
    }
    Cell at;
    if (!player_cell(map, pl, at)) { cancel(); return native; }
    if (!g_have_cell || !(at == g_last_cell)) {
        g_last_cell = at; g_have_cell = true; g_progress_at = frame::now();
    }
    const Step step = g_route->next(at);
    if (step.result == Result::Move) {
        if (frame::now() - g_progress_at > 2.0 || !field::baked_nav::walk_edge(map, pl, at, step.next, step.goal_edge)) {
            cancel(); ffpr::say(T(L"Autowalk blocked")); return {};
        }
        g_exit_approach = step.goal_edge && g_next_map > 0;
        return {static_cast<float>(step.x), static_cast<float>(step.y)};
    }
    if (step.result == Result::Arrived && g_next_map > 0 && g_has_goal) {
        g_transition.begin(frame::now()); return {};
    }
    const bool faced = step.face_direction >= 0 && field::face_player(pl, step.face_direction);
    cancel();
    if (step.result == Result::OffRoute) ffpr::say(T(L"Autowalk stopped. Route changed."));
    else if (step.result == Result::Action) ffpr::say(T(L"Autowalk stopped") + L", " + step.instruction);
    else ffpr::say(T(L"Arrived") + (faced || step.instruction.empty() ? L"" : L", " + step.instruction), false);
    return {};
}

unity::Vec2 d_InputAxis(void* self, const void* mi)
{
    const auto native = o_InputAxis(self, mi);
    // A failed route/lifetime check must release the walk, never keep the previous axis.
    try { return axis(native); }
    catch (...) { cancel(); ref::call_failed_note("Autowalk.InputAxis"); return native; }
}

int install()
{
    g_hooked = mIsMoving.ptr() && hooks::attach(mInputAxis, &d_InputAxis, &o_InputAxis);
    actions::define("Settings.Changed.Autowalk", [] { if (!ui::prefs::autowalk_enabled()) cancel(); });
    queries::define("Autowalk.IsActive", [] { return active(); });
    actions::define("Autowalk.Cancel", [] { cancel(); });
    events::on(events::Kind::BattleStart, [] { cancel(); });
    events::on(events::Kind::MenuOpened, [] { cancel(); });
    events::on(events::Kind::SceneLoaded, [] { if (field::scene_name() == L"Title") cancel(); });
    events::on(events::Kind::MapChanging, [] {
        if (!g_active) return;
        if (g_next_map > 0 && g_has_goal) {
            g_transition.begin(frame::now());
        } else cancel();
    });
    events::on(events::Kind::MapChanged, [] {
        if (!g_active || !g_transition.waiting()) return;
        if (!g_transition.map_ready(field::current_map_id(), frame::now())) { cancel(); return; }
        frame::every_frame([] {
            if (!g_active || !g_transition.waiting()) return false;
            if (g_transition.expired(frame::now())) {
                cancel(); ffpr::say(T(L"Autowalk stopped. Route changed.")); return false;
            }
            // MapChanged means construction finished; native input/fading can still be
            // unavailable. An event-bound readiness wait replaces the old one-shot timer.
            if (!g_transition.can_resume(context_ok() && controller_ok(field::player()))) return true;
            field::PathInfo path;
            if (!field::baked_nav::route_to_goal(field::map_handle(), field::player(),
                    *g_goal, g_kind, *g_name, path) || !path.success || path.walk_phases.empty()) {
                cancel(); ffpr::say(T(L"Autowalk stopped. Route changed.")); return false;
            }
            begin(path);
            return false;
        }, "Autowalk.Continue", 3600);
    });
    return g_hooked ? 1 : 0;
}
core::Install s_install{"Autowalk", &install, 120};
}

bool active() { return g_active; }
void cancel(bool announce)
{
    const bool was_active = g_active;
    g_active = false;
    g_transition.clear();
    frame::cancel_label("Autowalk.");
    if (was_active) {
        field::baked_nav::clear_pending();
        frame::cancel_label("BakedNav.ContinuePending");
        if (announce) ffpr::say(T(L"Autowalk stopped"));
    }
}
bool stop_requested()
{
    if (!active()) return false;
    cancel(true); return true;
}
void start(const field::PathInfo& path, const field::baked_nav::MapGoal* goal, field::TargetKind kind,
           const std::wstring& name)
{
    cancel();
    if (!path.success || path.walk_phases.empty()) {
        field::baked_nav::clear_pending();
        ffpr::say(!path.reason.empty() ? path.reason : T(L"No path")); return;
    }
    void* pl = field::player();
    if (!g_hooked || !context_ok() || !pl || !controller_ok(pl)) {
        field::baked_nav::clear_pending();
        ffpr::say(T(L"Autowalk unavailable in this movement mode")); return;
    }
    if (ref::call<bool>(mIsMoving, pl)) {
        field::baked_nav::clear_pending();
        ffpr::say(T(L"Stop moving before starting autowalk")); return;
    }
    g_has_goal = goal != nullptr; g_kind = kind;
    g_entity_identity = nullptr; g_entity_map = field::current_map_id();
    if (queries::is("NavigationTargetTracker.IsEntity")) g_entity_identity = field::selected().identity;
    if (!g_goal) g_goal = new field::baked_nav::MapGoal();
    if (!g_name) g_name = new std::wstring();
    if (goal) *g_goal = *goal;
    *g_name = name;
    ffpr::say(T(L"Autowalk started"));
    begin(path);
}
void input_updated()
{
    const bool lt_held = input::left_trigger() > .5f;
    const bool lt_pressed = lt_held && !g_lt_was_held;
    g_lt_was_held = lt_held;
    if (!g_active) return;
    if (!input::game_window_focused() || !ui::prefs::autowalk_enabled() || input::suppress_game_input()) {
        cancel(); return;
    }
    using K = input::Key;
    const bool request_key = !input::shift_held() && !input::ctrl_held() && !input::alt_held()
        && (input::key_pressed(K::Backslash) || input::key_pressed(K::P) || input::key_pressed(K::Slash));
    // Field bindings are unavailable during loading, so handle a second request here too.
    if (g_transition.waiting() && !queries::is("GameStatePatches.IsFieldNavigationReady") && (request_key || lt_pressed)) {
        cancel(true); return;
    }
    const bool manual_key = input::any_key_pressed() && !request_key;
    bool manual_pad = input::left_stick_x() != 0 || input::left_stick_y() != 0
        || input::right_stick_x() != 0 || input::right_stick_y() != 0 || input::right_trigger() > .5f;
    for (int i = 0; i < input::kButtonCount; ++i) manual_pad |= input::button_pressed(i);
    if (manual_key || manual_pad) { cancel(true); return; }
    if (queries::is("BattleStateHelper.IsInBattle") || menu_state::any_active()
        || queries::is("MessageWindowPatches.IsInDialogue") || queries::is("PuzzleGameState.IsActive")) {
        cancel(); return;
    }
    if (g_transition.waiting()) {
        if (g_transition.expired(frame::now())) { cancel(); ffpr::say(T(L"Autowalk stopped. Route changed.")); }
    } else if (!context_ok() || !controller_ok(field::player())) {
        if (!g_has_goal || !g_exit_approach || !g_transition.begin(frame::now())) cancel();
    }
}
}
