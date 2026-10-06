// Ports FF3 Patches/JobMenuPatches.cs (JobMenuState, JobMenuPatches) and the job parts of
// Utils/LocalizationHelper.cs.
//
// The job change screen (Serial.FF3.UI.KeyInput.JobChangeWindowController): "Job, Job Level N",
// plus ". Equipped." for the character's current job, from UpdateJobInfo (the game's info panel
// update, which carries the character and the focused job). This is also FF3's Phase 1 proof
// hook (UNIFIED_MOD_PLAN.md): the install logs each JobChangeWindowController hook by name.
//
// The I key reads the focused job's description. The core's I cascade asks
// "JobSelectionPatches.IsActive" / "JobSelectionPatches.GetCurrentDescription" (FF1's class
// choice) right after the config tooltip - exactly where FF3's cascade put the job menu - so FF3
// publishes its job menu under those two names. "JobSelectionPatches.IsHandlingCursor" is NOT
// defined: the job menu keeps the generic reader quiet through a cursor suppressor instead
// (FF3 CursorSuppressionCheck: JobMenuState.ShouldSuppress = IsActive).
//
// Parity notes:
//  * SelectContent's postfix only fed the write-only "JobMenu.Index" context (DEDUP_INVENTORY
//    FF3 #28: never gates anything); not hooked.
//  * The level is read from the job view's info text first, as the C# did (it found the view
//    with FindObjectOfType; the controller's own `view` field is the same object).
//  * Dedup "JobMenu.Select" compares the spoken text, as the C# did (FF3 #27).
#include <cwchar>
#include <string>
#include "../../core/core.h"
#include "../../core/dedup.h"
#include "../../core/hooks.h"
#include "../../core/log.h"
#include "../../core/modtext.h"
#include "../../core/refs.h"
#include "../../core/say.h"
#include "../../core/text.h"
#include "../../core/unity.h"
#include "../../core/status/status.h"
#include "../../core/system/cursor_event.h"

