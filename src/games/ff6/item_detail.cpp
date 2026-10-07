// Ports FF6 Patches/ItemDetailPatches.cs and Menus/ItemDetailNavigator.cs (FF6ScreenReader, by
// ZKline): the item equipment detail screen (Last.UI.KeyInput.ItemEquipmentController), which
// opens when an equipment item is confirmed in the Items menu.
//
// UpdateView opens a section buffer three frames later (the C# coroutine's three
// `yield return null`, for the sprite swaps) and speaks its first section. SetParameterFlag (the
// in-game description / parameter toggle) rebuilds the buffer one frame later without speaking.
// Sections, in the C# order: name and quantity; the stat rows in four groups; "Properties: ...";
// one line per element category; "Magic: ..."; the description; the parameter message;
// "Can equip: ...". Every value is the screen's own text, except the element names: those are
// icons only, so they are mod text (the C# table of attributeType -> name, then the icon-name
// keywords).
//
// Navigation follows the core's (FF1's) section buffers (ui::NavigationBuffer, as the status and
// key-help readers), not the C#'s: Up/Down step and wrap, Ctrl+Up/Down jump to the first/last
// section, every line ends with ", (n of N)" (MenuPositionAnnouncements). The C# said
// "Top"/"Bottom" at the ends and spoke no position (lead, 2026-10-07: FF1 conventions).
// The keys are Global bindings (docs/INPUT_BINDINGS.md) to actions that do nothing while the
// buffer is closed; the C#'s auto-close (controller gone or inactive) runs on them, as in the C#.
// The C# had no controller binding for this buffer; none is added (no input context for it).
//
// The C# CursorNavigationPatches had no rule for this screen; the generic cursor reader is kept
// silent on it with a cursor suppressor (ItemDetailNavigator.ShouldSuppress), at the lead's
// request (2026-10-07). The I key keeps the core's item-menu entry (the item panel's
// description).
//
// The C# "{0}, quantity {1}" and "Properties: {0}" were not in FF6's mod_text.json (they were
// spoken as the English key); the same English line is built from the existing keys
// "quantity {0}" and "Properties: ".
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include "../../core/core.h"
#include "../../core/frame.h"
#include "../../core/hooks.h"
#include "../../core/log.h"
#include "../../core/modtext.h"
#include "../../core/refs.h"
#include "../../core/say.h"
#include "../../core/text.h"
#include "../../core/ui_util.h"
#include "../../core/unity.h"
#include "../../core/input/input.h"
#include "../../core/system/cursor_event.h"
#include "../../core/ui/navigation_buffer.h"

