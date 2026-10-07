# FFPR Screen Reader (native) — development rules

One native C++ DLL, `hid.dll`, for FINAL FANTASY I–VI Pixel Remaster. It replaces the six
MelonLoader screen readers (`ff1\ff1-screen-reader` … `FF5\ff5-screen-reader`, now paused, and
`..\ff6\FF6ScreenReader`). Plan and decisions: `..\UNIFIED_MOD_PLAN.md`. Open issues:
`..\OPEN_ISSUES.md`. Porting contract: `docs\PORTING.md`. Authors: FF1–FF5 are bladestorm360's.
FF6 is ZKline's (BlindGuyNW/FF6ScreenReader); its author gave permission to include it (user,
2026-10-07). Keep its attribution; the old FF6 repo is read-only.

## Hard rules

1. **Never launch a game.** The user tests. Ask for results before building on unverified work.
2. **Nothing may stop a game from starting.** `hid.dll` is imported by UnityPlayer.dll:
   - forward all 47 System32 hid.dll exports (`tools\import_check.py` runs in `build.bat`);
   - import only system DLLs; MinHook, Tolk and SDL3 are LoadLibrary'd at runtime;
   - no CRT stdio, no `_s` functions with untrusted arguments (BDFFHD rule 9);
   - the boot thread never returns (BDFFHD rule 10: GC "SuspendThread loop failed").
3. **Build, smoke test, then deploy:** `build.bat` → `smoketest.bat` → `deploy.bat [ffN]`.
   Building never writes to a game folder. Parallel work uses `tools\compile_check.bat` only.
4. **Names, not addresses.** Classes, fields and methods resolve by name (`src\core\refs.h`).
   No RVAs, no offsets. `tools\name_check.py` checks every name against each game's dump.cs.
5. **Hooks** go through `hooks::attach`. One detour per address; extra subscribers are chained.
   Check a new hook's RVA for folded bodies (OPEN_ISSUES.md "Cross-cutting lessons") and
   class-check `self` when shared.
6. **Game pointers** are not kept past the hook unless re-checked (`unity::alive`). Strings are
   copied out (`ref::str`).
7. **Polling and timers:** none, except the allowed core (OPEN_ISSUES.md "Timers and per-frame
   work deliberately kept"): input, controller router, audio loops, map-transition detection,
   bounded 1–30-frame reads that start from an event. The frame tick is
   `System.Input.InputSystemManager.Update`.
8. **Deduplication** only through `dedup::`. A new text-based check needs the user's OK.
9. **Translations:** mod UI text is shared (`data\common\mod_text.json`, overrides in
   `data\ffN\mod_text.json`). Game content (entity names) stays per game, never copied between
   games. Never invent a translation without saying so.
10. **Files:** edit `.md` files with the Edit tool; keep encodings; batch files keep CRLF
    (`tools\crlf.py`). In Git Bash heredocs backslashes are mangled — write files with the Write
    tool.
11. **Git:** local repo, branch `master`. No remote yet; creating one on GitHub is the user's
    call.

## Layout

```
src/dllmain.cpp      boot thread (BDFFHD model), game detection, module selection
src/proxy/           hid.dll forwarding (47 exports) and hid.def
src/core/            shared by all games (no include path to src/games)
  refs, hooks, frame, say, modtext, settings, dedup, unity, ui_util, core (installs, actions,
  queries, menu_state, events), il2cpp, speech, log, minhook_dyn, sdl_dyn, runtime_gate
  input/ audio/ ui/ field/ fieldnav/ dialogue/ system/ menus/ status/ bestiary/ battle/
  shop/ extras/       the ported features
src/games/ffN/       one game's own screens and flags (module.cpp)
data/                mod_text (shared + overrides) and translation.json per game (no mod
                     loads an EntityNames.json; CMake embeds one only if it is added)
tools/               smoke host, import/name checks, compile check, merge_modtext
docs/                PORTING.md, FEATURES.md, DEDUP_INVENTORY.md, INPUT_BINDINGS.md
```

## Logs

`ffpr_screen_reader.log` in the game folder (the previous five runs are kept as `.1`–`.5`).
"Check the latest log" means that file in the game being tested. Never read tester logs
without the user's explicit permission for that request.
