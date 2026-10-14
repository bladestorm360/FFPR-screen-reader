// Status pages share the renderer-backed review buffer in every game.
// Native parameter rows decide the labels and values; there is no universal stat list.
#include "status.h"
#include "../core.h"
#include "../dedup.h"
#include "../displayed_ui.h"
#include "../frame.h"
#include "../game.h"
#include "../hooks.h"
#include "../modtext.h"
#include "../text.h"
#include "../unity.h"
#include "../system/system.h"
#include "../ui/review_buffer.h"
#include <tuple>

namespace ffpr::status {
namespace {
#define DETAILS(N, MASK) \
ref::Class kDetails##N{"Serial.FF" #N ".UI.KeyInput", "StatusDetailsController", MASK}; \
ref::Method mInit##N{kDetails##N, "InitDisplay", 0}, mExit##N{kDetails##N, "ExitDisplay", 0};
DETAILS(1, game::kFF1) DETAILS(2, game::kFF2) DETAILS(3, game::kFF3)
DETAILS(4, game::kFF4) DETAILS(5, game::kFF5) DETAILS(6, game::kFF6)
#undef DETAILS
ref::Class kBase{"Serial.Template.UI", "StatusDetailsControllerBase", game::kFF1 | game::kFF2 | game::kFF3 | game::kFF4 | game::kFF5};
ref::Method mNext{kBase, "SetNextPlayer", 0}, mPrev{kBase, "SetPrevPlayer", 0};
ref::Method mNext6{kDetails6, "SetNextPlayer", 0}, mPrev6{kDetails6, "SetPrevPlayer", 0};
using Callback = void (*)(void*, const void*);
Callback o_init, o_exit, o_next, o_prev;
ref::Class* g_details = nullptr;
unsigned g_generation = 0;
void* g_saved_owner = nullptr;
std::wstring g_saved_name;
int g_saved_index = 0;

void* char_view(void* owner) { return displayed::field(displayed::field(owner, "statusController"), "view"); }
std::wstring title(void* owner)
{
    void* view = char_view(owner);
    auto name = displayed::text_field(view, "nameText");
    auto level = displayed::pair(view, "fixedLevelText", "currentLevelText");
    return name + (level.empty() ? L"" : L". " + level);
}
std::vector<std::wstring> lines(void* owner, std::vector<int>& groups)
{
    std::vector<std::wstring> out;
    auto add = [&](std::wstring value) { if (!value.empty()) out.push_back(std::move(value)); };
    auto group = [&] { if (groups.empty() || groups.back() != static_cast<int>(out.size())) groups.push_back(static_cast<int>(out.size())); };
    group();
    void* sv = char_view(owner);
    add(displayed::text_field(sv, "jobNameText"));
    add(displayed::text_field(sv, "dominantArmText"));
    auto vitals = displayed::vital_lines(sv);
    out.insert(out.end(), vitals.begin(), vitals.end());
    void* dv = displayed::field(owner, "view");
    for (auto [label, value] : {std::pair{"fixedExpText", "expText"},
                              std::pair{"fixedNextExpText", "nextExpText"},
                              std::pair{"fixedCurrentJobExpText", "currentJobExpText"},
                              std::pair{"fixedJobLevelText", "jobLevelText"}})
        add(displayed::pair(dv, label, value));
    for (auto [label, current, separator, maximum] : {
            std::tuple{"fixedAbpText", "currentAbpText", "separateAbpText", "maxAbpText"},
            std::tuple{"fixedJobCountText", "currentJobCountText", "separateJobCountText", "maxJobCountText"},
            std::tuple{"fixedAbilityCount", "currentAbilityCountText", "separateAbilityCount", "maxAbilityCountText"}}) {
        auto value = displayed::pair(dv, label, current);
        if (!value.empty()) add(value + displayed::text_field(dv, separator) + displayed::text_field(dv, maximum));
    }
    group();
    void* parameters = displayed::field(owner, "contentList");
    for (int i = 0; i < unity::list_count(parameters); ++i) {
        void* view = displayed::field(unity::list_at(parameters, i), "view");
        auto value = displayed::text_field(view, "parameterValueText");
        if (value.empty()) continue;
        add(displayed::text_field(view, "fixedText") + L" " +
            displayed::text_field(view, "multipliedValueText") + displayed::text_field(view, "multipliedText") + L" " +
            value + displayed::text_field(view, "percentText"));
    }
    group();
    void* skills = displayed::field(owner, "skillLevelContentList");
    for (int i = 0; i < unity::list_count(skills); ++i) {
        add(displayed::skill_row(unity::list_at(skills, i)));
    }
    group();
    add(displayed::text_field(dv, "fixedBattleCommandText"));
    void* commands = displayed::field(dv, "battleCommandTextList");
    for (int i = 0; i < unity::list_count(commands); ++i) add(displayed::text(unity::list_at(commands, i)));
    void* abilities = displayed::field(dv, "equipAbilityContentList");
    for (int i = 0; i < unity::list_count(abilities); ++i)
        add(text::join(displayed::texts(displayed::field(unity::list_at(abilities, i), "view")), L" "));
    add(displayed::pair(dv, "fixedMagicalStoneText", "magicalStoneText"));
    add(displayed::pair(dv, "fixedLevelUpBonusText", "levelUpBonusText"));
    while (!groups.empty() && groups.back() >= static_cast<int>(out.size())) groups.pop_back();
    return out;
}
void remember()
{
    if (ui::review::active() && ui::review::domain() == ui::review::Domain::Status) {
        g_saved_owner = tracker().active_controller;
        g_saved_name = displayed::text_field(char_view(g_saved_owner), "nameText");
        g_saved_index = ui::review::index();
    }
}
void refresh(void* owner, bool switching)
{
    if (!g_details || !g_details->is(owner) || !displayed::visible(owner)) return;
    void* data = displayed::field(displayed::field(owner, "statusController"), "targetData");
    const auto name = displayed::text_field(char_view(owner), "nameText");
    // Preserve FF1/FF6's documented guard on redundant native player-switch calls.
    if (switching && game::in(game::kFF1 | game::kFF6) && tracker().current_character_data &&
        !dedup::text("Status.Character", name, dedup::Scope::Session)) return;
    if (!switching || !tracker().current_character_data) {
        dedup::reset("Status.Character"); dedup::text("Status.Character", name, dedup::Scope::Session);
    }
    tracker().active_controller = owner;
    tracker().current_character_data = data;
    tracker().is_navigation_active = true;
    set_current_character_data(data);
    ui::review::Options options;
    options.domain = ui::review::Domain::Status;
    if (!switching && owner == g_saved_owner && name == g_saved_name) options.restore_index = g_saved_index;
    auto content = lines(owner, options.groups);
    ui::review::show(owner, title(owner), std::move(content), std::move(options));
    tracker().current_stat_index = ui::review::index();
}
void defer(void* owner, bool switching)
{
    if (!g_details || !g_details->is(owner)) return;
    const auto generation = ++g_generation;
    frame::cancel_label("Status.Display");
    frame::next_frame([owner, switching, generation] { if (generation == g_generation) refresh(owner, switching); }, "Status.Display");
}
void d_init(void* self, const void* mi)
{
    o_init(self, mi);
    hooks::guarded("Status.Init", [self] {
        if (!g_details || !g_details->is(self)) return;
        system::clear_other_menu_states("StatusDetails");
        menu_state::set_active(menu_state::STATUS_DETAILS, true);
        defer(self, false);
    });
}
void d_exit(void* self, const void* mi)
{
    hooks::guarded("Status.SavePosition", [self] { if (g_details && g_details->is(self)) remember(); });
    o_exit(self, mi);
    hooks::guarded("Status.Exit", [self] {
        if (!g_details || !g_details->is(self)) return; // folded ExitDisplay bodies
        ++g_generation; frame::cancel_label("Status.Display");
        if (ui::review::owns(self)) ui::review::clear();
        tracker().reset(); clear_current_character_data();
        menu_state::reset(menu_state::STATUS_DETAILS);
    });
}
void d_next(void* self, const void* mi) { o_next(self, mi); hooks::guarded("Status.NextPlayer", [self] { defer(self, true); }); }
void d_prev(void* self, const void* mi) { o_prev(self, mi); hooks::guarded("Status.PreviousPlayer", [self] { defer(self, true); }); }
int install()
{
    ref::Class* types[] = {&kDetails1, &kDetails2, &kDetails3, &kDetails4, &kDetails5, &kDetails6};
    ref::Method* init[] = {&mInit1, &mInit2, &mInit3, &mInit4, &mInit5, &mInit6};
    ref::Method* exit[] = {&mExit1, &mExit2, &mExit3, &mExit4, &mExit5, &mExit6};
    int n = 0;
    for (int i = 0; i < 6; ++i) if (types[i]->applies()) {
        g_details = types[i];
        n += hooks::attach(*init[i], &d_init, &o_init) ? 1 : 0;
        n += hooks::attach(*exit[i], &d_exit, &o_exit) ? 1 : 0;
    }
    n += hooks::attach(game::is(game::Id::FF6) ? mNext6 : mNext, &d_next, &o_next) ? 1 : 0;
    n += hooks::attach(game::is(game::Id::FF6) ? mPrev6 : mPrev, &d_prev, &o_prev) ? 1 : 0;
    actions::define("StatusNavigationReader.NavigateNext", [] { navigate_next(); });
    actions::define("StatusNavigationReader.NavigatePrevious", [] { navigate_previous(); });
    actions::define("StatusNavigationReader.JumpToNextGroup", [] { jump_to_next_group(); });
    actions::define("StatusNavigationReader.JumpToPreviousGroup", [] { jump_to_previous_group(); });
    actions::define("StatusNavigationReader.JumpToTop", [] { jump_to_top(); });
    actions::define("StatusNavigationReader.JumpToBottom", [] { jump_to_bottom(); });
    actions::define("StatusNavigationReader.ReadCurrentStat", [] { read_current_stat(); });
    queries::define("StatusNavigationTracker.IsNavigationActive", [] { return tracker().is_navigation_active; });
    queries::define("StatusNavigationTracker.ValidateState", [] { return tracker().validate_state(); });
    queries::define("StatusNavigationTracker.IsActive", [] { return tracker().validate_state(); });
    return n;
}
core::Install s_install{"Status.DisplayedDetails", &install, 121};
}
void initialize_stat_list() { if (tracker().active_controller) refresh(tracker().active_controller, false); }
void navigate_next() { ui::review::next(); tracker().current_stat_index = ui::review::index(); }
void navigate_previous() { ui::review::previous(); tracker().current_stat_index = ui::review::index(); }
void jump_to_next_group() { ui::review::next_group(); tracker().current_stat_index = ui::review::index(); }
void jump_to_previous_group() { ui::review::previous_group(); tracker().current_stat_index = ui::review::index(); }
void jump_to_top() { ui::review::first(); tracker().current_stat_index = ui::review::index(); }
void jump_to_bottom() { ui::review::last(); tracker().current_stat_index = ui::review::index(); }
void read_current_stat() { ui::review::current(); }
}
