// Ports FF1 Patches/SaveLoadPatches.cs (SaveLoadMenuState and the save/load confirmation popups),
// Patches/SaveListPatches.cs (the open-read of the save list) and Menus/SaveSlotReader.cs.
#include <string>
#include "core.h"
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
#include "system/system.h"

namespace ffpr::system {
namespace {

// --- game names ---------------------------------------------------------------------------
ref::Class kTitleLoad{"Last.UI.KeyInput", "LoadGameWindowController"};        // title screen load
ref::Class kMenuLoad{"Last.UI.KeyInput", "LoadWindowController"};             // main menu load
ref::Class kMenuSave{"Last.UI.KeyInput", "SaveWindowController"};             // main menu save
ref::Class kInterruption{"Last.UI.KeyInput", "InterruptionWindowController"}; // quick save
ref::Class kSavePopup{"Last.UI.KeyInput", "SavePopup"};
ref::Class kSaveList{"Last.UI.KeyInput", "SaveListController"};
ref::Class kSaveContentKey{"Last.UI.KeyInput", "SaveContentController"};
ref::Class kSaveContentTouch{"Last.UI.Touch", "SaveContentController"};
ref::Class kSaveSlotData{"Last.Management", "SaveSlotData"};
ref::Class kOwnedCharacter{"Last.Data.User", "OwnedCharacterData"};
ref::Class kPlayerParameter{"Last.Data", "PlayerCharacterParameter"};
ref::Class kMessageManager{"Last.Management", "MessageManager"};
ref::Class kMenuManager{"Last.UI", "MenuManager"};

ref::Field fTitleLoadPopup{kTitleLoad, "savePopup"};
ref::Field fMenuLoadPopup{kMenuLoad, "savePopup"};
ref::Field fMenuSavePopup{kMenuSave, "savePopup"};
ref::Field fInterruptionPopup{kInterruption, "savePopup"};
ref::Field fSavePopupMessage{kSavePopup, "messageText"};
ref::Field fSavePopupCursor{kSavePopup, "selectCursor"};
ref::Field fSavePopupCommands{kSavePopup, "commandList"};
ref::Field fSaveListCursor{kSaveList, "selectCursor"};

ref::Method mTitleLoadSetPopupActive{kTitleLoad, "SetPopupActive", 1};
ref::Method mMenuLoadSetPopupActive{kMenuLoad, "SetPopupActive", 1};
ref::Method mMenuSaveSetPopupActive{kMenuSave, "SetPopupActive", 1};
ref::Method mInterruptionSetEnablePopup{kInterruption, "SetEnablePopup", 1};
ref::Method mSaveListSetActive{kSaveList, "SetActive", 3};

ref::Method mKeySlotName{kSaveContentKey, "get_SlotName", 0};
ref::Method mKeySlotData{kSaveContentKey, "get_SlotData", 0};
ref::Method mTouchSlotName{kSaveContentTouch, "get_SlotName", 0};
ref::Method mTouchSlotData{kSaveContentTouch, "get_SlotData", 0};
ref::Method mSlotCurrentArea{kSaveSlotData, "get_CurrentArea", 0};
ref::Method mSlotCurrentLocation{kSaveSlotData, "get_CurrentLocation", 0};
ref::Method mSlotPlayTime{kSaveSlotData, "get_PlayTime", 0};
ref::Method mSlotTimeStamp{kSaveSlotData, "get_TimeStamp", 0};
ref::Method mSlotPartyList{kSaveSlotData, "get_PartyList", 0};
ref::Method mCharName{kOwnedCharacter, "get_Name", 0};
ref::Method mCharParameter{kOwnedCharacter, "get_Parameter", 0};
ref::Method mConfirmedLevel{kPlayerParameter, "ConfirmedLevel", 0};
ref::Method mGetMessage{kMessageManager, "GetMessage", 2};
ref::Method mMenuIsOpen{kMenuManager, "get_IsOpen", 0};

// --- SaveLoadMenuState --------------------------------------------------------------------
bool g_in_confirmation = false;   // SaveLoadMenuState.IsInConfirmation

bool save_load_active() { return menu_state::is_active(menu_state::SAVE_LOAD_MENU); }

void set_save_load_active(bool v)
{
    // MenuStateRegistry.SetActive: clearing runs the key's reset handler.
    if (v) menu_state::set_active(menu_state::SAVE_LOAD_MENU, true);
    else menu_state::reset(menu_state::SAVE_LOAD_MENU);
}

void save_load_reset_state()   // SaveLoadMenuState.ResetState
{
    set_save_load_active(false);
    g_in_confirmation = false;
}

// --- SaveSlotReader -------------------------------------------------------------------------

// FindContentList: TextUtils.FindTransformInChildren(root, "Content") with the scroll-view shape.
void* find_slot_content_list(void* root)
{
    void* content = unity::find_descendant(root, L"Content", 64);
    if (!content) return nullptr;
    void* p = unity::parent(content);
    if (!p) return nullptr;
    if (unity::name(p) == L"Viewport") return content;
    void* gp = unity::parent(p);
    return (gp && unity::name(gp) == L"Scroll View") ? content : nullptr;
}

// GetSlotIdentifier: the controller's SlotName, else Autosave / Quicksave / File N by index.
std::wstring slot_identifier(const std::wstring& slot_name, int slot_index)
{
    if (!slot_name.empty()) return slot_name;
    if (slot_index == 0) return T(L"Autosave");
    if (slot_index == 1) return T(L"Quicksave");
    return TF(L"File {0}", {text::num(slot_index - 1)});
}

// BuildAnnouncement: slot name, date, character and level, location, play time.
std::wstring build_slot_announcement(const std::wstring& slot_id, const std::wstring& time_stamp,
                                      const std::wstring& location, const std::wstring& character_name,
                                      const std::wstring& level, const std::wstring& hours,
                                      const std::wstring& minutes)
{
    std::wstring a = slot_id;
    if (!time_stamp.empty()) a += L", " + time_stamp;
    // FF2 has no character levels: its slot keeps the game's level field at 1 (user, 2026-10-10).
    const bool with_level = !level.empty() && game::flags().has_levels;
    if (!character_name.empty()) {
        a += L": " + character_name;
        if (with_level) a += L" " + TF(L"Level {0}", {level});
    } else if (with_level) {
        a += L": " + TF(L"Level {0}", {level});
    }
    if (!location.empty()) a += L", " + location;
    if (!hours.empty() && !minutes.empty()) a += L", " + TF(L"Time {0}:{1}", {hours, minutes});
    return a;
}

// MessageManager.GetMessage(key, true)
std::wstring game_message(void* mm, const std::wstring& key)
{
    void* s = ref::new_str(key);
    if (!s) return key;
    return ref::str(ref::call<void*>(mGetMessage, mm, s, true));
}

// ReadFromController: the slot's SaveSlotData.
std::wstring read_from_controller(const std::wstring& slot_name, void* slot_data, int slot_index)
{
    const std::wstring slot_id = slot_identifier(slot_name, slot_index);
    if (!slot_data) return slot_id + L": " + T(L"Empty");

    std::wstring location = ref::call_str(mSlotCurrentArea, slot_data);
    std::wstring floor = ref::call_str(mSlotCurrentLocation, slot_data);
    const double play_seconds = ref::call<double>(mSlotPlayTime, slot_data);
    const std::wstring time_stamp = ref::call_str(mSlotTimeStamp, slot_data);

    // Translate message keys to localized text.
    if (void* mm = ref::instance(kMessageManager)) {
        if (!location.empty()) location = game_message(mm, location);
        if (!floor.empty()) floor = game_message(mm, floor);
    }

    // Lead character from the party list.
    std::wstring character_name;
    std::wstring level;
    void* party = ref::call<void*>(mSlotPartyList, slot_data);
    if (party && unity::list_count(party) > 0) {
        if (void* lead = unity::list_at(party, 0)) {
            character_name = ref::call_str(mCharName, lead);
            if (void* param = ref::call<void*>(mCharParameter, lead))
                level = text::num(ref::call<int32_t>(mConfirmedLevel, param));
        }
    }

    const bool has_data = !location.empty() || !character_name.empty() || play_seconds > 0;
    if (!has_data) return slot_id + L": " + T(L"Empty");

    std::wstring hours, minutes;
    if (play_seconds > 0) {
        const int total_minutes = static_cast<int>(play_seconds / 60);
        const int h = total_minutes / 60;
        const int m = total_minutes % 60;
        hours = text::num(h);
        minutes = (m < 10 ? L"0" : L"") + text::num(m);   // ToString("D2")
    }

    if (!floor.empty() && !location.empty()) location += L" - " + floor;

    return build_slot_announcement(slot_id, time_stamp, location, character_name, level, hours, minutes);
}

// ReadSlotFromTransform: KeyInput SaveContentController first, then Touch.
std::wstring read_slot_from_transform(void* slot, int slot_index, bool& found)
{
    found = false;
    if (!slot) return {};
    void* key = unity::get_component(slot, kSaveContentKey);
    if (!key) key = unity::get_component_in_children(slot, kSaveContentKey);
    if (key) {
        found = true;
        return read_from_controller(ref::call_str(mKeySlotName, key), ref::call<void*>(mKeySlotData, key), slot_index);
    }
    void* touch = unity::get_component(slot, kSaveContentTouch);
    if (!touch) touch = unity::get_component_in_children(slot, kSaveContentTouch);
    if (touch) {
        found = true;
        return read_from_controller(ref::call_str(mTouchSlotName, touch), ref::call<void*>(mTouchSlotData, touch),
                                    slot_index);
    }
    return {};
}

// --- SaveLoadPatches ------------------------------------------------------------------------

// TextUtils-free StripRichTextTags: remove every "<...>" tag (Regex <[^>]+>).
std::wstring strip_rich_text_tags(const std::wstring& s)
{
    std::wstring out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == L'<') {
            size_t j = s.find(L'>', i + 1);
            if (j != std::wstring::npos && j > i + 1) { i = j; continue; }
        }
        out.push_back(s[i]);
    }
    return out;
}

