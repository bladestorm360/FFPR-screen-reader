// Ports FF2 Core/MenuDetailCache.cs, Utils/LocalizationUtility.cs (GetMessage; the condition
// names use the core's menus::condition_name), the FF2-only parts of Utils/MenuStateRegistry.cs
// (KEYWORD_MENU, WORDS_MENU, NEW_GAME_NAMING and SetActiveExclusive) and the FF2 additions to
// FFII_ScreenReaderMod.ClearMenuFlagsForMapTransition (those three keys are cleared too).
//
// Not ported (the core replaces them): HarmonyPatchHelper, Helpers, StateReaderHelper,
// MenuStateHelper, IL2CppOffsets, TolkWrapper, CoroutineManager, GameObjectCache.
// FF2Constants.cs: TILE_SIZE is the core's; the magic menu states live in magic_menu.cpp.
// LocalizationUtility's ConditionType fallback named type 9 "Poison"; FF2's ConditionType enum
// says 9 = Blind, so the core's (FF1) fallback table is kept.
#include <string>
#include <vector>
#include "ff2.h"
#include "../../core/core.h"
#include "../../core/hooks.h"
#include "../../core/refs.h"
#include "../../core/text.h"

namespace ffpr::ff2 {
namespace {

ref::Class kMessageManager{"Last.Management", "MessageManager", game::kFF2};
ref::Method mGetMessage{kMessageManager, "GetMessage", 2};
ref::Class kMenuManager{"Last.UI", "MenuManager", game::kFF2};
ref::Method mMenuIsOpen{kMenuManager, "get_IsOpen", 0};

std::wstring g_menu_detail;

int install()
{
    // ClearMenuFlagsForMapTransition: FF2 also cleared these three of its own keys.
    events::on(events::Kind::MapChanged, [] {
        menu_state::reset(KEYWORD_MENU);
        menu_state::reset(WORDS_MENU);
        menu_state::reset(NEW_GAME_NAMING);
    });
    queries::define_text("MenuDetailCache.LastDetail", [] { return g_menu_detail; });
    return 0;
}

core::Install s_install{"FF2.Util", &install, 500, game::kFF2};

} // namespace

void set_active_exclusive(const char* key)
{
    const std::string keep = key ? key : "";
    for (const std::string& k : menu_state::active_states())
        if (k != keep && k != menu_state::MAIN_MENU) menu_state::reset(k.c_str());
    if (keep != menu_state::SHOP_MENU && actions::defined("ShopMenuTracker.ClearState"))
        actions::run("ShopMenuTracker.ClearState");
    menu_state::set_active(key, true);
}

std::wstring message(const std::wstring& mes_id)
{
    if (mes_id.empty()) return {};
    void* mm = ref::instance(kMessageManager);
    if (!mm) return {};
    std::wstring s = ref::call_str(mGetMessage, mm, ref::new_str(mes_id), false);
    return text::is_blank(s) ? std::wstring() : s;
}

std::wstring strip_icons(std::wstring_view s) { return text::trim(text::strip_markup(s)); }

bool menu_manager_open()
{
    void* mm = ref::instance(kMenuManager);
    return mm && ref::call<bool>(mMenuIsOpen, mm);
}

void set_menu_detail(const std::wstring& detail) { g_menu_detail = detail; }
const std::wstring& menu_detail() { return g_menu_detail; }

} // namespace ffpr::ff2
