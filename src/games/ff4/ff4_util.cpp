// Ports FF4 Core/MenuState.cs (MenuStateRegistry.SetActiveExclusive for FF4's own keys),
// Utils/MessageHelper.cs, Utils/TextUtils.cs (StripIconMarkup) and Utils/CharacterStatusHelper.cs.
// Shared by the files of the FF4 module (ff4.h).
#include "../../core/ui/preferences.h"
#include "ff4.h"
#include <vector>
#include "../../core/core.h"
#include "../../core/il2cpp.h"
#include "../../core/modtext.h"
#include "../../core/settings.h"
#include "../../core/text.h"
#include "../../core/unity.h"

namespace ffpr::ff4 {
namespace {

ref::Class  kMessageManager{"Last.Management", "MessageManager", game::kFF4};
ref::Method mGetMessage{kMessageManager, "GetMessage", 2};

ref::Class  kOwnedCharacter{"Last.Data.User", "OwnedCharacterData", game::kFF4};
ref::Field  fCharacterParameter{kOwnedCharacter, "parameter"};
ref::Method mCharacterGetName{kOwnedCharacter, "get_Name", 0};

ref::Class  kParameterBase{"Last.Data", "CharacterParameterBase", game::kFF4};
ref::Method mGetCurrentHP{kParameterBase, "get_CurrentHP", 0};
ref::Method mGetCurrentMP{kParameterBase, "get_CurrentMP", 0};
ref::Method mConfirmedMaxHp{kParameterBase, "ConfirmedMaxHp", 0};
ref::Method mConfirmedMaxMp{kParameterBase, "ConfirmedMaxMp", 0};
ref::Method mConfirmedConditionList{kParameterBase, "ConfirmedConditionList", 0};

ref::Class  kCondition{"Last.Data.Master", "Condition", game::kFF4};
ref::Method mConditionMesIdName{kCondition, "get_MesIdName", 0};

il2cpp::Method* dispatch(ref::Method& m, void* obj, void*& fp)
{
    fp = nullptr;
    il2cpp::Method* base = m.get();
    if (!base || !obj) return nullptr;
    il2cpp::Method* vm = il2cpp::virtual_method(reinterpret_cast<il2cpp::Object*>(obj), base);
    if (!vm) vm = base;
    fp = il2cpp::method_pointer(vm);
    return vm;
}

} // namespace

void set_active_exclusive(const char* key)
{
    // ClearOtherMenuStates(key): FF1's rule keeps MAIN_MENU (the parent of every sub-menu).
    for (const std::string& k : menu_state::active_states())
        if (k != key && k != menu_state::MAIN_MENU)
            menu_state::reset(k.c_str());
    // The shop tracker is not in the registry.
    if (actions::defined("ShopMenuTracker.ClearState")) actions::run("ShopMenuTracker.ClearState");
    menu_state::set_active(key, true);
}

std::wstring message(const std::wstring& mes_id)
{
    if (text::is_blank(mes_id)) return {};
    void* mm = ref::instance(kMessageManager);
    if (!mm) return {};
    void* key = ref::new_str(mes_id);
    if (!key) return {};
    std::wstring msg = ref::call_str(mGetMessage, mm, key, true);
    return text::is_blank(msg) ? std::wstring() : msg;
}

std::wstring strip_icon(std::wstring_view s)
{
    if (s.empty()) return {};
    return text::trim(text::strip_markup(s));
}

bool auto_detail() { return ui::prefs::auto_detail_enabled(); }

int call_virtual_int(ref::Method& m, void* obj)
{
    void* fp = nullptr;
    il2cpp::Method* vm = dispatch(m, obj, fp);
    if (!fp) return 0;
    try {
        using F = int32_t (*)(void*, const void*);
        return reinterpret_cast<F>(fp)(obj, vm);
    } catch (...) {
        ref::call_failed_note(m.name);
        return 0;
    }
}

void* call_virtual_ptr(ref::Method& m, void* obj)
{
    void* fp = nullptr;
    il2cpp::Method* vm = dispatch(m, obj, fp);
    if (!fp) return nullptr;
    try {
        using F = void* (*)(void*, const void*);
        return reinterpret_cast<F>(fp)(obj, vm);
    } catch (...) {
        ref::call_failed_note(m.name);
        return nullptr;
    }
}

std::wstring vitals_string(void* parameter)
{
    if (!parameter) return {};
    int hp = ref::call<int32_t>(mGetCurrentHP, parameter);
    int max_hp = call_virtual_int(mConfirmedMaxHp, parameter);
    int mp = ref::call<int32_t>(mGetCurrentMP, parameter);
    int max_mp = call_virtual_int(mConfirmedMaxMp, parameter);
    return T(L"HP") + L" " + text::num(hp) + L"/" + text::num(max_hp) + L", " +
           T(L"MP") + L" " + text::num(mp) + L"/" + text::num(max_mp);
}

std::wstring status_conditions(void* parameter)
{
    if (!parameter) return {};
    void* list = call_virtual_ptr(mConfirmedConditionList, parameter);
    int n = unity::list_count(list);
    if (n <= 0) return {};
    std::vector<std::wstring> names;
    for (int i = 0; i < n; ++i) {
        void* condition = unity::list_at(list, i);
        if (!condition) continue;
        std::wstring mes_id = ref::call_str(mConditionMesIdName, condition);
        // Conditions with no message id are internal/hidden statuses.
        if (mes_id.empty() || mes_id == L"None") continue;
        std::wstring name = message(mes_id);
        if (!name.empty()) names.push_back(std::move(name));
    }
    std::wstring out;
    for (size_t i = 0; i < names.size(); ++i) {
        if (i) out += L", ";
        out += names[i];
    }
    return out;
}

std::wstring full_status(void* parameter)
{
    if (!parameter) return {};
    std::wstring vitals = vitals_string(parameter);
    if (vitals.empty()) return {};
    std::wstring result = L", " + vitals;
    std::wstring conditions = status_conditions(parameter);
    if (!conditions.empty()) result += L", " + conditions;
    return result;
}

std::wstring character_name(void* owned_character)
{
    return owned_character ? ref::call_str(mCharacterGetName, owned_character) : std::wstring();
}

void* character_parameter(void* owned_character)
{
    return ref::rd<void*>(owned_character, fCharacterParameter);
}

} // namespace ffpr::ff4
