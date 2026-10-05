// Ports FF1 Patches/PopupPatches.cs (every popup: message and buttons; the game-over popups) and
// Patches/PopupState.cs (the active popup, for the generic cursor reader).
#include <functional>
#include <string>
#include "core.h"
#include "dedup.h"
#include "frame.h"
#include "hooks.h"
#include "log.h"
#include "modtext.h"
#include "refs.h"
#include "say.h"
#include "text.h"
#include "ui_util.h"
#include "unity.h"
#include "dialogue/dialogue.h"

namespace ffpr::dialogue {
namespace {

// --- game names ---------------------------------------------------------------------------
ref::Class kPopup{"Last.UI", "Popup"};
ref::Class kCommonPopup{"Last.UI.KeyInput", "CommonPopup"};
ref::Class kMagicStonePopup{"Last.UI.KeyInput", "ChangeMagicStonePopup"};
ref::Class kGameOverSelectPopup{"Last.UI.KeyInput", "GameOverSelectPopup"};
ref::Class kGameOverLoadPopup{"Last.UI.KeyInput", "GameOverLoadPopup"};
ref::Class kGameOverController{"Last.UI.KeyInput", "GameOverPopupController"};
ref::Class kGameOverView{"Last.UI.KeyInput", "GameOverPopupView"};
ref::Class kInfoPopup{"Last.UI.KeyInput", "InfomationPopup"};
ref::Class kInputPopup{"Last.UI.KeyInput", "InputPopup"};
ref::Class kChangeNamePopup{"Last.UI.KeyInput", "ChangeNamePopup"};
ref::Class kShopController{"Last.UI.KeyInput", "ShopController"};
ref::Class kTouchCommonPopup{"Last.UI.Touch", "CommonPopup"};
ref::Class kIconTextView{"Last.UI", "IconTextView"};
ref::Class kCommonCommand{"Last.UI.KeyInput", "CommonCommand"};

ref::Field fIconTextViewName{kIconTextView, "nameText"};
ref::Field fCommonCommandText{kCommonCommand, "text"};
ref::Field fCommonTitle{kCommonPopup, "titleText"};          // IconTextView
ref::Field fCommonMessage{kCommonPopup, "messageText"};      // Text
ref::Field fCommonCursor{kCommonPopup, "selectCursor"};
ref::Field fCommonCommands{kCommonPopup, "commandList"};
ref::Field fMagicStoneName{kMagicStonePopup, "nameText"};
ref::Field fMagicStoneDesc{kMagicStonePopup, "descriptionText"};
ref::Field fMagicStoneCommands{kMagicStonePopup, "commandList"};
ref::Field fGameOverCursor{kGameOverSelectPopup, "selectCursor"};
ref::Field fGameOverCommands{kGameOverSelectPopup, "commandList"};
ref::Field fInfoTitle{kInfoPopup, "titleText"};              // IconTextView
ref::Field fInfoMessage{kInfoPopup, "messageText"};
ref::Field fInputDesc{kInputPopup, "descriptionText"};
ref::Field fChangeNameDesc{kChangeNamePopup, "descriptionText"};
ref::Field fGameOverLoadMessage{kGameOverLoadPopup, "messageText"};
ref::Field fGameOverLoadCursor{kGameOverLoadPopup, "selectCursor"};
ref::Field fGameOverLoadCommands{kGameOverLoadPopup, "commandList"};
ref::Field fGameOverCtrlView{kGameOverController, "view"};
ref::Field fGameOverViewLoadPopup{kGameOverView, "loadPopup"};
ref::Field fShopIsOpen{kShopController, "<IsOpne>k__BackingField"};
ref::Field fTouchCommonTitle{kTouchCommonPopup, "titleText"};    // Text
ref::Field fTouchCommonMessage{kTouchCommonPopup, "messageText"};

ref::Method mPopupOpen{kPopup, "Open", 0};
ref::Method mPopupClose{kPopup, "Close", 0};
ref::Method mCommonSetCursor{kCommonPopup, "SetCommandSelectCursor", 0};
ref::Method mGameOverSetCursor{kGameOverSelectPopup, "SetCommandSelectCursor", 0};
ref::Method mGameOverLoadSetCursor{kGameOverLoadPopup, "SetCommandSelectCursor", 0};
ref::Method mInitSaveLoadPopup{kGameOverController, "InitSaveLoadPopup", 0};

// Dedup contexts (FF1 lastCommonPopupCursorIndex, lastGameOverSelectCursorIndex,
// lastGameOverLoadCursorIndex). Session scope: reset only where FF1 reset them.
constexpr const char* kCtxPopupButton = "Popup.Button";
constexpr const char* kCtxGameOverButton = "GameOver.Button";
constexpr const char* kCtxGameOverLoad = "GameOver.Load";
constexpr dedup::Scope kScope = dedup::Scope::Session;

// On a CommonPopup open the message must be read BEFORE the focused button. CommonPopup.Open
// calls SetCommandSelectCursor right after base Popup.Open (whose postfix sets this flag), so the
// button would be read at once while the message read is one frame late: backwards. The open
// read speaks "message button" together and this flag suppresses the open-time focus announce.
bool g_suppress_next_common_focus = false;

// Frames a game-over button read may wait for the popup's button text (open only).
constexpr int kGameOverButtonMaxFrames = 3;

// --- PopupState ---------------------------------------------------------------------------
std::string g_popup_type;
void* g_popup = nullptr;
ref::Field* g_command_list = nullptr;
bool g_own_focus_reader = false;

// --- helpers ------------------------------------------------------------------------------

// TextUtils.StripIconMarkup(s.Trim())
std::wstring strip_icons(const std::wstring& s) { return text::trim(text::strip_markup(text::trim(s))); }

bool is_shop_active()
{
    void* shop = unity::find_object(kShopController);
    return shop && ref::rd<bool>(shop, fShopIsOpen);
}

std::wstring read_text(void* text_component) { return unity::ui_text(text_component); }

std::wstring read_icon_text_view(void* view)
{
    if (!view) return {};
    return read_text(ref::rd<void*>(view, fIconTextViewName));
}

std::wstring build_announcement(const std::wstring& title_raw, const std::wstring& message_raw)
{
    std::wstring title = text::is_blank(title_raw) ? std::wstring() : strip_icons(title_raw);
    std::wstring message = text::is_blank(message_raw) ? std::wstring() : strip_icons(message_raw);
    if (!title.empty() && !message.empty()) return title + L". " + message;
    if (!title.empty()) return title;
    return message;
}

std::wstring read_button_from_command_list(void* popup, ref::Field& command_list, int index)
{
    void* list = ref::rd<void*>(popup, command_list);
    if (!list) return {};
    if (index < 0 || index >= unity::list_count(list)) return {};
    void* command = unity::list_at(list, index);
    if (!command) return {};
    return read_text(ref::rd<void*>(command, fCommonCommandText));
}

// --- type-specific readers ------------------------------------------------------------------
std::wstring read_common_popup(void* p)
{
    return build_announcement(read_icon_text_view(ref::rd<void*>(p, fCommonTitle)),
                              read_text(ref::rd<void*>(p, fCommonMessage)));
}

std::wstring read_magic_stone_popup(void* p)
{
    return build_announcement(read_text(ref::rd<void*>(p, fMagicStoneName)),
                              read_text(ref::rd<void*>(p, fMagicStoneDesc)));
}

std::wstring read_game_over_select_popup(void*) { return T(L"Game Over"); }

std::wstring read_infomation_popup(void* p)
{
    return build_announcement(read_icon_text_view(ref::rd<void*>(p, fInfoTitle)),
                              read_text(ref::rd<void*>(p, fInfoMessage)));
}

std::wstring read_description(void* p, ref::Field& f)
{
    std::wstring desc = read_text(ref::rd<void*>(p, f));
    return text::is_blank(desc) ? std::wstring() : strip_icons(desc);
}

std::wstring read_touch_common_popup(void* p)
{
    return build_announcement(read_text(ref::rd<void*>(p, fTouchCommonTitle)),
                              read_text(ref::rd<void*>(p, fTouchCommonMessage)));
}

// --- focus readers ----------------------------------------------------------------------------

// CommonPopup.SetCommandSelectCursor: the popup's own cursor-set (Open, the move callback, a
// click). Replaces FF1's earlier per-frame UpdateFocus hook.
void common_popup_focus_changed(void* popup)
{
    void* cur = ref::rd<void*>(popup, fCommonCursor);
    if (!cur) return;
    const int index = ui::cursor_index(cur);
    if (!dedup::index(kCtxPopupButton, index, kScope)) return;

    // The open read speaks the message and this button: do not also speak it here (out of order).
    if (g_suppress_next_common_focus) {
        g_suppress_next_common_focus = false;
        return;
    }

    std::wstring button = read_button_from_command_list(popup, fCommonCommands, index);
    if (!text::is_blank(button)) ffpr::say(strip_icons(button), true);
}

// ReadGameOverButton: speak the game-over popup's focused button when its index changed. False
// only when the button text is not set yet (the cursor can be set before the texts, at open);
// the index is then not stored, so the short retry can still speak it. (The text is checked
// before the index compare: the outcome is FF1's, the deduplicator has no "peek".)
bool read_game_over_button(void* popup, ref::Field& cursor_field, ref::Field& list_field, const char* ctx)
{
    void* cur = ref::rd<void*>(popup, cursor_field);
    if (!cur) return true;
    const int index = ui::cursor_index(cur);

    void* list = ref::rd<void*>(popup, list_field);
    if (!list) return false;
    if (index < 0 || index >= unity::list_count(list)) return false;
    void* command = unity::list_at(list, index);
    if (!command) return false;
    void* text_component = ref::rd<void*>(command, fCommonCommandText);
    if (!text_component) return false;
    std::wstring button = read_text(text_component);
    if (text::is_blank(button)) return false;

    if (!dedup::index(ctx, index, kScope)) return true;
    ffpr::say(strip_icons(button), true);
    return true;
}

void retry_game_over_button(void* popup, ref::Field* cursor_field, ref::Field* list_field, const char* ctx)
{
    int tries = 0;
    frame::every_frame([popup, cursor_field, list_field, ctx, tries]() mutable {
        if (++tries > kGameOverButtonMaxFrames || !unity::alive(popup)) return false;
        bool done = read_game_over_button(popup, *cursor_field, *list_field, ctx);
        return !done && tries < kGameOverButtonMaxFrames;
    }, "Popup.RetryGameOverButton", kGameOverButtonMaxFrames);
}

void game_over_focus_changed(void* popup, ref::Field& cursor_field, ref::Field& list_field, const char* ctx)
{
    if (!read_game_over_button(popup, cursor_field, list_field, ctx))
        retry_game_over_button(popup, &cursor_field, &list_field, ctx);
}

// DelayedGameOverLoadPopupRead: one frame after InitSaveLoadPopup, read the load popup's message.
void delayed_game_over_load_popup_read(void* controller)
{
    frame::next_frame([controller] {
        hooks::guarded("Popup.GameOverLoadRead", [&] {
            if (!unity::alive(controller)) return;
            void* view = ref::rd<void*>(controller, fGameOverCtrlView);
            if (!view) return;
            void* load_popup = ref::rd<void*>(view, fGameOverViewLoadPopup);
            if (!load_popup) return;
            std::wstring message = read_text(ref::rd<void*>(load_popup, fGameOverLoadMessage));
            if (!text::is_blank(message)) ffpr::say(strip_icons(message), false);
        });
    }, "Popup.GameOverLoadRead");
}

// --- open / close -------------------------------------------------------------------------------

// ReadFocusedCommonButton: the CommonPopup's focused button (and primes the button context).
std::wstring read_focused_common_button(void* popup)
{
    void* cur = ref::rd<void*>(popup, fCommonCursor);
    if (!cur) return {};
    const int index = ui::cursor_index(cur);
    dedup::index(kCtxPopupButton, index, kScope);   // so the focus reader won't re-announce it
    std::wstring button = read_button_from_command_list(popup, fCommonCommands, index);
    return text::is_blank(button) ? std::wstring() : strip_icons(button);
}

// DelayedCommonPopupRead: message FIRST, then the focused button ("Would you like to return to
// the title screen? No").
void delayed_common_popup_read(void* popup)
{
    frame::next_frame([popup] {
        std::wstring announcement;
        hooks::guarded("Popup.CommonPopupRead", [&] {
            if (!unity::alive(popup)) return;
            std::wstring message = read_common_popup(popup);
            std::wstring button = read_focused_common_button(popup);
            if (!text::is_blank(message) && !text::is_blank(button)) announcement = message + L" " + button;
            else announcement = text::is_blank(message) ? button : message;
        });
        g_suppress_next_common_focus = false;
        if (!text::is_blank(announcement)) ffpr::say(announcement, false);
    }, "Popup.CommonPopupRead");
}

using Reader = std::wstring (*)(void*);

// HandlePopupDetected + DelayedPopupRead
void handle_popup_detected(const char* type_name, void* popup, ref::Field* command_list, Reader reader,
                           bool own_focus_reader = false)
{
    popup_state::set_active(type_name, popup, command_list, own_focus_reader);
    frame::next_frame([popup, reader] {
        hooks::guarded("Popup.DelayedRead", [&] {
            if (!unity::alive(popup)) return;
            std::wstring announcement = reader(popup);
            if (!announcement.empty()) ffpr::say(announcement, false);
        });
    }, "Popup.DelayedRead");
}

// PopupOpen_Postfix
void popup_open(void* p)
{
    if (!p) return;
    if (is_shop_active()) return;

    if (kCommonPopup.is(p)) {
        // Read message + focused button together (message first); suppress the open-time focus
        // announce. CommonPopup has its own focus reader, so the generic reader must not read it.
        g_suppress_next_common_focus = true;
        dedup::reset(kCtxPopupButton);
        popup_state::set_active("CommonPopup", p, &fCommonCommands, true);
        delayed_common_popup_read(p);
        return;
    }
    if (kMagicStonePopup.is(p)) {
        handle_popup_detected("ChangeMagicStonePopup", p, &fMagicStoneCommands, &read_magic_stone_popup);
        return;
    }
    if (kGameOverSelectPopup.is(p)) {
        // Has its own focus reader: the generic reader must not also read the button.
        handle_popup_detected("GameOverSelectPopup", p, &fGameOverCommands, &read_game_over_select_popup, true);
        return;
    }
    if (kInfoPopup.is(p)) {
        handle_popup_detected("InfomationPopup", p, nullptr, &read_infomation_popup);
        return;
    }
    if (kInputPopup.is(p)) {
        handle_popup_detected("InputPopup", p, nullptr, [](void* q) { return read_description(q, fInputDesc); });
        return;
    }
    if (kChangeNamePopup.is(p)) {
        handle_popup_detected("ChangeNamePopup", p, nullptr, [](void* q) { return read_description(q, fChangeNameDesc); });
        return;
    }
    if (kTouchCommonPopup.is(p)) {
        handle_popup_detected("TouchCommonPopup", p, nullptr, &read_touch_common_popup);
        return;
    }
}

// PopupClose_Postfix
void popup_close()
{
    if (popup_state::is_confirmation_popup_active()) popup_state::clear();
    dedup::reset(kCtxPopupButton);
    dedup::reset(kCtxGameOverButton);
    dedup::reset(kCtxGameOverLoad);
    g_suppress_next_common_focus = false;

    // A Quit Game / Return to Title popup over the config menu closes WITHOUT changing config
    // state, so SelectCommand does not re-fire: re-announce the focused config option. Gated on
    // ConfigMenuState.IsActive so save/load/battle/dialogue popup closes don't trigger it.
    if (menu_state::is_active(menu_state::CONFIG_MENU)) {
        const char* reannounce = "ConfigController_SetActive_Patch.ReannounceFocusedConfigOption";
        if (actions::defined(reannounce)) actions::run(reannounce);
    }
}

// --- detours --------------------------------------------------------------------------------------
using Void_t = void (*)(void* self, const void* mi);
Void_t o_PopupOpen;
Void_t o_PopupClose;
Void_t o_CommonSetCursor;
Void_t o_GameOverSetCursor;
Void_t o_GameOverLoadSetCursor;
Void_t o_InitSaveLoadPopup;

void d_PopupOpen(void* self, const void* mi)
{
    o_PopupOpen(self, mi);
    hooks::guarded("Popup.Open", [&] { popup_open(self); });
}

void d_PopupClose(void* self, const void* mi)
{
    o_PopupClose(self, mi);
    hooks::guarded("Popup.Close", [&] { popup_close(); });
}

void d_CommonSetCursor(void* self, const void* mi)
{
    o_CommonSetCursor(self, mi);
    hooks::guarded("CommonPopup.SetCommandSelectCursor", [&] {
        if (self && kCommonPopup.is(self)) common_popup_focus_changed(self);
    });
}

// GameOverSelectPopup.SetCommandSelectCursor: Open, ResetCursor, the move callback, a click.
void d_GameOverSetCursor(void* self, const void* mi)
{
    o_GameOverSetCursor(self, mi);
    hooks::guarded("GameOverSelectPopup.SetCommandSelectCursor", [&] {
        if (self && kGameOverSelectPopup.is(self))
            game_over_focus_changed(self, fGameOverCursor, fGameOverCommands, kCtxGameOverButton);
    });
}

// GameOverLoadPopup.SetCommandSelectCursor: ResetCursor (open), SetData, the move callback,
// UpdateSelect's skip-disabled correction, a click.
void d_GameOverLoadSetCursor(void* self, const void* mi)
{
    o_GameOverLoadSetCursor(self, mi);
    hooks::guarded("GameOverLoadPopup.SetCommandSelectCursor", [&] {
        if (self && kGameOverLoadPopup.is(self))
            game_over_focus_changed(self, fGameOverLoadCursor, fGameOverLoadCommands, kCtxGameOverLoad);
    });
}

void d_InitSaveLoadPopup(void* self, const void* mi)
{
    o_InitSaveLoadPopup(self, mi);
    hooks::guarded("GameOverPopupController.InitSaveLoadPopup", [&] {
        if (self && kGameOverController.is(self)) delayed_game_over_load_popup_read(self);
    });
}

int install()
{
    int n = 0;
    if (hooks::attach(mPopupOpen, &d_PopupOpen, &o_PopupOpen)) ++n;
    if (hooks::attach(mPopupClose, &d_PopupClose, &o_PopupClose)) ++n;
    if (hooks::attach(mCommonSetCursor, &d_CommonSetCursor, &o_CommonSetCursor)) ++n;
    if (hooks::attach(mGameOverSetCursor, &d_GameOverSetCursor, &o_GameOverSetCursor)) ++n;
    if (hooks::attach(mGameOverLoadSetCursor, &d_GameOverLoadSetCursor, &o_GameOverLoadSetCursor)) ++n;
    if (hooks::attach(mInitSaveLoadPopup, &d_InitSaveLoadPopup, &o_InitSaveLoadPopup)) ++n;

    queries::define("PopupState.IsConfirmationPopupActive", [] { return popup_state::is_confirmation_popup_active(); });
    queries::define("PopupState.ShouldSuppress", [] { return popup_state::should_suppress(); });
    actions::define("PopupState.Clear", [] { popup_state::clear(); });
    return n;
}

core::Install s_install{"Popup", &install, 100};

} // namespace

// --- PopupState ---------------------------------------------------------------------------------
namespace popup_state {

bool is_confirmation_popup_active() { return menu_state::is_active(menu_state::POPUP); }
const std::string& current_popup_type() { return g_popup_type; }
void* active_popup() { return g_popup; }
bool has_command_list() { return g_command_list && g_command_list->get() >= 0; }
bool has_own_focus_reader() { return g_own_focus_reader; }

void set_active(const char* type_name, void* popup, ref::Field* command_list, bool own_focus_reader)
{
    menu_state::set_active(menu_state::POPUP, true);
    g_popup_type = type_name ? type_name : "";
    g_popup = popup;
    g_command_list = command_list;
    g_own_focus_reader = own_focus_reader;
}

void clear()
{
    menu_state::reset(menu_state::POPUP);   // MenuStateRegistry.SetActive(POPUP, false)
    g_popup_type.clear();
    g_popup = nullptr;
    g_command_list = nullptr;
    g_own_focus_reader = false;
}

bool should_suppress() { return is_confirmation_popup_active() && has_command_list(); }

} // namespace popup_state

// --- PopupPatches (public) ------------------------------------------------------------------------

// ReadCurrentButton: called by the generic cursor reader while a popup with buttons is active.
void read_current_button(void* cur)
{
    hooks::guarded("Popup.ReadCurrentButton", [&] {
        void* popup = g_popup;
        if (!popup || !cur) return;
        if (!popup_state::has_command_list()) return;
        if (!unity::alive(popup)) return;
        std::wstring button = read_button_from_command_list(popup, *g_command_list, ui::cursor_index(cur));
        if (!text::is_blank(button)) ffpr::say(text::trim(text::strip_markup(button)), true);
    });
}

// ReadFocusedButton: a Cursor-driven button popup's focused label (SavePopup: save/load/quick save).
std::wstring read_focused_button(void* popup, ref::Field& select_cursor, ref::Field& command_list)
{
    return hooks::guarded_r("Popup.ReadFocusedButton", std::wstring(), [&]() -> std::wstring {
        if (!popup) return {};
        void* cur = ref::rd<void*>(popup, select_cursor);
        if (!cur) return {};
        std::wstring button = read_button_from_command_list(popup, command_list, ui::cursor_index(cur));
        return text::is_blank(button) ? std::wstring() : text::trim(text::strip_markup(button));
    });
}

} // namespace ffpr::dialogue
