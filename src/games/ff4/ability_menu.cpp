// Ports FF4 Patches/AbilityMenuPatches.cs: the field Ability menu, which is FF4's magic list
// (white, black and summon magic per character, plus Cecil's and others' abilities).
//
// MenuStates.Ability is the core's MAGIC_MENU key (ff4.h), and the focused spell is handed to the
// core with ffpr::menus::set_last_announced_ability, so the core's I key
// (MagicDetailsAnnouncer.AnnounceCurrentSpellDescription) reads FF4's spell description. On the
// command bar no spell is focused (C# LastDescription = null): the ability is cleared.
//
// Also the Phase 1 proof hook (UNIFIED_MOD_PLAN.md): AbilityCharaStatusController.UpdateView logs
// the first time it fires.
//
// Folded body: AbilityUseContentListController.SelectContent shares its RVA (0x9AD020) with the
// same class's SetCursor, as it did under Harmony; the detour class-checks self.
#include <string>
#include "ff4.h"
#include "../../core/core.h"
#include "../../core/dedup.h"
#include "../../core/frame.h"
#include "../../core/hooks.h"
#include "../../core/il2cpp.h"
#include "../../core/log.h"
#include "../../core/modtext.h"
#include "../../core/say.h"
#include "../../core/text.h"
#include "../../core/ui_util.h"
#include "../../core/unity.h"
#include "../../core/menus/menus.h"

