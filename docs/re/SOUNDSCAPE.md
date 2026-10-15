# Soundscape implementation — October 15, 2026

Status: implemented, offline verified and deployed/hash-verified in all six games.
The user lifted the deployment hold on October 15. No game
launch/control/probe is authorized. Existing keyboard/flee/safe-teleport changes and
unrelated dirty work must be preserved. Sol owns this implementation and validation.

## Accepted behavior

- Use only the existing pathfinder-eligible entity registry, not a new database audit
  or scene search. ToLayer and standalone doors are silent; opened chests are silent.
- Independent repeating spatial voices: NPC chirp, muted chest chime, harmonic rising
  exit triple tone, distinct door knock, meaningful event double beep, lower trap
  double beep (faster within two steps), vehicle double chirp and save-point cue.
  Pair doors with actual exits within one tile including their own tile. Progress
  warps, forced battles and FF1 trial pads are not automatically traps. Only explicit
  developer identifying strings establish a resetting/falling/collapsing hazard.
- Default range 10 steps, 5–25 in steps of five. Shift+semicolon reduces it;
  Shift+apostrophe increases it. Both are free after the input audit and do nothing
  when the soundscape is off. No nearest-N cutoff or room sweep. Same-kind nearby
  voices vary slightly in pathfinder order when HRTF is inactive.
- Soundscape defaults off, with per-kind switches/volumes, master volume and previews
  in its own submenu below the switch in Navigation audio. Navigation audio has an
  independent master switch at the top; existing individual preferences remain saved.
- HRTF defaults off. Worker-owned loopback spatial rendering feeds the existing SDL
  device; actual HRTF availability is checked. Missing/failed HRTF uses stereo plus
  front/back pitch cues. Active HRTF has no directional pitch shift; wall tones retain
  their distinct pitches. Include runtime and licenses in future installs/releases.
- Every mod sound has an embedded default WAV and an optional game-root `sounds`
  override. Missing/invalid individual files fall back individually. Load/parse/DSP/
  mixing/filesystem work stays off the native/game owner. Document every filename.
- Speech master defaults on and mutes game/mod gameplay announcements only. The
  speech-only mod menu always speaks, including while changing or restoring this
  master. Keep this exception narrowly scoped at centralized say/speech ingress;
  preserve event identity/order, cancellation, repeat and braille. No text dedup.

## Reference and ownership

Read-only reference: Custom/FFXII/FFXII-Screen-Reader soundscape and Lessons.md.
Its independent voice clocks and actual-clip-duration cadence are useful; its older
game-thread polling/locking is not the FFPR ownership model. SDL subsystem initialization
remains on FFPR's dedicated SDL owner, audio device/DSP on its existing worker. Native
entity pointers never cross into audio work. Snapshot capture must have time/count,
staleness, context generation and overload/cancellation bounds. No waiting on game hooks.

OpenAL Soft loopback supplies HRTF without a second hardware device. Official versioned
runtime/headers and LGPL/dataset notices are retained. Actual HRTF enablement is verified.

## Acceptance scope

Offline verification covers production classification/adjacency, independent voices/range/pitch/cadence, overrides
and malformed files, master/cancel/stale transitions, nested menu and all-six keys;
actual loopback HRTF and fallback; future bundle completeness; native-owner capture
cost and worker request-to-first-output/overload bounds. Canonical build and isolated
smoke pass; short prioritized user gameplay checks are recorded in TESTING.md. Actual
audible/game-frame performance remains unmeasured until authorized/user gameplay.

## Implemented ownership and bounds

One stable cue bank contains all 18 effects, including the previous movement, wall,
beacon, counter and landing sounds. Original PCM16 WAVs are generated deterministically
by `tools/generate_cues.py` and embedded as RCDATA. `cue_bank` loads individual optional
game-root overrides on the audio worker; PCM16 mono/stereo, 8–96 kHz, at most four seconds
and 1,601,536 input bytes per file. Decoding/resampling stays on that worker. No game audio
assets or commercial samples were copied. README and its TXT companion list every file.

