// Ports FF1 Patches/TitleMenuPatches.cs (initial focus of the title menus, the title command
// count for "(n of N)") and Patches/TitleScreenPatches.cs (the "Press any button" prompt and the
// menu-state clear when the title main menu enables).
#include <string>
#include "core.h"
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
ref::Method mUpdateNone{kTitleWindow, "UpdateNone", 0};
ref::Method mUpdateShortcut{kTitleWindow, "UpdateShortcutCommand", 0};

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
    timing::mark("Title select begin");
    { timing::Scope native("Title InitSelect",timing::Kind::Original);
      o_InitSelect(self, mi); }
    hooks::guarded("Title.InitSelect", [&] {
        if (!kTitleWindow.is(self)) return;
        dismiss_press_prompt();
        announce_focus(self, "InitSelect");
    });
    timing::mark("Title select ready");
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
// All six UpdateNone bodies inline SetEnableStartObject after fade/preload
// readiness. Observe that native producer AFTER it activates view.startParent;
// Indicator.Hide is too early and needlessly requires a later frame/scene cache.
// Read one text candidate only while this native title phase is pending. Native
// phase updates also own delayed renderer readiness, without a frame deadline.
// FF5/FF6 own cinematic timing; no mod timer predicts when their prompt appears.
bool g_press_prompt_armed = false;
bool g_title_main_menu_shown = false;
void* g_prompt_controller = nullptr; // native owner only; checked before reading

void dismiss_press_prompt()
{
    g_title_main_menu_shown = true;
    g_press_prompt_armed = false;
    g_prompt_controller = nullptr;
}

// Same native callback as activation when ready; no native pointer or work is
// deferred to a worker. say() copies the one ready line into the speech queue.
void speak_press_prompt_if_shown(void* ctrl)
{
    if (!g_press_prompt_armed || g_title_main_menu_shown || !ctrl ||
        ctrl != g_prompt_controller || !unity::alive(ctrl)) return;
    void* view = ref::rd<void*>(ctrl, fTitleView);
    void* start_text = ref::rd<void*>(view, fTitleStartText);
    auto raw = displayed::text(start_text);
    if (text::is_blank(raw)) return;
    g_press_prompt_armed = false;
    LOGW(L"[Title] press prompt shown, text=", raw.c_str());
    // Preserve the game's displayed wording first. This is mod guidance,
    // requested by the user, only for games with native idle intro playback.
    if (game::flags().title_idle_cinematic)
        raw += L". " + T(L"Wait to watch the intro cinematic.");
    ffpr::say(raw, true);
}

void arm_for_controller(void* ctrl)
{
    if (ctrl && kTitleWindow.is(ctrl)) {
        field::cache::register_object(kTitleWindow, ctrl);
        if (void* cmd = ref::rd<void*>(ctrl, fTitleCommandController)) field::cache::register_object(kTitleCommand, cmd);
    }
    g_press_prompt_armed = true;
    g_title_main_menu_shown = false;
    g_prompt_controller = ctrl;
}

Void_t o_InitNone, o_InitShortcut, o_UpdateNone, o_UpdateShortcut;
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
        if (kTitleWindow.is(self)) {
            arm_for_controller(self);
            speak_press_prompt_if_shown(self);
        }
    });
}

void d_UpdateNone(void* self, const void* mi)
{
    { timing::Scope native("Title UpdateNone",timing::Kind::Original);
      o_UpdateNone(self, mi); }
    if (!g_press_prompt_armed || (g_prompt_controller && self != g_prompt_controller)) return;
    hooks::guarded("Title.UpdateNone", [=] {
        if (!kTitleWindow.is(self)) return;
        if (!g_prompt_controller) arm_for_controller(self);
        speak_press_prompt_if_shown(self);
    });
}

void d_UpdateShortcut(void* self, const void* mi)
{
    o_UpdateShortcut(self, mi);
    if (!g_press_prompt_armed || (g_prompt_controller && self != g_prompt_controller)) return;
    hooks::guarded("Title.UpdateShortcut", [=] {
        if (!kTitleWindow.is(self)) return;
        if (!g_prompt_controller) arm_for_controller(self);
        speak_press_prompt_if_shown(self);
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
    if (hooks::attach(mUpdateNone, &d_UpdateNone, &o_UpdateNone)) ++n;
    if (hooks::attach(mUpdateShortcut, &d_UpdateShortcut, &o_UpdateShortcut)) ++n;

    actions::define("TitleScreenPatches.ArmPressPrompt", [] { arm_press_prompt(); });
    // FF1 FFI_ScreenReaderMod.OnSceneLoaded arms the prompt when the "Title" Unity scene loads
    // (at the START of every title load: boot and return to title).
    events::on(events::Kind::SceneLoaded, [] {
        if (field::scene_name() == L"Title" || field::scene_name() == L"TitleScreen") arm_press_prompt();
        // Additive UI/preload scenes do not end the title's native phase.
        // InitSelect/main-menu enable retire it; every read validates its owner.
    });
    return n;
}

core::Install s_install{"TitleScreen", &install, 100};

} // namespace

// TitleScreenPatches.ArmPressPrompt: arm this visit; native title updates own readiness.
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
