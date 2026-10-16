# Shared autowalk and simultaneous navigation, 2026-10-11

## October 16 controller/mode audit

The tester's exact game, native mode and input device remain unknown. Two concrete defects
were found: eligibility accepted only ordinary key controllers and FFVI's key airship;
the cached controller could be replaced by the last initialized inactive alternative,
without observing native controller switches. A failed FFVI flight hook also disabled
ordinary walking. These are fixed independently; they are plausible causes, not a claim
that the tester's exact failure has been reproduced.

`live_context.cpp` observes native `FieldMap.OnSwitchPlayerController` **after** its original
and captures the actual current controller. Later inactive Init callbacks cannot overwrite
a live enabled owner. The fallback reads only the native FieldController player/controller
array, bounded to 32 entries, matching the real player, active object and InputEnable.
No scene search or native pointer goes to a worker. Ordinary key, touch route/vector,
FFIII special-airship, all-six key/touch airship and III/V/VI key/touch ground-bird profiles
are supported. Capability failure is per profile and cannot disable ordinary walking.

Airship key controls use the native two-shoulder cardinal branch; touch airships retain
their actual translation-mode setting. Turning mounts and nontranslation touch airships
use bounded analog steering through their original axis callback and native LateUpdate.
`native_steering.h` derives countersteering from native acceleration/cap/decay; it waits
until heading/momentum settle before requesting forward motion. Native collision, speed,
animation, encounters and position/camera updates remain native. Turning requires observed
angle progress and has a 15-second ceiling; a stalled turn stops. Native TwoStep controls
are up/down choices with a no-op movement callback: they ask the player to choose an option,
rather than inventing a spatial route. Existing manual III/VI airship boarding stays manual.

A request during a temporary input handoff waits at most .75 seconds without movement,
then requests a fresh worker route from current state. Focus/menu/manual input, map identity,
settings and BattleEntering invalidate that request. Hook failure, input readiness and
choice ownership now have distinct feedback. No game-thread wait, sleep, scene scan,
script execution, transform write, direct MoveTo call or new diagnostic key is introduced.
Remapped direction requests share the existing start/stop policy.

Offline evidence: `tools/re/autowalk_modes.py` records matching game/dump hashes, all native
controller declarations, method signatures/body hashes/aliases and actual transportation
master rows for all six. Outputs are in
`build_check/autowalk_controls_2026_10_16/native/`. Original native rotation coefficient .2,
cap 2*scale and decay .1 are preserved in the steering replay. 540 heading/momentum/scale/
direction cases settle within 122 simulated frames, with no forward movement before alignment.
`active_controller_check` uses actual lifecycle/selector code; `autowalk_native_check` uses
actual movement detours with all applicable mode profiles, missing-flight-hook isolation,
handoff replanning/timeout, and immediate battle/manual cancellation. Existing replay covers
all six baked map sets. These tests do not establish actual game-frame, collision response,
audible latency or every mount's gameplay behavior; the targeted user checks are in TESTING.

The durable matching-binary/signature/body-hash/alias/transport receipt is
[autowalk_modes_native.json](autowalk_modes_native.json); disassembly remains in the local
build-check record and can be regenerated with the audited offline tool.

User request: autowalk as a core feature for FF1–FF6 on navigable maps, a persistent menu
toggle, Backslash to walk when enabled, a separate full-route speech toggle, and an option
to use beacons and spoken directions together. Ordinary autowalk is explicitly authorized;
game launch/control and runtime probes remain user-side. Sol performed the implementation,
review and offline verification without delegation, under the shared model policy.

## Implementation and ownership

- `src/core/fieldnav/autowalk.cpp`, `autowalk_route.h`: shared native input adapter and pure
  cell follower. No game-specific walker, transform writes, teleportation or script execution.
- `field::PathInfo::walk_phases` carries exact cells/layers and manual action boundaries,
  separately from `world_path` and speech. `full_description` has every current-map leg;
  ordinary repeated directions keep their existing six-leg cap. Across maps, future legs
  are calculated only after native arrival, with current story gates and NPC state.
