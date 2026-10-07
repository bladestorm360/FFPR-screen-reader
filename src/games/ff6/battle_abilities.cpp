// Ports FF6 Patches/BattleCommandPatches.cs: BattleQuantityAbilityInfomationController.SelectContent
// (FF6's battle ability list: Magic, Lore, and the special commands Blitz, SwdTech, Dance, Rage ...,
// which FF6 shows in this same controller) and .SelectPhantomBeastContent (the equipped esper's
// "Summon" entry above the spell list).
//
// What is read is the C#'s (game text: the localized name and description from the ability's
// message ids, the MP cost from the entry's own value text, the summon's name, MP and description
// from the view). Conventions are FF4's battle ability list (src/games/ff4/battle_abilities.cpp),
// so behaviour matches the other games: "Name, MP 5, description, (n of N)" with the description
// under Auto Detail and the position under its setting; the BattleMagic menu state, which keeps the
// generic cursor reader silent and sends the I key to "BattleMagicPatches.AnnounceCurrentDescription"
// (defined here; the core's is FF1-only); the repeat check reset when the list opens
// (ShowUseSelect prefix). As the core's (FF1's) battle lists do, each read also runs
// "BattleCommandPatches.NotifyCommandSubmenuActive" (the command menu's back-out re-announce).
// The summon line is "Summon, Name, MP 10, description" (FF6 only; no position: it is not in the
// list). The C# dropped a description's leading "Name:"; kept (game-content cleanup).
//
// Dedup: the C#'s two lastAnnouncement strings (text) move to one dedup:: context, so moving
// between the summon and the list always speaks (with two, returning to the same spell was silent).
//
// Not ported: SpecialAbilityContentListController_SetCursor_Patch (battle Blitz/Tools lists). In
// FF6's GameAssembly.dll that class is used only by the field Ability menu (AbilityWindowController
// is its only caller and owner); battle special commands are listed by this controller, and the
// field menu is src/games/ff6/ability_menu.cpp.
//
// Folded bodies: none. SelectContent 0x8D44B0, SelectPhantomBeastContent 0x8D4730 and
// ShowUseSelect 0x8D5830 each have one RVA in FF6's dump.cs.
#include <cstdint>
#include <cwctype>
#include <string>
#include "../../core/core.h"
#include "../../core/dedup.h"
#include "../../core/hooks.h"
#include "../../core/modtext.h"
#include "../../core/refs.h"
#include "../../core/say.h"
#include "../../core/text.h"
#include "../../core/ui_util.h"
#include "../../core/unity.h"
#include "../../core/battle/battle_util.h"
#include "../../core/system/system.h"
#include "../../core/ui/preferences.h"