namespace ffpr::ff3 {
namespace {

ref::Class kJobWindow{"Serial.FF3.UI.KeyInput", "JobChangeWindowController", game::kFF3};
ref::Field fJobWindowView{kJobWindow, "view"};
ref::Method mUpdateJobInfo{kJobWindow, "UpdateJobInfo", 2};
ref::Method mJobWindowSetActive{kJobWindow, "SetActive", 1};

ref::Class kJobView{"Serial.FF3.UI.KeyInput", "JobChangeWindowView", game::kFF3};
ref::Field fInfoSkillLevelText{kJobView, "infoSkillLevelValueText"};
ref::Field fSkillLevelText{kJobView, "skillLevelValueText"};

ref::Class kJob{"Last.Data.Master", "Job", game::kFF3};
ref::Method mJobId{kJob, "get_Id", 0};
ref::Method mJobMesIdName{kJob, "get_MesIdName", 0};
ref::Method mJobMesIdDescription{kJob, "get_MesIdDescription", 0};

ref::Class kOwnedCharacter{"Last.Data.User", "OwnedCharacterData", game::kFF3};
ref::Method mCharJobId{kOwnedCharacter, "get_JobId", 0};
ref::Method mCharJobDataList{kOwnedCharacter, "get_OwnedJobDataList", 0};

ref::Class kOwnedJob{"Last.Data.User", "OwnedJobData", game::kFF3};
ref::Method mOwnedJobId{kOwnedJob, "get_Id", 0};
ref::Method mOwnedJobLevel{kOwnedJob, "get_Level", 0};

constexpr const char* kJobMenuKey = "JobMenu";        // MenuStateRegistry.JOB_MENU
constexpr const char* kCtxSelect = "JobMenu.Select";

// JobMenuState.FocusedJob: kept only as its description text (no game pointer is kept).
std::wstring g_focused_description;
bool g_has_focused_job = false;

/// LocalizationHelper.GetText: MessageManager.GetMessage(mesId, false), markup stripped; empty
/// when the id or the text is blank.
std::wstring localized(const std::wstring& mes_id)
{
    if (mes_id.empty()) return {};
    std::wstring s = ffpr::status::game_message(mes_id, false);
    if (text::is_blank(s)) return {};
    return text::strip_markup(s);
}

/// C# int.TryParse on a UI Text: optional surrounding white space, optional sign, ASCII digits.
bool parse_int(const std::wstring& s, int& out)
{
    const std::wstring t = text::trim(s);
    if (t.empty()) return false;
    size_t i = (t[0] == L'-' || t[0] == L'+') ? 1 : 0;
    if (i >= t.size()) return false;
    for (size_t k = i; k < t.size(); ++k)
        if (t[k] < L'0' || t[k] > L'9') return false;
    out = static_cast<int>(std::wcstol(t.c_str(), nullptr, 10));
    return true;
}

/// JobMenuState.TryReadJobLevelFromUI: the info panel's level text, then the list's; -1 if neither.
int job_level_from_ui(void* window)
{
    void* view = ref::rd<void*>(window, fJobWindowView);
    if (!view) return -1;
    int level = 0;
    if (parse_int(unity::ui_text(ref::rd<void*>(view, fInfoSkillLevelText)), level)) return level;
    if (parse_int(unity::ui_text(ref::rd<void*>(view, fSkillLevelText)), level)) return level;
    return -1;
}

/// JobMenuState.GetJobLevel: the UI first (most accurate), then the character's job data, else 1.
int job_level(void* window, void* character, int job_id)
{
    const int ui_level = job_level_from_ui(window);
    if (ui_level > 0) return ui_level;
    if (!character) return 1;
    void* list = ref::call<void*>(mCharJobDataList, character);
    const int n = unity::list_count(list);
    for (int i = 0; i < n; ++i) {
        void* job = unity::list_at(list, i);
        if (job && ref::call<int32_t>(mOwnedJobId, job) == job_id) {
            const int level = ref::call<int32_t>(mOwnedJobLevel, job);
            return level > 0 ? level : 1;
        }
    }
    return 1;
}

// UpdateJobInfo_Postfix
void on_update_job_info(void* self, void* character, void* job)
{
    if (!character || !job) return;
    const std::wstring job_name = localized(ref::call_str(mJobMesIdName, job));
    if (job_name.empty()) return;

    const int job_id = ref::call<int32_t>(mJobId, job);
    const int level = job_level(self, character, job_id);
    const bool equipped = job_id == ref::call<int32_t>(mCharJobId, character);
    const std::wstring announcement = equipped
        ? TF(L"{0}, Job Level {1}. Equipped.", {job_name, text::num(level)})
        : TF(L"{0}, Job Level {1}", {job_name, text::num(level)});

    if (!dedup::text(kCtxSelect, announcement)) return;

    // Active only after validation: the menu is confirmed open with valid data.
    menu_state::set_active_exclusive(kJobMenuKey);
    g_has_focused_job = true;
    g_focused_description = localized(ref::call_str(mJobMesIdDescription, job));
    ffpr::say(announcement, true);
}

void reset_job_menu()
{
    dedup::reset(kCtxSelect);
    g_has_focused_job = false;
    g_focused_description.clear();
}

using UpdateJobInfo_t = void (*)(void* self, void* character, void* job, const void* mi);
using SetActive_t = void (*)(void* self, bool active, const void* mi);
UpdateJobInfo_t o_UpdateJobInfo;
SetActive_t o_JobWindowSetActive;

void d_UpdateJobInfo(void* self, void* character, void* job, const void* mi)
{
    o_UpdateJobInfo(self, character, job, mi);
    hooks::guarded("FF3.JobMenu.UpdateJobInfo", [&] { on_update_job_info(self, character, job); });
}

void d_JobWindowSetActive(void* self, bool active, const void* mi)
{
    o_JobWindowSetActive(self, active, mi);
    if (!active) menu_state::reset(kJobMenuKey);   // runs reset_job_menu
}

int install()
{
    int n = 0;
    if (hooks::attach(mUpdateJobInfo, &d_UpdateJobInfo, &o_UpdateJobInfo)) {
        ++n;
        LOGF("[FF3 JobMenu] hooked JobChangeWindowController.UpdateJobInfo");
    }
    if (hooks::attach(mJobWindowSetActive, &d_JobWindowSetActive, &o_JobWindowSetActive)) {
        ++n;
        LOGF("[FF3 JobMenu] hooked JobChangeWindowController.SetActive");
    }

    menu_state::register_reset(kJobMenuKey, [] { reset_job_menu(); });
    // FFIII_ScreenReaderMod.ClearMenuFlagsForMapTransition also cleared JOB_MENU.
    events::on(events::Kind::MapChanged, [] { menu_state::reset(kJobMenuKey); });

    queries::define("JobMenuState.IsActive", [] { return menu_state::is_active(kJobMenuKey); });
    queries::define("JobMenuState.ShouldSuppress", [] { return menu_state::is_active(kJobMenuKey); });
    cursor::add_suppressor("JobMenuState.ShouldSuppress");

    // The I key (see the header): FF3's job description in the slot of FF1's class description.
    queries::define("JobSelectionPatches.IsActive", [] { return menu_state::is_active(kJobMenuKey); });
    queries::define_text("JobSelectionPatches.GetCurrentDescription",
                         [] { return g_has_focused_job ? g_focused_description : std::wstring(); });
    return n;
}

core::Install s_install{"FF3.JobMenu", &install, 500, game::kFF3};

} // namespace
} // namespace ffpr::ff3
