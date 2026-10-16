// Native-owned field instances: observe ready lifecycle data, never scan a scene.
#include "field_internal.h"
#include "object_cache.h"
#include "../core.h"
#include "../hooks.h"
namespace field {
namespace {
ref::Field fRootPlayer{r::kFieldController,"player"};
ref::Field fControllers{r::kFieldController,"<FieldPlayerControllerList>k__BackingField"};
ref::Field fEnabled{r::kFieldPlayerController,"<InputEnable>k__BackingField"};

ref::Method mStateInit{r::kFieldMap, "StateInit", 1};
ref::Class kKey{"Last.Map", "FieldPlayerKeyController"};
ref::Method mInit{kKey, "Init", 1};
ref::Class kAirship{"Last.Map", "FieldPlayerKeyAirshipController"};
ref::Method mAirshipInit{kAirship, "Init", 1};
ref::Method mSwitch{r::kFieldMap, "OnSwitchPlayerController", 1};
using State = void (*)(void*, int32_t, const void*);
using Init = void (*)(void*, void*, const void*);
State o_state; Init o_init, o_airship_init, o_switch;
void seed_controller(void* self,void* player) {
    void* current=cache::get(r::kFieldPlayerController);
    if(!current || !unity::active_in_hierarchy(current) || !ref::rd<bool>(current,fEnabled) || ref::rd<void*>(current,r::fFieldPlayer)!=player)
        cache::register_object(r::kFieldPlayerController,self);
}
void d_switch(void* self, void* current, const void* mi) {
    o_switch(self,current,mi);
    hooks::guarded("Field.SwitchController", [self,current] {
        if(r::kFieldMap.is(self))cache::register_object(r::kFieldMap,self);
        if(current && r::kFieldPlayerController.is(current))cache::register_object(r::kFieldPlayerController,current);
    });
}
void d_state(void* self, int32_t state, const void* mi) {
    o_state(self, state, mi);
    hooks::guarded("Field.Context", [self] {
        if (r::kFieldMap.is(self)) cache::register_object(r::kFieldMap, self);
    });
}
void d_init(void* self, void* player, const void* mi) {
    o_init(self, player, mi);
    hooks::guarded("Field.PlayerContext", [self, player] {
        if (player && kKey.is(self)) seed_controller(self,player);
    });
}
void d_airship_init(void* self, void* player, const void* mi) {
    o_airship_init(self, player, mi);
    hooks::guarded("Field.AirshipContext", [self, player] {
        if (player && kAirship.is(self)) seed_controller(self,player);
    });
}
int install() {
    return (hooks::attach(mStateInit, &d_state, &o_state) ? 1 : 0) +
           (hooks::attach(mInit, &d_init, &o_init) ? 1 : 0) +
           (hooks::attach(mAirshipInit, &d_airship_init, &o_airship_init) ? 1 : 0) +
           (hooks::attach(mSwitch, &d_switch, &o_switch) ? 1 : 0);
}
core::Install s_install{"Field.LiveContext", &install, 105};
}
void* player_controller() {
    void* map=cache::get(r::kFieldMap);
    void* root=map?ref::rd<void*>(map,r::fFieldController):nullptr;
    void* current=root?ref::rd<void*>(root,fRootPlayer):nullptr;
    auto valid=[current](void* c) { return c && unity::alive(c) && unity::active_in_hierarchy(c)
        && ref::rd<bool>(c,fEnabled) && (!current || ref::rd<void*>(c,r::fFieldPlayer)==current); };
    void* cached=cache::get(r::kFieldPlayerController);
    if(valid(cached))return cached;
    // Native-owned controller list only. No scene scan, allocation or worker wait.
    void* list=root?ref::rd<void*>(root,fControllers):nullptr;
    const int count=unity::array_length(list);
    if(count>0 && count<=32)for(int i=0;i<count;++i) {
        void* c=unity::array_at(list,i);
        if(valid(c)) {cache::register_object(r::kFieldPlayerController,c);return c;}
    }
    return nullptr;
}
void* current_native_player() {
    if(void* map=cache::get(r::kFieldMap))
        if(void* root=ref::rd<void*>(map,r::fFieldController))
            if(void* p=ref::rd<void*>(root,fRootPlayer))return p;
    if(void* c=player_controller())return ref::rd<void*>(c,r::fFieldPlayer);
    return nullptr;
}
}
