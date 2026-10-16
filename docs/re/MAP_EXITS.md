# Floor and world exits, October 13, 2026

The user relayed missing South Figaro floor exits, especially houses, and requested
an FF5/FF6 bake audit plus reachable world transition tiles and nearest-exit grouping
in all six games. Sol handled discovery, implementation and verification alone.
No tester logs/saves were accessed and no game was launched or probed.

## Reproduced source faults

- Every raw GotoMap/ToLayer was present in the old baked object tables. Geometry
  nevertheless disconnected their approaches or failed the native dimension check.
- FF6 has 406 MapRange records differing from their shared tile sheets. The old
  bake ignored these rectangles. Grids and objects now use native room bounds and
  subtract the room origin. South Figaro Map_20071_11, for example, is 27 by 20 at
  asset origin (100,41), not 128 by 64. Six existing FF6 puzzle step coordinates
  receive the same shift; their original asset coordinates remain in `_about`.
- Raw collision IDs are not boolean walls. Native CollisionFilter preserves half
  tiles' cardinal direction masks. DirectCollision/DiagonalBinaryInfo joins open
  cells alongside triangular stairs. Schema 2 stores these recovered masks;
  the reader remains compatible with schema 1. Search, flood, directions and
  autowalk share the edges. Open rooms gain no unrestricted diagonals. Native
  movement still checks every executed edge.
- ToLayer acts from its own layer. Inventing a reverse switch at the same tile
  could send a route straight back downstairs. The reverse direction now uses
  its separately placed native trigger.
- An unresolved package alias overwrote complete geometry with null dimensions
  and empty grids: Map_20051 mentions FF5 Karnak's Map_20050; Map_20050 mentions
  FF4 Kaipo's Map_20030. Their owning bundles restore the grids. The extractor
  preserves earlier complete geometry; `tools/repair_bake_sources.py` performs
  bounded recovery. Original JSON is archived under
  `../tools/mapdump/exit_audit_2026_10_13/`.
- Blob offset zero also meant absent data. Reserving a byte preserves the first
  grid, restoring FF3's intro-world attribute grid. All six bakes were regenerated.

## World exits and grouping

World passability requires both the transportation grid and the native terrain
permission. Native CheckOkList rejects out-of-range attributes. FF5's high raw
values remain rejected; no guessed terrain normalization is applied. Transition
goals need an actual GotoMap or step-on warp footprint. Empty/unenterable footprints
stay empty, removing the former fallback to a decorative or blocked tile.
Ordinary entrances are entered after legal landing/walking rather than flying over
the town being treated as entry.

The worker measures each exit row's exact active GotoMap footprint against copied
current movement and obstacles. Sibling arcs cannot rescue an invalid row.
Unreachable world rows are pruned even with grouping off. Grouping keeps the
shortest reachable route per destination, independently of list sorting, then
prunes the other rows. An inaccessible closest tile cannot hide a reachable one.
Indoor groups retain one representative if all routes are blocked, preserving
the existing explanation. Remote exits use the cross-floor planner.

Legal shore floods retain ride costs rather than making distant shores free.
World wrapping remains. Browse, directions/autowalk and beacon requests obtain
the current representative after movement/vehicle changes, and stale publication
is rejected. Existing category event guards remain; no text suppression was added.

## Native evidence and ownership

`tools/re/exit_native_check.py` records installed binary/metadata and source/bake
SHA256 values in `build_check/exit_native_provenance.json`. All six installed
metadata files match these initializer SHA1 identities:
CollisionFilter `8C4FA5D2E9C81CD19F19DA7FE51DB94B4413C3C8`, DirectCollision
`313F7A37A0D731DF68F640F81946E99CBBA07B2B`.

