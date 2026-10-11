// Ports FF6 Patches/ColiseumPatches.cs (FF6ScreenReader, by ZKline): the Coliseum
// (Last.UI.KeyInput.ColosseumController, base Last.UI.ColosseumControllerBase).
//
//   * SelectContent(int, WithinRangeType): the wager list.
//   * ItemConfirmPopupInit: one frame later, "Wager X, Win Y, Fight Z" (the wagered item, the
//     reward from GetRewardItemName and the monster from GetBattleMonsterInfo).
//   * SelectChallengerInit: one frame later, the "Choose a combatant" prompt and the focused
//     character's name; SelectChallenger(int): the focused character's name.
// The C# read ColosseumController.view (0x70), ColosseumView.characterNameText (0xB8) and
// challengerSelectText (0xD8) by raw offsets; here they are read by name.
//
// The game content is the C#'s; the mod-side conventions are the core's (FF1's) item list
// (src/core/menus/item_menu.cpp), at the lead's request (2026-10-07):
//   * a wager row is "Name, count (when > 1)", then ": description" only with AutoDetail on
//     (the C# always said "Name, description"), then ", (n of N)";
//   * the challenger name ends with ", (n of N)" (n from SelectChallenger's index or the
//     challenger cursor, N = the shown challenger entries);
//   * the list speaks its focused row when it gains focus (SelectItemInit, a bounded read like
//     the core's FieldItemReannouncePatches); the C# was silent until the cursor moved;
//   * the I key reads the screen's description panel (ColosseumView.descriptionText), as the
//     core's item menu does: published as "ColiseumPatches.HasDetails" / "ColiseumPatches.Details"
//     for the module's details-key entry.
//
// Repeat guards, moved to dedup:: from the C#:
//   * the wager list's lastItemAnnouncement (text) -> "FF6.Coliseum.Item", index + text (identity
//     first). The C# never reset it; it is reset when the list gains focus (SelectItemInit), as
//     the core's lists reset their guards on (re)entry;
//   * lastChallengerIndex (index; reset by SelectChallengerInit, set to the focused index when
//     the init line named a character) -> "FF6.Coliseum.Challenger".
//
// The C# CursorNavigationPatches skipped every cursor under a "colosseum"/"Colosseum" object;
// the same check is the cursor suppressor "ColiseumPatches.ShouldSuppress".
//
// Menu state "Coliseum" (core.h menu_state), as the core's menus set theirs: set by every
// Coliseum hook, cleared by ColosseumController.Close, a map change and a battle start. While it
// is set the field keys are silent (the input area's Field context needs no active menu state),
// as in every core menu; the C# had no such gate.
//
// Every hooked body has one RVA in FF6's dump.cs.
#include "../../core/ui/preferences.h"
#include <cstdint>
#include <string>
#include "../../core/core.h"
#include "../../core/dedup.h"
#include "../../core/frame.h"
#include "../../core/hooks.h"
#include "../../core/log.h"
#include "../../core/modtext.h"
#include "../../core/refs.h"
#include "../../core/say.h"
#include "../../core/settings.h"
#include "../../core/text.h"
#include "../../core/ui_util.h"
#include "../../core/unity.h"
#include "../../core/system/cursor_event.h"

