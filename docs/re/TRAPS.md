# Trap routing and transition recovery, October 14

The user reported a permanent pathfinder failure after Deist Cavern's pitfall,
requested trap-aware safe routes across floors in every game, and clarified the
unacceptable loop: fall, route back upstairs to the same trap, fall again. Some
destinations legitimately require a drop, so safe routes take priority and a drop
is allowed when no safe route exists. Optional warnings belong to Speech settings.
This is an existing-mod repair; no new mod, game launch/control or runtime probe.

## Evidence

The user's own FF2 log session began October 14 at 12:38:03 on the old 06:25 build.
Deist B3 map185's event9 at (14,27), named 落とし穴1, runs sc_e_0112. The log shows
Event12 -> ChangeMap1 -> Event12 -> Player3 and “Entering Deist Cavern B4”, then no
completed entity scan. A menu round trip did not restore it. The session is archived
locally under ignored build_check/pathfinding_diagnosis_2026_10_14.

The actual FF2 FadeManager.IsFadeFinish (RVA3E3A00) requires renderer alpha0 AND a
different Image color alpha. The continuation sc_e_0112_1 calls transparent
FadeIn[0,0,0,0], leaving both alphas0; IsFadeFinish therefore stays false. The mod
waited forever, although native control had returned. IsStateFadeIn (RVA3E3A90)
instead tests the actual CanvasRenderer alpha against0. The shared helper now uses
that native readiness predicate. Bounded matching-binary checks verified all six:
FF1 900810, FF2 3E3A90, FF3 9FE100, FF4 4EB9B0, FF5 A728E0, FF6 CB7DC0.
Native state/depth guards still block construction and scripts before control returns.

Deist's ChangeMap[186,1] initially names (36,28) Bottom, but the player-bound
continuation sets (9,28), then Y-axis MoveTo32. Final control is at (9,32) Bottom,
matching PointIn5. Routing to the initial point can invent a path through the wrong
region and cause the reported kind of loop. This bake now uses PointIn5.

The same survey identified thirteen FF5 arrivals needing correction, including
three Castle trap links whose scripts were missed by strict submap lookup, and
three FF6 cross-map landing differences. Thirty-five FF6 sc_e_0445 fall triggers
relocate the player to (7,8) on the same map, preserving entry layer. They now have
explicit directed relocation arcs. Native FF5 FieldController.GetScriptData
(27ADD0) tries MapModel.GetScript, then MapManager's resident name bank (2A6830);
the baker's trap fallback accepts only a unique script in the same parent package.
It does not borrow scripts from another game/package or guess ambiguous names.

Native Puppet/AxisAccessor bodies and direction/layer initializer prefixes were
checked against all six installed binaries. SetPos/Lerp coordinates are grid
coordinates; layer1/2/3 means Bottom/Ground/Upper and0 preserves. Move uses native
grid direction times count: Up Y=-1, Right X=1, Down Y=1, Left X=-1. MoveTo's axis0
sets X, axis1 sets Y; direct mode0 sets the given coordinate. Lerp's final layer
matters: several FF5 falls temporarily draw the player on Upper, then return to
Ground. Player-bound programs stop at Exit/Return; other actors' positions are
excluded. FF6 Map_30561_1's native MapRange origin (27,33) is applied when baking
its three corrected landing cells.

Reproducible checks: tools/re/trap_fade_check.py and trap_arrival_audit.py.
Raw native bytes/disassembly, hashes and original script audit remain ignored in
build_check/re; Ghidra read-only decompilation output remains ignored in docs/re.
Reviewed per-game warp_arrival_overrides.json records canonical script SHA256,
source identity and final coordinates. The baker rejects changed/unmatched scripts.
Synthetic PointIns are owned graph data, not changes to the game's files or grid.
Same-map arcs use a=-1 to retain the actual current map alias.

## Routing, recovery and speech

Trap masks are prepared per native layer. Current-map masks use active captured
triggers; remote masks use the copied story-state resolver. Named environmental
pits/drops are classified conservatively; actor/story falls are not inferred from
an arbitrary occurrence of “fall”. ID0 events require a containing baked footprint;
the existing unknown-ID collider guard is preserved. RandomEvent triggers also
receive the correct active mask. Native pointers never enter the route snapshot.