| Function | Matching RVA | Evidence/consumer |
| --- | --- | --- |
| FF5 MapConstants..cctor | 0x2E8740 | Native table initialization plus metadata; baker |
| FF5 MapUtility.ParseCollisionMappingData | 0x2F6570 | Cardinal masks and conditional stair diagonal patches |
| FF5 TransportationInfo.CheckOkList | 0xA97BE0 | Bounds/permission at attribute minus one; world eligibility |
| FF6 MapUtility.ParseMapRange | 0xCDE000 | Object type 27; pixel rectangle divided by 16 |
| FF6 MapUtility.ParseMapObjectGroupData | 0xCDDB80 | Subtract rectangle origin; native local position uses rectangle dimensions |
| FF6 MapUtility.ParseCollisionMappingData | 0xCDC480 | Cropped grids and cropped diagonal indices; baker |

Private ignored exports: `docs/re/ff5_exit_current.c`, `ff5_exit_attribute.c`,
`ff5_exit_movement.c`, `ff6_exit_current.c`. The older FF5_Analysis project did not
cover two matching targets correctly. Bounded no-analysis current-build projects
under `build_check/` preserve the old archive. Current scripts, function signatures,
addresses, logs and bundle hashes remain in these local evidence records.
No game function was executed for discovery.

Capture reuses EntityCursor, NativeObstacle and the central CaptureBudget. Native
pointers stay in the owner closure. ExitSnapshot contains copied state, cleared
vehicle pointers and immutable bake references. Navigation owns masks, floods,
route distances, remote planning, grouping and category construction. The native
owner never waits for work; request epochs cancel obsolete computation. Ordinary
floods keep FIFO BFS; only nonzero shore seed costs need a priority queue.

## Offline coverage and explicit limits

`tools/map_exit_check.cpp` uses the production reader/router. All 178 South Figaro
exits across both town variants have arrival-connected approaches; cropped room
bounds and recovered Karnak exits pass. World trigger geometry receives separate
terrain/approach checks. The actual worker/grouping fixture checks all 582 world
GotoMap candidates across 24 world records, prunes unusable rows from representative
starts, routes retained representatives and tests copied native gates. The existing
103 boarded-vehicle cases remain. Scanner/speech checks cover movement, current
publication, deferred planning and stale categories. Autowalk replays real routes
from all six regenerated bakes, including native stair edges.

This does not certify every script/puzzle state. Remaining FF5 static arrival
flags: Istory's upper world exit (Map_20090) and collision-less scripted Map_40011.
Remaining FF6 flags: three Lethe River forward exits (Map_30380), four Magitek
Factory exits (Map_30521_1/11), and Kefka's Tower Map_30871_4's north exit.
Lethe River requires native raft travel. Other disconnected cases remain exact
script/state investigations or gameplay checks; do not open walls or claim them
fixed from an arrival flood. They are distinct from the reproduced South Figaro
bake faults. Remaining zero-dimension FF5/FF6 master entries have no extracted
objects or incoming GotoMap/ChangeMap reference, rather than a demonstrated
navigable map. Preserve these distinctions.

Real native capture/frame impact, usable/audible response and special-state
behavior remain unmeasured; asynchronous placement alone is not responsiveness
evidence. Final offline results and deployment are recorded below.

## Prioritized gameplay checks

1. South Figaro: enter a house with stairs, browse Map exits, request directions to
   the next floor, then return. Expect listed exits and a stair route, without map
   data unavailable, no path on an ordinary stair, or a wall route. Repeat with
   autowalk if enabled; manual input must stop it.
2. A reachable world town: enable Group exits and browse Map exits. Expect one
   usable entry per destination. Follow directions onto it. After returning, move
   closer to another entrance to the same destination and request directions again.
   Expect the new nearest usable trigger. Grouping off restores individual usable
   triggers while decorative/unreachable tiles remain absent.
3. When available, repeat world entry aboard the FF2 ship: expect legal
   disembarkation and walking, retaining the previously confirmed ship behavior.
4. Progression-dependent Istory/Magitek Factory/Kefka's Tower cases: record the
   actual map/floor and whether the gate, lift or puzzle has been operated if the
   ordinary route remains unavailable. Own-session logs are useful only when the
   user requests inspection. No broad capture or early game launch is needed.

