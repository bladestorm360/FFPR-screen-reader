#pragma once
// FINAL FANTASY VI: helpers shared by the FF6 status, row and party files (status_*.cpp,
// formation_row.cpp, party_setting.cpp). Main thread only.
#include <string>

namespace ffpr::ff6 {

/// The Corps (front/back row) id of the party slot at a display index, as FF4's native
/// corps_id_at: FieldController.GetCorpsListCloneWithApparentOrder(), else
/// UserDataManager.GetCorpsListClone() (FF6 CharacterSelectionReader / FormationRowPatches).
/// 1 = Front, 2 = Back, -1 when unreadable. Defined in formation_row.cpp.
int corps_id_at(int slot_index);

/// ", Front Row" / ", Back Row" for a slot index (the FF6 CharacterSelectionReader row part),
/// empty when the row is unreadable. Defined in formation_row.cpp.
std::wstring row_suffix(int slot_index);

} // namespace ffpr::ff6
