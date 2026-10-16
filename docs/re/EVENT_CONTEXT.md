# Event context and navigation follow-up, October 16

Active scope: optional default-off per-battle result-notification suppression;
all-six event labels using actual developer identifiers, text and sprites where
needed; correct warp/trap semantics for labels/audio/routing; retire consumed
one-use events in live and remote lists; cancel all navigation at battle/map entry;
accurate cross-floor objectives with loop rejection; FFII field spell-target focus.
All work is maintenance of the shared native core. No game launch/probe is authorized.

## Confirmed findings so far

- The label baker treated any Msg/Message/Dialog command as Conversation. This is
  incorrect for descriptive inspections, switches, rewards, objects and travel.
  The Deist B2 native Entity32 in group ev_e_0049 is developer-tagged ペンダント;
  script sc_e_0049 begins with E0049_00_184_a_01, whose English first sentence
  identifies dragoon remains. The requested label is Dragoon, without the reward.
  Its script hides Entity32 and sets TreasureFlag1[235] and ScenarioFlag1[43].
  The B2 initializer selects ev_e_0050 after scenario43, so completion can be
  checked against actual state, not a mod-side guessed interaction counter.
- The latest requested FFII log is preserved under
  `build_check/event_audit_2026_10_16/ff2_latest.log`. Its apparent Deist Warp Tile
  targets are TelepoPoint type20 records: B1 entity40 at baked(35,14), B2 entity40
  at(9,32). Neither exists in the live list when the player reaches that floor.
  All six matching MapManager.CreateEntity switch tables route type20 to a null
  return. CreateTelepoPointList separately creates TelepoPointData cache metadata;
  FieldController.OnCacheTelepoPoint pairs it with GotoMap/PointIn for Teleport.
  Therefore these records must not be objectives. Real script warps stay eligible.
  The six bakes contain236/501/301/329/42/21 such markers. New native disassembly,
  switch destinations, method RVAs and binary/dump hashes are retained in
  `ffN_event_native.txt` and `native_provenance.json` in the investigation directory.
- Routing's narrow pit-name rule and soundscape's broader fall/crack rule disagree.
  The all-six identifier candidate lists are retained for review. A map-change
  command is travel, not proof of a trap. Narrative actor falls, progress warps,
  trial pads, broken scenery and animations must not become environmental hazards.
- Native battle entry already cancels the scan but missed the filter worker;
  MapChanging missed route/follow/reachability/browse cancellation. GetCurrentState
  can still report the preceding field after ChangeState queues a new state.
  The fixes latch the requested state and gate both chained detour scopes, cancel
  every relevant owner, and add cancellation checkpoints to beacon progress/legs.
  Latest log includes343.844ms cold owner-frame and61.521ms battle-frame samples;
  the precise source of these full-frame samples is not isolated by those logs.
  Finding/fixing cancellation gaps does not by itself prove the reported hitch gone.
- FFII AbilityUseContentListController.SetCursor at RVA3B65C0 selects
  contentList[cursor.Index] then calls Cursor.SetSource. The mod instead identifies
  the target by the cursor transform's parent, which can remain at the first row.
  Casting's own row/vitals update is a separate working producer.
- The pre-bake FFII `Field/EntityScanner.cs::ScanEntities` removes entries missing
  from the native collection or failing `NavigableEntity.IsAlive`. That property
  delegates to `FieldEntityState.IsPresent`, including its vehicle-hidden exception.
  It never records every interaction as consumption. The native port retains these
  presence rules. Known conversion reuse and a remote cache keyed only by map ID
  could nevertheless outlive script-driven action/flag changes. Script completion
  now invalidates those caches; it is not a guessed one-use-event counter.

## Implemented and offline-verified corrections

- Metadata-only TelepoPoint objectives are excluded in all six games. Wholly
  out-of-map objects are excluded from remote projection, and graph distance cannot
  invent an off-grid exit. Real native event/warp objectives remain. Exact developer
  names now accompany selected direction goals and restoration, so a different story
  variant at the same native ID/cell cannot silently substitute for the destination.
- Developer identifiers and explicit travel script names determine conversation,
  object, warp and environmental-trap labels. Msg alone has no conversational meaning.
  Dragoon is an exact FFII override whose source identity and first English sentence
  are asserted by the baker. Unknown identities remain neutral; no reward/outcome text
  enters labels. Router and soundscape now share event_semantics.h. The all-six
  Python/native parity fixture covers12,513 extracted candidate records.
- Expanded trap survey found27 additional FFV and2 FFVI final player landings. The
  original destination PointIn is often followed by player SetPos/Lerp/Move/MoveTo.
  Reviewed unconditional player programs, final layer, MapRange origin and canonical
  script hashes are recorded in warp_arrival_overrides.json. All-six grid round trips
  pass. FFV's off-grid (-1,0) duplicate event83 is not a playable destination.
- Script return refreshes native GetCanAction and other-floor projections; ordinary
  key refreshes reuse valid conversions. Same-map battle return explicitly revalidates.
  Native actionability also gates live selection. Baked persistent-flag entry guards
  cover20 additional exact native scripts (FFI10, FFIII1, FFV5, FFVI4); FFII/FFIV have
  no qualifying direct-prefix guards. Only a known flag prefix that skips every action
  consumes an event. Calls/unmodelled actions/unknown state stay available. Read-only
  prefix programs retain at most128 source nodes; the existing64-branch/8,192-op
  projection bounds apply. Repeatable doors, conversations and travel are preserved,
  and physical NPC/collider obstacles remain even when their interaction is consumed.
  Script return also invalidates previously captured route gates, preventing an old
  worker answer from publishing after a short event has already returned control.
  The actual scanner fixture covers an actionable reusable interaction surviving
  the same refresh that removes a native-disabled one-off interaction.
