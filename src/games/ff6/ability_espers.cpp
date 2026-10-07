// Ports FF6 Patches/AbilityMenuPatches.cs, the esper (magicite) part:
// MagicStoneListController_SelectContent_Patch, MagicStoneDetailsController_Show_Patch (with its
// ReadDetailsAfterDelay and TryReannounceEsperDetails) and MagicStoneDetailsController_UpdateView_Patch.
// Ported from FF6ScreenReader (BlindGuyNW/FF6ScreenReader, by ZKline), with its author's permission.
//
// The list line follows FF1's mod-side behaviour (lead decision, user rule 2026-09-25): the esper
// description only with AutoDetail (always on the I key, ability_menu.cpp), "(n of N)" last.
//
// Not ported: the list patch's "[Esper Detail Debug]" walk over every Text under the view's root
// (it only logged). KeyInput.ChangeMagicStonePopup (the "equip this esper?" popup) is read by the
// core (src/core/dialogue/popup.cpp): its override of Popup.Open calls base Popup.Open
// (FF6 0x676C60 -> 0xA29640), so the core's hook sees it.
//
// The details read: Show() stores target / candidates / esper on the controller and only sets the
// next state; WaitInputInit then fills the view through UpdateView (FF6 0x6056A0 -> 0x605030), so
// the view can still be the previous esper's on the frame after Show. The C# read one frame after
// Show and rebuilt silently one frame after UpdateView. Here an UpdateView that follows a Show
// carries that Show's announcement (one event, one line, read from the filled view); a Show that
// sees no UpdateView within two frames reads on its own, as the C# did. UpdateView alone (L/R
// between espers) rebuilds the I-key text silently, as in the C#.
//
// Game objects are not kept across frames: the esper and target are copied out as ids in the
// hooks (the C# kept the objects), the candidates are re-read from the controller's own field
// (Show stores them there, FF6 0x6041A0), and the controller is re-checked with unity::alive.
#include <cstdint>
#include <string>
#include <vector>
#include "ability.h"
#include "../../core/core.h"
#include "../../core/dedup.h"
#include "../../core/frame.h"
#include "../../core/hooks.h"
#include "../../core/modtext.h"
#include "../../core/refs.h"
#include "../../core/say.h"
#include "../../core/text.h"
#include "../../core/ui_util.h"
#include "../../core/unity.h"
#include "../../core/battle/battle_util.h"

