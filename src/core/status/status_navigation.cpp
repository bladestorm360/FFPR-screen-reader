// Ports FF1 Menus/StatusNavigationReader.cs, Menus/StatusStatDefinitions.cs and
// Core/Handlers/StatusNavigationHandler.cs.
//
// The status details screen's virtual stat list: Up/Down step it, Shift jumps a group, Ctrl
// jumps to the top or bottom, R re-reads (the input area binds those keys to the actions below,
// gated on "StatusNavigationTracker.IsNavigationActive" + "ValidateState", the same gate as
// StatusNavigationHandler.HandleInput and InputManager's KeyContext.Status).
//
// The stat list is FF1's (5 + 9 + 5 + 4 entries). The other games' C# mods each have their own
// list, built on the same OwnedCharacterData / PlayerCharacterParameter readers:
//   FF2: Name | HP, MP | Strength, Spirit, Intellect, Stamina, Agility, Magic | Attack, Accuracy,
//        Defense, Evasion, Magic Defense, Magic Interference | 8 weapon skill levels.
//   FF3: FF1's list plus Job Level, Spirit, Magic Defense, Magic Evasion (no Luck).
//   FF4: Level, Handed, Experience, Next Level | HP, MP | 5 attributes with Spirit | Attack,
//        Accuracy, Defense, Evasion, Magic Defense, Magic Evasion.
//   FF5: Job, Character Level, Job Level, EXP, Next, ABP | HP, MP | Strength, Agility, Stamina,
//        Magic | Attack, Defense, Evasion, Magic Defense.
//   FF6: Name, Job, Level, Experience, Next Level | HP, MP | Strength, Agility, Stamina, Magic |
//        Attack, Defense, Evasion, Magic Defense, Magic Evasion | Commands, Magicite, At Level Up
//        (src/games/ff6/status_details.cpp).
// Those games also need their own status details hooks (status_menu.cpp is FF1's controller).
#include <vector>
#include "status.h"
#include "../core.h"
#include "../game.h"
#include "../log.h"
#include "../modtext.h"
#include "../say.h"
#include "../text.h"
#include "../ui_util.h"
#include "../ui/navigation_buffer.h"