- Arrival now carries a structured `arrival_direction` in each phase. The shared
  `field::face_player` service calls the player's inherited `FieldSpriteEntity.LookAt`
  with the cardinal native Vector2 and `changeAnimDefault=true`, then verifies the
  name-resolved `FieldEntity.directionVector`. It turns in place after movement has ended,
  including zero-movement arrivals and NPCs across counters; no confirm action is sent.
  Ordinary direction speech still retains its facing guidance. Autowalk says "Arrived"
  after confirmed native facing, retaining the manual guidance if turning fails.
- Baked maps supply phases through `baked_nav::fill_result`. The native searcher fallback
  converts its world points to cells and keeps native collision/layer handling. The old
  FF5 evidence establishes that native search points are world coordinates, not grid cells:
  `breadcrumb_chain.cpp` source header. Origin `(0,0,0)` is valid; navigation checks the
  player/transform instead of treating that position as missing.
- `baked_nav::walk_edge` checks live solid NPCs, directional gates, movement rules, passages
  and stair layers before each new tile. Closed native exit/town cells are allowed only on
  a phase's final edge, matching the planner; live gates and native collision still veto them.
  A moved selected NPC stops the original walk instead of reporting a stale arrival.
- `Mod.NavigateToEntity` owns entity request policy for Backslash/P and controller LT.
  `WaypointHandler.PathfindToCurrentWaypoint` uses the same walker for Slash/controller LT.
  An active request stops the walk. Target cycling and other manual input cancel it.
- Settings in F8 → Navigation audio: `Autowalk`, `SpeakAutowalkPath`, `BeaconWithDirections`, all
  default false. The existing `AudioBeacons` setting still controls beacon playback.
  Enabling combined mode also enables Beacon Navigation; the user may explicitly switch
  beacons off afterwards. With autowalk off, combined mode restarts the beacon and speaks directions. With
  autowalk on, `SpeakAutowalkPath` controls route speech independently of combined mode.
  Settings use the existing per-game INI persistence and grouped menu/controller ownership.
- `GameStatePatches.IsFieldNavigationReady` exposes the existing native Player (3) / Transport
  (4) readiness rule to input and waypoints. Field feature keys work in both; game toggle
  gates keep the narrower Player-state rule where the native game requires it.

## Native evidence, signatures and lifetimes

`tools/re/movement_calls.py` reads the installed matching PE files and each dump, without
loading them. Complete instruction/call records are `ff1_movement_calls.txt` through
`ff6_movement_calls.txt` here. These confirm in every game:

1. Global `InputDevice.ObserveInput` calls `get_InputAxis` rather than inlining it, then
   tests against zero and dispatches the axis to enabled `IInputDeviceListener` instances.
2. `get_InputAxis`: native `Vector2(void* self, const MethodInfo*)`; the eight-byte value
   returns in RAX. The existing native input source executes first. Real movement input
   cancels the walker before any synthetic axis is returned, including native remaps/touch.
3. `Last.Map.FieldPlayerKeyController.OnTouchPadCallback(Vector2)` receives its eight-byte
   argument in RDX, multiplies by **16 native units**, and calls the player's native MoveTo
   virtual slot. Collision, encounters, dash speed, triggers and stairs remain native.
4. `Last.Entity.Field.FieldEntity.get_IsMoving` includes the current movement time/list.
   Autowalk returns zero while moving, and advances/turns only after completed displacement.
5. The controller's inherited `fieldPlayer` and `<InputEnable>k__BackingField` are name
   resolved and checked for the current live player. No native player/controller pointer
   is retained between calls. The follower stores copied route data; entity pointers are
   identity tokens only. An exception cancels the walk and returns physical input.

FF1 example RVAs: ObserveInput `0x3AB370`, get_InputAxis `0x3AC240`, KeyController callback
`0x963560`, IsMoving `0xE8A400`. All-six current RVAs and caller instructions are retained
in the per-game exports. The code resolves names, never these fixed addresses.

Read-only headless Ghidra reused `FF1_Analysis` with `-process -noanalysis -readOnly` and
`DecompileMany.java`. Local export `ff1_autowalk_decompiled.c` confirms ObserveInput,
get_InputAxis and IsMoving; the archive has no function defined at the movement callback,
so its complete matching-binary disassembly above supplies that evidence. Ghidra discarded
its in-memory language-version changes. No archive import/overwrite or analysis timeout.