## Final validation and deployment

Canonical `build.bat` passed with all included offline regressions: 7,077 native
name checks, zero missing names and zero undefined wiring. The production
reader/router, actual filter worker, scanner/speech readiness, map state, vehicle
routes, input and autowalk checks passed. `smoketest.bat` passed in its isolated
host: all 47 exports resolved/forwarded, normal boot thread, no crash/fastfail.
Neither script launches a game. `git diff --check` passed.

Fresh `tools/bake_maps.py all --out build_check/exit_roundtrip --check` output
matched all six committed-data locations byte for byte, including collision
roundtrip checks. The native initializer check passed for all six installed
metadata files. Production autowalk replay passed 2,293 sampled routes (FF1 118,
FF2 338, FF3 345, FF4 307, FF5 591, FF6 594), including stairs and wrapping.
The world worker fixture evaluated 582 candidates from representative starts:
46 reachable rows and 536 pruned rows. These are start/state-dependent results,
not permanent deletion of 536 triggers. It routed the retained rows and checked
native gate vetoes; all 103 boarded-vehicle cases also passed.

### Comparable offline performance

`tools/re/exit_perf_baseline.py` staged the previous reader/router and bakes from
HEAD `1c8dfb689818b59eea491601ec39692c1ba36a70` into ignored
`build_check/exit_baseline/`, then ran the same current performance harness. The
current implementation ran after that baseline, with no concurrent build. Both
checked 24 world records and 16 far routes with zero failures. Fifteen start/goal
pairs were identical; FF5 Map_10010's far goal changed from (70,157) to (63,158)
because native terrain validation excludes the old goal.

| Offline workload | Previous worst | Current worst |
| --- | ---: | ---: |
| World flood | 6.521 ms | 10.904 ms |
| Far world route | 51.927 ms | 64.235 ms |

The stricter native terrain checks add worker computation. These single-run
measurements are not distributions or game-frame/audio latency claims; the route
maximum includes the changed FF5 goal. Per-case inputs/timings and comparison are
in `build_check/exit_baseline/world_baseline.txt`, `exit_world_current.txt` and
`exit_perf_comparison.json`. The actual route-worker regression's worst vehicle
route/filter were 37.910/18.728 ms; world exit/category preparation's worst was
3.658 ms. These are separate offline workloads, not a combined pipeline duration.

Native observation/publication uses the existing 0.5 ms elapsed CaptureBudget per
continuation, checked between operations; this is not a hard cap on one native
call or on aggregate frame work. The fake-native scanner fixture published 2,000
rows in four ticks and passed movement-refresh/stale-result checks. Warm category
selection measured about 0.023 microseconds there. Those fixtures cannot measure
real Unity read cost, input-to-usable/audible response, data age, cancellation tail
or aggregate native cost under gameplay load. Pose/vehicle changes reject stale
publication and recapture; continuous movement can delay publication until a
current snapshot is ready. Worker cancellation checks run between flood/route
expansions and exit rows, using the owner epoch. Special script states, worst-case
overload and real cancellation delays remain untested.

### Installed build and recovery

Before deployment, the six installed DLLs were copied and hash-verified under
`build_check/exit_recovery/ff1-hid.dll` through `ff6-hid.dll`; `manifest.json`
records their previous `E3F6C174CAE0FE65D969E3258B72936035EBA579BE051084B1BBB0845D0602BE`
identity. Canonical `deploy.bat` passed its closed-game/Prism checks and installed
the new DLL in all six games. All installed hashes match the 9,122,304-byte build:

`24C1EF06A7E7153CADB8249D0C8FEE06324FB6E46C751320C0CC97E037E692B5`

`build_check/exit_deploy.json` retains the six-game receipt. Prism was unchanged;
no game launch, runtime probe or dependency install occurred. Gameplay confirmation
of these new changes remains pending. Sol performed the entire task independently.
The public V0.9 archive/tag/release was not updated during this fix. The user
subsequently requested commit/push and release 0.9.1; its packaging/publication
record is maintained in [release.md](../release.md).


