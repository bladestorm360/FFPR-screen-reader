# Native FFPR durable memory

- 2026-10-12 results follow-up: all six native phases now use current-screen review buffers;
  the L log is removed. Displayed EXP/gil totals and headings are automatic; member EXP,
  individual item/gil drops and growth/status tables follow Autodetail. Both native input
  APIs consume review down/held/release input before native checks, including before buffer
  readiness. Completion follows the actual native callback (the old wrapper is inlined).
  [Architecture, native paths, regressions and validation](docs/re/RESULTS.md).
  Build, real-reader/input regressions, all-six native path audit, 7,052 name checks with
  zero missing, zero undefined wiring names, and isolated DLL smoke passed. Deployed and
  hash-verified in all six games: `D45EE6672A8EE496F037F9E4FC4E5D22C58DBCC43B044582C966B40E9E09F5D3`.
  Supersedes preceding deployments below; gameplay confirmation remains user-side.

- FF2 tutorial-room correction, 2026-10-12: room IDs 7 and 327 now use Rebel Hideout
  Tutorial Chamber. Ten NPC lesson scripts establish its principal role; the displayed
  circle's interaction is labelled Magic Circle, replacing generic Conversation. Exact-name
  overrides preserve the label during regeneration; map grids, routes and interaction are
  unchanged. [Context/source evidence](docs/MAP_LABELS.md#tutorial-room-context-correction-2026-10-12).
  Label data/12 locales, canonical build and isolated DLL smoke passed. After the user closed
  FF2, deployment succeeded and the installed DLL hash matched:
  `17EFABD48586375DDBEC4D182644CC0C7B5A51A6F0A10625520D64F73B1979DB`.
  This build is installed in FF2; the other five games retain the preceding all-six build.
  Gameplay confirmation remains user-side.

- 2026-10-12 follow-up: settings now have only Speech, Navigation audio, Battle, with paired
  audio volumes and EXP settings in Battle. Shared modal keys require foreground game focus
  and seed held keys on return. NPC developer prefixes/suffixes, including Japanese circled
  and full-width numbering, normalize through one live/remote helper; exact same tags receive
  stable sequential suffixes (Soldier1/Soldier2), unique tags receive none. Existing inclusion
  guards are preserved. [Source investigation, delegation and validation](docs/FOLLOWUP_2026_10_12.md).
  Build, focus/name regressions, all-map native audits (6,962 records in 12 languages;
  83,544 labels), and isolated DLL smoke passed. Deployed/hash-verified in all six games:
  `AD3E9F4F20891D2B1E5B6E50350C60EB9475E5C3730BCBDF62E95F940D4DF6FE`.
  Supersedes the arrival/floor DLL below. Gameplay confirmation remains user-side.

- Arrival/floor follow-up: user requests physical facing at the interactible and automatic
  continuation across planned floor transitions. Central `field::face_player` uses verified
  native LookAt/idle sprite state; headings are structured in route phases, including
  zero-movement/counter arrivals. Transition continuation now waits for native controls
  and fade completion instead of a one-shot timer, retaining the goal through final-floor
  asynchronous scanning. All-six native facing paths and real baked floor-route regressions
  passed offline; [evidence](docs/re/AUTOWALK.md#arrivaltransition-follow-up-evidence).
  Gameplay confirmation remains user-side; examined warp activation remains manual.
  Build/smoke and 7,016 native-name checks passed (zero missing/undefined names). Updated
  DLL deployed and hash-verified in all six games:
  `7EA63358FBF982023AF9B17E198223A0A408AD056F223614436A6A1BA855FDBF`.
  Supersedes the initial autowalk DLL below; no games launched.

- 2026-10-11 autowalk request: shared core walker plus F8 Navigation toggles Autowalk,
  Speak autowalk path, Beacon with directions, all default off. Backslash/P and controller
  LT start/stop; Slash applies to waypoints. Full current-leg speech is independent of
  beacon mode. Native input/collision, manual cancellation and planned floor continuation
  are preserved; boarding/landing/examine actions and special flight controllers remain
  manual. [Architecture and all-six native evidence](docs/re/AUTOWALK.md); offline replay
  passed 2,292 sampled map routes. Gameplay timing/handback remains user-unconfirmed.
  Build, 6,980 native-name checks, zero undefined wiring names and isolated DLL host passed.
  Deployed to all six games; installed SHA-256 matches
  `45E04ABBC645128E390DB3AEDBEF57E3823D438F09BAF0515C19AB115BBDFC0C`.
  This supersedes the title-only DLL below. No games launched. Enabling combined mode
  also turns Beacon Navigation on; the user can subsequently turn beacons off separately.

- 2026-10-11: this native C++ `hid.dll` repository is the active shared FF1–FF6 mod. Workspace/catalog instructions naming only the old managed mods are historical. Branch `master`; named remote `FFPR-screen-reader` is https://github.com/bladestorm360/FFPR-screen-reader, user-owned and verified public. Existing Claude map/navigation work is preserved.
- October 11 user-test follow-up: [popup/loading/route evidence](docs/FOLLOWUP_2026_10_11.md). Scheduler mutable captures caused silent confirmation bodies; scans ran during map construction and every additive UI load; ordinary exits were incorrectly classified as events. Shared fixes use native popup/field readiness, a below-normal immutable-data worker and bounded main-thread entity conversion. Exact first-boot 166-hook failure remains unreproduced; diagnostic status added. No game launch/control.
- User retest confirmed fast startup and map transitions; entering-map speech now follows native field readiness, which the user accepted. Title prompt was silent after the preceding wrapper-hook change. [All-six title/cinematic evidence](docs/re/TITLE_PROMPT.md): UpdateNone inlines activation and never calls SetEnableStartObject. Corrected hooks use contextual Indicator.Hide, native InitNone/InitShortcutCommand and InitSelect cancellation. Actual rendered game wording comes first; FF5/FF6 then add the requested mod instruction about waiting for the intro. Title speech/timing retest remains pending.
- Follow-up offline validation: build and isolated DLL smoke pass; 6,914 native-name checks, zero missing; zero undefined wiring names; scheduler/worker, 512-hook, route and all-game map-state checks pass. Fully decoding the selected game's grids took 0.7–3.2 ms in the offline checker, not measured game loading. Title fix build adds 6,932 native-name checks with zero missing, and verifies real startup/shortcut prompt activation in all six binaries.
- Title follow-up deployment: build, isolated DLL host and all-six native prompt call-path checks passed; all six installed DLLs verified against SHA-256 `0F283AE7ECD7D8A1380F8E4CBB39C459AA9D769D98D9566AA0B08DD5F363265B`. No games launched. This supersedes the preceding popup/loading DLL hash.
- Ownership: FF1–FF5 bladestorm360; FF6 ZKline/BlindGuyNW, inclusion permission 2026-10-07. Maintain attribution.
- Read [project instructions](AGENTS.md) and [model policy](../../../codex/MODEL_POLICY.md). Sol performs the work; delegation preserves runtime/publication boundaries.
  Updated user rules (2026-10-12): delegate only known behavior following a verified pattern;
  Sol takes over after a conceptual/design misunderstanding rather than extending repair loops.
- The initial full audit is completed/offline-verified; user gameplay testing is in progress. [Task and evidence index](docs/AUDIT_2026_10.md) and the linked follow-up distinguish implementation, offline evidence and remaining user confirmation.
- Additional user request 2026-10-11: audit the Events category in all six games; infer spoiler-free labels (FFX precedent: party chat / who talks to whom) instead of developer identifiers that expose future outcomes. Use conservative neutral labels where evidence is insufficient.
- Finalization explicitly authorized by user 2026-10-11: deploy to all six games after completion; commit/push all shared-mod work to public user-owned `FFPR-screen-reader` (create if missing); make FF1–FF5 user-owned remote repositories private. No game launch authorization.
- Finalization verified: all six games deployed, DLL hashes matched; public unified master pushed (implementation commit 07f1790); FF1-Screen-Reader through FF5-Screen-Reader each verified private. Build, 6,892 native checks, bake round-trip, route/map-state/scheduler and isolated host checks passed. Gameplay and measured load/frame timing remain unconfirmed; [test checklist](docs/TESTING.md).
- Audit also includes native threading, stability, unnecessary lag and added load time (user 2026-10-11). Verify thread ownership/lifetimes and costs offline; do not claim measured gameplay performance without runtime evidence.
- User also requires displayed item/spell/ability use feedback (healing amounts, cured status etc). Trace native presentation; no invented or hidden parameter deltas.
- User requires T to announce the active turn through the shared core in every game, using actual native turn context.
- FF2 Dreadnought captains remain solid after battle; route around them. Audit all six games for the same permanent-NPC obstruction. Native Stamp is stationary, and the old six-cell detour exception was removed; all six bakes regenerated and a real Dreadnought regression passed.
- Backslash arrival guidance applies to every interactible object/NPC: speak the facing direction at arrival, including adjacent zero-movement routes; withhold it while movement remains.
- Audit periodic/status damage (poison, venom and other conditions) in all six games; vocalize any visually displayed damage, without inventing numeric feedback for hidden calculations.
- Build: `build.bat` → `smoketest.bat` → separate `deploy.bat [ffN]`. Build writes only locally. Never launch/control games or run runtime probes without authorization.
- Sources: `../ff1/dump.cs` … `../ff6/dump.cs`; extracted maps/scripts/master tables in `../tools/mapdump`; native analysis/scripts in `../tools/ghidra`. Use matching existing analysis first.
- Visible-only game speech, native results phases, central helpers, no new deduplication, controller/context gates, manual review and Autodetail requirements apply throughout.
- Scoped user authorization, 2026-10-11: preserve legacy optional enemy HP Numbers/Percentage/Hidden modes, defaults and saved preferences in all six games. HP may be spoken without native rendering under this exception; hidden enemy status parameters remain excluded. See [approval and implementation](docs/AUDIT_2026_10.md).
- Map routing now projects off-floor groups from read-only flag snapshots and navigation-only initializer programs, with live current-group overrides, collected-chest flags and future-floor blockers. Unknown control flow stays conservative; no native script execution or save writes. [Audit evidence and limits](docs/AUDIT_2026_10.md) replaces the earlier cross-floor gaps in [port status](docs/PORT_STATUS.md).