- Requested native state and both chained detour depths block navigation before
  GetCurrentState catches up. Battle/map entry cancel scan, filter, route, browse,
  reachability and beacon-progress owners. Running owned work checks its epoch;
  no game-thread wait or worker-native pointer is added. Actual transition/subscriber,
  blocked-entry, same-map return and queued-stale-answer fixtures cover the policy.
- Completed ordinary directed crossings join the existing trap receipts. A route may
  revisit a floor through another entrance, but cannot repeat the same directed edge
  for an unchanged exact goal/state. Flags or an actual same-floor entity-group change
  retire ordinary receipts; a new goal resets them. Both histories are bounded to400.
  Repeated-edge failure retains protection and says Route would repeat. Choose another
  route. Real all-six directed-edge tests and the valid Deist B2-B1-B2 replay remain.
- Every selected direction request cancels scan/filter refresh, including same-pose
  work and stale-origin views, then restarts a fresh refresh after its route answer.
  All filter stages and nested vehicle work observe cancellation. Parked-vehicle list
  preparation shares riding/legal-landing distance maps within its owned snapshot,
  rather than building a full route separately for each exit. Full-route reference
  parity, native gates, landing rules, wrap and cancellation are checked across six
  games. Shores cannot seed a flood through a blocked/trap/active-event landing.
- Small route captures take their first bounded slice on input. Log timing separates
  aggregate native capture from elapsed capture frames, worker queue, compute and total
  ready publication. Warm FFII actual request/capture/worker/publication fixtures have
  measured about0.2-0.3ms with fake native boundaries and immediate owner delivery;
  repeated real-bake ship-route computation is about0.02-0.05ms, cancellation tails
  below1ms in tested cases. These exclude real Unity reflection, game frames, Prism
  and audible latency. Cold complete vehicle routes and some full list preparations
  still exceed10ms on this host; no universal game/audible10ms guarantee is claimed.
- Vehicle board/land actions carry the actual wrapped shore heading. Their one
  WalkOn/WalkOff step is excluded from spoken movement counts, preserving the physical
  phases for autowalk. The exact example16 North, board the Ship North,15 West, land
  South and actual all-six watercraft phase headings have regression coverage.
- FFII Cure targeting uses native contentList[cursor.Index], not the cursor's parent.
  Native SetCursor calls Cursor.SetSource, which can retain first-row ancestry.
  Existing cast/vitals-update speech is unchanged. Four-ally/state2/state8/backtracking,
  hidden/hole/invalid/bounded-slot checks exercise the actual producer.
- Shared shop focus always includes rendered price before optional current-panel
  details. All-six item/weapon/armor/magic/relic examples, both Autodetail settings,
  both native panel choices and hidden/blank price checks cover the shared reader.
- Speech > Battle results: notifications once defaults off. Only designated result
  notifications are compared with earlier accepted buffers in this result session.
  Totals, current rows and equal first-buffer notifications retain existing behavior.
  Muted/hidden/cancelled pages do not consume messages. History is bounded to1,024
  messages/65,536 UTF-16 units/4,096 per stored line; excess lines still speak.

## Delegations and verification status

Luna gpt-6-luna supplied three bounded known-pattern edits: the notifications setting
and twelve-locale menu text; shop rendered-price insertion and the existing matrix;
FFII native-index target lookup and its existing focus fixture. Sol supplied exact
source/behavior boundaries, reviewed every diff, corrected minor fixture pointer/stub
issues, and owns all architecture, RE, integration and checks. Shop/target and earlier
notifications matrices passed their actual-source fixtures. No Astra was used. No
handoff scope remains unresolved.

Final integrated canonical build and isolated smoke pass: 6,736 name checks, zero
missing, zero undefined action/query references, all47 exports. Actual-source scanner,
filter, state, transition, notification, shop and target fixtures pass. The projection
audit covers2,454 master roots /2,686 retained event identities with zero failures;
the lower count than October15 excludes metadata-only teleport cache records.
Latest warm FFII direction pipeline measured0.191ms, including fake-native capture,
worker launch/join and immediate owner publication. Cold vehicle-route maximum was
28.706ms and boarded filter maximum19.544ms across103 world/transport cases;
85 lacked an eligible start/landing destination. Parked-vehicle refresh measured
1.065–14.910ms versus19.823–470.274ms for the full per-exit route reference, with
identical eligibility in the tested snapshots. Cancel tails measured0.066–0.658ms.
These are offline host measurements, not game-frame or audible timings.

All prior dirty work is preserved. No game was launched, attached to, controlled or
runtime-probed. Requested own FFII log only; no tester files/saves or dependency
install. Current ignored evidence is build_check/event_audit_2026_10_16, including
native function results and hashes, native shop/target/factory RVAs, label/state/bake
reports, final checks and intermediate failure reports.

All six games were checked closed, previous payloads copied to
`build_check/event_context_recovery_2026_10_16`, then deployed separately without
launching games. Installed DLL SHA256 is
`DFDDEFD1B4E16DFA33DAEE46914A61F4DB96054C7F0C46F435D26532685576CD`,
9,578,496 bytes, in every game. SDL3 and all eight pinned HRTF files match source.
Prism/settings/waypoint hashes, sizes and timestamps are unchanged. Before/after
inventories are `build_check/event_context_{before,deploy}_2026_10_16.json`.
Independent regeneration of all six bakes matches the built data byte-for-byte and
every collision-grid round trip passes. No new commit/push or release publication.
Prioritize the [focused gameplay checks](../TESTING.md#event-and-navigation-follow-up--october16)
when progression permits; gameplay/audible confirmation remains user-side.
