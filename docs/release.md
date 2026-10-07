# Release procedure

Triggered when the user says "prepare release X.Y". The work ends at the zip; the user ships it.

1. `build.bat` — must end with "Build OK", the import check, and the name check with 0 missing.
2. `smoketest.bat` — must PASS.
3. The user tests each game they want in the release with its section of `..\TEST_LIST.md`
   (Phase 4 rule: a game ships when its test list passes; until then its old C# mod stays the
   fallback).
4. Bump the version in `src/version.rc` (FILEVERSION, PRODUCTVERSION, both strings) and in
   `kBuild` in `src/dllmain.cpp`; build and smoke test again.
5. `python tools\make_release.py X.Y.Z` → `releases\FFPR_ScreenReader_X.Y.Z.zip`.
6. The readme in the zip is `readme.md` (repo root). It was written on the user's request on
   2026-10-06 from the FF1 readme; it is the user's document: edit it only when the user asks,
   and check its key lists against the code when keys change.

## What players do (for the readme the user writes)

- Remove the old `FFn_ScreenReader.dll` from the game's `Mods` folder. MelonLoader and BepInEx
  may stay (the built-in MinHook stacks with their hooks); the native mod speaks a warning while
  an old screen-reader DLL is still there.
- Copy the zip's files into the game folder (next to `FINAL FANTASY.exe` etc.). The same
  files work for all six games. The zip carries `MinHook_LICENSE.txt` (MinHook is compiled in).
- To remove the mod: delete `hid.dll`. Settings are in
  `ffpr_screen_reader.ini`, the log in `ffpr_screen_reader.log`, both in the game folder.
- First start imports the old mod's settings from `UserData\MelonPreferences.cfg` once.
