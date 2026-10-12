# Shared autowalk and simultaneous navigation, 2026-10-11

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
