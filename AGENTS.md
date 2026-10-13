# Native FFPR accessibility mod

Active source for all six games is this repository, on `master`. FF1–FF5 belong to bladestorm360; FF6 retains ZKline/BlindGuyNW attribution and inclusion permission recorded in `../OPEN_ISSUES.md`. The older managed repositories are paused references. Preserve existing uncommitted work. User explicitly authorized 2026-10-11: after the full audit, deploy all six games, commit/push everything to the public user-owned `FFPR-screen-reader` remote (create if absent), and make the user-owned FF1–FF5 remotes private. Preserve old history and existing named remotes.

Read `CODEX_MEMORY.md`, the shared `../../../AGENTS.md`, and `../../../codex/MODEL_POLICY.md`. Sol owns research, implementation, review and verification. Use Luna only for an economical bounded mechanical handoff; Astra only for the documented emergency criteria. Summarize any delegation, including outcome and remaining issues.

Current repository identity, October 13: local `FFPRCore-Screen-Reader`, branch `master`,
named remote `FFPRCore-Screen-Reader`, public user-owned
https://github.com/bladestorm360/FFPRCore-Screen-Reader. The user explicitly requested this
one-time rename with history preserved. For **prepare release X.X**, follow
[docs/release.md](docs/release.md) through README/TXT, full dependency bundle, 7-Zip
verification, commit/push and GitHub release publication. Do not repeat repository
renaming as part of that workflow. Installed dependency receipt identifiers are stable
compatibility data and are not repository names to migrate.

Prism 0.18.3 x64 is the required current speech backend. Keep every reader behind say/speech
and preserve speech/braille, queue/event identity, interruption, native readiness and worker
ownership. No new Tolk call sites or speech deduplication. Use the committed prism.lock.json
and matching SDK. Install dependencies only through separate `install_prism.bat [ffN]`;
normal builds/deploys must never download, install, copy, repair or upgrade Prism. Deployment
checks are read-only; an unchanged verified prerequisite install performs zero writes.
Detailed contract/evidence: `docs/PRISM_MIGRATION.md`. The user explicitly requested removal
of game-root Tolk.dll after successful migration deployment; retain recovery copies outside
the games, without deleting other dependencies or the preserved historical source.
Luna eligibility requires known behavior/correctness criteria and a verified pattern with
clear scope/checks. Sol retains discovery, design, unfamiliar lifetimes/pointers/callbacks
and game-state judgment. A conceptual misunderstanding in the first Luna result means Sol
takes over; minor localized corrections remain appropriate (user instructions, 2026-10-12).

Build with `build.bat`, then `smoketest.bat`; deployment is separate (`deploy.bat [ffN]`). These scripts must never launch a game. Gameplay testing and runtime probes are user-side unless explicitly authorized. Complete the current audit and offline checks before requesting gameplay tests.

Use name-resolved IL2CPP references and `hooks::attach`; verify signatures and folded native bodies against matching dumps/analysis. Preserve the custom MinHook coexistence changes. Hooks own UI timing; bounded deferrals start from native events. Game pointers need lifetime checks. Centralize services and use them at call sites; no new deduplication without explicit permission.

Replacement persistent preference, October 12: game-thread hooks may only detect state
or copy bounded ready native data and immediately continue the original operation.
Substantial speech/audio/logging/I/O/text/scanning/extraction/pathfinding work belongs
on workers or genuinely nonblocking snapshot coroutines. The game thread never waits
for workers/queues/locks/I/O/RPC/sleeps/joins, including startup/loading/shutdown.
Keep thread-affine APIs and native pointer lifetimes on their owner. Approved input/
movement actions remain isolated. Record ownership/evidence in docs/re/THREADING.md;
modded/unmodded runtime measurements remain user-side unless explicitly authorized.

October 13 responsiveness clarification: cheap hooks alone do not certify acceptable
mod performance. Measure player request through capture/batching, snapshot readiness,
worker/IPC queue, computation and first usable output, with relevant snapshot age.
Use evidence-based feature/platform budgets and normal/large/busy/cold/warm/rapid-action
checks. Prefer offline preprocessing, owned caches/live deltas and obsolete-event
cancellation; never weaken native affinity, reveal or speech-order contracts. Runtime
measurement remains user-side. Current [speech ordering/budgets](docs/re/SPEECH_ORDER.md)
and [navigation capture/compute evidence](docs/re/THREADING.md) distinguish compute,
fake-SDK first submission and actual audible/game-frame timing.

