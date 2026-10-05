# Porting guide: C# MelonLoader mods to the native core

Read this before porting any file. It is the contract every port follows.
Plan: `D:\Games\Dev\Unity\FFPR\UNIFIED_MOD_PLAN.md`. Open issues: `..\OPEN_ISSUES.md`.

## Hard rules

1. **Never launch a game. Never deploy.** Never run `build.bat`, `smoketest.bat` or `deploy.bat`
   during parallel work; the lead runs the shared build. Use `tools\compile_check.bat` only.
2. **Never edit a file you do not own.** Your brief lists your files. The core files in
   `src/core/*.cpp|h` (top level) belong to the lead. Need a core change? Put a small helper in
   your own area and say so in your report.
3. **Never edit the old mods** (`ff1\ff1-screen-reader` … `FF5\ff5-screen-reader`). Read only.
   Never read FF6 (`ff6\`).
4. **Never read a `dump.cs` whole** (15+ MB). Grep it. Paths: `FFPR\ff1\dump.cs`, `ff2\`, `ff3\`,
   `FF4\dump.cs`, `FF5\dump.cs`.
5. **No polling, no timers** except the allowed core: input, the controller router, audio loops
   (wall tones, footsteps, beacons, EXP tick), map-transition detection, and bounded 1–30-frame
   reads that start from an event. Keep the C# mod's event hooks; keep its documented one-frame
   deferrals.
6. **No new text deduplication.** Existing checks move to `dedup::` as they are (identity first).
   Never add a text comparison the C# code did not have.
7. **Translations:** mod UI text uses `T(L"key")`. The key must exist in
   `data/common/mod_text.json` (or `data/ffN/mod_text.json`). Do not add keys and do not write
   translations; list any key you need in your report.
8. **Game content** (entity names, map names) comes from the game or the game's own data files
   (`data::game_json("translation")`), never from another game.
9. **Behaviour parity with FF1** (the gold standard). Port behaviour and wording exactly; fix
   nothing "while you are there" unless it is a crash in native code.

## Layout and ownership

- `src/core/<area>/` — shared features, used by all five games (`ref::Class` with `game::kAll`).
- `src/games/ffN/` — one game's own screens (`Serial.FFn` classes, game-only `Last.*` classes),
  registered with `core::Install` masks like `game::kFF1`.
- Each source file starts with a comment saying which C# files it ports, e.g.
  `// Ports FF1 Patches/MessageWindowPatches.cs.`
- Namespaces: one per area (`ffpr::battle`, `ffpr::field`, `ffpr::menus` …). Everything not
  needed by another file goes in an anonymous namespace.

## The core API (headers in `src/core`)

| Header | Use |
|---|---|
| `refs.h` | `ref::Class`, `ref::Field`, `ref::Method` declared at file scope; `ref::rd<T>`, `ref::wr`, `ref::str`, `ref::rd_str`, `ref::call<R>`, `ref::call_static<R>`, `ref::call_void`, `ref::call_str`, `ref::instance` (singleton `.Instance`), `ref::new_str` |
| `hooks.h` | `hooks::attach(method, &detour, &original)`, `hooks::attach_addr`, `hooks::guarded(label, lambda)`, `hooks::guarded_r` |
| `frame.h` | `frame::next_frame`, `after_frames`, `after_seconds`, `every_frame` (bounded), `cancel`, `cancel_label`, `subscribe` (per-frame: input/audio only), `post` (from another thread), `now` |
| `say.h` | `ffpr::say(text, interrupt = true)`, `ffpr::say_queued`, `ffpr::silence`, `ffpr::last_spoken` |
| `modtext.h` | `T(L"key")`, `TF(L"key", {a, b})`, `modtext::lang()`, `data::game_json("translation")`, `data::game_json("entitynames")` |
| `settings.h` | `settings::get_bool("WallTones", false)`, `set_bool`, `get_int`, `toggle` … keys = the MelonPreferences entry names, same defaults |
| `dedup.h` | `dedup::index/object/text/index_text(ctx, value, scope)`, `reset`, `reset_scope` |
| `unity.h` | `find_object`, `find_objects`, `get_component`, `components_in_children`, `texts`, `ui_text`, `first_text`, `active_in_hierarchy`, `name`, `transform`, `parent`, `child`, `child_count`, `find_child`, `find_descendant`, `position`, `array_*`, `list_*`, `dict_for_each`, `alive` |
| `ui_util.h` | `ui::menu_position(text, index, count)`, `ui::cursor_index(cursor)`, `ui::state_tag(controller, field)`, `ui::list_item` |
| `core.h` | `core::Install`, `actions::define/run`, `menu_state::*` (FF1 MenuStateRegistry), `events::on/fire` |
| `game.h` | `game::current()`, `game::is(game::Id::FF1)`, `game::flags()` |
| `text.h` | `text::clean`, `strip_markup`, `collapse`, `trim`, `format`, `num`, `join`, `from_utf8`, `to_utf8` |
| `log.h` | `LOGF("fmt %d %ls", n, wide)` (UTF-8 log; `%ls` for `const wchar_t*`), `LOGW(L"prefix: ", wide)` |
| `il2cpp.h` | low level: `read_field<T>(obj, off)`, `object_class_name`, `is_a`, `find_class`, `find_nested_prefix` (coroutine state machines) |

## Converting C# patterns

**A Harmony postfix:**

```cpp
// Ports FF1 Patches/MessageWindowPatches.cs (SetContent postfix).
namespace {
ref::Class  kMwm{"Last.Message", "MessageWindowManager"};
ref::Field  fMessageList{kMwm, "messageList"};
ref::Method mSetContent{kMwm, "SetContent", 0};

using SetContent_t = void (*)(void* self, const void* mi);
SetContent_t o_SetContent;

void d_SetContent(void* self, const void* mi)
{
    o_SetContent(self, mi);                     // postfix: the game runs first
    hooks::guarded("MessageWindow.SetContent", [&] {
        void* list = ref::rd<void*>(self, fMessageList);
        ...
        ffpr::say(line, false);
    });
}

int install()
{
    return hooks::attach(mSetContent, &d_SetContent, &o_SetContent) ? 1 : 0;
}
core::Install s_install{"MessageWindow", &install, 100};
}
```

- **Signature:** every IL2CPP method is `R fn(void* self, args..., const void* methodInfo)`;
  static methods drop `self`. Types: `bool`, `int32_t`, `float`, enums as `int32_t`, references as
  `void*`, `Vector3`/`Vector2` structs by value (`unity::Vec3`). Get the exact parameter list from
  `dump.cs` for EVERY game the hook applies to.
- **Prefix** = body before calling the original. A prefix that `return false` = don't call the
  original. `ref __result` = take the original's return value, change it, return it.
- `__instance` = `self`. `__0`, `__1` = the parameters in order.
- **Overloads:** `ref::Method` resolves by name and argument count. If two overloads have the same
  count, find another hook point or resolve with `il2cpp::find_method_overload_param` in your
  install() and use `hooks::attach_addr`.
- **Folded bodies:** before adding a hook, grep its `// RVA: 0x…` in `dump.cs` and count the hits
  for every game it applies to; the count must be 1. If shared, the registry refuses a second
  detour on the same address — hook once and class-check `self` with `ref::Class::is`.
  OPEN_ISSUES.md "Cross-cutting lessons" lists known traps.
- **Typed property reads** (`instance.Foo`) → `ref::Field` on the backing field
  (`<Foo>k__BackingField`) or the plain field, or `ref::call<T>(mGetFoo, obj)` for a computed getter.
- **IL2CppOffsets / raw pointer offsets** → a `ref::Field` by NAME. Find the field name in
  `dump.cs` from the C# comment or the offset. The native mod has no offset tables.
- `FindObjectOfType<T>()` → `unity::find_object(kT)`. `GetComponentsInChildren<Text>()` →
  `unity::texts(obj)`, `unity::ui_text(t)`.
- `il2cpp List<T>` → `unity::list_count` / `list_at`; arrays → `array_length` / `array_at`.
- **Coroutines:** `yield return null` then work → `frame::next_frame([=]{ … })`. N frames →
  `after_frames`. `WaitForSeconds(s)` → `after_seconds`. A loop that waits for UI to fill in →
  `frame::every_frame([=]{ … return keep_going; }, "label", max_frames)`. Never capture raw game
  pointers in a deferred task unless you re-check them with `unity::alive` (Unity objects) when it
  runs; prefer re-finding the object.
- `SpeakText(text, interrupt)` → `ffpr::say(text, interrupt)` (default interrupt = true, as in FF1).
- `PreferencesManager.WallTonesEnabled` → `settings::get_bool("WallTones", false)` (key and
  default from FF1's `Core/PreferencesManager.cs`).
- `MenuStateRegistry` → `menu_state::`. `lastAnnouncedX` variables → `dedup::` with a context
  name `Area.What` (see docs/DEDUP_INVENTORY.md when it exists).
- `MelonLogger.Msg/Warning` → `LOGF`. Keep logging light on hot paths.
- C# `string` → `std::wstring`; literals `L"..."`. Format with `text::format` or `TF`.

## Actions (keys and buttons)

Input never calls a feature directly. A feature defines a named action and input binds to it:

```cpp
actions::define("StatusNavigationReader.NavigateNext", [] { navigate_next(); });
```

**Naming:** `Class.Method` after the C# method that implements it. Methods of
`FFI_ScreenReaderMod` are `Mod.Method` (e.g. `Mod.AnnounceGilAmount`, `Mod.CycleNext`). The agent
porting the implementing C# file defines the action, inside its install() function. The input
port binds keys and controller buttons to these exact names, from FF1's `KeyBindingRegistry`,
`InputManager`, `ControllerRouter` and `Core/Handlers/*`. A lambda with arguments becomes a
name per argument (`Mod.TeleportNorth` … `Mod.TeleportWest`).

## Shared state between areas

- `menu_state::` — which menu is active (FF1 keys in core.h).
- `events::fire(events::Kind::BattleStart)` etc. Fire from the hook that knows it happened;
  subscribe with `events::on`.
- For anything else another area needs (e.g. "is a dialogue window open"), expose a small
  function in your area's header and name it in your report.

## Checking your work

- `tools\compile_check.bat <your-tag> src\core\area\a.cpp src\core\area\b.cpp` from the repo
  root (run it through `cmd.exe /d /c`). Zero errors required. Fix /W4 warnings in your files.
- Verify every class, field and method name you use in the `dump.cs` of every game in its mask.
  A name missing in a game → narrow the mask (`game::kFF1 | game::kFF2`) or use the field's
  `alt` name, and note it.
- Your report (under 400 words): files written, C# files fully ported, anything not ported and
  why, actions defined, settings keys read, mod_text keys needed that do not exist, hooks whose
  RVA is shared in some game, and functions you expose for other areas.
