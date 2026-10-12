// Ports from FF1 Core/FFI_ScreenReaderMod.cs: the setting toggles (#region Filter Toggles:
// TogglePathfindingFilter, ToggleMapExitFilter, ToggleToLayerFilter, ToggleWallTones,
// ToggleFootsteps, ToggleAudioBeacons, ToggleAutoDetail, ToggleAnnounceOnBeaconRestart,
// ToggleMenuPositionAnnouncements, ToggleExpCounter, ToggleStickClickNormalization,
// SaveAndAnnounce) and #region Game Information (AnnounceGilAmount, AnnounceCharacterStatus).
//
// Each toggle flips its setting, announces "<feature> on/off", then runs
// "Settings.Changed.<Key>" when an area defines it (the side effects FF1 ran inline: entity
// scanner filters, wall-tone / beacon loops, stopping the EXP tick). The mod menu's toggles run
// the same notification, so both paths share the side effects.
//
// AnnounceCharacterStatus reads FF1's BattleCommandState.CurrentActor. That state belongs to
// the battle area and is an object, which a query cannot carry, so it is tracked here with the
// same rule: set by a postfix on KeyInput.BattleCommandSelectController.SetCommandData (one RVA
// in every dump.cs; a second subscriber to the battle area's hook), cleared when the
// BattleCommand menu state goes inactive (FF1's reset handler), at battle start and at battle
// end. MagicMenuState.GetConditionName is copied here for the status names.
#include <cstdint>
#include <string>
#include <vector>
#include "input_internal.h"
#include "../battle/battle.h"
#include "../core.h"
#include "../hooks.h"
#include "../refs.h"
#include "../unity.h"
#include "../say.h"
#include "../modtext.h"
#include "../text.h"
#include "../log.h"
#include "../ui/preferences.h"

namespace input {
namespace {

namespace prefs = ui::prefs;

// --- game data --------------------------------------------------------------------------------
ref::Class  kUserDataManager{"Last.Management", "UserDataManager"};
ref::Method mUdmInstance{kUserDataManager, "Instance", 0};
ref::Method mUdmGetOwendGil{kUserDataManager, "get_OwendGil", 0};

void* user_data_manager()
{
    il2cpp::Method* m = mUdmInstance.get();
    return m ? il2cpp::invoke_ptr(m, nullptr, nullptr, 0) : nullptr;
}

// --- Game Information ----------------------------------------------------------------------------
void announce_gil_amount()
{
    void* udm = user_data_manager();
    if (udm) {
        const int32_t gil = ref::call<int32_t>(mUdmGetOwendGil, udm);
        ffpr::say(TF(L"{0} Gil", {std::to_wstring(gil)}), true);
        return;
    }
    ffpr::say(T(L"Gil not available"), true);
}

void announce_character_status() { ffpr::battle::announce_active_status(); }

// --- Filter Toggles ------------------------------------------------------------------------------
struct Toggle {
    const char* action;      // "Mod.ToggleX"
    const char* key;         // settings key
    bool def;                // FF1 default
    const wchar_t* feature;  // mod_text key of the spoken name
};

const Toggle kToggles[] = {
    {"Mod.ToggleMapExitFilter", prefs::kMapExitFilter, prefs::kMapExitFilterDefault, L"Map exit filter"},
    {"Mod.ToggleToLayerFilter", prefs::kToLayerFilter, prefs::kToLayerFilterDefault, L"Layer transition filter"},
    {"Mod.ToggleWallTones", prefs::kWallTones, prefs::kWallTonesDefault, L"Wall tones"},
    {"Mod.ToggleFootsteps", prefs::kFootsteps, prefs::kFootstepsDefault, L"Footsteps"},
    {"Mod.ToggleAudioBeacons", prefs::kAudioBeacons, prefs::kAudioBeaconsDefault, L"Beacon navigation"},
    {"Mod.ToggleAutowalk", prefs::kAutowalk, prefs::kAutowalkDefault, L"Autowalk"},
    {"Mod.ToggleSpeakAutowalkPath", prefs::kSpeakAutowalkPath, prefs::kSpeakAutowalkPathDefault, L"Speak autowalk path"},
    {"Mod.ToggleBeaconWithDirections", prefs::kBeaconWithDirections, prefs::kBeaconWithDirectionsDefault, L"Beacon with directions"},
    {"Mod.ToggleAutoDetail", prefs::kAutoDetail, prefs::kAutoDetailDefault, L"Auto detail"},
    {"Mod.ToggleAnnounceOnBeaconRestart", prefs::kAnnounceOnBeaconRestart, prefs::kAnnounceOnBeaconRestartDefault, L"Beacon destination announcement"},
    {"Mod.ToggleMenuPositionAnnouncements", prefs::kMenuPositionAnnouncements, prefs::kMenuPositionAnnouncementsDefault, L"Menu position announcements"},
    {"Mod.ToggleExpCounter", prefs::kExpCounter, prefs::kExpCounterDefault, L"EXP counter sound"},
    {"Mod.ToggleStickClickNormalization", prefs::kStickClickNormalization, prefs::kStickClickNormalizationDefault, L"Stick click normalization"},
};

void run_toggle(const Toggle& t)
{
    const bool v = !settings::get_bool(t.key, t.def);
    settings::set_bool(t.key, v);
    save_and_announce(T(t.feature), v);
    prefs::notify_changed(t.key);
}

/// The pathfinding filter cycles its three modes (user, 2026-10-11): current floor, current
/// map with vehicles, current map. The old on/off value seeds the first cycle.
void cycle_pathfinding_filter()
{
    int mode = settings::get_int(prefs::kPathfindingFilterMode, -1);
    if (mode < 0) mode = settings::get_bool(prefs::kPathfindingFilter, prefs::kPathfindingFilterDefault) ? prefs::kFilterCurrentFloor : prefs::kFilterCurrentMap;
    mode = (mode + 1) % (prefs::kPathfindingFilterModeMax + 1);
    settings::set_int(prefs::kPathfindingFilterMode, mode);
    ffpr::say(T(L"Pathfinding filter") + L": " + T(prefs::kPathfindingFilterModeNames[mode]));
    prefs::notify_changed(prefs::kPathfindingFilterMode);
}

int install()
{
    for (const Toggle& t : kToggles) {
        const Toggle* p = &t;
        actions::define(t.action, [p] { run_toggle(*p); });
    }
    actions::define("Mod.TogglePathfindingFilter", [] { cycle_pathfinding_filter(); });
    actions::define("Mod.AnnounceGilAmount", [] { announce_gil_amount(); });
    actions::define("Mod.AnnounceCharacterStatus", [] { announce_character_status(); });

    return 0;
}
core::Install s_install{"ModActions", &install, 100};

} // namespace

void save_and_announce(const std::wstring& feature, bool value)
{
    ffpr::say(feature + L" " + (value ? T(L"on") : T(L"off")));
}

} // namespace input
