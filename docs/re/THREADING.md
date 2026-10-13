# Read-only snapshots and worker ownership

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

## Native lifecycle evidence and bounded field capture

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
The first-page speech, actual SDL/Prism/controller output and modded-versus-unmodded
same-save load/battle/map timings remain user-side. No game or runtime probe was run.
