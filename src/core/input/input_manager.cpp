// Ports FF1 Core/InputManager.cs, Core/KeyBinding.cs and Core/KeyBindingRegistry.cs.
//
// The per-frame input tick (frame::subscribe; FF1's OnUpdate -> InputManager.Update): gamepad
// and keyboard poll, game-input suppression, the controller router, the footstep poll, the mod's
// modal windows, then the keyboard hotkeys through the declarative binding registry.
//
// Every binding runs a named action (docs/INPUT_BINDINGS.md lists them); contexts are read
// through named queries. FF1's lambdas with arguments are one action per argument
// (TeleportInDirection -> Mod.TeleportNorth/South/West/East).
//
// Core/Handlers/FieldInputHandler.cs is not ported: FF1 never calls it (dead code); every key
// it handled is in the registry below with the same behaviour.
#include <windows.h>
#include <algorithm>
#include <functional>
#include <string>
#include <vector>
#include "input_internal.h"
#include "../core.h"
#include "../frame.h"
#include "../refs.h"
#include "../unity.h"
#include "../say.h"
#include "../modtext.h"
#include "../log.h"
#include "../ui/mod_ui.h"
#include "../ui/preferences.h"

namespace input {
namespace {

// --- Unity names ---------------------------------------------------------------------------------
ref::Class  kFieldPlayerController{"Last.Map", "FieldPlayerController"};
ref::Field  fFieldPlayer{kFieldPlayerController, "fieldPlayer"};
ref::Class  kEventSystem{"UnityEngine.EventSystems", "EventSystem"};
ref::Method mEventSystemGetCurrent{kEventSystem, "get_current", 0};
ref::Method mGetCurrentSelectedGameObject{kEventSystem, "get_currentSelectedGameObject", 0};
ref::Class  kInputField{"UnityEngine.UI", "InputField"};

// --- KeyBinding / KeyBindingRegistry ----------------------------------------------------------------
struct Binding {
    Key key;
    Mod mod;
    Context ctx;
    std::function<void()> fn;
    std::string description;
};

struct KeyEntry {
    Key key;
    std::vector<Binding> list;   // most specific modifier first, then most specific context
};

std::vector<KeyEntry> g_bindings;   // in registration order of the first binding per key

KeyEntry* entry(Key key)
{
    for (KeyEntry& e : g_bindings)
        if (e.key == key) return &e;
    return nullptr;
}

/// KeyBindingRegistry.Register followed by FinalizeRegistration's order: CtrlShift > Ctrl >
/// Shift > None, then KeyHelp > BestiaryDetail > Status > Battle > Field > Global. Keeps the
/// list sorted on every insert so game modules can add keys after the core's.
void register_binding(Key key, Mod mod, Context ctx, std::function<void()> fn, const char* description)
{
    KeyEntry* e = entry(key);
    if (!e) {
        g_bindings.push_back(KeyEntry{key, {}});
        e = &g_bindings.back();
    }
    Binding b{key, mod, ctx, std::move(fn), description ? description : ""};
    auto before = [](const Binding& x, const Binding& y) {
        if (x.mod != y.mod) return static_cast<int>(x.mod) > static_cast<int>(y.mod);
        return static_cast<int>(x.ctx) > static_cast<int>(y.ctx);
    };
    auto pos = std::upper_bound(e->list.begin(), e->list.end(), b, before);
    e->list.insert(pos, std::move(b));
}

void reg(Key key, Mod mod, Context ctx, const char* action, const char* description)
{
    std::string name = action;
    register_binding(key, mod, ctx, [name] { actions::run(name.c_str()); }, description);
}

/// RegisterFieldOnly: off the field (menu, battle, title) the active context is never Field, so
/// the binding has no match and the key does nothing.
void reg_field(Key key, Mod mod, const char* action, const char* description)
{
    reg(key, mod, Context::Field, action, description);
}

/// KeyBindingRegistry.TryExecute: the first binding with exactly these modifiers whose context
/// is Global or the active one.
bool try_execute(Key key, Mod mods, Context active)
{
    KeyEntry* e = entry(key);
    if (!e) return false;
    for (const Binding& b : e->list) {
        if (b.mod != mods) continue;
        if (b.ctx != Context::Global && b.ctx != active) continue;
        b.fn();
        return true;
    }
    return false;
}

// --- binding bodies that were lambdas or private methods in FF1 -----------------------------------
/// Backslash / P: mark the entity as the navigation target, then restart the beacon or speak.
void announce_entity_or_restart_beacon()
{
    actions::run("NavigationTargetTracker.MarkEntity");
    if (ui::prefs::audio_beacons_enabled()) actions::run("Mod.RestartEntityBeacon");
    else actions::run("Mod.AnnounceCurrentEntity");
}

/// R: repeat the current dialogue / message.
void repeat_dialogue()
{
    if (queries::is("MessageWindowPatches.IsInDialogue"))
        actions::run("MessageWindowPatches.RepeatLastDialogue");
}

/// Tab: clears a STUCK in-battle flag (battle ended without the end hooks). Only when no live
/// BattleController exists, so a Tab press in a real battle never silences battle speech.
void handle_tab_key()
{
    if (!queries::is("BattleStateHelper.IsInBattle")) return;
    if (queries::is("BattleStateHelper.IsBattleControllerAlive")) {
        LOGF("[Battle] Tab: battle is live (BattleController active), battle state kept");
        return;
    }
    LOGF("[Battle] Tab: no live BattleController, clearing stuck battle state");
    actions::run("BattleStateHelper.ForceClearBattleState");
}

void initialize_bindings()
{
    // --- Status screen: arrow key navigation ---
    reg(Key::DownArrow, Mod::Ctrl, Context::Status, "StatusNavigationReader.JumpToBottom", "Jump to bottom stat");
    reg(Key::DownArrow, Mod::Shift, Context::Status, "StatusNavigationReader.JumpToNextGroup", "Jump to next stat group");
    reg(Key::DownArrow, Mod::None, Context::Status, "StatusNavigationReader.NavigateNext", "Next stat");
    reg(Key::UpArrow, Mod::Ctrl, Context::Status, "StatusNavigationReader.JumpToTop", "Jump to top stat");
    reg(Key::UpArrow, Mod::Shift, Context::Status, "StatusNavigationReader.JumpToPreviousGroup", "Jump to previous stat group");
    reg(Key::UpArrow, Mod::None, Context::Status, "StatusNavigationReader.NavigatePrevious", "Previous stat");

    // --- Bestiary detail: arrow key navigation ---
    reg(Key::DownArrow, Mod::Ctrl, Context::BestiaryDetail, "BestiaryNavigationReader.JumpToBottom", "Jump to bottom stat (bestiary)");
    reg(Key::DownArrow, Mod::Shift, Context::BestiaryDetail, "BestiaryNavigationReader.JumpToNextGroup", "Jump to next group (bestiary)");
    reg(Key::DownArrow, Mod::None, Context::BestiaryDetail, "BestiaryNavigationReader.NavigateNext", "Next stat (bestiary)");
    reg(Key::UpArrow, Mod::Ctrl, Context::BestiaryDetail, "BestiaryNavigationReader.JumpToTop", "Jump to top stat (bestiary)");
    reg(Key::UpArrow, Mod::Shift, Context::BestiaryDetail, "BestiaryNavigationReader.JumpToPreviousGroup", "Jump to previous group (bestiary)");
    reg(Key::UpArrow, Mod::None, Context::BestiaryDetail, "BestiaryNavigationReader.NavigatePrevious", "Previous stat (bestiary)");

    // --- Key help / controls display: arrow navigation (flat list, no groups) ---
    reg(Key::DownArrow, Mod::Ctrl, Context::KeyHelp, "KeyHelpReader.JumpToBottom", "Jump to last control");
    reg(Key::DownArrow, Mod::None, Context::KeyHelp, "KeyHelpReader.NavigateNext", "Next control");
    reg(Key::UpArrow, Mod::Ctrl, Context::KeyHelp, "KeyHelpReader.JumpToTop", "Jump to first control");
    reg(Key::UpArrow, Mod::None, Context::KeyHelp, "KeyHelpReader.NavigatePrevious", "Previous control");

    // --- Field: entity navigation (brackets + backslash) ---
    reg_field(Key::LeftBracket, Mod::Shift, "Mod.CyclePreviousCategory", "Previous entity category");
    reg_field(Key::LeftBracket, Mod::None, "Mod.CyclePrevious", "Previous entity");
    reg_field(Key::RightBracket, Mod::Shift, "Mod.CycleNextCategory", "Next entity category");
    reg_field(Key::RightBracket, Mod::None, "Mod.CycleNext", "Next entity");
    reg_field(Key::Backslash, Mod::Ctrl, "Mod.ToggleToLayerFilter", "Toggle layer filter");
    reg_field(Key::Backslash, Mod::Shift, "Mod.TogglePathfindingFilter", "Toggle pathfinding filter");
    register_binding(Key::Backslash, Mod::None, Context::Field, &announce_entity_or_restart_beacon,
                     "Announce current entity / restart beacon");

    // --- Field: manual entity rescan (backtick) ---
    reg_field(Key::BackQuote, Mod::None, "Mod.ForceEntityRescan", "Force entity rescan");

    // --- Field: pathfinding alternate keys (J/K/L/P) ---
    reg_field(Key::J, Mod::Shift, "Mod.CyclePreviousCategory", "Previous entity category (alt)");
    reg_field(Key::J, Mod::None, "Mod.CyclePrevious", "Previous entity (alt)");
    reg_field(Key::K, Mod::None, "Mod.AnnounceEntityOnly", "Announce entity name (alt)");
    reg_field(Key::L, Mod::Shift, "Mod.CycleNextCategory", "Next entity category (alt)");
    reg_field(Key::L, Mod::None, "Mod.CycleNext", "Next entity (alt)");
    reg_field(Key::P, Mod::Ctrl, "Mod.ToggleToLayerFilter", "Toggle layer filter (alt)");
    reg_field(Key::P, Mod::Shift, "Mod.TogglePathfindingFilter", "Toggle pathfinding filter (alt)");
    register_binding(Key::P, Mod::None, Context::Field, &announce_entity_or_restart_beacon,
                     "Announce current entity / restart beacon (alt)");

    // --- Field: waypoint keys ---
    reg(Key::Comma, Mod::Shift, Context::Field, "WaypointHandler.CyclePreviousCategory", "Previous waypoint category");
    reg(Key::Comma, Mod::None, Context::Field, "WaypointHandler.CyclePrevious", "Previous waypoint");
    reg(Key::Period, Mod::Ctrl, Context::Field, "WaypointHandler.RenameCurrentWaypoint", "Rename waypoint");
    reg(Key::Period, Mod::Shift, Context::Field, "WaypointHandler.CycleNextCategory", "Next waypoint category");
    reg(Key::Period, Mod::None, Context::Field, "WaypointHandler.CycleNext", "Next waypoint");
    reg(Key::Slash, Mod::CtrlShift, Context::Field, "WaypointHandler.ClearAllWaypointsForMap", "Clear all waypoints for map");
    reg(Key::Slash, Mod::Ctrl, Context::Field, "WaypointHandler.RemoveCurrentWaypoint", "Remove current waypoint");
    reg(Key::Slash, Mod::Shift, Context::Field, "WaypointHandler.AddNewWaypointWithNaming", "Add waypoint with name");
    reg(Key::Slash, Mod::None, Context::Field, "WaypointHandler.PathfindToCurrentWaypoint", "Pathfind to waypoint");

    // --- Field: teleport (Ctrl+Arrow); TeleportInDirection(Vector2(0,16)) etc. ---
    reg(Key::UpArrow, Mod::Ctrl, Context::Field, "Mod.TeleportNorth", "Teleport north");
    reg(Key::DownArrow, Mod::Ctrl, Context::Field, "Mod.TeleportSouth", "Teleport south");
    reg(Key::LeftArrow, Mod::Ctrl, Context::Field, "Mod.TeleportWest", "Teleport west");
    reg(Key::RightArrow, Mod::Ctrl, Context::Field, "Mod.TeleportEast", "Teleport east");

    // --- Global: info / announcements ---
    reg(Key::G, Mod::None, Context::Global, "Mod.AnnounceGilAmount", "Announce Gil");
    reg(Key::H, Mod::None, Context::Global, "Mod.AnnounceCharacterStatus", "Announce character status");
    reg(Key::M, Mod::Shift, Context::Global, "Mod.ToggleMapExitFilter", "Toggle map exit filter");
    reg(Key::M, Mod::None, Context::Global, "Mod.AnnounceCurrentMap", "Announce current map");
    register_binding(Key::Tab, Mod::None, Context::Global, &handle_tab_key, "Clear battle state fallback");

    // --- Global: V (movement state) ---
    register_binding(Key::V, Mod::None, Context::Global, &global_hotkeys::announce_current_vehicle, "Announce vehicle state");

    // --- Global: R (repeat current dialogue / message) ---
    register_binding(Key::R, Mod::None, Context::Global, &repeat_dialogue, "Repeat dialogue");

    // --- Global: Shift+I (context-aware controls) ---
    register_binding(Key::I, Mod::Shift, Context::Global, &announce_context_controls, "Announce controls");

    // --- Global: I (cascading menu priority) ---
    register_binding(Key::I, Mod::None, Context::Global, &global_hotkeys::handle_item_details_key, "Item details");

    // --- Global: U (usable-by classes) ---
    reg(Key::U, Mod::None, Context::Global, "UsableByAnnouncer.AnnounceForCurrentContext", "Usable by classes");

    // --- Field-only toggles ---
    reg_field(Key::Quote, Mod::None, "Mod.ToggleFootsteps", "Toggle footsteps");
    reg_field(Key::Semicolon, Mod::None, "Mod.ToggleWallTones", "Toggle wall tones");
    reg_field(Key::F6, Mod::None, "Mod.ToggleAudioBeacons", "Toggle audio beacons");

    // --- Field: puzzle guide (src/core/field/puzzle_guide.cpp) ---
    reg_field(Key::G, Mod::Shift, "PuzzleGuide.Next", "Puzzle guide: start, or the current step again");
    reg_field(Key::G, Mod::Ctrl, "PuzzleGuide.Stop", "Puzzle guide: stop");

    // --- Field-only category shortcuts ---
    reg_field(Key::K, Mod::Shift, "Mod.ResetToAllCategory", "Reset to All category");
    reg_field(Key::Equals, Mod::None, "Mod.CycleNextCategory", "Next entity category (global)");
    reg_field(Key::Minus, Mod::None, "Mod.CyclePreviousCategory", "Previous entity category (global)");
}

// --- context ---------------------------------------------------------------------------------------
// Per-frame scene-scan throttle: where no FieldPlayerController exists (title, battle intro,
// loading) the per-frame caller would otherwise run FindObjectOfType every frame.
constexpr uint64_t kFieldScanIntervalFrames = 30;
uint64_t g_last_field_scan_frame = 0;
bool g_field_scanned = false;
void* g_field_player_controller = nullptr;   // GameObjectCache entry; re-checked with alive()

bool is_on_valid_map(bool on_demand)
{
    void* pc = unity::alive(g_field_player_controller) ? g_field_player_controller : nullptr;
    if (!pc) {
        // A cache miss scans the scene. A key press scans at once; the per-frame caller at
        // most once every kFieldScanIntervalFrames frames.
        const uint64_t f = frame::count();
        if (!on_demand && g_field_scanned && f - g_last_field_scan_frame < kFieldScanIntervalFrames) return false;
        g_last_field_scan_frame = f;
        g_field_scanned = true;
        pc = unity::find_object(kFieldPlayerController);
        g_field_player_controller = pc;
    }
    return pc && ref::rd<void*>(pc, fFieldPlayer) != nullptr;
}

/// Both queries when both are defined, else whichever one is (tracker IsNavigationActive and
/// ValidateState: FF1 required both).
bool both(const char* a, const char* b)
{
    const bool da = queries::defined(a), db = queries::defined(b);
    if (da && db) return queries::is(a) && queries::is(b);
    if (da) return queries::is(a);
    if (db) return queries::is(b);
    return false;
}

Context determine_context(bool on_demand)
{
    if (queries::is("ReviewBuffer.IsResults")) return Context::Results;
    // The key-help / controls overlay takes priority while shown.
    if (queries::is("KeyHelpReader.IsScreenActive")) return Context::KeyHelp;
    if (both("StatusNavigationTracker.IsNavigationActive", "StatusNavigationTracker.ValidateState")) return Context::Status;
    if (both("BestiaryNavigationTracker.IsNavigationActive", "BestiaryNavigationTracker.ValidateState")) return Context::BestiaryDetail;
    if (queries::is("BattleStateHelper.IsInBattle")) return Context::Battle;
    // The shop (its tracker validates the controller's state each time): the number row reads
    // the party panel's equipment comparison. Global keys still match in this context.
    if (queries::is("ShopMenuTracker.IsShopMenuActive") && queries::is("ShopMenuTracker.ValidateState")) return Context::Shop;
    // Field keys only while on a field map with no menu open; otherwise Global, so the field
    // keys are silent and the global info keys still work.
    if (queries::is("GameStatePatches.IsFieldPlayerState") && is_on_valid_map(on_demand) && !menu_state::any_active()) return Context::Field;
    return Context::Global;
}

Mod current_modifiers()
{
    const bool shift = shift_held();
    const bool ctrl = ctrl_held();
    if (ctrl && shift) return Mod::CtrlShift;
    if (ctrl) return Mod::Ctrl;
    if (shift) return Mod::Shift;
    return Mod::None;
}

bool any_modifier_held() { return shift_held() || ctrl_held() || alt_held(); }

bool is_buffer_context(Context c)
{
    return c == Context::Status || c == Context::BestiaryDetail || c == Context::Results || c == Context::KeyHelp;
}

void dispatch_registered_bindings(Context active, Mod mods)
{
    for (size_t i = 0; i < g_bindings.size(); ++i) {
        const Key k = g_bindings[i].key;
        if (key_pressed(k)) try_execute(k, mods, active);
    }

    // WASD as arrow keys, ONLY in the navigation-buffer contexts, so game WASD movement and
    // letter hotkeys elsewhere are untouched. Modifiers carry (Shift+W = Shift+Up).
    if (is_buffer_context(active)) {
        if (key_pressed(Key::W)) try_execute(Key::UpArrow, mods, active);
        if (key_pressed(Key::S)) try_execute(Key::DownArrow, mods, active);
        if (key_pressed(Key::A)) try_execute(Key::LeftArrow, mods, active);
        if (key_pressed(Key::D)) try_execute(Key::RightArrow, mods, active);
    }
}

void handle_function_key_input()
{
    if (key_pressed(Key::F1)) actions::run("GameToggles.ReportSpeed");
    if (key_pressed(Key::F3)) actions::run("GameToggles.ReportEncounters");
    if (key_pressed(Key::F7)) actions::run("Mod.ToggleAutoDetail");

    if (key_pressed(Key::F5)) {
        if (queries::is("BattleStateHelper.IsInBattle")) {
            const int next = (std::clamp(ui::prefs::enemy_hp_display(), 0, 2) + 1) % 3;
            ui::prefs::set_enemy_hp_display(next);
            const std::wstring options[] = {T(L"Numbers"), T(L"Percentage"), T(L"Hidden")};
            ffpr::say(TF(L"Enemy HP: {0}", {options[next]}), true);
            ui::prefs::notify_changed(ui::prefs::kEnemyHPDisplay);
        } else {
            speak_mod_menu_unavailable();
        }
    }
}

bool is_input_field_focused()
{
    void* es = ref::call_static<void*>(mEventSystemGetCurrent);
    if (!unity::alive(es)) return false;
    void* go = ref::call<void*>(mGetCurrentSelectedGameObject, es);
    if (!unity::alive(go)) return false;
    return unity::get_component(go, kInputField) != nullptr;
}

// --- Input.ResetInputAxes ----------------------------------------------------------------------------
// An internal call. FF3's IL2CPP build stripped the managed wrapper (no ResetInputAxes in its
// dump.cs; the C# mod reached it through Il2CppInterop's unstripping), so it is resolved as
// an icall in every game.
using ResetInputAxes_t = void (*)();
ResetInputAxes_t g_reset_input_axes = nullptr;
bool g_reset_input_axes_tried = false;

void reset_input_axes()
{
    if (!g_reset_input_axes_tried) {
        g_reset_input_axes_tried = true;
        HMODULE ga = GetModuleHandleW(L"GameAssembly.dll");
        using Resolve_t = void* (*)(const char*);
        auto resolve = ga ? reinterpret_cast<Resolve_t>(GetProcAddress(ga, "il2cpp_resolve_icall")) : nullptr;
        if (resolve) g_reset_input_axes = reinterpret_cast<ResetInputAxes_t>(resolve("UnityEngine.Input::ResetInputAxes()"));
        if (!g_reset_input_axes) LOGF("[InputManager] UnityEngine.Input::ResetInputAxes icall not found");
    }
    if (!g_reset_input_axes) return;
    try { g_reset_input_axes(); } catch (...) { ref::call_failed_note("Input.ResetInputAxes"); }
}

// --- InputManager.Update ------------------------------------------------------------------------------
void update()
{
    // Gamepad and keyboard state for this frame.
    gamepad::update();

    // Unity legacy Input off while the mod consumes input (the mod reads the keyboard with
    // GetAsyncKeyState, unaffected). With the InputSystemManager hooks: no game keyboard at all.
    if (suppress_game_input()) reset_input_axes();

    // Controller routing and IsFieldActive first, every frame.
    const Context context = determine_context(false);
    router::update(context);

    // Per-frame footstep tile-crossing poll (field-active gated, silent in vehicles).
    if (actions::defined("MovementSoundPatches.PollFootsteps")) actions::run("MovementSoundPatches.PollFootsteps");


    // The mod's modal windows take all keyboard input while open.
    if (ui::handle_modal_input()) return;

    // Game-context hotkeys only while the game window is the foreground window.
    if (!game_window_focused()) return;

    if (!any_key_pressed()) return;

    router::notify_keyboard_input();

    // No hotkeys while the player types in a text field.
    if (is_input_field_focused()) return;

    // Bare hotkeys only without modifiers, so OS shortcuts (Alt+F4, Ctrl+F4) do not trigger
    // them. Shift+M, Shift+K etc. have their own registry entries.
    const bool modifier_held = any_modifier_held();

    // F8: mod menu, field only (ControllerRouter.IsFieldActive).
    if (!modifier_held && key_pressed(Key::F8)) {
        if (field_active()) ui::mod_menu::open();
        else speak_mod_menu_unavailable();
        return;
    }

    // F5 / F7, bare only.
    if (!modifier_held) handle_function_key_input();

    const Context active = determine_context(true);
    const Mod mods = current_modifiers();

    // Alt held: no Alt bindings exist, so Alt+U must not run U.
    if (alt_held()) return;

    dispatch_registered_bindings(active, mods);
}

void tick()
{
    update();
}

int install()
{
    initialize_bindings();
    frame::subscribe(&tick, "InputManager.Update");
    return 0;
}
core::Install s_install{"InputManager", &install, 60};

} // namespace

void add_binding(Key key, Mod mod, Context ctx, const char* action, const char* description)
{
    if (!action) return;
    reg(key, mod, ctx, action, description);
}

void shutdown()
{
    router::reset();
    gamepad::shutdown();
}

} // namespace input
