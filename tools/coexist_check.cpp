// check-sources: +minhook
// Offline check: the built-in MinHook stacks with another mod's inline hook (2026-10-07).
//
// MelonLoader 0.7 (Dobby) and BepInEx 6 (Dobby, funchook) hook game functions too. FF1 crashed
// with MelonLoader on 2026-10-06 because MinHook resumed its trampoline at byte 5 and Dobby's
// 6-byte jump then covered that byte. This builds small x64 functions in memory, hooks them
// with our MinHook, puts a simulated foreign hook on top (or underneath), calls them and checks
// the result of the whole chain - in both orders, and through both kinds of unhooking.
//
// The foreign hooker is simulated the way those libraries patch: it overwrites the start of
// the function with its jump (6-byte "JMP [RIP+x]" to a pointer slot like Dobby, or a 14-byte
// "JMP [RIP+0] + address" like MonoMod/funchook), relocates the instructions it covered into
// its own trampoline, and restores its saved bytes when it unhooks.
#include <windows.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include "MinHook.h"

namespace {

int g_failures = 0;

void check(bool ok, const char* what)
{
    std::printf("  %s %s\n", ok ? "ok  " : "FAIL", what);
    if (!ok) ++g_failures;
}

using Fn = int (*)(int);

// f(x) = x + 7 with a real prologue whose first instruction ends at byte 5, the case that
// crashed: mov [rsp+8],rbx (5) / mov [rsp+10h],rsi (5) / push rdi / sub rsp,20h /
// lea eax,[rcx+7] / add rsp,20h / pop rdi / mov rsi,[rsp+10h] / mov rbx,[rsp+8] / ret
const unsigned char kProlog5[] = {
    0x48, 0x89, 0x5C, 0x24, 0x08, 0x48, 0x89, 0x74, 0x24, 0x10, 0x57, 0x48, 0x83, 0xEC, 0x20,
    0x8D, 0x41, 0x07, 0x48, 0x83, 0xC4, 0x20, 0x5F, 0x48, 0x8B, 0x74, 0x24, 0x10, 0x48, 0x8B,
    0x5C, 0x24, 0x08, 0xC3,
};
// A getter-sized function: lea eax,[rcx+7] / ret, then int3 padding as the compiler emits.
const unsigned char kTiny[] = {0x8D, 0x41, 0x07, 0xC3};

unsigned char* g_page = nullptr;   // RWX test page: functions, foreign trampolines, slots

unsigned char* place(size_t offset, const unsigned char* code, size_t n)
{
    unsigned char* p = g_page + offset;
    std::memset(p, 0xCC, 64);
    std::memcpy(p, code, n);
    FlushInstructionCache(GetCurrentProcess(), p, 64);
    return p;
}

// --- our hook: result * 2 ------------------------------------------------------------------
Fn o_ours[4] = {};
int d_ours0(int x) { return o_ours[0](x) * 2; }
int d_ours1(int x) { return o_ours[1](x) * 2; }
int d_ours2(int x) { return o_ours[2](x) * 2; }
int d_ours3(int x) { return o_ours[3](x) * 2; }
void* const kOurs[4] = {reinterpret_cast<void*>(&d_ours0), reinterpret_cast<void*>(&d_ours1),
                        reinterpret_cast<void*>(&d_ours2), reinterpret_cast<void*>(&d_ours3)};

// --- the foreign hook: result + 100 --------------------------------------------------------
Fn o_foreign = nullptr;
int d_foreign(int x) { return o_foreign(x) + 100; }

struct Foreign {
    unsigned char* target = nullptr;
    unsigned char saved[16] = {};
    size_t len = 0;
};

void write_abs_jmp(unsigned char* at, uint64_t dest)   // FF 25 00000000 <dest>
{
    at[0] = 0xFF; at[1] = 0x25;
    std::memset(at + 2, 0, 4);
    std::memcpy(at + 6, &dest, 8);
}

/// Hook `target` the way Dobby (6 bytes, slot) or MonoMod/funchook (14 bytes) does.
/// `copy` = how many bytes of the original start the foreign trampoline must re-run first
/// (whole instructions covering the patch); a leading E9 (our hook) is relocated instead.
Foreign foreign_hook(unsigned char* target, bool fourteen, size_t copy, unsigned char* tramp,
                     unsigned char* slot)
{
    Foreign f;
    f.target = target;
    f.len = fourteen ? 14 : 6;
    std::memcpy(f.saved, target, 16);
    if (target[0] == 0xE9) {
        // Our MinHook jump is first: relocate it as an absolute jump (it never falls through).
        int32_t rel;
        std::memcpy(&rel, target + 1, 4);
        write_abs_jmp(tramp, reinterpret_cast<uint64_t>(target + 5 + rel));
    } else {
        // Original code: re-run the covered instructions, then jump back after them.
        std::memcpy(tramp, target, copy);
        write_abs_jmp(tramp + copy, reinterpret_cast<uint64_t>(target + copy));
    }
    o_foreign = reinterpret_cast<Fn>(tramp);
    DWORD old;
    VirtualProtect(target, 16, PAGE_EXECUTE_READWRITE, &old);
    if (fourteen) {
        write_abs_jmp(target, reinterpret_cast<uint64_t>(&d_foreign));
    } else {
        uint64_t dest = reinterpret_cast<uint64_t>(&d_foreign);
        std::memcpy(slot, &dest, 8);
        int32_t disp = static_cast<int32_t>(slot - (target + 6));
        target[0] = 0xFF; target[1] = 0x25;
        std::memcpy(target + 2, &disp, 4);
    }
    FlushInstructionCache(GetCurrentProcess(), target, 16);
    FlushInstructionCache(GetCurrentProcess(), tramp, 64);
    return f;
}

void foreign_unhook(const Foreign& f)
{
    std::memcpy(f.target, f.saved, f.len);
    FlushInstructionCache(GetCurrentProcess(), f.target, 16);
}

int call(void* fn, int x)
{
    __try {
        return reinterpret_cast<Fn>(fn)(x);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        return -999999;   // the crash this check exists to catch
    }
}

UINT resume_of(void* target)
{
    UINT off = 0xFFFF;
    MH_GetResumeOffset(target, &off);
    return off;
}

// --- scenarios -------------------------------------------------------------------------------

/// Ours first, the other mod second (the 2026-10-06 FF1 crash), then both unhook orders.
void ours_then_foreign(int slot_index, bool fourteen, const unsigned char* code, size_t n,
                       size_t fn_off)
{
    unsigned char* fn = place(fn_off, code, n);
    int base = call(fn, 5);
    check(base == 12, "unhooked function returns x + 7");

    check(MH_CreateHook(fn, kOurs[slot_index], reinterpret_cast<void**>(&o_ours[slot_index])) == MH_OK,
          "our hook created");
    UINT resume = resume_of(fn);
    char msg[160];
    std::snprintf(msg, sizeof msg, "our trampoline resumes at byte %u (0 = never, needs 0 or >= %d)",
                  resume, MH_COEXIST_RESUME);
    check(resume == 0 || resume >= MH_COEXIST_RESUME, msg);
    check(MH_EnableHook(fn) == MH_OK, "our hook enabled");
    check(call(fn, 5) == 24, "ours alone: (5 + 7) * 2 = 24");

    Foreign f = foreign_hook(fn, fourteen, 0, g_page + 0x800 + fn_off / 4, g_page + 0xF00 + slot_index * 8);
    std::snprintf(msg, sizeof msg, "%s hook on top: (5 + 7) * 2 + 100 = 124",
                  fourteen ? "14-byte (MonoMod/funchook)" : "6-byte (Dobby)");
    check(call(fn, 5) == 124, msg);

    // We unhook while the other mod is on top: must be refused, nothing may change.
    unsigned char before[16];
    std::memcpy(before, fn, 16);
    check(MH_DisableHook(fn) == MH_ERROR_PATCHED_OVER, "our unhook under another hook is refused");
    check(std::memcmp(before, fn, 16) == 0, "the other mod's patch is untouched");
    check(call(fn, 5) == 124, "chain still works after the refused unhook");

    // The other mod unhooks: its saved bytes are our jump, so ours is back on top.
    foreign_unhook(f);
    check(call(fn, 5) == 24, "other mod unhooked: ours still runs (24)");
    check(MH_DisableHook(fn) == MH_OK, "now our unhook succeeds");
    check(call(fn, 5) == 12, "fully unhooked: 12");
    check(std::memcmp(fn, code, n) == 0, "original bytes restored exactly");
    MH_RemoveHook(fn);
}

/// The other mod first, ours second, then the other mod unhooks under us.
void foreign_then_ours(int slot_index, bool fourteen, size_t fn_off)
{
    unsigned char* fn = place(fn_off, kProlog5, sizeof kProlog5);
    // Whole instructions covering 6 or 14 bytes: 10 (5 + 5) or 15 (5 + 5 + 1 + 4).
    Foreign f = foreign_hook(fn, fourteen, fourteen ? 15 : 10, g_page + 0x800 + fn_off / 4,
                             g_page + 0xF00 + slot_index * 8);
    char msg[160];
    std::snprintf(msg, sizeof msg, "%s hook alone: 5 + 7 + 100 = 112",
                  fourteen ? "14-byte" : "6-byte");
    check(call(fn, 5) == 112, msg);

    check(MH_CreateHook(fn, kOurs[slot_index], reinterpret_cast<void**>(&o_ours[slot_index])) == MH_OK,
          "our hook created on top of the other mod's jump");
    check(MH_EnableHook(fn) == MH_OK, "our hook enabled");
    check(call(fn, 5) == 224, "both: (5 + 7 + 100) * 2 = 224");

    // The other mod unhooks first: it restores the original start, which removes our jump.
    foreign_unhook(f);
    check(call(fn, 5) == 12, "other mod unhooked under us: original runs, no crash (ours is gone)");
    unsigned char before[16];
    std::memcpy(before, fn, 16);
    check(MH_DisableHook(fn) == MH_ERROR_PATCHED_OVER,
          "our unhook is refused (stock MinHook would write the other mod's jump back half-way)");
    check(std::memcmp(before, fn, 16) == 0, "bytes untouched");
    check(call(fn, 5) == 12, "still 12");
}

} // namespace

