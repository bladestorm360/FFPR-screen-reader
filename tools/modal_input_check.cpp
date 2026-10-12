// Offline regression of the real modal sampler and settings menu; no game/window/control.
#include <windows.h>
#include <cstdio>
#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace fixture {
bool focused = true;
bool keys[256]{};
int key_reads = 0;
uint64_t tick = 1;
std::vector<std::wstring> spoken;
std::map<std::string, int> values;
std::vector<std::function<void()>> pending;
}

SHORT fake_key_state(int vk)
{
    ++fixture::key_reads;
    return fixture::keys[vk] ? static_cast<SHORT>(0x8000) : 0;
}

#define GetAsyncKeyState fake_key_state
#include "../src/core/ui/modal_input.cpp"
#undef GetAsyncKeyState
#include "../src/core/ui/mod_menu.cpp"

namespace core {
Install::Install(const char* n, int (*f)(), int o, uint8_t g) : name(n), fn(f), order(o), games(g) {}
}
namespace actions {
void define(const char*, Fn) {}
bool defined(const char*) { return false; }
bool run(const char*) { return false; }
}
namespace queries { void define(const char*, std::function<bool()>) {} }
namespace frame {
uint64_t count() { return fixture::tick; }
void subscribe(void (*)(), const char*) {}
void after_seconds(double, Task task, const char*) { fixture::pending.push_back(std::move(task)); }
}
namespace game { Id current() { return Id::FF2; } }
namespace input { bool game_window_focused() { return fixture::focused; } }
namespace modtext { std::wstring T(std::wstring_view key) { return std::wstring(key); } }
namespace ffpr { void say(std::wstring_view line, bool) { fixture::spoken.emplace_back(line); } }
namespace text {
std::wstring num(long long value) { return std::to_wstring(value); }
bool is_blank(std::wstring_view value) { return value.empty(); }
}
namespace settings {
int get_int(const char* key, int def) {
    const auto it = fixture::values.find(key);
    return it == fixture::values.end() ? def : it->second;
}
bool get_bool(const char* key, bool def) { return get_int(key, def ? 1 : 0) != 0; }
void set_int(const char* key, int value) { fixture::values[key] = value; }
bool toggle(const char* key, bool def) {
    const bool next = !get_bool(key, def);
    set_int(key, next ? 1 : 0);
    return next;
}
}
namespace ui {
std::wstring menu_position(const std::wstring& line, int, int) { return line; }
namespace text_input { bool is_open() { return false; } bool handle_input() { return false; } }
namespace confirmation { bool is_open() { return false; } bool handle_input() { return false; } }
}

int main()
{
    int failures = 0;
    auto expect = [&](bool ok, const char* message) {
        if (!ok) { ++failures; std::printf("FAIL %s\n", message); }
    };
    auto tick = [&] { ++fixture::tick; ui::handle_modal_input(); };
    auto press = [&](int key) {
        fixture::keys[key] = true; tick();
        fixture::keys[key] = false; tick();
    };

    ui::mod_menu::initialize();
    fixture::keys[VK_F8] = true;
    ui::mod_menu::open();
    for (auto& task : fixture::pending) task();
    fixture::pending.clear();
    tick();
    expect(ui::mod_menu::is_open(), "opening key held does not close the menu");
    fixture::keys[VK_F8] = false; tick();
    press(VK_RETURN);
    const auto initial_name = fixture::spoken.back();
    const size_t speech_count = fixture::spoken.size();

    fixture::focused = false;
    const int read_count = fixture::key_reads;
    for (int key : {VK_UP, VK_DOWN, VK_LEFT, VK_RIGHT, VK_RETURN, VK_SPACE, static_cast<int>('I'), VK_ESCAPE, VK_F8}) {
        press(key);
    }
    expect(fixture::key_reads == read_count, "background modal never reads hardware keys");
    expect(fixture::spoken.size() == speech_count && fixture::values.empty() && ui::mod_menu::is_open(),
           "background arrows, confirm, details and close keys have no menu effects");
    expect(!ui::modal_keys::held(VK_DOWN), "background held state is cleared");

    fixture::keys[VK_DOWN] = true;
    fixture::keys[VK_RETURN] = true;
    tick();
    fixture::focused = true; tick(); tick();
    expect(fixture::spoken.size() == speech_count && fixture::values.empty(),
           "keys held on focus return neither move nor change a setting");
    fixture::keys[VK_DOWN] = false;
    fixture::keys[VK_RETURN] = false; tick();
    press(VK_RETURN);
    expect(fixture::values.size() == 1 && fixture::spoken.back() != initial_name,
           "a fresh focused confirm changes the retained setting");
    const size_t once = fixture::spoken.size();
    fixture::keys[VK_DOWN] = true; tick();
    const size_t moved = fixture::spoken.size();
    ui::handle_modal_input();
    ui::mod_menu::handle_input();
    expect(moved == once + 1 && fixture::spoken.size() == moved,
           "main input, late tick and direct handler act only once per frame");
    fixture::keys[VK_DOWN] = false; tick();

    // The shared gate also protects a modal opened while the game is already unfocused.
    ui::mod_menu::close();
    fixture::focused = false;
    fixture::keys[VK_ESCAPE] = true;
    const int before_seed = fixture::key_reads;
    ui::mod_menu::open();
    expect(fixture::key_reads == before_seed, "background modal open does not sample hardware");
    tick();
    fixture::focused = true; tick();
    expect(ui::mod_menu::is_open(), "held Escape on focus return does not close a new modal");
    fixture::keys[VK_ESCAPE] = false; tick();
    press(VK_ESCAPE);
    expect(!ui::mod_menu::is_open(), "fresh focused Escape closes normally");
    std::printf("Modal focus regression: %s\n", failures ? "FAILED" : "PASS");
    return failures ? 1 : 0;
}
