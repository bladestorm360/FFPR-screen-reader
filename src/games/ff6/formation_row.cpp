// Ports FF6 Patches/FormationRowPatches.cs (StatusWindowController.SwitchCorps prefix: the row the
// character is about to move to) and the FF6-only part of Menus/CharacterSelectionReader.cs (the
// character's row, "Front Row" / "Back Row", in the character slots the generic menu reader
// speaks). FF6 has rows; FF1, whose reader the core ported, does not.
//
// The core's text query "CharacterSelectionReader.TryReadCharacterSelection"
// (src/core/menus/character_selection.cpp) is redefined for FF6, as FF4 does
// (src/games/ff4/character_row.cpp): it reads the core's announcement through menus.h and appends
// the row of the slot at the cursor index. The FF6 C# put the row after the level; here it comes
// last, after HP and MP (FF4's native order).
//
// Row switching happens in the Status menu's character list (STATUS_MENU, already suppressing
// the generic cursor reader), so SwitchCorps needs no cursor suppressor of its own.
// SwitchCorps' body is not folded in FF6 (RVA 0x90D370, 1 method).
#include <string>
#include "status.h"
#include "../../core/core.h"
#include "../../core/hooks.h"
#include "../../core/log.h"
#include "../../core/modtext.h"
#include "../../core/refs.h"
#include "../../core/say.h"
#include "../../core/text.h"
#include "../../core/ui_util.h"
#include "../../core/unity.h"
#include "../../core/menus/menus.h"
#include "../../core/system/cursor_event.h"

namespace ffpr::ff6 {
namespace {

constexpr uint8_t k6 = game::kFF6;

ref::Class  kStatusWindow{"Last.UI.KeyInput", "StatusWindowController", k6};
ref::Method mSwitchCorps{kStatusWindow, "SwitchCorps", 0};
ref::Class  kStatusWindowBase{"Serial.Template.UI.KeyInput", "StatusWindowControllerBase", k6};
ref::Field  fStatusSelectCursor{kStatusWindowBase, "selectCursor"};
ref::Class  kUserDataManager{"Last.Management", "UserDataManager", k6};
ref::Method mUserDataInstance{kUserDataManager, "Instance", 0};
ref::Method mGetCorpsListClone{kUserDataManager, "GetCorpsListClone", 0};
ref::Class  kFieldController{"Last.Map", "FieldController", k6};
ref::Method mCorpsApparentOrder{kFieldController, "GetCorpsListCloneWithApparentOrder", 0};
ref::Class  kCorps{"Last.Data.User", "Corps", k6};
ref::Method mCorpsId{kCorps, "get_Id", 0};

// CorpsId (Last.Defaine.User)
constexpr int kCorpsFront = 1;
constexpr int kCorpsBack = 2;

// StatusWindowController_SwitchCorps_Patch.Prefix: announce the row the character moves TO.
using Void_t = void (*)(void* self, const void* mi);
Void_t o_SwitchCorps;
void d_SwitchCorps(void* self, const void* mi)
{
    hooks::guarded("FF6.StatusWindowController.SwitchCorps", [&] {
        void* cursor = ref::rd<void*>(self, fStatusSelectCursor);
        if (!cursor) {
            LOGF("[FF6 Row] selectCursor is null");
            return;
        }
        int index = ui::cursor_index(cursor);
        if (!ref::call_static<void*>(mUserDataInstance)) {
            LOGF("[FF6 Row] UserDataManager is null");
            return;
        }
        int id = corps_id_at(index);
        if (id < 0) {
            LOGF("[FF6 Row] Invalid corps list or index: %d", index);
            return;
        }
        std::wstring new_row;
        if (id == kCorpsFront) new_row = T(L"Back Row");
        else if (id == kCorpsBack) new_row = T(L"Front Row");
        else new_row = TF(L"Unknown Row Type {0}", {text::num(id)});
        ffpr::say(new_row, true);
    });
    o_SwitchCorps(self, mi);   // prefix: the game switches after the announcement
}

// CharacterSelectionReader.TryReadCharacterSelection with FF6's row.
std::wstring read_with_row()
{
    const cursor::CursorEvent& ev = cursor::current();
    ev.out_count = -1;
    if (!ev.cursor || !unity::alive(ev.cursor)) return {};
    void* t = unity::alive(ev.transform) ? ev.transform : unity::transform(ev.cursor);
    int count = -1;
    std::wstring result = ffpr::menus::try_read_character_selection(t, ev.index, count);
    ev.out_count = count;
    if (result.empty()) return result;
    return result + row_suffix(ev.index);
}

int install()
{
    int n = hooks::attach(mSwitchCorps, &d_SwitchCorps, &o_SwitchCorps) ? 1 : 0;
    queries::define_text("CharacterSelectionReader.TryReadCharacterSelection", [] { return read_with_row(); });
    return n;
}

core::Install s_install{"FF6.FormationRow", &install, 550, game::kFF6};

} // namespace

// FF4's corps_id_at (src/games/ff4/party_setting.cpp): the display order first.
int corps_id_at(int slot_index)
{
    void* corps_list = ref::call_static<void*>(mCorpsApparentOrder);
    if (!corps_list) {
        // Fallback to data order (the FF6 C#'s list) when FieldController is not ready.
        void* udm = ref::call_static<void*>(mUserDataInstance);
        if (udm) corps_list = ref::call<void*>(mGetCorpsListClone, udm);
    }
    if (!corps_list || slot_index < 0 || slot_index >= unity::list_count(corps_list)) return -1;
    void* corps = unity::list_at(corps_list, slot_index);
    return corps ? ref::call<int32_t>(mCorpsId, corps) : -1;
}

std::wstring row_suffix(int slot_index)
{
    int id = corps_id_at(slot_index);
    if (id == kCorpsFront) return L", " + T(L"Front Row");
    if (id == kCorpsBack) return L", " + T(L"Back Row");
    return {};
}

} // namespace ffpr::ff6