The existing dedicated SDL owner initializes the subsystem. Its audio worker retains the
single physical device and feeds a Cues stream from the new mixer; the original bounded
raw-PCM engine contract remains intact. No callback, filesystem, synthesis, HRTF or SDL
device work was moved onto Unity. OpenAL Soft 1.25.2 performs loopback DSP only, uses a
thread-local context, and never opens another hardware device or changes the process-wide
OpenAL context. Actual `ALC_HRTF_SOFT` is queried; failure retains software stereo/pitch.
DLL imports are exclusively Windows system libraries. Runtime, full source URLs, pinned
hashes and notices (including MIT KEMAR/Gardner/Martin attribution) are recorded in
`openal.lock.json` and `third_party/openal-soft/SOURCE.txt`. Both release packaging routes
and future deploy copy this private eight-file payload; Prism remains read-only/separate.
Scratch fixtures verified deployment first; the user subsequently authorized real
deployment to all six games. See the deployment receipt below.

Soundscape observation traverses the already-published pathfinder All view, independent
of the currently selected category. No database inclusion rules changed. Native pointers
remain on the native owner. Conversion retains bounded raw identifying strings (512
characters), positive door metadata and an actual GotoMap flag; none is spoken. Current
position/presence/opened state is copied in at most 512 entries or a 0.25 ms elapsed slice
(the final native operation can overrun the deadline). A registry revision or map change
retires an unfinished capture. Counts above 8,192 are invalid state and fail closed before
traversal; this is not a nearest-entity cutoff. Four reserved snapshot buffers are prepared,
cleared and recycled by the audio worker. Native cancellation returns ownership through a
fixed SPSC queue rather than destroying large collections on the game thread.

Capture refresh starts 200 ms after successful publication; unfinished captures expire
after one second. The worker rejects snapshots older than 1.5 seconds and clears expired
emitters. Context/master generations cancel outside full queues, including a cancellation
that arrives during a driver submission. Field exit, loading, menus/dialogue, focus loss
and map changes retire field audio; EXP remains independently owned by battle results.
The navigation master gates previews and all old/new effects without changing individual
preferences. Soundscape off does not disable independently selected wall/beacon effects.

The worker classifies finite copied coordinates and normalized world-wrap deltas. Door
pairing uses same-layer Chebyshev distance one (own tile and eight neighbors); the exit
owns the knock and standalone door events are silent. Native GotoMap with unavailable
destination/subtype keeps the generic exit cue. Explicit hazard identifiers take precedence
over exit/door names, so a trapdoor is a trap, not a knock. Ordinary warp, teleport and
battle strings alone never establish a trap. Missing semantics remain ordinary eligible
events; no whole-database re-audit was performed. Opened chests, remote floors and ToLayer
are silent, with no new animation/placeholder inclusion.

Every in-range emitter retains an independent clock. Same-kind order slots cycle through
1, +5%, -5%, +10%, -10% up to +/-30%; active HRTF removes this detuning as requested.
Fallback rear half-plane pitch is 0.7x, matching the existing beacon convention. Map north
is ahead, regardless of character facing. Radius falloff reaches 25% at the cap. Normal
cadence is at least one second, nearby traps 600 ms, with actual clip/pitch duration plus
a safe gap and small order-slot timing offsets. A near trap's gap is shorter even for long
replacement clips. HRTF retains wall pitch cues and never applies directional pitch.

Mix state is bounded to 128 active voices, with 112 available to entity clips; at most eight
new entity starts per 20 ms chunk. Pressure rotates/defer starts, never selects nearest-N
or silently removes an in-range emitter. A long custom clip can therefore delay crowded
scenes; four-second 112-voice saturation is a limit, not a claim of unlimited polyphony.
SDL buffering targets 60 ms (three 20 ms chunks); the worker wakes at most 20 ms apart
without game polling and never catches up with an unbounded render loop. Driver stalls
can extend actual delivery/cancellation; the game producer never waits for them.

