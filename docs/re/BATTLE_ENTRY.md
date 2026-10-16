# Battle admission, opening presentation and enemy formations — October16

The user reports roughly one second of lag before battle entrance audio in FFII,
and requests opening enemy formations across all six games. Sol implemented this
follow-up independently; no delegation, launch, input, attachment or runtime probe.

## Log evidence and scope of the repair

The own FFII session started October16 09:31:57, PID38132, installed diagnostic DLL
17AE6B07. Preserved before rotation as
`build_check/battle_entry_2026_10_16/ff2_reported.log`.

- Frame6857: `GameState.BattleTransition` takes40.968ms (self40.960ms) before the
  chained native ChangeState. No child event handler accounts for5ms or more.
  Result/removal resets previously ran outside individual leaf scopes here;
  wall-clock self time does not prove which reset used CPU or excluded a wait.
- This is followed by44.444ms in the timed original and a489.652ms input gap
  at frame6864. No queued navigation tasks or ready posts remain at that gap.
- Frame8075:837.985ms input gap precedes state12 and13 markers. State13 then takes
  only4.912ms in the timed original, with no slow battle-prefix scope.
- These observations prove synchronous cleanup was in the native-entry prefix.
  They do **not** isolate the full reported delay or prove the game is responsible.
  Performance acceptance remains open until ordinary gameplay supplies new evidence.

## Two native lifecycle boundaries

`main_game_transition(13)` immediately closes field admission and fires
`BattleEntering`. Its handlers only change flags/epochs: category identity,
scanner/filter generations, four route-worker owners, autowalk intent, audio-loop
flags and the audio worker's field generation/wall mask. They perform no native
queries, scene scans, worker waits, callback destruction or frame-queue walks.
Active helpers cannot revive navigation using the old reported field state.

`worker_job::Owner::interrupt()` changes the shared atomic epoch only. Captured
native-owner callbacks stay on their owner until normal `cancel()` cleanup. Owned
workers observe cancellation at their established cooperative checks and discard
obsolete results on their worker. No native pointer/callback moves to a worker.
Retained objects are bounded by the existing job registries/capture limits; normal
BattleStart/field-return/request cancellation retires them. Repeated state13 calls
do not repeat either event or cleanup. Autowalk releases virtual movement/key flags
immediately while retaining its destination for exactly one later cleanup.

The all-six native `StartInBattle` phase enables units and calls PlayBGM;
`StartPreeMptiveMes` follows it and precedes Play. After its original returns,
`on_battle_start()` performs the existing result/removal resets and BattleStart
cleanup once. Cancellation remains immediate while entrance presentation/audio no
longer waits for those resets and route-cache/callback disposal. Audio invalidation
uses atomic output signals; device/mixer calls remain on the existing audio worker.

Rare automatic markers/original scopes now cover encounter animation setup,
initialization, object creation, entrance/BGM and opening presentation. They add no
hot engine-invoke hook or key. Reset leaf scopes distinguish future expensive
result/removal cleanup. A marker gap can still include unobserved engine work;
inlining/alternate native paths can bypass an individual audited body.

## All-six enemy producer evidence

`tools/re/battle_entry_native.py` reads local dumps, matching GameAssembly PE bodies
and each game's targeted key-battle asset bundle offline. Reproducible hashes,
unique RVAs, signatures/fields and serialized Text/controller/view references are
in [battle_entry_native.json](battle_entry_native.json). Bounded annotated native
disassemblies remain in the investigation folder. Thirteen native bodies per game
were audited; the new hooked bodies are unique, without folded-body detours.

| Game | Opening producer | Enemy-list UpdateView | Enemy row UpdateView |
| --- | --- | --- | --- |
| I | 0x3E6FD0 | 0x3C2FE0 | 0x3C1BD0 |
| II | 0x395490 | 0x43B030 | 0x439BF0 |
| III | 0x3B6330 | 0x3C6AA0 | 0x3C5610 |
| IV | 0x36A400 | 0x96BDD0 | 0x96A8C0 |
| V | 0x33FAD0 | 0x3F0200 | 0x3EECD0 |
| VI | 0x3BCBC0 | 0x417DC0 | 0x416690 |

Normal opening code has no roster-description text. Special opening conditions
use the native command-message reader, which remains unchanged. The row producer
gets the native localized enemy name, assigns nameText and always assigns the
numeric group count, even when its count graphic is hidden for one enemy. The
parent list producer deactivates its pool then activates/updates current groups.
Read its assigned active rows, rather than walking encounter tables, querying HP,
or inferring later phases from a bestiary preset. Native order and distinct groups
with identical names are preserved. There is no new text-content deduplication.