namespace ffpr::status {
namespace {

// StatGroup
enum class StatGroup { CharacterInfo, Vitals, Attributes, CombatStats };

// StatusStatDefinition
struct StatDef {
    const wchar_t* name;
    StatGroup group;
    std::wstring (*reader)(void* data);
};

std::wstring na() { return T(L"N/A"); }

// --- Character Info readers -----------------------------------------------------------------

std::wstring read_name(void* data)
{
    if (!data) return na();
    std::wstring name = character_name(data);
    return !text::is_blank(name) ? TF(L"Name: {0}", {name}) : na();
}

std::wstring read_job_name(void* data)
{
    if (!data) return na();
    std::wstring job = current_job_name(data);
    if (!text::is_blank(job)) return TF(L"Job: {0}", {job});
    return TF(L"Job: ID {0}", {text::num(character_job_id(data))});
}

std::wstring read_character_level(void* data)
{
    void* p = character_parameter(data);
    if (!p) return na();
    return TF(L"Level: {0}", {text::num(param_confirmed_level(p))});
}

std::wstring read_experience(void* data)
{
    if (!data) return na();
    return TF(L"Experience: {0}", {text::num(character_current_exp(data))});
}

std::wstring read_next_level(void* data)
{
    if (!data) return na();
    return TF(L"Next Level: {0}", {text::num(character_next_exp(data))});
}

// --- Vitals readers -------------------------------------------------------------------------

std::wstring read_hp(void* data)
{
    void* p = character_parameter(data);
    if (!p) return na();
    return TF(L"HP: {0} / {1}", {text::num(param_current_hp(p)), text::num(param_max_hp(p))});
}

std::wstring read_mp_level_n(void* data, int level)
{
    void* p = character_parameter(data);
    if (!p) return na();
    return TF(L"LV{0}: {1}", {text::num(level), text::num(param_mp_count(p, level))});
}

std::wstring read_mp_level1(void* d) { return read_mp_level_n(d, 1); }
std::wstring read_mp_level2(void* d) { return read_mp_level_n(d, 2); }
std::wstring read_mp_level3(void* d) { return read_mp_level_n(d, 3); }
std::wstring read_mp_level4(void* d) { return read_mp_level_n(d, 4); }
std::wstring read_mp_level5(void* d) { return read_mp_level_n(d, 5); }
std::wstring read_mp_level6(void* d) { return read_mp_level_n(d, 6); }
std::wstring read_mp_level7(void* d) { return read_mp_level_n(d, 7); }
std::wstring read_mp_level8(void* d) { return read_mp_level_n(d, 8); }

// --- Attribute and combat readers -----------------------------------------------------------

std::wstring read_param(void* data, const wchar_t* key, int (*value)(void*))
{
    void* p = character_parameter(data);
    if (!p) return na();
    return TF(key, {text::num(value(p))});
}

std::wstring read_strength(void* d) { return read_param(d, L"Strength: {0}", &param_power); }
std::wstring read_agility(void* d) { return read_param(d, L"Agility: {0}", &param_agility); }
std::wstring read_stamina(void* d) { return read_param(d, L"Stamina: {0}", &param_vitality); }
std::wstring read_intellect(void* d) { return read_param(d, L"Intellect: {0}", &param_intelligence); }
std::wstring read_luck(void* d) { return read_param(d, L"Luck: {0}", &param_luck); }
std::wstring read_attack(void* d) { return read_param(d, L"Attack: {0}", &param_attack); }
std::wstring read_accuracy(void* d) { return read_param(d, L"Accuracy: {0}%", &param_accuracy_rate); }
std::wstring read_defense(void* d) { return read_param(d, L"Defense: {0}", &param_defense); }
std::wstring read_evasion(void* d) { return read_param(d, L"Evasion: {0}%", &param_evasion_rate); }

// FF1's stat list (StatusNavigationReader.InitializeStatList).
const StatDef kFF1Stats[] = {
    // Character Info (indices 0-4)
    {L"Name", StatGroup::CharacterInfo, &read_name},
    {L"Job", StatGroup::CharacterInfo, &read_job_name},
    {L"Level", StatGroup::CharacterInfo, &read_character_level},
    {L"Experience", StatGroup::CharacterInfo, &read_experience},
    {L"Next Level", StatGroup::CharacterInfo, &read_next_level},
    // Vitals (indices 5-13): HP and the 8 spell charge levels
    {L"HP", StatGroup::Vitals, &read_hp},
    {L"LV1", StatGroup::Vitals, &read_mp_level1},
    {L"LV2", StatGroup::Vitals, &read_mp_level2},
    {L"LV3", StatGroup::Vitals, &read_mp_level3},
    {L"LV4", StatGroup::Vitals, &read_mp_level4},
    {L"LV5", StatGroup::Vitals, &read_mp_level5},
    {L"LV6", StatGroup::Vitals, &read_mp_level6},
    {L"LV7", StatGroup::Vitals, &read_mp_level7},
    {L"LV8", StatGroup::Vitals, &read_mp_level8},
    // Attributes (indices 14-18)
    {L"Strength", StatGroup::Attributes, &read_strength},
    {L"Agility", StatGroup::Attributes, &read_agility},
    {L"Stamina", StatGroup::Attributes, &read_stamina},
    {L"Intellect", StatGroup::Attributes, &read_intellect},
    {L"Luck", StatGroup::Attributes, &read_luck},
    // Combat Stats (indices 19-22)
    {L"Attack", StatGroup::CombatStats, &read_attack},
    {L"Accuracy", StatGroup::CombatStats, &read_accuracy},
    {L"Defense", StatGroup::CombatStats, &read_defense},
    {L"Evasion", StatGroup::CombatStats, &read_evasion},
};
const int kFF1GroupStarts[] = {0, 5, 14, 19};

bool g_stat_list_built = false;
ui::NavigationBuffer g_buffer;

// BuildBuffer: each entry reads its stat LIVE from the tracker's current character. No group
// names, so a status group jump does not prefix the group name.
void build_buffer(const StatDef* defs, int def_count, const int* starts, int start_count)
{
    std::vector<ui::NavigationBuffer::Entry> entries;
    entries.reserve(static_cast<size_t>(def_count));
    for (int i = 0; i < def_count; ++i) {
        auto reader = defs[i].reader;
        entries.push_back([reader] { return reader(tracker().current_character_data); });
    }
    g_buffer = ui::NavigationBuffer(std::move(entries), std::vector<int>(starts, starts + start_count));
}

using Op = std::wstring (ui::NavigationBuffer::*)();

// Shared dispatch: validate, sync the tracker's index into the buffer, run the navigation op,
// sync back, and speak with the position within the stat's group.
void move(Op op)
{
    if (!g_stat_list_built) initialize_stat_list();

    Tracker& t = tracker();
    if (!t.is_navigation_active) return;
    if (!t.validate_state()) {
        ffpr::say(T(L"Navigation not available"));
        return;
    }
    if (!t.current_character_data) {
        ffpr::say(T(L"No character data"));
        return;
    }

    g_buffer.set_index(t.current_stat_index);
    std::wstring value;
    try {
        value = (g_buffer.*op)();
    } catch (...) {
        LOGF("Error reading stat at index %d", g_buffer.index());
        ffpr::say(T(L"Error reading stat"));
        return;
    }
    t.current_stat_index = g_buffer.index();

    if (!value.empty()) {
        auto [local_index, group_count] = g_buffer.current_group_position();
        ffpr::say(ui::menu_position(value, local_index, group_count), true);
    }
}

int install()
{
    actions::define("StatusNavigationReader.NavigateNext", [] { navigate_next(); });
    actions::define("StatusNavigationReader.NavigatePrevious", [] { navigate_previous(); });
    actions::define("StatusNavigationReader.JumpToNextGroup", [] { jump_to_next_group(); });
    actions::define("StatusNavigationReader.JumpToPreviousGroup", [] { jump_to_previous_group(); });
    actions::define("StatusNavigationReader.JumpToTop", [] { jump_to_top(); });
    actions::define("StatusNavigationReader.JumpToBottom", [] { jump_to_bottom(); });
    actions::define("StatusNavigationReader.ReadCurrentStat", [] { read_current_stat(); });

    queries::define("StatusNavigationTracker.IsNavigationActive", [] { return tracker().is_navigation_active; });
    queries::define("StatusNavigationTracker.ValidateState", [] { return tracker().validate_state(); });
    // StatusNavigationHandler.HandleInput's gate (IsNavigationActive && ValidateState()).
    queries::define("StatusNavigationTracker.IsActive", [] {
        const Tracker& t = tracker();
        return t.is_navigation_active && t.validate_state();
    });
    return 0;
}

core::Install s_install{"StatusNavigation", &install, 121};

} // namespace

void initialize_stat_list()
{
    if (g_stat_list_built) return;
    g_stat_list_built = true;
    if (game::is(game::Id::FF1)) {
        build_buffer(kFF1Stats, static_cast<int>(sizeof(kFF1Stats) / sizeof(kFF1Stats[0])), kFF1GroupStarts,
                     static_cast<int>(sizeof(kFF1GroupStarts) / sizeof(kFF1GroupStarts[0])));
    }
    // Other games: no stat list yet (see the file comment); the buffer stays empty.
}

void navigate_next() { move(&ui::NavigationBuffer::next); }
void navigate_previous() { move(&ui::NavigationBuffer::previous); }
void jump_to_next_group() { move(&ui::NavigationBuffer::next_group); }
void jump_to_previous_group() { move(&ui::NavigationBuffer::previous_group); }
void jump_to_top() { move(&ui::NavigationBuffer::jump_top); }
void jump_to_bottom() { move(&ui::NavigationBuffer::jump_bottom); }
void read_current_stat() { move(&ui::NavigationBuffer::current); }

} // namespace ffpr::status
