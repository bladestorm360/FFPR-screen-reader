# Event tracking across floors, October 15

The subsequent [persistent-state repair](MAP_STATE.md) also corrects the native
scenario/treasure flag snapshot used by these projections. The all-bake profiles
below exercise supplied flags; the additional actual-reader regression covers
the native array-allocation boundary that those tests missed.

The user reported losing the selected Conversation event between Deist Cavern floors,
with guidance appearing to loop, and explicitly requested checking every game's bakes.
This is a shared native navigation repair, not an FFII event-name special case.

## Evidence and diagnosis

The requested latest user FFII log is archived locally at
`build_check/conversation_loop_2026_10_15/ff2_latest.log`. It records Conversation at
B2 map184 cell (35,18), with a detour from (8,32) through B1 map183. The actual bake
identifies native Entity32 in group `ev_e_0049`. The existing safe label is Conversation;
no dialogue, participants or future outcomes are exposed.

At QPC431579.984 the user reaches B1 at PointIn3 (8,29). The retained route asks for
57 steps to B1 exit (34,14), leading to B2 PointIn1 (33,16). Production planner replay
against the actual bakes reaches the original interaction in three further steps.
Thus this particular B2–B1–B2 detour reaches a different part of B2; it is not a
closed cycle in the extracted graph. The saved log ends before that return to B2,
so this replay is not live confirmation that the entire traversal now works.

The loss is real: `connected_objects_snapshot`, `RemoteBuilder` and `build_remote`
previously admitted NPCs/chests/save points/outward exits only. Events disappeared
from Current map on leaving their floor in all six games. Without a retained event
row, the scanner could fall back to another list position. Selection restoration
also used map-local coordinates without preserving a live target's absolute map.

The all-bake investigation found distinct native events sharing coordinates (including
many unmapped FFIII assets) and valid zero-valued event IDs. Initial regression output
is retained as `event_tracking_initial_report.txt`: the first identity implementation
failed85 projected zero-ID cases. The corrected metadata uses -1 for unavailable
identity, preserves zero and anchors static targets by floor/identity/cell.

## October 16 correction

The October 15 inclusion of type20 TelepoPoint records below was incorrect. All six
native factories return null for this type; these are Teleport return-cache metadata,
not warp objectives. They are now excluded, along with wholly out-of-map records.
Actual scripted travel events remain. See [current audit](EVENT_CONTEXT.md).

## Shared correction (October 15 historical implementation)

- Project actionable Entity, Event, RandomEvent and SwitchEvent targets, native teleport
  tiles and scripted/actionable vehicle pads through the existing connected-floor and
  copied initializer-state services. Keep empty scenery, AnimEntity and ToLayer excluded.
  Native warp arcs do not add a second entity when their original trigger already exists;
  this is native identity matching, never speech/text-content deduplication.
- Reuse the same safe event-label resolver as live objects, with a data-only overload
  for copied raw identifiers on workers. Existing game placeholder guards still apply;
  native teleport tiles retain their earlier detector priority. Vehicle pad grouping
  remains per floor and uses the original name, stripping the bake's transport-ID prefix.
- Preserve absolute map and native identity from live to projected rows and back. Current
  native handles are rebound only on the owner. Distinct events at the same cell are not
  substituted, including ID0. Explicit route, beacon and autowalk goals carry the identity;
  continued route names retain the destination floor even when initially selected live.
- A missing target leaves no selected entity instead of routing to row0. Explicit next/
  previous/category browsing still works. Native story-state removal retires an identified
  target and says `Destination is no longer available`; it cannot retain a stale route.
  A ready current native observation remains authoritative when remote state is unknown.

Other-floor rows still require Current map. Current floor and Current map with vehicles
continue to exclude other-floor entries; settings are not changed automatically. The
user's log ends after selecting Current map with vehicles, so the focused retest must
explicitly select Current map. Existing no-spoiler chest state/floor labels are preserved.

## Ownership, bounds and verification

Native owner captures IDs/state and resolves floor names in the existing bounded scanner
slices; only owned rows/flags/names enter workers. Projection and remote naming now check
cancellation within their loops and return no partial list after retirement. Connected
floor traversal retains the400-submap bound. No input/game-thread waits, worker-native
calls, new diagnostic keys or game launch/control/probes were introduced.

