# Title prompt and intro presentation, October 11

## Current timing and hook-installation follow-up, October 13

This section supersedes the October 11 Indicator.Hide/next-frame reader below.
The user reports late prompts in FF1–FF4, and an FF2 boarded-save session where V
said Walking and all navigation commands did nothing. The user's own FF2 log from
07:23:11 has 29 installed/199 failed hooks, including every title observer,
SceneManager.Internal_SceneLoaded, FieldMap.StateInit and FieldPlayerKeyController.Init.
It contains no prompt-read entry, then a zero-live/zero-remote map snapshot and Walking.
It therefore establishes absent observers in that session, not a measured prompt delay.
The earlier successful FF1 session has 218 installed/zero failed hooks and a prompt-read
entry; the log alone does not timestamp when its graphics became visible.

The allocator reproduction and fix are recorded in
[threading](THREADING.md#hook-allocation-and-boarded-save-follow-up-october-13).
Fixing the title reader alone would leave the failed-hook session without a reader.

The reader now observes the verified native UpdateNone producer after its original
activates the prompt parent. InitNone arms the visit without reading hidden preload
text; InitShortcutCommand can read its directly activated prompt immediately, and
UpdateShortcutCommand handles delayed renderer readiness. Only a pending native title
phase reads one known text candidate, with class, owner identity, Unity lifetime and
the existing displayed::text renderer/alpha/cull guards. After one ready presentation
the producer latch stops further reads. There is no scene search, Indicator.Hide
dependency, scheduled next frame or 60-frame expiry. Native phase/menu/lifetime guards
retire stale reads; this is event identity, not content deduplication. Identical wording
on a new visit remains eligible. Additive preload scenes do not cancel the title owner.

InitSelect and enabled main-menu callbacks cancel the pending event; original native
work always runs first. A same-frame button/menu transition therefore reads Load Game
normally without late prompt speech. Existing interrupt generations also discard a
queued prompt if later menu focus supersedes it while the speech worker is busy.
FF5/FF6 retain their native cinematic timing and their existing appended instruction.
No native pointer is sent to a worker. The ready line is copied through say/speech;
only the speech worker cleans/chunks it and calls Prism.

Offline evidence:

- `tools/re/title_calls.py` independently rechecked all six matching PE/dump call paths:
  fade/preload readiness, inlined Hide→parent activation and shortcut activation. New
  raw exports/manifests and binary hashes stay in ignored `build_check/re`; historical
  archives remain preserved. FF2 UpdateShortcutCommand is 0x806EE0; UpdateNone remains
  0x806C60. Production resolves names rather than using these offsets.
- `tools/title_prompt_check.cpp` uses the actual title source, cache, speech ingress,
  worker drain and Prism adapter with fake native owners/SDK. All-six prompt/instruction
  order, same-callback publication, hidden/120-update delayed rendering, repeated visits,
  shortcut/scene recovery, additive preload, foreign/destroyed owners and same-frame/
  queued menu cancellation pass. It does not execute the commercial game or real NVDA.
- Canonical build includes this regression. Final worst fake-native capture is
  7.600 microseconds; it excludes real Unity call cost, game-frame scheduling and audible
  delivery. Exact output is retained in `build_check/title_prompt_report.txt`. Actual
  display-to-audible timing still requires user gameplay confirmation.

Current deployment/concise test procedure is in
[testing](../TESTING.md#hook-installation-title-and-boarded-save-retest-october-13).
Sol implemented/reviewed/verified this follow-up; no delegation or runtime probe.

Owner: Sol. Matching installed `GameAssembly.dll` files were read as PE data,
never loaded/executed. Sources are the six adjacent `dump.cs` exports. Existing
FF2/FF5 Ghidra projects were opened headlessly with `-noanalysis -readOnly`;
`DecompileManifest.java` exported the focused title paths without an analysis
timeout. All six paths were independently disassembled with the already-installed
Capstone/pefile packages using [title_calls.py](../../tools/re/title_calls.py).
The `ffN_title_manifest.tsv` files identify functions; `ffN_title_calls.txt` retain
instructions and direct-call names. Folded aliases are summarized, not mistaken
for a unique callback.

## Executed prompt path

The previous follow-up incorrectly assumed that a resolved
`Last.UI.KeyInput.TitleWindowController.SetEnableStartObject(bool)` wrapper was
called during startup. Every game's `UpdateNone` **inlines** that wrapper. After
fade/preload readiness, it checks `view.startParent.activeSelf`, calls
`Last.Systems.Indicator.SystemIndicator.Hide()`, then calls
`UnityEngine.SafeSetFunction.SafeActiveSet(view.startParent, true)` directly.
Hook installation success therefore did not imply an executed reader.

| Game | UpdateNone RVA | Indicator.Hide RVA | InitNone RVA | InitShortcutCommand RVA |
| --- | --- | --- | --- | --- |
| FF1 | 0x7D82A0 | 0x4FF4A0 | 0x7D60F0 | 0x7D6510 |
| FF2 | 0x806C60 | 0x479000 | 0x804A50 | 0x804E70 |
| FF3 | 0x8CD650 | 0x60BBC0 | 0x8CB440 | 0x8CB860 |
| FF4 | 0x8F65E0 | 0x6CDDC0 | 0x8F43D0 | 0x8F47F0 |
| FF5 | 0x64F090 | 0x4DAAE0 | 0x64CB20 | 0x64D070 |
| FF6 | 0x914840 | 0x6AAA60 | 0x912350 | 0x912860 |

Every `InitShortcutCommand` also directly enables the prompt parent. Subsequent
`UpdateShortcutCommand`/`ShortcutExtraCommand` may move immediately to a selected
menu, so speech must recheck rendered visibility rather than disclose a transient
or already-dismissed prompt. `title_calls.py` asserts the real Hide→enable sequence,
absence of a startup call to the unused wrapper, and shortcut parent enable in
each game. It is an offline call-path check, not a gameplay speech test.

`src/core/dialogue/title.cpp` arms from native `InitNone`/`InitShortcutCommand`
and retains the existing Title scene-entry recovery. The contextual Hide callback
schedules the read for the next frame, after its caller has activated the parent.
It runs only while the title visit is armed and the current loaded scene is Title
or TitleScreen. No object search runs for field loading. The bounded 60-frame
reader follows `view.startText` through `displayed::text`, including active hierarchy,
Graphic alpha, CanvasRenderer alpha/cull and actual UI text. Blank/invisible text
does not use a MessageManager key or synthesized fallback. Native `InitSelect`
and main-menu enable cancel pending speech. Existing producer arming is retained;
no comparison of spoken text or new speech deduplication is introduced.

## FF5 and FF6 intro paths

The intros are native animated demo objects (`Last.FF5.Demo.Advertisement` and
`Last.FF6.Demo.Advertisement`), not a second text value to announce from assets.
Both own a 15-second idle timer and interact with `ITitleWindowForAdvertisement`.
They pause/stop idle playback during menu selection and use the native title
interface to freeze/restore title state while handling return to the menu.
FF5's `Advertisement.Idle` (0xEB7650) checks its timer and fades into playback.
FF6's `Advertisement.Update` (0xE06D70), at 0xE08681–0xE0890F, checks its idle
timer, pauses outside the idle title state, and starts its own fade/animation when
the timer finishes. FF6 also has `AdvertisementOpeningFader` for boot presentation.

`TitleWindowController.OnAdvertisement` and `SetTitleFadeCallback` are folded
no-op bodies in these PC binaries. They are not valid presentation hooks.
`InitNone`/`UpdateNone`, the native Hide call and `InitShortcutCommand` cover the
actual prompt path without predicting a 72/75-second animation duration or reading
a hidden prompt while the cinematic obscures it. Startup/cinematic timing stays
owned by the game.

User-requested mod guidance is enabled only by the FF5/FF6 module capability
`title_idle_cinematic`. Once the actual rendered prompt is available, the same
utterance reads that game text first, followed by:

> Wait to watch the intro cinematic, or press a button to open the title menu.

This is mod instructional text, not a replacement game prompt or undisplayed
game-text fallback. The sentence is in common mod translations (English fallback).
There is no intro instruction for FF1–FF4. User verification of speech/timing for
all six, including FF5/FF6 initial boot and return to title, remains pending.

## Build and deployment

`build.bat` and `smoketest.bat` passed, including 6,932 native-name checks (zero
missing), wiring (zero undefined), scheduler/worker lifetime, coexistence, all-six
map-state and baked-route checks, and 47 forwarding exports in the isolated host.
`deploy.bat` installed the shared build into all six games; every installed DLL
matched SHA-256 `0F283AE7ECD7D8A1380F8E4CBB39C459AA9D769D98D9566AA0B08DD5F363265B`.
No game was launched, controlled or runtime-probed.