namespace ffpr::ff6 {
namespace {

constexpr uint8_t k6 = game::kFF6;

ref::Class  kColosseumBase{"Last.UI", "ColosseumControllerBase", k6};
ref::Field  fColosseumState{kColosseumBase, "colosseumState"};
ref::Field  fContentDataList{kColosseumBase, "contentDataList"};
ref::Field  fSelectItemData{kColosseumBase, "selectItemData"};
ref::Field  fSelectContentCursor{kColosseumBase, "selectContentCursor"};
ref::Field  fChallengerSelectCursor{kColosseumBase, "challengerSelectCursor"};
ref::Method mGetRewardItemName{kColosseumBase, "GetRewardItemName", 1};
ref::Method mGetBattleMonsterInfo{kColosseumBase, "GetBattleMonsterInfo", 0};

ref::Class  kColosseum{"Last.UI.KeyInput", "ColosseumController", k6};
ref::Field  fView{kColosseum, "view"};
ref::Method mSelectContent{kColosseum, "SelectContent", 2};
ref::Method mSelectItemInit{kColosseum, "SelectItemInit", 0};
ref::Method mSelectChallenger{kColosseum, "SelectChallenger", 1};
ref::Method mSelectChallengerInit{kColosseum, "SelectChallengerInit", 0};
ref::Method mItemConfirmPopupInit{kColosseum, "ItemConfirmPopupInit", 0};
ref::Method mClose{kColosseum, "Close", 0};

ref::Class  kColosseumView{"Last.UI.KeyInput", "ColosseumView", k6};
ref::Field  fViewDescriptionText{kColosseumView, "descriptionText"};
ref::Field  fCharacterNameText{kColosseumView, "characterNameText"};
ref::Field  fChallengerSelectText{kColosseumView, "challengerSelectText"};
ref::Field  fChallengerList{kColosseumView, "challengerList"};

ref::Class  kItemListContentData{"Last.UI", "ItemListContentData", k6};
ref::Field  fDataName{kItemListContentData, "<Name>k__BackingField"};
ref::Field  fDataDescription{kItemListContentData, "<Description>k__BackingField"};
ref::Field  fDataCount{kItemListContentData, "<Count>k__BackingField"};

constexpr const char* kCtxItem = "FF6.Coliseum.Item";               // lastItemAnnouncement
constexpr const char* kCtxChallenger = "FF6.Coliseum.Challenger";   // lastChallengerIndex
constexpr const char* kMenuKey = "Coliseum";                        // menu_state key

// ColosseumControllerBase.ColosseumState.
constexpr int COLOSSEUM_STATE_ITEM_SELECT = 0;

// The core's bounded list read: one try per frame, at most this many frames.
constexpr int kMaxEntryFrames = 30;

// ColosseumControllerBase.MonsterInfo (a struct: Name, MonsterImage, ShadowImage, PartyId).
// 32 bytes, so IL2CPP returns it through a hidden pointer, as MSVC does for this type.
struct MonsterInfo {
    void* name;
    void* monster_image;
    void* shadow_image;
    int32_t party_id;
};

// The controller of the last Coliseum hook (re-checked with live() before use): the I key's.
void* g_controller = nullptr;
// A newer SelectItemInit supersedes a pending entry read.
int g_entry_gen = 0;

/// TextUtils.StripIconMarkup.
std::wstring strip_icon(const std::wstring& s)
{
    if (s.empty()) return {};
    return text::trim(text::strip_markup(s));
}

bool live(void* controller)
{
    return controller && unity::alive(controller) && kColosseum.is(controller);
}

/// A Coliseum hook ran: remember its controller and claim the menu state.
void activate(void* controller)
{
    g_controller = controller;
    menu_state::set_active(kMenuKey, true);
}

void deactivate()
{
    ++g_entry_gen;   // drop a pending entry read
    g_controller = nullptr;
    menu_state::reset(kMenuKey);
}

void* view_of(void* controller) { return ref::rd<void*>(controller, fView); }

/// A Text of the controller's ColosseumView.
std::wstring view_text(void* controller, ref::Field& text_field)
{
    void* view = view_of(controller);
    if (!view) return {};
    void* t = ref::rd<void*>(view, text_field);
    return t ? unity::ui_text(t) : std::wstring();
}

// --- the wager list -------------------------------------------------------------------------

/// One wager row, as the core's item list words it; false when nothing was spoken.
bool announce_row(void* controller, int index)
{
    void* list = ref::rd<void*>(controller, fContentDataList);
    int count = unity::list_count(list);
    if (count <= 0 || index < 0 || index >= count) return false;
    void* item = unity::list_at(list, index);
    if (!item) return false;

    std::wstring name = ref::rd_str(item, fDataName);
    if (name.empty()) return false;
    name = strip_icon(name);
    if (name.empty()) return false;

    std::wstring base = name;
    int c = ref::rd<int32_t>(item, fDataCount);
    if (c > 1) base += L", " + text::num(c);

    std::wstring announcement = base;
    if (ui::prefs::auto_detail_enabled()) {
        std::wstring description = strip_icon(ref::rd_str(item, fDataDescription));
        if (!text::is_blank(description)) announcement = base + L": " + description;
    }

    if (!dedup::index_text(kCtxItem, index, announcement, dedup::Scope::Session)) return false;
    LOGF("[Coliseum] %ls", announcement.c_str());
    ffpr::say(ui::menu_position(announcement, index, count), true);
    return true;
}

// SelectContent(int index, WithinRangeType scrollType) postfix.
using SelectContent_t = void (*)(void* self, int32_t index, int32_t scroll_type, const void* mi);
SelectContent_t o_SelectContent = nullptr;

void d_SelectContent(void* self, int32_t index, int32_t scroll_type, const void* mi)
{
    o_SelectContent(self, index, scroll_type, mi);
    hooks::guarded("ColiseumPatches.SelectContent", [&] {
        if (!self) return;
        g_controller = self;
        announce_row(self, index);
    });
}

/// The focused row once the list is built and shown (one try per frame).
bool try_entry_read(void* controller)
{
    if (!unity::active_in_hierarchy(controller)) return false;
    if (ref::rd<int32_t>(controller, fColosseumState) != COLOSSEUM_STATE_ITEM_SELECT) return false;
    if (unity::list_count(ref::rd<void*>(controller, fContentDataList)) <= 0) return false;
    int index = ui::cursor_index(ref::rd<void*>(controller, fSelectContentCursor));
    if (index < 0) return false;
    announce_row(controller, index);   // a row the game's own SelectContent already spoke stays silent
    return true;
}

using Init_t = void (*)(void* self, const void* mi);
Init_t o_SelectItemInit = nullptr;

// SelectItemInit: the wager list gains focus. The repeat guard resets first, so a SelectContent
// the game makes inside it speaks; the bounded read then speaks the row if nothing did.
void d_SelectItemInit(void* self, const void* mi)
{
    hooks::guarded("ColiseumPatches.SelectItemInit.Reset", [] { dedup::reset(kCtxItem); });
    o_SelectItemInit(self, mi);
    hooks::guarded("ColiseumPatches.SelectItemInit", [&] {
        if (!self) return;
        g_controller = self;
        const int gen = ++g_entry_gen;
        frame::every_frame([self, gen]() -> bool {
            if (gen != g_entry_gen || !live(self)) return false;
            return !hooks::guarded_r("ColiseumPatches.EntryRead", true, [&] { return try_entry_read(self); });
        }, "FF6.Coliseum.EntryRead", kMaxEntryFrames);
    });
}

// --- ItemConfirmPopupInit postfix: the wager confirmation, one frame later --------------------
void announce_confirmation(void* controller)
{
    std::wstring wager_name, reward_name, monster_name;

    if (void* select_item = ref::rd<void*>(controller, fSelectItemData)) {
        wager_name = strip_icon(ref::rd_str(select_item, fDataName));
        std::wstring reward = hooks::guarded_r("ColiseumPatches.GetRewardItemName", std::wstring(), [&] {
            return ref::call_str(mGetRewardItemName, controller, select_item);
        });
        if (!reward.empty()) reward_name = strip_icon(reward);
    }

    MonsterInfo info = hooks::guarded_r("ColiseumPatches.GetBattleMonsterInfo", MonsterInfo{}, [&] {
        return ref::call<MonsterInfo>(mGetBattleMonsterInfo, controller);
    });
    monster_name = ref::str(info.name);

    std::wstring announcement;
    if (!wager_name.empty()) announcement += TF(L"Wager {0}", {wager_name});
    if (!reward_name.empty()) announcement += TF(L", Win {0}", {reward_name});
    if (!monster_name.empty()) announcement += TF(L", Fight {0}", {monster_name});
    if (announcement.empty()) return;

    LOGF("[Coliseum Confirm] %ls", announcement.c_str());
    ffpr::say(announcement, true);
}

Init_t o_ItemConfirmPopupInit = nullptr;

void d_ItemConfirmPopupInit(void* self, const void* mi)
{
    o_ItemConfirmPopupInit(self, mi);
    hooks::guarded("ColiseumPatches.ItemConfirmPopupInit", [&] {
        if (!self) return;
        g_controller = self;
        frame::next_frame([self] {
            hooks::guarded("ColiseumPatches.DelayedAnnounceConfirmation", [&] {
                if (live(self)) announce_confirmation(self);
            });
        }, "FF6.Coliseum.Confirm");
    });
}

// --- the challenger list --------------------------------------------------------------------

/// The shown challenger entries (the view's list is a pool).
int challenger_count(void* controller)
{
    void* view = view_of(controller);
    void* list = view ? ref::rd<void*>(view, fChallengerList) : nullptr;
    int n = unity::list_count(list);
    int shown = 0;
    for (int i = 0; i < n; ++i) {
        void* entry = unity::list_at(list, i);
        if (entry && unity::active_in_hierarchy(entry)) ++shown;
    }
    return shown;
}

/// The focused character's name with ", (n of N)", or empty.
std::wstring challenger_line(void* controller, int index)
{
    std::wstring name = view_text(controller, fCharacterNameText);
    if (text::is_blank(name)) return {};
    return ui::menu_position(strip_icon(text::trim(name)), index, challenger_count(controller));
}

// SelectChallengerInit postfix: prompt and focused character, one frame later.
void announce_challenger_init(void* controller)
{
    std::wstring prompt = view_text(controller, fChallengerSelectText);
    int index = ui::cursor_index(ref::rd<void*>(controller, fChallengerSelectCursor));
    if (index < 0) index = 0;   // the C# assumed the first character
    std::wstring character = challenger_line(controller, index);

    std::wstring announcement;
    if (!text::is_blank(prompt)) announcement = strip_icon(text::trim(prompt));
    if (!character.empty()) {
        announcement = announcement.empty() ? character : announcement + L" " + character;
        dedup::index(kCtxChallenger, index, dedup::Scope::Session);   // lastChallengerIndex
    }
    if (announcement.empty()) return;

    LOGF("[Coliseum Challenger] %ls", announcement.c_str());
    ffpr::say(announcement, true);
}

Init_t o_SelectChallengerInit = nullptr;

void d_SelectChallengerInit(void* self, const void* mi)
{
    o_SelectChallengerInit(self, mi);
    hooks::guarded("ColiseumPatches.SelectChallengerInit", [&] {
        dedup::reset(kCtxChallenger);   // lastChallengerIndex = -1
        if (!self) return;
        g_controller = self;
        frame::next_frame([self] {
            hooks::guarded("ColiseumPatches.DelayedAnnounceChallengerInit", [&] {
                if (live(self)) announce_challenger_init(self);
            });
        }, "FF6.Coliseum.ChallengerInit");
    });
}

// SelectChallenger(int index) postfix: the focused character.
using SelectChallenger_t = void (*)(void* self, int32_t index, const void* mi);
SelectChallenger_t o_SelectChallenger = nullptr;

void d_SelectChallenger(void* self, int32_t index, const void* mi)
{
    o_SelectChallenger(self, index, mi);
    hooks::guarded("ColiseumPatches.SelectChallenger", [&] {
        if (!dedup::index(kCtxChallenger, index, dedup::Scope::Session)) return;
        if (!self) return;
        g_controller = self;
        std::wstring line = challenger_line(self, index);
        if (line.empty()) return;
        LOGF("[Coliseum Challenger] %ls", line.c_str());
        ffpr::say(line, true);
    });
}

// --- the I key and the generic cursor reader ------------------------------------------------

/// The wager list is on screen (the I key reads its description panel).
bool has_details()
{
    if (!live(g_controller)) {
        g_controller = nullptr;
        return false;
    }
    return unity::active_in_hierarchy(g_controller) &&
           ref::rd<int32_t>(g_controller, fColosseumState) == COLOSSEUM_STATE_ITEM_SELECT;
}

/// The description panel's text, as the core's ItemDescriptionAnnouncer reads its panel.
std::wstring details()
{
    if (!has_details()) return {};
    return strip_icon(view_text(g_controller, fViewDescriptionText));
}

/// CursorNavigationPatches' Coliseum rule: a cursor below a "colosseum"/"Colosseum" object.
bool should_suppress()
{
    const cursor::CursorEvent& ev = cursor::current();
    if (!ev.cursor || !unity::alive(ev.cursor)) return false;
    void* t = unity::alive(ev.transform) ? ev.transform : unity::transform(ev.cursor);
    void* parent = t ? unity::parent(t) : nullptr;
    for (int depth = 0; parent && depth < 64; ++depth) {
        std::wstring name = unity::name(parent);
        if (text::contains(name, L"colosseum") || text::contains(name, L"Colosseum")) return true;
        parent = unity::parent(parent);
    }
    return false;
}

int install()
{
    queries::define("ColiseumPatches.ShouldSuppress", [] { return should_suppress(); });
    cursor::add_suppressor("ColiseumPatches.ShouldSuppress");
    // For the module's "GameModule.HasDetails" / "GameModule.Details" (the core's I-key entry).
    queries::define("ColiseumPatches.HasDetails", [] { return has_details(); });
    queries::define_text("ColiseumPatches.Details", [] { return details(); });

    int n = 0;
    n += hooks::attach(mSelectContent, &d_SelectContent, &o_SelectContent) ? 1 : 0;
    n += hooks::attach(mSelectItemInit, &d_SelectItemInit, &o_SelectItemInit) ? 1 : 0;
    n += hooks::attach(mItemConfirmPopupInit, &d_ItemConfirmPopupInit, &o_ItemConfirmPopupInit) ? 1 : 0;
    n += hooks::attach(mSelectChallengerInit, &d_SelectChallengerInit, &o_SelectChallengerInit) ? 1 : 0;
    n += hooks::attach(mSelectChallenger, &d_SelectChallenger, &o_SelectChallenger) ? 1 : 0;
    LOGF("[Coliseum] %d coliseum hooks", n);
    return n;
}

core::Install s_install{"FF6.Coliseum", &install, 530, game::kFF6};

} // namespace
} // namespace ffpr::ff6
