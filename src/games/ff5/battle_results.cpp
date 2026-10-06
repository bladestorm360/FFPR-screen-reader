// Ports FF5 Core/BattleResultNavigator.cs, Utils/BattleResultDataStore.cs and, from
// Patches/BattleResultPatches.cs, the data the navigator reads: BuildPointsPage (ShowPointsInit:
// characters x EXP / Next / ABP) and the store's clears (ShowPointsInit, EndWaitInit).
//
// The victory screen's SPEECH stays the core's (src/core/battle/battle_result.cpp); this file only
// records the page and never speaks a result line itself. L opens the log (Global binding; the
// Field binding of L, Mod.CycleNext, is more specific and wins on the field) and does nothing when
// no results are stored.
//
// Not ported yet (pending): the level-up pages (ResultStatusUpController.SetData), the learned
// abilities page (ShowGetAbilitysInit / ShowLevelUpAbilitysInit) and the items page
// (ShowGetItemsInit). The log has one page, the points grid, until they are.
//
// The navigator is modal in FF5. The core has no modal hook for a game module, so this file:
//   * reads its own keys (GetAsyncKeyState edges, only while open and the game window has focus)
//     and SDL buttons through input.h, from a frame subscriber that returns at once when closed;
//   * chains on InputSystemManager.GetKeyDown / GetKey / GetKeyUp / GetAnyKey and answers false
//     while open, so the game's result screen does not also act on Enter, arrows or B.
// Core gaps (for the lead): the core's own hotkeys still run while it is open (FF5 consumed every
// key), Start is left to the core router, the left stick is read here with a 0.5 threshold, and
// controller mod mode + B (FF5's other way to open it) needs a router hook. Query published:
// "BattleResultNavigator.IsOpen" (add it to SuppressGameInput / the modal gate).
//
// EndWaitInit's body is shared by 2 methods in FF5: the detour class-checks self.
#include <windows.h>
#include <cstdint>
#include <string>
#include <vector>
#include "ff5_internal.h"
#include "../../core/core.h"
#include "../../core/frame.h"
#include "../../core/hooks.h"
#include "../../core/log.h"
#include "../../core/modtext.h"
#include "../../core/refs.h"
#include "../../core/say.h"
#include "../../core/text.h"
#include "../../core/unity.h"
#include "../../core/battle/battle_util.h"
#include "../../core/input/input.h"

