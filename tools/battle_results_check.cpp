// check-sources: text.cpp ui/navigation_buffer.cpp
// Real shared readers, scheduler and native input detours with fake rendered views.
// No game/window/probe is launched.
#include <map>
#include <cstdio>
#include <cstdlib>
#include "../src/core/frame.cpp"
#include "../src/core/ui/review_buffer.cpp"
#include "../src/core/battle/battle_result.cpp"
#include "../src/core/input/input_passthrough.cpp"

namespace fixture {
struct Node {
    bool alive = true, visible = true;
    int end_count = 0;
    Node* parent = nullptr;
    std::map<std::string, void*> fields;
    std::map<std::string, std::wstring> text;
    std::vector<std::wstring> texts;
    std::wstring row;
};
using List = std::vector<void*>;
std::map<std::string, std::function<bool()>> queries;
std::vector<std::wstring> spoken;
std::vector<input::Key> bound;
bool detail = false, suppress = false, pad = false;
bool keys[static_cast<int>(input::Key::Count)]{};
bool buttons[input::kButtonCount]{}, released[input::kButtonCount]{};
game::Id game_id = game::Id::FF2;
int native_calls = 0;
bool native_result = true;
Node point, status;
void tick() { frame::run_tick(); }
bool contains(std::wstring_view value) {
    for (auto& s : spoken) if (s.find(value) != std::wstring::npos) return true;
    return false;
}
void phase(void*, const void*) {}
void points_phase(void*, const void*) { ffpr::battle::point_rows(&point); }
void status_phase(void*, const void*) { ffpr::battle::status_rows(&status); }
void completion_step(void* s, const void*) { ++static_cast<Node*>(s)->end_count; }
bool native_key(void*, int32_t, const void*) { ++native_calls; return native_result; }
bool native_any(void*, const void*) { ++native_calls; return native_result; }
bool listener_down(void*, void*, int32_t, bool, bool, const void*) { ++native_calls; return native_result; }
bool listener_key(void*, void*, int32_t, bool, const void*) { ++native_calls; return native_result; }
}

