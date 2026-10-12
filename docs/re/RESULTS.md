# Current-screen results buffers (2026-10-12)

User requirement: replace the L results log in all six games with the currently
displayed native phase's arrow-review buffer. Always announce displayed EXP/gil
totals and each phase headline. Member EXP, individual drops and stat tables
follow Autodetail; manual review/details always remain available. Arrow input
must never advance a native results phase, including held/release input and
the interval before the page buffer is ready. Ordinary confirm advances normally.

## Offline evidence and producer paths

`tools/re/results_calls.py` disassembles metadata-sized PE ranges without loading
a game. `ff1_results_calls.txt` through `ff6_results_calls.txt`, matching edges
and manifests record native callers/addresses. Matching dump class layouts are
in `ffN_results_schema.json` and `ffN_results_views.json`.

- The shared `Last.UI.KeyInput.ResultMenuController` owns outer points, status,
  items, learned abilities and ability/job-level phases. Inner per-game
  `ResultPointController.PointsInit` and `ResultStatusUpController.SetData` own
  points initialization and each member's rendered growth page. FF4 uses
  `Serial.FF0.UI.KeyInput`; the other five use their game's numbered namespace.
- FF2 additionally enters `SkilllLevelsInit` (native spelling, three l's) and
  `SkillLevelsUpdate`; the former populates the displayed weapon/spell panel.
  Its outer hooks were disabled by stale FF1-port rules, despite the unified
  implementation now handling FF2 correctly. Remove those obsolete disables.
- Total fields belong to `ResultGetPointView` below `getPointController.view`:
  gil/EXP in all six layouts, ABP additionally in FF5, magic points in FF6.
  Renderer visibility determines availability; FF2 has no character EXP.
- Growth-page headline is `ResultStatusUpView.nameText`. Recursively reading
  every text beneath the view puts the stat table in the headline and bypasses
  Autodetail. FF6's displayed magic-stone bonus is a separate detailed row.
- Results call `System.Input.InputListener.AnyKey`, `AnyKeyDown`, and
  `GetKeyDown` directly. These bypass `InputSystemManager`: listener GetKey
  dispatches straight to KeyConfig, and AnyKeyDown/Up inline their key checks.
  Suppression only in manager postfixes cannot protect these screens. Prefix
  guards are required on both input APIs, covering down/held/up/any checks.
  FF2 examples: PointsUpdate 0x3D9690, SkillLevelsUpdate 0x3DA020,
  StatusUpUpdate 0x3DA490, ResultSkillController.ShowListUpdate 0x5BC2F0;
  listener AnyKey 0xA543D0, AnyKeyDown 0xA53EC0, AnyKeyUp 0xA54140.

## Existing defects traced

The shared reader includes learned detail rows in its points headline; learned
pages explicitly set headline_has_details and speak full rows unconditionally.
Status growth recursively collects the entire view into its headline. Review
context requires a nonempty populated buffer, and manager protection covers
only press edges after calling the native function. All are producer/lifecycle
bugs; no new speech deduplication is authorized or needed.

Bravely Default reference: `D:/Games/Dev/Unity/BDFFHD/bdffhd-screen-reader/src/`
`battle_buffer.*`, `buffer_controls.h`, and results section of
`hooks_battle.cpp`. Relevant pattern is current rendered content, copied lines,
native event timing and input ownership before native edges. FFPR retains its
own native phase order and grouping.

## Implementation and completion events

`battle_result.cpp` opens session ownership in native Show, before any data is
ready; outer phase prefixes invalidate the previous page before calling native
initializers. Each inner member SetData creates the displayed member's page.
Close/EndWait and battle reset cancel queued reads and release ownership.
The current buffer holds its title, separate essential totals, then detailed
rows. The shared service reads details automatically only when Autodetail is
on; manual I/arrows work in both states. The L binding/log API is removed.
Ordinary status retains its concise character/level headline and uses the same
Autodetail contract. Combined visible learning/job panels belong to current
points/growth details, never automatic totals. FF2/4/5/6 native learning page
updates replace the current buffer; FF1/3 do not have UpdateContentPage, so
that hook's game mask is retained.

Native PointsUpdate inlines IsEndPerformance; there are no direct calls to its
wrapper in the exported result paths. Therefore completion uses the actual
ResultCharacterListController `<PlayPerformance>b__6_0` callback after its
native end-count increment, plus ForcedEndPerformance after native fast finish.
Only the current owner and final callback can refresh/speak completed detail
rows. Early completion lets the first read announce the final rows once.
No new text deduplication or UI polling was introduced. Existing EXP audio's
100 ms feed/stop loop remains audio-only.

Read-only headless Ghidra reused FF2_Analysis with `-process -noanalysis
-readOnly`, no analysis timeout. `ff2_results_native.c` confirms the outer
virtual phase calls, direction/page checks through InputListener, and FF2's
per-member AnyKey advancement and skill-page AnyKeyDown. The initial sandboxed
wrapper did not start Java; the read-only export completed with the existing
JDK outside that sandbox, and the project reported discarded changes.

Input prefixes guard all four manager and six listener entry points. A physical
keyboard state read protects a press occurring before the frame poll; cached
current/previous states protect hold/release. This ownership depends on the
native session, not a nonempty buffer. Controller gestures remain owned until
release after mod-mode exit; controller-only mod mode supplies West/details,
shoulders or D-pad Up/Down review, and D-pad Left/Right first/last. Normal SDL
confirm also reaches the listener while reviewing input remains suppressed.

## Verification

Sol retained this task: native behavior, callback lifetimes, phase grouping and
input bypass paths required discovery, and the resulting readers/input/tests
share concrete state dependencies. No Luna or Astra delegation was used.

`tools/battle_results_check.cpp` compiles the real results reader, review
service, scheduler and native input detours against fake displayed views. It
covers all-six phase/Autodetail contracts, member/combined pages, current-only
manual details, pending-read cancellation, partial/final/forced/early animation
completion, regular status, removal of L, all ten native input prefixes with
zero original calls while reviewing, normal confirm, SDL and mode-exit release.
It is part of build.bat. Runtime rendering/timing is still user-unconfirmed.
Final canonical build passed: 7,052 native-name checks, zero missing; wiring
120 used / 323 defined / zero undefined. All regression checks passed, including
current results and existing route/map-state/modal/NPC/coexistence tests.
All-six PE path assertions passed; the isolated DLL host resolved/forwarded all
47 exports and exited normally. Deployed in all six games and SHA-256 verified:
`D45EE6672A8EE496F037F9E4FC4E5D22C58DBCC43B044582C966B40E9E09F5D3`.
This supersedes prior DLLs, including the FF2-only Tutorial Chamber build. No game was launched or
controlled.
