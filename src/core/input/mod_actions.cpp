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

ref::Class  kBattleCommandSelectController{"Last.UI.KeyInput", "BattleCommandSelectController"};
ref::Method mSetCommandData{kBattleCommandSelectController, "SetCommandData", 3};

ref::Class  kOwnedCharacterData{"Last.Data.User", "OwnedCharacterData"};
ref::Method mOcdGetName{kOwnedCharacterData, "get_Name", 0};
ref::Method mOcdGetParameter{kOwnedCharacterData, "get_Parameter", 0};

ref::Class  kCharacterParameterBase{"Last.Data", "CharacterParameterBase"};
ref::Method mParamGetCurrentHP{kCharacterParameterBase, "get_CurrentHP", 0};
ref::Class  kPlayerCharacterParameter{"Last.Data", "PlayerCharacterParameter"};
ref::Method mParamConfirmedMaxHp{kPlayerCharacterParameter, "ConfirmedMaxHp", 0};   // override of the abstract base
ref::Method mParamGetCurrentConditionList{kCharacterParameterBase, "get_CurrentConditionList", 0};

ref::Class  kCondition{"Last.Data.Master", "Condition"};
ref::Method mConditionGetMesIdName{kCondition, "get_MesIdName", 0};
ref::Method mConditionGetConditionType{kCondition, "get_ConditionType", 0};

ref::Class  kMessageManager{"Last.Management", "MessageManager"};
ref::Method mGetMessage{kMessageManager, "GetMessage", 2};

void* user_data_manager()
{
    il2cpp::Method* m = mUdmInstance.get();
    return m ? il2cpp::invoke_ptr(m, nullptr, nullptr, 0) : nullptr;
}

// --- CurrentActor (BattleCommandState.CurrentActor) --------------------------------------------
void* g_current_actor = nullptr;
bool g_battle_command_was_active = false;

using SetCommandData_t = void (*)(void* self, void* data, void* targets, void* items, const void* mi);
SetCommandData_t o_SetCommandData = nullptr;

void d_SetCommandData(void* self, void* data, void* targets, void* items, const void* mi)
{
    o_SetCommandData(self, data, targets, items, mi);
    // Every SetCommandData for a player is that actor's input turn, a re-entry included.
    if (data) g_current_actor = data;
}

// --- MagicMenuState.GetConditionName -------------------------------------------------------------
std::wstring condition_type_fallback(int type)
{
    switch (type) {
    case 4: return T(L"Critical");          // Dying (low HP)
    case 5: return T(L"KO");                // UnableFight
    case 6: return T(L"Silence");
    case 7: return T(L"Sleep");
    case 8: return T(L"Paralysis");
    case 9: return T(L"Blind");
    case 10: return T(L"Poison");
    case 11: return T(L"Stone");            // Mineralization
    case 12: return T(L"Confusion");
    case 16: return T(L"Slow");
    case 17: return T(L"Stop");
    case 32: return T(L"Aging");
    case 34: return T(L"Zombie");
    case 204: return T(L"Venom");
    case 404: return T(L"Doom");
    case 405: return T(L"Gradual Petrify"); // SlowlyMineralization
    case 406: return T(L"Curse");
    default: return {};
    }
}

