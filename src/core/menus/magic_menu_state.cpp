// Ports FF1 Patches/MagicMenuState.cs (all but ShouldSuppress, which reads FF1's own
// AbilityWindowController and lives in magic_menu_ff1.cpp) and Menus/MagicDetailsAnnouncer.cs.
// All five games: the state is the registry's MAGIC_MENU key and the helpers read Last.* data.
// Only FF1's spell list feeds it today (magic_menu_ff1.cpp); another game's magic list can call
// ffpr::menus::set_last_announced_ability and the magic_* functions of menus_internal.h.
//
// Not ported: OnSpellListUnfocused, which nothing in FF1 calls.
#include "menus.h"
#include "menus_internal.h"
#include "../core.h"
#include "../hooks.h"
#include "../modtext.h"
#include "../say.h"
#include "../text.h"
#include "../unity.h"

namespace ffpr::menus {
namespace {

ref::Class  kOwnedAbility{"Last.Data.User", "OwnedAbility"};
ref::Field  fOwnedAbilityAbility{kOwnedAbility, "ability"};
ref::Method mOwnedAbilityMesIdName{kOwnedAbility, "get_MesIdName", 0};
ref::Method mOwnedAbilityMesIdDescription{kOwnedAbility, "get_MesIdDescription", 0};

ref::Class  kAbility{"Last.Data.Master", "Ability"};
ref::Method mAbilityGetAbilityLv{kAbility, "get_AbilityLv", 0};

ref::Class  kCondition{"Last.Data.Master", "Condition"};
ref::Method mConditionMesIdName{kCondition, "get_MesIdName", 0};
ref::Method mConditionType{kCondition, "get_ConditionType", 0};

ref::Class  kMagicPlayerParameter{"Last.Data", "PlayerCharacterParameter"};
ref::Field  fCurrentMpCountList{kMagicPlayerParameter, "<CurrentMpCountList>k__BackingField"};
ref::Field  fBaseMaxMpCountList{kMagicPlayerParameter, "<BaseMaxMpCountList>k__BackingField"};
ref::Method mConfirmedMaxMpCount{kMagicPlayerParameter, "ConfirmedMaxMpCount", 1};

bool g_target_selection_active = false;
// MagicMenuState.CurrentCharacter: the OwnedCharacterData whose spells are listed. It belongs to
// the party data (alive for the session) and is cleared with the magic state, as in FF1.
void* g_current_character = nullptr;
// MagicMenuState.LastAnnouncedAbility, kept as what the I key reads: the description's message
// id (OwnedAbility.MesIdDescription). The ability object itself is not kept.
bool g_last_ability_valid = false;
std::wstring g_last_ability_description_id;

void clear_magic_fields()
{
    g_target_selection_active = false;
    g_current_character = nullptr;
    g_last_ability_valid = false;
    g_last_ability_description_id.clear();
}

/// Fallback names for conditions with an empty/missing MesIdName, localized at call time.
std::wstring condition_type_fallback(int condition_type)
{
    switch (condition_type) {
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

/// Dictionary<int, int>.TryGetValue.
bool dict_int(void* dict, int key, int& value)
{
    struct Hit { uint32_t key; bool found = false; int value = 0; } h{static_cast<uint32_t>(key)};
    unity::dict_for_each(dict, [](uint64_t k, uint64_t v, void* st) {
        auto* p = static_cast<Hit*>(st);
        if (!p->found && static_cast<uint32_t>(k) == p->key) { p->found = true; p->value = static_cast<int32_t>(v); }
    }, &h, 4, 4);
    if (h.found) value = h.value;
    return h.found;
}

/// The localized text of a message id read from a game object, markup stripped; empty on a miss.
std::wstring localized_stripped(const std::wstring& mes_id)
{
    if (mes_id.empty()) return {};
    std::wstring localized = detail::message(mes_id, false);
    if (detail::blank(localized)) return {};
    return detail::strip_icon(localized);
}

/// MagicDetailsAnnouncer.AnnounceCurrentSpellDescription (I key in the field magic menu).
void announce_current_spell_description()
{
    std::wstring description;
    if (!detail::magic_last_announced_description(description) || detail::blank(description)) {
        ffpr::say(T(L"No description"), true);
        return;
    }
    ffpr::say(text::trim(description), true);
}

int install()
{
    // MagicMenuState's static constructor.
    menu_state::register_reset(menu_state::MAGIC_MENU, [] { clear_magic_fields(); });

    queries::define("MagicMenuState.IsSpellListActive", [] { return detail::magic_spell_list_active(); });
    queries::define("MagicMenuState.IsTargetSelectionActive", [] { return g_target_selection_active; });
    actions::define("MagicMenuState.ResetState", [] { detail::magic_reset_state(); });
    actions::define("MagicDetailsAnnouncer.AnnounceCurrentSpellDescription", [] { announce_current_spell_description(); });
    return 0;
}

core::Install s_install{"Menus.MagicMenuState", &install, 110};

} // namespace

// --- public helpers (menus.h) ---------------------------------------------------------------

std::wstring condition_name(void* condition)
{
    if (!condition) return {};
    std::wstring mes_id = ref::call_str(mConditionMesIdName, condition);
    int condition_type = ref::call<int32_t>(mConditionType, condition);

    // Primary: localize via MesIdName. Reject a "$"-prefixed result - MessageManager echoes the
    // raw key when unresolved.
    if (!mes_id.empty() && mes_id != L"None") {
        std::wstring localized = detail::message(mes_id, false);
        if (!detail::blank(localized) && localized[0] != L'$') return localized;
    }
    return condition_type_fallback(condition_type);
}

std::wstring spell_name(void* owned_ability)
{
    if (!owned_ability) return {};
    return localized_stripped(ref::call_str(mOwnedAbilityMesIdName, owned_ability));
}

std::wstring spell_description(void* owned_ability)
{
    if (!owned_ability) return {};
    return localized_stripped(ref::call_str(mOwnedAbilityMesIdDescription, owned_ability));
}

int spell_level(void* owned_ability)
{
    void* ability = ref::rd<void*>(owned_ability, fOwnedAbilityAbility);
    return ability ? ref::call<int32_t>(mAbilityGetAbilityLv, ability) : 0;
}

void charges_for_level(void* owned_character, int level, int& current, int& max)
{
    current = 0;
    max = 0;
    if (!owned_character || level <= 0 || level > 8) return;

    void* param = detail::character_parameter(owned_character);
    if (!param || !kMagicPlayerParameter.is(param)) return;

    int value = 0;
    if (dict_int(ref::rd<void*>(param, fCurrentMpCountList), level, value)) current = value;

    // ConfirmedMaxMpCount((AbilityLevelType)level); on a failure, the base list.
    bool ok = false;
    if (void* fp = mConfirmedMaxMpCount.ptr()) {
        try {
            using F = int32_t (*)(void*, int32_t, const void*);
            max = reinterpret_cast<F>(fp)(param, level, mConfirmedMaxMpCount.get());
            ok = true;
        } catch (...) {
            ok = false;
        }
    }
    if (!ok) {
        max = 0;
        if (dict_int(ref::rd<void*>(param, fBaseMaxMpCountList), level, value)) max = value;
    }
}

void set_last_announced_ability(void* owned_ability) { detail::magic_set_last_announced_ability(owned_ability); }

// --- area state (menus_internal.h) ----------------------------------------------------------

namespace detail {

bool magic_spell_list_active() { return menu_state::is_active(menu_state::MAGIC_MENU); }
bool magic_target_selection_active() { return g_target_selection_active; }

void magic_on_spell_list_focused()
{
    clear_other_menu_states("Magic");
    menu_state::set_active(menu_state::MAGIC_MENU, true);
}

void magic_on_target_selection_active()
{
    clear_other_menu_states("Magic");
    // MAGIC_MENU going inactive runs the reset handler, which also clears the target flag. Run
    // it first, then set the flag so it sticks.
    menu_state::reset(menu_state::MAGIC_MENU);
    g_target_selection_active = true;
}

void magic_on_target_selection_closed() { g_target_selection_active = false; }

void magic_reset_state()
{
    menu_state::reset(menu_state::MAGIC_MENU);
    clear_magic_fields();
}

void* magic_current_character() { return g_current_character; }
void magic_set_current_character(void* owned_character) { g_current_character = owned_character; }

void magic_set_last_announced_ability(void* owned_ability)
{
    g_last_ability_valid = owned_ability != nullptr;
    g_last_ability_description_id = owned_ability ? ref::call_str(mOwnedAbilityMesIdDescription, owned_ability) : std::wstring();
}

bool magic_last_announced_description(std::wstring& description)
{
    description.clear();
    if (!g_last_ability_valid) return false;
    description = localized_stripped(g_last_ability_description_id);
    return true;
}

} // namespace detail
} // namespace ffpr::menus