## All-game asset audit, October 14

The user requested every bake be checked, especially complex FF4–FF6 towns,
multi-floor dungeons and FF6 World of Balance/Ruin variants. Sol independently
re-extracted all six installed games with `../tools/dump_map_objects.py all` into
ignored `build_check/map_audit_2026_10_14/fresh/`. No game process/function ran.
All extraction runs reported zero errors. Retained and fresh submaps, grids,
objects and scripts match semantically; master data was copied from the existing
matching extraction into this private audit directory, not changed in game folders.

`tools/re/map_bake_audit.py` regenerated each bake, decoded its binary grids and
validated every native MapRange, collision/stair layer, hidden passage, terrain,
transportation grid and map-id mapping. Native collision initializers still match
all six installed metadata files (`exit_native_check.py`). Counts:

| Game | Submaps | Collision cells checked |
| --- | ---: | ---: |
| I | 129 | 413,369 |
| II | 349 | 263,355 |
| III | 411 | 287,449 |
| IV | 368 | 234,726 |
| V | 655 | 574,822 |
| VI | 638 | 657,695 |
| Total | 2,550 | 2,431,416 |

FF1–FF5 regenerate byte-identically. FF6 has one confirmed additional conversion
fault: Map_30641_2, native map id 449, has MapRange pixel y=-2. Native signed integer
division truncates to tile origin zero; the baker used floor(-2/16)=-1. Python
negative slicing discarded the rows, then padding turned both collision layers
into all-blocked masks. The baker now uses the signed native quotient for x/y/w/h.
The previous private bake decodes to 1,116 zero masks per layer (raw padding value 1). The corrected room is 36 by 31, with both Ground/Upper collision layers retained.
`map_exit_check.cpp` checks that these layers contain native blocked cells instead
of uniformly blocked padding. No object coordinates or unrelated puzzle routes change.
Only `data/ff6/maps.bin` changed: SHA256
`DD75D6EA83FFCB921ACCB3C1E6E0B3BEE71A6DFD6EC5041E525329693E612729`.
The previous binary is preserved in this audit directory.

The private report records all rooms, dimensions, map ids, layers and object
counts, including mapped scenes without native grids. Each variant retains its
own submap/mapping; the audit does not merge rooms by displayed town title.
Both South Figaro sets still pass all 178 floor exit approaches. The existing
production router/follower checks replay 2,294 real routes from all six bakes.
Fresh-source parity and every static grid pass; this is not confirmation of every
native script, puzzle or vehicle state.

The static exit audit also records two FF1 intro-map variants with no native
collision/terrain grid, two FF4 Baron event exits outside the 34 by 34 map (group
ev_e_0033, x=-2), and FF4 Map_30141_1's disconnected Bottom exit. FF5 Map_40011
also has no native collision grid. The Istory, Lethe River, Magitek Factory and
Kefka's Tower cases described above remain exact script/state or gameplay gaps;
no walls were opened to force them to pass. Original objects/scripts for all
flagged cases are preserved in `flagged_native_records.json`. Absent native
geometry is recorded explicitly rather than invented. No further conversion or
bundle-overwrite fault was found in the fresh extraction.


## Exact selected entrance correction, October16

The shared goal builder previously merged sibling entrances to the same floor/arrival.
Explicitly selected transitions now retain their own x/y, including multi-cell trigger
footprints. Eligibility distance, browse and explicit planning use that same point;
optional exit grouping selects its representative before planning. No unselected sibling
can rescue an invalid or unreachable selected tile. Native terrain/layer, story state,
vehicles, interaction facing and cross-floor transition rules remain in effect.
[All-six evidence and Cornelia replay](BASELINE_FIXES_2026_10_16.md#exact-transition-goals).
This supersedes older nearby-arc goal-union descriptions; bakes are unchanged.