std::wstring condition_name(void* condition)
{
    if (!condition) return {};
    const std::wstring mes_id = ref::call_str(mConditionGetMesIdName, condition);
    const int type = ref::call<int32_t>(mConditionGetConditionType, condition);
    // Localize through MesIdName; MessageManager echoes an unresolved key with a "$".
    if (!mes_id.empty() && mes_id != L"None") {
        if (void* mm = ref::instance(kMessageManager)) {
            const std::wstring s = ref::call_str(mGetMessage, mm, ref::new_str(mes_id), false);
            if (!text::is_blank(s) && !text::starts_with(s, L"$")) return s;
        }
    }
    return condition_type_fallback(type);
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

void announce_character_status()
{
    // H works only in battle.
    if (!queries::is("BattleStateHelper.IsInBattle")) {
        ffpr::say(T(L"Party status only available in battle"), true);
        return;
    }

    // The actor whose turn is being selected/executed.
    void* actor = g_current_actor;
    if (!actor) {
        ffpr::say(T(L"No active character"), true);
        return;
    }

    const std::wstring name = ref::call_str(mOcdGetName, actor);
    // The battle unit's own OwnedCharacterData: its Parameter is the live battle parameter.
    void* param = ref::call<void*>(mOcdGetParameter, actor);
    if (!param) {
        ffpr::say(T(L"Character status not available"), true);
        return;
    }

    const int32_t hp = ref::call<int32_t>(mParamGetCurrentHP, param);
    int32_t max_hp = 0;
    if (il2cpp::Method* m = mParamConfirmedMaxHp.get())
        max_hp = il2cpp::invoke_int(il2cpp::virtual_method(param, m), param, nullptr, 0);
    std::wstring line = TF(L"{0}: {1}/{2} HP", {name, std::to_wstring(hp), std::to_wstring(max_hp)});

    // Active status effects (non-critical: a failure leaves the HP line alone).
    hooks::guarded("Mod.AnnounceCharacterStatus.conditions", [&] {
        void* list = ref::call<void*>(mParamGetCurrentConditionList, param);
        const int n = unity::list_count(list);
        std::vector<std::wstring> names;
        for (int i = 0; i < n; ++i) {
            const std::wstring c = condition_name(unity::list_at(list, i));
            if (!text::is_blank(c)) names.push_back(c);
        }
        if (!names.empty()) line += L" " + text::join(names, L", ");
    });

    ffpr::say(line, true);
}

// --- Filter Toggles ------------------------------------------------------------------------------
struct Toggle {
    const char* action;      // "Mod.ToggleX"
    const char* key;         // settings key
    bool def;                // FF1 default
    const wchar_t* feature;  // mod_text key of the spoken name
};

const Toggle kToggles[] = {
    {"Mod.TogglePathfindingFilter", prefs::kPathfindingFilter, prefs::kPathfindingFilterDefault, L"Pathfinding filter"},
    {"Mod.ToggleMapExitFilter", prefs::kMapExitFilter, prefs::kMapExitFilterDefault, L"Map exit filter"},
    {"Mod.ToggleToLayerFilter", prefs::kToLayerFilter, prefs::kToLayerFilterDefault, L"Layer transition filter"},
    {"Mod.ToggleWallTones", prefs::kWallTones, prefs::kWallTonesDefault, L"Wall tones"},
    {"Mod.ToggleFootsteps", prefs::kFootsteps, prefs::kFootstepsDefault, L"Footsteps"},
    {"Mod.ToggleAudioBeacons", prefs::kAudioBeacons, prefs::kAudioBeaconsDefault, L"Beacon navigation"},
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

int install()
{
    for (const Toggle& t : kToggles) {
        const Toggle* p = &t;
        actions::define(t.action, [p] { run_toggle(*p); });
    }
    actions::define("Mod.AnnounceGilAmount", [] { announce_gil_amount(); });
    actions::define("Mod.AnnounceCharacterStatus", [] { announce_character_status(); });

    events::on(events::Kind::BattleStart, [] { g_current_actor = nullptr; });
    events::on(events::Kind::BattleEnd, [] { g_current_actor = nullptr; });

    return hooks::attach(mSetCommandData, &d_SetCommandData, &o_SetCommandData) ? 1 : 0;
}
core::Install s_install{"ModActions", &install, 100};

} // namespace

void save_and_announce(const std::wstring& feature, bool value)
{
    ffpr::say(feature + L" " + (value ? T(L"on") : T(L"off")));
}

namespace mod {

void tick()
{
    // FF1's BattleCommandState reset handler cleared CurrentActor whenever the BattleCommand
    // state was switched off (another battle menu opening, battle end, map change).
    const bool active = menu_state::is_active(menu_state::BATTLE_COMMAND);
    if (g_battle_command_was_active && !active) g_current_actor = nullptr;
    g_battle_command_was_active = active;
}

} // namespace mod
} // namespace input