namespace core { Install::Install(const char* n, int (*f)(), int o, uint8_t g) : name(n), fn(f), order(o), games(g) {} }
namespace ref {
Class::Class(const char* n, const char* c, uint8_t g, const char* a) : ns(n), name(c), games(g), assembly(a) {}
Method::Method(Class& c, const char* n, int a) : cls(c), name(n), argc(a) {}
Field::Field(Class& c, const char* n, const char* a) : cls(c), name(n), alt(a) {}
bool Class::is(void* o) { return o != nullptr; }
std::wstring rd_str(void*, Field&) { return L"Action"; }
}
namespace hooks {
bool attach_raw(ref::Method&, void*, void**) { return false; }
void note_fault(const char*) { std::abort(); }
}
namespace queries {
void define(const char* key, std::function<bool()> fn) { fixture::queries[key] = std::move(fn); }
bool is(const char* key) { auto i = fixture::queries.find(key); return i != fixture::queries.end() && i->second(); }
}
namespace actions { void define(const char*, Fn) {} }
namespace events { void on(Kind, Handler) {} }
namespace game { Id current() { return fixture::game_id; } }
namespace settings { bool get_bool(const char* key, bool def) { return std::string(key) == "AutoDetail" ? fixture::detail : std::string(key) == "ExpCounter" ? false : def; } }
namespace ffpr {
void say(std::wstring_view s, bool) { fixture::spoken.emplace_back(s); }
namespace battle {
bool in_battle() { return true; }
void state_set(const char*, bool) {}
void command_reset_state() {}
void magic_reset_state() {}
}
}
namespace modtext { std::wstring T(std::wstring_view s) { return std::wstring(s); } }
namespace ui { std::wstring menu_position(const std::wstring& s, int, int) { return s; } }
namespace sound { void play_exp_counter() {} void stop_exp_counter() {} void top_up_exp_counter() {} }
namespace speech { void request_load() {} }
namespace log_ { void write(const char*, ...) {} }
namespace il2cpp {
Class* object_class(Object*) { return nullptr; }
int32_t field_offset(Class*, const char* name) { return std::string(name) == "perormanceEndCount" ? static_cast<int32_t>(offsetof(fixture::Node, end_count)) : -1; }
}
namespace unity {
bool alive(void* o) { return o && static_cast<fixture::Node*>(o)->alive; }
int list_count(void* l) { return l ? static_cast<int>(static_cast<fixture::List*>(l)->size()) : 0; }
void* list_at(void* l, int i) { return l && i >= 0 && i < list_count(l) ? (*static_cast<fixture::List*>(l))[i] : nullptr; }
}
namespace displayed {
bool visible(void* o) { return unity::alive(o) && static_cast<fixture::Node*>(o)->visible && (!static_cast<fixture::Node*>(o)->parent || visible(static_cast<fixture::Node*>(o)->parent)); }
void* field(void* o, const char* name) { if (!o) return nullptr; auto& m = static_cast<fixture::Node*>(o)->fields; auto i = m.find(name); return i == m.end() ? nullptr : i->second; }
std::wstring text_field(void* o, const char* name) { if (!visible(o)) return {}; auto& m = static_cast<fixture::Node*>(o)->text; auto i = m.find(name); return i == m.end() ? L"" : i->second; }
std::vector<std::wstring> texts(void* o) { return visible(o) ? static_cast<fixture::Node*>(o)->texts : std::vector<std::wstring>{}; }
std::wstring pair(void* o, const char* label, const char* value) { const auto v = text_field(o, value); return v.empty() ? L"" : text_field(o, label) + L" " + v; }
std::wstring skill_row(void* o) { return visible(o) ? static_cast<fixture::Node*>(o)->row : L""; }
std::wstring comparison(void* o) { return skill_row(o); }
}
namespace input {
void add_binding(Key k, Mod, Context, const char*, const char*) { fixture::bound.push_back(k); }
bool key_in_use(Key k) { return fixture::keys[static_cast<int>(k)]; }
bool suppress_game_input() { return fixture::suppress; }
bool field_active() { return false; }
bool gamepad_available() { return fixture::pad; }
bool button_pressed(int b) { return fixture::buttons[b]; }
bool button_held(int b) { return fixture::buttons[b]; }
bool button_released(int b) { return fixture::released[b]; }
float left_trigger() { return 0; } float right_trigger() { return 0; }
namespace router {
bool is_button_consumed(int) { return false; }
bool is_stick_pulse_down(int) { return false; } bool is_stick_pulse_up(int) { return false; }
}
namespace gamepad {
bool lstick_up_pressed() { return false; } bool lstick_down_pressed() { return false; }
bool lstick_left_pressed() { return false; } bool lstick_right_pressed() { return false; }
bool lstick_up_held() { return false; } bool lstick_down_held() { return false; }
bool lstick_left_held() { return false; } bool lstick_right_held() { return false; }
}
}

