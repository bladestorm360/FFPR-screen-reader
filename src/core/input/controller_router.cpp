// Ports FF1 Core/ControllerRouter.cs.
//
// The controller state machine: decides where each SDL input goes. The router consumes the
// buttons the mod uses; everything else passes to the game through input_passthrough.cpp.
//
// Not ported: FeedLeftStickToGame (unused in FF1, "kept for potential future use").
#include <cstring>
#include "input_internal.h"
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

void announce_mod_mode_controls()
{
    const std::wstring back = button_label(kBack);
    const std::wstring west = button_label(kWest);
    if (in_dialogue()) {
        ffpr::say(TF(L"{0} to repeat dialogue. {1} to cancel.", {west, back}), true);
    } else if (in_battle()) {
        ffpr::say(TF(L"{0} for party HP. {1} to cancel.", {west, back}), true);
    } else {
        const std::wstring north = button_label(kNorth);
        const std::wstring south = button_label(kSouth);
        ffpr::say(TF(L"{0} for Gil. {1} for location. {2} for vehicle. Right stick to teleport. {3} to cancel.",
                     {west, north, south, back}), true);
    }
}

void announce_mod_menu_controls()
{
    const std::wstring confirm = button_label(kSouth);
    const std::wstring close = button_label(kEast);
    const std::wstring start = button_label(kStart);
    ffpr::say(TF(L"D-pad or Left Stick Up, Down to navigate. Left, Right to adjust values. {0} to toggle. {1} or {2} to close.",
                 {confirm, close, start}), true);
}

void open_mod_menu() { ui::mod_menu::open(); }     // speaks; SyncWithModMenuOpen sets ModMenu
void close_mod_menu() { ui::mod_menu::close(); }   // speaks; SyncWithModMenuClosed sets Normal

