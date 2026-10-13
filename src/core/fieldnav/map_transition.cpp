// Ports FF1 Patches/MapTransitionPatches.cs, Utils/LocationMessageTracker.cs, and
// FFI_ScreenReaderMod.ClearMenuFlagsForMapTransition (called only from the map transition).
//
// SubSceneManagerMainGame.ChangeState(State) has one RVA hit in every dump.cs (not folded).
// FadeManager.IsFadeFinish is called, not hooked (FF1 avoided hooking FadeManager).
#include <windows.h>
#include <cstdint>
#include <string>
#include <vector>
#include "map_transition.h"
#include "../core.h"
#include "../frame.h"
#include "../hooks.h"
#include "../log.h"
#include "../modtext.h"
#include "../refs.h"
#include "../say.h"
#include "../text.h"
#include "../field/field_api.h"
#include "../field/field_internal.h"

namespace ffpr::fieldnav::location_message {
namespace {
std::wstring g_last_map_transition_message;
bool g_in_map_transition = false;

/// .NET ToUpperInvariant, for an OrdinalIgnoreCase search.
std::wstring upper_invariant(const std::wstring& s)
{
    if (s.empty()) return s;
    std::wstring out(s);
    int n = LCMapStringEx(LOCALE_NAME_INVARIANT, LCMAP_UPPERCASE, s.c_str(), static_cast<int>(s.size()),
                          out.data(), static_cast<int>(out.size()), nullptr, nullptr, 0);
    if (n <= 0) return s;
    out.resize(static_cast<size_t>(n));
    return out;
}

/// Location names are typically 1-4 words without sentence punctuation.
bool looks_like_location_name(const std::wstring& t)
{
    if (text::is_blank(t))
        return false;

    // Sentence-like punctuation: probably a system message
    if (t.find(L'.') != std::wstring::npos || t.find(L'!') != std::wstring::npos ||
        t.find(L'?') != std::wstring::npos)
        return false;

    // Count words (split on ' ', empty entries removed)
    int words = 0;
    bool in_word = false;
    for (wchar_t c : t) {
        if (c == L' ') { in_word = false; continue; }
        if (!in_word) { ++words; in_word = true; }
    }
    return words <= 4;
}
} // namespace

void set_last_map_transition(const std::wstring& message)
{
    g_last_map_transition_message = text::trim(message);
    g_in_map_transition = !g_last_map_transition_message.empty();
}

bool should_announce_fade_message(std::wstring_view message)
{
    if (text::is_blank(message))
        return false;

    std::wstring trimmed = text::trim(message);

    if (g_in_map_transition && !g_last_map_transition_message.empty()) {
        // Skip if this message is contained in the transition message
        // ("Cornelia 1F" contained in "Entering Cornelia 1F")
        if (upper_invariant(g_last_map_transition_message).find(upper_invariant(trimmed)) != std::wstring::npos)
            return false;
    } else if (!g_in_map_transition) {
        // No transition fired: menu open or another UI event. Skip a short location name.
        if (looks_like_location_name(trimmed))
            return false;
    }
    return true;
}

void reset()
{
    g_last_map_transition_message.clear();
    g_in_map_transition = false;
}

bool in_map_transition() { return g_in_map_transition; }
const std::wstring& last_map_transition() { return g_last_map_transition_message; }

} // namespace ffpr::fieldnav::location_message