// Frames a popup read waits for SavePopup.messageText to be filled in (bounded, 2-30).
constexpr int kPopupReadMinFrames = 2;
constexpr int kPopupReadMaxFrames = 30;

// ReadPopupTextDelayed: from two frames on, read SavePopup.messageText plus the focused button
// as soon as the text is there. The C# read once at two frames; the completion popup ("Save
// complete") re-shows the same SavePopup and fills its text later, after the write, so that
// single read found it empty ("Message is empty" in the FF2 log, 2026-10-10) and the
// completion was never spoken. Bounded retry, one announcement.
void read_popup_text_delayed(void* popup, const char* context)
{
    int frames = 0;
    frame::every_frame([popup, context, frames]() mutable {
        ++frames;
        if (frames < kPopupReadMinFrames) return true;
        bool done = hooks::guarded_r("SaveLoad.ReadPopupText", true, [&] {
            if (!unity::alive(popup) || !unity::active_in_hierarchy(popup)) return true;
            void* message_text = ref::rd<void*>(popup, fSavePopupMessage);
            if (!message_text) {
                LOGF("[SaveLoad] %s: messageText pointer is null", context);
                return true;
            }
            std::wstring message = unity::ui_text(message_text);
            if (text::is_blank(message)) {
                if (frames >= kPopupReadMaxFrames) LOGF("[SaveLoad] %s: message still empty after %d frames", context, frames);
                return false;   // not filled in yet: retry
            }
            message = strip_rich_text_tags(message);
            // Append the initially-focused button (Yes/No) so it is heard right after the body;
            // save, load and quick save share this SavePopup layout.
            std::wstring button = dialogue::read_focused_button(popup, fSavePopupCursor, fSavePopupCommands);
            ffpr::say(text::is_blank(button) ? message : message + L" " + button);
            return true;
        });
        return !done && frames < kPopupReadMaxFrames;
    }, "SaveLoad.ReadPopupText", kPopupReadMaxFrames + 1);
}

