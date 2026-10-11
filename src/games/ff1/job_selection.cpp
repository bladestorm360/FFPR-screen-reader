// Ports FF1 Patches/JobSelectionPatches.cs.
//
// Class choice on FF1's new-game screen (Last.UI.KeyInput.JobContentListController): the job
// name, then one frame later its description (when Auto Detail is on) and the position.
// The on-demand detail key reads the description through "JobSelectionPatches.GetCurrentDescription".
//
// Parity notes: names and descriptions are found in the list's UI Text components by matching
// the C#'s English job-name list, exactly as FF1 does. The C#'s Touch-version fallback only
// ran when the KeyInput class was missing (it never is in FF1); its "Strategy 2" loop and
// LogComponentTypes did nothing and are not ported.
//
// JobContentListController is a shared Last.UI.KeyInput class (all five dumps); masked to FF1
// because only FF1's module hooks it here.
#include "../../core/ui/preferences.h"
#include <windows.h>
#include <cstdint>
#include <string>
#include <vector>
#include "../../core/core.h"
#include "../../core/frame.h"
#include "../../core/hooks.h"
#include "../../core/log.h"
#include "../../core/refs.h"
#include "../../core/say.h"
#include "../../core/settings.h"
#include "../../core/text.h"
#include "../../core/ui_util.h"
#include "../../core/unity.h"

namespace ffpr::ff1 {
namespace {

ref::Class kJobList{"Last.UI.KeyInput", "JobContentListController", game::kFF1};
ref::Method mJobSelectContent{kJobList, "SelectContent", 1};

// IsHandlingCursor: set by SelectContent, cleared by the cursor router or ResetState.
bool g_handling_cursor = false;
// _lastController: the live job list, for the on-demand detail key. Re-checked on every use.
void* g_controller = nullptr;

// FF1 job names (the C#'s list, matched OrdinalIgnoreCase).
constexpr const wchar_t* kJobNames[] = {L"Warrior",    L"Thief",      L"Monk",    L"Red Mage",
                                        L"White Mage", L"Black Mage", L"Knight",  L"Ninja",
                                        L"Master",     L"Red Wizard", L"White Wizard", L"Black Wizard"};

bool is_job_name(const std::wstring& s)
{
    for (const wchar_t* name : kJobNames)
        if (CompareStringOrdinal(s.c_str(), static_cast<int>(s.size()), name, -1, TRUE) == CSTR_EQUAL) return true;
    return false;
}

bool controller_alive(void* ctrl)
{
    return ctrl && kJobList.is(ctrl) && unity::alive(ctrl);
}

/// GetJobNameViaHierarchy: the index-th Text (including inactive ones) whose trimmed text is a
/// job name. `job_count` is the number of such texts when found, -1 otherwise.
std::wstring job_name_via_hierarchy(void* list_ctrl, int index, int& job_count)
{
    job_count = -1;
    std::vector<std::wstring> job_texts;
    for (void* t : unity::texts(list_ctrl, true)) {
        std::wstring value = unity::ui_text(t);
        if (text::is_blank(value)) continue;
        value = text::trim(value);
        if (is_job_name(value)) job_texts.push_back(value);
    }
    if (index >= 0 && index < static_cast<int>(job_texts.size())) {
        job_count = static_cast<int>(job_texts.size());
        return job_texts[static_cast<size_t>(index)];
    }
    return {};
}

/// GetDescriptionViaHierarchy: the longest trimmed Text that is not a job name and has at
/// least 10 characters (the first one on a tie). Empty when none.
std::wstring description_via_hierarchy(void* list_ctrl)
{
    if (!controller_alive(list_ctrl)) return {};
    std::wstring best;
    size_t best_len = 0;
    for (void* t : unity::texts(list_ctrl, true)) {
        std::wstring value = unity::ui_text(t);
        if (text::is_blank(value)) continue;
        value = text::trim(value);
        if (is_job_name(value)) continue;
        if (value.size() < 10) continue;
        if (value.size() > best_len) {
            best_len = value.size();
            best = value;
        }
    }
    return best;
}

std::wstring safe_description(void* list_ctrl)
{
    return hooks::guarded_r("JobSelection.Description", std::wstring(), [&] { return description_via_hierarchy(list_ctrl); });
}

/// AnnounceJobWithDelayedDescription, one frame after SelectContent.
void announce_job(void* ctrl, const std::wstring& job_name, int index, int count)
{
    std::wstring announcement = job_name;
    // The description only with Auto Detail on; otherwise the detail key reads it on demand.
    if (ui::prefs::auto_detail_enabled()) {
        std::wstring description = safe_description(ctrl);
        if (!description.empty()) announcement = job_name + L". " + description;
    }
    // Position last, after any description.
    ffpr::say(ui::menu_position(announcement, index, count));
}

void on_select_content(void* self, void* cursor)
{
    g_handling_cursor = true;   // the generic cursor reader stays quiet
    if (!cursor || !self) return;
    int index = ui::cursor_index(cursor);
    g_controller = self;

    int job_count = -1;
    std::wstring job_name = job_name_via_hierarchy(self, index, job_count);
    if (job_name.empty()) return;

    frame::next_frame([self, job_name, index, job_count] { announce_job(self, job_name, index, job_count); },
                      "JobSelection.Announce");
}

bool is_active()
{
    void* ctrl = g_controller;
    return controller_alive(ctrl) && unity::active_in_hierarchy(ctrl);
}

std::wstring current_description()
{
    void* ctrl = g_controller;
    return ctrl ? safe_description(ctrl) : std::wstring();
}

void reset_state()
{
    g_handling_cursor = false;
    g_controller = nullptr;
}

using VoidPtr_t = void (*)(void* self, void* arg, const void* mi);
VoidPtr_t o_JobSelectContent;

void d_JobSelectContent(void* self, void* cursor, const void* mi)
{
    o_JobSelectContent(self, cursor, mi);
    hooks::guarded("JobSelection.SelectContent", [&] { on_select_content(self, cursor); });
}

int install()
{
    int n = hooks::attach(mJobSelectContent, &d_JobSelectContent, &o_JobSelectContent) ? 1 : 0;
    if (!n) LOGF("FF1 JobSelection: JobContentListController.SelectContent not hooked - job selection disabled");

    queries::define("JobSelectionPatches.IsActive", [] { return is_active(); });
    queries::define("JobSelectionPatches.IsHandlingCursor", [] { return g_handling_cursor; });
    queries::define_text("JobSelectionPatches.GetCurrentDescription", [] { return current_description(); });
    actions::define("JobSelectionPatches.ClearHandlingFlag", [] { g_handling_cursor = false; });
    actions::define("JobSelectionPatches.ResetState", [] { reset_state(); });
    return n;
}

core::Install s_install{"FF1.JobSelection", &install, 501, game::kFF1};

} // namespace
} // namespace ffpr::ff1