## Offline evidence and remaining limits

The canonical build runs the production classifier/bank/runtime and real OpenAL Soft
loopback harness, the actual native capture adapter, menu/keyboard producer fixtures and
blocked-driver checks. It also retains all previous route/speech/input checks. Specific
cases cover per-file missing/invalid fallback, stereo downmix/rate conversion, malformed
headers/chunks, same-kind order, doors/generic exits/traps, trial-pad/battle exclusions,
world wrapping, range, opened/ToLayer silence, master independence, previews, snapshot age,
queue pressure, generation cancellation and nested-menu parent focus in all six games.
Four hundred crowded emitters all sound under deferred scheduling. The actual HRTF test
verifies left/right ear energy and volume above 50%, then renders 128 simultaneous loops.
The native fixture includes normal 2,000-row and synthetic worst-string 8,192-row captures.
Timing reports distinguish native aggregate/slices, fake SDL submission and actual HRTF
render cost. Final receipts and measured values are appended after verification.

These fakes preserve production consumers but simulate native engine reads, frame spacing
and the SDL device. They do not establish modded versus unmodded game-frame cost, actual
audible first-output latency, driver/hardware tails, or live metadata coverage. A slow game
with an extreme registry can hit the capture age limit and temporarily silence stale audio;
the safety bounds remain in force. No universal response-time target is claimed. Gameplay
confirmation remains pending user gameplay with the installed build.

## Delegation review

Luna (`gpt-6-luna`, medium) handled the bounded, specified Speech master ingress/menu calls,
12-locale UI text and modal fixture updates. Sol retained all audio architecture,
classification, native capture, implementation, integration and validation. Sol reviewed
the speech APIs and all menu paths; corrected the queued-speech test to actually enqueue
before muting, and requested localized repairs for misleading arrow aliases and one
Japanese string in Traditional Chinese. Luna corrected those entries and passed static
JSON/locale checks. Sol runs the complete canonical checks; no implementation is accepted
solely on the handoff's static checks. No Astra delegation. Gameplay remains pending.

## Speech master gate — October 15, 2026

The Speech section now starts with persistent `SpeechEnabled` (default on). The centralized
`say`, `say_parts` and `say_sequence` ingress returns before repeat-state updates or backend
requests when disabled. Explicit `say_menu` and `say_menu_sequence` calls bypass only this
setting; the F8 mod menu uses them for its row, help, navigation, entry, back and close
announcements. Turning game speech off uses the ordinary interrupting menu announcement,
so the existing output generation retires queued gameplay speech. Confirmation dialogs and
text input remain on ordinary gameplay speech and are muted with the master.

`modal_speech_check` now covers muted producers and unchanged repeat state, menu output while
muted, queued-tail cancellation on the off confirmation, re-enabling, and duplicate queued
text. `modal_input_check` checks the master is the first Speech row across all six games.
Fixture linkage was updated in `title_prompt_check`. These checks were added but not run by
the bounded implementation handoff; Sol owns review and canonical validation.

## Audio menu translations and modal fixture — October 15, 2026

The common text catalog now covers the Navigation audio master, HRTF, Soundscape switch and
settings page, shared cue labels, per-cue volume/preview labels, and range/help text in all
12 locales. Preview help says previews use the current cue volume even when that cue is off,
while the Navigation audio master still applies. `modal_input_check` now links the real
cue definitions and records preview dispatches through a fixture; it checks the four root
categories and Close Menu, audio row order, nested cue controls, distance bounds/steps and
parent focus after Back. JSON/locale checks and `git diff --check` passed. No build or test
binary was run; Sol owns the canonical validation.

## Final verification receipt

