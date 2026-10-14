// Shared FF4/FF5 native quantity spell lists; retains the FF4 focus event guard.
#include <string>
#include "../core.h"
#include "../dedup.h"
#include "../hooks.h"
#include "../modtext.h"
#include "../say.h"
#include "../text.h"
#include "../ui_util.h"
#include "../unity.h"
#include "../system/system.h"
#include "battle_internal.h"
#include "battle_util.h"
#include "../displayed_ui.h"
#include "../ui/preferences.h"

namespace ffpr::battle {
namespace {

ref::Class kAbilityInfo4{"Serial.FF0.UI.KeyInput", "BattleQuantityAbilityInfomationController", game::kFF4};
ref::Class kAbilityInfo5{"Serial.FF5.UI.KeyInput", "BattleQuantityAbilityInfomationController", game::kFF5};
ref::Field fContents4{kAbilityInfo4, "contentList"}, fContents5{kAbilityInfo5, "contentList"};
ref::Method mSelect4{kAbilityInfo4, "SelectContent", 2}, mSelect5{kAbilityInfo5, "SelectContent", 2};
ref::Method mShow4{kAbilityInfo4, "ShowUseSelect", 4}, mShow5{kAbilityInfo5, "ShowUseSelect", 4};
ref::Class  kAbilityInfoContent{"Last.UI.KeyInput", "BattleAbilityInfomationContentController", game::kFF4 | game::kFF5};
ref::Field  fInfoData{kAbilityInfoContent, "<Data>k__BackingField"};

ref::Class  kOwnedAbility{"Last.Data.User", "OwnedAbility", game::kFF4 | game::kFF5};
ref::Method mOwnedAbilityMesIdName{kOwnedAbility, "get_MesIdName", 0};
ref::Method mOwnedAbilityMesIdDescription{kOwnedAbility, "get_MesIdDescription", 0};
// AnnouncementContexts.BATTLE_ABILITY_SELECT (DEDUP_INVENTORY.md FF4 row 7): text, as in the C#.
constexpr const char* kCtxAbilitySelect = "BattleAbility.Select";

// BattleListDetails.LastDescription for the ability list (empty: none focused).
std::wstring g_last_description;
bool g_has_description = false;

using Select_t = void (*)(void* self, void* cursor, int32_t type, const void* mi);
Select_t o_SelectContent;
void d_SelectContent(void* self, void* cursor, int32_t type, const void* mi)
{
    o_SelectContent(self, cursor, type, mi);
    hooks::guarded("BattleQuantity.SelectContent", [&] {
        if (!displayed::container_visible(displayed::field(self, "view"))) return;
        int index = ui::cursor_index(cursor);
        if (index < 0) return;
        void* list = ref::rd<void*>(self, game::is(game::Id::FF4) ? fContents4 : fContents5);
        void* content = ui::list_item(list, index);
        if (!content) return;
        void* owned = ref::rd<void*>(content, fInfoData);
        if (!owned) return;

        std::wstring mes_name = ref::call_str(mOwnedAbilityMesIdName, owned);
        std::wstring mes_description = ref::call_str(mOwnedAbilityMesIdDescription, owned);
        if (text::is_blank(mes_name)) return;
        std::wstring name = strip_icon(message(mes_name));
        if (text::is_blank(name)) return;

        void* view = displayed::field(content, "view");
        std::wstring announcement = displayed::text_field(displayed::field(view, "iconText"), "nameText");
        if (announcement.empty()) return;
        const auto cost = displayed::pair(view, "fixText", "valueText");
        if (!cost.empty()) announcement += L", " + cost;

        std::wstring description = text::is_blank(mes_description) ? std::wstring() : strip_icon(message(mes_description));
        g_last_description = description;
        g_has_description = !text::is_blank(mes_description);
        if (ui::prefs::auto_detail_enabled() && !text::is_blank(description)) announcement += L", " + description;

        // The battle magic state, so the I key reads this list's description.
        ffpr::system::clear_other_menu_states("BattleMagic");
        menu_state::set_active(menu_state::BATTLE_MAGIC, true);
        set_target_selection_active(false);
        clear_cached_target_controller();
        notify_command_submenu_active();
        // Preserve FF4's documented text guard. FF5 is driven by accepted native
        // focus events and must not acquire a new text-content deduplication rule.
        if (game::is(game::Id::FF4) && !dedup::text(kCtxAbilitySelect, announcement, dedup::Scope::Battle)) return;
        ffpr::say(ui::menu_position(announcement, index, unity::list_count(list)), true);
    });
}

using ShowUseSelect_t = void (*)(void* self, void* player, void* command, void* selected, void* cancel, const void* mi);
ShowUseSelect_t o_ShowUseSelect;
void d_ShowUseSelect(void* self, void* player, void* command, void* selected, void* cancel, const void* mi)
{
    dedup::reset(kCtxAbilitySelect);   // prefix: the list re-reads its first entry
    o_ShowUseSelect(self, player, command, selected, cancel, mi);
}

/// HandleItemDetailsKey's battle branch for the ability list.
void announce_current_description()
{
    if (!g_has_description || text::is_blank(g_last_description)) {
        ffpr::say(T(L"No description"), true);
        return;
    }
    ffpr::say(text::trim(g_last_description), true);
}

int install()
{
    int n = 0;
    if (hooks::attach(game::is(game::Id::FF4) ? mSelect4 : mSelect5, &d_SelectContent, &o_SelectContent)) ++n;
    if (hooks::attach(game::is(game::Id::FF4) ? mShow4 : mShow5, &d_ShowUseSelect, &o_ShowUseSelect)) ++n;
    menu_state::register_reset(menu_state::BATTLE_MAGIC, [] {
        g_last_description.clear();
        g_has_description = false;
    });
    actions::define("BattleMagicPatches.AnnounceCurrentDescription", [] { announce_current_description(); });
    return n;
}

core::Install s_install{"Battle.QuantityAbilities", &install, 520, game::kFF4 | game::kFF5};

} // namespace
} // namespace ffpr::battle