Ordinary walking cannot cross a trap, including the incidental-event fallback.
Cross-floor search first excludes trap arcs. If that complete search finds no route,
the second search minimizes trap count before distance. A required trap ends a
movement leg at its trigger, then planning resumes from the actual landing. A
search that reaches its existing400-state work bound does not claim a trap is
required. Existing native terrain, story gates, vehicle and landing rules remain.

The planned chain rejects reusing a trap. Completed-drop receipts remain bounded
to400 and scoped to the destination's map/cell/layer; a failed repeated-drop request
does not erase them. Native Event/ChangeMap observation marks a planned trigger
near its entry; ready route capture records the arrival. Same-map receipts additionally
require the expected landing cell, so a same map ID alone cannot certify a fall.
A different goal retires the prior history. Guide/follower requests keep a separate
latest receipt and preserve the manually chosen destination. Title return retires
both contexts. All planning and formatting use the existing owned navigation worker;
native observation is bounded, and the game thread never waits for it.

Event12 return schedules a fresh scan even without ChangeMap. A planned same-map
fall also emits the existing transition lifecycle so navigation/autowalk can resume
after native control and fade readiness. Continuation and puzzle guidance wait on
native readiness with event generations, replacing the old one-shot0.8-second timers.
The Deist guide now routes through safe stairs to B5 PointIn1 (9,8) Ground and checks
completion on native map arrival, including a spawn directly on that point.

Selected-event endpoint warnings are omitted; other incidental events still warn.
Required drops say “Warning: a trap is required to reach this chest.” or the general
route equivalent. Warning text is separate from direction text; entity, waypoint,
continuation, guide and autowalk producers consult the persistent default-on
Speech > Mod warnings preference. Off preserves route planning and directions.
No text-content deduplication was added.

## Validation and limits

Canonical build.bat passes: 6,724 names/zero missing, zero undefined wiring, all
appropriate regressions. Isolated smoketest.bat passes all47 system HID exports.
Final DLL C8FD888C133131CDC0D061D7CEDADCFD08F3B7015B29A18867F6C88BE14667CB,
9,101,824 bytes, is deployed/hash-verified in all six with games closed. Separate
deploy.bat passed; SDL hashes are unchanged and Prism was checked read-only.
Recovery: build_check/trap_recovery_2026_10_14. Before/deploy receipts:
build_check/trap_before_2026_10_14.json and trap_deploy_2026_10_14.json.
No save, dependency install, game launch/control/probe, commit/push or publication.
Public V0.9.2 is unchanged; new gameplay confirmation remains pending.

Actual-source fixtures cover transparent completed/active fades, native script and
same-map return lifecycle, boot/input guards, held input, selected-event warning
suppression, warning-on/off speech order, separate guide receipts and Deist completion.
Real bakes verify all52 reviewed arrival/relocation arcs. Eight Deist chest routes
from B3 use safe stairs, including paths that first go back through B2. A controlled
closed-stair snapshot requires the actual B3 drop; subsequent requests reject a
repeated drop without clearing history. A real FF6 same-map fall similarly advances
to its landing and rejects the return-to-trigger loop. Worker native API violations0.
All-six vehicle/exit, map-state, autowalk, battle renderer and existing UI checks pass.

The survey covers extracted named environmental traps: FF1 20, FF2 2, FF5 28,
FF6 44 candidates; FF3/4 have no matching named pit candidates. Two FF5 inactive/
scriptless variant records have no new transition inferred. This is not proof that
every unnamed or conditional dungeon mechanic has been discovered. Existing all-map
bake coverage is retained; complete live all-area, current-story-state and audible
response confirmation remains user-side. The original intermittent world-map filter
starvation/parked-vehicle issue remains diagnosed with its fix plan in THREADING.md;
it is not certified solved by this trap repair.

Luna delegation: gpt-6-luna, medium, bounded warning preference/Speech row, twelve
locale translations and modal assertions. Sol reviewed its diff and integrated
modal, warning producer, full build and smoke checks; accepted, no remaining issue
within that handoff. Sol retained RE, routing, lifecycle, review and verification.
