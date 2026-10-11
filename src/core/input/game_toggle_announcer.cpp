// Ports FF1 Core/Handlers/GameToggleAnnouncer.cs.
//
// Announces the two game-side field toggles when the player flips them on the field: random
// encounters and auto-dash (walk/run). Event-driven: hooks the CLIENT setters, which are unique
// bodies in all five games (one RVA each in every dump.cs; the data-class setter
// CheatSettingsData.set_IsEnableEncount is folded, so it is not used):
//  - Last.Management.CheatSettingsClient.SetIsEnableEncount(bool)
//  - Last.Management.ConfigClient.SetIsAutoDash(int)
// The prefix part records the old value; the postfix part speaks only for a real change made
// while the main game is in its Player state ("GameStatePatches.IsFieldPlayerState"), so a
// config-menu change or a save load stays silent.
#include <cstdint>
#include "input_internal.h"
#include "../core.h"
#include "../hooks.h"
#include "../refs.h"
#include "../say.h"
#include "../modtext.h"
#include "../frame.h"
#include "../unity.h"

namespace input {
namespace {

ref::Class  kUserDataManager{"Last.Management", "UserDataManager"};
ref::Method mUdmInstance{kUserDataManager, "Instance", 0};
ref::Method mUdmGetCheatSettingsData{kUserDataManager, "get_CheatSettingsData", 0};
ref::Method mUdmGetConfig{kUserDataManager, "get_Config", 0};
ref::Class  kCheatSettingsData{"Last.Data", "CheatSettingsData"};
ref::Field  fIsEnableEncount{kCheatSettingsData, "isEnableEncount"};
ref::Class  kConfig{"Last.Data.User", "Config"};
ref::Method mConfigGetIsAutoDash{kConfig, "get_IsAutoDash", 0};

ref::Class  kCheatSettingsClient{"Last.Management", "CheatSettingsClient"};
ref::Method mSetIsEnableEncount{kCheatSettingsClient, "SetIsEnableEncount", 1};
ref::Class  kConfigClient{"Last.Management", "ConfigClient"};
ref::Method mSetIsAutoDash{kConfigClient, "SetIsAutoDash", 1};
ref::Class kSaveMapManager{"Last.Map", "SaveMapManager"};
ref::Method mSaveMapInstance{kSaveMapManager, "get_Instance", 0};
ref::Field fSaveMapData{kSaveMapManager, "<SaveData>k__BackingField"};
ref::Class kSaveMapData{"Last.Map", "SaveMapData"};
ref::Method mEncountEnable{kSaveMapData, "get_EncountEnable", 0};
ref::Class kMapUI{"Last.Map", "MapUIManager"};
ref::Method mWorldMap{kMapUI, "CheckCurrentWorldMap", 0};
bool g_report_speed = false, g_report_encounters = false;

void* user_data_manager()
{
    il2cpp::Method* m = mUdmInstance.get();
    return m ? il2cpp::invoke_ptr(m, nullptr, nullptr, 0) : nullptr;
}

bool field_player_state() { return queries::is("GameStatePatches.IsFieldPlayerState"); }

bool read_encounters(bool& value)
{
    void* manager = ref::call_static<void*>(mSaveMapInstance);
    void* data = manager ? ref::rd<void*>(manager, fSaveMapData) : nullptr;
    if (!data || !mEncountEnable) return false;
    value = ref::call<bool>(mEncountEnable, data);
    return true;
}
bool read_speed(int32_t& value)
{
    void* udm = user_data_manager();
    void* cfg = udm ? ref::call<void*>(mUdmGetConfig, udm) : nullptr;
    if (!cfg || !mConfigGetIsAutoDash) return false;
    value = ref::call<int32_t>(mConfigGetIsAutoDash, cfg);
    return true;
}
void report_speed()
{
    if (!field_player_state()) return;
    int32_t before = 0;
    const bool had_before = read_speed(before);
    g_report_speed = true;
    // Input tick precedes native key processing. Read its committed result next frame.
    frame::next_frame([before, had_before] {
        g_report_speed = false;
        if (!field_player_state()) return;
        int32_t value = 0;
        if (!read_speed(value)) return;
        void* map_ui = ref::instance(kMapUI);
        if (had_before && value == before && map_ui && ref::call<bool>(mWorldMap, map_ui))
            ffpr::say(T(L"Cannot change speed on the world map"), true);
        else ffpr::say(value != 0 ? T(L"Run") : T(L"Walk"), true);
    }, "GameToggles.Speed");
}
void report_encounters()
{
    if (!field_player_state()) return;
    g_report_encounters = true;
    frame::next_frame([] {
        g_report_encounters = false;
        if (!field_player_state()) return;
        bool enabled = false;
        if (read_encounters(enabled)) ffpr::say(enabled ? T(L"Encounters on") : T(L"Encounters off"), true);
    }, "GameToggles.Encounters");
}

// --- Encounters (CheatSettingsClient.SetIsEnableEncount) ---
using SetEncount_t = void (*)(void* self, bool value, const void* mi);
SetEncount_t o_SetIsEnableEncount = nullptr;

void d_SetIsEnableEncount(void* self, bool value, const void* mi)
{
    // Prefix: the old value; unreadable -> stay silent.
    bool have_before = false;
    bool before = false;
    hooks::guarded("GameToggleAnnouncer.SetIsEnableEncount.prefix", [&] {
        void* udm = user_data_manager();
        void* cheat = udm ? ref::call<void*>(mUdmGetCheatSettingsData, udm) : nullptr;
        if (cheat && fIsEnableEncount.get() >= 0) {
            before = ref::rd<bool>(cheat, fIsEnableEncount);
            have_before = true;
        }
    });

    o_SetIsEnableEncount(self, value, mi);

    hooks::guarded("GameToggleAnnouncer.SetIsEnableEncount", [&] {
        if (g_report_encounters) return; // This keyboard event is read after native dispatch instead.
        if (!have_before || before == value) return;   // not a real change
        if (!field_player_state()) return;             // config menu / save load
        bool enabled = false;
        if (read_encounters(enabled)) ffpr::say(enabled ? T(L"Encounters on") : T(L"Encounters off"), true);
    });
}

// --- Auto-dash / walk-run (ConfigClient.SetIsAutoDash) ---
using SetAutoDash_t = void (*)(void* self, int32_t value, const void* mi);
SetAutoDash_t o_SetIsAutoDash = nullptr;

void d_SetIsAutoDash(void* self, int32_t value, const void* mi)
{
    bool have_before = false;
    int32_t before = 0;
    hooks::guarded("GameToggleAnnouncer.SetIsAutoDash.prefix", [&] {
        void* udm = user_data_manager();
        void* cfg = udm ? ref::call<void*>(mUdmGetConfig, udm) : nullptr;
        if (cfg && mConfigGetIsAutoDash) {
            before = ref::call<int32_t>(mConfigGetIsAutoDash, cfg);
            have_before = true;
        }
    });

    o_SetIsAutoDash(self, value, mi);

    hooks::guarded("GameToggleAnnouncer.SetIsAutoDash", [&] {
        if (g_report_speed) return;
        if (!have_before || (before != 0) == (value != 0)) return;   // not a real change
        if (!field_player_state()) return;                         // config menu
        int32_t actual = 0;
        if (read_speed(actual)) ffpr::say(actual != 0 ? T(L"Run") : T(L"Walk"), true);
    });
}

int install()
{
    actions::define("GameToggles.ReportSpeed", [] { report_speed(); });
    actions::define("GameToggles.ReportEncounters", [] { report_encounters(); });
    int n = 0;
    n += hooks::attach(mSetIsEnableEncount, &d_SetIsEnableEncount, &o_SetIsEnableEncount) ? 1 : 0;
    n += hooks::attach(mSetIsAutoDash, &d_SetIsAutoDash, &o_SetIsAutoDash) ? 1 : 0;
    return n;
}
core::Install s_install{"GameToggleAnnouncer", &install, 100};

} // namespace
} // namespace input