namespace ffpr::ff5 {
namespace {

constexpr uint8_t k5 = game::kFF5;

ref::Class  kResultMenu{"Last.UI.KeyInput", "ResultMenuController", k5};
ref::Field  fResultTargetData{kResultMenu, "targetData"};
ref::Method mShowPointsInit{kResultMenu, "ShowPointsInit", 0};
ref::Method mEndWaitInit{kResultMenu, "EndWaitInit", 0};
ref::Class  kResultData{"Last.Data", "BattleResultData", k5};
ref::Field  fCharacterList{kResultData, "<CharacterList>k__BackingField"};
ref::Class  kResultChar{"Last.Data", "BattleResultData.BattleResultCharacterData", k5};
ref::Field  fBeforData{kResultChar, "<BeforData>k__BackingField"};
ref::Field  fAfterData{kResultChar, "<AfterData>k__BackingField"};
ref::Method mCharGetExp{kResultChar, "get_GetExp", 0};
ref::Class  kOwnedChar{"Last.Data.User", "OwnedCharacterData", k5};
ref::Method mGetNextExp{kOwnedChar, "GetNextExp", 0};
ref::Method mOwnedJob{kOwnedChar, "get_OwnedJob", 0};
ref::Class  kOwnedJob{"Last.Data.User", "OwnedJobData", k5};
ref::Method mJobId{kOwnedJob, "get_Id", 0};
ref::Method mJobProficiency{kOwnedJob, "get_CurrentProficiency", 0};
ref::Class  kExpUtility{"Last.Systems", "ExpUtility", k5};
ref::Method mExpGetNextExp{kExpUtility, "GetNextExp", 3};
constexpr int32_t kExpTableJobExp = 101;   // Last.Defaine.Master.ExpTableType.JobExp

ref::Class  kInputSystemManager{"System.Input", "InputSystemManager", k5};
ref::Method mGetKeyDown{kInputSystemManager, "GetKeyDown", 1};
ref::Method mGetKey{kInputSystemManager, "GetKey", 1};
ref::Method mGetKeyUp{kInputSystemManager, "GetKeyUp", 1};
ref::Method mGetAnyKey{kInputSystemManager, "GetAnyKey", 0};

// --- BattleResultDataStore ----------------------------------------------------------------------
struct Page {
    std::wstring title;
    std::vector<std::wstring> rows;
    std::vector<std::wstring> cols;
    std::vector<std::vector<std::wstring>> cells;   // [row][col]
    std::vector<std::wstring> summaries;            // optional per-row phrasing
};
std::vector<Page> g_pages;

void add_page(Page p)
{
    if (p.rows.empty()) return;
    g_pages.push_back(std::move(p));
}

// --- BuildPointsPage ------------------------------------------------------------------------------
void build_points_page(void* data)
{
    void* list = ref::rd<void*>(data, fCharacterList);
    int n = list ? unity::list_count(list) : 0;
    if (n == 0) return;
    std::vector<std::wstring> names;
    std::vector<int> exps, nexts, abps;
    for (int i = 0; i < n; ++i) {
        void* c = unity::list_at(list, i);
        void* after = c ? ref::rd<void*>(c, fAfterData) : nullptr;
        if (!after) continue;
        int next = ref::call<int32_t>(mGetNextExp, after);
        int abp_to_next = 0;
        void* before = ref::rd<void*>(c, fBeforData);
        void* job = before ? ref::call<void*>(mOwnedJob, before) : nullptr;
        if (job)
            abp_to_next = ref::call_static<int32_t>(mExpGetNextExp, ref::call<int32_t>(mJobId, job),
                                                    ref::call<int32_t>(mJobProficiency, job), kExpTableJobExp);
        names.push_back(battle::character_name(after));
        exps.push_back(ref::call<int32_t>(mCharGetExp, c));
        nexts.push_back(next);
        abps.push_back(abp_to_next);
    }
    if (names.empty()) return;
    bool any_abp = false;
    for (int a : abps) any_abp = any_abp || a > 0;

    Page p;
    p.title = T(L"Battle Results");
    p.rows = names;
    p.cols = {T(L"EXP"), T(L"Next")};
    if (any_abp) p.cols.push_back(T(L"ABP"));
    for (size_t i = 0; i < names.size(); ++i) {
        std::vector<std::wstring> row{battle::n0(exps[i]), nexts[i] > 0 ? battle::n0(nexts[i]) : L"-"};
        if (any_abp) row.push_back(abps[i] > 0 ? text::num(abps[i]) : L"-");
        p.cells.push_back(std::move(row));
    }
    add_page(std::move(p));
}

// --- BattleResultNavigator ------------------------------------------------------------------------
bool g_open = false;
bool g_swallow = false;
int g_page = 0, g_row = 0, g_col = 0;

const Page* page()
{
    return g_page >= 0 && g_page < static_cast<int>(g_pages.size()) ? &g_pages[static_cast<size_t>(g_page)] : nullptr;
}

std::wstring row_text(int row)
{
    const Page* p = page();
    if (!p || row < 0 || row >= static_cast<int>(p->rows.size())) return {};
    if (row < static_cast<int>(p->summaries.size())) return p->summaries[static_cast<size_t>(row)];
    std::wstring s = p->rows[static_cast<size_t>(row)];
    for (size_t c = 0; c < p->cols.size(); ++c) {
        const std::wstring& v = p->cells[static_cast<size_t>(row)][c];
        if (v == L"-") continue;   // no value for this row
        s += L", " + v + L" " + p->cols[c];
    }
    return s;
}

std::wstring page_header()
{
    const Page* p = page();
    if (!p) return {};
    std::vector<std::wstring> parts;
    if (!p->title.empty()) parts.push_back(p->title);
    if (g_pages.size() > 1)
        parts.push_back(TF(L"Page {0} of {1}", {text::num(g_page + 1), text::num(static_cast<long long>(g_pages.size()))}));
    if (!p->rows.empty()) parts.push_back(row_text(0));
    return text::join(parts);
}

void close_navigator()
{
    g_open = false;
    g_swallow = false;
    g_page = g_row = g_col = 0;
}

void navigate_page(int delta)
{
    int count = static_cast<int>(g_pages.size());
    if (count <= 1) return;
    g_page = ((g_page + delta) % count + count) % count;
    g_row = g_col = 0;
    ffpr::say(page_header(), true);
}

void navigate_row(int delta)
{
    const Page* p = page();
    if (!p || p->rows.empty()) return;
    int count = static_cast<int>(p->rows.size());
    g_row = ((g_row + delta) % count + count) % count;
    ffpr::say(row_text(g_row), true);
}

void navigate_col(int delta)
{
    const Page* p = page();
    if (!p || p->cols.empty()) return;
    int count = static_cast<int>(p->cols.size());
    g_col = ((g_col + delta) % count + count) % count;
    ffpr::say(p->cols[static_cast<size_t>(g_col)] + L": " + p->cells[static_cast<size_t>(g_row)][static_cast<size_t>(g_col)], true);
}

// Keyboard edges (only while open).
constexpr int kVks[] = {VK_ESCAPE, VK_PRIOR, VK_NEXT, VK_UP, VK_DOWN, VK_LEFT, VK_RIGHT, VK_RETURN, VK_HOME};
bool g_prev[sizeof(kVks) / sizeof(kVks[0])] = {};
bool g_now[sizeof(kVks) / sizeof(kVks[0])] = {};
float g_prev_lx = 0, g_prev_ly = 0;

void poll_keys()
{
    for (size_t i = 0; i < sizeof(kVks) / sizeof(kVks[0]); ++i) {
        g_prev[i] = g_now[i];
        g_now[i] = (GetAsyncKeyState(kVks[i]) & 0x8000) != 0;
    }
}
bool pressed(int vk)
{
    for (size_t i = 0; i < sizeof(kVks) / sizeof(kVks[0]); ++i)
        if (kVks[i] == vk) return g_now[i] && !g_prev[i];
    return false;
}

// SDL_GamepadButton values.
constexpr int kSouth = 0, kEast = 1, kLeftShoulder = 9, kRightShoulder = 10, kDpadUp = 11, kDpadDown = 12,
              kDpadLeft = 13, kDpadRight = 14;

void open_navigator()
{
    if (g_open) return;
    if (g_pages.empty()) return;   // nothing to review (the C#'s "no data" line is unreachable from L)
    g_open = true;
    g_swallow = true;
    g_page = static_cast<int>(g_pages.size()) - 1;
    g_row = g_col = 0;
    poll_keys();
    poll_keys();   // seed: a key already down is not a press
    g_prev_lx = input::left_stick_x();
    g_prev_ly = input::left_stick_y();
    frame::after_frames(2, [] {
        if (g_open) ffpr::say(page_header(), false);
    }, "FF5.BattleResultNavigator.Open");
}

/// HandleInput, once per frame while open.
void handle_input()
{
    if (!g_open) return;
    if (g_swallow) {
        g_swallow = false;
        poll_keys();
        return;
    }
    const bool focused = input::game_window_focused();
    if (focused) poll_keys();
    auto key = [&](int vk) { return focused && pressed(vk); };
    float lx = input::left_stick_x(), ly = input::left_stick_y();
    auto edge = [](float prev, float now, float sign) { return now * sign > 0.5f && prev * sign <= 0.5f; };
    // SDL axes: negative y is up.
    bool stick_up = edge(g_prev_ly, ly, -1.0f), stick_down = edge(g_prev_ly, ly, 1.0f);
    bool stick_left = edge(g_prev_lx, lx, -1.0f), stick_right = edge(g_prev_lx, lx, 1.0f);
    g_prev_lx = lx;
    g_prev_ly = ly;

    if (key(VK_ESCAPE) || input::button_pressed(kEast)) {
        close_navigator();
        return;
    }
    if (key(VK_PRIOR) || input::button_pressed(kLeftShoulder)) return navigate_page(-1);
    if (key(VK_NEXT) || input::button_pressed(kRightShoulder)) return navigate_page(1);
    if (key(VK_UP) || input::button_pressed(kDpadUp) || stick_up) return navigate_row(-1);
    if (key(VK_DOWN) || input::button_pressed(kDpadDown) || stick_down) return navigate_row(1);
    if (key(VK_LEFT) || input::button_pressed(kDpadLeft) || stick_left) return navigate_col(-1);
    if (key(VK_RIGHT) || input::button_pressed(kDpadRight) || stick_right) return navigate_col(1);
    if (key(VK_RETURN) || key(VK_HOME) || input::button_pressed(kSouth)) {
        const Page* p = page();
        if (p && !p->rows.empty()) ffpr::say(row_text(g_row), true);
    }
}

void clear_store()
{
    g_pages.clear();
    close_navigator();
}

// --- detours ------------------------------------------------------------------------------------
using V_t = void (*)(void* self, const void* mi);
using KeyX_t = bool (*)(void* self, int32_t type, const void* mi);
using AnyKey_t = bool (*)(void* self, const void* mi);

V_t o_ShowPointsInit;
void d_ShowPointsInit(void* self, const void* mi)
{
    o_ShowPointsInit(self, mi);
    hooks::guarded("FF5.BattleResult.ShowPointsInit", [&] {
        void* data = self ? ref::rd<void*>(self, fResultTargetData) : nullptr;
        if (!data) return;
        clear_store();   // a new result sequence drops the old pages
        build_points_page(data);
    });
}

V_t o_EndWaitInit;
void d_EndWaitInit(void* self, const void* mi)
{
    o_EndWaitInit(self, mi);
    if (self && kResultMenu.is(self)) clear_store();   // folded body: 2 methods
}

KeyX_t o_GetKeyDown, o_GetKey, o_GetKeyUp;
AnyKey_t o_GetAnyKey;
bool d_GetKeyDown(void* self, int32_t type, const void* mi) { bool r = o_GetKeyDown(self, type, mi); return g_open ? false : r; }
bool d_GetKey(void* self, int32_t type, const void* mi) { bool r = o_GetKey(self, type, mi); return g_open ? false : r; }
bool d_GetKeyUp(void* self, int32_t type, const void* mi) { bool r = o_GetKeyUp(self, type, mi); return g_open ? false : r; }
bool d_GetAnyKey(void* self, const void* mi) { bool r = o_GetAnyKey(self, mi); return g_open ? false : r; }

void tick()
{
    if (!g_open) return;
    hooks::guarded("FF5.BattleResultNavigator.HandleInput", [] { handle_input(); });
}

int install()
{
    int n = 0;
    n += hooks::attach(mShowPointsInit, &d_ShowPointsInit, &o_ShowPointsInit) ? 1 : 0;
    n += hooks::attach(mEndWaitInit, &d_EndWaitInit, &o_EndWaitInit) ? 1 : 0;
    n += hooks::attach(mGetKeyDown, &d_GetKeyDown, &o_GetKeyDown) ? 1 : 0;
    n += hooks::attach(mGetKey, &d_GetKey, &o_GetKey) ? 1 : 0;
    n += hooks::attach(mGetKeyUp, &d_GetKeyUp, &o_GetKeyUp) ? 1 : 0;
    n += hooks::attach(mGetAnyKey, &d_GetAnyKey, &o_GetAnyKey) ? 1 : 0;
    frame::subscribe(&tick, "FF5.BattleResultNavigator");
    events::on(events::Kind::BattleStart, [] { clear_store(); });

    actions::define("BattleResultNavigator.Open", [] { open_navigator(); });
    actions::define("BattleResultNavigator.Close", [] { close_navigator(); });
    queries::define("BattleResultNavigator.IsOpen", [] { return g_open; });
    queries::define("BattleResultDataStore.HasData", [] { return !g_pages.empty(); });
    input::add_binding(input::Key::L, input::Mod::None, input::Context::Global, "BattleResultNavigator.Open",
                       "Open battle result details");
    return n;
}

core::Install s_install{"FF5.BattleResults", &install, 545, game::kFF5};

} // namespace
} // namespace ffpr::ff5
