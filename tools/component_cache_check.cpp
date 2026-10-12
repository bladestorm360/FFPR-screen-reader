// check-sources: field/object_cache.cpp
// Real owner cache with fake Unity lifetimes; no engine/window is accessed.
#include "../src/core/field/object_cache.h"
#include <cstdio>
#include <cstring>
namespace fixture { struct Object { int type; bool alive = true, attached = true; }; }
namespace ref {
Class::Class(const char* n, const char* c, uint8_t g, const char* a) : ns(n), name(c), games(g), assembly(a) {}
il2cpp::Class* Class::get() { return reinterpret_cast<il2cpp::Class*>(std::strcmp(name, "Window") == 0 ? 1 : 2); }
bool Class::is(void* object) { return object && static_cast<fixture::Object*>(object)->type == reinterpret_cast<uintptr_t>(get()); }
}
namespace unity {
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
    std::printf("Component cache: shared keys, incoming ownership, reuse/destroyed/widget lifetimes: %d failures\n", failures);
    return failures ? 1 : 0;
}
