// Ports FF1 Menus/ItemDetailsAnnouncer.cs, Menus/ItemDescriptionAnnouncer.cs and
// Menus/UsableByAnnouncer.cs: the on-demand detail keys of the item menu and shops (I key: the
// focused item's description; U key: which unlocked jobs can equip it). All five games (every
// name below exists in each dump.cs; JobGroup has Job1Accept..Job22Accept in all of them).
#include <algorithm>
#include <vector>
#include "menus_internal.h"
#include "../core.h"
#include "../log.h"
#include "../modtext.h"
#include "../say.h"
#include "../text.h"
#include "../unity.h"

namespace ffpr::menus {
namespace {

ref::Class  kWeapon{"Last.Data.Master", "Weapon"};
ref::Method mWeaponEquipJobGroupId{kWeapon, "get_EquipJobGroupId", 0};
ref::Class  kArmor{"Last.Data.Master", "Armor"};
ref::Method mArmorEquipJobGroupId{kArmor, "get_EquipJobGroupId", 0};

ref::Class  kJob{"Last.Data.Master", "Job"};
ref::Method mJobGetId{kJob, "get_Id", 0};
ref::Method mJobMesIdName{kJob, "get_MesIdName", 0};

ref::Class  kContent{"Last.Data.Master", "Content"};
ref::Method mContentMesIdName{kContent, "get_MesIdName", 0};
ref::Method mContentTypeId{kContent, "get_TypeId", 0};
ref::Method mContentTypeValue{kContent, "get_TypeValue", 0};

ref::Class  kUserDataManager{"Last.Management", "UserDataManager"};
ref::Method mUserDataManagerInstance{kUserDataManager, "Instance", 0};
ref::Field  fReleasedJobs{kUserDataManager, "<ReleasedJobs>k__BackingField"};

ref::Class  kItemWindowView{"Last.UI", "ItemWindowView"};
ref::Field  fItemWindowViewDescriptionText{kItemWindowView, "descriptionText"};

ref::Class  kJobGroup{"Last.Data.Master", "JobGroup"};
ref::Method mJob1Accept{kJobGroup, "get_Job1Accept", 0};
ref::Method mJob2Accept{kJobGroup, "get_Job2Accept", 0};
ref::Method mJob3Accept{kJobGroup, "get_Job3Accept", 0};
ref::Method mJob4Accept{kJobGroup, "get_Job4Accept", 0};
ref::Method mJob5Accept{kJobGroup, "get_Job5Accept", 0};
ref::Method mJob6Accept{kJobGroup, "get_Job6Accept", 0};
ref::Method mJob7Accept{kJobGroup, "get_Job7Accept", 0};
ref::Method mJob8Accept{kJobGroup, "get_Job8Accept", 0};
ref::Method mJob9Accept{kJobGroup, "get_Job9Accept", 0};
ref::Method mJob10Accept{kJobGroup, "get_Job10Accept", 0};
ref::Method mJob11Accept{kJobGroup, "get_Job11Accept", 0};
ref::Method mJob12Accept{kJobGroup, "get_Job12Accept", 0};
ref::Method mJob13Accept{kJobGroup, "get_Job13Accept", 0};
ref::Method mJob14Accept{kJobGroup, "get_Job14Accept", 0};
ref::Method mJob15Accept{kJobGroup, "get_Job15Accept", 0};
ref::Method mJob16Accept{kJobGroup, "get_Job16Accept", 0};
ref::Method mJob17Accept{kJobGroup, "get_Job17Accept", 0};
ref::Method mJob18Accept{kJobGroup, "get_Job18Accept", 0};
ref::Method mJob19Accept{kJobGroup, "get_Job19Accept", 0};
ref::Method mJob20Accept{kJobGroup, "get_Job20Accept", 0};
ref::Method mJob21Accept{kJobGroup, "get_Job21Accept", 0};
ref::Method mJob22Accept{kJobGroup, "get_Job22Accept", 0};

// JobGroup.Job1Accept .. Job22Accept (FF1 only uses 1-12); job ids are 1-based.
ref::Method* const kJobAccept[] = {
    &mJob1Accept, &mJob2Accept, &mJob3Accept, &mJob4Accept, &mJob5Accept, &mJob6Accept,
    &mJob7Accept, &mJob8Accept, &mJob9Accept, &mJob10Accept, &mJob11Accept, &mJob12Accept,
    &mJob13Accept, &mJob14Accept, &mJob15Accept, &mJob16Accept, &mJob17Accept, &mJob18Accept,
    &mJob19Accept, &mJob20Accept, &mJob21Accept, &mJob22Accept,
};

// ContentType (Last.Defaine.Content; identical in all five games).
constexpr int CONTENT_TYPE_WEAPON = 2;
constexpr int CONTENT_TYPE_ARMOR = 3;

/// ItemDetailsAnnouncer.GetEquipJobGroupId: from the weapon or armor master data.
int equip_job_group_id(int item_type, int item_id)
{
    if (item_type == CONTENT_TYPE_WEAPON) {
        if (void* weapon = detail::master_data(kWeapon, item_id))
            return ref::call<int32_t>(mWeaponEquipJobGroupId, weapon);
    } else if (item_type == CONTENT_TYPE_ARMOR) {
        if (void* armor = detail::master_data(kArmor, item_id))
            return ref::call<int32_t>(mArmorEquipJobGroupId, armor);
    }
    return 0;
}

/// ItemDetailsAnnouncer.GetUnlockedJobIds: the jobs the player has released (FF1 starts with
/// 6 base jobs; class change unlocks the upgraded ones).
std::vector<int> unlocked_job_ids()
{
    std::vector<int> ids;
    void* udm = ref::call_static<void*>(mUserDataManagerInstance);
    if (!udm) {
        LOGF("[ItemDetails] UserDataManager not available");
        return ids;
    }
    void* released = ref::rd<void*>(udm, fReleasedJobs);
    if (!released) {
        LOGF("[ItemDetails] ReleasedJobs is null");
        return ids;
    }
    int n = unity::list_count(released);
    for (int i = 0; i < n; ++i)
        if (void* job = unity::list_at(released, i)) ids.push_back(ref::call<int32_t>(mJobGetId, job));
    return ids;
}

/// ItemDetailsAnnouncer.GetJobName: the localized job name.
std::wstring job_name(void* jobs, void* mm, int job_id)
{
    void* job = detail::master_find(jobs, job_id);
    if (!job) return {};
    std::wstring mes_id = ref::call_str(mJobMesIdName, job);
    if (mes_id.empty()) return {};
    std::wstring localized = detail::message_from(mm, mes_id, false);
    return detail::blank(localized) ? std::wstring() : localized;
}

/// ItemDetailsAnnouncer.GetEquippableJobs: names of the jobs whose accept flag is set AND that
/// are unlocked (no spoilers).
std::vector<std::wstring> equippable_jobs(void* job_group, const std::vector<int>& unlocked)
{
    std::vector<std::wstring> names;
    void* mm = detail::message_manager();
    void* jobs = detail::master_list(kJob);
    for (int i = 0; i < static_cast<int>(sizeof(kJobAccept) / sizeof(kJobAccept[0])); ++i) {
        int job_id = i + 1;
        int accept = ref::call<int32_t>(*kJobAccept[i], job_group);
        if (accept == 0 || std::find(unlocked.begin(), unlocked.end(), job_id) == unlocked.end()) continue;
        std::wstring name = job_name(jobs, mm, job_id);
        if (!name.empty()) names.push_back(std::move(name));
    }
    return names;
}

/// ItemDetailsAnnouncer.BuildAnnouncement.
std::wstring build_announcement(const std::vector<std::wstring>& job_names)
{
    if (job_names.empty()) return T(L"No unlocked jobs can equip");
    return T(L"Can equip: ") + detail::join_all(job_names, L", ");
}

/// "Can equip: ..." for a weapon/armor, or empty when it is not equipment or has no job group.
std::wstring equip_requirements(int item_type, int item_id)
{
    if (item_type != CONTENT_TYPE_WEAPON && item_type != CONTENT_TYPE_ARMOR) return {};
    int group_id = equip_job_group_id(item_type, item_id);
    if (group_id <= 0) return {};
    void* job_group = detail::master_data(kJobGroup, group_id);
    if (!job_group) return {};
    return build_announcement(equippable_jobs(job_group, unlocked_job_ids()));
}

/// ItemDetailsAnnouncer.AnnounceEquipRequirements: which jobs can equip the selected item.
/// Weapons and armor only; silent for other items.
void announce_equip_requirements()
{
    int item_type = 0, item_id = 0;
    if (!detail::item_last_selected(item_type, item_id)) return;
    if (item_type != CONTENT_TYPE_WEAPON && item_type != CONTENT_TYPE_ARMOR) return;
    if (!detail::master_manager()) {
        LOGF("[ItemDetails] MasterManager not available");
        return;
    }
    std::wstring announcement = equip_requirements(item_type, item_id);
    if (!announcement.empty()) ffpr::say(announcement, true);
}

/// ItemDescriptionAnnouncer.AnnounceCurrentDescription: the live item panel's description
/// (ItemWindowView.descriptionText), exactly what the game rendered.
void announce_current_description()
{
    std::wstring description;
    if (void* view = unity::find_object(kItemWindowView))
        if (void* text_component = ref::rd<void*>(view, fItemWindowViewDescriptionText))
            description = unity::ui_text(text_component);
    if (detail::blank(description)) {
        ffpr::say(T(L"No description"), true);
        return;
    }
    ffpr::say(text::trim(detail::strip_icon(description)), true);
}

/// UsableByAnnouncer.TryResolveContent: the Content master entry whose localized name is the
/// shop's display name (TypeId = ContentType, TypeValue = id in the Weapon/Armor master).
/// Matching by name bypasses the unreliable ShopListItemContentController.ContentId.
bool try_resolve_content(const std::wstring& item_name, int& item_type, int& item_id)
{
    item_type = -1;
    item_id = 0;
    void* mm = detail::message_manager();
    if (!mm) return false;
    std::wstring target = detail::strip_icon(item_name);
    if (target.empty()) return false;

    void* contents = detail::master_list(kContent);
    if (!contents) return false;
    for (void* content : detail::master_values(contents)) {
        if (!content) continue;
        std::wstring mes_id = ref::call_str(mContentMesIdName, content);
        if (mes_id.empty()) continue;
        std::wstring localized = detail::message_from(mm, mes_id, false);
        if (localized.empty()) continue;
        if (detail::strip_icon(localized) == target) {
            item_type = ref::call<int32_t>(mContentTypeId, content);
            item_id = ref::call<int32_t>(mContentTypeValue, content);
            return true;
        }
    }
    return false;
}

/// UsableByAnnouncer.BuildShopItemAnnouncement: "Can equip: ..." for the focused shop item, or
/// empty when it isn't equipment or can't be resolved by name.
std::wstring build_shop_item_announcement()
{
    std::wstring item_name = detail::shop_last_item_name();
    if (item_name.empty()) return {};
    if (!detail::master_manager()) return {};
    int item_type = -1, item_id = 0;
    if (!try_resolve_content(item_name, item_type, item_id)) return {};
    return equip_requirements(item_type, item_id);
}

/// UsableByAnnouncer.AnnounceForCurrentContext (U key / right stick left): shops resolve the
/// item by name; the item menu uses the selected row.
void announce_for_current_context()
{
    if (detail::shop_validate_state()) {
        std::wstring announcement = build_shop_item_announcement();
        if (!announcement.empty()) ffpr::say(announcement, true);
        return;
    }
    if (detail::item_menu_active()) {
        announce_equip_requirements();
        return;
    }
}

int install()
{
    actions::define("ItemDetailsAnnouncer.AnnounceEquipRequirements", [] { announce_equip_requirements(); });
    actions::define("ItemDescriptionAnnouncer.AnnounceCurrentDescription", [] { announce_current_description(); });
    actions::define("UsableByAnnouncer.AnnounceForCurrentContext", [] { announce_for_current_context(); });
    queries::define_text("UsableByAnnouncer.BuildShopItemAnnouncement", [] { return build_shop_item_announcement(); });
    return 0;
}

core::Install s_install{"Menus.ItemDetails", &install, 120};

} // namespace
} // namespace ffpr::menus
