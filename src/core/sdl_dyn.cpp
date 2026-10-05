#include <windows.h>
#include "sdl_dyn.h"
#include "log.h"
namespace sdl {
#define DEFINE_SDL(name) decltype(&::name) name=nullptr;
SDL_DYNAMIC_FUNCTIONS(DEFINE_SDL)
#undef DEFINE_SDL
bool load() {
    static bool attempted=false, available=false;
    if(attempted) return available;
    attempted=true;
    wchar_t path[MAX_PATH]{};
    const DWORD length=GetModuleFileNameW(nullptr,path,MAX_PATH);
    if(!length || length>=MAX_PATH) return false;
    wchar_t* slash=wcsrchr(path,L'\\'); if(!slash) return false;
    wcscpy_s(slash+1,MAX_PATH-static_cast<size_t>(slash+1-path),L"SDL3.dll");
    HMODULE module=LoadLibraryExW(path,nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_SYSTEM32);
    if(!module) { LOGF("SDL: unavailable; native controller input retained (error %lu)",GetLastError()); return false; }
    bool valid=true;
#define BIND_SDL(name) name=reinterpret_cast<decltype(name)>(GetProcAddress(module,#name)); valid=valid && name;
    SDL_DYNAMIC_FUNCTIONS(BIND_SDL)
#undef BIND_SDL
    if(!valid || SDL_GetVersion()<SDL_VERSIONNUM(3,2,0) || SDL_GetVersion()>=SDL_VERSIONNUM(4,0,0)) {
        LOGF("SDL: unsupported library or missing required export"); FreeLibrary(module); return false;
    }
    SDL_SetMainReady();
    LOGF("SDL: dynamic version %d",SDL_GetVersion());
    available=true; return true; // Process lifetime: audio callbacks must never outlive this module.
}
}