Canonical `build.bat` and separate `smoketest.bat` pass. All 6,736 native name checks
resolve; 122 used/319 defined action-query names have zero undefined wiring. The
isolated host resolves/forwards all 47 exports and completes boot/logger initialization.
Current local DLL: 9,448,960 bytes, SHA256
`4FE4FB6594E4082FD619B49D7EB8C17C79E9533032964062CE56B0B53E51DB58`.
The previous input/flee/teleport fixes remain included. No real deployment, game launch,
runtime probe, save change, commit/push or publication occurred.

Final offline run (machine/scenario-specific, simulated engine/device boundaries):

- 2,000 published rows: four native slices, peak 0.056 ms, aggregate 0.200 ms.
- 8,192 rows with 512-character hints: 16 slices, peak 0.231 ms, aggregate 3.016 ms.
  These copy measurements exclude actual frame spacing; add 3/15 inter-frame gaps
  respectively. A slice deadline bounds traversal, not an individual SDK call.
- Snapshot receipt through the real classifier/mixer to fake SDL: 0.020 ms with an
  immediately serviced queue. Production adds worker scheduling, any existing SDL
  buffer (target 60 ms), capture/frame spacing and hardware/backend latency.
- 400 dense entity voices: all 400 heard, 489 deferred starts, max software chunk
  0.333 ms. No nearest-N culling.
- Actual OpenAL HRTF: cold worker open 66.621 ms; 128 looping voices, mean/peak
  20 ms render 1.139/1.526 ms. Cold setup never runs on the game owner.
- Blocked fake SDL submission: full request pressure, master/context cancellation and
  the post-driver stale-buffer clear pass. Existing blocked speech/controller/audio
  ownership tests also pass. Fake-SDK speech: cold 0.745 ms, worst warm 0.119 ms;
  2,000 blocked-SDK publications 1.226 ms, latest output 1.374 ms.
- Eighteen actual DLL RCDATA WAVs match their files and deterministic regeneration;
  every filename is documented. Forty-three new/static/dynamic labels cover all 12
  locales; README/TXT agree. Eight OpenAL runtime/notice files pass pinned hashes,
  x64/system-only dependency checks, real scratch deployment and archive tests.

Receipt: `build_check/soundscape_2026_10_15.json`, with hashes and retained reports.
The existing dirty work was preserved. Luna's bounded speech/menu/translation work is
reviewed and accepted after Sol's corrections and full checks; no remaining handoff
issue. No gameplay/audio quality or comparative game-frame measurement is claimed.

## Deployment — October 15

The user explicitly requested deployment, lifting the earlier hold. Sol handled this
step without delegation. All six games were confirmed closed, the local source/data
was no newer than the checked DLL, and Prism prerequisites passed read-only checks.
Previous overwritten files were backed up and hash-verified in
`build_check/soundscape_recovery_2026_10_15` before running the existing `deploy.bat`.

All six installed copies match the checked 9,448,960-byte DLL hash
`4FE4FB6594E4082FD619B49D7EB8C17C79E9533032964062CE56B0B53E51DB58`, SDL3 and the
pinned eight-file OpenAL Soft runtime/notice payload. Prism, settings and waypoint
files retain their original hashes and timestamps. No game was launched or controlled;
no commit/push or public release occurred. Soundscape and HRTF remain default off.

Before/deploy receipts: `build_check/soundscape_before_2026_10_15.json` and
`build_check/soundscape_deploy_2026_10_15.json`. The user subsequently confirmed the entity soundscape and cadence work well. Legacy-effect
regressions and the follow-up corrections are recorded below; remaining checks are in TESTING.md.


## Legacy-effect and watercraft follow-up — October 15

User gameplay confirms the entity soundscape and cadence. They report muffled wall bumps,
missing/inaudible footsteps and concern about simultaneous pathfinding. The original
soundscape integration incorrectly approximated old cues with new Python sine envelopes:
the bump lost its filtered noise/quadratic attack and the footstep became a sine instead
of the original seeded noise click. Center panning also reduced dual-mono effects by 3 dB;
beacon gain, wall cycle alignment and landing/counter envelopes changed. These were real
implementation regressions, not a requested redesign. FF2's read-only log confirms the
user tested footsteps On before switching them Off; saved Off does not explain the report.
No saved preference was changed by the fix.

