// Ports FF5 Utils/MenuFocusAnnouncer.cs and the small FF5 helpers the module's screens share
// (see ff5_internal.h).
//
// MenuFocusAnnouncer is not a poll and not a timer: a request starts from a state-entry hook,
// runs at most MaxSettleFrames (6) frames and stops the frame the read succeeds (a bounded read
// that starts from an event, as the shared rules allow).
#include "../../core/ui/preferences.h"
#include "ff5_internal.h"

#include <cstdint>
#include <memory>
#include <vector>
#include "../../core/frame.h"
#include "../../core/modtext.h"
#include "../../core/battle/battle_util.h"
#include "../../core/hooks.h"
#include "../../core/log.h"
#include "../../core/refs.h"
#include "../../core/settings.h"
#include "../../core/text.h"
#include "../../core/ui_util.h"
#include "../../core/unity.h"
#include "../../core/status/status.h"

namespace ffpr::ff5 {
namespace {

ref::Class  kMenuManager{"Last.UI", "MenuManager", game::kFF5};
ref::Method mMenuIsOpen{kMenuManager, "get_IsOpen", 0};

// TIMEOUT, not a delay: the loop exits the frame the read succeeds.
constexpr int kMaxSettleFrames = 6;
// Frames a read may take before it is worth logging.
constexpr int kSlowReadFrames = 2;

// Bumped on every request; an in-flight read stops when a newer request superseded it.
int g_gen = 0;

} // namespace

namespace focus {

void cancel() { ++g_gen; }

void request(const char* tag, std::function<bool()> try_announce)
{
    if (!try_announce) return;
    const int gen = ++g_gen;
    std::string label = tag ? tag : "Focus";
    auto frame_no = std::make_shared<int>(0);
    frame::every_frame(
        [gen, label, frame_no, fn = std::move(try_announce)]() -> bool {
            const int f = (*frame_no)++;
            if (gen != g_gen) return false;   // a newer request superseded this one
            bool done = false;
            try {
                done = fn();
            } catch (...) {
                LOGF("[%s] Error reading focus", label.c_str());
                return false;
            }
            if (done) {
                if (f >= kSlowReadFrames) LOGF("[%s] initial focus read took %d frames", label.c_str(), f + 1);
                return false;
            }
            if (f + 1 >= kMaxSettleFrames) {
                LOGF("[%s] initial focus read gave up after %d frames", label.c_str(), kMaxSettleFrames);
                return false;
            }
            return true;
        },
        "FF5.MenuFocus", kMaxSettleFrames + 1);
}

bool menu_open()
{
    return hooks::guarded_r("FF5.MenuFocus.IsMenuOpen", false, [] {
        void* mm = ref::instance(kMenuManager);
        return mm && unity::alive(mm) && ref::call<bool>(mMenuIsOpen, mm);
    });
}

bool alive(void* component)
{
    return component && unity::alive(component) && unity::active_in_hierarchy(component);
}

} // namespace focus

std::wstring message(const std::wstring& mes_id)
{
    if (mes_id.empty()) return {};
    return ffpr::status::game_message(mes_id, true);
}

std::wstring message_from(void* il2cpp_string_id)
{
    return message(ref::str(il2cpp_string_id));
}

bool auto_detail() { return ui::prefs::auto_detail_enabled(); }

bool blank(const std::wstring& s) { return text::is_blank(s); }

std::wstring text_of(void* ui_text)
{
    if (!ui_text) return {};
    return text::trim(unity::ui_text(ui_text));
}

std::wstring text_if_shown(void* ui_text)
{
    if (!ui_text || !unity::alive(ui_text) || !unity::active_in_hierarchy(ui_text)) return {};
    std::wstring v = unity::ui_text(ui_text);
    return text::is_blank(v) ? std::wstring() : text::trim(v);
}

int cursor_index(void* cursor) { return ui::cursor_index(cursor); }

// --- CharacterStatusHelper --------------------------------------------------------------------
namespace {

ref::Class  kCharParam5{"Last.Data", "CharacterParameterBase", game::kFF5};
ref::Field  fCurrentMP5{kCharParam5, "currentMP"};
ref::Method mConfirmedMaxMp5{kCharParam5, "ConfirmedMaxMp", 0};

// ConditionTypeFallbackNames: user-visible Last.Defaine.ConditionType values -> mod_text key.
struct TypeName { int type; const wchar_t* key; };
constexpr TypeName kConditionTypeFallbackNames[] = {
    {4, L"Critical"},  {5, L"KO"},     {6, L"Silence"},  {7, L"Sleep"},    {8, L"Paralysis"},
    {9, L"Blind"},     {10, L"Poison"}, {11, L"Stone"},  {12, L"Confusion"}, {14, L"Blink"},
    {15, L"Haste"},    {16, L"Slow"},  {17, L"Stop"},    {19, L"Protect"}, {25, L"Regen"},
    {32, L"Old"},      {34, L"Zombie"}, {401, L"Mini"},  {402, L"Toad"},   {404, L"Doom"},
    {409, L"Float"},   {410, L"Berserk"}, {412, L"Shell"}, {413, L"Reflect"},
};

const wchar_t* fallback_key(int type)
{
    for (const TypeName& t : kConditionTypeFallbackNames)
        if (t.type == type) return t.key;
    return nullptr;
}

// One-shot diagnostic (_hasLoggedConditionDiag): the first read of a session is logged.
bool g_logged_condition_diag = false;

} // namespace

std::wstring condition_type_name(int condition_type)
{
    const wchar_t* key = fallback_key(condition_type);
    return key ? T(key) : std::wstring();
}

std::wstring vitals_string(void* parameter)
{
    if (!parameter) return {};
    return hooks::guarded_r("FF5.CharacterStatusHelper.Vitals", std::wstring(), [&]() -> std::wstring {
        int current_hp = battle::param_current_hp(parameter);
        int max_hp = 0;
        if (!battle::param_confirmed(parameter, battle::Stat::MaxHp, max_hp)) return {};
        int current_mp = ref::rd<int32_t>(parameter, fCurrentMP5);
        int max_mp = 0;
        if (!battle::vcall(mConfirmedMaxMp5, parameter, max_mp)) return {};
        return T(L"HP") + L" " + text::num(current_hp) + L"/" + text::num(max_hp) + L", " + T(L"MP") + L" " +
               text::num(current_mp) + L"/" + text::num(max_mp);
    });
}

std::wstring status_conditions(void* parameter)
{
    if (!parameter) return {};
    const bool should_log = !g_logged_condition_diag;
    return hooks::guarded_r("FF5.CharacterStatusHelper.Conditions", std::wstring(), [&]() -> std::wstring {
        std::vector<void*> list = battle::confirmed_conditions(parameter);
        if (should_log) LOGF("[ConditionDiag] ConfirmedConditionList: count=%d", static_cast<int>(list.size()));
        if (list.empty()) return {};
        std::vector<std::wstring> names;
        for (void* condition : list) {
            if (!condition) continue;
            const int type = battle::condition_type(condition);
            if (!fallback_key(type)) {
                if (should_log) LOGF("[ConditionDiag] ConditionType %d not in whitelist, skipping", type);
                continue;
            }
            std::wstring display;
            const std::wstring mes_id = battle::condition_mes_id(condition);
            if (!mes_id.empty() && mes_id != L"None") display = message(mes_id);
            display = display.empty() ? condition_type_name(type) : battle::strip_icon(display);
            names.push_back(display);
        }
        if (should_log) {
            g_logged_condition_diag = true;
            LOGF("[ConditionDiag] Final status names: %d", static_cast<int>(names.size()));
        }
        std::wstring out;
        for (size_t i = 0; i < names.size(); ++i) {
            if (i) out += L", ";
            out += names[i];
        }
        return out;
    });
}

std::wstring full_status(void* parameter)
{
    if (!parameter) return {};
    std::wstring vitals = vitals_string(parameter);
    if (vitals.empty()) return {};
    std::wstring result = L", " + vitals;
    std::wstring conditions = status_conditions(parameter);
    if (!conditions.empty()) result += L", " + T(L"Status") + L": " + conditions;
    return result;
}

} // namespace ffpr::ff5
