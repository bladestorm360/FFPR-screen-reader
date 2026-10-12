// check-sources: text.cpp ui/navigation_buffer.cpp displayed_values.cpp battle/rendered_feedback.cpp
// Real shared readers, scheduler and native input detours with fake rendered views.
// No game/window/probe is launched.
#include <map>
#include <cstdio>
#include <cstdlib>
#include "../src/core/frame.cpp"
#include "../src/core/displayed_skill_progress.cpp"
#include "../src/core/ui/review_buffer.cpp"
#include "../src/core/battle/battle_result.cpp"
#include "../src/core/input/input_passthrough.cpp"
#include "../src/core/battle/rendered_feedback.h"

namespace fixture {
struct Node {
    bool alive = true, visible = true;
    int end_count = 0;
    int weapon_type = 0;
    Node* parent = nullptr;
    std::map<std::string, void*> fields;
    std::map<std::string, std::wstring> text;
    std::vector<std::wstring> texts;
    std::wstring row, name;
    void* sprite = nullptr;
    float x = 0;
    float fill = 0;
    std::vector<Node*> children;
};
using List = std::vector<void*>;
using SkillDict = std::map<int, int>;
constexpr int thresholds[] = {0,20,45,70,100,135,175,220,270,330,400,480,570,660,760,860};
int skill_level(int, int points, int, const void*) {
    int level = 1; for (int i = 1; i < 16; ++i) if (points >= thresholds[i]) level = i + 1;
    return level;
}
int skill_remaining(int group, int points, int type, const void* mi) {
    int level = skill_level(group, points, type, mi);
    return level == 16 ? 0 : thresholds[level] - points;
}
int skill_interval(int group, int points, int type, const void* mi) {
    int level = skill_level(group, points, type, mi);
    return level == 16 ? -860 : thresholds[level] - thresholds[level - 1];
}
void setting(int, void*, void*, const void*) {}
void color(void*, void*, void*, const void*) {}
std::map<std::string, std::function<bool()>> queries;
std::vector<std::wstring> spoken;
std::vector<input::Key> bound;
bool changed_stats = false;
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
int32_t Field::get() { return std::string(name) == "weaponType" ? offsetof(fixture::Node, weapon_type) : -1; }
il2cpp::Method* Method::get() { return reinterpret_cast<il2cpp::Method*>(ptr()); }
void* Method::ptr() {
    if (std::string(name) == "GetExpLevel") return reinterpret_cast<void*>(&fixture::skill_level);
    if (std::string(name) == "GetNextExp") return reinterpret_cast<void*>(&fixture::skill_remaining);
    if (std::string(name) == "GetExpDifference") return reinterpret_cast<void*>(&fixture::skill_interval);
    return nullptr;
}
void call_failed_note(const char*) { std::abort(); }
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
namespace settings { int get_int(const char*, int def) { return def; } bool get_bool(const char* key, bool def) { return std::string(key) == "AnnounceResultStatChanges" ? fixture::changed_stats : std::string(key) == "AutoDetail" ? fixture::detail : std::string(key) == "ExpCounter" ? false : def; } }
namespace ffpr {
void say(std::wstring_view s, bool) { fixture::spoken.emplace_back(s); }
namespace battle {
bool in_battle() { return true; }
bool dict_int_int(void* dict, int key, int& out) {
    if (!dict) return false;
    auto& values = *static_cast<fixture::SkillDict*>(dict); auto it = values.find(key);
    if (it == values.end()) return false; out = it->second; return true;
}
void state_set(const char*, bool) {}
void command_reset_state() {}
void magic_reset_state() {}
}
}
namespace modtext { std::wstring T(std::wstring_view s) { return std::wstring(s); }
std::wstring TF(std::wstring_view s, std::initializer_list<std::wstring> args) { return text::format(s, args); } }
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
std::wstring name(void* o) { return o ? static_cast<fixture::Node*>(o)->name : L""; }
Vec3 position(void* o) { return {o ? static_cast<fixture::Node*>(o)->x : 0, 0, 0}; }
void* transform(void* o) { return o; }
int child_count(void* o) { return o ? static_cast<int>(static_cast<fixture::Node*>(o)->children.size()) : 0; }
void* child(void* o, int i) { return static_cast<fixture::Node*>(o)->children[i]; }
void* get_component(void* o, ref::Class&) { return o; }
ref::Class& image_class() { static ref::Class image{"UnityEngine.UI", "Image"}; return image; }
int array_length(void* a) { return list_count(a); }
void* array_at(void* a, int i) { return list_at(a, i); }
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
std::wstring skill_name(void* o) { return visible(o) ? static_cast<fixture::Node*>(o)->name : L""; }
std::optional<float> bar_fraction(void* o) { return visible(o) ? std::optional<float>(static_cast<fixture::Node*>(o)->fill) : std::nullopt; }
bool container_visible(void* o) { return visible(o); }
std::wstring comparison_arrow(void*) { return {}; }
bool image_visible(void* o) { return visible(o) && static_cast<fixture::Node*>(o)->sprite; }
void* image_sprite(void* o) { return image_visible(o) ? static_cast<fixture::Node*>(o)->sprite : nullptr; }
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
    stat_view.text = {{"categoryText", L"Strength"}, {"beforValueText", L"10"}, {"afterValueText", L"11"}}; stat.fields["view"] = &stat_view;
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
    Node weapon_member; weapon_member.fields = {{"view", &status_view}, {"contentList", &learned_list}};
    List weapon_members{&weapon_member}; weapon.fields["contentList"] = &weapon_members;
    o_points = &points_phase; o_status = &status_phase; o_items = o_learning = o_ability = o_skill_levels = o_close = o_end = o_skill_page = o_update_menu = &phase;
    Phase steps[] = {&dStep1, &dStep2, &dStep3, &dStep4, &dStep5, &dStep6};
    oStep1 = oStep2 = oStep3 = oStep4 = oStep5 = oStep6 = &completion_step;
    for (int n = 1; n <= 6; ++n) for (bool automatic : {false, true}) {
        game_id = static_cast<game::Id>(n); detail = automatic; result_reset_state(); spoken.clear(); members.end_count = 0;
        d_points(&menu, nullptr);
        expect(queries::is("ReviewBuffer.IsResults") && !ui::review::active(), "native results owns context before buffer readiness");
        tick();
        expect(spoken.front() == (n == 2 ? L"Battle results. Gil obtained 50" : L"Battle results. EXP obtained 120. Gil obtained 50"), "only totals in points headline");
        expect(!contains(L"Firion EXP"), "animated member details wait for completion and Autodetail");
        steps[n - 1](&members, nullptr); tick();
        expect(!contains(L"Firion EXP"), "partial native animation completion keeps details deferred");
        steps[n - 1](&members, nullptr); tick();
        expect(contains(L"Firion EXP") == (automatic && n != 2), "final member details follow Autodetail");
        if (n != 1 && n != 3) {
            spoken.clear(); d_skill_page(&skill, nullptr); tick();
            expect(contains(L"Cure") == automatic && contains(L"Gil obtained"), "combined skill paging announces totals and follows Autodetail for details");
        }
        spoken.clear(); ui::review::first(); ui::review::next();
        if (n != 2) ui::review::next();
        expect(spoken == (n == 2 ? std::vector<std::wstring>({L"Battle results", L"Gil obtained 50"}) :
            std::vector<std::wstring>({L"Battle results", L"EXP obtained 120", L"Gil obtained 50"})), "title and real game totals are navigable");
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
        if (n == 2) { spoken.clear(); d_skill_levels(&point, nullptr); tick(); expect(spoken.front() == L"Skill gains. Gil obtained 50" && contains(L"Cure") == automatic, "FF2 weapon phase follows native entry"); }
        d_close(&menu, nullptr);
        expect(!queries::is("ReviewBuffer.IsResults") && !ui::review::active(), "close releases context and page");
    }
    expect(!ff2_weapon_rows(&weapon).size(), "FF2 weapon buffer cannot apply to other games");
    // A fade longer than the old deadline still reads on the native update event.
    result_reset_state(); spoken.clear(); totals.visible = false; members.visible = false; skill.visible = false; point.texts.clear();
    d_points(&menu, nullptr); for (int i = 0; i < 50; ++i) tick();
    expect(spoken.empty() && g_pending_page.has_value(), "pending results survive presentation fades beyond 30 frames");
    totals.visible = members.visible = skill.visible = true;
    d_update_menu(&menu, nullptr);
    expect(contains(L"EXP obtained") && !g_pending_page, "native update reveals totals without a global timer");
    point.texts = {L"Battle results"};
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
    // Full growth rows stay navigable; I and the new toggle use only changed rows.
    Node unchanged, rate, unchanged_view, rate_view;
    unchanged.fields["view"] = &unchanged_view; rate.fields["view"] = &rate_view;
    unchanged_view.text = {{"categoryText", L"Agility"}, {"beforValueText", L"8"}, {"afterValueText", L"8"}};
    rate_view.text = {{"categoryText", L"Evasion"}, {"beforValueText", L"2"}, {"afterValueText", L"2"},
        {"beforeMultipleSymbolText", L"x"}, {"afterMultipleSymbolText", L"x"},
        {"beforePercentValueText", L"45"}, {"afterPercentValueText", L"46"},
        {"beforePercentSymbolText", L"%"}, {"afterPercentSymbolText", L"%"}};
    List full_stats{&stat, &unchanged, &rate}; status.fields["contentList"] = &full_stats;
    detail = false; changed_stats = true; spoken.clear(); status_rows(&status); tick();
    expect(contains(L"Strength 10 to 11") && contains(L"Evasion 2x45% to 2x46%") && !contains(L"Agility"),
        "proactive brief includes numeric and percentage-only changes without unchanged rows");
    spoken.clear(); ui::review::details();
    expect(!contains(L"Strength"), "I on the heading does not read every delta");
    ui::review::next(); spoken.clear(); ui::review::details();
    expect(contains(L"Strength") && !contains(L"Evasion") && !contains(L"Agility"), "I rereads only the selected stat delta");
    spoken.clear(); ui::review::first(); ui::review::next(); ui::review::next();
    expect(contains(L"Agility"), "arrow buffer still contains unchanged stats");
    spoken.clear(); ui::review::details();
    expect(contains(L"No stat changes") && !contains(L"Strength"), "I on an unchanged row cannot read another row's delta");
    changed_stats = false; spoken.clear(); status_rows(&status); tick();
    expect(!contains(L"Strength") && spoken.front() == L"Firion level 2", "toggle off keeps entry concise");
    ui::review::next(); spoken.clear(); ui::review::details(); expect(contains(L"Strength"), "I works with proactive toggle off");
    detail = true; spoken.clear(); status_rows(&status); tick();
    expect(contains(L"Agility"), "Autodetail still reads the full table");
    int strength_reads = 0; for (auto& s : spoken) if (s.find(L"Strength") != std::wstring::npos) ++strength_reads;
    expect(strength_reads == 1, "Autodetail full table does not repeat the brief");
    expect(displayed::comparison_reading(&rate_view).changed, "percentage-only change is detected");
    rate_view.text["afterPercentValueText"] = L"45"; rate_view.text["afterValueText"] = L"3";
    expect(displayed::comparison_reading(&rate_view).changed, "multiplier-only change is detected");
    rate_view.text["afterValueText"] = L"2";
    expect(!displayed::comparison_reading(&rate_view).changed, "identical percentage/multiplier is not a change");
    rate_view.text.erase("afterValueText"); rate_view.text.erase("afterPercentValueText");
    expect(!displayed::comparison_reading(&rate_view).changed, "hidden after values never create inferred changes");