// ReadSavePopup
void read_save_popup(void* controller, ref::Field& save_popup_field, const char* context, const char* popup_type)
{
    if (!controller) {
        LOGF("[SaveLoad] %s: Controller pointer is null", context);
        return;
    }
    void* popup = ref::rd<void*>(controller, save_popup_field);
    if (!popup) {
        LOGF("[SaveLoad] %s: SavePopup pointer is null", context);
        return;
    }
    // State for button navigation, at once.
    set_save_load_active(true);
    g_in_confirmation = true;
    dialogue::popup_state::set_active(popup_type, popup, &fSavePopupCommands);
    read_popup_text_delayed(popup, context);
}

void clear_popup_state()
{
    save_load_reset_state();
    dialogue::popup_state::clear();
}

using SetPopup_t = void (*)(void* self, bool is_enable, const void* mi);
SetPopup_t o_TitleLoadSetPopupActive;
SetPopup_t o_MenuLoadSetPopupActive;
SetPopup_t o_MenuSaveSetPopupActive;
SetPopup_t o_InterruptionSetEnablePopup;

void d_TitleLoadSetPopupActive(void* self, bool is_enable, const void* mi)
{
    o_TitleLoadSetPopupActive(self, is_enable, mi);
    hooks::guarded("SaveLoad.TitleLoad", [&] {
        if (is_enable) {
            if (kTitleLoad.is(self)) read_save_popup(self, fTitleLoadPopup, "TitleLoad", "TitleLoadPopup");
        } else {
            clear_popup_state();
        }
    });
}

