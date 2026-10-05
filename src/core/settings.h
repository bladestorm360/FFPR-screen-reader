#pragma once
#include <string>

// The mod's settings: one small key = value file, ffpr_screen_reader.ini in the game folder.
//
// Keys keep the names of the old MelonPreferences entries ("WallTones", "FootstepVolume"...),
// so the one-time import is a copy. On the first start without our file, the entries of the
// game's old category ([FFI_ScreenReader] .. [FFV_ScreenReader]) in UserData\MelonPreferences.cfg
// are copied in, and the import is logged. MelonPreferences.cfg itself is never written.
//
// Writes only mark the store dirty and wake the speech worker, which saves off the game
// thread (speech.cpp's pump calls save_pending()). Thread safe.
namespace settings {

/// Boot thread, after game::detect() and before anything reads a setting.
void init();

bool get_bool(const char* key, bool def);
int get_int(const char* key, int def);
float get_float(const char* key, float def);
std::wstring get_str(const char* key, const wchar_t* def);

void set_bool(const char* key, bool v);
void set_int(const char* key, int v);
void set_float(const char* key, float v);
void set_str(const char* key, const std::wstring& v);

/// Flip a bool, store it and return the new value.
bool toggle(const char* key, bool def);

/// Write the file if anything changed. Called by the speech worker; safe from any thread.
void save_pending();

/// True when this start imported the old MelonPreferences category.
bool imported();

} // namespace settings
