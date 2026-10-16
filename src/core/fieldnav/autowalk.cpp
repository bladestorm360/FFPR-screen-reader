// Ordinary native input only: no transform writes, MoveTo calls or script execution.
#include "autowalk.h"
#include "autowalk_route.h"
#include "autowalk_transition.h"
#include "native_steering.h"
#include "../core.h"
#include "../frame.h"
#include "../hooks.h"
#include "../input/input.h"
#include "../input/bindings.h"
#include "../modtext.h"
#include "../say.h"
#include "../field/object_cache.h"
#include "../field/scene_events.h"
#include "../ui/preferences.h"
#include "../log.h"
#include <cmath>

namespace ffpr::fieldnav::autowalk {
namespace {
ref::Class kInputDevice{"", "InputDevice"};
ref::Method mInputAxis{kInputDevice, "get_InputAxis", 0};
ref::Class kController{"Last.Map", "FieldPlayerController"};
ref::Class kKeyController{"Last.Map", "FieldPlayerKeyController"};
ref::Class kAirshipController{"Last.Map", "FieldPlayerKeyAirshipController"};
ref::Method mAirshipUpdate{kAirshipController, "LateUpdateObserveInput", 0};
ref::Method mAirshipKey{kAirshipController, "OnKey", 1}, mAirshipKeyUp{kAirshipController, "OnKeyUp", 1};
ref::Method mAirshipAxis{kAirshipController, "OnTouchPadCallback", 1};
ref::Field fShiftUp{kAirshipController, "isDownShiftUp"}, fShiftDown{kAirshipController, "isDownShiftDown"};
ref::Class kFieldMap{"", "FieldMap"};
ref::Field fFieldController{kFieldMap, "fieldController"};
ref::Class kFieldController{"Last.Map", "FieldController"};
ref::Method mRotation{kFieldController, "GetZAxisRotateBirdCamera", 0};
ref::Field fInputEnable{kController, "<InputEnable>k__BackingField"};
ref::Field fPlayer{kController, "fieldPlayer"};
ref::Class kTouch{"Last.Map","FieldPlayerTouchBaseController"};
ref::Class kChoice{"Last.Map","FieldPlayerKeyTwoStepController"};
using AirshipUpdate = void (*)(void*, const void*);
struct Profile {
    ref::Class& cls;
    ref::Method update, axis;
    ref::Field acceleration;
    ref::Field* translation=nullptr;
    ref::Field* left_scale=nullptr;
    ref::Field* right_scale=nullptr;
    AirshipUpdate original=nullptr;
    bool hooked=false, touch_airship=false, key_bird=false;
    Profile(ref::Class& type,bool air=false,bool key=false):
        cls(type), update(cls,"LateUpdateObserveInput",0),axis(cls,"OnTouchPadCallback",1),
        acceleration(cls,"rotateAccelerator"),touch_airship(air),key_bird(key) {}
};
ref::Class kTouchAirship{"Last.Map","FieldPlayerTouchAirshipController"};
ref::Class kKeyBird{"Last.Map","FieldPlayerKeyGroundBirdController",game::kFF3|game::kFF5|game::kFF6};
ref::Class kTouchBird{"Last.Map","FieldPlayerTouchGroundBirdController",game::kFF3|game::kFF5|game::kFF6};
ref::Field fTranslation{kTouchAirship,"isTranslationMode"};
ref::Field fLeftScale{kKeyBird,"leftFastRotateScale"},fRightScale{kKeyBird,"rightFastRotateScale"};
Profile g_touch_airship{kTouchAirship,true};
Profile g_key_bird{kKeyBird,false,true};
Profile g_touch_bird{kTouchBird};
bool g_airship_hooked=false;
Profile* profile(void* c) {
    for(auto* p:{&g_touch_airship,&g_key_bird,&g_touch_bird})if(p->cls.is(c))return p;
    return nullptr;
}
ref::Class kEntity{"Last.Entity.Field", "FieldEntity"};
ref::Method mIsMoving{kEntity, "get_IsMoving", 0};
using AxisFn = unity::Vec2 (*)(void*, const void*);
AxisFn o_InputAxis = nullptr;
bool g_hooked = false, g_active = false, g_has_goal = false;
bool g_planning = false;
bool g_battle_interrupted = false;
bool g_exit_approach = false;
Transition g_transition;
int g_map = 0, g_next_map = 0, g_width = 0, g_height = 0;
Route* g_route = nullptr;
field::baked_nav::MapGoal* g_goal = nullptr;
field::TargetKind g_kind = field::TargetKind::Spot;
std::wstring* g_name = nullptr;
double g_progress_at = 0;
double g_turn_started=0;
float g_turn_angle=0;
bool g_turning=false;
Cell g_last_cell;
bool g_have_cell = false;
VehicleAction g_vehicle_action = VehicleAction::None;
int g_vehicle_id = 0, g_key_action = -1;
uint64_t g_key_frame = 0;
double g_vehicle_deadline = 0;
double g_boundary_deadline = 0;
Cell g_boundary_target;
VehicleAction g_boundary_action = VehicleAction::None;
int g_boundary_vehicle = 0;
const void* g_entity_identity = nullptr; // identity only, never dereferenced
void* g_flight_axis_owner = nullptr; // release only through this controller's native input callback
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

bool controller_supported(void* pc)
{
    const auto p=profile(pc);
    return p?p->hooked : kAirshipController.is(pc)?g_airship_hooked : kKeyController.is(pc)||kTouch.is(pc);
}
bool controller_ok(void* player)
{
    void* pc = field::player_controller();
    return player && pc && unity::active_in_hierarchy(pc) && controller_supported(pc)
        && ref::rd<void*>(pc, fPlayer) == player && ref::rd<bool>(pc, fInputEnable);
}

bool player_cell(void* map, void* pl, Cell& cell)
{
    if (field::baked_nav::player_cell(map, pl, cell)) return true;
    // During native vehicle handoff the layer can be temporarily unavailable. Use
    // the current baked dimensions and last verified layer; walk_edge still requires a bake.
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
    if (g_vehicle_action != VehicleAction::None) {
        const auto vehicle = field::current_vehicle();
        const bool boarded = vehicle.valid && vehicle.id == g_vehicle_id;
        const bool completed = g_vehicle_action == VehicleAction::ConfirmBoard ? boarded :
            vehicle.valid && (vehicle.type == 1 || vehicle.type == 5);
        if (completed && context_ok() && controller_ok(field::player())) {
            g_route->complete_vehicle_action(); g_vehicle_action = VehicleAction::None;
            g_progress_at = frame::now(); g_have_cell = false;
        } else {
            if (frame::now() > g_vehicle_deadline) { cancel(); ffpr::say(T(L"Autowalk blocked")); }
            return {};
        }
    }
    if (!context_ok()) {
        if (g_boundary_deadline > frame::now()) return {};
        if (g_has_goal && g_exit_approach && g_transition.begin(frame::now())) return native;
        cancel(); return native;
    }
    void* pl = field::player();
    void* map = field::map_handle();
    if (pl && map && unity::alive(pl) && field::current_map_id() == g_map && !controller_ok(pl)
        && g_has_goal && g_exit_approach && g_transition.begin(frame::now())) return native;
    if (!pl || !map || !unity::alive(pl) || !controller_ok(pl) || field::current_map_id() != g_map) {
        if (pl && map && field::current_map_id() == g_map && g_boundary_deadline > frame::now()) return {};
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
    if (g_boundary_deadline > 0 && at == g_boundary_target) {
        const auto vehicle = field::current_vehicle();
        const bool correct = vehicle.valid && (g_boundary_action == VehicleAction::WalkOn
            ? vehicle.id == g_boundary_vehicle : vehicle.type == 1 || vehicle.type == 5);
        g_boundary_deadline = 0;
        if (!correct) { cancel(); ffpr::say(T(L"Autowalk stopped. Route changed.")); return {}; }
    }
    if (!g_have_cell || !(at == g_last_cell)) {
        g_last_cell = at; g_have_cell = true; g_progress_at = frame::now();
    }
    const Step step = g_route->next(at);
    if (step.result == Result::Move) {
        if (frame::now() - g_progress_at > 2.0 || !field::baked_nav::walk_edge(map, pl, at, step.next, step.goal_edge,
            step.vehicle_action, step.transportation_id)) {
            cancel(); ffpr::say(T(L"Autowalk blocked")); return {};
        }
        g_exit_approach = step.goal_edge && g_next_map > 0;
        if (step.vehicle_action == VehicleAction::WalkOn || step.vehicle_action == VehicleAction::WalkOff) {
            g_boundary_deadline = frame::now() + 10.0;
            g_boundary_target = step.next; g_boundary_action = step.vehicle_action; g_boundary_vehicle = step.transportation_id;
        }
        unity::Vec2 wanted{static_cast<float>(step.x), static_cast<float>(step.y)};
        void* controller=field::player_controller();
        auto* special=profile(controller);
        if (special || kAirshipController.is(controller)) {
            void* fc = ref::rd<void*>(field::cache::get(kFieldMap), fFieldController);
            const float angle = fc ? ref::call<float>(mRotation, fc) : NAN;
            if (!std::isfinite(angle)) { cancel(); return {}; }
            if(special && (!special->touch_airship || !ref::rd<bool>(controller,*special->translation))) {
                const float a=ref::rd<float>(controller,special->acceleration);
                const float l=special->key_bird?ref::rd<float>(controller,*special->left_scale):1.f;
                const float r=special->key_bird?ref::rd<float>(controller,*special->right_scale):1.f;
                if(!std::isfinite(a)||!std::isfinite(l)||!std::isfinite(r)) {cancel();return {};}
                const auto result=steer(wanted.x,wanted.y,angle,a,l,r);
                if(!result.y) {
                    if(!g_turning) {g_turning=true;g_turn_started=frame::now();g_turn_angle=angle;}
                    if(std::fabs(std::remainder(angle-g_turn_angle,360.f))>.01f) {g_turn_angle=angle;g_progress_at=frame::now();}
                    if(frame::now()-g_turn_started>15 || frame::now()-g_progress_at>2) {cancel();ffpr::say(T(L"Autowalk blocked"));return {};}
                } else g_turning=false;
                return {result.x,result.y};
            }
            const float radians = angle * 0.017453292519943295f;
            // Native MoveToAxis rotates its input by the bird camera angle.
            wanted = {wanted.x * std::cos(radians) + wanted.y * std::sin(radians),
                      -wanted.x * std::sin(radians) + wanted.y * std::cos(radians)};
        }
        return wanted;
    }
    if (step.result == Result::Arrived && g_next_map > 0 && g_has_goal) {
        g_transition.begin(frame::now()); return {};
    }
    const bool faced = step.face_direction >= 0 && field::face_player(pl, step.face_direction);
    if (step.result == Result::Action && step.vehicle_action != VehicleAction::None) {
        if (step.face_direction >= 0 && !faced) { cancel(); ffpr::say(T(L"Autowalk blocked")); return {}; }
        g_vehicle_action = step.vehicle_action; g_vehicle_id = step.transportation_id;
        g_key_action = step.vehicle_action == VehicleAction::BackLand ? 1 : 0;
        g_key_frame = frame::count(); g_vehicle_deadline = frame::now() + 10.0;
        return {};
    }
    cancel();
    if (step.result == Result::OffRoute) {
        LOGF("[Autowalk] off route map %d cell (%d,%d) L%d vehicle %d type %d",
            g_map, at.x, at.y, at.layer, field::current_vehicle().id, field::current_vehicle().type);
        ffpr::say(T(L"Autowalk stopped. Route changed."));
    }
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

AirshipUpdate o_airship_update;
void d_airship_update(void* self, const void* mi)
{
    // Only the native input route is changed. Both shoulder inputs select the
    // game's cardinal-movement branch; its collision, speed and animation remain native.
    const bool eligible = kAirshipController.is(self) && self == field::player_controller();
    if (eligible && (g_active || g_flight_axis_owner == self)) {
        // LateUpdate consumes the axis cached by the PREVIOUS input observation
        // before reading InputDevice again. Publish this frame's route intent through
        // the normal native callback first; otherwise a corner/stop has one extra move.
        const auto wanted = axis(o_InputAxis(nullptr, nullptr));
        ref::call<void>(mAirshipAxis, self, wanted);
        g_flight_axis_owner = g_active ? self : nullptr;
    }
    const bool driving = g_active && context_ok() && eligible && controller_ok(field::player());
    const bool up = driving && !ref::rd<bool>(self, fShiftUp);
    const bool down = driving && !ref::rd<bool>(self, fShiftDown);
    if (up) ref::call<void>(mAirshipKey, self, int32_t{2});
    if (down) ref::call<void>(mAirshipKey, self, int32_t{3});
    o_airship_update(self, mi);
    if (unity::alive(self)) {
        if (up) ref::call<void>(mAirshipKeyUp, self, int32_t{2});
        if (down) ref::call<void>(mAirshipKeyUp, self, int32_t{3});
    }
}

void special_update(Profile& p,void* self,const void* mi) {
    const bool eligible=p.cls.is(self) && self==field::player_controller();
    if(eligible && (g_active || g_flight_axis_owner==self)) {
        const auto wanted=axis(o_InputAxis(nullptr,nullptr));
        ref::call<void>(p.axis,self,wanted);
        g_flight_axis_owner=g_active?self:nullptr;
    }
    p.original(self,mi);
}
void d_touch_airship(void* self,const void* mi) {special_update(g_touch_airship,self,mi);}
void d_key_bird(void* self,const void* mi) {special_update(g_key_bird,self,mi);}
void d_touch_bird(void* self,const void* mi) {special_update(g_touch_bird,self,mi);}

int install()
{
    g_touch_airship.translation=&fTranslation;
    g_key_bird.left_scale=&fLeftScale;g_key_bird.right_scale=&fRightScale;
    g_hooked = mIsMoving.ptr() && hooks::attach(mInputAxis, &d_InputAxis, &o_InputAxis);
    g_airship_hooked = hooks::attach(mAirshipUpdate, &d_airship_update, &o_airship_update) && mAirshipAxis.ptr();
    g_touch_airship.hooked=hooks::attach(g_touch_airship.update,&d_touch_airship,&g_touch_airship.original) && g_touch_airship.axis.ptr();
    if(game::in(game::kFF3|game::kFF5|game::kFF6)) {
        g_key_bird.hooked=hooks::attach(g_key_bird.update,&d_key_bird,&g_key_bird.original) && g_key_bird.axis.ptr();
        g_touch_bird.hooked=hooks::attach(g_touch_bird.update,&d_touch_bird,&g_touch_bird.original) && g_touch_bird.axis.ptr();
    }
    actions::define("Settings.Changed.Autowalk", [] { if (!ui::prefs::autowalk_enabled()) cancel(); });
    queries::define("Autowalk.IsActive", [] { return active(); });
    actions::define("Autowalk.Cancel", [] { cancel(); });
    events::on(events::Kind::BattleEntering, [] {
        g_battle_interrupted = active();
        g_active = g_planning = false;
        g_key_action = -1;
    });
    events::on(events::Kind::BattleStart, [] { cancel(); });
    events::on(events::Kind::MenuOpened, [] { cancel(); });
    events::on(events::Kind::SceneLoaded, [] { if (field::scene_name() == L"Title") cancel(); });
    events::on(events::Kind::MapChanging, [] {
        if (!g_active) {g_planning=false;return;}
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
            g_planning = true;
            field::baked_nav::request_route(*g_goal, g_kind, *g_name, [](field::PathInfo path) {
                if (!g_planning || !g_active || !context_ok()) return;
                g_planning = false;
                if (!path.success || path.walk_phases.empty()) {
                    cancel(); ffpr::say(T(L"Autowalk stopped. Route changed.")); return;
                }
                begin(path);
            });
            return false;
        }, "Autowalk.Continue", 3600);
    });
    return g_hooked ? 1 : 0;
}
core::Install s_install{"Autowalk", &install, 120};
}

bool active() { return g_active || g_planning; }
void cancel(bool announce)
{
    const bool was_active = active() || g_battle_interrupted;
    g_battle_interrupted = false;
    if (g_planning) field::baked_nav::cancel_requests();
    g_planning = false;
    g_active = false;
    g_turning=false;
    g_vehicle_action = VehicleAction::None; g_key_action = -1; g_key_frame = 0; g_boundary_deadline = 0;
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
void request(field::baked_nav::MapGoal goal, field::TargetKind kind, std::wstring name)
{
    cancel();
    if (!context_ok()) return;
    g_planning = true;
    // Separate copies avoid unspecified argument evaluation moving the goal/name
    // before request_route receives them.
    auto completion = [goal, kind, name](field::PathInfo path) {
        if (!g_planning || !context_ok()) return;
        g_planning = false;
        start(path, &goal, kind, name);
    };
    field::baked_nav::request_route(std::move(goal), kind, std::move(name), std::move(completion));
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
    void* controller=field::player_controller();
    if(g_hooked && context_ok() && pl && !controller_ok(pl) && (!controller || controller_supported(controller)) && goal) {
        const auto target=*goal;
        const auto requested_map=field::current_map_id();
        const double deadline=frame::now()+.75;
        g_planning=true;
        ffpr::say(T(L"Autowalk is waiting for player input."));
        frame::every_frame([target,kind,name,requested_map,deadline] {
            if(!g_planning)return false;
            if(!safe_context() || requested_map!=field::current_map_id()) {cancel();return false;}
            if(context_ok() && controller_ok(field::player())) {request(target,kind,name);return false;}
            if(frame::now()>=deadline) {cancel();ffpr::say(T(L"Autowalk could not start. Player input is disabled."));return false;}
            return true;
        },"Autowalk.InputReady",120);
        return;
    }
    if (!g_hooked || !context_ok() || !pl || !controller_ok(pl)) {
        field::baked_nav::clear_pending();
        const wchar_t* reason=kChoice.is(controller)?L"Choose an option before starting autowalk.":
            !g_hooked || (controller && !controller_supported(controller))?L"Autowalk input hook is unavailable.":
            !context_ok()?L"Autowalk is waiting for player input.":L"Autowalk could not start. Player input is disabled.";
        LOGF("[Autowalk] start rejected hook=%d context=%d player=%d controller=%d",g_hooked,context_ok(),pl!=nullptr,field::player_controller()!=nullptr);
        ffpr::say(T(reason)); return;
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
    if (!path.warning.empty() && ui::prefs::mod_warnings_enabled()) ffpr::say(path.warning, false);
    begin(path);
}
void input_updated()
{
    if (queries::is("GameStatePatches.IsInputTransitionBlocked")) return;
    if (!active()) return;
    if (g_vehicle_action != VehicleAction::None && frame::now() > g_vehicle_deadline) {
        cancel(); ffpr::say(T(L"Autowalk blocked")); return;
    }
    if (!input::game_window_focused() || !ui::prefs::autowalk_enabled() || input::suppress_game_input()) {
        cancel(); return;
    }
    using K = input::Key;
    const bool request_key=input::bindings::navigation_request_pressed();
    bool manual_key=false;
    if(!request_key)for(int i=0;i<int(K::Count);++i)
        if((i<int(K::LeftShift)||i>int(K::RightWindows)) && input::key_pressed(K(i))) {manual_key=true;break;}
    namespace b=input::bindings;
    auto stick_manual=[](float x,float y,int base) {
        return (y<-.5f&&!b::navigation_controller_input(base)) || (y>.5f&&!b::navigation_controller_input(base+1))
            || (x<-.5f&&!b::navigation_controller_input(base+2)) || (x>.5f&&!b::navigation_controller_input(base+3));
    };
    bool manual_pad=stick_manual(input::left_stick_x(),input::left_stick_y(),b::kLeftUp)
        || stick_manual(input::right_stick_x(),input::right_stick_y(),b::kRightUp)
        || (input::left_trigger()>.5f&&!b::navigation_controller_input(b::kLeftTrigger))
        || (input::right_trigger()>.5f&&!b::navigation_controller_input(b::kRightTrigger));
    for(int i=0;i<input::kButtonCount;++i)manual_pad|=input::button_pressed(i)&&!b::navigation_controller_input(i);
    if (manual_key || (manual_pad && !request_key)) { cancel(true); return; }
    if (queries::is("BattleStateHelper.IsInBattle") || menu_state::any_active()
        || queries::is("MessageWindowPatches.IsInDialogue") || queries::is("PuzzleGameState.IsActive")) {
        cancel(); return;
    }
    if (g_planning && !g_active) return; // no native movement while worker plans or input handoff settles
    if (g_transition.waiting()) {
        if (g_transition.expired(frame::now())) { cancel(); ffpr::say(T(L"Autowalk stopped. Route changed.")); }
    } else if (g_vehicle_action == VehicleAction::None && g_boundary_deadline <= frame::now() && (!context_ok() || !controller_ok(field::player()))) {
        if (!g_has_goal || !g_exit_approach || !g_transition.begin(frame::now())) cancel();
    }
}
bool virtual_key(int action, bool up)
{
    if (!virtual_input_active() || g_key_action != action || !safe_context()) return false;
    const auto now = frame::count();
    return up ? now == g_key_frame + 1 : now == g_key_frame;
}
bool virtual_input_active()
{
    if (!g_active || g_key_action < 0) return false;
    const auto now = frame::count();
    return now == g_key_frame || now == g_key_frame + 1;
}
}
