// Ports FF1 Core/TextInputWindow.cs.
//
// Modal text entry (virtual: no window, nothing steals focus), used to name waypoints. Game
// input is suppressed while it is open (the input area reads "TextInputWindow.IsOpen"). Keys
// are read with GetAsyncKeyState (modal_keys.h), as FF1 read them through GamepadManager.
#include <windows.h>
#include <cstdint>
#include <string>
#include <utility>
#include "core.h"
#include "frame.h"
#include "log.h"
#include "modtext.h"
#include "say.h"
#include "text.h"
#include "mod_ui.h"
#include "modal_keys.h"

namespace ui::text_input {
namespace {

struct State {
    bool open = false;
    std::wstring buffer;
    std::wstring prompt;
    Confirm on_confirm;
    Cancel on_cancel;
    int cursor = 0;
    uint64_t input_frame = UINT64_MAX;
    bool input_result = false;
};

State* g_state = nullptr;
State& st()
{
    if (!g_state) g_state = new State();
    return *g_state;
}

/// Ready mod-owned answer and its callback belong to one ordered input event.
void complete_input(std::wstring text, std::function<void()> callback)
{
    close();
    ffpr::say(text, true);
    ffpr::QueuedSpeechScope followup;
    if (callback) callback();
}

std::wstring character_name(wchar_t c)
{
    switch (c) {
    case L' ': return T(L"space");
    case L'.': return T(L"period");
    case L',': return T(L"comma");
    case L'\'': return T(L"apostrophe");
    case L'"': return T(L"quote");
    case L'-': return T(L"dash");
    case L'_': return T(L"underscore");
    case L';': return T(L"semicolon");
    case L':': return T(L"colon");
    case L'!': return T(L"exclamation");
    case L'?': return T(L"question");
    case L'/': return T(L"slash");
    case L'\\': return T(L"backslash");
    case L'(': return T(L"open paren");
    case L')': return T(L"close paren");
    case L'[': return T(L"open bracket");
    case L']': return T(L"close bracket");
    case L'{': return T(L"open brace");
    case L'}': return T(L"close brace");
    case L'`': return T(L"backtick");
    case L'~': return T(L"tilde");
    case L'=': return T(L"equals");
    case L'+': return T(L"plus");
    case L'|': return T(L"pipe");
    default: return std::wstring(1, c);
    }
}

void insert(wchar_t c)
{
    State& s = st();
    s.buffer.insert(static_cast<size_t>(s.cursor), 1, c);
    ++s.cursor;
}

bool handle_punctuation(int vk, bool shift_held, wchar_t shift_char, wchar_t normal_char)
{
    if (!modal_keys::pressed(vk)) return false;
    insert(shift_held ? shift_char : normal_char);
    return true;
}

bool handle_input_now()
{
    State& s = st();
    if (!s.open) return false;

    modal_keys::sample();
    using modal_keys::pressed;
    using modal_keys::held;

    if (pressed(VK_RETURN)) {
        const std::wstring final_text = text::trim(s.buffer);
        if (final_text.empty()) {
            ffpr::say(T(L"Name cannot be empty"), true);
            return true;
        }
        Confirm callback = s.on_confirm;
        complete_input(TF(L"Confirmed: {0}", {final_text}),
                                   [callback = std::move(callback), final_text] {
                                       if (callback) callback(final_text);
                                   });
        return true;
    }

    if (pressed(VK_ESCAPE)) {
        Cancel callback = s.on_cancel;
        complete_input(T(L"Cancelled"), std::move(callback));
        return true;
    }

    if (pressed(VK_BACK)) {
        if (s.cursor > 0) {
            const wchar_t deleted = s.buffer[static_cast<size_t>(s.cursor - 1)];
            s.buffer.erase(static_cast<size_t>(s.cursor - 1), 1);
            --s.cursor;
            ffpr::say(character_name(deleted), true);
        }
        return true;
    }

    if (pressed(VK_LEFT)) {
        if (s.cursor > 0) {
            --s.cursor;
            ffpr::say(character_name(s.buffer[static_cast<size_t>(s.cursor)]), true);
        }
        return true;
    }

    if (pressed(VK_RIGHT)) {
        if (s.cursor < static_cast<int>(s.buffer.size())) {
            ffpr::say(character_name(s.buffer[static_cast<size_t>(s.cursor)]), true);
            ++s.cursor;
        }
        return true;
    }

    if (pressed(VK_UP) || pressed(VK_DOWN)) {
        ffpr::say(!s.buffer.empty() ? s.buffer : T(L"empty"), true);
        return true;
    }

    if (pressed(VK_HOME)) {
        s.cursor = 0;
        if (!s.buffer.empty()) ffpr::say(character_name(s.buffer[0]), true);
        return true;
    }

    if (pressed(VK_END)) {
        s.cursor = static_cast<int>(s.buffer.size());
        return true;
    }

    if (pressed(VK_SPACE)) {
        insert(L' ');
        return true;
    }

    const bool shift_held = held(VK_LSHIFT) || held(VK_RSHIFT);

    // Letters A-Z
    for (int vk = 'A'; vk <= 'Z'; ++vk) {
        if (pressed(vk)) {
            wchar_t c = static_cast<wchar_t>(L'a' + (vk - 'A'));
            if (shift_held) c = static_cast<wchar_t>(L'A' + (vk - 'A'));
            insert(c);
            return true;
        }
    }

    // Numbers 0-9
    for (int vk = '0'; vk <= '9'; ++vk) {
        if (pressed(vk)) {
            insert(static_cast<wchar_t>(L'0' + (vk - '0')));
            return true;
        }
    }

    // Punctuation
    if (handle_punctuation(VK_OEM_MINUS, shift_held, L'_', L'-')) return true;
    if (handle_punctuation(VK_OEM_PERIOD, shift_held, L'>', L'.')) return true;
    if (handle_punctuation(VK_OEM_COMMA, shift_held, L'<', L',')) return true;
    if (handle_punctuation(VK_OEM_7, shift_held, L'"', L'\'')) return true;      // Quote
    if (handle_punctuation(VK_OEM_1, shift_held, L':', L';')) return true;       // Semicolon
    if (handle_punctuation(VK_OEM_2, shift_held, L'?', L'/')) return true;       // Slash
    if (handle_punctuation(VK_OEM_3, shift_held, L'~', L'`')) return true;       // BackQuote
    if (handle_punctuation(VK_OEM_4, shift_held, L'{', L'[')) return true;       // LeftBracket
    if (handle_punctuation(VK_OEM_5, shift_held, L'|', L'\\')) return true;      // Backslash
    if (handle_punctuation(VK_OEM_6, shift_held, L'}', L']')) return true;       // RightBracket
    if (handle_punctuation(VK_OEM_PLUS, shift_held, L'+', L'=')) return true;    // Equals

    return true;   // consume all input while the window is open
}

int install()
{
    queries::define("TextInputWindow.IsOpen", [] { return is_open(); });
    actions::define("TextInputWindow.HandleInput", [] { handle_input(); });
    return 0;
}

core::Install s_install{"TextInputWindow", &install, 100};

} // namespace

bool is_open() { return g_state && g_state->open; }

void open(const std::wstring& prompt, const std::wstring& initial, Confirm on_done, Cancel on_cancel)
{
    State& s = st();
    if (s.open) return;

    s.open = true;
    s.prompt = prompt;
    s.buffer = initial;
    s.on_confirm = std::move(on_done);
    s.on_cancel = std::move(on_cancel);
    s.cursor = static_cast<int>(s.buffer.size());
    modal_keys::seed();

    ffpr::say_sequence({s.prompt, s.buffer}, true);
}

void close()
{
    State& s = st();
    if (!s.open) return;
    s.open = false;
    s.on_confirm = nullptr;
    s.on_cancel = nullptr;
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

} // namespace ui::text_input