    Node hp_stat, hp_view;
    hp_view.text = {{"categoryText", L"HP"}, {"beforValueText", L"20"}, {"afterValueText", L"25"}};
    hp_stat.fields["view"] = &hp_view; List hp_rows{&hp_stat}; status.fields["contentList"] = &hp_rows;
    detail = false; changed_stats = true;
    for (int n = 1; n <= 6; ++n) {
        game_id = static_cast<game::Id>(n); spoken.clear(); status_rows(&status); tick();
        expect(contains(L"HP 20 to 25"), "displayed HP gain uses the stat toggle in every game");
        ui::review::next(); spoken.clear(); ui::review::details();
        expect(contains(L"HP 20 to 25"), "selected HP gain is manually readable in every game");
    }
    status.fields["contentList"] = &full_stats;

    // Real FF2 native-gauge hooks and integer math: every reachable gauge point.
    game_id = game::Id::FF2;
    displayed::o_setting = &setting; displayed::o_color = &color;
    Node skill_content, skill_view, gauge, before_data, after_data;
    SkillDict before_points{{0, 1}}, after_points{{0, 2}};
    before_data.fields["<SkillLevelTargets>k__BackingField"] = &before_points;
    after_data.fields["<SkillLevelTargets>k__BackingField"] = &after_points;
    skill_content.fields["view"] = &skill_view; skill_content.name = L"Sword";
    skill_view.fields["gauge"] = &gauge;
    for (int points = 0; points <= 860; ++points) {
        auto value = displayed::native_progress(points);
        expect(value.valid(), "native FF2 gauge operands are valid at every point and the cap");
        gauge.fill = value.fill(); skill_view.text["levelText"] = text::num(value.level);
        displayed::d_setting(points, nullptr, &gauge, nullptr);
        expect(displayed::skill_percentage(&skill_content) == value.percent(), "exact current gauge percent matches its visible level/fill");
        if (points < 860) {
            const auto next = displayed::native_progress(points + 1);
            const auto delta = displayed::skill_progress::gain(value, next);
            expect(delta && *delta >= 1, "one FF2 progress point is at least one percent, including rollover");
        }
    }
    auto current = displayed::native_progress(2); gauge.fill = current.fill(); skill_view.text["levelText"] = L"1";
    displayed::d_setting(2, nullptr, &gauge, nullptr); displayed::d_color(&skill_content, &before_data, &after_data, nullptr);
    expect(displayed::skill_change(&skill_content) == L"Sword: level 1 at 5% to level 1 at 10%, gained 5%", "weapon brief uses before/after native gauge fractions");
    Node weapon_character, weapon_name, weapon_view;
    List equipped{&skill_content}, weapon_characters{&weapon_character};
    weapon_name.text["nameText"] = L"Guy"; weapon_character.fields = {{"view", &weapon_name}, {"contentList", &equipped}};
    weapon_view.text["fixedRightHandText"] = L"Right hand";
    weapon.fields = {{"contentList", &weapon_characters}, {"view", &weapon_view}}; skill_content.row = L"Sword level 1, 10%";
    for (bool auto_detail : {false, true}) for (bool announce_changes : {false, true}) {
        detail = auto_detail; changed_stats = announce_changes; spoken.clear(); d_skill_levels(&point, nullptr); tick();
        expect(contains(L"gained 5%") == announce_changes, "weapon delta auto-announces only with the toggle in both Autodetail states");
        expect(contains(L"Sword level 1, 10%") == auto_detail, "Autodetail reads the displayed weapon table");
        ui::review::first(); ui::review::next(); ui::review::next(); spoken.clear(); ui::review::details();
        expect(contains(L"Guy: Right hand Sword") && contains(L"gained 5%"), "I reads the selected weapon row delta with either toggle state");
    }
    before_points[0] = 859; after_points[0] = 860; gauge.fill = 1; skill_view.text["levelText"] = L"16";
    displayed::d_setting(860, nullptr, &gauge, nullptr); displayed::d_color(&skill_content, &before_data, &after_data, nullptr);
    expect(displayed::skill_change(&skill_content).find(L"gained 1%") != std::wstring::npos, "cap fill is not an extra hundred percent gain");
    gauge.visible = false; expect(displayed::skill_change(&skill_content).empty(), "hidden weapon gauge cannot expose a delta");
    gauge.visible = true; gauge.fill = .3f; expect(displayed::skill_change(&skill_content).empty(), "different rendered gauge cannot expose a stale snapshot");
    gauge.fill = 1; game_id = game::Id::FF1; expect(displayed::skill_change(&skill_content).empty(), "weapon delta service is FF2-only");
    game_id = game::Id::FF2; displayed::clear_skill_changes(); expect(displayed::skill_change(&skill_content).empty(), "battle reset clears prior weapon deltas");