namespace ffpr::ff4 {
namespace {

ref::Class  kAbilityWindow{"Serial.FF4.UI.KeyInput", "AbilityWindowController", game::kFF4};
ref::Field  fWindowCommandController{kAbilityWindow, "commandController"};
ref::Method mWindowSetActive{kAbilityWindow, "SetActive", 1};
ref::Method mCommandInit{kAbilityWindow, "CommandInit", 0};
ref::Method mUseListInit{kAbilityWindow, "UseListInit", 0};
ref::Method mUseTargetInit{kAbilityWindow, "UseTargetInit", 0};

ref::Class  kAbilityCommand{"Serial.FF4.UI.KeyInput", "AbilityCommandController", game::kFF4};
ref::Field  fCommandContentList{kAbilityCommand, "contentList"};
ref::Field  fCommandSelectCursor{kAbilityCommand, "selectCursor"};
ref::Method mCommandSelectContent{kAbilityCommand, "SelectContent", 1};
ref::Class  kAbilityCommandView{"Serial.FF4.UI.KeyInput", "AbilityCommandContentView", game::kFF4};
ref::Field  fCommandViewText{kAbilityCommandView, "text"};

ref::Class  kAbilityContentList{"Serial.FF4.UI.KeyInput", "AbilityContentListController", game::kFF4};
ref::Field  fContentList{kAbilityContentList, "contentList"};
ref::Method mContentSelectContent{kAbilityContentList, "SelectContent", 3};
ref::Class  kAbilityInfoContent{"Last.UI.KeyInput", "BattleAbilityInfomationContentController", game::kFF4};
ref::Field  fInfoData{kAbilityInfoContent, "<Data>k__BackingField"};

ref::Class  kOwnedAbility{"Last.Data.User", "OwnedAbility", game::kFF4};
ref::Field  fOwnedAbilityAbility{kOwnedAbility, "ability"};
ref::Method mOwnedAbilityMesIdName{kOwnedAbility, "get_MesIdName", 0};
ref::Method mOwnedAbilityMesIdDescription{kOwnedAbility, "get_MesIdDescription", 0};
ref::Class  kAbility{"Last.Data.Master", "Ability", game::kFF4};
ref::Method mAbilityUseValue{kAbility, "get_UseValue", 0};

ref::Class  kAbilityUseList{"Serial.FF4.UI.KeyInput", "AbilityUseContentListController", game::kFF4};
ref::Field  fUseContentList{kAbilityUseList, "contentList"};
ref::Method mUseSelectContent{kAbilityUseList, "SelectContent", 2};
ref::Class  kTargetContent{"Last.UI.KeyInput", "ItemTargetSelectContentController", game::kFF4};
ref::Field  fTargetCurrentData{kTargetContent, "<CurrentData>k__BackingField"};

ref::Class  kCharaStatus{"Serial.FF4.UI.KeyInput", "AbilityCharaStatusController", game::kFF4};
ref::Method mCharaUpdateView{kCharaStatus, "UpdateView", 2};

// AnnouncementContexts (DEDUP_INVENTORY.md FF4 rows 1-3): text, as in the C#.
constexpr const char* kCtxCommand = "AbilityMenu.Command";
constexpr const char* kCtxContent = "AbilityMenu.Content";
constexpr const char* kCtxUseTarget = "AbilityMenu.UseTarget";

/// AbilityCommandController_SelectContent_Patch.Announce
void announce_command(void* controller, int index)
{
    void* list = ref::rd<void*>(controller, fCommandContentList);
    void* view = ui::list_item(list, index);
    if (!view) return;
    void* text_component = ref::rd<void*>(view, fCommandViewText);
    if (!text_component) return;
    std::wstring name = strip_icon(unity::ui_text(text_component));
    if (text::is_blank(name)) return;
    if (!dedup::text(kCtxCommand, name)) return;

    // The command bar has no spell focused for the details key.
    set_active_exclusive(ABILITY);
    ffpr::menus::set_last_announced_ability(nullptr);
    ffpr::say(ui::menu_position(name, index, unity::list_count(list)), true);
}

using SelectIndex_t = void (*)(void* self, int32_t index, const void* mi);
SelectIndex_t o_CommandSelectContent;
void d_CommandSelectContent(void* self, int32_t index, const void* mi)
{
    o_CommandSelectContent(self, index, mi);
    hooks::guarded("FF4 AbilityCommandController.SelectContent", [&] { announce_command(self, index); });
}

// AbilityContentListController_SelectContent_Patch
using ContentSelect_t = void (*)(void* self, void* cursor, int32_t type, bool page_skip, const void* mi);
ContentSelect_t o_ContentSelectContent;
void d_ContentSelectContent(void* self, void* cursor, int32_t type, bool page_skip, const void* mi)
{
    o_ContentSelectContent(self, cursor, type, page_skip, mi);
    hooks::guarded("FF4 AbilityContentListController.SelectContent", [&] {
        int index = ui::cursor_index(cursor);
        if (index < 0) return;
        void* list = ref::rd<void*>(self, fContentList);
        void* content = ui::list_item(list, index);
        if (!content) return;
        void* owned = ref::rd<void*>(content, fInfoData);
        if (!owned) return;

        std::wstring name = strip_icon(message(ref::call_str(mOwnedAbilityMesIdName, owned)));
        if (text::is_blank(name)) return;
        std::wstring announcement = name;

        // MP cost from master data (the controller-level MP text can lag one entry behind).
        void* ability = ref::rd<void*>(owned, fOwnedAbilityAbility);
        int mp_cost = ability ? ref::call<int32_t>(mAbilityUseValue, ability) : 0;
        if (mp_cost > 0) announcement += L", " + T(L"MP") + L" " + text::num(mp_cost);

        std::wstring description = strip_icon(message(ref::call_str(mOwnedAbilityMesIdDescription, owned)));
        // Auto Detail: include the description the details key would read.
        if (auto_detail() && !text::is_blank(description)) announcement += L". " + description;

        if (!dedup::text(kCtxContent, announcement)) return;

        set_active_exclusive(ABILITY);
        ffpr::menus::set_last_announced_ability(owned);   // LastDescription, for the I key
        ffpr::say(ui::menu_position(announcement, index, unity::list_count(list)), true);
    });
}

/// targetContents as a List<T> (its display order), or null when it is another enumerable.
void* as_list(void* enumerable)
{
    if (!enumerable) return nullptr;
    const char* cls = il2cpp::object_class_name(reinterpret_cast<il2cpp::Object*>(enumerable));
    return (cls && text::starts_with(text::widen(cls), L"List`1")) ? enumerable : nullptr;
}

// AbilityUseContentListController_SelectContent_Patch
using UseSelect_t = void (*)(void* self, void* target_contents, void* cursor, const void* mi);
UseSelect_t o_UseSelectContent;
void d_UseSelectContent(void* self, void* target_contents, void* cursor, const void* mi)
{
    o_UseSelectContent(self, target_contents, cursor, mi);
    hooks::guarded("FF4 AbilityUseContentListController.SelectContent", [&] {
        if (!kAbilityUseList.is(self)) return;
        int index = ui::cursor_index(cursor);
        if (index < 0) return;

        // targetContents (display order) rather than contentList (data order).
        void* target_list = as_list(target_contents);
        void* selected = nullptr;
        int count = 0;
        if (target_list && index < unity::list_count(target_list)) {
            selected = unity::list_at(target_list, index);
            count = unity::list_count(target_list);
        } else {
            void* list = ref::rd<void*>(self, fUseContentList);
            selected = ui::list_item(list, index);
            count = unity::list_count(list);
        }
        if (!selected) return;
        void* data = ref::rd<void*>(selected, fTargetCurrentData);
        if (!data) return;
        std::wstring name = character_name(data);
        if (name.empty()) return;

        std::wstring announcement = name + full_status(character_parameter(data));
        if (!dedup::text(kCtxUseTarget, announcement)) return;

        set_active_exclusive(ABILITY);
        ffpr::say(ui::menu_position(announcement, index, count), true);
    });
}

// AbilityWindowController_StateInit_Patch: each screen's guard clears as it (re)gains focus; on
// command (re)entry the focused command is read one frame later.
using Void_t = void (*)(void* self, const void* mi);
Void_t o_CommandInit;
void d_CommandInit(void* self, const void* mi)
{
    dedup::reset(kCtxCommand);
    o_CommandInit(self, mi);
    hooks::guarded("FF4 AbilityWindowController.CommandInit", [&] {
        frame::next_frame([] {
            void* window = unity::find_object(kAbilityWindow);
            if (!window || !unity::active_in_hierarchy(window)) return;
            void* controller = ref::rd<void*>(window, fWindowCommandController);
            void* cursor = controller ? ref::rd<void*>(controller, fCommandSelectCursor) : nullptr;
            if (cursor) announce_command(controller, ui::cursor_index(cursor));
        }, "FF4.AbilityMenu.CommandFocus");
    });
}

Void_t o_UseListInit;
void d_UseListInit(void* self, const void* mi)
{
    dedup::reset(kCtxContent);
    o_UseListInit(self, mi);
}

Void_t o_UseTargetInit;
void d_UseTargetInit(void* self, const void* mi)
{
    dedup::reset(kCtxUseTarget);
    o_UseTargetInit(self, mi);
}

// AbilityMenuStatePatches: the menu closing clears the Ability state.
using SetActive_t = void (*)(void* self, bool is_active, const void* mi);
SetActive_t o_WindowSetActive;
void d_WindowSetActive(void* self, bool active, const void* mi)
{
    o_WindowSetActive(self, active, mi);
    hooks::guarded("FF4 AbilityWindowController.SetActive", [&] {
        if (!active && is_active(ABILITY)) reset(ABILITY);
    });
}

// Phase 1 proof hook (a game-only class, hooked by name).
using UpdateView_t = void (*)(void* self, void* data, bool is_face, const void* mi);
UpdateView_t o_CharaUpdateView;
bool g_proof_logged = false;
void d_CharaUpdateView(void* self, void* data, bool is_face, const void* mi)
{
    o_CharaUpdateView(self, data, is_face, mi);
    if (!g_proof_logged) {
        g_proof_logged = true;
        LOGF("FF4: AbilityCharaStatusController.UpdateView fired (Phase 1 proof hook)");
    }
}

int install()
{
    int n = 0;
    if (hooks::attach(mCommandSelectContent, &d_CommandSelectContent, &o_CommandSelectContent)) ++n;
    if (hooks::attach(mContentSelectContent, &d_ContentSelectContent, &o_ContentSelectContent)) ++n;
    if (hooks::attach(mUseSelectContent, &d_UseSelectContent, &o_UseSelectContent)) ++n;
    if (hooks::attach(mCommandInit, &d_CommandInit, &o_CommandInit)) ++n;
    if (hooks::attach(mUseListInit, &d_UseListInit, &o_UseListInit)) ++n;
    if (hooks::attach(mUseTargetInit, &d_UseTargetInit, &o_UseTargetInit)) ++n;
    if (hooks::attach(mWindowSetActive, &d_WindowSetActive, &o_WindowSetActive)) ++n;
    if (hooks::attach(mCharaUpdateView, &d_CharaUpdateView, &o_CharaUpdateView)) ++n;
    queries::define("MenuStates.Ability.IsActive", [] { return is_active(ABILITY); });
    return n;
}

core::Install s_install{"FF4.AbilityMenu", &install, 510, game::kFF4};

} // namespace
} // namespace ffpr::ff4
