# Current-screen results buffers (2026-10-12)

## FF2 screenshot review, October 13

The user's later spoken example is `Firion: R. Hand Spear 5, 66%`; their attached
FF2 results screenshot (`codex-clipboard-cb1ac045-2fc0-4abc-99bb-77e160ffe37a.png`)
shows Gil 300 and a Skill Lv. table. R. Hand and L. Hand are visible column headings.
Firion's right-hand column has the weapon-type icon, visible level 5 and a filled
progress bar; his left-hand column has a shield icon, level 7 and another bar.
Maria/Guy/Leila likewise have a character name and two icon/level/bar columns.
No percentage text is printed on this screenshot: spoken 66% is the numeric description
of the visible gauge under the existing user-approved weapon progress contract.
The image establishes the presentation structure, not the exact percentage from pixels.

Source audit: `battle_result.cpp::ff2_weapon_rows` reads the rendered fixedRightHandText/
fixedLeftHandText headings and nameText; `displayed::skill_row` combines visible text
(including the level numeral), the matching visible weapon icon's meaning and the
rendered gauge. The weaponType enum selects a translated label; the enum itself is
never appended as a number. Thus the 5 in this row is displayed skill level, not an
internal weapon identifier. All three fields questioned by the user are represented
on screen; this concern requires no reader code change. Existing visibility/native
phase/sprite checks and scoped before/after progress comparisons remain in force.

