// Ports FF6 Patches/PartySettingPatches.cs (PartySettingMenuBaseController.SelectContent: the
// party division screen, used when FF6 splits the characters into two or three parties - the
// character list and the party slot grid, with the section transitions). The commented-out
// StateMachine.Change patch of that file is not ported (it was disabled in the C#).
//
// Same screen as FF4's (src/games/ff4/party_setting.cpp), and the same mod-side behaviour: its
// wording, the character list's position, the "PartySetting.Select" check (Menu scope) and the
// never-reset section transition. FF6's State enum differs (FirstSlotSelect and
// FirstMemberSelect come first, plus OnlySlotSelect).
//
// The generic cursor reader stays silent on this screen's cursors (the FF6 C# skipped every
// cursor under a "party" object): "MenuStates.Party.ShouldSuppress" (FF4's query name) is true
// while the cursor being read is one of the live controller's own cursors. FF4 uses a "Party"
// menu_state flag instead, which nothing resets when the screen closes; the identity check gives
// the same speech and cannot stay stuck.
//
// SelectContent's body is not folded in FF6 (RVA 0x4697C0, 1 method).
#include <cstdint>
#include <string>
#include <vector>
#include "../../core/core.h"
#include "../../core/dedup.h"
#include "../../core/hooks.h"
#include "../../core/log.h"
#include "../../core/modtext.h"
#include "../../core/refs.h"
#include "../../core/say.h"
#include "../../core/text.h"
#include "../../core/ui_util.h"
#include "../../core/unity.h"
#include "../../core/status/status.h"
#include "../../core/system/cursor_event.h"

