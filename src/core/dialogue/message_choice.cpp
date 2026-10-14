// A native dialogue choice has an initial focus even before its cursor is moved.
#include "../core.h"
#include "../displayed_ui.h"
#include "../frame.h"
#include "../hooks.h"
#include "../system/system.h"
#include "../ui_util.h"
#include "../unity.h"
#include "../say.h"

namespace ffpr::dialogue {
namespace {
ref::Class kChoice{"Last.UI.KeyInput", "MessageSelectController"};
ref::Field fCursor{kChoice, "selectCursor"}, fContents{kChoice, "contentList"};
ref::Method mShow{kChoice, "Show", 1}, mClose{kChoice, "Close", 0};
unsigned g_generation = 0;
using Show = void (*)(void*, int32_t, const void*);
using Close = void (*)(void*, const void*);
Show o_show;
Close o_close;
void d_show(void* self, int32_t index, const void* mi)
{
    o_show(self, index, mi);
    hooks::guarded("MessageChoice.Show", [self] {
        if (!kChoice.is(self)) return;
        const auto generation = ++g_generation;
        const int initial_index = ui::cursor_index(ref::rd<void*>(self, fCursor));
        frame::cancel_label("MessageChoice.Initial");
        // The existing cursor reader uses the same native one-frame text population deferral.
        frame::next_frame([self, generation, initial_index] {
            if (generation != g_generation || !displayed::container_visible(self)) return;
            void* cursor = ref::rd<void*>(self, fCursor);
            const int index = ui::cursor_index(cursor);
            if (index != initial_index) return; // a later native cursor event owns changed focus
            void* list = ref::rd<void*>(self, fContents);
            if (index < 0 || index >= unity::list_count(list)) return;
            void* content = unity::list_at(list, index);
            const auto value = displayed::text_field(displayed::field(content, "view"), "text");
            if (!value.empty()) ffpr::say(ui::menu_position(value, index, unity::list_count(list)), false);
        }, "MessageChoice.Initial");
    });
}
void d_close(void* self, const void* mi)
{
    if (kChoice.is(self)) { ++g_generation; frame::cancel_label("MessageChoice.Initial"); }
    o_close(self, mi);
}
int install()
{
    return (hooks::attach(mShow, &d_show, &o_show) ? 1 : 0) +
        (hooks::attach(mClose, &d_close, &o_close) ? 1 : 0);
}
core::Install s_install{"MessageChoice", &install, 101};
}
}
