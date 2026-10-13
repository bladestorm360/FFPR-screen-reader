#include <windows.h>
#include <cwchar>
#include "core/core.h"
#include "core/dedup.h"
#include "core/frame.h"
#include "core/field/baked_nav.h"
#include "core/game.h"
#include "core/hooks.h"
#include "core/il2cpp.h"
#include "core/log.h"
#include "core/minhook_dyn.h"
#include "core/modtext.h"
#include "core/refs.h"
#include "core/runtime_gate.h"
#include "core/say.h"
#include "core/settings.h"
#include "core/speech.h"
#include "core/text.h"
#include "core/unity.h"
#include "games/modules.h"

// ---------------------------------------------------------------------------
// Bootstrap (BDFFHD's, which is proven; read its dllmain.cpp header for the full history).
//
// DllMain runs under the loader lock, so it only spawns the boot thread. The boot thread:
//
//   1. detects the game from the exe name and picks its module;
//   2. waits for GameAssembly.dll PROPERLY (acquire_module), binds the IL2CPP API;
//   3. waits for the GAME to call its own runtime (runtime_gate) - never probes;
//   4. attaches, installs the frame tick, the core readers and the game module's hooks, and
//      enables them all in one thread freeze;
//   5. runs the startup self-check (every name the mod uses, for this game);
//   6. waits for the first hook to fire, then loads Tolk off the main thread;
//   7. detaches and becomes the speech pump. It NEVER returns: a thread that attached to the
//      runtime and then exits arms "SuspendThread loop failed" at the next collection.
// ---------------------------------------------------------------------------

namespace {

constexpr const char* kBuild = "Final Fantasy Pixel Master Core Screen Reader V0.9.1 (" __DATE__ " " __TIME__ ")";
constexpr int kSpeechWaitMs = 30000;
constexpr int kMaxWaitMs = 120000;
constexpr int kPollMs = 50;
constexpr int kRuntimeWaitMs = 180000;
// With MelonLoader or BepInEx installed, their first start (or the first after a game update)
// generates interop assemblies before the game's runtime runs, which can take minutes. The
// boot thread only sleeps while it waits, so it waits much longer then.
constexpr int kRuntimeWaitWithLoaderMs = 20 * 60 * 1000;

/// Is this process one we should boot in?
///
/// hid.dll is loaded by anything in the game folder that wants HID, so the boot thread is
/// gated: a Unity player exe (it has an "<exe>_Data" folder beside it) or the smoke host. The
/// crash handler and other helpers get the proxy exports and nothing else. A Unity game that
/// is not FF1-FF6 boots only far enough to say it is not supported.
bool host_should_boot(bool& smoke)
{
    wchar_t path[MAX_PATH];
    DWORD n = GetModuleFileNameW(nullptr, path, MAX_PATH);
    if (n == 0 || n >= MAX_PATH - 8) return false;
    const wchar_t* slash = wcsrchr(path, L'\\');
    const wchar_t* exe = slash ? slash + 1 : path;
    smoke = _wcsicmp(exe, L"smoke_host.exe") == 0;
    if (smoke) return true;
    wchar_t data[MAX_PATH];
    lstrcpynW(data, path, MAX_PATH);
    wchar_t* dot = wcsrchr(data, L'.');
    if (!dot) return false;
    *dot = 0;
    lstrcatW(data, L"_Data");
    DWORD attr = GetFileAttributesW(data);
    return attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY);
}

[[noreturn]] void park_forever()
{
    for (;;) Sleep(INFINITE);
}

/// The old C# screen reader for this game, if it is still in Mods, would speak too.
void warn_old_mod(const game::Module* m)
{
    if (!m || !m->old_mod_dll) return;
    std::wstring path = std::wstring(game::dir()) + L"Mods\\" + text::widen(m->old_mod_dll);
    if (GetFileAttributesW(path.c_str()) == INVALID_FILE_ATTRIBUTES) return;
    LOGF("boot: old screen reader still installed: Mods\\%s", m->old_mod_dll);
    std::wstring line = TF(L"Old screen reader still installed: {0}. Remove it from the Mods folder.",
                           {text::widen(m->old_mod_dll)});
    ffpr::say(line, false);
}

bool exists(const std::wstring& rel)
{
    return GetFileAttributesW((std::wstring(game::dir()) + rel).c_str()) != INVALID_FILE_ATTRIBUTES;
}

