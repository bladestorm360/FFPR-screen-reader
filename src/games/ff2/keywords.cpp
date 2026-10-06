// Ports FF2 Patches/KeywordPatches.cs (KeywordMenuState, WordsMenuState, KeywordPatches), the
// keyword/Words parts of Core/CursorSuppressionCheck.cs, and the FF2 MenuTransitionPatches
// postfix WordsWindowController.SetActive(false) (Core/FFII_ScreenReaderMod.cs).
//
// Keywords are FF2's own system: Ask / Learn / Key Items in NPC dialogue (SecretWordController)
// and the Words menu (WordsWindowController) in the field menu.
//
// Differences from the C#, both forced by the core:
//  * The C# patched SecretWordController.SetActive(bool) to clear the keyword state, but the
//    KeyInput SecretWordController has no SetActive, so that patch never applied; the state was
//    cleared by ShouldSuppress's self-check and by FF2's ControllerRouter running that check
//    every frame on the field. The core's key context treats any active menu state as "not
//    field", with no such self-check, so a stale KEYWORD_MENU would block every field key. The
//    same clear therefore runs from the events that end the menu: SecretWordController.NoneInit
//    (state None) and Close.
//  * The I key: the C# read MenuDetailCache for these lists. The core's I-key cascade has no
//    entry for a game module; this file publishes "KeywordMenuState.IsActive",
//    "WordsMenuState.IsActive" and the text "MenuDetailCache.LastDetail" for it (reported).
//
// No hook here is folded: one RVA each in FF2's dump.cs.
#include <cstdint>
#include <string>
#include "ff2.h"
#include "../../core/core.h"
#include "../../core/dedup.h"
#include "../../core/frame.h"
#include "../../core/hooks.h"
#include "../../core/il2cpp.h"
#include "../../core/log.h"
#include "../../core/modtext.h"
#include "../../core/refs.h"
#include "../../core/say.h"
#include "../../core/text.h"
#include "../../core/ui_util.h"
#include "../../core/unity.h"
#include "../../core/system/cursor_event.h"
#include "../../core/ui/preferences.h"

