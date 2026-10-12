// Ports FF6 Menus/ItemEquipAnnouncer.cs (FF6ScreenReader, by ZKline): which party members can
// equip the selected weapon or armor - "Can equip: Terra, Celes" / "No characters can equip".
//
// FF6 has no job system the player sees: each character has its own job (Job master
// "Magitek Elite", "Adventurer" ...), and the core's U key (FF1 UsableByAnnouncer, which lists
// the player's released jobs) has nothing to say in FF6. Like FF2's game_info.cpp, this file
// replaces "UsableByAnnouncer.AnnounceForCurrentContext" for FF6 and keeps the core's contexts
// and wording rules:
//   * shop: the focused item resolved by its name, as the core does (TryResolveContent);
//   * item menu: the selected row (the C#'s ItemMenuTracker.LastSelectedItem, which the core's
//     item menu keeps), only while the item list is shown (the C#'s ItemListController check).
// The C# bound this to the I key; in the unified mod I reads the description and U the equip
// check, in every game.
//
// The C# asked EquipUtility.CanEquipped(SearchOwnedItem(contentId), character.JobId). The core
// keeps the row's ItemType/ItemId, not its contentId, and a shop item need not be owned, so both
// contexts read the same master data the core's U key reads (Weapon/Armor.EquipJobGroupId ->
// JobGroup) and ask JobGroup.GetJobAccept(group, character.JobId) for each party member
// (UserDataManager.GetOwnedCharactersClone(false), as in the C#).
//
// Uses the menus area's own helpers (src/core/menus/menus_internal.h): the selected row, the
// master-data lookups, the message table and the shop tracker.
#include <cstdint>
#include <string>
#include <vector>
#include "../../core/core.h"
#include "../../core/log.h"
#include "../../core/modtext.h"
#include "../../core/refs.h"
#include "../../core/say.h"
#include "../../core/text.h"
#include "../../core/unity.h"
#include "../../core/field/object_cache.h"
#include "../../core/displayed_ui.h"
#include "../../core/menus/menus_internal.h"

