// FF6 Esper list and acquired-magic page: current rendered fields and markers.
#include "ability.h"
#include <climits>
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
#include "../../core/displayed_ui.h"
#include "../../core/ui/review_buffer.h"

namespace ffpr::ff6 {
namespace {
constexpr uint8_t k6 = game::kFF6;
ref::Class kStoneListBase{"Serial.FF6.UI", "MagicStoneListControllerBase", k6};
ref::Field fListContentDataList{kStoneListBase, "contentDataList"}, fListSelectCursor{kStoneListBase, "selectCursor"};
ref::Class kStoneList{"Serial.FF6.UI.KeyInput", "MagicStoneListController", k6};
ref::Method mListSelectContent{kStoneList, "SelectContent", 2};
ref::Class kStoneContent{"Serial.FF6.UI.KeyInput", "MagicStoneListContentController", k6};
ref::Class kStoneData{"Serial.FF6.UI", "MagicStoneListData", k6};
ref::Field fStoneId{kStoneData, "<ContentId>k__BackingField"}, fStoneOwned{kStoneData, "<IsOwned>k__BackingField"};
ref::Class kScroll{"Last.UI", "InfiniteScrollView", k6};
ref::Method mGetContent{kScroll, "GetContentObject", 1};
ref::Class kDetails{"Serial.FF6.UI.KeyInput", "MagicStoneDetailsController", k6};
ref::Method mDetailsShow{kDetails, "Show", 3}, mDetailsUpdateView{kDetails, "UpdateView", 2};
ref::Method mDetailsReady{kDetails, "WaitInputInit", 0};
ref::Class kWindow{"Serial.FF6.UI.KeyInput", "AbilityWindowController", k6};
ref::Method mWindowActive{kWindow, "SetActive", 1};
void* g_list_controller = nullptr, *g_details_controller = nullptr, *g_last_controller = nullptr;
std::wstring g_last_full;
uint64_t g_generation = 0;
bool g_show_pending = false;
int g_details_id = -1;
constexpr const char* kCtxList = "FF6.MagicStoneListController.SelectContent";

std::wstring markers(void* view, const char* other) {
    std::wstring result;
    if (displayed::image_visible(displayed::field(view, "equipIcon")) ||
        displayed::image_visible(displayed::field(view, other))) result = T(L"Equipped");
    if (displayed::image_visible(displayed::field(view, "masterImage")))
        result += (result.empty() ? L"" : L", ") + T(L"Mastered");
    return result;
}
void add(std::vector<std::wstring>& rows, std::wstring value) {
    if (!value.empty()) rows.push_back(std::move(value));
}
void footer(void* view, std::vector<std::wstring>& rows) {
    add(rows, displayed::text_field(view, "descriptionText"));
    add(rows, displayed::pair(view, "fixedMpText", "mpValueText"));
}
void clear_buffer() {
    if ((unity::alive(g_list_controller) && ui::review::owns(displayed::field(g_list_controller, "view"))) ||
        (unity::alive(g_details_controller) && ui::review::owns(displayed::field(g_details_controller, "view")))) ui::review::clear();
    g_last_controller = nullptr;
    g_last_full.clear();
}
void buffer(void* controller, void* view, const std::wstring& heading, std::vector<std::wstring> rows, bool announce) {
    if (!displayed::container_visible(view) || heading.empty()) return;
    g_last_controller = controller;
    g_last_full = heading + (rows.empty() ? L"" : L". " + text::join(rows, L". "));
    ui::review::Options options;
    options.domain = ui::review::Domain::Information;
    options.include_headline = true;
    options.announce = announce;
    ui::review::show(view, heading, std::move(rows), std::move(options));
}
bool announce_list(void* self, int index) {
    if (!unity::alive(self) || !kStoneList.is(self) || !unity::active_in_hierarchy(self)) return false;
    void* view = displayed::field(self, "view");
    if (!displayed::container_visible(view)) return false;
    void* data = ref::rd<void*>(self, fListContentDataList);
    const int count = unity::list_count(data);
    if (count <= 0 || count > 128 || index < 0 || index >= count) return false;
    // SelectContent uses this exact native lookup. Its cursor parents are separate
    // GameObjects, so GetComponentInParent(cursor) cannot identify the Esper row.
    void* go = ref::call<void*>(mGetContent, displayed::field(view, "scrollView"), index);
    void* row = unity::get_component(go, kStoneContent);
    void* row_view = displayed::field(row, "view");
    if (!displayed::container_visible(row_view)) return false; // virtual row is not ready
    auto name = displayed::text_field(displayed::field(row_view, "IconTextView"), "nameText");
    if (name.empty()) {
        void* selected = unity::list_at(data, index);
        if (!selected || ref::rd<bool>(selected, fStoneOwned)) return false;
        // ContentUpdate explicitly renders an empty name for an unowned slot.
        // Do not read its hidden Name, description, cost or equipped state.
        ++g_generation; g_show_pending = false; g_list_controller = self;
        set_ability_focus(self, {});
        buffer(self, view, T(L"Empty"), {}, false);
        dedup::reset(kCtxList); // return to the prior named Esper is a new focus
        ffpr::say(ui::menu_position(T(L"Empty"), index, count));
        return true;
    }
    const auto status = markers(row_view, "targetEquipIcon");
    if (!status.empty()) name += L", " + status;
    ++g_generation;
    g_show_pending = false;
    g_list_controller = self;
    auto description = displayed::text_field(view, "descriptionText");
    set_ability_focus(self, description);
    std::vector<std::wstring> rows;
    footer(view, rows);
    const auto cost = displayed::pair(view, "fixedMpText", "mpValueText");
    auto focus = name + (cost.empty() ? L"" : L", " + cost);
    if (auto_detail() && !description.empty()) focus += L". " + description;
    buffer(self, view, name, std::move(rows), false);
    if (dedup::text(kCtxList, focus, dedup::Scope::Menu)) ffpr::say(ui::menu_position(focus, index, count));
    return true;
}
bool read_details(void* self, bool announce) {
    if (!unity::alive(self) || !unity::active_in_hierarchy(self)) return false;
    void* view = displayed::field(self, "view");
    if (!displayed::container_visible(view)) return false;
    auto heading = displayed::text_field(view, "nameText");
    if (heading.empty()) return false;
    auto status = markers(view, "targetEquipeedIcon");
    if (!status.empty()) heading += L", " + status;
    std::vector<std::wstring> rows;
    auto progress_label = displayed::text_field(view, "skillLevelText");
    auto rate_label = displayed::text_field(view, "acquisitionRatetext");
    add(rows, progress_label + (rate_label.empty() ? L"" : L", " + rate_label));
    void* contents = displayed::field(view, "contents");
    const int count = unity::list_count(contents);
    if (count < 0 || count > 32) return false;
    for (int i = 0; i < count; ++i) {
        void* row_view = displayed::field(unity::list_at(contents, i), "view");
        auto name = displayed::text_field(displayed::field(row_view, "magicNameText"), "nameText");
        if (name.empty()) continue;
        auto progress = displayed::text_field(row_view, "skillLevelText") + displayed::text_field(row_view, "percentText");
        auto rate = displayed::text_field(row_view, "multiplicationText") + L" " + displayed::text_field(row_view, "acquisitionRateText");
        if (!progress.empty()) name += L", " + progress;
        if (!text::trim(rate).empty()) name += L", " + text::trim(rate);
        if (displayed::image_visible(displayed::field(row_view, "masterImage"))) name += L", " + T(L"Mastered");
        add(rows, std::move(name));
    }
    std::vector<std::wstring> bonus;
    for (const char* field : {"levelUpText", "parameterNameText", "parameterFixText", "parameterValueText"})
        add(bonus, displayed::text_field(view, field));
    add(rows, text::join(bonus, L" "));
    footer(view, rows);
    set_ability_focus(self, displayed::text_field(view, "descriptionText"));
    buffer(self, view, heading, std::move(rows), announce);
    return true;
}
using ListSelect = void (*)(void*, int32_t, int32_t, const void*);
ListSelect o_ListSelectContent;
void d_ListSelectContent(void* self, int32_t index, int32_t scroll_type, const void* mi) {
    hooks::guarded("FF6.Esper.Selection", [self] {
        if (!kStoneList.is(self)) return;
        ++g_generation; g_show_pending = false;
        clear_buffer(); g_list_controller = self;
        set_ability_focus(self, {});
    });
    o_ListSelectContent(self, index, scroll_type, mi);
    hooks::guarded("FF6.Esper.List", [&] { announce_list(self, index); });
}
using Show = void (*)(void*, void*, void*, void*, const void*);
Show o_DetailsShow;
void d_DetailsShow(void* self, void* target, void* candidates, void* data, const void* mi) {
    hooks::guarded("FF6.Esper.Show", [self] {
        if (kDetails.is(self)) { ++g_generation; clear_buffer(); set_ability_focus(self, {}); g_details_controller = self; g_show_pending = true; g_details_id = -1; }
    });
    o_DetailsShow(self, target, candidates, data, mi);
}
void schedule_details(void* self) {
    const auto generation = ++g_generation;
    frame::cancel_label("FF6.EsperDetails.UpdateView");
    frame::every_frame([self, generation] {
        if (generation != g_generation || !unity::alive(self) || !unity::active_in_hierarchy(self)) return false;
        if (!read_details(self, g_show_pending)) return true;
        g_show_pending = false;
        return false;
    }, "FF6.EsperDetails.UpdateView", INT_MAX);
}
using Update = void (*)(void*, void*, void*, const void*);
Update o_DetailsUpdateView;
void d_DetailsUpdateView(void* self, void* target, void* data, const void* mi) {
    o_DetailsUpdateView(self, target, data, mi);
    hooks::guarded("FF6.Esper.UpdateView", [self, data] {
        if (!kDetails.is(self)) return;
        const int id = ref::rd<int32_t>(data, fStoneId);
        g_show_pending |= g_details_controller != self || id != g_details_id;
        g_details_id = id;
        g_details_controller = self;
        schedule_details(self);
    });
}
using Ready = void (*)(void*, const void*);
Ready o_details_ready;
void d_details_ready(void* self, const void* mi) {
    o_details_ready(self, mi);
    hooks::guarded("FF6.Esper.Ready", [self] {
        // WaitInputInit finishes equipped/mastered markers after UpdateView.
        if (kDetails.is(self)) schedule_details(self);
    });
}
using Active = void (*)(void*, bool, const void*);
Active o_window_active;
void d_window_active(void* self, bool active, const void* mi) {
    o_window_active(self, active, mi);
    hooks::guarded("FF6.Esper.Close", [self, active] {
        if (kWindow.is(self) && !active) reset_esper_list_guard();
    });
}
int install() {
    return (hooks::attach(mListSelectContent, &d_ListSelectContent, &o_ListSelectContent) ? 1 : 0) +
        (hooks::attach(mDetailsShow, &d_DetailsShow, &o_DetailsShow) ? 1 : 0) +
        (hooks::attach(mDetailsUpdateView, &d_DetailsUpdateView, &o_DetailsUpdateView) ? 1 : 0) +
        (hooks::attach(mDetailsReady, &d_details_ready, &o_details_ready) ? 1 : 0) +
        (hooks::attach(mWindowActive, &d_window_active, &o_window_active) ? 1 : 0);
}
core::Install s_install{"FF6.AbilityEspers", &install, 521, k6};
}
std::wstring esper_details_reannounce_text() {
    return unity::alive(g_last_controller) && unity::active_in_hierarchy(g_last_controller) && displayed::container_visible(displayed::field(g_last_controller, "view")) ? g_last_full : std::wstring();
}
void refresh_esper_list(void* window) {
    void* self = displayed::field(window, "magicStoneListController");
    if (!unity::alive(self) || !g_last_full.empty()) return;
    g_list_controller = self;
    const auto generation = g_generation;
    frame::cancel_label("FF6.Esper.ListReady");
    frame::every_frame([self, generation] {
        if (generation != g_generation || !unity::alive(self) || !unity::active_in_hierarchy(self)) return false;
        return !announce_list(self, ui::cursor_index(ref::rd<void*>(self, fListSelectCursor)));
    }, "FF6.Esper.ListReady", INT_MAX);
}
void reset_esper_list_guard() {
    ++g_generation; g_show_pending = false;
    g_details_id = -1;
    dedup::reset(kCtxList);
    frame::cancel_label("FF6.Esper.ListReady");
    frame::cancel_label("FF6.EsperDetails.UpdateView");
    clear_buffer();
    g_list_controller = g_details_controller = g_last_controller = nullptr;
    g_last_full.clear();
}
bool is_esper_cursor(void* cursor) {
    return cursor && ((unity::alive(g_list_controller) && ref::rd<void*>(g_list_controller, fListSelectCursor) == cursor) ||
        (unity::alive(g_details_controller) && displayed::field(g_details_controller, "selectCursor") == cursor));
}
}