void d_MenuLoadSetPopupActive(void* self, bool is_enable, const void* mi)
{
    o_MenuLoadSetPopupActive(self, is_enable, mi);
    hooks::guarded("SaveLoad.MainMenuLoad", [&] {
        if (is_enable) {
            if (kMenuLoad.is(self)) read_save_popup(self, fMenuLoadPopup, "MainMenuLoad", "MainMenuLoadPopup");
        } else {
            clear_popup_state();
        }
    });
}

void d_MenuSaveSetPopupActive(void* self, bool is_enable, const void* mi)
{
    o_MenuSaveSetPopupActive(self, is_enable, mi);
    hooks::guarded("SaveLoad.MainMenuSave", [&] {
        if (is_enable) {
            if (kMenuSave.is(self)) read_save_popup(self, fMenuSavePopup, "MainMenuSave", "MainMenuSavePopup");
        } else {
            clear_popup_state();
        }
    });
}

void d_InterruptionSetEnablePopup(void* self, bool is_enable, const void* mi)
{
    o_InterruptionSetEnablePopup(self, is_enable, mi);
    hooks::guarded("SaveLoad.QuickSave", [&] {
        if (is_enable) {
            if (kInterruption.is(self)) read_save_popup(self, fInterruptionPopup, "QuickSave", "QuickSavePopup");
        } else {
            clear_popup_state();
        }
    });
}

// --- SaveListPatches ----------------------------------------------------------------------------

// DelayedReadSlot: one frame after SaveListController.SetActive(true), read the highlighted slot.
void delayed_read_slot(void* controller)
{
    frame::next_frame([controller] {
        std::wstring announcement;
        hooks::guarded("SaveList.DelayedReadSlot", [&] {
            // Only when a real save/load menu is up: the background autosave during a map load also
            // fires SaveListController.SetActive, with no menu open.
            if (!unity::alive(controller) || !unity::active_in_hierarchy(controller) || !should_read_save_slot())
                return;
            void* cur = ref::rd<void*>(controller, fSaveListCursor);
            if (!cur) return;
            void* t = unity::transform(cur);
            if (!t) return;
            const int index = ui::cursor_index(cur);
            int count = -1;
            announcement = try_read_save_slot(t, index, count);
            if (!announcement.empty()) announcement = ui::menu_position(announcement, index, count);
        });
        if (!text::is_blank(announcement)) ffpr::say(announcement, true);
    }, "SaveList.DelayedReadSlot");
}

