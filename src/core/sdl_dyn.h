#pragma once
#define SDL_MAIN_HANDLED
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_gamepad.h>
#include <SDL3/SDL_audio.h>
#include <SDL3/SDL_hints.h>
#include <SDL3/SDL_version.h>
#include <SDL3/SDL_keyboard.h>
// Header types only. Every call is through GetProcAddress; no SDL import library.
#define SDL_DYNAMIC_FUNCTIONS(X) \
    X(SDL_SetMainReady) X(SDL_IsMainThread) X(SDL_GetVersion) X(SDL_GetError) X(SDL_SetHint) \
    X(SDL_InitSubSystem) X(SDL_QuitSubSystem) X(SDL_PumpEvents) X(SDL_PeepEvents) X(SDL_UpdateGamepads) \
    X(SDL_GetGamepads) X(SDL_free) X(SDL_OpenGamepad) X(SDL_CloseGamepad) \
    X(SDL_GetGamepadName) X(SDL_GetGamepadType) X(SDL_GetGamepadButton) X(SDL_GetGamepadAxis) \
    X(SDL_GamepadConnected) X(SDL_GetGamepadButtonLabel) \
    X(SDL_OpenAudioDeviceStream) X(SDL_ResumeAudioStreamDevice) \
    X(SDL_DestroyAudioStream) X(SDL_PutAudioStreamData) X(SDL_GetAudioStreamDevice) \
    X(SDL_GetAudioDeviceFormat) X(SDL_GetAudioStreamFormat) \
    X(SDL_CreateAudioStream) X(SDL_GetAudioStreamData) X(SDL_FlushAudioStream) \
    X(SDL_BindAudioStream) X(SDL_ClearAudioStream) X(SDL_CloseAudioDevice) X(SDL_GetAudioStreamQueued) \
    X(SDL_OpenAudioDevice) X(SDL_ResumeAudioDevice) X(SDL_SetAudioStreamGain) X(SDL_PollEvent) \
    X(SDL_GetGamepadID) X(SDL_GetKeyboardState)
namespace sdl {
#define DECLARE_SDL(name) extern decltype(&name) name;
SDL_DYNAMIC_FUNCTIONS(DECLARE_SDL)
#undef DECLARE_SDL
bool load(); // Dedicated Windows SDL owner only, never a game callback or DllMain.
}
