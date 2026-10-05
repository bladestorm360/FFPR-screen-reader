// Ports FF1 Patches/GalleryPatches.cs (GalleryStateTracker, GalleryManualPatches) and
// Menus/GalleryReader.cs.
//
// The Extra Gallery: entry ("Gallery" then the focused picture), list navigation, and opening a
// picture. SubSceneManagerExtraGallery (Last.Management) and the KeyInput
// GalleryTopListController are identical in all five games; neither hooked body is folded.
#include "extras.h"
#include <cstdlib>
#include "../core.h"
#include "../frame.h"
#include "../hooks.h"
#include "../log.h"
#include "../modtext.h"
#include "../say.h"
#include "../text.h"
#include "../unity.h"

namespace ffpr::extras {
namespace {

ref::Class  kSubSceneGallery{"Last.Management", "SubSceneManagerExtraGallery"};
ref::Method mChangeState{kSubSceneGallery, "ChangeState", 1};

ref::Class  kTopList{"Last.UI.KeyInput", "GalleryTopListController"};
ref::Method mSetFocusContent{kTopList, "SetFocusContent", 1};
ref::Field  fSelectData{kTopList, "selectData"};

ref::Class  kContentData{"Last.OutGame.Gallery", "GalleryListCotentData"};
ref::Field  fNumber{kContentData, "<number>k__BackingField"};
ref::Field  fName{kContentData, "<name>k__BackingField"};

// SubSceneManagerExtraGallery.State
constexpr int STATE_INIT = 0, STATE_VIEW = 1, STATE_DETAILS = 2, STATE_GOTO_TITLE = 3;

// GalleryStateTracker
bool g_in_gallery = false;
bool g_suppress_content_change = false;
void* g_cached_focused = nullptr;   // GalleryTopListController; re-checked before use
int g_previous_state = STATE_INIT;

// Entry announce (FF1 round 2, 2026-09-24 - replaces a 2-second poll). "Gallery" is spoken one
// frame after entry; the focused entry follows as soon as SetFocusContent has reported it.
bool g_entry_header_spoken = false;

// --- GalleryReader ---------------------------------------------------------------------------

/// C# "D{width}" for an int.
std::wstring pad(int v, int width)
{
    std::wstring digits = std::to_wstring(std::abs(static_cast<long long>(v)));
    while (static_cast<int>(digits.size()) < width) digits.insert(digits.begin(), L'0');
    return v < 0 ? L"-" + digits : digits;
}

/// ReadContentFromPointer: selectData's number and name. False when anything is missing.
bool read_content(void* controller, int& number, std::wstring& name)
{
    number = 0;
    name.clear();
    if (!controller) return false;
    void* data = ref::rd<void*>(controller, fSelectData);
    if (!data) return false;
    number = ref::rd<int32_t>(data, fNumber);
    name = ref::rd_str(data, fName);
    return !name.empty();
}

/// ReadListEntry: "001: Image Name".
std::wstring list_entry(int number, const std::wstring& name)
{
    if (name.empty()) return {};
    return pad(number, 3) + L": " + name;
}

bool cached_focus_alive()
{
    return g_cached_focused && kTopList.is(g_cached_focused) && unity::alive(g_cached_focused);
}

// --- entry -------------------------------------------------------------------------------------

/// Speaks the cached focused entry after the "Gallery" header, then ends the entry suppression.
/// False (suppression kept) until an entry can be read.
bool try_announce_entry_item_body()
{
    if (!g_suppress_content_change || !g_entry_header_spoken) return false;
    int number = 0;
    std::wstring name;
    if (!cached_focus_alive() || !read_content(g_cached_focused, number, name)) return false;

    g_suppress_content_change = false;
    std::wstring entry = list_entry(number, name);
    if (!entry.empty()) ffpr::say(entry, false);
    return true;
}

bool try_announce_entry_item()
{
    bool completed = false;
    bool result = hooks::guarded_r("GalleryPatches.TryAnnounceEntryItem", true, [&] {
        bool r = try_announce_entry_item_body();
        completed = true;
        return r;
    });
    // The C# catch: a fault ends the entry suppression so navigation still speaks.
    if (!completed) g_suppress_content_change = false;
    return result;
}

void start_entry_announce()
{
    g_entry_header_spoken = false;
    frame::next_frame([] {
        hooks::guarded("GalleryPatches.Entry", [] {
            ffpr::say(T(L"Gallery"), true);
            g_entry_header_spoken = true;
            try_announce_entry_item();
        });
    }, "Gallery.Entry");
}

// --- SubSceneManagerExtraGallery.ChangeState(State) postfix -----------------------------------
using ChangeState_t = void (*)(void* self, int32_t state, const void* mi);
ChangeState_t o_ChangeState = nullptr;

void d_ChangeState(void* self, int32_t state, const void* mi)
{
    o_ChangeState(self, state, mi);
    hooks::guarded("GalleryPatches.ChangeState", [&] {
        switch (state) {
        case STATE_VIEW:
            if (g_previous_state == STATE_INIT) {   // first entry from Init
                g_in_gallery = true;
                g_suppress_content_change = true;
                menu_state::set_active_exclusive(menu_state::GALLERY);
                start_entry_announce();
            }
            g_previous_state = STATE_VIEW;
            break;
        case STATE_DETAILS:   // image opened
            ffpr::say(T(L"Image open"), true);
            g_previous_state = STATE_DETAILS;
            break;
        case STATE_GOTO_TITLE:   // leaving the gallery
            clear_gallery_state();
            break;
        default:
            break;
        }
    });
}

// --- GalleryTopListController.SetFocusContent(bool) postfix: list navigation --------------------
using SetFocusContent_t = void (*)(void* self, bool is_focus, const void* mi);
SetFocusContent_t o_SetFocusContent = nullptr;

void d_SetFocusContent(void* self, bool is_focus, const void* mi)
{
    o_SetFocusContent(self, is_focus, mi);
    hooks::guarded("GalleryPatches.SetFocusContent", [&] {
        if (!is_focus) return;
        if (!g_in_gallery) return;
        if (!self) return;

        if (g_suppress_content_change) {
            // Entry: cache the focus; once "Gallery" has been spoken, this read ends the entry.
            g_cached_focused = self;
            try_announce_entry_item();
            return;
        }

        int number = 0;
        std::wstring name;
        if (!read_content(self, number, name)) return;
        std::wstring entry = list_entry(number, name);
        if (!entry.empty()) ffpr::say(entry, true);
    });
}

int install()
{
    queries::define("GalleryStateTracker.IsInGallery", [] { return g_in_gallery; });

    int n = 0;
    n += hooks::attach(mChangeState, &d_ChangeState, &o_ChangeState) ? 1 : 0;
    n += hooks::attach(mSetFocusContent, &d_SetFocusContent, &o_SetFocusContent) ? 1 : 0;
    LOGF("[Gallery] %d hooks", n);
    return n;
}

core::Install s_install{"GalleryPatches", &install, 100};

} // namespace

bool is_in_gallery() { return g_in_gallery; }

void clear_gallery_state()
{
    g_in_gallery = false;
    g_suppress_content_change = false;
    g_cached_focused = nullptr;
    g_previous_state = STATE_INIT;
    menu_state::reset(menu_state::GALLERY);
}

} // namespace ffpr::extras