int main()
{
    using namespace fixture;
    using namespace ffpr::battle;
    int failures = 0;
    auto expect = [&](bool ok, const char* message) { if (!ok) { ++failures; std::printf("FAIL %s\n", message); } };
    ui::review::install(); ffpr::battle::install();
    expect(std::find(bound.begin(), bound.end(), input::Key::L) == bound.end(), "L log binding removed");
    Node menu, totals, get_points, members, member, skill, learned, level, status_view, stat, stat_view, item, drop, weapon;
    List member_list{&member}, learned_list{&learned}, level_list{&level}, stat_list{&stat}, drop_list{&drop};
    member.row = L"Firion EXP 60 of 120"; learned.row = L"Cure learned"; level.row = L"Job level 2";
    stat_view.row = L"Strength 10 to 11"; stat.fields["view"] = &stat_view;
    status_view.text["nameText"] = L"Firion level 2";
    status_view.texts = {L"Firion level 2", L"Strength 10 to 11", L"Agility 8 to 9"};
    status.fields = {{"view", &status_view}, {"contentList", &stat_list}};
    totals.text = {{"getExpFixText", L"EXP obtained"}, {"getExpValueText", L"120"}, {"getGilFixText", L"Gil obtained"}, {"getGilValueText", L"50"}};
    get_points.fields["view"] = &totals;
    members.fields["contentList"] = &member_list;
    List performances{&member, &member}; // two completion callbacks
    members.fields["performanceList"] = &performances;
    skill.fields = {{"contentList", &learned_list}, {"levelUpContentList", &level_list}};
    menu.fields = {{"skillController", &skill}, {"itemControlle", &item}};
    point.fields = {{"getPointController", &get_points}, {"characterListConteroller", &members}, {"skillLevelController", &weapon}};
    point.texts = {L"Battle results"}; item.texts = {L"Items obtained"}; skill.texts = {L"Learned abilities"}; weapon.texts = {L"Skill gains"};
    item.fields["itemContentList"] = &drop_list; drop.row = L"Gil 10. Potion 1";
    weapon.fields["contentList"] = &learned_list;
    o_points = &points_phase; o_status = &status_phase; o_items = o_learning = o_ability = o_skill_levels = o_close = o_end = o_skill_page = &phase;
    Phase steps[] = {&dStep1, &dStep2, &dStep3, &dStep4, &dStep5, &dStep6};
    oStep1 = oStep2 = oStep3 = oStep4 = oStep5 = oStep6 = &completion_step;
    for (int n = 1; n <= 6; ++n) for (bool automatic : {false, true}) {
        game_id = static_cast<game::Id>(n); detail = automatic; result_reset_state(); spoken.clear(); members.end_count = 0;
        d_points(&menu, nullptr);
        expect(queries::is("ReviewBuffer.IsResults") && !ui::review::active(), "native results owns context before buffer readiness");
        tick();
        expect(spoken.front() == L"Battle results. EXP obtained 120. Gil obtained 50", "only totals in points headline");
        expect(contains(L"Firion EXP") == (automatic && n == 2), "animated member details wait for completion and Autodetail");
        steps[n - 1](&members, nullptr); tick();
        expect(contains(L"Firion EXP") == (automatic && n == 2), "partial native animation completion keeps details deferred");
        steps[n - 1](&members, nullptr); tick();
        expect(contains(L"Firion EXP") == automatic, "final member details follow Autodetail");
        if (n != 1 && n != 3) {
            spoken.clear(); d_skill_page(&skill, nullptr); tick();
            expect(contains(L"Cure") == automatic && contains(L"EXP obtained"), "combined skill paging announces totals and follows Autodetail for details");
        }
        spoken.clear(); ui::review::first(); ui::review::next(); ui::review::next();
        expect(spoken == std::vector<std::wstring>({L"Battle results", L"EXP obtained 120", L"Gil obtained 50"}), "title and totals are individually navigable");
        spoken.clear(); d_status(&menu, nullptr); tick();
        expect(spoken.front() == L"Firion level 2", "growth headline excludes descendant stat table");
        expect(contains(L"Strength") == automatic, "growth stats follow Autodetail");
        spoken.clear(); d_items(&menu, nullptr); tick();
        expect(spoken.front() == L"Items obtained" && contains(L"Gil 10") == automatic, "individual drops follow Autodetail");
        spoken.clear(); ui::review::details();
        expect(contains(L"Potion") && !contains(L"Strength") && !contains(L"EXP 60"), "manual details contains only current phase");
        spoken.clear(); d_learning(&menu, nullptr); tick();
        expect(spoken.front() == L"Learned abilities" && contains(L"Cure") == automatic, "learning phase honors Autodetail");
        spoken.clear(); d_ability(&menu, nullptr); tick();
        expect(spoken.front() == L"Ability and job level gains" && contains(L"Job level 2") == automatic, "ability/job phase honors Autodetail");
        if (n == 2) { spoken.clear(); d_skill_levels(&point, nullptr); tick(); expect(spoken.front() == L"Skill gains" && contains(L"Cure") == automatic, "FF2 weapon phase follows native entry"); }
        d_close(&menu, nullptr);
        expect(!queries::is("ReviewBuffer.IsResults") && !ui::review::active(), "close releases context and page");
    }
    // An unusually early native animation completion must not duplicate details.
    game_id = game::Id::FF1; detail = true; result_reset_state(); spoken.clear();
    members.end_count = 0;
    d_points(&menu, nullptr); dStep1(&members, nullptr); dStep1(&members, nullptr); tick(); tick();
    int member_announcements = 0;
    for (auto& s : spoken) if (s.find(L"Firion EXP") != std::wstring::npos) ++member_announcements;
    expect(member_announcements == 1, "early completion has one producer announcement");
    result_reset_state(); spoken.clear(); d_points(&menu, nullptr); tick();
    oForced1 = &phase; dForced1(&members, nullptr); tick();
    expect(contains(L"Firion EXP"), "forced native animation completion refreshes final details");
    // A later native phase invalidates old pending reads.
    result_reset_state(); spoken.clear(); d_points(&menu, nullptr); d_items(&menu, nullptr); tick();
    expect(!contains(L"EXP obtained"), "phase changes cancel obsolete reads");
    ui::review::Options status_options; status_options.domain = ui::review::Domain::Status;
    for (bool automatic : {false, true}) {
        detail = automatic; spoken.clear(); ui::review::show(&status, L"Maria level 3", {L"Strength 20", L"Agility 30"}, status_options);
        expect(spoken.front() == L"Maria level 3" && contains(L"Strength") == automatic, "regular status table follows Autodetail");
        spoken.clear(); ui::review::details(); expect(contains(L"Agility"), "regular status manual details works in both states");
    }

    input::o_GetKeyDown = input::o_GetKey = input::o_GetKeyUp = &native_key; input::o_GetAnyKey = &native_any;
    input::o_listener_down = &listener_down; input::o_listener_held = input::o_listener_up = &listener_key;
    input::o_any_held = &native_key; input::o_any_down = input::o_any_up = &native_any;
    auto all_checks = [&] {
        bool any = false;
        any |= input::d_GetKeyDown(nullptr, 4, nullptr); any |= input::d_GetKey(nullptr, 0, nullptr);
        any |= input::d_GetKeyUp(nullptr, 5, nullptr); any |= input::d_GetAnyKey(nullptr, nullptr);
        any |= input::d_listener_down(nullptr, nullptr, 3, false, false, nullptr);
        any |= input::d_listener_held(nullptr, nullptr, 3, false, nullptr);
        any |= input::d_listener_up(nullptr, nullptr, 3, false, nullptr);
        any |= input::d_any_held(nullptr, 3, nullptr); any |= input::d_any_down(nullptr, nullptr); any |= input::d_any_up(nullptr, nullptr);
        return any;
    };
    result_reset_state(); begin_phase(&menu, ResultPhase::Points); // empty buffer still owns input
    for (input::Key key : {input::Key::UpArrow, input::Key::DownArrow, input::Key::LeftArrow, input::Key::RightArrow, input::Key::W, input::Key::S, input::Key::A, input::Key::D, input::Key::I, input::Key::F7}) {
        keys[static_cast<int>(key)] = true; native_calls = 0;
        expect(!all_checks() && native_calls == 0, "review gestures consumed before all native input entry points");
        keys[static_cast<int>(key)] = false;
    }
    native_calls = 0; expect(all_checks() && native_calls == 10, "ordinary native confirm reaches input APIs");
    suppress = true; native_calls = 0; expect(!all_checks() && native_calls == 0, "controller/modals suppress both native input stacks");
    pad = true; buttons[input::kEast] = true; input::update_native_input_ownership(); suppress = false;
    expect(!all_checks(), "mode-exit held button cannot advance results");
    buttons[input::kEast] = false; released[input::kEast] = true; input::update_native_input_ownership();
    expect(!all_checks(), "mode-exit release cannot advance results");
    released[input::kEast] = false; input::update_native_input_ownership();
    native_result = false; buttons[input::kSouth] = true;
    expect(input::d_any_down(nullptr, nullptr) && input::d_listener_down(nullptr, nullptr, 3, false, false, nullptr), "normal SDL confirm advances results through listener");
    expect(!input::d_any_held(nullptr, 0, nullptr), "keyboard-only listener query does not inject controller");
    buttons[input::kSouth] = false; pad = false;
    result_reset_state(); native_result = true; keys[static_cast<int>(input::Key::UpArrow)] = true;
    native_calls = 0; expect(all_checks() && native_calls == 10, "review input ownership releases outside results");
    std::printf("Results: all-six phase/Autodetail/lifecycle and 10 native input entry points: %d failures\n", failures);
    return failures ? 1 : 0;
}