/// Other mod loaders in the game folder. The mod coexists with both (the built-in MinHook
/// stacks with their hooks, see minhook_dyn.h); this records what was there, so a log from a
/// crash says at once whether another loader was involved. True when one is present.
bool log_other_loaders()
{
    const bool melon = exists(L"MelonLoader") || exists(L"version.dll");
    const bool bepinex = exists(L"BepInEx") || exists(L"winhttp.dll") || exists(L"doorstop_config.ini");
    if (melon)
        LOGF("boot: MelonLoader present (%s%s); coexisting", exists(L"MelonLoader") ? "MelonLoader folder" : "",
             exists(L"version.dll") ? (exists(L"MelonLoader") ? ", version.dll" : "version.dll") : "");
    if (bepinex)
        LOGF("boot: BepInEx present (%s%s%s); coexisting", exists(L"BepInEx") ? "BepInEx folder " : "",
             exists(L"winhttp.dll") ? "winhttp.dll " : "", exists(L"doorstop_config.ini") ? "doorstop_config.ini" : "");
    return melon || bepinex;
}

void boot_guarded(bool smoke)
{
    game::Id id = game::detect();
    const game::Module* module = games::module_for(id);
    game::set_module(module);

    log_::init();
    speech::init();
    settings::init();
    dedup::set_logging(settings::get_bool("DebugLogging", false));
    LOGF("boot: %s", kBuild);
    LOGF("boot: game %s (%s)%s, pid %lu", game::label(), module ? module->name : "not supported",
         game::product_confirmed() ? ", product name confirmed" : "", GetCurrentProcessId());
    if (settings::imported()) LOGF("boot: settings imported from MelonPreferences");
    const int runtime_wait = log_other_loaders() ? kRuntimeWaitWithLoaderMs : kRuntimeWaitMs;

    int waited = 0;
    while (!il2cpp::module_present() && waited < kMaxWaitMs) {
        Sleep(kPollMs);
        waited += kPollMs;
    }
    if (!il2cpp::module_present()) {
        LOGF("boot: GameAssembly.dll never appeared after %d ms - going quiet", waited);
        return;
    }
    if (!il2cpp::acquire_module() || !il2cpp::bind()) {
        LOGF("boot: could not bind the IL2CPP API - going quiet");
        return;
    }
    if (!gate::arm()) {
        LOGF("boot: could not arm the runtime gate - going quiet");
        return;
    }
    if (!gate::wait(runtime_wait)) {
        LOGF("boot: the game never called its own IL2CPP API within %d ms - going quiet", runtime_wait);
        return;
    }
    LOGF("boot: runtime live - the game called %s", gate::fired_by());
    gate::disarm();
    if (!il2cpp::runtime_ready()) {
        LOGF("boot: gate fired but the domain is still null - going quiet");
        return;
    }

    il2cpp::attach_current_thread();

    if (!module) {
        // One spoken line, then passive. The proxy keeps forwarding hid.dll.
        hooks::apply();   // applies the gate disarm
        il2cpp::detach_current_thread();
        speech::request_load();
        speech::wait_and_load(kSpeechWaitMs);
        ffpr::say(L"FFPR screen reader: this game is not supported.", true);
        return;
    }

    modtext::init();
    unity::init();
    if (module->configure) module->configure(game::flags_mut());

    if (!frame::install())
        LOGF("boot: frame tick unavailable - hotkeys and deferred reads will not run");
    int n = core::install_all();
    if (module->install) n += module->install();
    // Resolve shared lazy refs while hooks are still disabled. Class/Field/Method caches
    // are deliberately lock-free on the main thread; enabling readers before this pass
    // made their first use race this boot thread's writes to tried_/cached values.
    ref::self_check();
    // This uses mod text; finish it before main-thread hooks can change the language.
    warn_old_mod(module);
    hooks::apply();
    LOGF("boot: %d hooks installed, %d failed", hooks::installed(), hooks::failed());

    il2cpp::detach_current_thread();
    field::baked_nav::preload();
    speech::wait_and_load(kSpeechWaitMs);
    LOGF("boot: startup complete");
    (void)smoke;
}

struct BootArgs { bool smoke; };
BootArgs g_args{};

DWORD WINAPI boot(LPVOID)
{
    __try {
        boot_guarded(g_args.smoke);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        OutputDebugStringA("[FFPR reader] boot faulted; mod disabled, game continues\n");
    }
    // A startup fault after attachment must not retain a GC-registered speech worker.
    il2cpp::detach_current_thread();
    __try {
        speech::pump();
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        OutputDebugStringA("[FFPR reader] speech pump faulted; speech disabled, game continues\n");
    }
    park_forever();
}

} // namespace

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID)
{
    switch (reason) {
    case DLL_PROCESS_ATTACH: {
        DisableThreadLibraryCalls(module);
        bool smoke = false;
        if (host_should_boot(smoke)) {
            g_args.smoke = smoke;
            HANDLE thread = CreateThread(nullptr, 0, boot, nullptr, 0, nullptr);
            if (thread) CloseHandle(thread); // The boot thread owns its lifetime.
        }
        break;
    }
    case DLL_PROCESS_DETACH:
        // Deliberately empty: process exit only, loader lock held (see BDFFHD's dllmain.cpp).
        break;
    }
    return TRUE;
}