void handle_state_transitions()
{
    // Start: mod menu toggle. Closing is always allowed; opening only on the active field.
    if (button_pressed(kStart)) {
        consume(kStart);
        if (g_state == RouterState::ModMenu) close_mod_menu();
        else if (g_field_active) open_mod_menu();
        else speak_mod_menu_unavailable();
        return;
    }

    // Back/Select: mod mode toggle (Normal <-> ModMode).
    if (button_pressed(kBack)) {
        consume(kBack);
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
        || gamepad::dpad_up_pressed() || gamepad::dpad_down_pressed()
        || gamepad::dpad_left_pressed() || gamepad::dpad_right_pressed()
        || gamepad::rstick_up_pressed() || gamepad::rstick_down_pressed()
        || gamepad::rstick_left_pressed() || gamepad::rstick_right_pressed();
    if (any_nav) ffpr::silence();

    // D-pad: waypoint navigation (consumed). The callees mark the navigation target.
    if (gamepad::dpad_up_pressed()) { consume(kDpadUp); run_action("WaypointHandler.CyclePrevious"); }
    if (gamepad::dpad_down_pressed()) { consume(kDpadDown); run_action("WaypointHandler.CycleNext"); }
    if (gamepad::dpad_left_pressed()) { consume(kDpadLeft); run_action("WaypointHandler.CyclePreviousCategory"); }
    if (gamepad::dpad_right_pressed()) { consume(kDpadRight); run_action("WaypointHandler.CycleNextCategory"); }

    // Right stick: entity scanner.
    if (gamepad::rstick_up_pressed()) run_action("Mod.CyclePrevious");
    if (gamepad::rstick_down_pressed()) run_action("Mod.CycleNext");
    if (gamepad::rstick_left_pressed()) run_action("Mod.CyclePreviousCategory");
    if (gamepad::rstick_right_pressed()) run_action("Mod.CycleNextCategory");

    // Left trigger: pathfind to the last selected target (or restart the beacon).
    if (left_trigger() > 0.5f && !g_left_trigger_was_active) {
        if (queries::is("NavigationTargetTracker.IsWaypoint")) {
            run_action("WaypointHandler.PathfindToCurrentWaypoint");
        } else if (queries::is("NavigationTargetTracker.IsEntity")) {
            if (ui::prefs::audio_beacons_enabled()) run_action("Mod.RestartEntityBeacon");
            else run_action("Mod.AnnounceCurrentEntity");
        } else {
            ffpr::say(T(L"No target selected"), true);
        }
    }
    g_left_trigger_was_active = left_trigger() > 0.5f;
}

void handle_normal_non_field(Context context)
{
    // Right stick up: read description (I key).
    if (gamepad::rstick_up_pressed()) global_hotkeys::handle_item_details_key();
    // Right stick down: read controls (Shift+I).
    if (gamepad::rstick_down_pressed()) run_action("KeyHelpReader.AnnounceKeyHelp");
    // Right stick left: usable by classes (U key).
    if (gamepad::rstick_left_pressed()) run_action("UsableByAnnouncer.AnnounceForCurrentContext");

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
    if (gamepad::dpad_up_pressed() || gamepad::lstick_up_pressed()) { consume(kDpadUp); run_action(prev); }
    if (gamepad::dpad_down_pressed() || gamepad::lstick_down_pressed()) { consume(kDpadDown); run_action(next); }
}

void handle_normal_state(Context context)
{
    // Face buttons interrupt queued speech, as Enter does on the keyboard.
    if (button_pressed(kSouth) || button_pressed(kEast) || button_pressed(kWest) || button_pressed(kNorth))
        ffpr::silence();

    // 15-puzzle: right stick up reads the cursor row/column, whatever the context.
    if (puzzle_active()) {
        if (gamepad::rstick_up_pressed()) run_action("PuzzlePatches.AnnouncePosition");
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
    // Retire the previous synthetic press: down -> up -> none.
    if (g_pulse_button >= 0) {
        if (g_pulse_down_phase) g_pulse_down_phase = false;
        else g_pulse_button = -1;
    }

    if (g_stick_click_button < 0) {
        const bool l3_down = button_pressed(kLeftStick);
        if (!l3_down && !button_pressed(kRightStick)) return;
        if (g_state != RouterState::Normal || !g_field_active || puzzle_active()) return;
        g_stick_click_button = l3_down ? kLeftStick : kRightStick;
        g_stick_chord_fired = false;
    }

    consume(kLeftStick);
    consume(kRightStick);

    const bool l3_held = button_held(kLeftStick);
    const bool r3_held = button_held(kRightStick);
    const bool can_act = g_state == RouterState::Normal && g_field_active;

    if (l3_held && r3_held) {
        if (!g_stick_chord_fired && can_act) {
            g_stick_chord_fired = true;
            run_action("Mod.ToggleStickClickNormalization");
        }
        return;
    }
    if (l3_held || r3_held) return;

    // Both up: the press is over.
    const int button = g_stick_click_button;
    g_stick_click_button = -1;
    if (g_stick_chord_fired || !can_act) return;

    if (ui::prefs::stick_click_normalization_enabled()) {
        g_pulse_button = button;
        g_pulse_down_phase = true;
    } else if (button == kLeftStick) {
        run_action("Mod.ToggleAudioBeacons");
    } else {
        run_action("Mod.TogglePathfindingFilter");
    }
}

/// Mod mode: face buttons give mod information, then the router returns to Normal.
void handle_mod_mode_state()
{
    consume_all();

    // Dialogue first: with a message window up the player wants the message again.
    if (in_dialogue()) {
        if (button_pressed(kWest)) { run_action("MessageWindowPatches.RepeatLastDialogue"); g_state = RouterState::Normal; return; }
    } else if (in_battle()) {
        // Battle mod mode: X = party HP.
        if (button_pressed(kWest)) { run_action("Mod.AnnounceCharacterStatus"); g_state = RouterState::Normal; return; }
    } else {
        // Field mod mode: X = Gil, Y = location, A = vehicle.
        if (button_pressed(kWest)) { run_action("Mod.AnnounceGilAmount"); g_state = RouterState::Normal; return; }
        if (button_pressed(kNorth)) { run_action("Mod.AnnounceCurrentMap"); g_state = RouterState::Normal; return; }
        if (button_pressed(kSouth)) { global_hotkeys::announce_current_vehicle(); g_state = RouterState::Normal; return; }

        // With Stick Click Normalization on, the stick-click functions move here.
        if (ui::prefs::stick_click_normalization_enabled()) {
            if (button_pressed(kRightStick)) { run_action("Mod.TogglePathfindingFilter"); g_state = RouterState::Normal; return; }
            if (button_pressed(kLeftStick)) { run_action("Mod.ToggleAudioBeacons"); g_state = RouterState::Normal; return; }
        }

        // Right stick: teleport (field only).
        if (gamepad::rstick_up_pressed()) { run_action("Mod.TeleportNorth"); g_state = RouterState::Normal; return; }
        if (gamepad::rstick_down_pressed()) { run_action("Mod.TeleportSouth"); g_state = RouterState::Normal; return; }
        if (gamepad::rstick_left_pressed()) { run_action("Mod.TeleportWest"); g_state = RouterState::Normal; return; }
        if (gamepad::rstick_right_pressed()) { run_action("Mod.TeleportEast"); g_state = RouterState::Normal; return; }
    }

    // Right stick down: mod mode controls (in the field it teleports south above).
    if (in_battle() && gamepad::rstick_down_pressed())
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

    const bool up = gamepad::dpad_up_pressed() || gamepad::lstick_up_pressed();
    const bool down = gamepad::dpad_down_pressed() || gamepad::lstick_down_pressed();
    const bool left = gamepad::dpad_left_pressed() || gamepad::lstick_left_pressed();
    const bool right = gamepad::dpad_right_pressed() || gamepad::lstick_right_pressed();

    if (up) ui::mod_menu::navigate_previous();
    if (down) ui::mod_menu::navigate_next();
    if (left) ui::mod_menu::adjust_current_item(-1);
    if (right) ui::mod_menu::adjust_current_item(1);

    if (button_pressed(kSouth)) ui::mod_menu::toggle_current_item();
    if (button_pressed(kEast)) close_mod_menu();

    // Right stick down: mod menu controls.
    if (gamepad::rstick_down_pressed()) announce_mod_menu_controls();
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

    handle_state_transitions();
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

bool is_button_consumed(int b) { return b >= 0 && b < kButtonCount && g_consumed[b]; }
bool is_stick_pulse_down(int b) { return b >= 0 && b == g_pulse_button && g_pulse_down_phase; }
bool is_stick_pulse_up(int b) { return b >= 0 && b == g_pulse_button && !g_pulse_down_phase; }

} // namespace router
} // namespace input
