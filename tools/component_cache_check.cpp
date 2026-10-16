// check-sources: field/object_cache.cpp
// Real owner cache with fake Unity lifetimes; no engine/window is accessed.
#include "../src/core/field/object_cache.h"
#include "../src/core/field/live_context.cpp"
#include <cstdio>
#include <cstring>
namespace fixture {
struct Object { int type; bool alive = true, attached = true; void* player = nullptr; int state = 0; };
int original_calls = 0;
void native_state(void* self, int state, const void*) { ++original_calls; static_cast<Object*>(self)->state = state; }
void native_init(void* self, void* player, const void*) { ++original_calls; static_cast<Object*>(self)->player = player; }
}
namespace core { Install::Install(const char* n, int (*f)(), int o, uint8_t g) : name(n), fn(f), order(o), games(g) {} }
namespace ref {
Field::Field(Class&c,const char*n,const char*a):cls(c),name(n),alt(a){}
int32_t Field::get(){return -1;}
Class::Class(const char* n, const char* c, uint8_t g, const char* a) : ns(n), name(c), games(g), assembly(a) {}
Method::Method(Class& c, const char* n, int a) : cls(c), name(n), argc(a) {}
il2cpp::Class* Class::get() {
    uintptr_t key = 2;
    if (std::strcmp(name,"Window")==0) key=1;
    else if (std::strcmp(name,"FieldMap")==0) key=3;
    else if (std::strcmp(name,"FieldPlayerController")==0 || std::strcmp(name,"FieldPlayerKeyController")==0 || std::strcmp(name,"FieldPlayerKeyAirshipController")==0) key=4;
    return reinterpret_cast<il2cpp::Class*>(key);
}
bool Class::is(void* object) { return object && static_cast<fixture::Object*>(object)->type == reinterpret_cast<uintptr_t>(get()); }
}
namespace field::r {
ref::Class kFieldMap{"", "FieldMap"};
ref::Class kFieldPlayerController{"Last.Map", "FieldPlayerController"};
ref::Class kFieldController{"Last.Map","FieldController"};
ref::Field fFieldPlayer{kFieldPlayerController,"fieldPlayer"},fFieldController{kFieldMap,"fieldController"};
}
namespace hooks {
bool attach_raw(ref::Method& m, void*, void** original) {
    *original=std::strcmp(m.name,"StateInit")==0 ? reinterpret_cast<void*>(&fixture::native_state) : reinterpret_cast<void*>(&fixture::native_init);
    return true;
}
void note_fault(const char*) { std::abort(); }
}
namespace unity {
bool active_in_hierarchy(void*p){return p!=nullptr;}
int array_length(void*){return 0;}void* array_at(void*,int){return nullptr;}
bool alive(void* object) { return object && static_cast<fixture::Object*>(object)->alive; }
void* game_object(void* object) { return object && static_cast<fixture::Object*>(object)->attached ? object : nullptr; }
}
int main() {
    using namespace field::cache;
    ref::Class window{"UI", "Window"}, alias{"UI", "Window"};
    fixture::Object first{1}, second{1};
    int failures = 0;
    auto expect = [&](bool ok, const char* reason) { if (!ok) { ++failures; std::printf("FAIL %s\n", reason); } };
    register_object(window, &first);
    expect(get(alias) == &first, "duplicate class declarations share native owner");
    register_object(alias, &second);
    if (get(window) == &first) clear(window); // closing old owner must not clear incoming one
    expect(get(window) == &second, "old close preserves incoming controller");
    second.type = 2;
    expect(!get(window) && !has(window), "reused address with another class is retired");
    register_object(window, &first); first.alive = false;
    expect(!get(window), "destroyed controller is retired without scene scan");
    first.alive = true; second.type = 1;
    register_multiple(window, &first); register_multiple(alias, &first); register_multiple(alias, &second);
    second.attached = false;
    expect(get_all(window).size() == 1 && get_all(alias).front() == &first, "multiple current widgets retain existing inclusion/lifetime guard");
    clear_all(); expect(!get(window) && get_all(window).empty(), "explicit reset retires all owner handles");
    field::install();
    fixture::Object map{3}, key{4}, ship_player{5}, foot_player{5}, incoming_map{3}, incoming_key{4};
    // Native Transport state is 4 in all six games. A save loaded aboard has
    // no intervening GetOn event: lifecycle observers must be sufficient.
    field::d_state(&map,4,nullptr); field::d_init(&key,&ship_player,nullptr);
    expect(get(field::r::kFieldMap)==&map && get(field::r::kFieldPlayerController)==&key &&
        key.player==&ship_player && map.state==4 && fixture::original_calls==2,
        "boarded-save owner initialization needs no separate boarding event and calls originals first");
    fixture::Object flight{4};
    field::d_airship_init(&flight,&ship_player,nullptr);
    expect(get(field::r::kFieldPlayerController)==&flight && flight.player==&ship_player,
        "FF6 takeoff and loaded flight replace the walking controller with the actual airship owner");
    field::d_state(&incoming_map,3,nullptr); field::d_init(&incoming_key,&foot_player,nullptr);
    expect(get(field::r::kFieldMap)==&incoming_map && get(field::r::kFieldPlayerController)==&incoming_key && incoming_key.player==&foot_player,
        "another loaded map/player replaces prior ship owners");
    incoming_key.alive=false;
    expect(!get(field::r::kFieldPlayerController),"destroyed loaded player controller retires");
    clear_all(); field::d_init(&key,nullptr,nullptr);
    expect(!get(field::r::kFieldPlayerController),"unready native Init does not seed a field player");
    field::d_state(&first,4,nullptr);
    expect(!get(field::r::kFieldMap),"folded unrelated state receiver cannot supply a map");
    std::printf("Component cache: shared keys, replacement/lifetimes and actual loaded Transport/Player lifecycle observers: %d failures\n", failures);
    return failures ? 1 : 0;
}