namespace ffpr::fieldnav::map_transition {
namespace {

ref::Class  kFadeManager{"System.Fade", "FadeManager"};
ref::Method mIsFadeFinish{kFadeManager, "IsFadeFinish", 0};

ref::Class  kSubSceneManagerMainGame{"Last.Management", "SubSceneManagerMainGame"};
ref::Method mChangeState{kSubSceneManagerMainGame, "ChangeState", 1};

// SubSceneManagerMainGame.State (identical in all five games)
constexpr int32_t kStateChangeMap = 1;
constexpr int32_t kStatePlayer = 3;
constexpr int32_t kStateTransport = 4;

int g_last_announced_map_id = -1;
bool g_needs_map_scan = true;

/// FFI_ScreenReaderMod.ClearMenuFlagsForMapTransition: clear menu and popup flags a set/clear
/// mismatch left behind, so they cannot block the field on the new map. Battle flags stay.
void clear_menu_flags_for_map_transition()
{
    std::vector<std::string> stuck = menu_state::active_states();
    if (!stuck.empty()) {
        std::string list;
        for (size_t i = 0; i < stuck.size(); ++i) {
            if (i) list += ",";
            list += stuck[i];
        }
        LOGF("[MapTransition] Pre-clear active flags: %s", list.c_str());
    }

    static const char* const kKeys[] = {
        menu_state::MAIN_MENU, menu_state::MAGIC_MENU, menu_state::ITEM_MENU, menu_state::EQUIP_MENU,
        menu_state::STATUS_MENU, menu_state::STATUS_DETAILS, menu_state::CONFIG_MENU,
        menu_state::SAVE_LOAD_MENU, menu_state::SHOP_MENU, menu_state::BESTIARY_LIST,
        menu_state::BESTIARY_DETAIL, menu_state::BESTIARY_FORMATION, menu_state::BESTIARY_MAP,
        menu_state::MUSIC_PLAYER, menu_state::GALLERY, menu_state::POPUP,
    };
    for (const char* key : kKeys)
        menu_state::reset(key);

    // ShopMenuTracker.ClearState() and PopupState.Clear() belong to the shop and popup areas.
    if (actions::defined("ShopMenuTracker.ClearState")) actions::run("ShopMenuTracker.ClearState");
    if (actions::defined("PopupState.Clear")) actions::run("PopupState.Clear");
}

/// Read the current map id and announce a change. Announces on every transition, including
/// a loaded game.
void check_map_transition()
{
    int current_map_id = ::field::current_map_id();
    if (current_map_id <= 0)
        return;

    // Same map: no announcement
    if (current_map_id == g_last_announced_map_id && !g_needs_map_scan) {
        if (::field::scanner::needs_scan(current_map_id)) ::field::request_rescan();
        return;
    }

    g_last_announced_map_id = current_map_id;
    g_needs_map_scan = false;

    // The map really changed, so the transition is over (no menu is possible during it).
    clear_menu_flags_for_map_transition();

    events::fire(events::Kind::MapChanged);
    ::field::request_rescan();

    std::wstring map_name = queries::text("MapNameResolver.GetCurrentMapName");
    if (map_name.empty() || map_name == T(L"Unknown"))
        return;

    std::wstring announcement = TF(L"Entering {0}", {map_name});

    // Record for the fade-message check (suppresses the bare name from FadeMessageManager.Play)
    location_message::set_last_map_transition(announcement);

    ffpr::say(announcement, false);
}

using ChangeState_t = void (*)(void* self, int32_t state, const void* mi);
ChangeState_t o_ChangeState = nullptr;

void d_ChangeState(void* self, int32_t state, const void* mi)
{
    const double started = frame::now();
    if (state == kStateChangeMap) {
        events::fire(events::Kind::MapChanging);
        g_needs_map_scan = true;
        ::field::scanner::invalidate_map();
    }
    const double native_started = frame::now();
    o_ChangeState(self, state, mi);
    const double observed = frame::now();
    hooks::guarded("MapTransition.ChangeState", [&] {
        // Never scan/name the incoming map while ChangeMap/FieldReady constructs it.
        if (state == kStatePlayer || state == kStateTransport)
            check_map_transition();
    });
    LOGF("[LoadTiming] state %d: prefix %.3f ms, native %.3f ms, observer %.3f ms",
        state, (native_started - started) * 1000, (observed - native_started) * 1000,
        (frame::now() - observed) * 1000);
}

int install()
{
    if (!kFadeManager.get())
        LOGF("[MapTransition] FadeManager type not found");
    else if (!mIsFadeFinish.get())
        LOGF("[MapTransition] IsFadeFinish not found - fade detection disabled");

    queries::define("MapTransitionPatches.IsScreenFading", [] { return is_screen_fading(); });
    queries::define("LocationMessageTracker.InMapTransition", [] { return location_message::in_map_transition(); });
    queries::define_text("LocationMessageTracker.LastMapTransition", [] { return location_message::last_map_transition(); });

    // FFI_ScreenReaderMod.OnSceneLoaded: "Reset location message tracker (but NOT
    // lastAnnouncedMapId - battle transitions change scenes without changing maps)".
    events::on(events::Kind::SceneLoaded, [] { location_message::reset(); });

    if (hooks::attach(mChangeState, &d_ChangeState, &o_ChangeState))
        return 1;
    LOGF("[MapTransition] Could not find SubSceneManagerMainGame.ChangeState - map transition announcements disabled");
    return 0;
}
core::Install s_install{"MapTransition", &install, 110};

} // namespace

bool is_screen_fading()
{
    void* fp = mIsFadeFinish.ptr();
    if (!fp) return false;
    try {
        void* instance = ref::instance(kFadeManager);
        if (!instance) return false;
        // Called directly rather than through ref::call: a failed call must read as "not
        // fading" (FF1's catch), not as IsFadeFinish() == false.
        using F = bool (*)(void*, const void*);
        bool finished = reinterpret_cast<F>(fp)(instance, mIsFadeFinish.get());
        return !finished;
    } catch (...) {
        return false;   // treat as not fading
    }
}

void reset_map_tracking()
{
    location_message::reset();
}

} // namespace ffpr::fieldnav::map_transition