namespace ffpr::ff6 {
namespace {

namespace md = ffpr::menus::detail;

constexpr uint8_t k6 = game::kFF6;

ref::Class kItemWindowOwner{"Last.UI.KeyInput", "ItemWindowController", k6};
ref::Class  kItemListController{"Last.UI.KeyInput", "ItemListController", k6};

ref::Class  kWeapon{"Last.Data.Master", "Weapon", k6};
ref::Method mWeaponEquipJobGroupId{kWeapon, "get_EquipJobGroupId", 0};
ref::Class  kArmor{"Last.Data.Master", "Armor", k6};
ref::Method mArmorEquipJobGroupId{kArmor, "get_EquipJobGroupId", 0};
ref::Class  kJobGroup{"Last.Data.Master", "JobGroup", k6};
ref::Method mGetJobAccept{kJobGroup, "GetJobAccept", 2};

ref::Class  kContent{"Last.Data.Master", "Content", k6};
ref::Method mContentMesIdName{kContent, "get_MesIdName", 0};
ref::Method mContentTypeId{kContent, "get_TypeId", 0};
ref::Method mContentTypeValue{kContent, "get_TypeValue", 0};

ref::Class  kUserDataManager{"Last.Management", "UserDataManager", k6};
ref::Method mUserDataManagerInstance{kUserDataManager, "Instance", 0};
ref::Method mGetOwnedCharactersClone{kUserDataManager, "GetOwnedCharactersClone", 1};
ref::Class  kOwnedCharacterData{"Last.Data.User", "OwnedCharacterData", k6};
ref::Method mCharacterGetJobId{kOwnedCharacterData, "get_JobId", 0};

// ContentType (Last.Defaine.Content; the same values as FF1).
constexpr int CONTENT_TYPE_WEAPON = 2;
constexpr int CONTENT_TYPE_ARMOR = 3;

/// The JobGroup master entry of a weapon or armor; null for other items or no group.
void* equip_job_group(int item_type, int item_id)
{
    int group_id = 0;
    if (item_type == CONTENT_TYPE_WEAPON) {
        if (void* weapon = md::master_data(kWeapon, item_id)) group_id = ref::call<int32_t>(mWeaponEquipJobGroupId, weapon);
    } else if (item_type == CONTENT_TYPE_ARMOR) {
        if (void* armor = md::master_data(kArmor, item_id)) group_id = ref::call<int32_t>(mArmorEquipJobGroupId, armor);
    } else {
        return nullptr;
    }
    return group_id > 0 ? md::master_data(kJobGroup, group_id) : nullptr;
}

/// The C# loop over GetOwnedCharactersClone(false): "Can equip: ..." / "No characters can equip",
/// or empty when there is no party to ask (the C# returned unhandled).
std::wstring build_announcement(void* job_group)
{
    void* udm = ref::call_static<void*>(mUserDataManagerInstance);
    if (!udm) return {};
    void* characters = ref::call<void*>(mGetOwnedCharactersClone, udm, false);
    int n = unity::list_count(characters);
    if (n <= 0) return {};

    std::vector<std::wstring> names;
    for (int i = 0; i < n; ++i) {
        void* character = unity::list_at(characters, i);
        if (!character) continue;
        int job_id = ref::call<int32_t>(mCharacterGetJobId, character);
        if (ref::call_static<int32_t>(mGetJobAccept, job_group, static_cast<int32_t>(job_id)) == 0) continue;
        std::wstring name = md::strip_icon(md::character_name(character));
        if (!name.empty()) names.push_back(std::move(name));
    }
    if (names.empty()) return T(L"No characters can equip");
    return TF(L"Can equip: {0}", {md::join_all(names, L", ")});
}

/// ItemEquipAnnouncer.TryAnnounceEquipRequirements (item menu): true when it spoke.
bool try_announce_equip_requirements()
{
    int item_type = 0, item_id = 0;
    if (!md::item_last_selected(item_type, item_id)) return false;
    if (item_type != CONTENT_TYPE_WEAPON && item_type != CONTENT_TYPE_ARMOR) return false;

    // Only while the item list itself is shown.
    void* list_controller = displayed::field(field::cache::get(kItemWindowOwner), "itemListController");
    if (!list_controller || !unity::active_in_hierarchy(list_controller)) return false;

    void* job_group = equip_job_group(item_type, item_id);
    if (!job_group) return false;
    std::wstring announcement = build_announcement(job_group);
    if (announcement.empty()) return false;
    ffpr::say(announcement, true);
    return true;
}

/// UsableByAnnouncer.TryResolveContent (the core's, for FF6): the Content master entry whose
/// localized name is the shop's display name.
bool try_resolve_content(const std::wstring& item_name, int& item_type, int& item_id)
{
    item_type = -1;
    item_id = 0;
    void* mm = md::message_manager();
    if (!mm) return false;
    std::wstring target = md::strip_icon(item_name);
    if (target.empty()) return false;

    void* contents = md::master_list(kContent);
    if (!contents) return false;
    for (void* content : md::master_values(contents)) {
        if (!content) continue;
        std::wstring mes_id = ref::call_str(mContentMesIdName, content);
        if (mes_id.empty()) continue;
        std::wstring localized = md::message_from(mm, mes_id, false);
        if (localized.empty()) continue;
        if (md::strip_icon(localized) == target) {
            item_type = ref::call<int32_t>(mContentTypeId, content);
            item_id = ref::call<int32_t>(mContentTypeValue, content);
            return true;
        }
    }
    return false;
}

/// UsableByAnnouncer.BuildShopItemAnnouncement for FF6: the party members for the focused shop
/// item, or empty when it isn't equipment or can't be resolved by name.
std::wstring build_shop_item_announcement()
{
    std::wstring item_name = md::shop_last_item_name();
    if (item_name.empty()) return {};
    if (!md::master_manager()) return {};
    int item_type = -1, item_id = 0;
    if (!try_resolve_content(item_name, item_type, item_id)) return {};
    void* job_group = equip_job_group(item_type, item_id);
    return job_group ? build_announcement(job_group) : std::wstring();
}

/// UsableByAnnouncer.AnnounceForCurrentContext for FF6 (U key / right stick left).
void announce_for_current_context()
{
    if (md::shop_validate_state()) {
        std::wstring announcement = build_shop_item_announcement();
        if (!announcement.empty()) ffpr::say(announcement, true);
        return;
    }
    if (md::item_menu_active()) try_announce_equip_requirements();
}

int install()
{
    actions::define("ItemEquipAnnouncer.TryAnnounceEquipRequirements", [] { try_announce_equip_requirements(); });
    // Replace the core's job-based versions (src/core/menus/item_details.cpp, order 120).
    actions::define("UsableByAnnouncer.AnnounceForCurrentContext", [] { announce_for_current_context(); });
    queries::define_text("UsableByAnnouncer.BuildShopItemAnnouncement", [] { return build_shop_item_announcement(); });
    return 0;
}

core::Install s_install{"FF6.ItemEquip", &install, 540, game::kFF6};

} // namespace
} // namespace ffpr::ff6