Speech interrupts once per new input/context; category/heading then initial focus and
same-event callback output queue through the central sequence/continuation services.
Never restore arbitrary speech delays or add text deduplication. Scope queued callbacks
only to known mod-owned operations, never around native game calls or on workers.

All navigable maps must have baked data. Missing bakes are extraction/data bugs;
never restore the deprecated native MapRouteSearcher, its distance cap, breadcrumb
fallback or temporary game-grid writes. Use the cancellable owned-snapshot baked
worker service for routes, floods and cross-floor planning. Current native instances
come from lifecycle/parent observations, never whole-scene searches on callbacks.
See docs/re/THREADING.md for ownership, capture budgets, evidence and validation.

Speak game information only when displayed, or under an explicit scoped user approval. Read active rendered fields, labels, comparison arrows and bar values rather than every parameter in a shared data class. Do not infer approval from old implementation. Results follow native phases and current displayed details; no early disclosure of later rewards. Keep Autodetail persistent, default off, and manual details/review available.

Scoped user approval, 2026-10-11: retain the optional Enemy HP Display modes and saved/default preferences in every game, despite HP not being rendered in native target panels. Numbers adds `HP current/maximum`; Percentage adds `remaining%`; Hidden adds nothing and must not query parameters. This exception covers enemy HP only, not hidden enemy conditions or other statistics. F5 cycles these modes during battle; the Battle submenu explains the disclosure. Scan/Libra follows the ordinary displayed-message reader.

Results follow-up, 2026-10-12: all six growth tables support Battle's changed-stats toggle
(default on). October 13 user correction: I/controller West reads every delta in the
current results phase, independent of highlighted row or automatic-summary preferences.
FF2 alone has nested weapon rows;
keep its full displayed weapon/level/gauge buffer independent from delta summaries and
Autodetail. User explicitly requests current displayed weapon progress comparisons using
the native before/after battle data. Scope that access to matching visible weapon icons,
levels and gauges; no unused/unequipped skill disclosure. Use native ExpUtility integer
operands and ceil only at speech formatting; FF2's verified minimum gain is 1% (16 levels,
20..100 point intervals). Native result updates own pending readiness; no 30-frame page
deadline. Read hit/damage digit sprites, without attack-sound or status-capacity arithmetic.
See `docs/re/RESULTS.md` for evidence, controls, validation and remaining gameplay checks.

Settings use exactly Speech, Navigation audio, Battle submenus, followed by Close Menu;
volumes sit beside their audio setting and battle results settings belong to Battle (user,
2026-10-12). Stat delta detail belongs to Speech because it also governs equipment comparisons
(user correction, 2026-10-13). Preserve direct F8 keyboard entry and controller mode/context input ownership.
All mod modal keyboard input requires active game-window focus, with held-key seeding on
return. Never add keyboard mod mode. Navigation must cancel at battle transition, preserve
live story gates and map state, route between floors to each transition, and explain blocked
routes without filtering away every useful destination.
NPC labels remove developer prefixes/suffixes (ASCII, full-width and circled numbering).
Only exact same descriptive tags on the same floor get sequential suffixes, e.g. Soldier1,
Soldier2; unique names have none. Use the central NPC naming service for live/remote entries;
number before distance/reachability filters. Preserve existing entity inclusion guards.

World navigation includes already-boarded vehicles: use the current riding grid,
legal native landing rules and walking/owned automatic-vehicle legs. World edges wrap
in search, floods, direction grouping and beacon corners. Vehicle identity changes
invalidate pending/preserved plans; all computation stays on the Navigation worker.
Current regression/evidence: docs/re/THREADING.md.

Shared autowalk and simultaneous beacon/directions are implemented in the core. Preserve
the three independent, default-off Navigation settings and the Backslash/P/Slash/controller
LT policy. Native movement/collision owns each tile; never drive a special flight controller
as a tile controller. Manual input and unsafe contexts stop the walk, and only a planned
matching floor transition may resume it. See `docs/re/AUTOWALK.md` for contracts and evidence.
Autowalk must physically face the interactible through the central native facing service
after arrival, including adjacent zero-movement routes. Preserve structured headings;
never parse translated facing speech. Floor continuation waits for native input and fade
readiness, retaining the final destination through asynchronous scans and every planned leg.
Build now includes `tools/autowalk_check.cpp` route replay across all six bakes. Gameplay
confirmation is still separate from offline verification.

Authoritative task/evidence records: `docs/AUDIT_2026_10.md`, existing `docs/PORT_STATUS.md`, `docs/INPUT_BINDINGS.md`, `docs/TESTING.md`, and `../tools/mapdump/`. Keep evidence and unresolved issues current; distinguish implemented, offline verified and gameplay confirmed.