`tools/event_tracking_check.cpp` decodes every bake's objects and every mapped non-world
master-map ID, including aliases. It runs actual connected-floor projections with copied
all-zero, all-one and unknown flags against each game's actual initializer programs.
For every active eligible event on every connected floor it checks projection, floor,
identity selection and pending-goal matching, including an unrelated collocated object.
It also checks cancellation, the three-leg logged Deist route, safe labels/categories
through the real scanner conversion, and native-state retirement. These three flag
profiles are broad regression coverage, not an exhaustive proof of every quest state.

`entity_scanner_check.cpp` exercises actual capture/worker/publication from a live event
to another-floor row, then back to its native instance, for IDs0 and32. Removing it must
leave selection empty; previous/next must recover only on explicit input. Beacon and
direction producer checks verify identity and floor carryover. Existing route/trap/
autowalk/soundscape/chest-state checks remain part of the canonical build.

Final counts, compute observations, canonical build/smoke and deployment are recorded
below after verification. Offline projection cost is not native capture/queue age,
first audible response or game-frame cost. All-map/quest-state behavior and the full
reported gameplay traversal remain user-side confirmation; focused steps are in
`docs/TESTING.md`. Sol owns this investigation, implementation and validation; no
delegation, saves or tester data beyond the explicitly requested log were used.

## Final verification and deployment

The actual-bake audit covers42,643 baked objects,2,454 mapped non-world master IDs
and7,362 flag-profile projections. Ninety baked submaps have no mapped master ID;
their objects were decoded/count-audited, not claimed as runnable floor entries.
Every active eligible connected-floor event in those projections is checked, across
3,859 distinct map/type/layer/cell/native-ID/group identities. Final audit failures0.

| Game | Master roots | Projected event identities | Largest remote list | Worst projection compute |
| --- | ---: | ---: | ---: | ---: |
| FF1 | 129 | 317 | 115 | 0.237 ms |
| FF2 | 346 | 500 | 105 | 0.355 ms |
| FF3 | 347 | 470 | 154 | 0.304 ms |
| FF4 | 350 | 477 | 160 | 0.433 ms |
| FF5 | 635 | 972 | 196 | 0.794 ms |
| FF6 | 647 | 1,123 | 480 | 1.377 ms |

These are offline compute observations for the stated profiles, with zero native
API reads on workers. Native capture/format/scheduling and audible/game-frame timing
are not represented by that column. Current scanner fixture passes exact live/remote/
live restoration for IDs0 and32, missing-target no-substitution, explicit browsing
recovery, cancellation and concurrent soundscape checks. Its existing8,192-row busy
soundscape case observed0.246 ms peak/3.105 ms aggregate copy across16 slices, plus
frame scheduling. Largest projected list480 remains below that existing registry bound.

The independent extracted-object audit retains hashes and counts for all six raw
object dumps in `source_event_guard_audit.json`: no message-only Event/Entity candidates
or zero-script/action SwitchEvent rows were found, so no missing message-presence bit
or bake regeneration is needed for this repair. Native TelepoPoint rows have no
script/action by design and now follow their earlier live detector priority. The
scanner fixture's old two-category fake was corrected to model Events; its six failed
new assertions are retained as `scanner_fixture_category_report.txt`. Label lookup is
reviewed at the shared data-only resolver; scanner consumer tests use a fake label
boundary. The new status label covers12 locales and all previous catalog values remain
unchanged. No text-content deduplication was added.

Canonical build and isolated smoke pass:6,736 names/zero missing, zero undefined
references,47 exports. Existing full route/trap/autowalk/chest/beacon/soundscape
regressions pass. Production Deist replay succeeds in1,57,3 steps for the three legs;
the third leg clears continuation instead of returning to B1.

Deployed/hash-verified to all six closed games. Final DLL SHA256
333F1BFFB9F85FADC8D9747D8CAF503BABD888A5B18A62B55BDEAB95A5C74002,
9,529,344 bytes. SDL3/pinned HRTF payloads match; Prism/preferences/waypoints retain
before hashes and timestamps. Recovery is `build_check/conversation_recovery_2026_10_15`;
before/deploy receipts are `conversation_before_2026_10_15.json` and
`conversation_deploy_2026_10_15.json`; detailed build/replay/raw-guard evidence is in
`build_check/conversation_loop_2026_10_15`. No game was launched/controlled/probed and
no dependency installation, new commit/push or publication was performed. All work
was Sol alone. Full gameplay confirmation remains pending using TESTING.md.
