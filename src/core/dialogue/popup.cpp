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
#include "displayed_ui.h"
#include <unordered_set>
#include "dialogue/dialogue.h"
#include "system/cursor_event.h"

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
ref::Class kJobPopup{"Last.UI.KeyInput", "JobChangePopup"};
ref::Class kPlayDataPopup{"Last.UI.KeyInput", "PlayDataPopupController"};
ref::Class kWarehousePopup{"Last.UI.KeyInput", "WarehousePopupController"};
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
ref::Field fJobCommands{kJobPopup, "commandList"};
ref::Field fTouchCommonTitle{kTouchCommonPopup, "titleText"};    // Text
ref::Field fTouchCommonMessage{kTouchCommonPopup, "messageText"};

ref::Method mPopupOpen{kPopup, "Open", 0};
ref::Method mPopupClose{kPopup, "Close", 0};
ref::Method mCommonSetCursor{kCommonPopup, "SetCommandSelectCursor", 0};
ref::Method mGameOverSetCursor{kGameOverSelectPopup, "SetCommandSelectCursor", 0};
ref::Method mGameOverLoadSetCursor{kGameOverLoadPopup, "SetCommandSelectCursor", 0};
ref::Method mInitSaveLoadPopup{kGameOverController, "InitSaveLoadPopup", 0};
ref::Method mPlayDataDisplay{kPlayDataPopup, "Open", 0};
ref::Method mPlayDataClose{kPlayDataPopup, "Close", 0};
ref::Method mWarehouseShow{kWarehousePopup, "Show", 7};
ref::Method mWarehouseClose{kWarehousePopup, "Close", 0};

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

// The game-over load popup ("Start from recent save data?"), identity only (re-checked with
// unity::alive). It is a MonoBehaviour, not a Popup: Popup.Open never registers it, and choosing
// Load closes the select popup (Popup.Close clears PopupState) before InitSaveLoadPopup opens it.
// Its moves run Cursor.NextIndex/PrevIndex (GameOverLoadPopup.<UpdateSelect>b__32_0) with
// SetCommandSelectCursor as the callback, so each button was spoken by the focus reader below
// and again one frame later by the generic cursor reader. The generic reader now skips exactly
// this popup's cursor (identity, not text); the focus reader keeps the game's button label.
void* g_game_over_load_popup = nullptr;

// ChangeNamePopup opens (see dialogue.h change_name_popup_opens).
int g_change_name_popup_opens = 0;

bool owns_game_over_load_cursor()
{
    void* cur = cursor::current().cursor;
    void* popup = g_game_over_load_popup;
    if (!cur || !popup) return false;
    if (!unity::alive(popup)) {
        g_game_over_load_popup = nullptr;
        return false;
    }
    return ref::rd<void*>(popup, fGameOverLoadCursor) == cur;
}

// --- PopupState ---------------------------------------------------------------------------
std::string g_popup_type;
void* g_popup = nullptr;
ref::Field* g_command_list = nullptr;
bool g_own_focus_reader = false;

// --- helpers ------------------------------------------------------------------------------

// TextUtils.StripIconMarkup(s.Trim())
std::wstring strip_icons(const std::wstring& s) { return text::trim(text::strip_markup(text::trim(s))); }

std::wstring read_text(void* text_component) { return displayed::text(text_component); }

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
    void* component = ref::rd<void*>(p, fCommonMessage);
    auto body = read_text(component);
    const auto image_message = displayed::text_field(p, "imageTextMessage");
    if (!text::is_blank(unity::ui_text(component)) && body.empty() && image_message.empty()) return {};
    if (!image_message.empty()) { if (!body.empty()) body += L". "; body += image_message; }
    return build_announcement(read_icon_text_view(ref::rd<void*>(p, fCommonTitle)), body);
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

