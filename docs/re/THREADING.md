# Read-only snapshots and worker ownership

October 15 soundscape update: the native owner copies only published pathfinder entries,
at most 512 entries/0.25 ms per slice, into four worker-prepared/recycled buffers. This is
part of fixed subscriber/audio work before the existing 0.5 ms deferred-dispatch allowance;
it is an additional bounded contribution, not a claim that total callbacks cost 0.25 ms.
The audio worker owns custom/embedded WAV decoding, voice clocks, mixing and OpenAL Soft
HRTF loopback into the existing SDL output. No native pointers cross that boundary;
generation cancellation remains independent of queue capacity/driver stalls. Actual HRTF,
capture, overload and fake-SDL measurements, state/count/age limits and remaining gameplay
performance gaps are recorded in [SOUNDSCAPE.md](SOUNDSCAPE.md#final-verification-receipt).
Canonical build/all checks and isolated smoke pass. The user lifted the hold; all six
games are deployed/hash-verified, with gameplay/performance confirmation still pending.

Current October 14 investigation: intermittent FF2 world-map delays remain open;
earlier qualitative confirmations do not cover this reported session. The new input
transition guard is built/offline-verified and deployed/hash-verified in all six.
[Diagnosis, evidence and scoped remaining fix plan](#intermittent-world-refresh-and-transition-input-october-14).

October 13 exit follow-up adds owned route-distance/grouping snapshots, native
stair masks and FF6 cropped room geometry. [Evidence, limits and validation](MAP_EXITS.md).

User contract, October 12: pure computation, formatting, speech, audio, scanning,
extraction, pathfinding and I/O must not hold up native processing. Game callbacks
only observe current state or copy bounded ready values. They never wait for a
worker, queue, lock, I/O, RPC, sleep or join, including loading and shutdown.
Thread-affine Unity/IL2CPP objects remain on their native owner. Coroutines only
spread necessary owner reads; synchronous heavy calls inside them remain blocking.
Previously approved remapping, autowalk and facing stay on the native input route.
No new speech deduplication or game launch/probe permission.

Final October 12 build and isolated DLL smoke pass. Deployed/hash-verified in all six:
`DA45D46CA76664B2E722D20AA898B7AD10389642F97E009D4D41D2B7B818C95F`.
Native-name checks: 7,061, zero missing; wiring: zero undefined. Actual-source checks
listed below pass. Runtime speech/loading/frame confirmation remains user-side.

October 13 user confirmation after the latest deployment: pathfinding has no lag,
battle transitions are smooth, and the FF1 results buffer reads correctly. FF2 is
being tested; FF3–FF6 results are expected through shared code but not directly
confirmed. These are qualitative gameplay observations, not instrumented latency or
aggregate frame/load measurements. [Current coverage](../TESTING.md#gameplay-confirmations-october-13).

## Implementation and ownership

| Subsystem | Native owner work | Worker work / handoff |
| --- | --- | --- |
| Battle detection | Native transition flag, one mod-memory cancellation per entry/exit, immediate original continuation. No shutdown poll or scene search. | No worker completion is required to disable a context. |
| Speech/Prism | Copy bounded ready strings/parts and attempt raw ingress publication. | Exclusive speech owner cleans/chunks/joins and serializes Prism RPC/speech/braille. Generation-based interruption; no shared game/worker lock. |
| Settings/waypoints | Capture owned preference/file snapshots. | Separate I/O lane, latest pending preference save, checked temporary writes and atomic replacement. Failed/busy preference saves retry on the speech owner's 250 ms timeout; no callback wait or error retry spin. Waypoint jobs retain their snapshot under queue pressure. |
| Logger | Bounded formatting and non-waiting enqueue. | Below-normal file owner writes/rotates. Full/contended diagnostic queues count lost diagnostics; no new speech/event deduplication. |
| SDL/gamepad | Read copied physical state/edges; focus and native input routing. Publish initialization readiness only. | Exclusive dedicated Windows SDL owner loads/initializes/pumps/gamepads and publishes owned snapshots. Audio worker owns permitted device/stream calls. |
| Mod audio | Fixed copied PCM commands or owned synthesis parameters; atomic gains/cancellation. | Below-normal audio owner synthesizes dynamic sounds and opens/streams/clears/closes devices. Cancellation cannot be lost when the PCM ring is full. |
| Baked maps/state | Read atomic readiness and copy current flags/pose/rules. | Data worker prepares immutable grids, translation tables and remote projection. Compute worker uses copied flag resolver/locale. No callback cold decoding. |
| Entity capture | Current native list/dictionary slots and fields under one central 0.5 ms elapsed capture budget. Lifetime/context validation and owned-handle publication remain here. | Remote formatting, exact-tag NPC numbering, sorting, inclusion filters, transition destination matching and live-exit cache construction. All six category views are prepared together; owner publication moves names/handles under the same elapsed budget. |
| Routes/reachability/beacon | Copied pose, map/layer, vehicle/group data, current obstacles and rendered next-floor name, under the same central 0.5 ms elapsed capture budget. Validate context/request before publication. | Baked A*, floods, boarding and cross-floor Dijkstra run on the separate Navigation lane; route remainder/corners, formatting and replacement of owned name tokens use owned cancellable snapshots. Same-floor route publication avoids an unnecessary second worker/frame round trip. No native pointers. |
| UI/results/status/bestiary | Current native phase/page detection and ready visible fields. Native lifecycle supplies current controllers; input is owned immediately during buffer preparation. | Owned buffer construction, group indexing and full-page text assembly. Manual actions are retained FIFO, then replayed at most four per frame while the same page/generation is active. |

The necessary owner reads are bounded by the native UI's displayed rows or the explicit
capture batches. A time budget is checked between operations; it does not preempt a
single engine call. This source contract and offline stall tests cannot certify literal
zero instruction overhead or actual load/frame timing.

`background.cpp` prepares Data/Compute/Io lanes during boot; a separate normal-priority
Navigation lane is also created during boot for full routes, reachability floods and
corner computation. Data/Compute/Io remain below-normal priority. Callback publication uses
`try_to_lock`, with no inline fallback. `worker_job.h` sends only owned Snapshot/Work
values to workers. Native callbacks and handles remain in its owner registry. An epoch
invalidates old work; busy/full publication keeps the owned job for a later frame.
Workers may wait on their own queues; the native frame handoff uses a non-waiting acquire.
Callbacks run before posted worker completions, preserving current native context.
Process-lifetime worker state avoids destructor waits from DllMain.

October 13 speech ordering follow-up: `say_sequence` publishes an owned heading/focus
event to speech ingress; only the first nonempty part interrupts. Known mod-owned
prompt callbacks use an owner-only queued scope, never around native game calls or
workers. Capture/generation checks retire stale category/route replies. Ready modal
timers are removed; SDK calls, cleaning/chunking and timing logs remain on the speech
owner. Tagged request/capture/enqueue timestamps contain no native pointer. See
[speech producer audit and whole-pipeline evidence](SPEECH_ORDER.md) for queue bounds,
pressure recovery, cancellation/data-age limits and fake-SDK versus audible timing.

## Native lifecycle evidence and bounded field capture

### Boarded vehicles and wrapped worlds, October 13

Subsequent user retest after `4013355`: FF2 ship pathfinding works, including boarding/
disembarking across vehicle/foot route legs. Other vehicles remain untested. This is
qualitative gameplay confirmation of those reported cases, not numerical timing or
every loaded-save/story-state combination. It supersedes pending ship-landing statements
below; the initial failures and offline evidence remain historical provenance.

The later FF2 report narrows the initial lag-free confirmation to tested land routes.
The user's own current log records Ship boarding and No path from world map 1 cell
`(126,129)` to `(110,98)` and `(218,43)`. Existing bakes already contain ship group 4
and native landing group 2; no map/master-data change was needed.

Two defects reproduce offline: only the land-to-parked-vehicle boarding plan existed,
so the ship-only search could not reach inland towns; and A*/flood rejected world
edges instead of wrapping. The same FF2 bake connects 37,212 ship cells with wrapping,
versus 22,471 reached by the old unwrapped fixture flood, excluding Deist's shores.

The owned planner now routes the already-ridden vehicle to a legal landing and then
walks with owned automatic-vehicle rules to the destination. It omits boarding again,
keeps a manual land boundary, and respects specific legal water-to-shore pairs or
other vehicles' landing terrain. Live blockers/directional/event rules remain. Two
floods score ride/walk distances, then A* searches one selected landing. Known land
goals skip a doomed ship-only search. The filter uses one multi-source foot flood
from all legal reachable shores, with unique seeds bounded by width times height.
Each additional reach grid is bounded by the validated baked world dimensions.

All scans/floods/A* remain on Navigation workers with cancellation. Native owner work
copies ready transport rules and rejects publication after vehicle ID/type changes;
preserved routes also key transport identity. Search, heuristic, turn/direction grouping
and beacon corners now honor world seams. Beacon points project along the chosen leg,
not the long Cartesian span to the opposite edge. Existing native/manual movement,
boarding/landing and special flight controls are unchanged.

Actual-source route-worker regression reproduces the old ship-only failure, then
validates both logged targets, legal shore/ship cells, filter inclusion, cancellation,
westward seam directions/beacon corner and zero native calls on workers. Final logged
FF2 fixture routes: 7.516/11.974 ms, two phases each. Across all six real bakes, 103
eligible world/transport cases pass, with all automatic groups owned in the fixture;
85 combinations have no eligible start/land destination and are reported separately.
Worst observed route/filter compute: 25.100/12.319 ms. The same wrapped 103-case scenario
before bypassing the doomed search observed up to 105.122 ms; host load varies.

General world benchmark: 24 worlds/16 far routes, worst flood/route 5.854/49.694 ms.
Wrapping changes its selected farthest endpoint, so this is not an unchanged-scenario
comparison against the older unwrapped benchmark. Native/autowalk movement already
normalized seam steps; an intermediate replay failed because it expected raw coordinate
subtraction. That expectation now matches the native wrapped axis. All 2,292 sampled
map routes, real floor/facing handoffs and land Fynn/canoe regressions pass. Intermediate
reports are retained locally in build_check. Target-game capture/queue/audible/frame
timings and actual boarded landing behavior still require gameplay retest; these are
offline compute observations, not universal latency guarantees.

There are no active `unity::find_object` / `unity::find_objects` callers in core or
per-game sources. Central class-keyed caches receive native instances from their own
initialize/show/set-active/phase events, or known parent fields. Every use checks class,
Unity lifetime and component attachment; an old owner's close cannot clear an incoming
owner. Additive scene loading retains valid observations. Title clears field roots only,
so freshly observed incoming UI controllers are not lost.

`tools/re/field_capture_calls.py` records 20 native functions per game, binary SHA-256,
RVA, instructions and named callees in `ff1_field_capture.txt` through
`ff6_field_capture.txt`. It asserts required lifecycle/capture methods in all six
matching GameAssembly binaries. Particularly useful FF2 evidence:

- FieldPlayerKeyController.Init, 94C5C0, stores its fieldPlayer at +20. The base Init
  body is folded; do not depend on a base detour being invoked.
- FieldController.SetControlPlayer, 2894C0, invokes controller virtual slot 14
  (+210/+218) with the current player; CreateFieldPlayer, 2776E0, assigns +98.
- TransportationController.NeedInteractiveList, 551190, allocates a list and filters
  modelList through Transportation.CheckNeedInteractive before returning MapObject.
  The new cursor reads one current slot and uses the same predicate, avoiding that
  whole-list allocation. CheckNeedInteractive, 553440, is read-only and checks the
  current transport's short attribute list.
- KeyHelp.Initialize, BattlePlayerInfomationController.Initalize, Shop.Show,
  ExtraSound.Initialize, LoadGameWindow.SetActive and the FF4–FF6 timer initialization
  hooks provide current UI owners. Shop/save/music prefix observers only publish the
  owner before nested native callbacks; original native functions proceed immediately.

`navigation.cpp`'s EntityCursor reacquires current roots and reads one list/dictionary
slot at a time; it never hands collection pointers to a worker. Existing cross-list and
story-variant inclusion guards remain. Owned vehicles erase native pointers; vehicle
and automatic-group caches are tagged with their map. Route capture, live entity
conversion and remote native-name capture share the central 0.5 ms elapsed capture budget;
there is no fixed entries-per-frame limit. Invalid map/battle/readiness generations retire
capture instead of publishing stale values. Native collision revalidation checks one
approved movement edge using the actual local player position; baked search never rewrites
native grids.

## Navigation latency follow-up, October 13

The user's October 13 FF1 log showed repeated world scans of 872.7–876.0 ms with 47 live
targets. The old fixed four-entries-per-frame stages stacked scan/prune, filter snapshot,
name/handle publication and route work. Field route capture, live scans, filter snapshots,
remote labels and publication now use the shared 0.5 ms elapsed owner-read budget, without
the old four-entry cap. A single filter-worker pass prepares all six category views while
retaining the central NPC naming/grouping guards. Warm category changes use those views
without recapture, renaming or queued work. Ordinary input-triggered live refresh does not
hold browse readiness; map/settings invalidation still requires fresh views. Lifetime is
rechecked for each entity, and a refresh completion preserves the latest selection if the
user has moved it meanwhile. Same-floor routes publish directly from the worker result;
cross-floor native name completion retains its owner-thread continuation. Route logs now
include capture, compute and total milliseconds.
After player movement, category entry refreshes the live row positions/distance sort
without waiting for a full entity scan, preserving nearest-first selection. Route-log
capture/total values include elapsed frame/queue handoffs; they are not game-thread CPU time.

Actual-source offline checks: `entity_scanner_check.cpp` made 2,000 rows ready in 3–4
ticks; cached category switching measured below 0.1 µs per switch in the fake-native
fixture. Blocked scan/filter browsing, nearest-first entry after movement,
latest-selection retention, despawn skipping and stale-map rejection passed.
`entity_filter_check.cpp` prepared six views for 10,000 rows in about 2–4 ms.
`route_worker_check.cpp` measured the FF2 Salamand–Semitt route and flood at about 1.3–1.5 ms,
with zero worker-native calls and cancellation checks passing.
`navigation_worker_check.cpp` verified that a blocked unrelated Compute owner does not
block the separate normal-priority Navigation owner. `navigation_perf_check.cpp` exercises
all six games' real world-map bakes: 24 world records, 16 representative reachable routes,
with final-run worst flood 7.454 ms and far route 14.260 ms, zero failures.
Eight records have no foot-start cell; some starts are isolated, so this is representative
coverage, not an exhaustive benchmark of every origin, movement mode or destination.
That check exposed an A* zero-movement early return that published internal layer 0; the
caller layer is now preserved. The first benchmark caught that endpoint mismatch; its
corrected replay passes. The scanner fixture also retires deliberately blocked jobs before
C++ static teardown, and its canonical check catches every nonzero process status.
Canonical build passes (7,061 native-name checks, zero missing; zero undefined wiring),
as do all existing route/autowalk/input/worker checks and the isolated DLL smoke host.
A final sandbox build stalled at process handling and was interrupted; the canonical
build and smoke then passed outside that sandbox. All six installed DLL hashes match
`ED888BC45404857EA010B80991E38DAD8107F9047FC1D8F34075BC6E9421C7C2`.
No game was launched or probed. Actual input-to-speech and load/frame timing remains
user-side; these compute/fixture timings do not include screen-reader delivery latency.

Other current-owner paths use the native parents' item/equipment view fields, shop
list/info fields, bestiary view/list/content chain, config details and game-specific
ability windows. Music's only optional duration lookup is the current PlayerList's
Dictionary<int, SoundPlayerList>.TryGetValue ABI (`bool(self,int,reference-out,MethodInfo)`),
matching all six dumps; it does not enumerate the dictionary or discover a scene owner.
Native metadata, fields and lifecycle methods are name-checked in the canonical build.
Actual runtime hook ordering on every screen remains a user-side confirmation.

## Baked-only routing

User clarification, October 12: every map must be baked. Missing baked data is a bug
that must be extracted/baked, never permission to fall back to a game-side search.
The deprecated MapRouteSearcher, breadcrumb chain, native fallback formatting and
all temporary CurrentMappingData writes are removed. Missing data says Navigation
map data unavailable. A speech brevity threshold is not a route-distance limit.

`baked_nav.cpp` captures only current context, then uses a private worker SnapshotScope.
A*, reachability floods, boarding and cross-floor Dijkstra check owned cancellation
state. The immutable published route is shared; workers calculate its remainder/corner.
Native-only map names are requested in a separate owner continuation and replaced in
owned strings on the worker before publication. Entity, waypoint, puzzle, beacon and
floor/autowalk continuation use this service. Publication rejects changed map, request,
player cell or unsafe context; manual input cancels autowalk during planning before any
movement is owned. There is no synchronous worker future/wait or inline full search.

The real-source route worker fixture uses actual FF2 bakes, Fynn's shop route and the
Salamand–Semitt Falls world route with the copied canoe group. It checks cancelled
A*/flood work and asserts zero native API calls on the worker. Existing all-six baked
route/autowalk replays validate route geometry and movement phases separately.

## Windows SDL ownership and reusable template

SDL 3.4.4 Windows permits the first/exclusive initializer to designate its own SDL
main owner: SetMainReady records only an unset owner; IsMainThread checks SDL's
video/event/main IDs. Event initialization records that owner, independently of
Unity's window thread. FFPR does not use SDL video integration or steal host ownership.
The dedicated physical owner initializes SDL and directly initializes audio; permitted
subsequent audio calls use the separate audio owner. Backend failure hands native
input back. Focus/held edges, quick presses, hotplug, stale snapshots and failure
seeding remain explicit. Shutdown signals without joining; process lifetime owns SDL.

Packaged third_party/runtime/SDL3.dll: PE 3.4.4.0, SHA-256
`132E7F953951944F7628861227ECAEA8C37A5A484F004C2072B7F3A1C343FA73`.
Runtime gate requires the researched API version 3.4.4.

Primary source/applicability/sequence/tests are preserved in the canonical
[Windows SDL lesson](../../../../../codex/templates/accessibility-mod/references/sdl-worker-ownership.md),
linked from the skill, architecture reference, project blueprint and persistent guidance.
It cites release-3.4.4 SDL.c, IsMainThread, SetMainReady, PumpEvents, PollEvent and
OpenAudioDevice. Template updates passed link resolution and zero-change idempotence.
The [cross-platform ownership reference](../../../../../codex/templates/accessibility-mod/references/sdl-cross-platform-ownership.md)
separately constrains Linux backends and macOS helper ownership. This FFPR work implements
Windows only; research does not certify another platform/loader/ABI.

## Hook allocation and boarded-save follow-up, October 13

The user's own FF2 session at 07:23:11 installed 29 hooks and failed 199 with
MH_ERROR_MEMORY_ALLOC. The first failure follows 27 GameAssembly readers plus four
runtime trip-wires: exactly the 31 slots in the old 4 KiB/128-byte trampoline block.
The two later successes chain onto mod detours in a different address neighborhood.
Title, scene-loaded, field-map/player lifecycle and boarding observers all failed.
The later scanner captured zero live/remote objects; V announced Walking. Missing
owner roots explain that session's inactive navigation without implicating the baked
vehicle grids or reverting worker ownership. The exact process memory layout was not
captured; the allocation mechanism below reproduces its capacity/error signature.

`buffer.c` now allocates a full 64 KiB Windows allocation-granularity region instead
of 4 KiB at each aligned reservation address. The old allocator could not reserve the
remaining 60 KiB at another aligned base, wasting its only nearby usable region in a
crowded rel32 window. The corrected block has 511 of the existing 128-byte slots.
It preserves the ±1 GiB near-target range, instruction relocation, 16-byte coexistence
resume, original chaining, batched enable and refusal to overwrite another mod's patch.
Blocks still grow only when needed and release when their final slot is freed. This
is boot allocation, not new game-thread pathfinding or a hook-retry loop.

`coexist_check.cpp` reserves virtual address space in its isolated process around
synthetic functions, leaving exactly one free 64 KiB region within the production
allocator's reach. Only target code/trampoline pages are committed, not the surrounding
2 GiB virtual reservation. With the old allocator, hook 31 returns MH_ERROR_MEMORY_ALLOC;
31/384 succeed (`build_check/coexist_crowded_before.txt`). With the corrected allocator,
384/384 create; every original trampoline returns correctly, a late relay executes,
released slots reuse, and final teardown frees the block. The existing spacious
512-hook multiple-block and Dobby/MonoMod/funchook chain/unhook checks also pass.
This replaces the inadequate earlier spacious-only regression for this failure.

Boarded-load initialization was audited separately: existing FieldMap.StateInit and
FieldPlayerKeyController.Init observe ready native roots without requiring GetOn.
FF2's archived SetControlPlayer calls controller virtual Init with the current player;
the concrete key Init stores fieldPlayer at +20. Current vehicle queries read the native
transportation controller for route snapshots. Native Transport
state 4 is already navigation-ready. `component_cache_check.cpp` now executes the actual
lifecycle observers with copied fake loaded Transport/Player owners, original-first
initialization, incoming-owner replacement, invalid/unready/destroyed handles. It passes
without a boarding event. Existing real-bake vehicle routes/filters and cancellations
remain covered. These do not certify the game's save deserializer or every vehicle/story
state; loading the user's boarded save after successful hook installation remains the
decisive gameplay check. No speculative vehicle-state rewrite was added.

The explicit V/controller vehicle-name query separately used only the cached GetOn/
GetOff announcement. It now reads that same current native transportation snapshot
while the field is navigation-ready, including the actual game/id-specific name.
A native Player row says Walking even if a prior boarding cache said Ship. Unready
contexts and absent roots retain the existing cached fallback, with no scene scan or
automatic announcement. `vehicle_state_check.cpp` executes the actual query with fake
ship/airship/Snowcraft/native-player snapshots, missing/unready ownership and no GetOn
event. It passes; exactly one current snapshot is read per ready explicit request.
This does not add periodic reconciliation or change boarding/landing speech priorities.

The title prompt's native producer timing correction is documented in
[title evidence](TITLE_PROMPT.md#current-timing-and-hook-installation-follow-up-october-13).
Canonical build/name/wiring, actual-source lifecycle/title/speech/route regressions and
isolated smoke checks remain the acceptance route. Current installed DLL and user checks
are in [testing](../TESTING.md#hook-installation-title-and-boarded-save-retest-october-13).
Sol handled implementation/review/verification; no delegation, game launch/control/probe
or Prism dependency change.

## Delegation record

All bounded handoffs used Luna (`gpt-6-luna`, medium, fresh task-specific context).
Sol retained discovery, architecture, lifetimes, native ABI, debugging, integration
and verification. No Astra was used; no measured token or reserve saving is claimed.

| Luna task | Outcome and Sol review/validation | Remaining issue |
| --- | --- | --- |
| Controller snapshot adapter | Implemented specified copied-state adapter. Sol corrected first-focus/reconnect trigger seeding. Real-source fake SDK tests pass, including blocked initialization and failed backend handback. | Runtime physical controller confirmation. |
| Deprecated route removal | Removed five-file native fallback/chainer/grid-write path. Sol reviewed deletions and absence of callers; baked route regressions pass. | Missing future maps must be extracted/baked. |
| Bestiary owned buffer | Implemented known snapshot/locale worker pattern and four-action FIFO replay. Sol reviewed ownership; blocked-compute actual-source checks pass. | Runtime bestiary navigation. |
| Review queue replay | Removed arbitrary pending-input drop cap and preserved same-page actions. Sol corrected localized initial-Autodetail replacement behavior. Fifty queued actions/manual-details checks pass. | Runtime arrows must never advance native results. |
| Command-window lifecycle | Converted command bar/item/equipment/FF1 ability to specified native owner cache paths. Sol reviewed timing/fields and incoming-owner close behavior; canonical checks pass. | Runtime native screen entry/return. |
| Bestiary lifecycle refs | Replaced all five actual scene finds with specified parent/current-controller paths (handoff had said six). Sol verified all-six chains/lifetime guards and build. | Runtime detail/list/formation transitions. |
| Game-specific window lifecycle | Added specified FF2/3/4/6 ability and FF2 keyword native owner observations. Sol reviewed class guards/close identity and canonical name/build checks. | Runtime game-specific screen transitions. |
| October 13 navigation latency documentation | Recorded known implementation/evidence and concise user checks. Sol reviewed against source/reports and corrected minor lane/publication/benchmark wording. | Gameplay timing remains user-side; final build/deployment recorded below and in memory. |
| October 13 settings-menu speech sequencing | Implemented the specified owned heading/first-item sequence only in mod_menu.cpp. Sol reviewed native readiness/later focus and verified actual-source menu/input plus canonical checks. | Real F8 spoken completion remains user-side; no implementation issue outstanding. |

## Offline validation and runtime boundary

Canonical checks use real source and fake blocked external APIs, without launching
or probing a game. Coverage includes MPMC producer progress, logger stalled/full disk,
audio stalled loading/driver/cancelled copied PCM, exclusive SDL ownership, controller
focus/quick edges/hotplug/stale/failure handback, native battle detour continuation,
FF2 no-spell combined result page and skipped/per-character phases, all-six review input,
exact gauge operands, worker buffer replay/cancellation, settings blocked writes/queue
pressure/partial-write recovery, entity worker grouping/selection and native cache
replacement/lifetime. Native PE paths and all-six native-name/wiring checks supplement
these isolated tests. Build and isolated DLL smoke/deployment are recorded in memory.

Implemented and offline-verified behavior is separate from gameplay-confirmed behavior.
FF1 results reading, pathfinding responsiveness and smooth battle transitions now
have the user's October 13 confirmation above. FF2 first-page speech, unreported
SDL/controller/speech-order cases and modded-versus-unmodded numeric same-save timings
remain user-side. No game or runtime probe was run by the agent.

## Large-map loading dispatcher follow-up, October 13

User reports a large stall leaving FF2 Castle Deist for the world map, despite responsive
navigation afterward. Read-only existing FF2 log records last Deist route output at
247028.718, Entering World Map at 247037.781, then the ready world entity scan at
247037.796 (38 live entries, 17.0 ms elapsed across frames). The roughly nine-second
output gap precedes the scan; it does not identify which native/mod function consumed
CPU, a worker wait, or an unmodded loading baseline. No agent game launch/probe.

Static audit: SubSceneManagerMainGame.ChangeState prefix invalidates context; native
ChangeState then executes; Player/Transport post-observation starts navigation only after
native construction. FieldMap.StateInit and FieldPlayerKeyController.Init observations
publish current instances after their originals. ChangeMap/FieldReady do not start a map
scan. Prepared grid/JSON tables remain immutable worker data; no cold world-grid decoding
or native MapRouteSearcher is introduced. Main publication uses TryAcquireSRWLock;
background owner publication uses try-lock. Blocking waits remain on boot, data/speech/
SDL/audio/file owners, outside game callbacks. Thread-affine native reads remain on owner.

The dispatcher still had an aggregate work hole: each frame drained every ready post
and pending callback, even though individual captures were bounded. Ready posts and
owner readers now share a 0.5 ms elapsed dispatch allowance after the fixed input/audio
observations. Half is reserved for readers, and the reader cursor rotates so a persistent
capture cannot monopolize every frame. Leftover posts remain in FIFO order; readers run
at most once per tick. This is an elapsed work bound, not a small fixed entity batch.
It cannot preempt an already-running native read/callback; those retain their own bounded
readiness/lifetime contracts. Fixed subscriber CPU and callback tails are measured separately
from the nominal allowance. No worker lock/wait/drain barrier is added to the native route.

worker_job now checks cancellation after computation as well as before it: an obsolete
owned result is destroyed on its helper rather than unnecessarily posted for destruction
on the game thread. Native callbacks remain in the owner registry and are never moved
onto a worker with their engine pointers.

FrameTiming logs mod dispatcher/subscriber CPU, original InputSystemManager.Update time,
interval since the prior update, and owner-ready work counts when mod >=1 ms, native
>=25 ms or interval >=250 ms. LoadTiming logs prefix, original ChangeState and post-observer
CPU for each state transition. These diagnostics use the existing nonblocking logger;
there is no speech deduplication, game sleep or runtime probing. Interval gaps can include
other engine/render/OS work; native-update time excludes the rest of the frame. These
boundaries avoid attributing the entire loading gap to asynchronous path computation.

Actual-source frame fixtures add a 300-callback, synthetic 50-us work burst, native update
continuation on each slice, preserved complete output and fairness; they also keep a worker
publication lock held while native input and an owner-side post proceed. An owned result
cancelled during blocked helper computation must be destroyed on that helper without
owner publication. Existing blocked data/Compute/Navigation/speech/audio/file fixtures
pass. The final synthetic burst's worst dispatch is 0.510 ms; its callbacks deliberately
consume about 50 us each. Held worker publication lock permits 100 native updates and the
owner-side post; a cancelled owned result dies on the helper. Existing scheduler lifetime,
1,000 posts, blocked data/compute lanes and cancellation checks pass. All six actual-world
vehicle worker cases and prepared category checks also pass; fake/native-owning calls on
workers remain zero. These are offline/fake-native observations, not target-game callback,
Unity frame, first-audible or comparable modded/unmodded loading measurements.

Canonical build: 7,077 names/zero missing, zero undefined wiring, all regressions pass.
Isolated DLL smoke and all-six deployment/hash verification pass:
`E3F6C174CAE0FE65D969E3258B72936035EBA579BE051084B1BBB0845D0602BE`. Prism unchanged; no game launched/probed.
Subsequent user gameplay retest: the Deist-to-world transition lag seems fixed and is
accepted as good enough for V0.9 release preparation. This confirms the reported case
qualitatively; no absolute zero-overhead or all-map numerical timing claim follows.


## Selected-destination world latency follow-up, October 14

Latest user FF6 log: directions generations 153/155/169 took 468.461/958.265/1009.128 ms
from request to SDK submission. Their route captures took 16.815–17.658 ms, computation
0.636–2.246 ms and total route stage 33.433–34.230 ms. Speech queue was 0.151–0.211 ms,
SDK calls 0.438–0.597 ms. Thus most logged delay preceded route planning; SDK submission
is not first audible output. Native frame/Unity costs are not inferred from worker placement.

The prior flow awaited when_filter_ready after movement, including full world exit
reachability, sorting and obsolete-origin republishing, before requesting an otherwise
small route. Existing live target requests now bypass that wait. Full list refresh is still
scheduled; browsing/category selection keeps its fresh-generation wait. A flood for an
obsolete pose/vehicle is cancelled by epoch, with checks inside worker search, without
an owner join, lock wait, I/O wait, scene search or thread-affine pointer transfer. New
route capture retains its 0.5-ms elapsed slice budget, copies live gates/vehicle/pose and
checks map/pose/vehicle before committing. Crow-flies and native target positions remain
live reads; route directions and beacon/autowalk retain their existing request/update
mechanics. The user accepts publication/order finishing after selected-target directions.

Baseline offline real-bake computation, before this change: 24 worlds, 16 far walking
routes; worst flood 21.006 ms, route 146.768 ms (FF5). Representative FF6 walking world
routes were 66.382 and 24.044 ms. All-six boarded route/filter maxima were 41.807/31.839 ms
for 103 eligible world/transport cases. These were cold independent worker samples with
real bakes, not game-frame/audible costs or universal targets; no A* semantics were changed.
The native-fixture scanner regression checks the complete selection-to-readiness path
against a 2,000-row capture/publication burst and an obsolete pending flood. The final
reported selected-target native-fixture dispatch is recorded by entity_scanner_report.txt;
zero destination-list readiness ticks are required for this warm case. Cold invalidation,
map change, busy/retired work, preserved browse ordering and worker-native-call exclusion
remain checked. Existing real worker lane, priority, callback budget, cancellation and
fake-SDK speech checks also run in build.bat. Numerical actual post-fix SDK/audible and
modded/unmodded game-frame timings require a subsequent user log/gameplay confirmation;
no speedup ratio or instantaneous runtime guarantee is claimed.


Final canonical native-fixture sample: 2,000 rows in three capture/publication ticks;
selected-target dispatch 0.200 us and zero list-readiness ticks; zero worker native calls.
Final real-bake worker sample: 103 boarded cases, route/filter maxima 32.446/16.093 ms;
582 world exit candidates, six-category preparation 3.642 ms. All checks and isolated DLL
smoke pass; installed receipt is linked from docs/FOLLOWUP_2026_10_14.md. These are
bounded offline/fake-native tests. Actual post-fix request-to-submit/audible latency and
comparable game-frame baselines are unmeasured until user gameplay/log feedback.


### Native page and group-target follow-up, October 14

The latest Esper/equipment/group-target correction is recorded in
[the follow-up](../FOLLOWUP_2026_10_14.md#blank-espers-equipment-pages-and-whole-group-target-entry).
Native Show/WaitInputInit/SelectContent/SetPage/SetParameterFlag producers own readiness;
no keyboard-specific polling or three-frame delay remains for FF6 equipment entry.
Esper observations are bounded to one virtual row (128-slot validation) or 32 spell rows,
with one labelled continuation and generation/owner cancellation. Empty selections retire
old review work immediately. Items preserve the existing elapsed 0.5 ms capture budget
and collection bounds; copying displayed data stays on its owner, review assembly remains
on the owned-text worker. Group-target labels use completed unique phase entries; FF6
range qualification is a bounded static native call on that owner. No native pointers
move to workers, no callback waits on work, and the confirmed single-enemy producer guard
is unchanged. Actual-source renderer/review checks report zero worker native calls.
These checks establish ownership and cancellation, not measured audible latency or new
runtime frame budgets. No gameplay probes/measurements were performed for this revision.


Equipment pager investigation traced native StartPrevPage/StarNextPage iterators, both
calling the already hooked SetPage. The user confirmed actual numbered pages update
correctly and the report was Q/footer-toggle confusion. The temporary completion hooks
and their specific checks were removed, restoring the confirmed existing native producers
and bounded owner capture/review service. No new timer, polling, hidden-number fallback,
thread route or runtime latency claim remains from that investigation. Source/build/
deployment recovery and mastery evidence are in the page-count/mastery follow-up record.


## Intermittent world refresh and transition input, October 14

Scope: user requested diagnosis and a fix plan for intermittent world-map movement
lag and delayed post-battle recovery, then explicitly requested an input guard during
map/battle transitions and loading. This section records that first guard phase.
The subsequent trap recovery/routing repair and bounded Luna warning-menu handoff
are recorded in [TRAPS.md](TRAPS.md). The broader world refresh correction below
remains planned. No game launch/control or runtime probe.

### Own-session evidence and limits

The newest local session was FF2, beginning 2026-10-14 12:10:59, installed October 14
06:25:04 build. Archived initial log and FF3's own hit-count log are in
`build_check/pathfinding_diagnosis_2026_10_14/`; no tester logs/saves were read.
The FF2 log continued growing during the investigation. Initial line references below
remain valid in that session. The later Deist Cavern portions predominantly show
~33–85 ms direction submissions, consistent with the user's world-only qualification;
this is not an all-map performance guarantee.

| FF2 world directions | Request to SDK submission | Route capture/compute/total |
| --- | --- | --- |
| Generation 75, log lines 648–651 | 1177.416 ms | 16.840 / 0.052 / 33.631 ms |
| Generation 79, lines 668–671 | 1178.691 ms | 16.914 / 0.055 / 33.338 ms |
| Generation 83, lines 688–691 | 1181.408 ms | 16.901 / 0.052 / 33.477 ms |

Adjacent generations submit in ~33–50 ms. Speech queue cost in the three delayed cases
is 0.136–0.177 ms and SDK call cost 0.370–0.442 ms. The ~1.14-second excess precedes
`request_route`; it is not the logged route computation or speech ingress. SDK submission
is not audible completion. Existing Directions `data-age` is populated with request time,
so its duplicated request latency does not independently establish snapshot freshness.

After the first battle, native state 3 returns at 334053.125 (line 565). The scanner logs
38 live rows at 334053.750 and repeatedly through 334062.546; a menu round trip and manual
rescan occur in between. Category generation 62 finally submits at 334063.140 after
607.604 ms for that particular request. The live-row `ready` lines measure registry
capture, not completion of asynchronous category/filter publication. Therefore they do
not prove usable navigation was ready during that interval. We cannot assign the entire
~10 seconds to one request or prove which intervening key belonged to the failed return:
pre-dispatch ignored keys, filter stages and readiness reasons are not correlated in
this log. Second battle return at 334099.046 is followed by a fast generation 94 at
334100.453 with 33.520-ms request-to-submit. User confirms the delay is intermittent
and both availability and lag improve when refresh finally completes.

### Production paths and reproduced defects

- `entity_nav.cpp::refresh_entities_if_needed` calls `scan_entities` for every navigation
  request. Each completed scan reaches `scanner::finish_scan -> apply_filter ->
  cancel_filter`, cancelling the preceding category preparation. Native row collection
  is tiny in this case, so repeated keys can restart preparation faster than it finishes.
- `entity_scanner.cpp::when_filter_ready(existing_selection=true)` skips the full list
  wait only if `g_views_ready` is true. `apply_filter` publication clears that flag when
  the live player pose differs from its captured origin and starts another filter.
  The selected object may still be alive and current, yet directions fall back to a
  full-list wait. This is the missing case in the October 14 FF6 bypass. Previous fixture
  coverage exercised movement with readiness still true, not stale-origin publication.
- `BattleStart` cancels the live scan, while `scanner::needs_scan` only checks map ID and
  whether a scan is running. Returning to the same map can therefore skip automatic
  revalidation despite cancelled/unready filter work. First input starts work instead;
  repeated inputs can then cause the cancellation cycle above. This is a source-backed
  recovery defect consistent with the user's report, not an exact traced attribution of
  every silent second in this session.
- `baked_nav.cpp::exit_distances` may call full `boarding_route` separately for each
  inaccessible world exit in the Vehicles filter. That work repeats target-area floods,
  vehicle approaches, riding floods, landing scans and path reconstruction. User settings
  are PathfindingFilter=true, mode=2 (with vehicles), MapExitFilter=false, Autowalk=false.
  Earlier all-six performance fixtures covered boarded vehicles or walking without
  parked candidates; they did not model this walking-plus-parked-ship list preparation.
- Cancellation supplied to `exit_distances` reaches its initial flood and outer target
  loop. `ExitSnapshot.route` has no attached epoch, while boarding's riding flood and
  landing scan use `planning_cancelled()`. Those inner stages therefore do not see that
  filter cancellation. Rejected results stay rejected by the owner epoch, but obsolete
  compute can still occupy the serial Navigation lane before an interactive request.

Actual scanner diagnosis fixture results (`scanner_diagnosis.txt`): stale world-origin
publication leaves views_ready=0, selected_alive=1, directions_delivered=0 and
filter_pending=1. Stable publication then delivers the directions. Twenty actual
`finish_scan` calls before worker delivery advance the filter generation twenty times,
queue twenty fixture jobs and deliver no directions; stopping those refresh requests
allows publication and delivery. Worker native-call count is zero. The fixture uses real
scanner capture/publication/wait logic; native owners, flood computation and worker
scheduling are modeled boundaries, not a measurement of Unity's collection or time.

Actual real-bake `exit_distances` fixture (`world_filter_diagnosis.txt`), FF2 origin
(216,40), canoe enabled, 150 baked candidate exits: 0.494 ms without a parked ship versus
101.299 ms with a ship on the nearest legal water cell (210,40). Both return three
reachable exits under the fixture flags. This is representative workload evidence, not
a replay of the user's exact ship position, nine live gates, story flags or 38 live rows;
it does not account for the full observed ~1.14-second delay. The missing inner epoch is
also demonstrated. No universal target or runtime speedup is inferred. Temporary fixture
sources are archived in the same evidence directory, copied into `tools/*_check.cpp` for
canonical compilation and removed afterward. Their binaries remain under `out/checks`.

### Implemented transition input guard

`system::input_transition_blocked` uses the existing cached native main-game manager and
FadeManager.IsFadeFinish. A native-owner nesting counter blocks input throughout the
original ChangeState call, including reentrant callbacks before the new state settles.
Matching all-six dumps give Init=0, ChangeMap=1, FieldReady=2, Event=12, Battle=13 and
GotoTitle=14. Construction/title states and Event/encounter preparation block mod input;
visible event dialogue remains manually reviewable. Any active fade blocks input even
while GetCurrentState still reports Player/Transport/Battle or a menu. Stable title,
field, vehicle, menu, naming, dialogue, battle and results controls remain available.

All keyboard binding dispatch and function-key entry, controller routing, mod mode,
modal input (including late/direct handlers), entity navigation and waypoint context
entry use the shared guard. Blocked entity actions return before cache refresh, scan,
setting action, speech or readiness work. Pending readiness callbacks retire during a
transition; map/battle entry advances navigation speech identity. Completed route speech
rechecks its context. Autowalk's former loading-time second-request exception is removed;
its native lifecycle/route-transition handling is preserved. Controller pending clicks
are retired and held LT/stick state is seeded, so held loading input cannot become a new
navigation request on return. Mod modal held keys are seeded similarly.

This suspends player-triggered mod actions; native observation, cancellation, automatic
rendered speech and legitimate post-readiness map capture still have their existing
producers. The patch does not promise that no hook executes during loading. No game
thread waits, new scene search, worker/native-pointer transfer, timer, input injection,
text deduplication, dependency upgrade, bake or save change is introduced.

Canonical build/all regressions pass: 6,724 native names, zero missing/undefined wiring.
`transition_input_check` exercises the actual complete keyboard registry/router and
shows zero action/speech/native-cache/UI-handler work while blocked, no held-LT replay,
and working fresh keyboard/LT input afterward. Actual native transition, scanner,
category/route speech, modal main/late/direct-handler and high-hit regressions pass.
Isolated DLL smoke passes all 47 forwarding exports and normal host exit. The first
sandboxed build stopped progressing at file-glob validation and was cancelled; the
canonical build completed outside the sandbox. New input-fixture duplicate inline stubs
were corrected before the final successful build. This does not change test conclusions.

Local `out/hid.dll`: SHA256
`B32DBD80307EBD0E118918A06AB74D8F4744C4294556511C80A753546D5619D9`, 9,073,152 bytes.
The user's FF2 session kept writing its log during investigation, then closed. A final
read-only process check confirmed it was closed. After preserving prior DLLs, deploy.bat
succeeded in all six and every installed hash matched the local build. All six SDL hashes
are unchanged; Prism prerequisites were checked read-only. Recovery copies are in
build_check/transition_guard_recovery_2026_10_14; before/deploy receipts are
build_check/transition_guard_before_2026_10_14.json and
build_check/transition_guard_deploy_2026_10_14.json. No commit/push/release publication,
save/bake change or dependency upgrade occurred; public V0.9.2 remains unchanged. Runtime
transition costs, modded/unmodded frame/load comparison and audible behavior remain
unmeasured; guard gameplay confirmation is pending.

### Prioritized remaining world-map fix plan

1. Give native battle/map readiness an explicit resumption/dirty-generation contract.
   Cancel obsolete filter work at transition entry; on same-map field readiness start one
   bounded revalidation even when the map ID is unchanged. Separate registry completion
   from usable category publication. Input during transitions is discarded by the new guard.
2. Separate valid selected-target context from list sorting/reachability readiness.
   Directions should capture current pose/vehicle/gates from a still-valid selected target
   while an ordinary origin refresh is pending. Preserve map/settings/story/entity/category
   invalidation and grouped representative policy; never route from a dead or retired target.
   Do not simply delete the readiness check without replacing those validity guarantees.
3. Coalesce ordinary live refreshes: keys must not unconditionally cancel/restart equivalent
   in-progress work. Track authoritative entity/gate/transport changes and latest pose,
   preserve a compatible published selection, and schedule a bounded follow-up generation
   when needed. Maintain documented browse-event fidelity and latest direction cancellation;
   no text-content deduplication or arbitrary speech delays.
4. Propagate the filter's actual epoch into every inner boarding/flood/landing operation.
   Replace per-exit full parked-vehicle route construction with reusable walking, vehicle
   approach/riding and legal landing distance maps for the captured state. Keep exact native
   transport/landing, wrapping, directional gates, hazards and shortest usable exit semantics.
   Reconstruct movement phases only for the explicitly requested destination. Share/cache
   only under keys covering map, live blockers/story state, player cell and vehicle ownership,
   identity/location/rules; bound memory, pending work and stale retention. Keep interactive
   routing ahead of obsolete bulk list work without owner waits or pointer-affinity changes.
5. Add cheap correlated request/blocked-reason, registry/filter capture, queue, compute,
   origin rejection, cancellation and first-usable-publication timestamps. Format/write
   off-thread. Extend production fixtures to stale-origin publication, rapid keys with a
   slow current filter, battle return during work, walking plus parked vehicles, and changed
   flags/targets/vehicles. Compare whole request and cancel/tail costs against this baseline.

After implementation, run canonical build/smoke and all-six real-bake regression coverage,
then focused user gameplay confirmation in TESTING.md. Existing guards are offline-verified;
the remaining world-map algorithm/readiness fix and its gameplay/performance acceptance
are open. No speculative runtime capture or user code/architecture review is requested.