    // The real feedback reader uses visible glyphs rather than supplied counters.
    Node one, two, nine, image_one, image_two, image_nine, word, counter, damage_renderer, prefab;
    one.name = L"Battle_Common_DamageNumber_1"; two.name = L"Battle_Common_DamageNumber_2"; nine.name = L"Battle_Common_DamageNumber_9";
    image_one.sprite = &one; image_one.x = 1; image_two.sprite = &two; image_two.x = 2; image_nine.sprite = &nine; image_nine.x = 3;
    word.sprite = &one; List count_images{&image_one, &image_two};
    counter.fields = {{"hitText", &word}, {"numRenderers", &count_images}};
    spoken.clear(); word.visible = false; ffpr::battle::rendered::hit_count(&counter); tick();
    expect(spoken.empty(), "hit count waits for the visible word and digits");
    word.visible = true; tick(); expect(contains(L"12 hits"), "hit count decodes the current drawn digits");
    count_images = {&image_one}; spoken.clear(); ffpr::battle::rendered::hit_count(&counter); tick();
    expect(contains(L"1 hit") && !contains(L"7 hits"), "a single displayed hit is read without attack-capacity math");
    damage_renderer.fields["prefab"] = &prefab;
    prefab.children = {&image_nine, &image_two, &image_one}; // units-first native prefab
    int damage_read = -1; ffpr::battle::rendered::damage(&damage_renderer, false, [&](int v) { damage_read = v; }); tick();
    expect(damage_read == 129, "damage comes from drawn left-to-right glyphs, not child order or damage arguments");
    image_two.visible = false; expect(displayed::sprite_number({&image_nine, &image_two, &image_one}) == 19, "unused hidden slots are omitted");
    Node localized_hit; localized_hit.name = L"Battle_Common_DamageNumber_HITS_7"; image_two.visible = true; image_two.sprite = &localized_hit;
    expect(!displayed::sprite_number({&image_two}), "localized word suffix 7 cannot become seven hits");
    image_two.sprite = &two; image_two.x = image_one.x;
    expect(!displayed::sprite_number({&image_two, &image_one}), "unsettled overlapping number layout stays unreadable");
    image_two.x = 2;
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