namespace ffpr::ff2 {
namespace {

// --- game names ------------------------------------------------------------------------------
ref::Class kSecretWordBase{"Last.UI", "SecretWordControllerBase", game::kFF2};
ref::Field fSwStateMachine{kSecretWordBase, "stateMachine"};
ref::Field fSwWordDataList{kSecretWordBase, "wordDataList"};
ref::Field fSwItemDataList{kSecretWordBase, "itemDataList"};

ref::Class kSecretWord{"Last.UI.KeyInput", "SecretWordController", game::kFF2};
ref::Method mSwSelectCommand{kSecretWord, "SelectCommand", 1};
ref::Method mSwCommandSelectingInit{kSecretWord, "CommandSelectingInit", 0};
ref::Method mSwSelectContentByWord{kSecretWord, "SelectContentByWord", 2};
ref::Method mSwSelectContentByItem{kSecretWord, "SelectContentByItem", 2};
ref::Method mSwNoneInit{kSecretWord, "NoneInit", 0};
ref::Method mSwClose{kSecretWord, "Close", 0};

ref::Class kSelectFieldData{"Last.UI", "SelectFieldContentManager.SelectFieldContentData", game::kFF2};
ref::Field fSfNameMessageId{kSelectFieldData, "<NameMessageId>k__BackingField"};
ref::Field fSfDescriptionMessageId{kSelectFieldData, "<DescriptionMessageId>k__BackingField"};

ref::Class kItemListContentData{"Last.UI", "ItemListContentData", game::kFF2};
ref::Field fIlName{kItemListContentData, "<Name>k__BackingField"};
ref::Field fIlDescription{kItemListContentData, "<Description>k__BackingField"};

ref::Class kWordsList{"Last.UI.KeyInput", "WordsContentListController", game::kFF2};
ref::Field fWlView{kWordsList, "view"};
ref::Field fWlContentList{kWordsList, "contentList"};
ref::Field fWlSelectCursor{kWordsList, "selectCursor"};
ref::Field fWlDictionary{kWordsList, "keyWordContentDictionary"};
ref::Method mWlSetCommandSelectCursor{kWordsList, "SetCommandSelectCursor", 0};

ref::Class kWordsListView{"Last.UI.KeyInput", "WordsContentListView", game::kFF2};
ref::Field fWlvDescriptionText{kWordsListView, "descriptionText"};

ref::Class kCommonCommandContent{"Last.UI.KeyInput", "CommonCommandContentController", game::kFF2};
ref::Field fCccName{kCommonCommandContent, "<Name>k__BackingField"};

ref::Class kWordsWindow{"Last.UI.KeyInput", "WordsWindowController", game::kFF2};
ref::Method mWwExitSelect{kWordsWindow, "ExitSelect", 0};
ref::Method mWwSetActive{kWordsWindow, "SetActive", 1};

ref::Class kContent{"Last.Data.Master", "Content", game::kFF2};
ref::Method mContentMesIdName{kContent, "get_MesIdName", 0};
ref::Method mContentMesIdDescription{kContent, "get_MesIdDescription", 0};

ref::Class kTouchWordsList{"Last.UI.Touch", "WordsContentListController", game::kFF2};
ref::Field fTwlView{kTouchWordsList, "view"};
ref::Field fTwlContentList{kTouchWordsList, "contentList"};
ref::Method mTwlSetSelectContent{kTouchWordsList, "SetSelectContent", 1};
ref::Method mTwlUpdateView{kTouchWordsList, "UpdateView", 1};
ref::Class kTouchWordsListView{"Last.UI.Touch", "WordsContentListView", game::kFF2};
ref::Field fTwlvDescriptionText{kTouchWordsListView, "descriptionText"};
ref::Class kTouchWordsContent{"Last.UI.Touch", "WordsContentController", game::kFF2};
ref::Method mTwcNameText{kTouchWordsContent, "get_NameText", 0};

// SecretWordControllerBase.State
constexpr int STATE_NONE = 0;
constexpr int STATE_COMMAND_SELECT = 1;
constexpr int STATE_COMMAND_SELECTING = 2;

// WordsMenuState._lastIndex: one keyword per index (SetCommandSelectCursor repeats). Reset when
// the Words state clears (its reset handler).
constexpr const char* kCtxWordsIndex = "FF2.Words.Index";

bool g_words_exit_in_progress = false;   // WordsWindowController.ExitSelect is running

// --- KeywordMenuState ------------------------------------------------------------------------
bool keyword_active() { return menu_state::is_active(KEYWORD_MENU); }
void keyword_set_active() { set_active_exclusive(KEYWORD_MENU); }
void keyword_clear() { menu_state::set_active(KEYWORD_MENU, false); }

int keyword_state(void* controller) { return ui::state_tag(controller, fSwStateMachine); }

/// KeywordMenuState.ShouldSuppress: the whole active keyword menu, validated against the
/// controller (gone, inactive or state None clears it).
bool keyword_should_suppress()
{
    if (!keyword_active()) return false;
    void* controller = unity::find_object(kSecretWord);
    if (!controller || !unity::active_in_hierarchy(controller)) {
        keyword_clear();
        return false;
    }
    if (keyword_state(controller) == STATE_NONE) {
        keyword_clear();
        return false;
    }
    return true;
}

std::wstring command_name(int command_id)
{
    switch (command_id) {
    case 0: return T(L"Ask");
    case 1: return T(L"Learn");
    case 2: return T(L"Key Items");
    case 3: return T(L"Cancel");
    default: return TF(L"Command {0}", {text::num(command_id)});
    }
}

// --- WordsMenuState --------------------------------------------------------------------------
bool words_active() { return menu_state::is_active(WORDS_MENU); }
void words_set_active() { set_active_exclusive(WORDS_MENU); }
bool words_is_new_index(int index) { return dedup::index(kCtxWordsIndex, index, dedup::Scope::Session); }

// --- announcements ---------------------------------------------------------------------------
/// ComposeKeywordAnnouncement: cache the description for the I key, append it with Auto Detail.
std::wstring compose(const std::wstring& name, const std::wstring& description_in)
{
    if (name.empty()) return {};
    std::wstring description = text::is_blank(description_in) ? std::wstring() : description_in;
    set_menu_detail(description);
    if (ui::prefs::auto_detail_enabled() && !description.empty()) return name + L": " + description;
    return name;
}

/// The IEnumerable fields hold a List`1 at runtime (the C# TryCast); anything else reads nothing.
void* as_list(void* enumerable)
{
    if (!enumerable) return nullptr;
    const char* cls = il2cpp::object_class_name(enumerable);
    return (cls && std::string(cls) == "List`1") ? enumerable : nullptr;
}

/// GetKeywordAtIndex + FormatKeywordAnnouncement (wordDataList).
std::wstring keyword_at(void* controller, int index, int& count)
{
    count = -1;
    void* list = as_list(ref::rd<void*>(controller, fSwWordDataList));
    if (!list || index < 0 || index >= unity::list_count(list)) return {};
    void* data = unity::list_at(list, index);
    if (!data) return {};
    count = unity::list_count(list);
    std::wstring name, description;
    std::wstring name_id = ref::rd_str(data, fSfNameMessageId);
    if (!name_id.empty()) name = strip_icons(message(name_id));
    std::wstring desc_id = ref::rd_str(data, fSfDescriptionMessageId);
    if (!desc_id.empty()) description = strip_icons(message(desc_id));
    return compose(name, description);
}

/// GetItemAtIndex + FormatItemAnnouncement (itemDataList).
std::wstring item_at(void* controller, int index, int& count)
{
    count = -1;
    void* list = as_list(ref::rd<void*>(controller, fSwItemDataList));
    if (!list || index < 0 || index >= unity::list_count(list)) return {};
    void* data = unity::list_at(list, index);
    if (!data) return {};
    count = unity::list_count(list);
    std::wstring name = ref::rd_str(data, fIlName);
    if (name.empty()) return {};
    std::wstring description = ref::rd_str(data, fIlDescription);
    if (!description.empty()) description = strip_icons(description);
    return compose(strip_icons(name), description);
}

/// GetWordsRenderedDescription: view.descriptionText.text.
std::wstring words_rendered_description(void* list_ctrl)
{
    void* view = ref::rd<void*>(list_ctrl, fWlView);
    std::wstring s = unity::ui_text(view ? ref::rd<void*>(view, fWlvDescriptionText) : nullptr);
    return text::is_blank(s) ? std::wstring() : strip_icons(s);
}

struct DictLookup {
    uint32_t key;
    bool found = false;
    void* value = nullptr;
};

/// GetWordsKeywordFromDictionary: keyWordContentDictionary is keyed by the LIST INDEX.
std::wstring words_keyword(void* list_ctrl, int index, int& count)
{
    count = -1;
    void* content_list = ref::rd<void*>(list_ctrl, fWlContentList);
    if (!content_list || index < 0 || index >= unity::list_count(content_list)) return {};
    count = unity::list_count(content_list);
    void* item = unity::list_at(content_list, index);
    if (!item) return {};

    void* content = nullptr;
    if (void* dict = ref::rd<void*>(list_ctrl, fWlDictionary)) {
        DictLookup d{static_cast<uint32_t>(index)};
        unity::dict_for_each(dict, [](uint64_t k, uint64_t v, void* st) {
            auto* p = static_cast<DictLookup*>(st);
            if (!p->found && static_cast<uint32_t>(k) == p->key) {
                p->found = true;
                p->value = reinterpret_cast<void*>(v);
            }
        }, &d, 4, 8);
        content = d.value;
    }

    const std::wstring item_name = ref::rd_str(item, fCccName);
    if (!content) {
        // Fallback: the row's name plus the rendered description panel.
        if (!item_name.empty()) return compose(strip_icons(item_name), words_rendered_description(list_ctrl));
        return {};
    }

    std::wstring name, description;
    std::wstring name_id = ref::call_str(mContentMesIdName, content);
    if (!name_id.empty()) name = strip_icons(message(name_id));
    std::wstring desc_id = ref::call_str(mContentMesIdDescription, content);
    if (!desc_id.empty()) description = strip_icons(message(desc_id));
    if (text::is_blank(description)) description = words_rendered_description(list_ctrl);
    if (text::is_blank(name) && !item_name.empty()) name = strip_icons(item_name);
    return compose(name, description);
}

/// GetWordsTouchKeywordAtIndex + GetWordsTouchDescription.
std::wstring touch_words_keyword(void* list_ctrl, int index, int& count)
{
    count = -1;
    void* content_list = ref::rd<void*>(list_ctrl, fTwlContentList);
    if (!content_list || index < 0 || index >= unity::list_count(content_list)) return {};
    count = unity::list_count(content_list);
    void* content = unity::list_at(content_list, index);
    if (!content) return {};
    void* name_text = ref::call<void*>(mTwcNameText, content);
    if (!name_text) return {};
    std::wstring name = unity::ui_text(name_text);
    if (name.empty()) return {};
    void* view = ref::rd<void*>(list_ctrl, fTwlView);
    std::wstring description = unity::ui_text(view ? ref::rd<void*>(view, fTwlvDescriptionText) : nullptr);
    if (!description.empty()) description = strip_icons(description);
    return compose(strip_icons(name), description);
}

// --- KeyInput SecretWordController -----------------------------------------------------------
using VoidInt_t = void (*)(void* self, int32_t a, const void* mi);
using VoidInt2_t = void (*)(void* self, int32_t a, int32_t b, const void* mi);
using Void_t = void (*)(void* self, const void* mi);
using VoidBool_t = void (*)(void* self, bool a, const void* mi);

VoidInt_t o_SelectCommand;
Void_t o_CommandSelectingInit;
VoidInt2_t o_SelectContentByWord;
VoidInt2_t o_SelectContentByItem;
Void_t o_NoneInit;
Void_t o_Close;

void d_SelectCommand(void* self, int32_t index, const void* mi)
{
    o_SelectCommand(self, index, mi);
    hooks::guarded("Keyword.SelectCommand", [&] {
        // Only on the command bar: a cursor reset during term select or close stays silent.
        if (!self) return;
        int state = keyword_state(self);
        if (state != STATE_COMMAND_SELECT && state != STATE_COMMAND_SELECTING) return;
        std::wstring name = command_name(index);
        if (name.empty()) return;
        keyword_set_active();
        // Command name only; interrupt false so the NPC's intro line is not cut off.
        ffpr::say(name, false);
    });
}

void d_CommandSelectingInit(void* self, const void* mi)
{
    o_CommandSelectingInit(self, mi);
    // Ownership only (suppresses the generic reader); SelectCommand is the sole speaker.
    hooks::guarded("Keyword.CommandSelectingInit", [&] { keyword_set_active(); });
}

void d_SelectContentByWord(void* self, int32_t index, int32_t scroll_type, const void* mi)
{
    o_SelectContentByWord(self, index, scroll_type, mi);
    hooks::guarded("Keyword.SelectContentByWord", [&] {
        if (index < 0) return;
        int count = -1;
        std::wstring s = keyword_at(self, index, count);
        if (s.empty()) return;
        keyword_set_active();
        ffpr::say(ui::menu_position(s, index, count), true);
    });
}

void d_SelectContentByItem(void* self, int32_t index, int32_t scroll_type, const void* mi)
{
    o_SelectContentByItem(self, index, scroll_type, mi);
    hooks::guarded("Keyword.SelectContentByItem", [&] {
        if (index < 0) return;
        int count = -1;
        std::wstring s = item_at(self, index, count);
        if (s.empty()) return;
        keyword_set_active();
        ffpr::say(ui::menu_position(s, index, count), true);
    });
}

void d_NoneInit(void* self, const void* mi)
{
    o_NoneInit(self, mi);
    hooks::guarded("Keyword.NoneInit", [&] { keyword_clear(); });
}

void d_Close(void* self, const void* mi)
{
    o_Close(self, mi);
    hooks::guarded("Keyword.Close", [&] { keyword_clear(); });
}

// --- KeyInput Words menu ---------------------------------------------------------------------
Void_t o_SetCommandSelectCursor;
Void_t o_ExitSelect;
VoidBool_t o_WordsSetActive;

void d_SetCommandSelectCursor(void* self, const void* mi)
{
    o_SetCommandSelectCursor(self, mi);
    hooks::guarded("Words.SetCommandSelectCursor", [&] {
        if (!menu_manager_open()) return;
        if (!self || !unity::active_in_hierarchy(self)) return;
        if (g_words_exit_in_progress) return;   // SetDefaultCursor from ExitSelect: closing

        int index = ui::cursor_index(ref::rd<void*>(self, fWlSelectCursor));
        if (index < 0) return;
        int count = -1;
        std::wstring s = words_keyword(self, index, count);
        if (s.empty()) return;
        if (!words_is_new_index(index)) return;
        words_set_active();
        ffpr::say(ui::menu_position(s, index, count), true);
    });
}

void d_ExitSelect(void* self, const void* mi)
{
    g_words_exit_in_progress = true;
    o_ExitSelect(self, mi);
    g_words_exit_in_progress = false;
}

void d_WordsSetActive(void* self, bool is_active, const void* mi)
{
    o_WordsSetActive(self, is_active, mi);
    hooks::guarded("Words.SetActive", [&] {
        if (!is_active) menu_state::reset(WORDS_MENU);
    });
}

// --- Touch Words menu ------------------------------------------------------------------------
VoidInt_t o_TouchSetSelectContent;
using VoidPtr_t = void (*)(void* self, void* a, const void* mi);
VoidPtr_t o_TouchUpdateView;

void d_TouchSetSelectContent(void* self, int32_t id, const void* mi)
{
    o_TouchSetSelectContent(self, id, mi);
    hooks::guarded("Words.Touch.SetSelectContent", [&] {
        if (!menu_manager_open()) return;
        if (!words_is_new_index(id)) return;
        int count = -1;
        std::wstring s = touch_words_keyword(self, id, count);
        if (s.empty()) return;
        words_set_active();
        ffpr::say(ui::menu_position(s, id, count), true);
    });
}

void d_TouchUpdateView(void* self, void* ids, const void* mi)
{
    o_TouchUpdateView(self, ids, mi);
    hooks::guarded("Words.Touch.UpdateView", [&] {
        if (!menu_manager_open()) return;
        // AnnounceWordsFirstKeyword_Touch: two frames later, the first keyword.
        frame::after_frames(2, [self] {
            hooks::guarded("Words.Touch.FirstKeyword", [&] {
                if (!kTouchWordsList.is(self) || !unity::alive(self) || !unity::active_in_hierarchy(self)) return;
                if (!words_is_new_index(0)) return;
                int count = -1;
                std::wstring s = touch_words_keyword(self, 0, count);
                if (s.empty()) return;
                words_set_active();
                ffpr::say(ui::menu_position(s, 0, count), true);
            });
        }, "FF2.Words.TouchFirst");
    });
}

int install()
{
    menu_state::register_reset(WORDS_MENU, [] { dedup::reset(kCtxWordsIndex); });

    queries::define("KeywordMenuState.ShouldSuppress", [] { return keyword_should_suppress(); });
    queries::define("KeywordMenuState.IsActive", [] { return keyword_active(); });
    queries::define("WordsMenuState.ShouldSuppress", [] { return words_active(); });
    queries::define("WordsMenuState.IsActive", [] { return words_active(); });
    actions::define("KeywordMenuState.ClearState", [] { keyword_clear(); });
    actions::define("WordsMenuState.ClearState", [] { menu_state::reset(WORDS_MENU); });
    // CursorSuppressionCheck: KeywordMenu, then WordsMenu.
    cursor::add_suppressor("KeywordMenuState.ShouldSuppress");
    cursor::add_suppressor("WordsMenuState.ShouldSuppress");

    int n = 0;
    n += hooks::attach(mSwSelectCommand, &d_SelectCommand, &o_SelectCommand) ? 1 : 0;
    n += hooks::attach(mSwCommandSelectingInit, &d_CommandSelectingInit, &o_CommandSelectingInit) ? 1 : 0;
    n += hooks::attach(mSwSelectContentByWord, &d_SelectContentByWord, &o_SelectContentByWord) ? 1 : 0;
    n += hooks::attach(mSwSelectContentByItem, &d_SelectContentByItem, &o_SelectContentByItem) ? 1 : 0;
    n += hooks::attach(mSwNoneInit, &d_NoneInit, &o_NoneInit) ? 1 : 0;
    n += hooks::attach(mSwClose, &d_Close, &o_Close) ? 1 : 0;
    n += hooks::attach(mWlSetCommandSelectCursor, &d_SetCommandSelectCursor, &o_SetCommandSelectCursor) ? 1 : 0;
    n += hooks::attach(mWwExitSelect, &d_ExitSelect, &o_ExitSelect) ? 1 : 0;
    n += hooks::attach(mWwSetActive, &d_WordsSetActive, &o_WordsSetActive) ? 1 : 0;
    n += hooks::attach(mTwlSetSelectContent, &d_TouchSetSelectContent, &o_TouchSetSelectContent) ? 1 : 0;
    n += hooks::attach(mTwlUpdateView, &d_TouchUpdateView, &o_TouchUpdateView) ? 1 : 0;
    LOGF("[FF2 Keyword] %d of 11 hooks queued", n);
    return n;
}

core::Install s_install{"FF2.Keywords", &install, 510, game::kFF2};

} // namespace
} // namespace ffpr::ff2
