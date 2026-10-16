// Ports FF1 Core/ControllerRouter.cs.
//
// The controller state machine: decides where each SDL input goes. The router consumes the
// buttons the mod uses; everything else passes to the game through input_passthrough.cpp.
//
// Not ported: FeedLeftStickToGame (unused in FF1, "kept for potential future use").
#include <cstring>
#include "input_internal.h"
#include "bindings.h"
#include "../core.h"
#include "../say.h"
#include "../modtext.h"
#include "../log.h"
#include "../ui/mod_ui.h"
#include "../ui/preferences.h"

namespace input {
namespace {

RouterState g_state = RouterState::Normal;
Context g_game_context = Context::Global;    // CurrentGameContext
bool g_field_active = false;                 // IsFieldActive
bool g_consumed[kButtonCount]{};

enum class LastDevice { Keyboard, Controller };
LastDevice g_last_device = LastDevice::Keyboard;

// Field state tracking
bool g_left_trigger_was_active = false;
bool g_was_left_stick_active = false;

// Field stick clicks (update_stick_clicks)
int g_stick_click_button = -1;   // the stick click being tracked, -1 if none
bool g_stick_chord_fired = false;  // L3+R3 already toggled during this press

// Synthetic press handed to the game for a lone stick click while Stick Click Normalization is
// on: "down" (GetKeyDown + GetKey) on the frame the click resolves, "up" (GetKeyUp) on the
// next frame. Read by input_passthrough.cpp.
int g_pulse_button = -1;
bool g_pulse_down_phase = false;

const char* state_name(RouterState s)
{
    switch (s) {
    case RouterState::ModMode: return "ModMode";
    case RouterState::ModMenu: return "ModMenu";
    default: return "Normal";
    }
}

const wchar_t* context_name(Context c)
{
    switch (c) {
    case Context::Field: return L"Field";
    case Context::Battle: return L"Battle";
    case Context::Status: return L"Status";
    case Context::BestiaryDetail: return L"BestiaryDetail";
    case Context::Results: return L"Results";
    case Context::KeyHelp: return L"KeyHelp";
    default: return L"Global";
    }
}

bool in_dialogue() { return queries::is("MessageWindowPatches.IsInDialogue"); }
bool in_battle() { return queries::is("BattleStateHelper.IsInBattle"); }
bool puzzle_active() { return queries::is("PuzzleGameState.IsActive"); }

void consume(int b)
{
    if (b >= 0 && b < kButtonCount) g_consumed[b] = true;
}

void consume_all()
{
    for (bool& c : g_consumed) c = true;
}

void run_action(const char* name) { actions::run(name); }

/// Called every frame while no gamepad is available. Mod mode exists only for the controller,
/// so it is dropped; mod-menu state is kept only while the menu is open (the keyboard can still
/// drive and close it).
void release_controller_owned_state()
{
    if (g_state == RouterState::ModMode || (g_state == RouterState::ModMenu && !ui::mod_menu::is_open())) {
        LOGF("[ControllerRouter] No gamepad: leaving %s, game input restored", state_name(g_state));
        g_state = RouterState::Normal;
    }
    // Stale edge/consume state must not carry over to a reconnected controller.
    g_left_trigger_was_active = false;
    g_was_left_stick_active = false;
    g_stick_click_button = -1;
    g_stick_chord_fired = false;
    g_pulse_button = -1;
    std::memset(g_consumed, 0, sizeof g_consumed);
}

void announce_bindings(int scope,bool menu) {
    std::wstring message;
    const auto& list=bindings::entries();
    for(size_t i=0;i<list.size();++i) {
        const auto& e=list[i];if(!e.controller || (e.scope!=scope && e.scope!=-10) || e.primary<0)continue;
        if(e.id.find("teleport")!=std::string::npos && !ui::prefs::teleport_shortcuts_enabled())continue;
        if(!message.empty())message+=L". ";
        message+=T(e.label)+L": "+bindings::value(int(i));
    }
    if(menu)ffpr::say_menu(message,true);else ffpr::say(message,true);
}
void announce_mod_mode_controls() {
    announce_bindings(in_dialogue()?-14:queries::is("ReviewBuffer.IsResults")||queries::is("ReviewBuffer.IsActive")?-15:in_battle()?-16:-17,false);
}
void announce_mod_menu_controls() {announce_bindings(-18,true);}

void open_mod_menu() { ui::mod_menu::open(); }     // speaks; SyncWithModMenuOpen sets ModMenu
void close_mod_menu() { ui::mod_menu::close(); }   // speaks; SyncWithModMenuClosed sets Normal

void handle_state_transitions()
{
    // Start: mod menu toggle. Closing is always allowed; opening only on the active field.
    if (bindings::pad("pad.menu.toggle")) {

        if (g_state == RouterState::ModMenu) close_mod_menu();
        else if (g_field_active) open_mod_menu();
        else speak_mod_menu_unavailable();
        return;
    }

    // Back/Select: mod mode toggle (Normal <-> ModMode).
    if (bindings::pad("pad.mode.toggle")) {

        if (g_state == RouterState::Normal) {
            g_state = RouterState::ModMode;
            ffpr::say(T(L"Mod"), true);
        } else if (g_state == RouterState::ModMode) {
            g_state = RouterState::Normal;
            ffpr::say(T(L"Cancelled"), true);
        }
    }
}

void handle_normal_field()
{
    // L3 / R3 are handled by update_stick_clicks.

    // Interrupt speech on any navigation input.
    const bool left_stick_active = left_stick_x() != 0.f || left_stick_y() != 0.f;
    const bool left_stick_just_moved = left_stick_active && !g_was_left_stick_active;
    g_was_left_stick_active = left_stick_active;

    const bool any_nav = left_stick_just_moved
        || bindings::pad("pad.field.waypoint.previous") || bindings::pad("pad.field.waypoint.next")
        || bindings::pad("pad.field.waypoint.category.previous") || bindings::pad("pad.field.waypoint.category.next")
        || bindings::pad("pad.field.entity.previous") || bindings::pad("pad.field.entity.next")
        || bindings::pad("pad.field.entity.category.previous") || bindings::pad("pad.field.entity.category.next");
    if (any_nav) ffpr::silence();

    // D-pad: waypoint navigation (consumed). The callees mark the navigation target.
    if (bindings::pad("pad.field.waypoint.previous")) {  run_action("WaypointHandler.CyclePrevious"); }
    if (bindings::pad("pad.field.waypoint.next")) {  run_action("WaypointHandler.CycleNext"); }
    if (bindings::pad("pad.field.waypoint.category.previous")) {  run_action("WaypointHandler.CyclePreviousCategory"); }
    if (bindings::pad("pad.field.waypoint.category.next")) {  run_action("WaypointHandler.CycleNextCategory"); }

    // Right stick: entity scanner.
    if (bindings::pad("pad.field.entity.previous")) run_action("Mod.CyclePrevious");
    if (bindings::pad("pad.field.entity.next")) run_action("Mod.CycleNext");
    if (bindings::pad("pad.field.entity.category.previous")) run_action("Mod.CyclePreviousCategory");
    if (bindings::pad("pad.field.entity.category.next")) run_action("Mod.CycleNextCategory");

    // Left trigger: pathfind to the last selected target (or restart the beacon).
    if (bindings::pad("pad.field.route")) {
        if (queries::is("NavigationTargetTracker.IsWaypoint")) {
            run_action("WaypointHandler.PathfindToCurrentWaypoint");
        } else if (queries::is("NavigationTargetTracker.IsEntity")) {
            run_action("Mod.NavigateToEntity");
        } else {
            ffpr::say(T(L"No target selected"), true);
        }
    }
    g_left_trigger_was_active = left_trigger() > 0.5f;
}

void handle_normal_non_field(Context context)
{
    // Right stick up: read description (I key).
    if (bindings::pad("pad.other.details")) global_hotkeys::handle_item_details_key();
    // Right stick down: read controls (Shift+I).
    if (bindings::pad("pad.other.help")) run_action("KeyHelpReader.AnnounceKeyHelp");
    // D-pad and left stick: virtual buffer navigation in Status / Bestiary / key help.
    const char* prev = nullptr;
    const char* next = nullptr;
    if (context == Context::Status) {
        prev = "StatusNavigationReader.NavigatePrevious";
        next = "StatusNavigationReader.NavigateNext";
    } else if (context == Context::BestiaryDetail) {
        prev = "BestiaryNavigationReader.NavigatePrevious";
        next = "BestiaryNavigationReader.NavigateNext";
    } else if (context == Context::KeyHelp) {
        prev = "KeyHelpReader.NavigatePrevious";
        next = "KeyHelpReader.NavigateNext";
    }
    if (!prev) return;
    if (bindings::pad("pad.other.previous") || bindings::pad("pad.other.previous.stick")) {  run_action(prev); }
    if (bindings::pad("pad.other.next") || bindings::pad("pad.other.next.stick")) {  run_action(next); }
}

void handle_normal_state(Context context)
{
    // Face buttons interrupt queued speech, as Enter does on the keyboard.
    if (button_pressed(kSouth) || button_pressed(kEast) || button_pressed(kWest) || button_pressed(kNorth))
        ffpr::silence();

    // 15-puzzle: right stick up reads the cursor row/column, whatever the context.
    if (puzzle_active()) {
        if (bindings::pad("pad.puzzle.position")) run_action("PuzzlePatches.AnnouncePosition");
        return;
    }

    if (g_field_active) handle_normal_field();
    else handle_normal_non_field(context);

    // LB/RB always pass to the game (tab switching in status and menus).
}

/// A stick click that starts in Normal on the active field resolves on RELEASE, so both clicks
/// together make the L3+R3 chord without the first acting alone. The chord toggles Stick Click
/// Normalization. A lone click then does its job: normalization off -> L3 beacon navigation,
/// R3 pathfinding filter; on -> the click goes to the game as a one-frame press. Both clicks
/// are consumed from the first press until both are up, and act only if the player is still on
/// the field in Normal. Mod mode and every other screen keep their own stick-click handling.
void update_stick_clicks()
{
    if(g_pulse_button>=0) { if(g_pulse_down_phase)g_pulse_down_phase=false;else g_pulse_button=-1; }
    if(g_state!=RouterState::Normal || !g_field_active || puzzle_active())return;
    const bool beacon=bindings::pad_held("pad.field.beacon"), filter=bindings::pad_held("pad.field.filter");
    if(bindings::pad("pad.field.normalize")) { g_stick_chord_fired=true; run_action("Mod.ToggleStickClickNormalization"); }
    if(beacon || filter)return;
    if(g_stick_chord_fired) {g_stick_chord_fired=false;return;}
    for(const auto& item:{std::pair{"pad.field.beacon",kLeftStick},std::pair{"pad.field.filter",kRightStick}}) {
        if(!bindings::pad(item.first,true))continue;
        if(ui::prefs::stick_click_normalization_enabled()) {
            g_pulse_button=item.second;g_pulse_down_phase=true;
        } else run_action(item.second==kLeftStick?"Mod.ToggleAudioBeacons":"Mod.TogglePathfindingFilter");
    }
}

/// Mod mode: face buttons give mod information, then the router returns to Normal.
void handle_mod_mode_state()
{
    consume_all();

    // Dialogue first: with a message window up the player wants the message again.
    if (in_dialogue()) {
        if (bindings::pad("pad.mode.dialogue.repeat")) { run_action("MessageWindowPatches.RepeatLastDialogue"); g_state = RouterState::Normal; return; }
    } else if (queries::is("ReviewBuffer.IsResults") || queries::is("ReviewBuffer.IsActive")) {
        if (bindings::pad("pad.mode.results.details")) { run_action("ReviewBuffer.Details"); return; }
        if (bindings::pad("pad.mode.results.next") || bindings::pad("pad.mode.results.next.dpad")) { run_action("ReviewBuffer.Next"); return; }
        if (bindings::pad("pad.mode.results.previous") || bindings::pad("pad.mode.results.previous.dpad")) { run_action("ReviewBuffer.Previous"); return; }
        if (bindings::pad("pad.mode.results.first")) { run_action("ReviewBuffer.First"); return; }
        if (bindings::pad("pad.mode.results.last")) { run_action("ReviewBuffer.Last"); return; }
    } else if (in_battle()) {
        // Battle mod mode: X = party HP.
        if (bindings::pad("pad.mode.battle.hp")) { run_action("Mod.AnnounceCharacterStatus"); g_state = RouterState::Normal; return; }
        if (bindings::pad("pad.mode.battle.turn")) { run_action("BattleParty.ActiveTurn"); g_state = RouterState::Normal; return; }
        if (bindings::pad("pad.mode.battle.next")) { run_action("BattleParty.Next"); return; }
        if (bindings::pad("pad.mode.battle.previous")) { run_action("BattleParty.Previous"); return; }
    } else {
        // Field mod mode: X = Gil, Y = location, A = vehicle, RB = puzzle guide, LB = stop it.
        if (bindings::pad("pad.mode.field.gil")) { run_action("Mod.AnnounceGilAmount"); g_state = RouterState::Normal; return; }
        if (bindings::pad("pad.mode.field.map")) { run_action("Mod.AnnounceCurrentMap"); g_state = RouterState::Normal; return; }
        if (bindings::pad("pad.mode.field.vehicle")) { global_hotkeys::announce_current_vehicle(); g_state = RouterState::Normal; return; }
        if (bindings::pad("pad.mode.field.puzzle")) { run_action("PuzzleGuide.Next"); g_state = RouterState::Normal; return; }
        if (bindings::pad("pad.mode.field.puzzle.stop")) { run_action("PuzzleGuide.Stop"); g_state = RouterState::Normal; return; }

        // With Stick Click Normalization on, the stick-click functions move here.
        if (ui::prefs::stick_click_normalization_enabled()) {
            if (bindings::pad("pad.mode.field.filter")) { run_action("Mod.TogglePathfindingFilter"); g_state = RouterState::Normal; return; }
            if (bindings::pad("pad.mode.field.beacon")) { run_action("Mod.ToggleAudioBeacons"); g_state = RouterState::Normal; return; }
        }

        // Right stick: teleport (field only).
        if (ui::prefs::teleport_shortcuts_enabled()) {
            if (bindings::pad("pad.mode.field.teleport.north")) { run_action("Mod.TeleportNorth"); g_state = RouterState::Normal; return; }
            if (bindings::pad("pad.mode.field.teleport.south")) { run_action("Mod.TeleportSouth"); g_state = RouterState::Normal; return; }
            if (bindings::pad("pad.mode.field.teleport.west")) { run_action("Mod.TeleportWest"); g_state = RouterState::Normal; return; }
            if (bindings::pad("pad.mode.field.teleport.east")) { run_action("Mod.TeleportEast"); g_state = RouterState::Normal; return; }
        }
    }

    // Right stick down: mod mode controls (in the field it teleports south above).
    if (in_battle() && bindings::pad("pad.mode.battle.help"))
        announce_mod_mode_controls();
}

/// Mod menu: the controller drives the menu; all game input is suppressed.
void handle_mod_menu_state()
{
    consume_all();

    if (!ui::mod_menu::is_open()) {
        g_state = RouterState::Normal;
        return;
    }

    const bool up = bindings::pad("pad.menu.previous") || bindings::pad("pad.menu.previous.stick");
    const bool down = bindings::pad("pad.menu.next") || bindings::pad("pad.menu.next.stick");
    const bool left = bindings::pad("pad.menu.decrease") || bindings::pad("pad.menu.decrease.stick");
    const bool right = bindings::pad("pad.menu.increase") || bindings::pad("pad.menu.increase.stick");

    if (up) ui::mod_menu::navigate_previous();
    if (down) ui::mod_menu::navigate_next();
    if (left) ui::mod_menu::adjust_current_item(-1);
    if (right) ui::mod_menu::adjust_current_item(1);

    if (bindings::pad("pad.menu.select")) ui::mod_menu::toggle_current_item();
    if (bindings::pad("pad.menu.back")) ui::mod_menu::back();

    // Right stick down: mod menu controls.
    if (bindings::pad("pad.menu.help")) announce_mod_menu_controls();
}

std::wstring text_state()
{
    switch (g_state) {
    case RouterState::ModMode: return L"ModMode";
    case RouterState::ModMenu: return L"ModMenu";
    default: return L"Normal";
    }
}

int install()
{
    bindings::add("pad.menu.toggle","Open or close mod menu",true,-10,kStart,-1);
    bindings::add("pad.mode.toggle","Toggle controller mod mode",true,-10,kBack,-1);
    bindings::add("pad.field.waypoint.previous","Previous waypoint",true,-11,kDpadUp,-1);
    bindings::add("pad.field.waypoint.next","Next waypoint",true,-11,kDpadDown,-1);
    bindings::add("pad.field.waypoint.category.previous","Previous waypoint category",true,-11,kDpadLeft,-1);
    bindings::add("pad.field.waypoint.category.next","Next waypoint category",true,-11,kDpadRight,-1);
    bindings::add("pad.field.entity.previous","Previous object",true,-11,bindings::kRightUp,-1);
    bindings::add("pad.field.entity.next","Next object",true,-11,bindings::kRightDown,-1);
    bindings::add("pad.field.entity.category.previous","Previous object category",true,-11,bindings::kRightLeft,-1);
    bindings::add("pad.field.entity.category.next","Next object category",true,-11,bindings::kRightRight,-1);
    bindings::add("pad.field.route","Navigate to selected destination",true,-11,bindings::kLeftTrigger,-1);
    bindings::add("pad.other.details","Read item details",true,-12,bindings::kRightUp,-1);
    bindings::add("pad.other.help","Read controls",true,-12,bindings::kRightDown,-1);
    bindings::add("pad.other.previous","Previous information line",true,-12,kDpadUp,-1);
    bindings::add("pad.other.next","Next information line",true,-12,kDpadDown,-1);
    bindings::add("pad.other.previous.stick","Previous information line (stick)",true,-12,bindings::kLeftUp,-1);
    bindings::add("pad.other.next.stick","Next information line (stick)",true,-12,bindings::kLeftDown,-1);
    bindings::add("pad.puzzle.position","Read puzzle position",true,-13,bindings::kRightUp,-1);
    bindings::add("pad.mode.dialogue.repeat","Repeat dialogue",true,-14,kWest,-1);
    bindings::add("pad.mode.results.details","Read result details",true,-15,kWest,-1);
    bindings::add("pad.mode.results.next","Next result line",true,-15,kRightShoulder,-1);
    bindings::add("pad.mode.results.next.dpad","Next result line (D-pad)",true,-15,kDpadDown,-1);
    bindings::add("pad.mode.results.previous","Previous result line",true,-15,kLeftShoulder,-1);
    bindings::add("pad.mode.results.previous.dpad","Previous result line (D-pad)",true,-15,kDpadUp,-1);
    bindings::add("pad.mode.results.first","First result line",true,-15,kDpadLeft,-1);
    bindings::add("pad.mode.results.last","Last result line",true,-15,kDpadRight,-1);
    bindings::add("pad.mode.battle.hp","Read party HP",true,-16,kWest,-1);
    bindings::add("pad.mode.battle.turn","Read active turn",true,-16,kNorth,-1);
    bindings::add("pad.mode.battle.next","Next party member",true,-16,kRightShoulder,-1);
    bindings::add("pad.mode.battle.previous","Previous party member",true,-16,kLeftShoulder,-1);
    bindings::add("pad.mode.field.gil","Read Gil",true,-17,kWest,-1);
    bindings::add("pad.mode.field.map","Read location",true,-17,kNorth,-1);
    bindings::add("pad.mode.field.vehicle","Read vehicle",true,-17,kSouth,-1);
    bindings::add("pad.mode.field.puzzle","Puzzle guide next step",true,-17,kRightShoulder,-1);
    bindings::add("pad.mode.field.puzzle.stop","Stop puzzle guide",true,-17,kLeftShoulder,-1);
    bindings::add("pad.mode.field.filter","Toggle pathfinding filter",true,-17,kRightStick,-1);
    bindings::add("pad.mode.field.beacon","Toggle audio beacon",true,-17,kLeftStick,-1);
    bindings::add("pad.mode.field.teleport.north","Teleport north",true,-17,bindings::kRightUp,-1);
    bindings::add("pad.mode.field.teleport.south","Teleport south",true,-17,bindings::kRightDown,-1);
    bindings::add("pad.mode.field.teleport.west","Teleport west",true,-17,bindings::kRightLeft,-1);
    bindings::add("pad.mode.field.teleport.east","Teleport east",true,-17,bindings::kRightRight,-1);
    bindings::add("pad.mode.battle.help","Read mod mode controls",true,-16,bindings::kRightDown,-1);
    bindings::add("pad.menu.previous","Previous mod setting",true,-18,kDpadUp,-1);
    bindings::add("pad.menu.next","Next mod setting",true,-18,kDpadDown,-1);
    bindings::add("pad.menu.decrease","Decrease mod setting",true,-18,kDpadLeft,-1);
    bindings::add("pad.menu.increase","Increase mod setting",true,-18,kDpadRight,-1);
    bindings::add("pad.menu.previous.stick","Previous mod setting (stick)",true,-18,bindings::kLeftUp,-1);
    bindings::add("pad.menu.next.stick","Next mod setting (stick)",true,-18,bindings::kLeftDown,-1);
    bindings::add("pad.menu.decrease.stick","Decrease mod setting (stick)",true,-18,bindings::kLeftLeft,-1);
    bindings::add("pad.menu.increase.stick","Increase mod setting (stick)",true,-18,bindings::kLeftRight,-1);
    bindings::add("pad.menu.select","Select mod setting",true,-18,kSouth,-1);
    bindings::add("pad.menu.back","Back in mod menu",true,-18,kEast,-1);
    bindings::add("pad.menu.help","Read mod setting help",true,-18,bindings::kRightDown,-1);
    bindings::add("pad.field.beacon","Toggle audio beacon",true,-11,kLeftStick,-1);
    bindings::add("pad.field.filter","Toggle pathfinding filter",true,-11,kRightStick,-1);
    bindings::add("pad.field.normalize","Toggle stick click normalization",true,-11,kRightStick,kLeftStick);
    queries::define("ControllerRouter.IsFieldActive", [] { return g_field_active; });
    queries::define("ControllerRouter.SuppressGameInput", [] { return suppress_game_input(); });
    queries::define("ControllerRouter.IsNormal", [] { return g_state == RouterState::Normal; });
    queries::define("ControllerRouter.IsModMode", [] { return g_state == RouterState::ModMode; });
    queries::define("ControllerRouter.IsModMenu", [] { return g_state == RouterState::ModMenu; });
    queries::define("ControllerRouter.LastDeviceIsController", [] { return g_last_device == LastDevice::Controller; });
    queries::define_text("ControllerRouter.State", [] { return text_state(); });
    queries::define_text("ControllerRouter.CurrentGameContext", [] { return std::wstring(context_name(g_game_context)); });
    queries::define("GamepadManager.IsAvailable", [] { return gamepad_available(); });
    queries::define("GamepadManager.IsSdlReady", [] { return sdl_ready(); });

    actions::define("ControllerRouter.AnnounceContextControls", [] { announce_context_controls(); });
    actions::define("ControllerRouter.SpeakModMenuUnavailable", [] { speak_mod_menu_unavailable(); });
    actions::define("ControllerRouter.SyncWithModMenuOpen", [] { sync_with_mod_menu(true); });
    actions::define("ControllerRouter.SyncWithModMenuClosed", [] { sync_with_mod_menu(false); });
    actions::define("ControllerRouter.Reset", [] { router::reset(); });
    return 0;
}
core::Install s_install{"ControllerRouter", &install, 60};

} // namespace

// --- public ---------------------------------------------------------------------------------------
RouterState router_state() { return g_state; }
bool field_active() { return g_field_active; }

/// True when the game should receive no input at all.
bool suppress_game_input()
{
    return g_state == RouterState::ModMode
        || g_state == RouterState::ModMenu
        || ui::any_modal_open();   // ModMenu.IsOpen || TextInputWindow.IsOpen || ConfirmationDialog.IsOpen
}

/// Keeps the router in step with the mod menu however it was opened or closed (F8 / Escape /
/// "Close Menu" on the keyboard, Start / B on the controller). Opening puts the router in
/// ModMenu (the D-pad, stick and LT drive the menu instead of acting underneath it); closing
/// returns it to Normal.
void sync_with_mod_menu(bool menu_open)
{
    if (menu_open) g_state = RouterState::ModMenu;
    else if (g_state == RouterState::ModMenu) g_state = RouterState::Normal;
}

/// Why the mod menu cannot open. Shared by the Start button and F8.
void speak_mod_menu_unavailable()
{
    std::wstring reason;
    if (in_battle()) reason = T(L"Unavailable in battle");
    else if (menu_state::any_active()) reason = T(L"Unavailable in menu");
    else reason = T(L"Unavailable here");
    ffpr::say(reason, true);
}

/// RB (controller) or Shift+I (keyboard): mod controls in mod mode / the mod menu, the game's
/// own key help everywhere else.
void announce_context_controls()
{
    if (g_state == RouterState::ModMode) announce_mod_mode_controls();
    else if (g_state == RouterState::ModMenu || ui::mod_menu::is_open()) announce_mod_menu_controls();
    else run_action("KeyHelpReader.AnnounceKeyHelp");
}

namespace router {

void update(Context game_context)
{
    g_game_context = game_context;

    if (queries::is("GameStatePatches.IsInputTransitionBlocked")) {
        g_field_active = false;
        release_controller_owned_state();
        if (ui::mod_menu::is_open()) g_state = RouterState::ModMenu;
        g_left_trigger_was_active = left_trigger() > 0.5f;
        g_was_left_stick_active = left_stick_x() != 0.f || left_stick_y() != 0.f;
        return; // clear pending LT/stick-click gestures without firing an action
    }

    // Field-active state, once per frame (the single source of truth). Runs without a gamepad
    // too: audio suppression needs it for keyboard-only players.
    g_field_active = game_context == Context::Field && !menu_state::any_active() && !in_battle();

    if (!gamepad_available()) {
        // Gamepad gone (unplugged or never present): a leftover mod mode must not keep
        // SuppressGameInput set and lock the keyboard out of the game.
        release_controller_owned_state();
        return;
    }

    for (int i = 0; i < kButtonCount; ++i) {
        if (button_pressed(i)) { g_last_device = LastDevice::Controller; break; }
    }

    // Every button starts as "pass to game".
    std::memset(g_consumed, 0, sizeof g_consumed);

    if(bindings::capturing() || ui::confirmation::is_open() || ui::text_input::is_open()) {consume_all();return;}
    const auto old_state=g_state;
    handle_state_transitions();
    if(old_state!=g_state)return;
    update_stick_clicks();

    switch (g_state) {
    case RouterState::Normal: handle_normal_state(game_context); break;
    case RouterState::ModMode: handle_mod_mode_state(); break;
    case RouterState::ModMenu: handle_mod_menu_state(); break;
    }
}

void reset()
{
    g_state = RouterState::Normal;
    std::memset(g_consumed, 0, sizeof g_consumed);
}

void notify_keyboard_input() { g_last_device = LastDevice::Keyboard; }

bool is_button_consumed(int b) { return bindings::consumed(b) || (b >= 0 && b < kButtonCount && g_consumed[b]); }
bool is_stick_pulse_down(int b) { return b >= 0 && b == g_pulse_button && g_pulse_down_phase; }
bool is_stick_pulse_up(int b) { return b >= 0 && b == g_pulse_button && !g_pulse_down_phase; }

} // namespace router
} // namespace input
