// Observe ready native widget owners; never discover them through scene scans.
#include "../core.h"
#include "../hooks.h"
#include "../field/object_cache.h"
namespace ui {
namespace {
constexpr auto games = game::kFF4 | game::kFF5 | game::kFF6;
ref::Class kScreen{"Last.UI", "ScreenTimerController", games};
ref::Class kField{"Last.UI.Common.Map", "FieldGrobalTimer", games};
ref::Method mScreen{kScreen, "Initialize", 0}, mField{kField, "Init", 0};
using Init = void (*)(void*, const void*);
Init o_screen, o_field;
void d_screen(void* self, const void* mi) {
    o_screen(self, mi);
    hooks::guarded("Timer.ScreenOwner", [self] {
        if (kScreen.is(self)) field::cache::register_multiple(kScreen, self);
    });
}
void d_field(void* self, const void* mi) {
    o_field(self, mi);
    hooks::guarded("Timer.FieldOwner", [self] {
        if (kField.is(self)) field::cache::register_multiple(kField, self);
    });
}
int install() {
    return (hooks::attach(mScreen, &d_screen, &o_screen) ? 1 : 0) +
        (hooks::attach(mField, &d_field, &o_field) ? 1 : 0);
}
core::Install s_install{"UI.TimerOwners", &install, 105, games};
}
}