Opening formation disclosure is explicitly user-approved, including assigned rows
while the parent canvas is fading. The current producer is the ownership root;
no scene/singleton lookup or retained native panel pointer is needed. Capture is
bounded to64 pool entries, nine active rows,256 characters/name and16/count.
Unexpected/missing data stays pending for a later native update without a timer,
invented count or partial/truncated roster. Current rows are copied to owned strings.

The ready event submits one `Enemy formation` heading and `count name` per group.
If rows arrive after that event, their native postfix publishes instead. Cleaning,
joining, chunking and backend work run on the existing speech worker. First-turn
speech queues behind the roster; later turns retain interruption. Native opening
messages remain on their existing queue. The central Game speech master applies;
Autodetail does not. The heading has all12 mod locales. Battle end/new admission
retires the event identity; old/preloaded panels cannot announce a prior encounter.

## Verification and remaining confirmation

The actual-source checks cover admission-before-original with cleanup deferred,
rare detour ABI/original ordering, each route/scanner worker epoch without native
reads or callback disposal, autowalk/audio release, blocked driver cancellation,
stale publication, native roster order/counts, pooled entries, unknown same-name
groups, repeated UI refreshes, late readiness, incomplete/empty/oversized data and
separate encounters. Offline checks cannot confirm first audible output or actual
engine frame time. [First gameplay checks](../TESTING.md#battle-entry-and-formations--october16)
keep that distinction explicit.

Canonical build and every regression passed:6,784 native-name checks/zero missing,
zero undefined wiring,47 exports. Isolated DLL loader smoke passed forwarding,
startup/logging and normal exit. New actual-source entry, formation, route/scanner,
autowalk and audio checks all pass. Shared dispatcher burst peak0.551ms offline;
timing adds0.039us/call across one million warm fixture callbacks (baseline0.981ms,
timed39.912ms). These are fixture measurements, not actual entrance/audio guarantees.

All six games were checked closed before backup/deployment. Installed DLL hashes
match `0222C0EC149EBAD0FEBAAE01671F4FE11BF89C80B0A1352B8CCF2466FE17C17A`
(9,631,744 bytes); SDL3/pinned HRTF payloads match source. Prism, INI and waypoint
hashes/sizes/timestamps are unchanged. Recovery:
`build_check/battle_entry_recovery_2026_10_16`; before/verify inventories:
`build_check/battle_entry_{before,deploy}_2026_10_16.json`. Reports/native artifacts
are under `build_check/battle_entry_2026_10_16`, with final checks preserved there.
Prior dirty work is preserved. No dependency install, game launch/control/probe,
save change, commit/push or new publication was performed. First audible formation
and the remaining intermittent-stall cause await ordinary gameplay confirmation.


## Opening-message order follow-up, October16

User observed formation interruption, then clarified that pressing Control may have
caused it. Explicit silence remains effective; no global noninterruptible speech
mode or backend completion wait was added. The existing first native command turn
already queues behind the roster and remains unchanged.

A separate source-order defect was confirmed: the native ambush/preemptive condition
went through the two-frame action-name/actor pairing guard, whereas the formation
was submitted immediately in the opening postfix. The six matching
StartPreeMptiveMes bodies call SetCommandMessageAtKey synchronously before returning
(FFVI retains its actual two-argument message ABI). A scoped thread-local producer
tag now classifies just these native opening calls. Their already-read localized
text queues immediately, before the postfix's formation. Normal action pairing
retains its two-frame event guard in battle_message_line.h; no text comparison,
new delay, native pointer retention, new hook, native write or worker wait is added.
The scope classifies provenance only; it is not QueuedSpeechScope around native code.
It unwinds on exceptions and ends before BattleStart cleanup/command presentation.

The actual formation/start/pairing fixture covers all-six shared paths, ambush,
preemptive, back attack, ordinary openings, action-first/message-first pairing,
unpaired messages and exception unwinding. The real speech front end/worker/fake
Prism SDK fixture checks condition -> formation -> first turn -> command, central
speech mute and explicit silence cancelling the pending opening chain. These are
offline submissions, not confirmation of audible gameplay order. First user check:
let the opening finish without Control, then separately confirm Control stops it.
The known intermittent lag remains under investigation in STALL_2026_10_16.md.
