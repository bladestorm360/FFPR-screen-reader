// Ports FF1 Menus/KeyHelpReader.cs: the controls display.
//   - Shift+I (AnnounceKeyHelp): reads the on-screen control-hint bar (KeyHelpController) at once.
//   - Arrows/WASD: step the "Gamepad/Keyboard Controls" pop-up one entry at a time, with a
//     navigation buffer armed ONLY by the config area's controls pop-up (open_controls_help); never
//     by the hint bar, which hijacked the title Options menu's arrows in FF1.
// The buffer is FF1 Core/NavigationBuffer.cs reduced to the flat list this reader uses (no groups).
#include <string>
#include <vector>
#include "core.h"
#include "hooks.h"
#include "log.h"
#include "modtext.h"
#include "refs.h"
#include "say.h"
#include "text.h"
#include "ui_util.h"
#include "unity.h"
#include "system/system.h"
#include "field/object_cache.h"

namespace ffpr::system {
namespace {

ref::Class kKeyHelpController{"Last.UI.KeyInput", "KeyHelpController"};
ref::Class kKeyHelpView{"Last.UI.KeyInput", "KeyHelpView"};
ref::Field fKeyHelpView{kKeyHelpController, "view"};                        // private, no accessor
ref::Field fKeyHelpContentsParent{kKeyHelpView, "keyHelpContentsParent"};    // KeyHelpView.ContentsParent
ref::Method mInitialize{kKeyHelpController, "Initialize", 0};
using Initialize = void (*)(void*, const void*);
Initialize o_initialize;
void d_initialize(void* self, const void* mi) {
    o_initialize(self, mi);
    hooks::guarded("KeyHelp.Owner", [self] {
        if (kKeyHelpController.is(self)) field::cache::register_object(kKeyHelpController, self);
    });
}

// NavigationBuffer over pre-rendered entries.
struct Buffer {
    std::vector<std::wstring> entries;
    int index = 0;
    bool empty() const { return entries.empty(); }
    int count() const { return static_cast<int>(entries.size()); }
    const std::wstring& current()
    {
        if (index < 0 || index >= count()) index = 0;
        return entries[static_cast<size_t>(index)];
    }
    const std::wstring& next() { index = (index + 1) % count(); return current(); }
    const std::wstring& previous() { if (--index < 0) index = count() - 1; return current(); }
    const std::wstring& top() { index = 0; return current(); }
    const std::wstring& bottom() { index = count() - 1; return current(); }
};

Buffer g_buffer;          // empty = no buffer (C# null)
bool g_primed = false;
// The controls pop-up state. g_help_owner validates the screen is still up, so a missed close
// cannot leave the KeyHelp key context stuck.
bool g_help_active = false;
void* g_help_owner = nullptr;   // ConfigKeysSettingController (identity; alive-checked)

// GameObjectCache.Get<KeyHelpController>() ?? Refresh: cleared on every scene load.
void* g_controller_cache = nullptr;

void speak(const std::wstring& s)
{
    if (s.empty()) return;
    // Flat controls list: whole-buffer position (CurrentGroupPosition without groups).
    std::wstring line = g_buffer.empty() ? s : ui::menu_position(s, g_buffer.index, g_buffer.count());
    ffpr::say(line, true);
}

// Fallback orient-at-top if the open announce was missed.
const std::wstring& prime()
{
    g_primed = true;
    return g_buffer.current();
}

void navigate_next()
{
    if (g_buffer.empty()) return;
    speak(g_primed ? g_buffer.next() : prime());
}

void navigate_previous()
{
    if (g_buffer.empty()) return;
    speak(g_primed ? g_buffer.previous() : prime());
}

void jump_to_top()
{
    if (g_buffer.empty()) return;
    g_primed = true;
    speak(g_buffer.top());
}

void jump_to_bottom()
{
    if (g_buffer.empty()) return;
    g_primed = true;
    speak(g_buffer.bottom());
}

void* key_help_controller()
{
    if (unity::alive(g_controller_cache)) return g_controller_cache;
    g_controller_cache = field::cache::get(kKeyHelpController);
    return g_controller_cache;
}

// ReadVisibleEntries: "{action}: {key}" for each visible hint of the on-screen KeyHelpController.
std::vector<std::wstring> read_visible_entries()
{
    std::vector<std::wstring> entries;
    void* controller = key_help_controller();
    if (!controller || !unity::active_in_hierarchy(controller)) return entries;

    void* view = ref::rd<void*>(controller, fKeyHelpView);
    if (!view) return entries;
    void* contents_parent = ref::rd<void*>(view, fKeyHelpContentsParent);
    if (!contents_parent) return entries;
    void* contents = unity::transform(contents_parent);
    if (!contents) return entries;
    const int n = unity::child_count(contents);
    if (n == 0) return entries;

    // Each child is one entry (KeyIconController); entries on other pages are deactivated.
    for (int i = 0; i < n; ++i) {
        void* child = unity::child(contents, i);
        if (!child || !unity::active_in_hierarchy(child)) continue;
        std::vector<std::wstring> parts;
        for (void* t : unity::texts(child, false)) {
            std::wstring s = unity::ui_text(t);
            if (!text::is_blank(s)) parts.push_back(text::trim(s));
        }
        if (!parts.empty()) entries.push_back(text::join(parts, L": "));
    }
    return entries;
}

// AnnounceKeyHelp (Shift+I)
void announce_key_help()
{
    bool ok = false;
    std::wstring result;
    hooks::guarded("KeyHelpReader.AnnounceKeyHelp", [&] {
        std::vector<std::wstring> entries = read_visible_entries();
        result = entries.empty() ? T(L"No controls displayed") : text::join(entries, L", ");
        ok = true;
    });
    if (!ok) {
        LOGF("Error in AnnounceKeyHelp");
        result = T(L"Error reading controls");
    }
    ffpr::say(result, true);
}

int install()
{
    actions::define("KeyHelpReader.NavigateNext", [] { navigate_next(); });
    actions::define("KeyHelpReader.NavigatePrevious", [] { navigate_previous(); });
    actions::define("KeyHelpReader.JumpToTop", [] { jump_to_top(); });
    actions::define("KeyHelpReader.JumpToBottom", [] { jump_to_bottom(); });
    actions::define("KeyHelpReader.AnnounceKeyHelp", [] { announce_key_help(); });
    actions::define("KeyHelpReader.CloseControlsHelp", [] { close_controls_help(); });
    queries::define("KeyHelpReader.IsScreenActive", [] { return key_help_screen_active(); });

    // FF1 OnSceneLoaded: the controls help never survives a scene change, and GameObjectCache is
    // cleared.
    events::on(events::Kind::SceneLoaded, [] {
        g_controller_cache = nullptr;
        close_controls_help();
    });
    return hooks::attach(mInitialize, &d_initialize, &o_initialize) ? 1 : 0;
}

core::Install s_install{"KeyHelpReader", &install, 100};

} // namespace

// OpenControlsHelp: the controls pop-up opened with its rendered entries; announce the first.
void open_controls_help(void* owner, const std::vector<std::wstring>& entries)
{
    g_help_owner = owner;
    if (entries.empty()) {
        g_buffer = Buffer{};
        g_help_active = false;
        g_primed = false;
        return;
    }
    g_buffer = Buffer{};
    g_buffer.entries = entries;
    g_help_active = true;
    g_primed = true;   // entry 0 is announced now; the first Down moves to entry 1
    speak(g_buffer.current());
}

// CloseControlsHelp: the pop-up closed / returned to the controls list.
void close_controls_help()
{
    g_help_active = false;
    g_help_owner = nullptr;
    g_buffer = Buffer{};
    g_primed = false;
}

// IsScreenActive: true while the controls pop-up is on screen.
bool key_help_screen_active()
{
    if (!g_help_active) return false;
    bool on_screen = false;
    bool ok = false;
    hooks::guarded("KeyHelpReader.IsScreenActive", [&] {
        on_screen = unity::alive(g_help_owner) && unity::active_in_hierarchy(g_help_owner);
        ok = true;
    });
    if (!ok || !on_screen) {
        close_controls_help();
        return false;
    }
    return true;
}

} // namespace ffpr::system