Cancellation occurs on focus loss, physical input, mod input ownership, battle/menu/dialogue,
native input disable, unavailable field state, stale player, changed route/map, blocked next
cell or two seconds without movement. Battle cancellation never resumes automatically.
`events::Kind::MapChanging` fires before old-map invalidation. Only a planned cross-floor
destination may wait for the matching next map. On the final approach to an exit, native
input disable or fading arms that wait before map construction can invalidate the old player.
MapChanged starts an event-bound readiness wait, replacing the old one-shot 0.8-second
timer. It resumes only when native Player/Transport state, input ownership, the current
controller and the fade are ready. Each wait is bounded to 30 seconds; an unexpected map,
battle, menu, dialogue, focus loss or manual input cancels it. `autowalk_transition.h` owns
the tested expected-map/readiness lifecycle. The original MapGoal/name/kind survive every
leg, including routes that leave a floor and return to it. A pending destination is retained
through the final leg so asynchronous scanning and the beacon can keep the same target.
Autowalk owns that continuation, so the existing spoken-route callback cannot read a leg
when `SpeakAutowalkPath` is off. There is no new speech deduplication or keyboard mod mode.

## Scope, verification and remaining gameplay checks

The walker applies to ordinary tile movement on field maps and world maps, including
native tile-controlled vehicles and automatic canoe/snowcraft traversal. Boarding, landing,
examining a warp, confirming interactions and special airship flight controllers remain
manual. Manual phase boundaries announce the required action and stop; custom flight/choice
controllers are explicitly rejected rather than driven with an incorrect tile axis.
Unreachable/story-gated destinations retain the existing route explanation.

Offline production-follower tests cover turns, duplicate native cell points, zero-movement
arrival/facing, stairs, unexpected displacement, revisited cells, world seams, manual warps
and boarding. `tools/autowalk_check.cpp` also replays sampled routes from every baked submap
with a walking cell: **2,292 routes across all six games, zero failures**. Another 258 maps
have no baked walkable cell (including presentation/non-navigation maps); these are not
claimed gameplay-tested. Native-name/ABI checks pass for all six games. The canonical build
runs the follower check and the existing route, map-state, scheduler and hook-coexistence checks.

Runtime movement timing, map transitions, controller handback and actual collision/encounter
behavior remain user-unconfirmed. Follow the concise checklist in `../TESTING.md`.
Build/smoke/deployment results and the final DLL hash are recorded there and in project memory.

## Arrival/transition follow-up evidence

The user explicitly requested physical arrival facing and continuation through floor
transitions. `tools/re/autowalk_arrival_calls.py` retains all-six native calls, actual PE
SHA-256 values and manifests in `ffN_arrival_calls.txt` / `ffN_arrival_manifest.tsv`.
`FieldSpriteEntity.LookAt(Vector2,bool)` takes this/axis/flag/MethodInfo in RCX/RDX/R8/R9.
In FF1/FF2 it inlines the base direction update; FF3 calls `FieldEntity.LookAt` instead.
The checker handles both compiler paths and verifies both native direction-vector writes
in every game. FieldSpriteEntity is FieldPlayer's inherited LookAt override; its animation
calls display the idle-facing sprite and do not move, teleport or activate the target.

Read-only headless Ghidra reused FF1_Analysis with `DecompileManifest.java`, defining missing
metadata functions only in memory and discarding all changes. Local export
`ff1_arrival_complete.c` confirms sprite/base LookAt, GotoMap CanEnter, trigger overlap and
FootEvent PreExcute/Excute/RequestTriggerAction. FF1 facing RVAs are base `0xE881F0`, sprite
`0xE94CC0`; FootEvent trigger request is `0x645520`. GotoMap's stay callback is a no-op;
native foot/overlap processing owns map activation. The walker supplies the last ordinary
direction towards the native transition cell, and the game owns the transition itself.
Examined warps are routed to an interaction cell and faced before the manual instruction;
their activation remains manual. No game was launched and no runtime probe was requested.

Offline checks now include all four arrival headings, zero-movement facing, counters,
slow fades/input disable, expected/unexpected maps, cancellation and a route that returns
to its starting floor. A real baked GotoMap → matching PointIn → interactible route is
also replayed in every game, in addition to the existing 2,292 sampled map routes.
All passed. Native facing and actual multi-floor gameplay still await user confirmation.

## Vehicle action follow-up, October 13

The user now explicitly authorizes automatic boarding/disembarking/landing through
native input, including controller navigation requests. This supersedes the earlier
manual vehicle boundary above; unrelated interaction confirms remain manual.