namespace ffpr::ff6 {
namespace {

constexpr uint8_t k6 = game::kFF6;

ref::Class  kPartySetting{"Last.UI.Common", "PartySettingMenuBaseController", k6};
ref::Field  fPartyStateMachine{kPartySetting, "stateMachine"};
ref::Field  fPartyMembers{kPartySetting, "members"};
ref::Field  fMembersCursor{kPartySetting, "membersCursor"};
ref::Field  fSlotsCursor{kPartySetting, "slotsCursor"};
ref::Field  fSelectingCursor{kPartySetting, "characterSelectingCursor"};
ref::Field  fPartySlotCount{kPartySetting, "slotCount"};
ref::Field  fSlot1Members{kPartySetting, "slot1Members"};
ref::Field  fSlot2Members{kPartySetting, "slot2Members"};
ref::Field  fSlot3Members{kPartySetting, "slot3Members"};
ref::Method mPartySelectContent{kPartySetting, "SelectContent", 1};
ref::Method mGetSlotCharacterId{kPartySetting, "GetSlotPostionCharaterId", 2};

ref::Class  kOwnedCharacter{"Last.Data.User", "OwnedCharacterData", k6};
ref::Method mCharacterId{kOwnedCharacter, "get_Id", 0};
ref::Class  kParamBase{"Last.Data", "CharacterParameterBase", k6};
ref::Field  fParamCurrentMp{kParamBase, "currentMP"};
ref::Class  kPlayerParam{"Last.Data", "PlayerCharacterParameter", k6};
ref::Method mConfirmedMaxMp{kPlayerParam, "ConfirmedMaxMp", 0};

// PartySettingMenuBaseController.State (FF6)
enum PartyState {
    None = 0, FirstSlotSelect = 1, FirstMemberSelect = 2, MemberSelect = 3, MemberSelecting = 4,
    SlotSelect = 5, SlotSelecting = 6, SlotIndexSelect = 7, SlotIndexSelecting = 8, OnlySlotSelect = 9,
};

// lastAnnouncedText (both sections share it): the C#'s existing text check, as FF4's port.
constexpr const char* kCtxPartySelect = "PartySetting.Select";
// lastState: never reset, as in the C#.
int g_last_state = None;
// The controller of the last SelectContent (identity for the cursor suppressor; re-checked
// with unity::alive before any read).
void* g_controller = nullptr;

bool is_member_state(int s) { return s == FirstMemberSelect || s == MemberSelect || s == MemberSelecting; }
bool is_slot_state(int s)
{
    return s == FirstSlotSelect || s == SlotSelect || s == SlotSelecting || s == SlotIndexSelect ||
           s == SlotIndexSelecting || s == OnlySlotSelect;
}

/// CheckAndAnnounceStateTransition: "Entering character list." / "Entering party slot grid."
void announce_state_transition(void* controller)
{
    int current = ui::state_tag(controller, fPartyStateMachine);
    if (current < 0 || current == g_last_state) return;
    std::wstring announcement;
    bool was_slot = g_last_state == FirstSlotSelect || g_last_state == SlotSelect || g_last_state == SlotSelecting;
    bool was_member = g_last_state == FirstMemberSelect || g_last_state == MemberSelect || g_last_state == MemberSelecting;
    if (was_slot && (current == FirstMemberSelect || current == MemberSelect))
        announcement = T(L"Entering character list.");
    else if (was_member && (current == FirstSlotSelect || current == SlotSelect))
        announcement = T(L"Entering party slot grid.");
    g_last_state = current;
    if (!announcement.empty()) ffpr::say(announcement, true);
}

bool list_has(void* int_list, int value)
{
    int n = unity::list_count(int_list);
    for (int i = 0; i < n; ++i)
        if (unity::list_value<int32_t>(int_list, i) == value) return true;
    return false;
}

/// GetPartyAssignment
std::wstring party_assignment(void* controller, void* character)
{
    int id = ref::call<int32_t>(mCharacterId, character);
    if (list_has(ref::rd<void*>(controller, fSlot1Members), id)) return TF(L"Party {0}", {L"1"});
    if (list_has(ref::rd<void*>(controller, fSlot2Members), id)) return TF(L"Party {0}", {L"2"});
    if (list_has(ref::rd<void*>(controller, fSlot3Members), id)) return TF(L"Party {0}", {L"3"});
    return T(L"not assigned");
}

/// BuildCharacterAnnouncement: name, level, HP, MP, party assignment (FF4's native wording).
std::wstring character_announcement(void* controller, void* character, int index)
{
    std::vector<std::wstring> parts;
    std::wstring name = status::character_name(character);
    parts.push_back(text::is_blank(name) ? TF(L"Character {0}", {text::num(index + 1)}) : name);
    if (void* p = status::character_parameter(character)) {
        parts.push_back(T(L"Level") + L" " + text::num(status::param_confirmed_level(p)));
        parts.push_back(T(L"HP") + L" " + text::num(status::param_current_hp(p)) + L"/" +
                        text::num(status::param_max_hp(p)));
        parts.push_back(T(L"MP") + L" " + text::num(ref::rd<int32_t>(p, fParamCurrentMp)) + L"/" +
                        text::num(ref::call<int32_t>(mConfirmedMaxMp, p)));
    }
    std::wstring assignment = party_assignment(controller, character);
    if (!text::is_blank(assignment)) parts.push_back(assignment);
    std::wstring out;
    for (size_t i = 0; i < parts.size(); ++i) {
        if (i) out += L", ";
        out += parts[i];
    }
    return out;
}

void announce_character(void* controller, int index)
{
    void* members = ref::rd<void*>(controller, fPartyMembers);
    void* character = ui::list_item(members, index);
    if (!character) return;
    std::wstring announcement = character_announcement(controller, character, index);
    if (text::is_blank(announcement)) return;
    if (!dedup::text(kCtxPartySelect, announcement)) return;
    ffpr::say(ui::menu_position(announcement, index, unity::list_count(members)), true);
}

/// GetCharacterName: the member with this id.
std::wstring member_name(void* controller, int character_id)
{
    void* members = ref::rd<void*>(controller, fPartyMembers);
    int n = unity::list_count(members);
    for (int i = 0; i < n; ++i) {
        void* m = unity::list_at(members, i);
        if (m && ref::call<int32_t>(mCharacterId, m) == character_id) return status::character_name(m);
    }
    return {};
}

/// AnnounceSlot: the grid is 2 columns per party, parties side by side, 2 rows.
void announce_slot(void* controller, int index)
{
    int slot_count = ref::rd<int32_t>(controller, fPartySlotCount);
    constexpr int kPartyWidth = 2;
    int total_width = slot_count * kPartyWidth;
    if (total_width <= 0) return;
    int row = index / total_width;
    int col = index % total_width;
    int party_number = col / kPartyWidth + 1;
    int position = row * kPartyWidth + col % kPartyWidth + 1;

    int character_id = ref::call<int32_t>(mGetSlotCharacterId, controller, slot_count, static_cast<int32_t>(index));
    std::wstring occupant;
    if (character_id == 0) {
        occupant = T(L"Empty");
    } else {
        occupant = member_name(controller, character_id);
        if (occupant.empty()) occupant = TF(L"Character {0}", {text::num(character_id)});
    }
    std::wstring announcement = TF(L"Party {0}, Position {1}: {2}", {text::num(party_number), text::num(position), occupant});
    if (!dedup::text(kCtxPartySelect, announcement)) return;
    ffpr::say(announcement, true);
}

using SelectIndex_t = void (*)(void* self, int32_t index, const void* mi);
SelectIndex_t o_PartySelectContent;
void d_PartySelectContent(void* self, int32_t index, const void* mi)
{
    o_PartySelectContent(self, index, mi);
    hooks::guarded("FF6.PartySettingMenuBaseController.SelectContent", [&] {
        if (!self) return;
        g_controller = self;
        announce_state_transition(self);
        int state = ui::state_tag(self, fPartyStateMachine);
        if (is_member_state(state)) announce_character(self, index);
        else if (is_slot_state(state)) announce_slot(self, index);
    });
}

/// The generic reader is handling one of the party screen's own cursors.
bool should_suppress()
{
    void* cur = cursor::current().cursor;
    if (!cur || !g_controller || !unity::alive(g_controller)) return false;
    return cur == ref::rd<void*>(g_controller, fMembersCursor) || cur == ref::rd<void*>(g_controller, fSlotsCursor) ||
           cur == ref::rd<void*>(g_controller, fSelectingCursor);
}

int install()
{
    int n = hooks::attach(mPartySelectContent, &d_PartySelectContent, &o_PartySelectContent) ? 1 : 0;
    queries::define("MenuStates.Party.ShouldSuppress", [] {
        return hooks::guarded_r("FF6.PartySetting.ShouldSuppress", false, [] { return should_suppress(); });
    });
    cursor::add_suppressor("MenuStates.Party.ShouldSuppress");
    return n;
}

core::Install s_install{"FF6.PartySetting", &install, 512, game::kFF6};

} // namespace
} // namespace ffpr::ff6
