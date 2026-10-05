// Ports FF1 Patches/TitleMenuPatches.cs (initial focus of the title menus, the title command
// count for "(n of N)") and Patches/TitleScreenPatches.cs (the "Press any button" prompt and the
// menu-state clear when the title main menu enables).
#include <string>
#include "core.h"
#include "frame.h"
#include "hooks.h"
#include "log.h"
#include "modtext.h"
#include "refs.h"
#include "say.h"
#include "text.h"
#include "unity.h"
#include "dialogue/dialogue.h"
#include "field/scene_events.h"
#include "system/system.h"

namespace ffpr::dialogue {
namespace {

ref::Class kTitleWindow{"Last.UI.KeyInput", "TitleWindowController"};
ref::Class kTitleWindowView{"Last.UI.KeyInput", "TitleWindowView"};
ref::Class kTitleCommand{"Last.UI.KeyInput", "TitleMenuCommandController"};
ref::Class kTouchTitleCommand{"Last.UI.Touch", "TitleMenuCommandController"};
ref::Class kSystemIndicator{"Last.Systems.Indicator", "SystemIndicator"};

ref::Field fTitleCommandController{kTitleWindow, "commandController"};
ref::Field fTitleView{kTitleWindow, "view"};
ref::Field fTitleStartText{kTitleWindowView, "startText"};
ref::Field fTitleCursor{kTitleCommand, "selectCursor"};
ref::Field fTitleActiveContents{kTitleCommand, "activeContents"};

ref::Method mInitSelect{kTitleWindow, "InitSelect", 0};
ref::Method mInitializeOption{kTitleWindow, "InitializeOption", 0};
ref::Method mInitializeExtra{kTitleWindow, "InitializeExtra", 0};
ref::Method mSetEnableMainMenu{kTitleCommand, "SetEnableMainMenu", 1};
ref::Method mTouchSetEnableMainMenu{kTouchTitleCommand, "SetEnableMainMenu", 1};
ref::Method mIndicatorHide{kSystemIndicator, "Hide", 0};

// --- TitleMenuReader ------------------------------------------------------------------------

// AnnounceFocus: the initially-focused item of the title menus. Both menus are drawn by the same
// TitleMenuCommandController; the initial cursor placement never fires Cursor.NextIndex, so the
// per-state Init callbacks reuse the generic cursor reader.
//   InitSelect       -> the main menu (first entry AND back-out)
//   InitializeOption -> the Options list
//   InitializeExtra  -> the Extras list
void announce_focus(void* inst, const char* source)
{
    if (!unity::alive(inst) || !unity::active_in_hierarchy(inst)) return;
    void* cmd = ref::rd<void*>(inst, fTitleCommandController);
    if (!cmd) return;
    void* cur = ref::rd<void*>(cmd, fTitleCursor);
    if (!cur) return;
    LOGF("[Title] Announcing initial title-menu focus via %s", source);
    system::wait_and_read_cursor(cur);
}

using Void_t = void (*)(void* self, const void* mi);
Void_t o_InitSelect;
Void_t o_InitializeOption;
Void_t o_InitializeExtra;
Void_t o_IndicatorHide;

void d_InitSelect(void* self, const void* mi)
{
    o_InitSelect(self, mi);
    hooks::guarded("Title.InitSelect", [&] { announce_focus(self, "InitSelect"); });
}

void d_InitializeOption(void* self, const void* mi)
{
    o_InitializeOption(self, mi);
    hooks::guarded("Title.InitializeOption", [&] { announce_focus(self, "InitializeOption"); });
}

void d_InitializeExtra(void* self, const void* mi)
{
    o_InitializeExtra(self, mi);
    hooks::guarded("Title.InitializeExtra", [&] { announce_focus(self, "InitializeExtra"); });
}

// --- TitleScreenPatches -------------------------------------------------------------------------
//
// Event-driven (FF1 round 2, 2026-09-24):
//  - ARM on each "Title" Unity scene load (boot AND return to title);
//  - SPEAK: TitleWindowController.UpdateNone shows the prompt, calling SystemIndicator.Hide and in
//    the same branch activating view.startParent. A Hide postfix, while armed, reads startText one
//    frame later. Other Hide callers find the prompt inactive, stay silent and keep the arm;
//  - DISARM: prompt spoken, main menu enabled, or the title controller gone.
bool g_press_prompt_armed = false;
bool g_title_controller_seen = false;
bool g_title_main_menu_shown = false;

// GetPressText: FF1 looks up UiMessageConstants.MENU_TITLE_PRESS_TEXT by reflection
// (Type.GetField). Under Il2CppInterop static fields are properties, so that lookup never
// resolves and FF1 always falls back to its own text. Ported as that effective behaviour.
std::wstring press_text() { return T(L"Press any button"); }

// SpeakPressPromptIfShown: one frame after a Hide (or the arm), speak the prompt if it is on screen.
void speak_press_prompt_if_shown()
{
    frame::next_frame([] {
        hooks::guarded("Title.PressPrompt", [] {
            if (!g_press_prompt_armed) return;
            if (g_title_main_menu_shown) {
                g_press_prompt_armed = false;
                return;
            }

            // One lookup per Hide event (not a poll). Missing before it was seen = still loading
            // (stay armed); missing after = the title was left.
            void* ctrl = unity::find_object(kTitleWindow);
            if (!ctrl) {
                if (g_title_controller_seen) g_press_prompt_armed = false;
                return;
            }
            g_title_controller_seen = true;

            void* view = ref::rd<void*>(ctrl, fTitleView);
            if (!view) return;
            void* start_text = ref::rd<void*>(view, fTitleStartText);
            if (!start_text) return;
            if (!unity::game_object(start_text) || !unity::active_in_hierarchy(start_text)) return;

            // The prompt is on screen now: read its displayed text.
            g_press_prompt_armed = false;
            const std::wstring raw = unity::ui_text(start_text);
            std::wstring spoken = !text::is_blank(raw) ? text::trim(text::strip_markup(text::trim(raw))) : press_text();
            LOGW(L"[Title] press prompt shown, text=", raw.c_str());
            if (!text::is_blank(spoken)) ffpr::say(spoken, false);
        });
    }, "Title.PressPrompt");
}

// SystemIndicator.Hide postfix: while armed, check the prompt one frame later.
void d_IndicatorHide(void* self, const void* mi)
{
    o_IndicatorHide(self, mi);
    if (!g_press_prompt_armed) return;
    hooks::guarded("Title.IndicatorHide", [] { speak_press_prompt_if_shown(); });
}

// TitleMenuCommandController.SetEnableMainMenu postfix (KeyInput and Touch).
void set_enable_main_menu_postfix(bool is_enable)
{
    if (!is_enable) return;
    g_title_main_menu_shown = true;   // press-any-button dismissed
    g_press_prompt_armed = false;
    menu_state::reset_all();
    actions::run("BattleResultPatches.ClearAllBattleMenuFlags");
}

using SetEnable_t = void (*)(void* self, bool is_enable, const void* mi);
SetEnable_t o_SetEnableMainMenu;
SetEnable_t o_TouchSetEnableMainMenu;

void d_SetEnableMainMenu(void* self, bool is_enable, const void* mi)
{
    o_SetEnableMainMenu(self, is_enable, mi);
    hooks::guarded("Title.SetEnableMainMenu", [&] { set_enable_main_menu_postfix(is_enable); });
}

void d_TouchSetEnableMainMenu(void* self, bool is_enable, const void* mi)
{
    o_TouchSetEnableMainMenu(self, is_enable, mi);
    hooks::guarded("Title.TouchSetEnableMainMenu", [&] { set_enable_main_menu_postfix(is_enable); });
}

int install()
{
    int n = 0;
    if (hooks::attach(mInitSelect, &d_InitSelect, &o_InitSelect)) ++n;
    if (hooks::attach(mInitializeOption, &d_InitializeOption, &o_InitializeOption)) ++n;
    if (hooks::attach(mInitializeExtra, &d_InitializeExtra, &o_InitializeExtra)) ++n;
    if (hooks::attach(mSetEnableMainMenu, &d_SetEnableMainMenu, &o_SetEnableMainMenu)) ++n;
    if (hooks::attach(mTouchSetEnableMainMenu, &d_TouchSetEnableMainMenu, &o_TouchSetEnableMainMenu)) ++n;
    if (hooks::attach(mIndicatorHide, &d_IndicatorHide, &o_IndicatorHide)) ++n;

    actions::define("TitleScreenPatches.ArmPressPrompt", [] { arm_press_prompt(); });
    // FF1 FFI_ScreenReaderMod.OnSceneLoaded arms the prompt when the "Title" Unity scene loads
    // (at the START of every title load: boot and return to title).
    events::on(events::Kind::SceneLoaded, [] {
        if (field::scene_name() == L"Title") arm_press_prompt();
    });
    return n;
}

core::Install s_install{"TitleScreen", &install, 100};

} // namespace

// TitleScreenPatches.ArmPressPrompt: arm for this title visit, and check once right away in case
// the prompt is already up (normally it is not: the scene loads seconds before UpdateNone).
void arm_press_prompt()
{
    g_press_prompt_armed = true;
    g_title_controller_seen = false;
    g_title_main_menu_shown = false;
    speak_press_prompt_if_shown();
}

// TitleMenuReader.TryGetActiveCommandCount: visible title commands (activeContents, so disabled
// ones such as Continue with no save are excluded), or -1 when `cursor` is not the title cursor.
int title_active_command_count(void* cursor)
{
    return hooks::guarded_r("Title.CommandCount", -1, [&]() -> int {
        if (!cursor) return -1;
        void* cmd = unity::find_object(kTitleCommand);
        if (!cmd || !unity::active_in_hierarchy(cmd)) return -1;
        void* title_cursor = ref::rd<void*>(cmd, fTitleCursor);
        if (!title_cursor || title_cursor != cursor) return -1;
        void* list = ref::rd<void*>(cmd, fTitleActiveContents);
        if (!list) return -1;
        return unity::list_count(list);
    });
}

} // namespace ffpr::dialogue
