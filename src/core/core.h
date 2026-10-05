#pragma once
#include <cstdint>
#include <functional>
#include <string>
#include <vector>
#include "game.h"

// The core's registries: what a ported reader plugs into.
//
//  * core::Install   - a reader's install function, registered at file scope. The boot thread
//                      runs every Install that applies to the running game, in `order`.
//  * actions         - named actions (a feature exposes "CycleNext"; input binds keys to the
//                      name). This keeps input and the readers apart: a binding to an action
//                      no game module defines logs "not available" instead of failing to link.
//  * menu_state      - FF1's MenuStateRegistry: which menu/screen is active, with reset handlers.
//  * events          - core reset points (menu open/close, battle start/end, map change,
//                      scene load) that readers and the deduplicator subscribe to.
//
// Registration objects must be file-scope statics in a source file that is part of the DLL.
// The core and games are CMake OBJECT libraries, so the linker never drops an object file
// that is only reachable through one of these registrations.
namespace core {

struct Install {
    const char* name;
    int (*fn)();          // returns hooks installed (or 0); must not throw
    int order;            // lower runs first; core infrastructure 0-49, readers 100+, games 500+
    uint8_t games;        // game::kAll, or game::kFF1 | ...
    Install* next = nullptr;
    Install(const char* name_, int (*fn_)(), int order_ = 100, uint8_t games_ = game::kAll);
};

/// Run every registered Install for the running game. Returns the total hooks reported.
int install_all();

} // namespace core

namespace actions {

using Fn = std::function<void()>;
/// Define (or replace) a named action. Main thread or boot thread.
void define(const char* name, Fn fn);
/// Run a named action; false and a log line when nothing defines it for this game.
bool run(const char* name);
bool defined(const char* name);

} // namespace actions

// Named state queries, the read-only twin of actions: an area that owns a state publishes it
// ("MessageWindowPatches.IsInDialogue", "BattleStateHelper.IsInBattle", "ModMenu.IsOpen") and
// any other area asks by name without including that area's headers. Same naming as actions:
// the C# member it ports. An undefined query answers false / empty.
namespace queries {

void define(const char* name, std::function<bool()> fn);
bool is(const char* name);
bool defined(const char* name);
void define_text(const char* name, std::function<std::wstring()> fn);
std::wstring text(const char* name);

} // namespace queries

namespace menu_state {

// The keys FF1 uses (MenuStateRegistry.cs). Other games add their own string keys.
constexpr const char* BATTLE_COMMAND = "BattleCommand";
constexpr const char* BATTLE_TARGET = "BattleTarget";
constexpr const char* BATTLE_ITEM = "BattleItem";
constexpr const char* BATTLE_MAGIC = "BattleMagic";
constexpr const char* CONFIG_MENU = "ConfigMenu";
constexpr const char* EQUIP_MENU = "EquipMenu";
constexpr const char* ITEM_MENU = "ItemMenu";
constexpr const char* MAGIC_MENU = "MagicMenu";
constexpr const char* SAVE_LOAD_MENU = "SaveLoadMenu";
constexpr const char* SHOP_MENU = "ShopMenu";
constexpr const char* STATUS_MENU = "StatusMenu";
constexpr const char* STATUS_DETAILS = "StatusDetails";
constexpr const char* POPUP = "Popup";
constexpr const char* EVENT_ITEM_SELECT = "EventItemSelect";
constexpr const char* BESTIARY_LIST = "BestiaryList";
constexpr const char* BESTIARY_DETAIL = "BestiaryDetail";
constexpr const char* BESTIARY_FORMATION = "BestiaryFormation";
constexpr const char* BESTIARY_MAP = "BestiaryMap";
constexpr const char* MUSIC_PLAYER = "MusicPlayer";
constexpr const char* GALLERY = "Gallery";
constexpr const char* MAIN_MENU = "MainMenu";
constexpr const char* PUZZLE_GAME = "PuzzleGame";

void set_active(const char* key, bool active);
/// Activate `key` and reset every other state (their reset handlers run).
void set_active_exclusive(const char* key);
bool is_active(const char* key);
bool any_active();
std::vector<std::string> active_states();
void register_reset(const char* key, std::function<void()> handler);
void reset(const char* key);
void reset_all();
void reset_all_except(const char* key);

} // namespace menu_state

namespace events {

enum class Kind : uint8_t {
    SceneLoaded,      // a Unity scene finished loading (arg: none)
    MapChanged,       // field map id changed
    BattleStart,
    BattleEnd,
    MenuOpened,
    MenuClosed,
    DialogueClosed,
    TitleScreen,
    Count
};

using Handler = std::function<void()>;
void on(Kind k, Handler h);
/// Fire an event: runs handlers in registration order and the matching dedup reset.
void fire(Kind k);

} // namespace events
