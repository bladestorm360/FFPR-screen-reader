#pragma once
#include <string>
#include "refs.h"

// Small helpers every menu reader uses (FF1's MenuPosition, StateMachineHelper,
// SelectContentHelper). Shared so the readers do not each grow their own copy.
namespace ui {

/// FF1's MenuPosition.Format: append ", (3 of 12)" when the "MenuPositionAnnouncements"
/// setting is on (default on), count > 1 and 0 <= index < count. `index` is zero-based.
std::wstring menu_position(const std::wstring& text, int index, int count);

/// Last.UI.Cursor.Index (its <Index>k__BackingField). -1 for null.
int cursor_index(void* cursor);

/// The current state tag of a controller's StateMachine<T> field: stateMachine -> current
/// (State<T>) -> Tag. -1 when anything is null. FF1's StateMachineHelper.ReadState.
int state_tag(void* controller, ref::Field& state_machine_field);

/// Element of a game List<T> by index, or null (SelectContentHelper.TryGetItem).
void* list_item(void* il2cpp_list, int index);

} // namespace ui