Phase metadata carries VehicleAction and transportation identity/facing, separately
from translated speech. Ship/water-craft legs include the adjacent boarding cell and
extra shore tile. The follower emits ordinary input for WalkOn/WalkOff boundaries;
walk_edge retains native collision and checks the baked ride/foot/landing rules.
No world/transform/mechanics or grid writes occur.

Explicit boarding/landing queues one native Confirm frame; FF6 plane/special-plane
landing uses Back. Input manager and listener routes receive the same pulse. Obsolete
or unsafe requests release it; the confirmed native transportation id and input-ready
controller own continuation, with a bounded 10-second failed-action wait. Automatic
vehicle transitions tolerate native input handoff and verify the resulting mode before
continuing. On-foot/owned automatic-content modes are valid walking continuations.

FF6 FieldPlayerKeyAirshipController.LateUpdateObserveInput 0x5F3FD0 uses its cardinal
branch when isDownPageUp or both isDownShiftDown/isDownShiftUp are set. OnKey(2/3)
and OnKeyUp(2/3), RVAs 0x5F4610/0x5F45B0, are the native shoulder input callbacks.
Autowalk supplies both only during the original update, restoring pre-existing holds.
OnTouchPadCallback 0x5F4670 receives the common InputDevice axis and stores it.
MoveToAxis 0x5F4330 rotates that axis by the native bird-camera angle, normalizes it
and submits one 16-unit movement to the native entity; autowalk supplies the inverse
rotation and waits for native IsMoving completion. It never calls MoveToAxis directly.
SwitchInputEnable also registers the current flight controller in the shared player cache.

Local bounded exports: ignored build_check/re/ff6_equipment_party_airship.txt and
ffN_vehicle_input.txt, in addition to existing ffN_movement_calls.txt. Native behavior,
not merely class presence, established these inputs/geometry. Native input pulses are
inactive outside their exact frame/release frame; listener names are not read for this
purpose while autowalk has no pulse. All engine pointers remain on the native owner.

Existing Backslash/P and controller LT entity requests share Mod.NavigateToEntity;
Slash and waypoint LT share WaypointHandler.PathfindToCurrentWaypoint. Autowalk mode
selects the walker in those common actions; controller input does not have a separate
route policy. Gameplay timing and untested vehicle/story cases remain user-side.

Final canonical build, all-six 2,292 baked route replays, route-worker and actual-native-
input fixtures pass. Exact planned action cells prevent stale-route confirms; unexpected
vehicle identities cannot resume a ride. Installed/hash-verified in all six games with
the menu/results follow-up recorded in MENU_DETAILS.md. New automated boarding/landing,
FF6 flight and controller handback remain gameplay-unconfirmed. Existing user confirmation
of FF2 ship routing and cross-vehicle path directions is not autowalk confirmation.


## FF3/FF6 airship follow-up, October 14

The user withdraws automatic airship boarding for FF3 and FF6. Plane and special
plane boarding phases keep their manual instruction but carry VehicleAction::None,
so the walker stops at that boundary without Confirm. Ordinary ships retain their
existing action policy, and already-steering routes remain available. Future deck
automation would have to walk to the wheel, interact, select Take the wheel, wait
for the native steering transition/readiness, then replan/continue. This sequence
is documentation only; no wheel interaction automation was added.

The FF6 native late update consumes its stored input axis before calling the
common InputDevice getter at its tail. Providing only the latter left the previous
route direction in place at a corner or stop. Before original flight processing,
the owner now publishes the current route axis through native OnTouchPadCallback,
using the existing inverse bird-camera rotation and both native shoulder callbacks.
The same callback releases the stored synthetic axis after cancel/arrival; native
manual input has priority. No direct MoveToAxis, heading/transform, grid or vehicle
state writes are introduced. Input remains isolated on the native owner thread.

The actual-source native-input fixture now models the recovered consume-then-observe
order, and checks corners, stored-axis release, arrival, cancel, manual priority,
inverse rotation and pre-existing shoulder holds. It passes. The safe controller
check accepts the FF6 flight controller as well as ordinary walking input. A bounded
off-route diagnostic records map/cell/layer/native vehicle identity if a user-side
retest still stops. Real Falcon flight/handback timing remains gameplay-unconfirmed.