namespace ffpr::ff6 {
namespace {

ref::Class  kQuantityCtl{"Serial.FF6.UI.KeyInput", "BattleQuantityAbilityInfomationController", game::kFF6};
ref::Field  fContentList{kQuantityCtl, "contentList"};
ref::Method mSelectContent{kQuantityCtl, "SelectContent", 2};
ref::Method mSelectPhantomBeastContent{kQuantityCtl, "SelectPhantomBeastContent", 1};
ref::Method mShowUseSelect{kQuantityCtl, "ShowUseSelect", 4};
ref::Class  kCtlBase{"Serial.Template.UI.KeyInput", "BattleAbilityInfomationControllerBase", game::kFF6};
ref::Field  fCtlView{kCtlBase, "view"};
ref::Class  kInfoView{"Serial.Template.UI.KeyInput", "BattleAbilityInfomationView", game::kFF6};
ref::Field  fPhantomBeastContent{kInfoView, "phantomBeastContent"};
ref::Field  fDescriptionText{kInfoView, "descriptionText"};

ref::Class  kAbilityContent{"Last.UI.KeyInput", "BattleAbilityInfomationContentController", game::kFF6};
ref::Field  fAbilityData{kAbilityContent, "<Data>k__BackingField"};
ref::Field  fAbilityContentView{kAbilityContent, "view"};
ref::Method mGetIconTextView{kAbilityContent, "get_iconTextView", 0};
ref::Class  kAbilityContentView{"Last.UI.KeyInput", "BattleAbilityInfomationContentView", game::kFF6};
ref::Field  fValueText{kAbilityContentView, "valueText"};
ref::Class  kIconTextView{"Last.UI", "IconTextView", game::kFF6};
ref::Field  fNameText{kIconTextView, "nameText"};

ref::Class  kOwnedAbility{"Last.Data.User", "OwnedAbility", game::kFF6};
ref::Method mMesIdName{kOwnedAbility, "get_MesIdName", 0};
ref::Method mMesIdDescription{kOwnedAbility, "get_MesIdDescription", 0};

// The C#'s lastAnnouncement of the SelectContent and SelectPhantomBeastContent patches (shared).
constexpr const char* kAbilityCtx = "FF6.BattleAbility.Select";

// BattleListDetails.LastDescription for the list (FF4): the focused entry's description.
std::wstring g_last_description;

bool blank(const std::wstring& s) { return text::is_blank(s); }

std::wstring stripped(const std::wstring& s)
{
    if (blank(s)) return {};
    std::wstring r = battle::strip_icon(s);
    return blank(r) ? std::wstring() : r;
}

/// MessageManager text for an OwnedAbility mes id getter, icon markup stripped.
std::wstring ability_text(ref::Method& getter, void* owned)
{
    const std::wstring mes_id = ref::call_str(getter, owned);
    if (blank(mes_id)) return {};
    return stripped(battle::message(mes_id));
}

/// FF6's descriptions start with "Name: ..."; the C# dropped that repeat of the name.
std::wstring without_name_prefix(const std::wstring& description, const std::wstring& name)
{
    const std::wstring prefix = name + L":";
    if (!text::starts_with(description, prefix)) return description;
    std::wstring rest = description.substr(prefix.size());
    size_t i = 0;
    while (i < rest.size() && iswspace(rest[i])) ++i;
    return rest.substr(i);
}

/// ", MP 5" from the entry's displayed value text (FF4's wording); empty when blank or "0".
std::wstring mp_part(void* content_view)
{
    void* value_text = content_view ? ref::rd<void*>(content_view, fValueText) : nullptr;
    const std::wstring mp = value_text ? text::trim(unity::ui_text(value_text)) : std::wstring();
    if (blank(mp) || mp == L"0") return {};
    return L", " + T(L"MP") + L" " + mp;
}

/// Description under Auto Detail (", description"), kept for the I key either way.
std::wstring detail_part(const std::wstring& description)
{
    g_last_description = description;
    return (ui::prefs::auto_detail_enabled() && !blank(description)) ? L", " + description : std::wstring();
}

/// The battle magic state (the I key reads this list's description), then speak.
void speak(const std::wstring& line)
{
    ffpr::system::clear_other_menu_states("BattleMagic");
    menu_state::set_active(menu_state::BATTLE_MAGIC, true);
    if (actions::defined("BattleCommandPatches.NotifyCommandSubmenuActive"))
        actions::run("BattleCommandPatches.NotifyCommandSubmenuActive");
    ffpr::say(line, true);
}

// --- SelectContent(Cursor, WithinRangeType): postfix ---------------------------------------------
using SelectContent_t = void (*)(void* self, void* cursor, int32_t type, const void* mi);
SelectContent_t o_SelectContent;

void d_SelectContent(void* self, void* cursor, int32_t type, const void* mi)
{
    o_SelectContent(self, cursor, type, mi);
    hooks::guarded("FF6 BattleQuantityAbilityInfomationController.SelectContent", [&] {
        if (!self || !cursor) return;
        const int index = ui::cursor_index(cursor);
        if (index < 0) return;
        void* list = ref::rd<void*>(self, fContentList);
        void* content = ui::list_item(list, index);
        if (!content) return;
        void* owned = ref::rd<void*>(content, fAbilityData);
        if (!owned) return;

        const std::wstring name = ability_text(mMesIdName, owned);
        if (name.empty()) return;
        std::wstring announcement = name;
        announcement += mp_part(ref::rd<void*>(content, fAbilityContentView));
        announcement += detail_part(without_name_prefix(ability_text(mMesIdDescription, owned), name));

        if (!dedup::text(kAbilityCtx, announcement, dedup::Scope::Battle)) return;
        speak(ui::menu_position(announcement, index, unity::list_count(list)));
    });
}

// --- SelectPhantomBeastContent(Cursor): postfix ---------------------------------------------------
using SelectPhantom_t = void (*)(void* self, void* cursor, const void* mi);
SelectPhantom_t o_SelectPhantomBeastContent;

void d_SelectPhantomBeastContent(void* self, void* cursor, const void* mi)
{
    o_SelectPhantomBeastContent(self, cursor, mi);
    hooks::guarded("FF6 BattleQuantityAbilityInfomationController.SelectPhantomBeastContent", [&] {
        if (!self || !cursor) return;
        void* view = ref::rd<void*>(self, fCtlView);
        void* summon = view ? ref::rd<void*>(view, fPhantomBeastContent) : nullptr;
        if (!summon) return;

        // The displayed name (the C# found the summon entry's Data unset).
        void* icon = ref::call<void*>(mGetIconTextView, summon);
        void* name_text = icon ? ref::rd<void*>(icon, fNameText) : nullptr;
        const std::wstring name = stripped(name_text ? unity::ui_text(name_text) : std::wstring());
        if (name.empty()) return;

        std::wstring announcement = TF(L"Summon, {0}", {name});
        announcement += mp_part(ref::rd<void*>(summon, fAbilityContentView));
        // The list's description window, which the game has just filled for the summon.
        void* desc_text = ref::rd<void*>(view, fDescriptionText);
        announcement += detail_part(stripped(desc_text ? unity::ui_text(desc_text) : std::wstring()));

        if (!dedup::text(kAbilityCtx, announcement, dedup::Scope::Battle)) return;
        speak(announcement);
    });
}

// --- ShowUseSelect(player, command, selected, cancel): prefix (FF4's reset point) ---------------
using ShowUseSelect_t = void (*)(void* self, void* player, void* command, void* selected, void* cancel, const void* mi);
ShowUseSelect_t o_ShowUseSelect;

void d_ShowUseSelect(void* self, void* player, void* command, void* selected, void* cancel, const void* mi)
{
    dedup::reset(kAbilityCtx);   // the list re-reads its first entry
    o_ShowUseSelect(self, player, command, selected, cancel, mi);
}

/// HandleItemDetailsKey's battle branch for the list (FF4).
void announce_current_description()
{
    ffpr::say(blank(g_last_description) ? T(L"No description") : text::trim(g_last_description), true);
}

int install()
{
    int n = 0;
    if (hooks::attach(mSelectContent, &d_SelectContent, &o_SelectContent)) ++n;
    if (hooks::attach(mSelectPhantomBeastContent, &d_SelectPhantomBeastContent, &o_SelectPhantomBeastContent)) ++n;
    if (hooks::attach(mShowUseSelect, &d_ShowUseSelect, &o_ShowUseSelect)) ++n;
    menu_state::register_reset(menu_state::BATTLE_MAGIC, [] { g_last_description.clear(); });
    // The I key in this list (core cascade: BattleMagicMenuState.IsActive).
    actions::define("BattleMagicPatches.AnnounceCurrentDescription", [] { announce_current_description(); });
    return n;
}

core::Install s_install{"FF6.BattleAbilities", &install, 510, game::kFF6};

} // namespace
} // namespace ffpr::ff6