namespace ffpr::ff6 {
namespace {

constexpr uint8_t k6 = game::kFF6;

ref::Class  kItemEquipmentController{"Last.UI.KeyInput", "ItemEquipmentController", k6};
ref::Field  fDetailController{kItemEquipmentController, "detailController"};
ref::Field  fEquipableList{kItemEquipmentController, "equipableList"};
ref::Field  fTargetData{kItemEquipmentController, "targetData"};
ref::Method mUpdateView{kItemEquipmentController, "UpdateView", 2};
ref::Method mSetParameterFlag{kItemEquipmentController, "SetParameterFlag", 1};

ref::Class  kDetailController{"Last.UI.KeyInput", "ItemEquipmentDetailController", k6};
ref::Field  fDetailView{kDetailController, "view"};
ref::Class  kDetailView{"Last.UI.KeyInput", "ItemEquipmentDetailView", k6};
ref::Field  fStatusContentList{kDetailView, "statusContentList"};
ref::Field  fAbilityContentList{kDetailView, "abilityContentList"};
ref::Field  fMagicContentList{kDetailView, "magicContentList"};
ref::Field  fAttributeContentList{kDetailView, "attributeContentList"};

ref::Class  kStatusContent{"Last.UI", "ItemEquipmentStatusContentController", k6};
ref::Field  fStatusContentView{kStatusContent, "view"};
ref::Class  kStatusContentView{"Last.UI", "ItemEquipmentStatusContentView", k6};
ref::Field  fStatusNameText{kStatusContentView, "nameText"};
ref::Field  fStatusPlusText{kStatusContentView, "plusText"};
ref::Field  fStatusValueText{kStatusContentView, "valueText"};

ref::Class  kAbilityContent{"Last.UI", "ItemEquipmentAbilityContentController", k6};
ref::Field  fAbilityContentView{kAbilityContent, "view"};
ref::Class  kAbilityContentView{"Last.UI", "ItemEquipmentAbilityContentView", k6};
ref::Field  fAbilityNameText{kAbilityContentView, "nameText"};
ref::Field  fAbilityValueText{kAbilityContentView, "valueText"};

ref::Class  kMagicContent{"Last.UI", "ItemEquipmentMagicController", k6};
ref::Field  fMagicContentView{kMagicContent, "view"};
ref::Class  kMagicView{"Last.UI", "ItemEquipmentMagicView", k6};
ref::Field  fMagicNameText{kMagicView, "nameText"};
ref::Field  fMagicValueText{kMagicView, "valueText"};

ref::Class  kAttributeContent{"Last.UI", "ItemEquipmentAttributeController", k6};
ref::Field  fAttributeContentView{kAttributeContent, "view"};
ref::Class  kAttributeView{"Last.UI", "ItemEquipmentAttributeView", k6};
ref::Field  fAttributeNameText{kAttributeView, "nameText"};
ref::Field  fAttributeIconsList{kAttributeView, "iconsList"};
ref::Class  kAttributeImage{"Last.UI", "ItemEquipmentAttributeImage", k6};
ref::Field  fIconAttributeType{kAttributeImage, "attributeType"};
ref::Field  fIconImage{kAttributeImage, "image"};
ref::Field  fIconEnableIconName{kAttributeImage, "enableIconName"};
ref::Class  kImage{"UnityEngine.UI", "Image", k6};
ref::Method mImageGetSprite{kImage, "get_sprite", 0};

ref::Class  kEquipable{"Last.UI.KeyInput", "ItemEquipableController", k6};
ref::Field  fEquipableContentList{kEquipable, "contentList"};
ref::Class  kEquipableContent{"Last.UI.KeyInput", "ItemEquipableContentController", k6};
ref::Field  fEquipableContentView{kEquipableContent, "view"};
ref::Class  kEquipableContentView{"Last.UI.KeyInput", "ItemEquipableContentView", k6};
ref::Field  fEquipableNameText{kEquipableContentView, "nameText"};

ref::Class  kItemListContentData{"Last.UI", "ItemListContentData", k6};
ref::Field  fDataName{kItemListContentData, "<Name>k__BackingField"};
ref::Field  fDataDescription{kItemListContentData, "<Description>k__BackingField"};
ref::Field  fDataParameterMessage{kItemListContentData, "<ParameterMessage>k__BackingField"};
ref::Field  fDataCount{kItemListContentData, "<Count>k__BackingField"};

// ItemDetailNavigator state. The controller pointer is re-checked (alive + class) before use.
// g_sections is what BuildSections collects; g_buffer navigates it.
bool g_active = false;
std::vector<std::wstring> g_sections;
ui::NavigationBuffer g_buffer;
void* g_controller = nullptr;
// The controller of the last UpdateView: the screen the cursor suppressor checks (set at once,
// before the three-frame open).
void* g_screen = nullptr;

bool live_controller(void* c)
{
    return c && unity::alive(c) && kItemEquipmentController.is(c);
}

/// TextUtils.StripIconMarkup.
std::wstring strip_icon(const std::wstring& s)
{
    if (s.empty()) return {};
    return text::trim(text::strip_markup(s));
}

/// ItemDetailNavigator.GetTextSafe: a Text component's trimmed text, empty when blank.
std::wstring text_safe(void* text_component)
{
    if (!text_component) return {};
    std::wstring s = unity::ui_text(text_component);
    return text::is_blank(s) ? std::wstring() : text::trim(s);
}

/// string.Join(sep, parts).
std::wstring join(const std::vector<std::wstring>& parts, const wchar_t* sep)
{
    std::wstring out;
    for (size_t i = 0; i < parts.size(); ++i) {
        if (i) out += sep;
        out += parts[i];
    }
    return out;
}

/// The view of a list row that is shown (C#: `content?.view == null` and an inactive
/// gameObject are skipped). Null for a row to skip.
void* shown_row_view(void* content, ref::Field& view_field)
{
    if (!content || !unity::alive(content)) return nullptr;
    void* view = ref::rd<void*>(content, view_field);
    if (!view) return nullptr;
    if (!unity::active_in_hierarchy(content)) return nullptr;
    return view;
}

// --- sections -------------------------------------------------------------------------------

/// BuildNameSection: "Name, quantity N" (N > 0), else the name.
std::wstring name_section(void* controller)
{
    void* data = ref::rd<void*>(controller, fTargetData);
    if (!data) return {};
    std::wstring name = ref::rd_str(data, fDataName);
    if (name.empty()) return {};
    name = strip_icon(name);
    int count = ref::rd<int32_t>(data, fDataCount);
    return count > 0 ? name + L", " + TF(L"quantity {0}", {text::num(count)}) : name;
}

/// AddStatGroups: base, attack, defense and evasion rows, one section per non-empty group.
void add_stat_groups(void* detail_view)
{
    void* list = ref::rd<void*>(detail_view, fStatusContentList);
    int n = unity::list_count(list);
    if (n <= 0) return;

    std::vector<std::wstring> base_stats, attack_stats, defense_stats, evasion_stats;
    for (int i = 0; i < n; ++i) {
        void* view = shown_row_view(unity::list_at(list, i), fStatusContentView);
        if (!view) continue;
        std::wstring stat_name = text_safe(ref::rd<void*>(view, fStatusNameText));
        std::wstring stat_value = text_safe(ref::rd<void*>(view, fStatusValueText));
        std::wstring plus = text_safe(ref::rd<void*>(view, fStatusPlusText));
        if (stat_name.empty()) continue;

        std::wstring formatted;
        if (!plus.empty() && !stat_value.empty()) formatted = stat_name + L" " + plus + stat_value;
        else if (!stat_value.empty()) formatted = stat_name + L" " + stat_value;
        else formatted = stat_name;

        // Categorize by name keywords (the C#'s English keywords).
        std::wstring lower = text::lower(stat_name);
        if (text::contains(lower, L"attack")) attack_stats.push_back(formatted);
        else if (text::contains(lower, L"defense") || text::contains(lower, L"def")) defense_stats.push_back(formatted);
        else if (text::contains(lower, L"evasion") || text::contains(lower, L"eva")) evasion_stats.push_back(formatted);
        else base_stats.push_back(formatted);
    }
    if (!base_stats.empty()) g_sections.push_back(join(base_stats, L". "));
    if (!attack_stats.empty()) g_sections.push_back(join(attack_stats, L". "));
    if (!defense_stats.empty()) g_sections.push_back(join(defense_stats, L". "));
    if (!evasion_stats.empty()) g_sections.push_back(join(evasion_stats, L". "));
}

/// "name value" rows of a name/value list (abilities, magic), joined with ". ".
std::wstring name_value_rows(void* list, ref::Field& view_field, ref::Field& name_field, ref::Field& value_field)
{
    std::vector<std::wstring> parts;
    int n = unity::list_count(list);
    for (int i = 0; i < n; ++i) {
        void* view = shown_row_view(unity::list_at(list, i), view_field);
        if (!view) continue;
        std::wstring name = text_safe(ref::rd<void*>(view, name_field));
        std::wstring value = text_safe(ref::rd<void*>(view, value_field));
        if (name.empty()) continue;
        parts.push_back(value.empty() ? name : name + L" " + value);
    }
    return join(parts, L". ");
}

/// BuildAbilitiesSection: "Properties: ...".
std::wstring abilities_section(void* detail_view)
{
    std::wstring rows = name_value_rows(ref::rd<void*>(detail_view, fAbilityContentList), fAbilityContentView,
                                        fAbilityNameText, fAbilityValueText);
    return rows.empty() ? std::wstring() : T(L"Properties: ") + rows;
}

/// BuildMagicSection: "Magic: ...".
std::wstring magic_section(void* detail_view)
{
    std::wstring rows = name_value_rows(ref::rd<void*>(detail_view, fMagicContentList), fMagicContentView,
                                        fMagicNameText, fMagicValueText);
    return rows.empty() ? std::wstring() : TF(L"Magic: {0}", {rows});
}

/// ResolveElementName: the hard-coded attributeType table (the sprite names are numbered), then
/// keywords in enableIconName, then "Element N".
std::wstring element_name(const std::wstring& enable_icon_name, int attribute_type)
{
    switch (attribute_type) {
    case 1: return T(L"Fire");
    case 5: return T(L"Ice");
    case 3: return T(L"Lightning");
    case 6: return T(L"Poison");
    case 10: return T(L"Wind");
    case 8: return T(L"Holy");
    case 7: return T(L"Earth");
    case 9: return T(L"Water");
    default: break;
    }
    if (!enable_icon_name.empty()) {
        static const wchar_t* const kKeywords[] = {L"fire", L"ice", L"lightning", L"thunder", L"poison",
                                                   L"wind", L"holy", L"earth", L"water"};
        static const wchar_t* const kNames[] = {L"Fire", L"Ice", L"Lightning", L"Lightning", L"Poison",
                                                L"Wind", L"Holy", L"Earth", L"Water"};
        std::wstring lower = text::lower(enable_icon_name);
        for (size_t i = 0; i < sizeof(kKeywords) / sizeof(kKeywords[0]); ++i)
            if (text::contains(lower, kKeywords[i])) return T(kNames[i]);
    }
    return TF(L"Element {0}", {text::num(attribute_type)});
}

/// An attribute icon is enabled when its image shows the enable sprite ("(Clone)" stripped).
bool icon_enabled(void* icon, std::wstring& enable_name)
{
    void* enable_str = ref::rd<void*>(icon, fIconEnableIconName);
    enable_name = ref::str(enable_str);
    void* image = ref::rd<void*>(icon, fIconImage);
    if (!enable_str || !image || !unity::alive(image)) return false;
    void* sprite = ref::call<void*>(mImageGetSprite, image);
    if (!sprite || !unity::alive(sprite)) return false;
    std::wstring sprite_name = unity::name(sprite);
    if (text::ends_with(sprite_name, L"(Clone)")) sprite_name.resize(sprite_name.size() - 7);
    return sprite_name == enable_name;
}

/// AddAttributeSections: "<category>: Fire, Ice" or "<category>: none", one per category.
void add_attribute_sections(void* detail_view)
{
    void* list = ref::rd<void*>(detail_view, fAttributeContentList);
    int n = unity::list_count(list);
    for (int i = 0; i < n; ++i) {
        void* view = shown_row_view(unity::list_at(list, i), fAttributeContentView);
        if (!view) continue;
        std::wstring category = text_safe(ref::rd<void*>(view, fAttributeNameText));
        if (category.empty()) continue;

        std::vector<std::wstring> enabled;
        void* icons = ref::rd<void*>(view, fAttributeIconsList);
        int m = unity::list_count(icons);
        for (int j = 0; j < m; ++j) {
            void* icon = unity::list_at(icons, j);
            if (!icon) continue;
            std::wstring enable_name;
            bool on = hooks::guarded_r("FF6.ItemDetailNavigator.Icon", false, [&] { return icon_enabled(icon, enable_name); });
            if (!on) continue;
            enabled.push_back(element_name(enable_name, ref::rd<int32_t>(icon, fIconAttributeType)));
        }
        g_sections.push_back(TF(L"{0}: {1}", {category, enabled.empty() ? T(L"none") : join(enabled, L", ")}));
    }
}

/// GetDescription / GetParameterMessage: a targetData string, markup stripped.
std::wstring target_data_text(void* controller, ref::Field& field)
{
    void* data = ref::rd<void*>(controller, fTargetData);
    if (!data) return {};
    return strip_icon(ref::rd_str(data, field));
}

/// BuildEquippableSection: the character names on every page of the equipable list.
std::wstring equippable_section(void* controller)
{
    void* pages = ref::rd<void*>(controller, fEquipableList);
    int n = unity::list_count(pages);
    if (n <= 0) return {};
    std::vector<std::wstring> names;
    for (int i = 0; i < n; ++i) {
        void* equipable = unity::list_at(pages, i);
        void* contents = equipable ? ref::rd<void*>(equipable, fEquipableContentList) : nullptr;
        int m = unity::list_count(contents);
        for (int j = 0; j < m; ++j) {
            void* view = shown_row_view(unity::list_at(contents, j), fEquipableContentView);
            if (!view) continue;
            std::wstring name = text_safe(ref::rd<void*>(view, fEquipableNameText));
            if (!name.empty()) names.push_back(name);
        }
    }
    return names.empty() ? std::wstring() : TF(L"Can equip: {0}", {join(names, L", ")});
}

void add_if(const std::wstring& section)
{
    if (!section.empty()) g_sections.push_back(section);
}

/// BuildSections (each C# try/catch is a guarded step).
void build_sections(void* controller)
{
    g_sections.clear();
    hooks::guarded("FF6.ItemDetailNavigator.Name", [&] { add_if(name_section(controller)); });

    void* detail_controller = ref::rd<void*>(controller, fDetailController);
    void* detail_view = detail_controller ? ref::rd<void*>(detail_controller, fDetailView) : nullptr;
    if (detail_view) {
        hooks::guarded("FF6.ItemDetailNavigator.Stats", [&] { add_stat_groups(detail_view); });
        hooks::guarded("FF6.ItemDetailNavigator.Abilities", [&] { add_if(abilities_section(detail_view)); });
        hooks::guarded("FF6.ItemDetailNavigator.Attributes", [&] { add_attribute_sections(detail_view); });
        hooks::guarded("FF6.ItemDetailNavigator.Magic", [&] { add_if(magic_section(detail_view)); });
    }

    hooks::guarded("FF6.ItemDetailNavigator.Description", [&] { add_if(target_data_text(controller, fDataDescription)); });
    hooks::guarded("FF6.ItemDetailNavigator.Parameter", [&] { add_if(target_data_text(controller, fDataParameterMessage)); });
    hooks::guarded("FF6.ItemDetailNavigator.Equippable", [&] { add_if(equippable_section(controller)); });

    // The buffer: pre-rendered entries, no groups (as the key-help reader).
    std::vector<ui::NavigationBuffer::Entry> entries;
    entries.reserve(g_sections.size());
    for (const std::wstring& s : g_sections) entries.push_back([s] { return s; });
    g_buffer = ui::NavigationBuffer(std::move(entries));
}

// --- ItemDetailNavigator --------------------------------------------------------------------

/// Speak a buffer entry with its ", (n of N)" (the core's buffer readers).
void speak_entry(const std::wstring& entry)
{
    if (entry.empty()) return;
    auto [index, count] = g_buffer.current_group_position();
    ffpr::say(ui::menu_position(entry, index, count), true);
}

void close()
{
    g_active = false;
    g_sections.clear();
    g_buffer = ui::NavigationBuffer();
    g_controller = nullptr;
    LOGF("[ItemDetailNavigator] Closed");
}

void open(void* controller)
{
    g_controller = controller;
    build_sections(controller);
    if (g_buffer.empty()) {
        LOGF("[ItemDetailNavigator] No sections built, not activating");
        return;
    }
    g_buffer.set_index(0);
    g_active = true;
    LOGF("[ItemDetailNavigator] Opened with %d sections", g_buffer.count());
    speak_entry(g_buffer.current());
}

void refresh()
{
    if (!g_active || !live_controller(g_controller)) return;
    const int previous = g_buffer.index();
    build_sections(g_controller);
    const int n = g_buffer.count();
    g_buffer.set_index(previous >= n ? (n > 0 ? n - 1 : 0) : previous);
}

/// HandleInput's auto-close: false (and closed) when the screen is gone.
bool still_open()
{
    if (!g_active) return false;
    if (!live_controller(g_controller) || !unity::active_in_hierarchy(g_controller)) {
        close();
        return false;
    }
    return !g_buffer.empty();
}

using Op = std::wstring (ui::NavigationBuffer::*)();

void move(Op op)
{
    if (!still_open()) return;
    speak_entry((g_buffer.*op)());
}

/// The generic cursor reader stays silent while the detail screen is shown.
bool should_suppress()
{
    if (!live_controller(g_screen)) {
        g_screen = nullptr;
        return false;
    }
    return unity::active_in_hierarchy(g_screen);
}

// --- ItemEquipmentController.UpdateView(ItemListContentData data, bool isParamter) postfix ---
using UpdateView_t = void (*)(void* self, void* data, bool is_paramter, const void* mi);
UpdateView_t o_UpdateView = nullptr;

void d_UpdateView(void* self, void* data, bool is_paramter, const void* mi)
{
    o_UpdateView(self, data, is_paramter, mi);
    hooks::guarded("ItemDetailPatches.AfterUpdateView", [&] {
        if (!self) return;
        g_screen = self;
        LOGF("[ItemDetailPatches] UpdateView called, isParameter=%d", is_paramter ? 1 : 0);
        // DelayedOpen: three frames for the sprite swaps, then open if the screen is still up.
        frame::after_frames(3, [self] {
            hooks::guarded("ItemDetailPatches.DelayedOpen", [&] {
                if (live_controller(self) && unity::active_in_hierarchy(self)) open(self);
            });
        }, "FF6.ItemDetail.Open");
    });
}

// --- ItemEquipmentController.SetParameterFlag(bool value) postfix ---------------------------
using SetParameterFlag_t = void (*)(void* self, bool value, const void* mi);
SetParameterFlag_t o_SetParameterFlag = nullptr;

void d_SetParameterFlag(void* self, bool value, const void* mi)
{
    o_SetParameterFlag(self, value, mi);
    hooks::guarded("ItemDetailPatches.AfterSetParameterFlag", [&] {
        if (!g_active) return;
        LOGF("[ItemDetailPatches] SetParameterFlag=%d, refreshing navigator", value ? 1 : 0);
        frame::next_frame([] { hooks::guarded("ItemDetailPatches.DelayedRefresh", [] { refresh(); }); },
                          "FF6.ItemDetail.Refresh");
    });
}

int install()
{
    actions::define("ItemDetailNavigator.NavigateNext", [] { move(&ui::NavigationBuffer::next); });
    actions::define("ItemDetailNavigator.NavigatePrevious", [] { move(&ui::NavigationBuffer::previous); });
    actions::define("ItemDetailNavigator.JumpToTop", [] { move(&ui::NavigationBuffer::jump_top); });
    actions::define("ItemDetailNavigator.JumpToBottom", [] { move(&ui::NavigationBuffer::jump_bottom); });
    actions::define("ItemDetailNavigator.Close", [] { close(); });
    queries::define("ItemDetailNavigator.IsActive", [] { return g_active; });
    queries::define("ItemDetailNavigator.ShouldSuppress", [] { return should_suppress(); });
    cursor::add_suppressor("ItemDetailNavigator.ShouldSuppress");

    // Global, so they reach the item menu; Field (Ctrl+arrows teleport) and the buffer contexts
    // (Status, BestiaryDetail, KeyHelp) keep their own bindings first.
    input::add_binding(input::Key::DownArrow, input::Mod::Ctrl, input::Context::Global, "ItemDetailNavigator.JumpToBottom",
                       "Jump to last item detail");
    input::add_binding(input::Key::DownArrow, input::Mod::None, input::Context::Global, "ItemDetailNavigator.NavigateNext",
                       "Next item detail");
    input::add_binding(input::Key::UpArrow, input::Mod::Ctrl, input::Context::Global, "ItemDetailNavigator.JumpToTop",
                       "Jump to first item detail");
    input::add_binding(input::Key::UpArrow, input::Mod::None, input::Context::Global, "ItemDetailNavigator.NavigatePrevious",
                       "Previous item detail");

    int n = 0;
    n += hooks::attach(mUpdateView, &d_UpdateView, &o_UpdateView) ? 1 : 0;             // one RVA in FF6
    n += hooks::attach(mSetParameterFlag, &d_SetParameterFlag, &o_SetParameterFlag) ? 1 : 0;   // one RVA in FF6
    return n;
}

core::Install s_install{"FF6.ItemDetail", &install, 520, game::kFF6};

} // namespace
} // namespace ffpr::ff6
