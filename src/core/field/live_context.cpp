// Native-owned field instances: observe ready lifecycle data, never scan a scene.
#include "field_internal.h"
#include "object_cache.h"
#include "../core.h"
#include "../hooks.h"
namespace field {
namespace {
ref::Method mStateInit{r::kFieldMap, "StateInit", 1};
ref::Class kKey{"Last.Map", "FieldPlayerKeyController"};
ref::Method mInit{kKey, "Init", 1};
using State = void (*)(void*, int32_t, const void*);
using Init = void (*)(void*, void*, const void*);
State o_state; Init o_init;
void d_state(void* self, int32_t state, const void* mi) {
    o_state(self, state, mi);
    hooks::guarded("Field.Context", [self] {
        if (r::kFieldMap.is(self)) cache::register_object(r::kFieldMap, self);
    });
}
void d_init(void* self, void* player, const void* mi) {
    o_init(self, player, mi);
    hooks::guarded("Field.PlayerContext", [self, player] {
        if (player && kKey.is(self)) cache::register_object(r::kFieldPlayerController, self);
    });
}
int install() {
    return (hooks::attach(mStateInit, &d_state, &o_state) ? 1 : 0) +
           (hooks::attach(mInit, &d_init, &o_init) ? 1 : 0);
}
core::Install s_install{"Field.LiveContext", &install, 105};
}
}