std::wstring read_rendered_popup(void* root)
{
    std::unordered_set<void*> buttons;
    void* commands = displayed::field(root, "commandList");
    for (int i = 0; i < unity::list_count(commands); ++i)
        buttons.insert(displayed::field(unity::list_at(commands, i), "text"));
    std::wstring body;
    for (void* component : unity::texts(root, false)) {
        if (buttons.count(component)) continue;
        const auto line = displayed::text(component);
        if (!line.empty()) { if (!body.empty()) body += L". "; body += line; }
    }
    return body;
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
    void* view = ref::rd<void*>(controller, fGameOverCtrlView);
    void* popup = view ? ref::rd<void*>(view, fGameOverViewLoadPopup) : nullptr;
    if (!popup) return;
    read_popup_when_ready(popup, "Popup.GameOverLoadRead", [popup] {
        return read_text(ref::rd<void*>(popup, fGameOverLoadMessage));
    }, [popup] { return popup == g_game_over_load_popup; });
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
    read_popup_when_ready(popup, "Popup.CommonPopupRead", [popup] {
        std::wstring message = read_common_popup(popup);
        if (text::is_blank(message)) return std::wstring(); // never settle for buttons alone
        std::wstring button = read_focused_common_button(popup);
        g_suppress_next_common_focus = false;
        return text::is_blank(button) ? message : message + L" " + button;
    }, [popup] { return popup_state::active_popup() == popup; });
}

using Reader = std::wstring (*)(void*);

// HandlePopupDetected + DelayedPopupRead
void handle_popup_detected(const char* type_name, void* popup, ref::Field* command_list, Reader reader,
                           bool own_focus_reader = false)
{
    popup_state::set_active(type_name, popup, command_list, own_focus_reader);
    read_popup_when_ready(popup, "Popup.DelayedRead", [popup, reader] { return reader(popup); },
                          [popup] { return popup_state::active_popup() == popup; });
}

// PopupOpen_Postfix
void popup_open(void* p)
{
    if (!p) return;

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
        ++g_change_name_popup_opens;
        handle_popup_detected("ChangeNamePopup", p, nullptr, [](void* q) { return read_description(q, fChangeNameDesc); });
        return;
    }
    if (kTouchCommonPopup.is(p)) {
        handle_popup_detected("TouchCommonPopup", p, nullptr, &read_touch_common_popup);
        return;
    }
    if (kJobPopup.is(p)) {
        handle_popup_detected("JobChangePopup", p, &fJobCommands, &read_rendered_popup);
        return;
    }
    // Remaining Popup subclasses (job/name/other information layouts): rendered Texts,
    // excluding command-button labels, which retain their existing focus reader.
    handle_popup_detected("Popup", p, nullptr, &read_rendered_popup);
}