using SaveListSetActive_t = void (*)(void* self, bool is_active, bool is_reset, bool is_load_game, const void* mi);
SaveListSetActive_t o_SaveListSetActive;

void d_SaveListSetActive(void* self, bool is_active, bool is_reset, bool is_load_game, const void* mi)
{
    o_SaveListSetActive(self, is_active, is_reset, is_load_game, mi);
    if (!is_active || !self) return;
    hooks::guarded("SaveList.SetActive", [&] {
        if (kSaveList.is(self)) delayed_read_slot(self);
    });
}

int install()
{
    int n = 0;
    if (hooks::attach(mTitleLoadSetPopupActive, &d_TitleLoadSetPopupActive, &o_TitleLoadSetPopupActive)) ++n;
    if (hooks::attach(mMenuLoadSetPopupActive, &d_MenuLoadSetPopupActive, &o_MenuLoadSetPopupActive)) ++n;
    if (hooks::attach(mMenuSaveSetPopupActive, &d_MenuSaveSetPopupActive, &o_MenuSaveSetPopupActive)) ++n;
    if (hooks::attach(mInterruptionSetEnablePopup, &d_InterruptionSetEnablePopup, &o_InterruptionSetEnablePopup)) ++n;
    if (hooks::attach(mSaveListSetActive, &d_SaveListSetActive, &o_SaveListSetActive)) ++n;

    queries::define("SaveLoadMenuState.IsActive", [] { return save_load_active(); });
    queries::define("SaveLoadMenuState.ShouldSuppress", [] { return save_load_should_suppress(); });
    queries::define("SaveListController_SetActive_Patch.ShouldReadSaveSlot", [] { return should_read_save_slot(); });
    return n;
}

core::Install s_install{"SaveLoad", &install, 100};

} // namespace

// SaveLoadMenuState.ShouldSuppress
bool save_load_should_suppress() { return save_load_active() && g_in_confirmation; }

// SaveListController_SetActive_Patch.ShouldReadSaveSlot: a real in-game save/load/quick-save menu
// is open (MenuManager.IsOpen, reliably false during a map load's scene construction), or the
// title Load screen is on screen (not a MenuManager menu).
bool should_read_save_slot()
{
    return hooks::guarded_r("SaveList.ShouldReadSaveSlot", false, [] {
        if (void* mm = ref::instance(kMenuManager))
            if (ref::call<bool>(mMenuIsOpen, mm)) return true;
        void* title_load = unity::find_object(kTitleLoad);
        return title_load && unity::active_in_hierarchy(title_load);
    });
}

// SaveSlotReader.TryReadSaveSlot
std::wstring try_read_save_slot(void* cursor_transform, int cursor_index, int& count)
{
    count = -1;
    if (!cursor_transform) return {};
    return hooks::guarded_r("SaveSlotReader", std::wstring(), [&]() -> std::wstring {
        // Walk up to the save list structure.
        void* current = cursor_transform;
        for (int depth = 0; current && depth < 15; ++depth) {
            const std::wstring lower = text::lower(unity::name(current));
            if (text::contains(lower, L"save") || text::contains(lower, L"load") ||
                text::contains(lower, L"data_select")) {
                void* content = find_slot_content_list(current);
                if (content && cursor_index >= 0 && cursor_index < unity::child_count(content)) {
                    count = unity::child_count(content);
                    bool found = false;
                    std::wstring info = read_slot_from_transform(unity::child(content, cursor_index), cursor_index, found);
                    if (found && !info.empty()) return info;
                }
            }
            current = unity::parent(current);
        }
        return {};
    });
}

} // namespace ffpr::system