namespace ffpr::ff6 {
namespace {

constexpr uint8_t k6 = game::kFF6;

// --- game names ------------------------------------------------------------------------------
ref::Class  kStoneListBase{"Serial.FF6.UI", "MagicStoneListControllerBase", k6};
ref::Field  fListTargetData{kStoneListBase, "targetData"};
ref::Field  fListContentDataList{kStoneListBase, "contentDataList"};
ref::Field  fListSelectCursor{kStoneListBase, "selectCursor"};
ref::Class  kStoneList{"Serial.FF6.UI.KeyInput", "MagicStoneListController", k6};
ref::Field  fListView{kStoneList, "view"};
ref::Method mListSelectContent{kStoneList, "SelectContent", 2};
ref::Class  kStoneListView{"Serial.FF6.UI.KeyInput", "MagicStoneListView", k6};
ref::Field  fListViewMpValue{kStoneListView, "mpValueText"};

ref::Class  kStoneData{"Serial.FF6.UI", "MagicStoneListData", k6};
ref::Field  fDataContentId{kStoneData, "<ContentId>k__BackingField"};
ref::Field  fDataIsOwned{kStoneData, "<IsOwned>k__BackingField"};
ref::Field  fDataName{kStoneData, "<Name>k__BackingField"};
ref::Field  fDataDescription{kStoneData, "<Description>k__BackingField"};

ref::Class  kDetailsBase{"Serial.FF6.UI", "MagicStoneDetailsControllerBase", k6};
ref::Field  fDetailsCandidates{kDetailsBase, "candidates"};
ref::Class  kDetails{"Serial.FF6.UI.KeyInput", "MagicStoneDetailsController", k6};
ref::Field  fDetailsView{kDetails, "view"};
ref::Field  fDetailsSelectCursor{kDetails, "selectCursor"};
ref::Method mDetailsShow{kDetails, "Show", 3};
ref::Method mDetailsUpdateView{kDetails, "UpdateView", 2};
ref::Class  kDetailsView{"Serial.FF6.UI.KeyInput", "MagicStoneDetailsView", k6};
ref::Field  fDetailsViewContents{kDetailsView, "contents"};
ref::Field  fDetailsViewParameterValue{kDetailsView, "parameterValueText"};
ref::Class  kDetailsContent{"Serial.FF6.UI.KeyInput", "MagicStoneDetailsContentController", k6};
ref::Field  fContentView{kDetailsContent, "view"};
ref::Class  kDetailsContentView{"Serial.FF6.UI.KeyInput", "MagicStoneDetailsContentView", k6};
ref::Field  fContentViewMagicName{kDetailsContentView, "magicNameText"};
ref::Field  fContentViewSkillLevel{kDetailsContentView, "skillLevelText"};
ref::Field  fContentViewAcquisitionRate{kDetailsContentView, "acquisitionRateText"};
ref::Field  fContentViewMasterImage{kDetailsContentView, "masterImage"};
ref::Class  kIconTextView{"Last.UI", "IconTextView", k6};
ref::Field  fIconTextViewName{kIconTextView, "nameText"};

ref::Class  kOwnedCharacter{"Last.Data.User", "OwnedCharacterData", k6};
ref::Method mCharMagicStoneId{kOwnedCharacter, "get_MagicStoneId", 0};

// --- dedup contexts (the C#'s lastAnnouncement strings) -----------------------------------------
// The list's is cleared when the esper list is (re)entered or the window closes
// (ability_menu.cpp, reset_esper_list_guard); the details' by each UpdateView, as in the C#.
constexpr const char* kCtxList = "FF6.MagicStoneListController.SelectContent";
constexpr const char* kCtxDetails = "FF6.MagicStoneDetailsController.Show";
constexpr dedup::Scope kScope = dedup::Scope::Menu;

// --- state ------------------------------------------------------------------------------------
// Identities only (re-checked with unity::alive before any read).
void* g_list_controller = nullptr;      // the esper list (for the cursor suppressor)
void* g_details_controller = nullptr;   // the details panel last shown or updated
void* g_last_controller = nullptr;      // MagicStoneDetailsController_Show_Patch.lastController
std::wstring g_last_full;               // lastFullAnnouncement (the I-key re-read)

// lastMagicStoneData / lastTarget, copied out as ids.
bool g_has_esper = false;
int g_esper_id = 0;
bool g_has_target = false;
int g_target_id = 0;

// A Show whose announcement is still owed (see the file comment).
bool g_show_pending = false;

std::wstring strip(const std::wstring& s) { return battle::strip_icon(s); }
bool blank(const std::wstring& s) { return text::is_blank(s); }
std::wstring text_of(void* text_component) { return text_component ? unity::ui_text(text_component) : std::wstring(); }

int magic_stone_id(void* character) { return character ? ref::call<int32_t>(mCharMagicStoneId, character) : 0; }

// --- MagicStoneListController.SelectContent ---------------------------------------------------
void announce_list(void* self, int index)
{
    if (!self || !kStoneList.is(self)) return;
    g_list_controller = self;

    void* list = ref::rd<void*>(self, fListContentDataList);
    const int count = list ? unity::list_count(list) : 0;
    if (count <= 0 || index < 0 || index >= count) return;
    void* data = unity::list_at(list, index);
    if (!data) return;

    // The esper name (already localized in the data object).
    std::wstring name = ref::rd_str(data, fDataName);
    if (blank(name)) return;
    name = strip(name);
    if (blank(name)) return;
    std::wstring announcement = name;

    if (!ref::rd<bool>(data, fDataIsOwned)) {
        announcement += T(L" - NOT OBTAINED");
    } else {
        // Equipped to the character whose list this is.
        if (void* target = ref::rd<void*>(self, fListTargetData)) {
            const int equipped_id = magic_stone_id(target);
            const int this_id = ref::rd<int32_t>(data, fDataContentId);
            if (equipped_id == this_id && equipped_id > 0) announcement += T(L" - EQUIPPED");
        }
    }

    // The description: on focus with AutoDetail, always on the I key (FF1 behaviour).
    const std::wstring raw_description = ref::rd_str(data, fDataDescription);
    const std::wstring description = blank(raw_description) ? std::wstring() : strip(raw_description);
    set_ability_focus(self, description);
    if (auto_detail() && !blank(description)) announcement += L". " + description;

    // MP cost from the list view.
    if (void* view = ref::rd<void*>(self, fListView)) {
        const std::wstring raw_mp = text_of(ref::rd<void*>(view, fListViewMpValue));
        if (!blank(raw_mp)) {
            const std::wstring mp = strip(raw_mp);
            if (!blank(mp) && text::trim(mp) != L"0") announcement += TF(L", MP Cost {0}", {mp});
        }
    }

    if (!dedup::text(kCtxList, announcement, kScope)) return;
    ffpr::say(ui::menu_position(announcement, index, count));
}

// --- MagicStoneDetailsController: Show / UpdateView / ReadDetailsAfterDelay ---------------------

/// The learnable abilities: "Name, Mastered, x2" / "Name, 40%, x3".
std::vector<std::wstring> learnable_abilities(void* view)
{
    std::vector<std::wstring> abilities;
    void* contents = ref::rd<void*>(view, fDetailsViewContents);
    const int count = contents ? unity::list_count(contents) : 0;
    for (int i = 0; i < count; ++i) {
        void* content = unity::list_at(contents, i);
        if (!content || !unity::active_in_hierarchy(content)) continue;
        void* content_view = ref::rd<void*>(content, fContentView);
        if (!content_view) continue;

        std::wstring ability_name;
        if (void* icon_text = ref::rd<void*>(content_view, fContentViewMagicName))
            ability_name = text_of(ref::rd<void*>(icon_text, fIconTextViewName));
        std::wstring ap_cost = text_of(ref::rd<void*>(content_view, fContentViewAcquisitionRate));

        // Mastered: the star icon; otherwise the skill level text.
        std::wstring progress;
        void* master_image = ref::rd<void*>(content_view, fContentViewMasterImage);
        if (master_image && unity::active_in_hierarchy(master_image)) {
            progress = T(L"Mastered");
        } else if (void* level_text = ref::rd<void*>(content_view, fContentViewSkillLevel)) {
            const std::wstring level = unity::ui_text(level_text);
            if (!blank(level)) progress = text::trim(level) + L"%";
        }

        if (blank(ability_name)) continue;
        std::wstring entry = strip(ability_name);
        if (!blank(progress)) entry += L", " + progress;
        if (!blank(ap_cost)) entry += L", " + strip(ap_cost);
        abilities.push_back(entry);
    }
    return abilities;
}

/// ReadDetailsAfterDelay (after its one-frame wait).
void read_details(void* controller, bool announce)
{
    if (!controller || !unity::alive(controller)) return;
    void* view = ref::rd<void*>(controller, fDetailsView);
    if (!view) return;

    std::vector<std::wstring> details;

    // Who has this esper equipped (checked first).
    std::wstring equipped_status = T(L"Not equipped");
    void* candidates = ref::rd<void*>(controller, fDetailsCandidates);
    if (g_has_esper && candidates) {
        const int this_id = g_esper_id;
        const int count = unity::list_count(candidates);
        for (int i = 0; i < count; ++i) {
            void* character = unity::list_at(candidates, i);
            if (!character || this_id <= 0 || magic_stone_id(character) != this_id) continue;
            std::wstring character_name = battle::character_name(character);
            character_name = blank(character_name) ? T(L"Unknown") : strip(character_name);
            if (g_has_target && battle::character_id(character) == g_target_id)
                equipped_status = TF(L"EQUIPPED to {0}", {character_name});
            else
                equipped_status = TF(L"Equipped to {0}", {character_name});
            break;
        }
    }
    details.push_back(equipped_status);

    // Learnable abilities, all of them.
    const std::vector<std::wstring> abilities = learnable_abilities(view);
    if (!abilities.empty()) {
        std::wstring joined;
        for (size_t i = 0; i < abilities.size(); ++i) {
            if (i) joined += L", ";
            joined += abilities[i];
        }
        details.push_back(TF(L"Teaches {0} abilities: ", {text::num(static_cast<long long>(abilities.size()))}) + joined);
    }

    // Level-up bonus, last; hidden when this esper has none (the text is then stale).
    if (void* bonus_text = ref::rd<void*>(view, fDetailsViewParameterValue)) {
        const bool bonus_active = unity::active_in_hierarchy(bonus_text);
        const std::wstring raw_bonus = unity::ui_text(bonus_text);
        if (bonus_active && !blank(raw_bonus)) {
            const std::wstring bonus = strip(raw_bonus);
            if (!blank(bonus)) details.push_back(TF(L"Level up bonus: {0}", {bonus}));
        }
    }

    std::wstring announcement;
    for (size_t i = 0; i < details.size(); ++i) {
        if (i) announcement += L". ";
        announcement += details[i];
    }

    // Kept for the I-key re-read.
    g_last_full = announcement;
    g_last_controller = controller;

    // Spoken only when announcing (UpdateView's rebuild is silent).
    if (announce && dedup::text(kCtxDetails, announcement, kScope)) ffpr::say(announcement);
}

/// The Show / UpdateView parameters the read needs, copied out.
void remember(void* controller, void* target, void* magic_stone_data)
{
    g_details_controller = controller;
    g_has_esper = magic_stone_data != nullptr;
    g_esper_id = magic_stone_data ? ref::rd<int32_t>(magic_stone_data, fDataContentId) : 0;
    g_has_target = target != nullptr;
    g_target_id = target ? battle::character_id(target) : 0;
}

// --- detours -------------------------------------------------------------------------------------
using ListSelect_t = void (*)(void* self, int32_t index, int32_t scroll_type, const void* mi);
ListSelect_t o_ListSelectContent;
void d_ListSelectContent(void* self, int32_t index, int32_t scroll_type, const void* mi)
{
    o_ListSelectContent(self, index, scroll_type, mi);
    hooks::guarded("FF6.MagicStoneList.SelectContent", [&] { announce_list(self, index); });
}

using Show_t = void (*)(void* self, void* target, void* candidates, void* magic_stone_data, const void* mi);
Show_t o_DetailsShow;
void d_DetailsShow(void* self, void* target, void* candidates, void* magic_stone_data, const void* mi)
{
    o_DetailsShow(self, target, candidates, magic_stone_data, mi);
    hooks::guarded("FF6.MagicStoneDetails.Show", [&] {
        if (!self || !magic_stone_data || !kDetails.is(self)) return;
        remember(self, target, magic_stone_data);
        g_show_pending = true;
        void* controller = self;
        frame::after_frames(2, [controller] {
            if (!g_show_pending) return;   // an UpdateView took this Show's announcement
            g_show_pending = false;
            hooks::guarded("FF6.MagicStoneDetails.ShowRead", [&] { read_details(controller, true); });
        }, "FF6.EsperDetails.Show");
    });
}

using UpdateView_t = void (*)(void* self, void* target, void* magic_stone_data, const void* mi);
UpdateView_t o_DetailsUpdateView;
void d_DetailsUpdateView(void* self, void* target, void* magic_stone_data, const void* mi)
{
    o_DetailsUpdateView(self, target, magic_stone_data, mi);
    hooks::guarded("FF6.MagicStoneDetails.UpdateView", [&] {
        if (!self || !magic_stone_data || !kDetails.is(self)) return;
        remember(self, target, magic_stone_data);
        dedup::reset(kCtxDetails);   // the C# cleared lastAnnouncement here
        const bool announce = g_show_pending;
        g_show_pending = false;
        void* controller = self;
        frame::next_frame([controller, announce] {
            hooks::guarded("FF6.MagicStoneDetails.Read", [&] { read_details(controller, announce); });
        }, "FF6.EsperDetails.UpdateView");
    });
}

int install()
{
    int n = 0;
    n += hooks::attach(mListSelectContent, &d_ListSelectContent, &o_ListSelectContent) ? 1 : 0;
    n += hooks::attach(mDetailsShow, &d_DetailsShow, &o_DetailsShow) ? 1 : 0;
    n += hooks::attach(mDetailsUpdateView, &d_DetailsUpdateView, &o_DetailsUpdateView) ? 1 : 0;
    return n;
}

core::Install s_install{"FF6.AbilityEspers", &install, 521, game::kFF6};

} // namespace

std::wstring esper_details_reannounce_text()
{
    return hooks::guarded_r("FF6.EsperDetails.Reannounce", std::wstring(), []() -> std::wstring {
        void* c = g_last_controller;
        if (!c || !unity::alive(c) || !unity::active_in_hierarchy(c)) return {};
        return g_last_full;
    });
}

void reset_esper_list_guard() { dedup::reset(kCtxList); }

bool is_esper_cursor(void* cursor)
{
    if (!cursor) return false;
    return hooks::guarded_r("FF6.EsperCursor", false, [cursor] {
        if (g_list_controller && unity::alive(g_list_controller) &&
            ref::rd<void*>(g_list_controller, fListSelectCursor) == cursor)
            return true;
        if (g_details_controller && unity::alive(g_details_controller) &&
            ref::rd<void*>(g_details_controller, fDetailsSelectCursor) == cursor)
            return true;
        return false;
    });
}

} // namespace ffpr::ff6
