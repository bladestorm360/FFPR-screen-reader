#include "battle.h"
#include "battle_util.h"
#include "../core.h"
#include "../displayed_ui.h"
#include "../hooks.h"
#include "../modtext.h"
#include "../say.h"
#include "../text.h"
#include "../unity.h"
#include "../input/input.h"
#include "../field/object_cache.h"

namespace ffpr::battle {
namespace {
ref::Class kParty{"Last.UI.KeyInput", "BattlePlayerInfomationController"};
ref::Field fContents{kParty, "contentList"};
ref::Field fTargets{kParty, "targetSelectContentList"};
ref::Class kCommands{"Last.UI.KeyInput", "BattleCommandSelectController"};
ref::Field fActor{kCommands, "targetData"};
void* g_party = nullptr;
ref::Method mInitialize{kParty, "Initalize", 0};
ref::Method mUpdate{kParty, "UpdateView", 2}, mFocus{kParty, "SetFocus", 2};
void* g_focused_player = nullptr;
using Update = void (*)(void*, void*, bool, const void*);
Update o_update;
void d_update(void* self, void* players, bool target, const void* mi) {
    o_update(self, players, target, mi);
    hooks::guarded("Battle.PartyViewOwner", [self] {
        if (kParty.is(self)) { g_party = self; field::cache::register_object(kParty, self); }
    });
}
Update o_focus;
void d_focus(void* self, void* player, bool focused, const void* mi) {
    o_focus(self, player, focused, mi);
    hooks::guarded("Battle.PartyFocus", [=] {
        if (!kParty.is(self)) return;
        g_party = self; field::cache::register_object(kParty, self);
        if (focused) g_focused_player = player;
        else if (g_focused_player == player) g_focused_player = nullptr;
    });
}
using Initialize = void (*)(void*, const void*);
Initialize o_initialize;
void d_initialize(void* self, const void* mi) {
    o_initialize(self, mi);
    hooks::guarded("Battle.PartyOwner", [self] {
        if (kParty.is(self)) field::cache::register_object(kParty, self);
    });
}
int g_review_slot = 0;
void* g_execution_player = nullptr;
std::wstring g_execution_name;

std::vector<void*> rows()
{
    std::vector<void*> out;
    if (!in_battle()) return out;
    if (!unity::alive(g_party)) g_party = field::cache::get(kParty);
    if (!unity::alive(g_party)) return out;
    // The game swaps its ordinary and target-select panels. Read whichever it displays.
    // Controllers and their serialized views need not share a GameObject. The
    // rendered row owns visibility; a hidden controller must not veto its panel.
    for (void* list : {ref::rd<void*>(g_party, fTargets), ref::rd<void*>(g_party, fContents)}) {
        const int count = unity::list_count(list);
        if (count <= 0 || count > 9) continue;
        std::vector<void*> slots(static_cast<size_t>(count));
        bool any = false;
        for (int i = 0; i < count; ++i) {
            void* row = unity::list_at(list, i);
            void* view = displayed::field(row, "view");
            if (displayed::container_visible(view) && !displayed::text_field(view, "nameText").empty()) { slots[i] = row; any = true; }
        }
        if (any) { out = std::move(slots); break; }
    }
    return out;
}

std::wstring read_row(void* row)
{
    auto line = displayed::character(displayed::field(row, "view"));
    void* conditions = displayed::field(row, "condtionController");
    auto details = displayed::texts(displayed::field(conditions, "view"));
    if (!details.empty()) line += L", " + text::join(details, L", ");
    auto icons = displayed::conditions(conditions);
    if (!icons.empty()) line += L", " + icons;
    return line;
}

void* active_row(const std::vector<void*>& content)
{
    for (void* row : content)
        if (g_execution_player && displayed::field(row, "<TargetData>k__BackingField") == g_execution_player) return row;
    // Read rooted native command data afresh; opening Magic/Items must not clear its owner.
    void* command = field::cache::get(kCommands);
    const bool choosing = menu_state::is_active(menu_state::BATTLE_COMMAND) || menu_state::is_active(menu_state::BATTLE_ITEM) ||
                          menu_state::is_active(menu_state::BATTLE_MAGIC) || menu_state::is_active(menu_state::BATTLE_TARGET);
    if (!choosing && !g_execution_name.empty() && !g_execution_player) return nullptr;
    void* actor = choosing && unity::alive(command) ? ref::rd<void*>(command, fActor) : nullptr;
    if (actor) {
        const int id = character_id(actor);
        for (void* row : content) {
            void* player = displayed::field(row, "<TargetData>k__BackingField");
            void* owned = player_owned_character(player);
            if (owned && (owned == actor || character_id(owned) == id)) return row;
        }
    }
    // The command panel may be inactive while its Magic/Item child owns input.
    // Match the preserved actor by identity only; never dereference an unrooted stale pointer.
    for (void* row : content) {
        void* owned = player_owned_character(displayed::field(row, "<TargetData>k__BackingField"));
        if (choosing && owned && owned == current_actor()) return row;
    }
    for (void* row : content)
        if (g_focused_player && displayed::field(row, "<TargetData>k__BackingField") == g_focused_player) return row;
    for (void* row : content)
        if (displayed::visible(displayed::field(displayed::field(row, "view"), "focusImage"))) return row;
    return nullptr;
}

void announce_slot(int slot)
{
    const auto content = rows();
    if (slot < 1 || slot > static_cast<int>(content.size()) || !content[slot - 1]) return;
    ffpr::say(read_row(content[slot - 1]), true);
}

void cycle_party(int delta)
{
    const auto content = rows();
    if (content.empty()) return;
    const int count = static_cast<int>(content.size());
    for (int i = 0; i < count; ++i) {
        g_review_slot = g_review_slot < 0 ? (delta < 0 ? count - 1 : 0) : (g_review_slot + delta + count) % count;
        if (content[g_review_slot]) break;
    }
    ffpr::say(read_row(content[g_review_slot]), true);
}

int install()
{
    for (int i = 0; i < 9; ++i) {
        const auto key = static_cast<input::Key>(static_cast<int>(input::Key::Alpha1) + i);
        const std::string action = "BattleParty.Slot" + std::to_string(i + 1);
        actions::define(action.c_str(), [i] { announce_slot(i + 1); });
        input::add_binding(key, input::Mod::None, input::Context::Battle,
                          action.c_str(), "Battle: displayed party member statistics");
    }
    actions::define("BattleParty.ActiveTurn", [] { announce_active_turn(); });
    input::add_binding(input::Key::T, input::Mod::None, input::Context::Battle,
                       "BattleParty.ActiveTurn", "Battle: announce active turn");
    actions::define("BattleParty.Next", [] { cycle_party(1); });
    actions::define("BattleParty.Previous", [] { cycle_party(-1); });
    events::on(events::Kind::BattleStart, [] { g_party = g_focused_player = nullptr; g_review_slot = -1; action_actor_finished(); });
    events::on(events::Kind::BattleEnd, [] { g_party = g_focused_player = nullptr; g_review_slot = -1; action_actor_finished(); });
    return (hooks::attach(mInitialize, &d_initialize, &o_initialize) ? 1 : 0) +
        (hooks::attach(mUpdate, &d_update, &o_update) ? 1 : 0) + (hooks::attach(mFocus, &d_focus, &o_focus) ? 1 : 0);
}
core::Install s_install{"Battle.Party", &install, 110};
}

std::wstring displayed_player(void* player)
{
    if (!player) return {};
    void* owned = player_owned_character(player);
    const int id = owned ? character_id(owned) : 0;
    for (void* row : rows()) {
        void* data = displayed::field(row, "<TargetData>k__BackingField");
        if (!data) continue;
        if (data == player) return read_row(row);
        // Target enumerables can carry another BattlePlayerData for the same
        // character. Identity selects the rendered row, never hidden statistics.
        void* row_owned = player_owned_character(data);
        if (owned && row_owned && (owned == row_owned || (id > 0 && id == character_id(row_owned)))) return read_row(row);
    }
    return {}; // A parameter object alone is not renderer evidence.
}

void observe_target_party(void* owner)
{
    if (unity::alive(owner) && kParty.is(owner)) g_party = owner;
}

void announce_active_status()
{
    if (!in_battle()) { ffpr::say(T(L"Party status only available in battle"), true); return; }
    void* row = active_row(rows());
    ffpr::say(row ? read_row(row) : T(L"No active character"), true);
}

void announce_active_turn()
{
    if (!in_battle()) return;
    if (!g_execution_name.empty()) { ffpr::say(TF(L"{0}'s turn", {g_execution_name}), true); return; }
    void* row = active_row(rows());
    const auto name = row ? displayed::text_field(displayed::field(row, "view"), "nameText") : std::wstring();
    ffpr::say(name.empty() ? T(L"No active character") : TF(L"{0}'s turn", {name}), true);
}
void action_actor_started(void* unit)
{
    g_execution_name = unit_name(unit);
    g_execution_player = kBattlePlayerData.is(unit) ? unit : nullptr;
}
void action_actor_finished() { g_execution_player = nullptr; g_execution_name.clear(); }
}