`legacy_cues` now reuses the untouched original `tone_generator` on the audio worker.
Embedded legacy WAVs are regenerated from that same compiled generator, with full-level
mono for HRTF and original panned/dual-mono stereo for fallback. The worker preserves
original frequency-specific durations/envelopes instead of time-stretching defaults.
Beacon gain retains its original 0.10–0.50 clamp (an explicit user volume of zero mutes).
Landing synthesis scale stays separate from the user gain, retaining float expression order.
The original wall-count headroom and cos/sin panning are preserved. Custom per-file WAVs
still work; soundscape category timbres/cadence/range/detuning are unchanged.
HRTF receives the original unpanned signals at their original source gains and retains
its short filter tail. Its directional filtering naturally differs from stereo playback.

Synthesis/file/DSP work remains on the existing audio worker; native callbacks enqueue
only copied scalars. Original effects have reserved voices 0–9 and previews 15; entity
voices remain 16–127. Additional dry PCM storage is bounded by those legacy slots (at
most 5,512 mono frames and twice that stereo per voice). Per-request synthesis is bounded
by these fixed lengths. The 64-request queue and three 20 ms output-chunk limits remain.
HRTF owns one extra reusable buffer per voice on the same worker, never another physical
device/context owner. No native callback waits for this processing.

Stale entity snapshots now retire only entity voices and clocks, preserving beacon,
footstep, wall and landing lifetimes. Route producers never consulted Soundscape;
actual-producer checks now exercise soundscape with directions, beacon-only and combined
beacon/directions. Ctrl+Apostrophe toggles the same persistent Soundscape master as F8
in both keyboard layouts, without changing navigation preferences. The existing
Navigation audio master continues to gate every mod cue.

Watercraft eligibility now reads current native TransportationInfo rather than the
boarding-event cache, including a loaded boarded save. Extracted transportation masters
verify ship rows in FF1/2/3/5, submarines where present, and canoe row 2 (type 5) in FF1–3.
FF2's snowcraft row 5 is excluded despite sharing type 5; aircraft and FF4/6 are excluded.
Landing probes check only the four adjacent cells on the native owner, using actual
transportation IDs. A shore must allow player walking, reject the ridden watercraft,
and satisfy that craft's explicit landing group when present. A non-sailable wall alone
is insufficient. Existing 100 ms checks, 250 ms directional ping rotation, transition/
focus/dialogue gates and default-off setting remain. Actual watercraft gameplay remains
for the user when progression permits; no game launch/control/probe was performed.

Offline validation includes 79 full PCM stereo comparisons against the original signal
formulas (footstep, bump, four walls, counter, twelve landing pan/frequency combinations,
sixty beacon pan/frequency/gain combinations), actual HRTF playback of original noise/
thud inputs, stale-snapshot and master independence, plus the real navigation producer
and both-layout dispatch checks. The asset check compares all 18 actual DLL resources
against deterministic regeneration. Final build/deployment evidence follows below.


### FF2 navigation no-op clarification

The user clarifies every pathfinder command seemed inactive with Soundscape On; they
used brackets/backslash and did not test switching Soundscape Off. Read-only FF2 INI
contains no NavigationKeymap, selecting the documented Letters default (0). That build
also introduced exclusive selectable layouts: brackets/backslash deliberately dispatch
nothing in Letters. This explains the observed controls without assuming a planner
initialization failure. The same session's soundscape and shifted range keys were active;
the soundscape adapter consumes an already-published pathfinder All view. Source tracing
finds no Soundscape gate in keyboard/controller dispatch, scanner readiness or routing.

