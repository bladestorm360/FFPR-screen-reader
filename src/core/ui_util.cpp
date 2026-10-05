#include "ui_util.h"
#include "modtext.h"
#include "settings.h"
#include "text.h"
#include "unity.h"

namespace ui {
namespace {
ref::Class kCursor{"Last.UI", "Cursor"};
ref::Field fCursorIndex{kCursor, "<Index>k__BackingField"};

// StateMachine<T> { ... current @ 0x10 }, State<T> { Tag @ 0x10 } in all five games. Generic
// instances cannot be named for a ref, so these two stay as the C# mods' constants.
constexpr int32_t kStateMachineCurrent = 0x10;
constexpr int32_t kStateTag = 0x10;
}

std::wstring menu_position(const std::wstring& text, int index, int count)
{
    if (!settings::get_bool("MenuPositionAnnouncements", true)) return text;
    if (text.empty() || count <= 1 || index < 0 || index >= count) return text;
    return text + L", (" + TF(L"{0} of {1}", {text::num(index + 1), text::num(count)}) + L")";
}

int cursor_index(void* cursor)
{
    if (!cursor) return -1;
    return ref::rd<int32_t>(cursor, fCursorIndex);
}

int state_tag(void* controller, ref::Field& state_machine_field)
{
    void* sm = ref::rd<void*>(controller, state_machine_field);
    if (!sm) return -1;
    void* current = il2cpp::read_field<void*>(sm, kStateMachineCurrent);
    if (!current) return -1;
    return il2cpp::read_field<int32_t>(current, kStateTag);
}

void* list_item(void* list, int index) { return unity::list_at(list, index); }

} // namespace ui
