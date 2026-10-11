# Title prompt and intro presentation, October 11

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