Full input-tick regression reproduces backslash doing nothing with Letters and Soundscape
On, then dispatching Mod.NavigateToEntity after selecting Brackets (1), leaving Soundscape
On. The production scanner fixture repeatedly captures the soundscape and verifies both
browse/directions readiness, selection/category and pending work remain intact. These
checks complement actual route-producer and both-layout/controller dispatch checks.
Sol selected the user's used Brackets/backslash layout in FF2 at deployment, with an
INI recovery copy and an explicit expected settings delta. No planner-failure reproduction
or game confirmation is claimed; if the problem persists in the selected layout, retain
this as a separate gameplay issue rather than claiming the audio-lifetime fix settled it.


### Follow-up verification and deployment

Canonical build and every required check pass: 6,736 native names/zero missing,
122 used/321 defined action/query names/zero undefined, 18 embedded/generated WAVs,
43 cue/menu labels in 12 locales, README/TXT agreement and pinned eight-file HRTF payload.
All 79 actual stereo PCM comparisons pass, as do actual HRTF noise/thud output, short
filter-tail preservation, master/expiry independence, concurrent route producers,
continuous production scanner capture and full input-tick layout reproduction/correction.
Isolated smoke loads the DLL, resolves/forwards all 47 exports and completes boot/logger
without a game. No game or physical output device was used for these checks.

This run's existing workload evidence: actual 128-source HRTF mean/peak 1.110/2.087 ms
per 20 ms worker render; cold open 47.878 ms; copied snapshot to fake SDL 0.019 ms;
400 entities all heard, 489 deferred starts, peak worker chunk 0.282 ms. Actual scanner
2,000-row capture uses four slices, peak/aggregate 0.050/0.180 ms; 8,192-row fixture
uses 17 slices, peak/aggregate 0.289/3.307 ms. These are offline host measurements,
not comparative game-frame/physical audible timing or a new performance certification.

All six closed games now contain the checked 9,469,440-byte DLL:
`6499036C6CF512AD921CADB75D700CC2738CBECCB9A1FC31371CF065C0765A49`.
DLL, SDL3 and all eight HRTF runtime/notice files were hash-verified. Prism and waypoints
retain their bytes/timestamps. Every preference is unchanged except FF2's explicit
NavigationKeymap=1 selection, verified against the backed-up file with only that key
added. Footsteps remains saved Off in FF2; the user can enable it with apostrophe.

Recovery: `build_check/soundscape_fix_recovery_2026_10_15` (including FF2 INI).
Build receipt/reports: `build_check/soundscape_fix_2026_10_15.json` and
`build_check/soundscape_fix/reports`. Before/deploy receipts:
`soundscape_fix_{before,deploy}_2026_10_15.json` in build_check. Sol handled this follow-up
alone, preserving prior dirty work; no game launch/control/probe, commit/push or public
release. User confirmation of restored physical sound levels, corrected FF2 controls
and watercraft landing cases remains separate from offline verification.


## HRTF output level and contextual volumes — October 15

User confirms the corrected build is excellent, while HRTF wall bumps still require
roughly doubled volume. Requested one 2x HRTF master gain and separate remembered volume
banks through the existing controls. `Hrtf::open` now applies listener AL_GAIN=2 once,
after the individual source gains. Every mod effect and preview shares that loopback
renderer; no per-cue or soundscape-master multiplication is added. The prior AL_MAX_GAIN=4
source headroom remains. Only active HRTF passes through this stage: software stereo and
unavailable-HRTF fallback receive no output boost. Source PCM/timbres/envelopes, cue timing,
pitch behavior, distance falloff, native audio and speech are untouched.

`audio/volume_settings.h` is the native-owner volume context site shared by the existing
mod-menu Volume rows, original Preferences getters/setters, SoundPlayer's six legacy
volume APIs and publication of soundscape master/category gains. No new row, submenu,
slider, percentage range or independent toggle is added. With HRTF requested On, those
same controls read/write the HRTF bank; Off selects stereo. Existing unprefixed persisted
keys remain stereo, preserving installed levels. HRTF values have internal HRTF.<key>
storage in the same INI and independently default to 50 until edited. Defaults never
follow later edits to the other bank. Each bank persists through the existing owned,
asynchronous settings snapshot/I/O path. There are at most 15 additional volume values;
no periodic writes are introduced. Feature toggles, range, keymap and all other settings
stay shared. Menu speech still describes the currently selected slider normally.

