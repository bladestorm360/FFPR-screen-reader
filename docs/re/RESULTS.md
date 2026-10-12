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
Arrows retain the full displayed table. I/controller West on a growth/weapon row reads
that row's delta; an unchanged row says No stat changes. On titles/totals it repeats that
line, without reading other rows' changes. Autodetail reads full tables; weapon delta
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
