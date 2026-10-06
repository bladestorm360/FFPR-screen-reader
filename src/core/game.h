#pragma once
#include <cstdint>

// Which game this process is, and the contract a game module fills in.
//
// Detection compares the whole exe name, because "FINAL FANTASY" is the start of every other
// name. The product name in <exe>_Data\app.info is the second check. An unknown host gets one
// spoken line and the mod stays passive.
namespace game {

enum class Id : uint8_t { Unknown = 0, FF1 = 1, FF2 = 2, FF3 = 3, FF4 = 4, FF5 = 5 };

/// Bit for one game in a game mask (ref::Class::games, hooks, feature tables).
constexpr uint8_t bit(Id g) { return static_cast<uint8_t>(1u << static_cast<uint8_t>(g)); }
constexpr uint8_t kFF1 = 1u << 1, kFF2 = 1u << 2, kFF3 = 1u << 3, kFF4 = 1u << 4, kFF5 = 1u << 5;
constexpr uint8_t kAll = kFF1 | kFF2 | kFF3 | kFF4 | kFF5;

/// Run once on the boot thread, before anything else asks. Reads the exe name only.
Id detect();
Id current();
inline bool is(Id g) { return current() == g; }
inline bool in(uint8_t mask) { return (mask & bit(current())) != 0; }

/// "ff1".."ff5" (data and resource names), "FF1".."FF5" (log), or "unknown".
const char* key();
const char* label();
/// 1..5, or 0 when unknown.
int number();

/// The game folder with a trailing backslash ("D:\...\FINAL FANTASY PR\").
const wchar_t* dir();

/// True when the second check (app.info product name) agrees with the exe name.
bool product_confirmed();

// --- the game module contract -----------------------------------------------------------
//
// Each game is a static library under src/games/ffN that defines one Module. The core never
// includes a game's headers (CMake gives src/core no include path to src/games), so shared
// code cannot call into one game: it asks the Flags instead.
struct Flags {
    // Navigation design: FF1-FF3 use the entity scanner; FF4-FF5 the entity cache. The core
    // keeps one design (FF1's) and these switch the per-game differences in data, not code.
    bool has_vehicle_pads = false;      // merged vehicle trigger pads (FF2, FF4, FF5)
    bool has_hidden_passages = false;   // hidden_passage route layer (FF3, FF4, FF5)
    bool has_airship = false;           // FieldAirShip exists (FF1, FF2, FF4)
    bool multi_hit_setting = true;      // "Multi-hit Damage" setting (FF1-FF3; removed in FF4/FF5)
    bool atb = false;                   // active-time battle (FF4, FF5)
    bool has_jobs = false;              // job system (FF3, FF5)
    bool has_skill_levels = false;      // FF2 weapon/magic skill levels
    bool has_mp = false;                // MP shown with HP (FF2, FF4, FF5; FF1/FF3 use spell charges)
    bool timer_freeze = false;          // FF5 Shift+T timer freeze
};

struct Module {
    Id id;
    const char* name;            // "FINAL FANTASY" .. "FINAL FANTASY V"
    const char* settings_category;  // old MelonPreferences category, for the one-time import
    const char* old_mod_dll;     // "FFI_ScreenReader.dll" .. warned about if still in Mods
    /// Before any hook: set flags, switch off core hooks this game must not have.
    void (*configure)(Flags& flags);
    /// After the core's hooks: install this game's own hooks. Returns the number installed.
    int (*install)();
};

/// The module for the running game, or null when unknown. Set once by dllmain.
const Module* module();
void set_module(const Module* m);
const Flags& flags();
Flags& flags_mut();

} // namespace game