Volume lookup/publication remains on the native owner, with finite small key strings
and scalar options copied to the audio worker. The worker never reads live settings;
OpenAL remains private worker-owned loopback into the existing SDL device. No thread,
queue or capture limit changes, new library, runtime probe or game launch is introduced.

Regression checks exercise every existing Volume row in all six menus: independent
50% HRTF defaults, edits in both directions, restoration, 0/100 bounds and no extra rows.
Range remains shared. Actual SoundPlayer publication selects the correct legacy and
entity gains plus soundscape master in each mode. The real loopback test compares unity
against production master gain for all 18 effects: expected 4x energy (2x amplitude),
with zero-volume mute retained. Previous 79 sample-exact stereo checks remain required.
Results and deployment are recorded below.


### Contextual-volume validation and deployment

Canonical build/all checks and isolated smoke pass: 6,736 native names with zero missing,
122 used/321 defined action/query names with zero undefined, 47 exported/proxied entry
points. All 18 actual HRTF effects fall within 3.9–4.1 times unity-master output energy,
confirming the requested single 2x amplitude stage. The zero-gain loopback output peaks
at one PCM16 unit (OpenAL output quantization dither), with no residual cue signal.
Every existing volume row in all six real menu fixtures passes independent mode/default/
edit/restoration/limit checks, and the real sound producer publishes each bank's master,
legacy and entity gains. Shared distance and unchanged menu-row checks pass. All 79
original stereo PCM comparisons, capture/ownership/cancellation and existing regressions
remain passing. Current actual HRTF 128-voice mean/peak per 20 ms render is 1.123/1.692 ms;
cold open 47.767 ms; dense 400-entity peak worker chunk 0.275 ms. These are host fixtures,
not physical audibility or comparative game-frame measurements.

The checked 9,472,000-byte DLL is deployed and hash-verified in all six closed games:
`C2042BDF2B58114F9F79F850C1B5FECD978F990AF5B24D9821A6AF77F00E9A27`.
SDL3 and all eight pinned HRTF runtime/notice files also match; Prism, every saved setting
and waypoints retain their prior bytes and timestamps. Recovery is
`build_check/hrtf_volume_recovery_2026_10_15`. Checked receipt/reports:
`build_check/hrtf_volume_2026_10_15.json` and `build_check/hrtf_volume/reports`.
Before/deploy receipts: `build_check/hrtf_volume_{before,deploy}_2026_10_15.json`.
Sol handled the entire update; no delegation, game launch/control/probe, source publication
or unrelated changes. Actual audibility and saved-profile restart confirmation remain
user-side in TESTING.md. Existing uncommitted work/history were preserved.


## Beacon targeting modes — October 15

User requests the existing route-leg beacon or a direct object-location beacon, with
Shift+F6 switching modes. BeaconMode is a saved enum (0 Follow route, default; 1 Object
location), also available under Navigation audio after Beacon Volume. F6 retains beacon
on/off. Both layouts and all six games share Shift+F6. Switching announces the mode and
resets beacon cadence/arrival state, without enabling an off beacon or changing soundscape,
spoken directions, HRTF/volume banks or the Autowalk preference. Existing manual-key input
safety still stops an active autowalk when Shift+F6 is pressed.

