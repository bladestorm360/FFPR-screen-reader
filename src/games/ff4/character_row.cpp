// Ports the FF4-only part of FF4 Menus/CharacterSelectionReader.cs: the character's row
// ("Front Row" / "Back Row") in the character slots the generic menu reader speaks (formation,
// magic and item targets, equipment...). FF4 has rows; FF1, whose reader the core ported, does
// not ("Row information removed: FF1 has no front/back row system").
//
// The core's text query "CharacterSelectionReader.TryReadCharacterSelection"
// (src/core/menus/character_selection.cpp) is redefined for FF4: it reads the core's
// announcement through menus.h and appends the row of the slot at the cursor index. The FF4 C#
// put the row after the level; here it comes last, after HP and MP.
#include <string>
#include "ff4.h"
#include "../../core/core.h"
#include "../../core/modtext.h"
#include "../../core/unity.h"
#include "../../core/menus/menus.h"
#include "../../core/system/cursor_event.h"

namespace ffpr::ff4 {
namespace {

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
    int corps = corps_id_at(ev.index);
    if (corps == 1) result += L", " + T(L"Front Row");
    else if (corps == 2) result += L", " + T(L"Back Row");
    return result;
}

int install()
{
    queries::define_text("CharacterSelectionReader.TryReadCharacterSelection", [] { return read_with_row(); });
    return 0;
}

core::Install s_install{"FF4.CharacterRow", &install, 550, game::kFF4};

} // namespace
} // namespace ffpr::ff4
