// Ports FF1 Core/ConfirmationDialog.cs.
//
// Simple Yes/No confirmation (virtual: no window, nothing steals focus). Game input is
// suppressed while it is open (the input area reads "ConfirmationDialog.IsOpen"). Keys are read
// with GetAsyncKeyState (modal_keys.h), as FF1 read them through GamepadManager.
//
// Chained prompts: a Yes/No callback may itself open a new confirmation. The dialog then stays
// open and the new prompt is spoken at once, so the player goes from one question to the next
// without a close/reopen gap.
#include <windows.h>
#include <cstdint>
#include <string>
#include <utility>
#include "core.h"
#include "frame.h"
#include "log.h"
#include "modtext.h"
#include "say.h"
#include "mod_ui.h"
#include "modal_keys.h"

namespace ui::confirmation {
namespace {

struct State {
    bool open = false;
    std::wstring prompt;
    Callback on_yes;
    Callback on_no;
    bool selected_yes = true;   // default selection is Yes
    uint64_t input_frame = UINT64_MAX;
    bool input_result = false;
};

State* g_state = nullptr;
State& st()
{
    if (!g_state) g_state = new State();
    return *g_state;
}

/// Speaks the chosen option after a short delay, then runs the callback. If the callback opened
/// a new prompt (chained confirmation), the dialog stays open (the new prompt announced itself);
/// otherwise it closes.
void delayed_close_announcement(std::wstring text, Callback callback)
{
    // Clear the callbacks first, so a callback that opens a new prompt (which sets on_yes again)
    // can be detected.
    State& s = st();
    s.on_yes = nullptr;
    s.on_no = nullptr;

    frame::after_seconds(0.1, [text = std::move(text), callback = std::move(callback)] {
        ffpr::say(text, true);
        if (callback) callback();
        if (!st().on_yes) close();
    }, "ConfirmationDialog.DelayedCloseAnnouncement");
}

bool handle_input_now()
{
    State& s = st();
    if (!s.open) return false;

    modal_keys::sample();
    using modal_keys::pressed;

    // Y - Yes at once
    if (pressed('Y')) {
        delayed_close_announcement(T(L"Yes"), s.on_yes);
        return true;
    }
    // N - No at once
    if (pressed('N')) {
        delayed_close_announcement(T(L"No"), s.on_no);
        return true;
    }
    // Escape - same as No
    if (pressed(VK_ESCAPE)) {
        delayed_close_announcement(T(L"Cancelled"), s.on_no);
        return true;
    }
    // Enter - confirm the current selection
    if (pressed(VK_RETURN)) {
        if (s.selected_yes)
            delayed_close_announcement(T(L"Yes"), s.on_yes);
        else
            delayed_close_announcement(T(L"No"), s.on_no);
        return true;
    }
    // Left/Right - switch the selection
    if (pressed(VK_LEFT) || pressed(VK_RIGHT)) {
        s.selected_yes = !s.selected_yes;
        ffpr::say(s.selected_yes ? T(L"Yes") : T(L"No"), true);
        return true;
    }
    return true;   // consume all input while the dialog is open
}

int install()
{
    queries::define("ConfirmationDialog.IsOpen", [] { return is_open(); });
    actions::define("ConfirmationDialog.HandleInput", [] { handle_input(); });
    return 0;
}

core::Install s_install{"ConfirmationDialog", &install, 100};

} // namespace

bool is_open() { return g_state && g_state->open; }

void open(const std::wstring& prompt, Callback on_yes, Callback on_no)
{
    State& s = st();
    const bool was_already_open = s.open;

    s.open = true;
    s.prompt = prompt;
    s.on_yes = std::move(on_yes);
    s.on_no = std::move(on_no);
    s.selected_yes = true;   // default to Yes

    const std::wstring announcement = s.prompt + L" " + T(L"Yes or No");
    if (!was_already_open) {
        // First open: speak the prompt after a short delay so it settles cleanly.
        modal_keys::seed();
        frame::after_seconds(0.1, [announcement] { ffpr::say(announcement, true); },
                             "ConfirmationDialog.DelayedPromptAnnouncement");
    } else {
        // Continuation: the dialog is already open, speak the new prompt at once.
        ffpr::say(announcement, true);
    }
}

void close()
{
    State& s = st();
    if (!s.open) return;
    s.open = false;
    s.on_yes = nullptr;
    s.on_no = nullptr;
}

bool handle_input()
{
    State& s = st();
    const uint64_t f = frame::count();
    if (s.input_frame == f) return s.input_result;   // already handled this frame
    s.input_frame = f;
    s.input_result = handle_input_now();
    return s.input_result;
}

} // namespace ui::confirmation