Route mode keeps the existing follow/follow_goal service, leg corners, cross-floor routing,
blocked-route lower pitch and route arrival. Object mode instead uses the current selected
entity/waypoint's local position, including live moving targets and nearest wrapped image
on world maps. It skips map accessor, availability, follow/follow_goal and quick-path calls;
no traversability claim is made, so it can point through walls with the ordinary beacon.
It retains existing proximity cadence, gain/pan, arrival radius and resume after moving away.
Current native-owner target/pose reads and published immutable submap dimensions remain
bounded; no new scene/database audit, route computation, blocking work or native-pointer
worker transfer is introduced. Existing context/navigation master gates, audio worker queue,
voice reservation, HRTF 2x stage and volume profiles remain unchanged. An already requested
spoken/autowalk/previous route job is not cancelled by this audio preference change.

Projected entities on another floor have meaningless local positions (field_api.h Target).
Object mode remains silent for them; an explicit restart/navigation request explains the
current-floor requirement through central say. It does not repeat this message from the
periodic audio tick; optional destination announcement cannot overwrite that explanation.
Route mode continues routing across floors. Finite x/y checks reject invalid owned target/
player observations before direct spatial math. Remote route goals use their baked cells.

Production beacon-loop/action fixture covers route corner versus object aim, blocked routes,
zero planner/accessor calls in direct mode, current target updates, world wrapping, waypoints,
arrival/resume, mode announcement/persistence, off-state changes, remote explanation and
all six focus/field/dialogue/master suppression gates. Actual all-six menu fixtures check one
mode row, default/adjustment and shared HRTF context; keyboard fixture checks Shift+F6 in both
layouts while retaining F6 and transition guards. Required build checks now run beacon_check.
Existing routing, simultaneous soundscape/directions, HRTF/output/legacy PCM checks remain
required. Physical gameplay/audible response is separate; no game launch/probe is authorized
or performed for this task, and no new frame-performance certification is claimed.

Luna (gpt-6-luna, medium) handled only six menu/help catalog entries in 12 languages. Sol
reviewed all additions and compared against MODTEXT_COMMON in the previously checked
C2042BDF DLL: every prior catalog pair is preserved exactly, with exactly six unique new
keys and complete locale sets. Two unrelated pre-existing duplicate catalog keys remain
untouched; the subagent's global duplicate-validation claim was narrowed by Sol's audit.
The bounded localization handoff is accepted with no remaining issue. Review receipt:
build_check/beacon_text_review_2026_10_15.json. Sol owns implementation/integration and all
checks. Build/deployment results follow below.


### Beacon-mode verification and deployment

Canonical build/all checks and isolated smoke pass: 6,736 native names/zero missing,
122 used/323 defined action/query names/zero undefined, 47 exported/proxied entry points.
New production beacon-loop/action fixture passes route/object, blocked-route lower ping,
zero planner/accessor calls in Object mode, moving target, cross-floor explanation,
waypoint, wrapped bearing, arrival/resume and suppression tests. All-six menu/default/
adjustment and both-layout Shift+F6/F6/transition dispatch checks pass. The full dependency/
resource checks pass: 18 unchanged embedded/generated WAVs, 49 labels in 12 locales,
README/TXT consistency and the pinned eight-file HRTF payload. Previous HRTF 2x/all-18
output, independent volume banks, 79 sample-exact stereo cases and concurrent navigation/
soundscape checks remain passing. No physical/gameplay confirmation is claimed.

All six closed games contain the checked 9,482,240-byte DLL:
`816E5DB3F6B1816DA2E74A75653ECCC5298C33DE418A8B3E0F0DDF7D7774499C`.
Installed DLL, SDL3 and all eight HRTF runtime/notice files were hash-verified. Prism,
settings and waypoints retain their prior bytes and timestamps. Follow route remains
the unset default; deployment does not enable beacons or alter user preferences.
Recovery: `build_check/beacon_mode_recovery_2026_10_15`. Checked receipt/reports:
`build_check/beacon_mode_2026_10_15.json` and `build_check/beacon_mode/reports`.
Before/deploy receipts: `build_check/beacon_mode_{before,deploy}_2026_10_15.json`.
No game launch/control/probe, tester-file access, commit/push or public release occurred;
prior dirty work/history and third-party attribution remain preserved.