int main()
{
    g_page = static_cast<unsigned char*>(
        VirtualAlloc(nullptr, 0x1000, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
    if (!g_page) { std::printf("VirtualAlloc failed\n"); return 2; }
    if (MH_Initialize() != MH_OK) { std::printf("MH_Initialize failed\n"); return 2; }

    std::printf("Ours first, Dobby-style 6-byte hook on top (the FF1 crash):\n");
    ours_then_foreign(0, false, kProlog5, sizeof kProlog5, 0x000);
    std::printf("Ours first, 14-byte hook on top:\n");
    ours_then_foreign(1, true, kProlog5, sizeof kProlog5, 0x100);
    std::printf("Ours first on a 4-byte getter, 6-byte hook on top:\n");
    ours_then_foreign(2, false, kTiny, sizeof kTiny, 0x200);
    std::printf("Dobby-style hook first, ours on top:\n");
    foreign_then_ours(3, false, 0x300);
    std::printf("14-byte hook first, ours on top:\n");
    foreign_then_ours(0, true, 0x400);

    UINT l = 0, s = 0, p = 0;
    MH_CoexistStats(&l, &s, &p);
    std::printf("stats: %u long resume, %u short, %u refused unhooks\n", l, s, p);
    check(s == 0, "no hook fell back to the short resume");

    // First FF2 boot failed after one trampoline page filled. Exercise many pages
    // without loading a game, rather than testing only five transient hooks.
    auto* many = static_cast<unsigned char*>(VirtualAlloc(nullptr, 0x10000,
        MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
    bool many_ok = many != nullptr;
    if (many) {
        std::memset(many, 0xCC, 0x10000);
        for (int i = 0; i < 512; ++i) {
            auto* target = many + i * 64;
            std::memcpy(target, kProlog5, sizeof(kProlog5));
            void* original = nullptr;
            const auto status = MH_CreateHook(target, kOurs[0], &original);
            if (status != MH_OK) { std::printf("  allocation hook %d: %s\n", i, MH_StatusToString(status)); many_ok = false; break; }
        }
    }
    check(many_ok, "512 simultaneous hooks span multiple trampoline pages");

    MH_Uninitialize();
    if (many) VirtualFree(many, 0, MEM_RELEASE);
    std::printf(g_failures ? "COEXIST CHECK FAILED (%d)\n" : "COEXIST CHECK PASSED\n", g_failures);
    return g_failures ? 1 : 0;
}
