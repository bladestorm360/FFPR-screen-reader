// Ports FF1 Patches/TitleMenuPatches.cs (initial focus of the title menus, the title command
// count for "(n of N)") and Patches/TitleScreenPatches.cs (the "Press any button" prompt and the
// menu-state clear when the title main menu enables).
#include <string>
#include "core.h"
#include "frame.h"
#include "game.h"
#include "hooks.h"
#include "log.h"
#include "modtext.h"
#include "refs.h"
#include "say.h"
#include "text.h"
#include "unity.h"
#include "displayed_ui.h"
#include "dialogue/dialogue.h"
#include "field/scene_events.h"
#include "field/object_cache.h"
#include "system/system.h"

namespace ffpr::dialogue {
namespace {

ref::Class kTitleWindow{"Last.UI.KeyInput", "TitleWindowController"};
ref::Class kTitleWindowView{"Last.UI.KeyInput", "TitleWindowView"};
ref::Class kTitleCommand{"Last.UI.KeyInput", "TitleMenuCommandController"};
ref::Class kTouchTitleCommand{"Last.UI.Touch", "TitleMenuCommandController"};
ref::Class kIndicator{"Last.Systems.Indicator", "SystemIndicator"};

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
ref::Method mInitNone{kTitleWindow, "InitNone", 0};
ref::Method mInitShortcut{kTitleWindow, "InitShortcutCommand", 0};
ref::Method mIndicatorHide{kIndicator, "Hide", 0};

void dismiss_press_prompt();

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
    field::cache::register_object(kTitleWindow, inst);
    field::cache::register_object(kTitleCommand, cmd);
    void* cur = ref::rd<void*>(cmd, fTitleCursor);
    if (!cur) return;
    LOGF("[Title] Announcing initial title-menu focus via %s", source);
    system::wait_and_read_cursor(cur);
}

using Void_t = void (*)(void* self, const void* mi);
Void_t o_InitSelect;
Void_t o_InitializeOption;
Void_t o_InitializeExtra;

void d_InitSelect(void* self, const void* mi)
{
    o_InitSelect(self, mi);
    hooks::guarded("Title.InitSelect", [&] {
        if (!kTitleWindow.is(self)) return;
        dismiss_press_prompt();
        announce_focus(self, "InitSelect");
    });
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
// All six UpdateNone bodies inline SetEnableStartObject: they call Indicator.Hide
// immediately before activating view.startParent. The wrapper is never called
// on this path. InitShortcutCommand also activates that parent directly. Start
// a bounded rendered-text read from those actual events, not an Update hook.
// FF5/FF6 own cinematic timing; no mod timer predicts when their prompt appears.
bool g_press_prompt_armed = false;
bool g_title_main_menu_shown = false;

void dismiss_press_prompt()
{
    g_title_main_menu_shown = true;
    g_press_prompt_armed = false;
    frame::cancel_label("Title.PressPrompt");
}

// One frame after the presentation event, wait only for its actual fade/text.
void speak_press_prompt_if_shown(void* ctrl)
{
    if (!ctrl) return;
    frame::cancel_label("Title.PressPrompt");
    frame::every_frame([ctrl] {
        if (!g_press_prompt_armed || g_title_main_menu_shown || !unity::alive(ctrl)) return false;
        void* view = ref::rd<void*>(ctrl, fTitleView);
        void* start_text = ref::rd<void*>(view, fTitleStartText);
        if (!start_text || !unity::active_in_hierarchy(start_text)) return false;
        auto raw = displayed::text(start_text);
        if (text::is_blank(raw)) return true;
        g_press_prompt_armed = false;
        LOGW(L"[Title] press prompt shown, text=", raw.c_str());
        // Preserve the game's displayed wording first. This is mod guidance,
        // requested by the user, only for games with native idle intro playback.
        if (game::flags().title_idle_cinematic)
            raw += L". " + T(L"Wait to watch the intro cinematic, or press a button to open the title menu.");
        ffpr::say(raw, true);
        return false;
    }, "Title.PressPrompt", 60);
}

void arm_for_controller(void* ctrl)
{
    if (ctrl && kTitleWindow.is(ctrl)) {
        field::cache::register_object(kTitleWindow, ctrl);
        if (void* cmd = ref::rd<void*>(ctrl, fTitleCommandController)) field::cache::register_object(kTitleCommand, cmd);
    }
    frame::cancel_label("Title.PressPrompt");
    g_press_prompt_armed = true;
    g_title_main_menu_shown = false;
    speak_press_prompt_if_shown(ctrl);
}

Void_t o_InitNone, o_InitShortcut, o_IndicatorHide;
void d_InitNone(void* self, const void* mi)
{
    o_InitNone(self, mi);
    hooks::guarded("Title.InitNone", [=] {
        if (kTitleWindow.is(self)) arm_for_controller(self);
    });
}

void d_InitShortcut(void* self, const void* mi)
{
    o_InitShortcut(self, mi);
    hooks::guarded("Title.InitShortcut", [=] {
        if (kTitleWindow.is(self)) arm_for_controller(self);
    });
}

void d_IndicatorHide(void* self, const void* mi)
{
    o_IndicatorHide(self, mi);
    hooks::guarded("Title.IndicatorHide", [=] {
        if (!g_press_prompt_armed || g_title_main_menu_shown || !kIndicator.is(self)) return;
        const auto& scene = field::scene_name();
        if (scene != L"Title" && scene != L"TitleScreen") return;
        // UpdateNone enables the prompt immediately after this call returns.
        // One contextual lookup per native Hide; none during field loading.
        speak_press_prompt_if_shown(field::cache::get(kTitleWindow));
    });
}

// TitleMenuCommandController.SetEnableMainMenu postfix (KeyInput and Touch).
void set_enable_main_menu_postfix(bool is_enable)
{
    if (!is_enable) return;
    dismiss_press_prompt();
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
    if (hooks::attach(mInitNone, &d_InitNone, &o_InitNone)) ++n;
    if (hooks::attach(mInitShortcut, &d_InitShortcut, &o_InitShortcut)) ++n;
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
    // One scene-entry recovery lookup; the native activation event owns readiness.
    arm_for_controller(field::cache::get(kTitleWindow));
}

// TitleMenuReader.TryGetActiveCommandCount: visible title commands (activeContents, so disabled
// ones such as Continue with no save are excluded), or -1 when `cursor` is not the title cursor.
int title_active_command_count(void* cursor)
{
    return hooks::guarded_r("Title.CommandCount", -1, [&]() -> int {
        if (!cursor) return -1;
        void* cmd = field::cache::get(kTitleCommand);
        if (!cmd || !unity::active_in_hierarchy(cmd)) return -1;
        void* title_cursor = ref::rd<void*>(cmd, fTitleCursor);
        if (!title_cursor || title_cursor != cursor) return -1;
        void* list = ref::rd<void*>(cmd, fTitleActiveContents);
        if (!list) return -1;
        return unity::list_count(list);
    });
}

} // namespace ffpr::dialogue
