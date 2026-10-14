#include "../../core/displayed_ui.h"
// Ports FF4 Patches/PartySettingPatches.cs (the party setting screen: character list and party
// slot grid) and Patches/FormationRowPatches.cs (the row a character moves to when the status
// window's row switch is pressed).
#include <string>
#include <vector>
#include "ff4.h"
#include "../../core/core.h"
#include "../../core/dedup.h"
#include "../../core/hooks.h"
#include "../../core/modtext.h"
#include "../../core/say.h"
#include "../../core/text.h"
#include "../../core/ui_util.h"
#include "../../core/unity.h"
#include "../../core/system/cursor_event.h"

namespace ffpr::ff4 {
namespace {

ref::Class  kPartySetting{"Last.UI.Common", "PartySettingMenuBaseController", game::kFF4};
ref::Field  fPartyStateMachine{kPartySetting, "stateMachine"};
ref::Field  fPartyMembers{kPartySetting, "members"};
ref::Field  fPartySlotCount{kPartySetting, "slotCount"};
ref::Field  fSlot1Members{kPartySetting, "slot1Members"};
ref::Field  fSlot2Members{kPartySetting, "slot2Members"};
ref::Field  fSlot3Members{kPartySetting, "slot3Members"};
ref::Method mPartySelectContent{kPartySetting, "SelectContent", 1};
ref::Method mGetSlotCharacterId{kPartySetting, "GetSlotPostionCharaterId", 2};

ref::Class  kOwnedCharacter{"Last.Data.User", "OwnedCharacterData", game::kFF4};
ref::Method mCharacterId{kOwnedCharacter, "get_Id", 0};
ref::Class  kParamBase{"Last.Data", "CharacterParameterBase", game::kFF4};
ref::Method mConfirmedLevel{kParamBase, "ConfirmedLevel", 0};
ref::Method mGetCurrentHP{kParamBase, "get_CurrentHP", 0};
ref::Method mGetCurrentMP{kParamBase, "get_CurrentMP", 0};
ref::Method mConfirmedMaxHp{kParamBase, "ConfirmedMaxHp", 0};
ref::Method mConfirmedMaxMp{kParamBase, "ConfirmedMaxMp", 0};

// PartySettingMenuBaseController.State
enum PartyState {
    None = 0, MemberSelect = 1, MemberSelecting = 2, SlotSelect = 3, SlotSelecting = 4,
    SlotIndexSelect = 5, SlotIndexSelecting = 6, StatusView = 7,
};

// AnnouncementContexts.PARTY_SETTING_SELECT (DEDUP_INVENTORY.md FF4 rows 33-34): text.
constexpr const char* kCtxPartySelect = "PartySetting.Select";
// lastState (scattered tracker 18): never reset, as in the C#.
int g_last_state = None;

/// CheckAndAnnounceStateTransition: "Entering character list." / "Entering party slot grid."
void announce_state_transition(void* controller)
{
    int current = ui::state_tag(controller, fPartyStateMachine);
    if (current < 0 || current == g_last_state) return;
    std::wstring announcement;
    bool was_slot = g_last_state == SlotSelect || g_last_state == SlotSelecting;
    bool was_member = g_last_state == MemberSelect || g_last_state == MemberSelecting;
    if (was_slot && current == MemberSelect) announcement = T(L"Entering character list.");
    else if (was_member && current == SlotSelect) announcement = T(L"Entering party slot grid.");
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

/// BuildCharacterAnnouncement
std::wstring character_announcement(void* controller, void* character, int index)
{
    std::vector<std::wstring> parts;
    void* view = displayed::field(controller, "view");
    auto name = displayed::text_field(view, "currentCharacterNameText");
    if (name.empty()) return {};
    parts.push_back(name);
    auto job = displayed::text_field(view, "currentChracterJobText");
    auto level = displayed::pair(view, "fixedLevelText", "currentLevelText");
    auto vitals = displayed::vitals(view);
    for (auto value : {job, level, vitals}) if (!value.empty()) parts.push_back(std::move(value));
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
        if (m && ref::call<int32_t>(mCharacterId, m) == character_id) return character_name(m);
    }
    return {};
}

void announce_slot(void* controller, int index)
{
    int slot_count = ref::rd<int32_t>(controller, fPartySlotCount);
    constexpr int kPartyWidth = 2;   // each party occupies 2 columns
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
    hooks::guarded("FF4 PartySettingMenuBaseController.SelectContent", [&] {
        if (!self) return;
        announce_state_transition(self);
        set_active_exclusive(PARTY);
        int state = ui::state_tag(self, fPartyStateMachine);
        if (state == MemberSelect || state == MemberSelecting) announce_character(self, index);
        else if (state == SlotSelect || state == SlotSelecting || state == SlotIndexSelect || state == SlotIndexSelecting)
            announce_slot(self, index);
    });
}

// StatusWindowController_SwitchCorps_Patch (prefix): the row the character is about to move to.
int install()
{
    // MenuStates.Party registers with the registry (its reset is the flag only).
    menu_state::register_reset(PARTY, [] {});
    int n = 0;
    if (hooks::attach(mPartySelectContent, &d_PartySelectContent, &o_PartySelectContent)) ++n;
    queries::define("MenuStates.Party.ShouldSuppress", [] { return is_active(PARTY); });
    cursor::add_suppressor("MenuStates.Party.ShouldSuppress");
    return n;
}

core::Install s_install{"FF4.PartySetting", &install, 512, game::kFF4};

} // namespace

} // namespace ffpr::ff4