// PopupClose_Postfix
void popup_close()
{
    frame::cancel_label("Popup.Body.");
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
Void_t o_PlayDataDisplay, o_PlayDataClose, o_WarehouseClose;
using WarehouseShow_t = void (*)(void*, void*, void*, void*, int32_t, int32_t, void*, void*, const void*);
WarehouseShow_t o_WarehouseShow;

void d_PlayDataDisplay(void* self, const void* mi)
{
    o_PlayDataDisplay(self, mi);
    hooks::guarded("Popup.PlayData", [=] {
        if (kPlayDataPopup.is(self))
            handle_popup_detected("PlayDataPopup", self, nullptr, &read_rendered_popup);
    });
}
void d_PlayDataClose(void* self, const void* mi)
{
    o_PlayDataClose(self, mi);
    hooks::guarded("Popup.PlayDataClose", [=] {
        if (kPlayDataPopup.is(self) && popup_state::active_popup() == self) popup_close();
    });
}
void d_WarehouseShow(void* self, void* item, void* comment, void* command, int32_t max_count,
                     int32_t possible_count, void* selected, void* cancel, const void* mi)
{
    o_WarehouseShow(self, item, comment, command, max_count, possible_count, selected, cancel, mi);
    hooks::guarded("Popup.Warehouse", [=] {
        if (!kWarehousePopup.is(self)) return;
        popup_state::set_active("WarehousePopup", self, nullptr);
        read_popup_when_ready(self, "Warehouse.Show", [=] {
            return read_rendered_popup(displayed::field(self, "view"));
        }, [=] { return popup_state::active_popup() == self; });
    });
}
void d_WarehouseClose(void* self, const void* mi)
{
    o_WarehouseClose(self, mi);
    hooks::guarded("Popup.WarehouseClose", [=] {
        if (kWarehousePopup.is(self) && popup_state::active_popup() == self) popup_close();
    });
}

void d_PopupOpen(void* self, const void* mi)
{
    // The native Open sets its cursor before returning. Arm the existing opening
    // guard before that callback, so the message still precedes the first button.
    if (self && kCommonPopup.is(self)) {
        g_suppress_next_common_focus = true;
        dedup::reset(kCtxPopupButton);
    }
    o_PopupOpen(self, mi);
    hooks::guarded("Popup.Open", [&] { popup_open(self); });
}

void d_PopupClose(void* self, const void* mi)
{
    o_PopupClose(self, mi);
    hooks::guarded("Popup.Close", [&] {
        // Closing an older/parent popup must not retire the newly focused reader.
        if (!popup_state::active_popup() || popup_state::active_popup() == self) popup_close();
    });
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
        if (!self || !kGameOverController.is(self)) return;
        void* view = ref::rd<void*>(self, fGameOverCtrlView);
        g_game_over_load_popup = view ? ref::rd<void*>(view, fGameOverViewLoadPopup) : nullptr;
        delayed_game_over_load_popup_read(self);
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
    if (hooks::attach(mPlayDataDisplay, &d_PlayDataDisplay, &o_PlayDataDisplay)) ++n;
    if (hooks::attach(mPlayDataClose, &d_PlayDataClose, &o_PlayDataClose)) ++n;
    if (hooks::attach(mWarehouseShow, &d_WarehouseShow, &o_WarehouseShow)) ++n;
    if (hooks::attach(mWarehouseClose, &d_WarehouseClose, &o_WarehouseClose)) ++n;

    queries::define("PopupState.IsConfirmationPopupActive", [] { return popup_state::is_confirmation_popup_active(); });
    queries::define("PopupState.ShouldSuppress", [] { return popup_state::should_suppress(); });
    actions::define("PopupState.Clear", [] { popup_state::clear(); });
    queries::define("PopupPatches.OwnsGameOverLoadCursor", [] { return owns_game_over_load_cursor(); });
    cursor::add_suppressor("PopupPatches.OwnsGameOverLoadCursor");
    return n;
}

core::Install s_install{"Popup", &install, 100};

} // namespace

void read_popup_when_ready(void* popup, const char* context, std::function<std::wstring()> reader,
                           std::function<bool()> current)
{
    if (!popup) return;
    const std::string label = std::string("Popup.Body.") + context;
    frame::every_frame([popup, reader = std::move(reader), current = std::move(current),
                       context, frames = 0]() mutable {
        if (!unity::alive(popup) || (current && !current())) return false;
        if (++frames < 2 || !displayed::visible(popup)) return frames < 60;
        const auto body = reader();
        if (text::is_blank(body)) {
            if (frames == 60) LOGF("[Popup] %s: no rendered body within 60 frames", context);
            return frames < 60;
        }
        ffpr::say(text::clean(body), true);
        return false;
    }, label.c_str(), 60);
}
void read_popup_message(void* popup, const char* context)
{
    read_popup_when_ready(popup, context, [popup] { return displayed::text_field(popup, "messageText"); });
}

// --- PopupState ---------------------------------------------------------------------------------
namespace popup_state {

bool is_confirmation_popup_active() { return menu_state::is_active(menu_state::POPUP); }
const std::string& current_popup_type() { return g_popup_type; }
void* active_popup() { return g_popup; }
bool has_command_list() { return g_command_list && g_command_list->get() >= 0; }
bool has_own_focus_reader() { return g_own_focus_reader; }

void set_active(const char* type_name, void* popup, ref::Field* command_list, bool own_focus_reader)
{
    frame::cancel_label("Popup.Body."); // Retire the reader for prior popup content.
    menu_state::set_active(menu_state::POPUP, true);
    g_popup_type = type_name ? type_name : "";
    g_popup = popup;
    g_command_list = command_list;
    g_own_focus_reader = own_focus_reader;
}

void clear()
{
    frame::cancel_label("Popup.Body.");
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

int change_name_popup_opens() { return g_change_name_popup_opens; }

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
