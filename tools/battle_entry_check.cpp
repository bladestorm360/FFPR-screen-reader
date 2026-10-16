// Actual rare native-entry detours: ABI forwarding and original-before-cleanup.
#include "../src/core/battle/battle_start.cpp"
#include <cstdio>
#include <cstdlib>
namespace fixture {
int native = 0, ready = 0;
bool opening_ran = false, forwarded = true;
void* self = reinterpret_cast<void*>(0x1234);
const void* method = reinterpret_cast<void*>(0x5678);
void original(void* object, const void* mi) {
    forwarded &= object == self && mi == method;
    ++native; opening_ran = true;
}
void encounter(void* object, void* entity, void* texture, int32_t effect,
               bool visible, bool bgm, void* callback, const void* mi) {
    forwarded &= object == self && entity == reinterpret_cast<void*>(1) && texture == reinterpret_cast<void*>(2) &&
        effect == 7 && visible && !bgm && callback == reinterpret_cast<void*>(3) && mi == method;
    ++native;
}
}
namespace core { Install::Install(const char* n, int (*f)(), int o, uint8_t g) : name(n), fn(f), order(o), games(g) {} }
namespace ref {
Class::Class(const char* n, const char* c, uint8_t g, const char* a) : ns(n), name(c), games(g), assembly(a) {}
Method::Method(Class& c, const char* n, int a) : cls(c), name(n), argc(a) {}
}
namespace hooks { bool attach_raw(ref::Method&, void*, void**) { return false; } void note_fault(const char*) { std::abort(); } }
namespace ffpr::battle {
void on_battle_start() {
    // A misplaced prefix would fail before it could intercept native entrance audio.
    if (!fixture::opening_ran) std::abort();
    ++fixture::ready;
}
}
int main()
{
    using namespace ffpr::battle;
    o_StartPreeMptiveMes = &fixture::original;
    for (auto& original : o_stage) original = &fixture::original;
    o_encounter = &fixture::encounter;
    for (int game = 1; game <= 6; ++game) {
        d_encounter(fixture::self, reinterpret_cast<void*>(1), reinterpret_cast<void*>(2), 7, true, false,
                    reinterpret_cast<void*>(3), fixture::method);
        d_stage<0>(fixture::self, fixture::method);
        d_stage<1>(fixture::self, fixture::method);
        d_stage<2>(fixture::self, fixture::method);
        fixture::opening_ran = false;
        d_StartPreeMptiveMes(fixture::self, fixture::method);
    }
    const bool ok = fixture::forwarded && fixture::native == 30 && fixture::ready == 6;
    std::printf("%s: opening presentation precedes cleanup; rare stage/encounter ABI forwards all arguments and calls every original once\n", ok ? "PASS" : "FAIL");
    return ok ? 0 : 1;
}