User's ship-pathfinding/boarding/disembarking confirmation is recorded separately in
[testing](../TESTING.md#gameplay-confirmations-october-13). Other vehicles, phase-wide I,
title-prompt timing and unreported results cases remain pending. Sol inspected the
provided image and current source; no game launch/probe, new test/build or deployment
was necessary for this evidence/documentation update.

User requirement: replace the L results log in all six games with the currently
displayed native phase's arrow-review buffer. Always announce displayed EXP/gil
totals and each phase headline. Member EXP, individual drops and stat tables
follow Autodetail; manual review/details always remain available. Arrow input
must never advance a native results phase, including held/release input and
the interval before the page buffer is ready. Ordinary confirm advances normally.

## Gameplay confirmation, October 13

The user confirms that FF1's battle-results buffer reads correctly after the shared
deployment. Initial FF2 testing reported missing weapon types and requested phase-wide
I summaries. The later example confirms the weapon label is now read; screenshot/source
review above confirms the hand/level/icon/bar presentation. Phase-wide I, gil speech and
other phase details remain unreported. The user expects FF3–FF6 to work because of the shared results
implementation; this is an expectation, not direct gameplay confirmation for those
games. Specific Autodetail, held/release input, row details and native advancement
cases were not individually reported as tested. The user also confirms lag-free
pathfinding and smooth battle transitions in the tested session.
[Current per-feature status](../TESTING.md#gameplay-confirmations-october-13).

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
  `SkillLevelsUpdate`; the former populates weapons. The outer
  `ShowLevelUpAbilitysInit` combines gil/weapons and optional spell-level rows.
  Its outer hooks were disabled by stale FF1-port rules, despite the unified
  implementation now handling FF2 correctly. Remove those obsolete disables.
- Total fields belong to `ResultGetPointView` below `getPointController.view`:
  gil in all six layouts, EXP in the other five (FF2's serialized EXP references
  are null), ABP additionally in FF5, magic points in FF6. Renderer visibility
  determines availability; FF2 has no character EXP.
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

## FF2 retest, growth deltas and rendered digits, October 12

The user's FF2 retest found the first gil phase silent. The existing installed log
records `frame: stopped BattleResult.Read after its frame budget`; later member growth
and items were spoken. This proves expiry, not the precise runtime cull/opacity cause.
The reader now retains a pending page through native ResultMenuController.UpdateController
events after one frame deferral, until required displayed totals/weapon rows are ready.
New phase, close, destruction and battle reset cancel it. There is no global UI timer or
arbitrary 30-frame page deadline. Container activity and individual Graphic visibility
are separate: a disabled controller script cannot by itself hide its rendered children.
The inspected FF2 prefab scripts are enabled; script state is not claimed as the cause
of this observed failure. [Serialized hierarchy](ff2_result_hierarchy.json).

FF2 PointsInit enables gil/character containers; SkilllLevelsInit replaces the character
container with the weapon panel and retains gil; StatusUpInit shows native member tables.
The dedicated FF2 weapon reader traverses each character's nested contentList, preserving
rendered character and right/left-hand labels, weapon icon description, level and gauge.
The other five games retain the native member EXP division. FF2's spell level-up pages
remain the separate native ResultSkillController levelUpContentList; nothing is collapsed
into an early combined reward readout. [Selected prefab records](ff2_rendered_prefabs.json).

Battle's persistent `Battle results: changed stats` toggle defaults on. With Autodetail
off, the title/essential totals remain automatic and the toggle adds changed rows only.
Arrows retain the full displayed table. October 13 user correction supersedes the prior
row-specific policy: I/controller West reads all deltas for the current phase, from any
cursor position. A phase without changes says No stat changes. Autodetail reads full tables; weapon delta
summaries still require the toggle. Native stat before/after values already belong to
the full table, so its automatic full reading does not also repeat a stat brief.

All six games have native growth tables. Serialized types (native enum names; actual
spoken category labels remain renderer-owned) include:

| Game | Growth table |
|---|---|
| FF1 | HP, Power, Vitality, Agility, Intelligence, Luck |
| FF2 | HP, MP, Power, Vitality, Agility, Intelligence, Spirit, Magic, EvasionRate, AbilityDefense |
| FF3 | HP, Power, Vitality, Agility, Intelligence, Spirit, Luck |
| FF4 | HP, MP, Power, Vitality, Agility, Intelligence, Spirit |
| FF5 | HP, MP, JobLevel, with separate job/master presentation |
| FF6 | HP, MP, with separate displayed magicite bonus |

Native level rows/headlines are preserved too. Conditional/unused rows remain subject
to actual renderer visibility, including FF6's IsDisplayMp check. Reproduce with
`python tools/re/results_growth_check.py`; [all-six serialized tables](results_growth_assets.json)
and existing `ffN_results_edges.tsv` show each per-game SetData calling the shared
ResultStatusContentController.SetData. Its FF2 implementation (RVA 8E3340) exposes
after-values when the multiplier **or** percentage increases. Comparison now checks both
visible components independently; unchanged or hidden after-values never create a delta.

### Exact FF2 weapon gauge scale

Read-only headless Ghidra reused FF2_Analysis (`-process -noanalysis -readOnly`, no analysis
timeout; changes discarded). [Gauge manifest](ff2_gauge_manifest.tsv) and
[rendering follow-up manifest](ff2_rendered_followup_manifest.tsv) index the exports.
Local raw `ff2_gauge_native.c`, `ff2_rendered_followup.c` and
`ff2_results_followup_native.c` are intentionally ignored and preserved locally.

SkillLevelProvider.SettingSkillLevel at RVA 6FC630 computes:

`level = ExpUtility.GetExpLevel(1, points, 1)`

`fill = 1 - GetNextExp(1, points, 1) / GetExpDifference(1, points, 1)`

Both operands are native integer values. GetNextExp (382D30) is next-threshold minus
current points; GetExpDifference (381C40) is next-threshold minus current-level threshold.
At native maximum level 16 it overrides fill to 1. CommonGauge.SetValue (5335A0) writes
Image.fillAmount directly, with no percentage animation in this path. Both weapon and
magic caps are 16 in the extracted constants; there are eight weapon categories, not
eight levels. Thresholds are 0,20,45,70,100,135,175,220,270,330,400,480,570,660,760,860.
Intervals range from 20 to 100, so a positive integer point gain is always at least 1%.
Sub-1% point gains cannot occur in this installed native scale; no fractional carry state
is needed. Do not silently reuse that conclusion after a changed native scale.

`displayed_skill_progress.cpp` captures integer operands at the native gauge producer and
before/after weapon data at SetSkillLevelUpColor (3DC2E0). It never retains character-data
pointers. It requires a matching current rendered weapon icon, level text and fill before
disclosing a change. The user explicitly requested these weapon percentage comparisons;
this access is scoped to current displayed weapon rows, not unused/unequipped skills,
future levels, or other hidden parameters. Example within the requested comparison scope:
`Guy: Sword: level 1 at 5% to level 1 at 10%, gained 5%`.
Integer fractions stay exact until ceiling to whole percent for speech. Gain accounts for
level rollover and treats the full capped gauge as completion, not an extra 100%.
Reset clears current changes. Full weapon review contains its actual displayed level and
gauge; the delta is separate and only automatic with the toggle or manual on its row.
[Native/table checksum and scale evidence](ff2_gauge_scale.json), reproduced by
`python tools/re/skill_gauge_check.py` without launching/loading the game.

### Hit count and damage

The previous FF2 count argument was not a mod-computed attack capacity: native
BattleBasicFunction.CreateHitCount (63D6C0) reads ICalcResult.GetHitCount, interface slot 6,
from its result dictionary and passes it to the UI. Status accuracy/evasion values are
also native rendered fields; the mod does not multiply rates or subtract evasion to
calculate successful hits. Sound count is not sufficient evidence that seven is wrong;
native damage presentation has independent sound control, and no fixed sounds-per-five-
hits relationship was established.

The shared feedback service now decodes the actual displayed hit and damage digits.
HitCountEntity.SetNumSprite (FF2 4920B0) fills serialized numRenderers in string order.
ViewDamageEntity.CreateDamageValue (4B6A20) fills immediate prefab children units-first.
Sorting visible numeric images by actual screen x covers both layouts. Exact glyph names
`Battle_Common_DamageNumber_0` through `_9` are verified in all six bundles; localized
HIT/HITS suffix numbers are rejected as word images. Invisible/unused slots, unknown
sprites and unfinished overlapping layouts cannot substitute raw arguments. Native miss
presentation still requires its visible image. Existing event lifetimes and cancellation
are preserved. [Hit prefab](ff2_hit_prefabs.json), [all-six digit palettes](rendered_digit_assets.json);
reproduce the latter with `python tools/re/rendered_asset_check.py`.

### Delegation and verification

Sol retained native behavior, timing, ABI/lifetimes, gauge derivation, integration and
verification. Luna (`gpt-6-luna`, medium, fresh task-specific handoff) added only the central
preference/menu toggle and 12-locale strings; bounded follow-ups added singular-hit,
no-change and progress formatting strings/descriptions. Sol reviewed all touched keys and
placeholders, clarified progress translations that initially sounded like distance to a
future level, and owns the final implementation. JSON/12-locale placeholder checks pass;
no remaining delegation issues. No Astra was used. This is not a measured usage saving.

Real-reader checks cover all six phases, FF2-only weapon nesting, native readiness after
50 frames, percentage-only/multiplier-only changes, selected-row I, both toggle/Autodetail
states, all 861 gauge point values and rollover/cap transitions, hidden/stale gauge rejection,
rendered hit/damage digits, native completion and all ten input prefixes. Canonical build
passes with zero native-name/wiring failures. Final deployment and the added Prism migration
are recorded separately below/in `docs/PRISM_MIGRATION.md`. Runtime timing, actual on-screen
digit agreement and user speech output remain gameplay-unconfirmed; no game/probe was run.

## FF2 first-page retest and loading follow-up, October 12

The preceding build still failed the user's first-page retest: its log has no gil/weapon
announcement, although Maria's growth page and items speak. Title/load speech reaches
Prism's NVDA backend. This is an unresolved first-page defect in that deployed build,
not evidence of a Prism outage. The correction is deployed to all six games;
the user's first-page retest remains pending.

Native ResultMenuController.Show (FF2 5B9800) reads BattleResultData.IsSkillLevel at
+0x10 and selects state 6, ShowLevelUpAbilitys. Its init (5B9020) calls virtual slot 7
(+0x1A0), ShowSkillLevels (3D9E50), and optionally SetLevelUpList on the spell panel.
Inner SkilllLevelsInit (3DA130) fills gil and equipped weapon rows. These are one combined
outer page. The old outer handler could overwrite its reader with a spell-only buffer;
with no spell gains, that optional owner stays inactive. FF2 now owns this page through
the point controller: gil summary, full displayed weapons, then any visible spell rows.
Autodetail/manual arrows retain full rows; weapon deltas require the toggle or row I.

Only native ResultMenuController post-update may build a pending page. The removed
global next-frame callback could run before its queued inner state populated new views.
During fades, a small rendered total is checked before full-table traversal; recursive
text scans no longer repeat. Points/member SetData require an active matching native
phase. Skipped phases/UI initialization build nothing; every actual character SetData
replaces only that character's growth screen. Phase diagnostics use the async logger.
`tools/re/results_calls.py` checks FF2's first-phase dispatch/combination explicitly.

### Blocking work and context ownership

Automatic battle detection uses native main-game transition and a cached mod boolean,
with one mod-memory cancellation per entry/exit. It does not poll shutdown completion,
scan scenes, write game battle/loading state or call the on-demand Tab fallback during
transitions. However, BattleStart.stop_all could synchronously call SDL stream clear.
Device/stream/gain/queue/clear/close work now belongs to a dedicated below-normal worker.
Game callbacks copy bounded PCM commands and publish gains/cancellation generations.
Old pending PCM is rejected; a full ring cannot drop cancellation. No game pointers or
stack callbacks enter the worker, and driver stalls never cause producer waits.
The researched Windows SDL 3.4.4 dedicated owner loads/initializes/pumps the SDK;
the native callback only publishes readiness. Permitted device/stream operations
use the separate audio worker. Shutdown only signals teardown; SDK lifetime is the
process, and DllMain performs no wait. [Ownership evidence](THREADING.md#windows-sdl-ownership-and-reusable-template).

Logger WriteFile/rotation formerly ran under a shared producer critical section. Its
new below-normal worker owns I/O; producers only format/attempt bounded enqueue. A
full/contended queue records diagnostic loss, never speech/input loss or deduplication.
FF2 gauges no longer repeat allocating/cache-building native EXP utility calls: local
integer math uses the exact extracted threshold table, checked against exp_table. The
original native provider still runs once unchanged. Cache keys clear at session/scene
boundaries, without probing every old gauge on each setting call.

Native lifecycle/parent observations now supply all current UI/field instances;
no active callback calls a whole-scene object search. Additive loading preserves
valid observations; Title clears field roots only, retaining incoming UI owners.
Entity/native-name capture waits for fade completion and uses four-entry / 0.5 ms
owner batches. Remote formatting, entity sorting/naming, baked routing/floods/corners
and buffer assembly use owned workers. Queue pressure retries without an inline
fallback or game wait. Coroutines spread only necessary owner reads; native pointers
remain in owner registries. [Full audit and native provenance](THREADING.md).

### Validation and remaining work

Real-reader checks cover no-spell FF2 outer entry/nested callbacks, skipped growth phases,
per-character screens, Autodetail/deltas, all-six input/native paths and every FF2 gauge
point. Logger/audio checks block fake disk/driver calls while producers finish, including
full queues and cancellation/copied PCM. Battle-context check uses the real ChangeState
detour/state code to verify continuation, single cleanup and no automatic game query/scan.
These are offline ownership/nonblocking tests, not game load/frame measurements. The
mod necessarily performs small owner-thread reads/input work; literal zero instruction
overhead is not established. Canonical worker/route/buffer/persistence/native-reference
checks and isolated DLL smoke pass. Gameplay first-page speech and modded/unmodded
load/frame timing remain unconfirmed; no game/probe was launched.

Final follow-up DLL, deployed/hash-verified in all six games:
`DA45D46CA76664B2E722D20AA898B7AD10389642F97E009D4D41D2B7B818C95F`.
Canonical build: 7,061 native-name checks, zero missing/undefined wiring; all
results/worker/route/input/cache checks and isolated DLL host pass. Prism prerequisites
were checked without changes and all six game-root Tolk.dll files remain absent.

## Weapon labels and phase-wide changes, October 13

The user's FF2 retest reads Firion/Maria/Guy hand levels and progress, but omits weapon
type. The first-page buffer is therefore producing rows; this is a missing icon label,
not a wholly silent buffer. The user also replaces the prior row-specific I behavior:
I/controller West must repeat all current-phase changes, just like the automatic brief.

Exact FF2 native evidence: results SetData calls SkillLevelContentController.SetBothHandsData
(`3DBF80`, ABI self/OwnedItemData/MethodInfo), which sets weaponType and IconTextUtility
directly, without calling the separately hooked SetWeaponIcon (`3DC460`). Capture the
same bounded owned type/visible-sprite meaning after either producer. The renderer's
current sprite must match the captured sprite; hidden/changed icons, ability reuse,
nonweapon enums and missing fields cannot disclose a weapon label. Existing eight-type
translations remain; no guessed weapon is inferred from hand or progress. Full rows and
delta summaries now receive the actual Sword/Knife/Spear/Axe/Staff/Bow/Shield/Unarmed
icon meaning. No asset/master lookup or Unity traversal is moved onto workers.

Read-only `tools/re/ff2_weapon_icon_check.py` asserts both exact producer paths, preserving
matching binary SHA-256 and raw native instructions locally in ignored
`build_check/re/ff2_weapon_icon.txt`. Raw commercial code is excluded from publication.

The review worker retains its already-built complete change brief. Manual I publishes
headline plus that brief as one interrupting input event, independent of cursor,
Autodetail or automatic-summary preference. It does not join a table on the native
thread, read another character/phase, advance the game or add text deduplication.
Obsolete row-detail alignment/capture is removed. Existing generation/readiness/FIFO
and phase cancellation still govern pending input.

`battle_results_check` now links the actual displayed_conditions reader rather than
faking skill names/rows. It invokes real registered producer detours with fake native
presentation, exercises all eight types, visibility/sprite/ability/invalid-type guards,
and verifies FF2 first-page review/Autodetail/deltas. Phase-wide I checks cover headings,
changed/unchanged rows, no-change phases, automatic-summary off and all six games.
Canonical build passes 7,063 native names, zero missing/undefined wiring; results,
worker/input/speech/route checks pass. Gameplay retest of these corrections remains
pending; FF1 reading and smooth battle transitions retain the user's confirmation.

This build is deployed/hash-verified in all six games:
`FA433F55BD5743D3FFAA57450528E796C39FB875495BF329C5015E0F95A7D99E`.
Isolated DLL smoke passes; Prism dependency checks were read-only. No game was launched
or probed by the agent. [Current focused retest](../TESTING.md#current-ff2-results-and-vehicle-retest-october-13).

## Essential gains and Verbose/Brief detail, October 13

Displayed native learned-spell/ability and weapon/job level-up messages now live in
essential summary lines and the review buffer, regardless of Autodetail or the optional
changed-stat announcement switch. Native job-mastery message fields receive the same
treatment; no message-text heuristic/deduplication was added. Visible native phase and
owner generations still govern collection; no later rewards are read early.

Speech / Stat delta detail defaults to Verbose. Brief formats only gains/losses from
rendered before/after numbers, with their native units; FF2 primary EvasionRate and
AbilityDefense percentages and secondary count/multiplier changes are independent.
Example 51x2% -> 55x2% becomes Evasion: gained 4%; a secondary-only count gain uses x.
Brief also applies to matching visible FF2 weapon progress and equipment comparison
buffers/Autodetail. It leaves raw item-stat/description panels intact. Autodetail Brief
speaks changed rows once; unchanged rows remain reviewable as No change. I/West still
reads all current-phase changes, independent of cursor/automatic-summary preferences.

Settings changes rebuild current comparisons without automatic replay. Extra item
panels retain native page ownership for I. Actual-source all-six phase/visibility,
learning/level-gain, Brief/Verbose and input tests pass; full build/smoke/all-six deployment
record and pending gameplay checks are in [MENU_DETAILS.md](MENU_DETAILS.md).
