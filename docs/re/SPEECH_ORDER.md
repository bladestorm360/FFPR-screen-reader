# Speech ordering and Prism priority audit, October 13

User report: category names are cut off by the initially focused destination. Required
order is category, then destination queued behind it. Audit the same priorities across
the shared mod, retaining intentional manual interruption and game-event ordering.

## Backend and central queue evidence

- Pinned Prism 0.18.3, source `94329ebeccbaee7f70b3494a290c9dfaac0184ee`;
  [NVDA source](https://github.com/ethindp/prism/blob/94329ebeccbaee7f70b3494a290c9dfaac0184ee/source/backends/nvda.cpp#L178).
  Read-only exact source fetch is `build_check/prism_nvda_pinned.cpp`, SHA-256
  `C7138B9813D2443E7EB84A7484CB269C8EC7181C09DEEB5B3218E9C980E145F9`.
  `speak(text,interrupt)` calls `nvdaController_cancelSpeech` only for true, then
  `nvdaController_speakText`. `output` invokes that speech path then braille. Stop
  explicitly calls cancelSpeech. False does not cancel queued NVDA speech.
- `prism_backend.h::Backend::output` passes that Boolean unchanged for combined
  output and separate speech. It does not assign additional NVDA priority or poll
  speaking completion. Only the exclusive speech worker calls the backend.
- `speech.cpp::enqueue_owned` assigns an interrupt/silence generation;
  `dequeue` retires older ingress/chunks. `speech_queue.h::Queue::say` clears pending
  lines on true and gives only the first chunk true; later chunks are queued false.
  Therefore a same-event follow-up using true can erase its own earlier headline.
  Submission is asynchronous and does not mean a line was heard to completion.

## Producer investigation and corrections

The preceding entity-category reader combines category and destination in one short
string; no separate interrupting initial destination exists on that direct path.
The available FF1 session also logs one combined category line. This does not fully
reproduce the user's report. Pending cycle callbacks, however, did not carry category
event identity, so an older cycle waiting for filter/flood readiness could run after a
new category and issue an interrupting entity announcement. Category callbacks used
only the enum, which also failed to distinguish a later return to the same category.
Keyboard registry dispatch matches exact modifiers and executes one binding; entity
marking only changes mod state. Beacon restart announcements are explicit route-key
actions, not automatic category-selection speech. Controller navigation silence runs
before its requested action and should not be moved behind the category response.

The queue-pressure defect is independently reproducible. The old expression
`message.interrupt || g_carry_interrupt.exchange(false)` skips the exchange when the
message already interrupts. The carry flag remains set, so the next queued follow-up
interrupts, discarding its heading. The regression reproduces that exact old expression
with the production queue, then checks the corrected ingress. Consume the flag before
the OR; retain it only if there was no usable text, and clear it on explicit silence.

Corrections in this build:

- Category replies carry an increasing category-event generation, including return to
  the same enum. Old browse/category/route callbacks cannot replace the current category.
  Same-category browse actions remain FIFO. New category/browse input cancels obsolete
  pending route requests unless an active autowalk owns them.
- `say_sequence` publishes category then initial destination as one owned event. Only
  the first nonempty part interrupts; tails share its generation and queue false. This
  also prevents pressure between separately published heading/focus messages. A later
  manual input still supersedes obsolete output. Identical eligible text is preserved.
- F8 menu entry/back/submenu publishes heading then first option immediately; the old
  150 ms initial-item timer is removed. Text entry and confirmation similarly publish
  ready prompts immediately, removing 300/100 ms timers that could fire in a new context.
  Confirmed/cancelled/Yes/No interrupts once; a known mod-owned completion callback's
  result or chained question queues behind it through owner-only `QueuedSpeechScope`.
- FF1–FF3 native name entry retains ready nested focus strings until the initializer
  returns, then publishes its ready heading before those strings. Initial focus arriving
  in the existing entry-frame window queues false; later explicit navigation retains
  its existing policy. FF2 receives the corresponding one-frame entry distinction.
  Existing local event/text guards remain; none are added to suppress repeated content.

`QueuedSpeechScope` is confined to existing mod waypoint prompt/CRUD callbacks. These
own mod strings/IDs/pose snapshots and schedule persistence separately. It must never
wrap native game operations, asynchronous work or workers. Repeat state and native UI
readiness stay on their existing owners. `InitialSpeech` holds only owned strings for
one native name-entry event; no Unity pointer reaches speech/compute workers. The
native initializer still runs synchronously on its original owner without waiting for
mod completion. No new native operation, hook or game information source is introduced.

## Audit coverage and retained priorities

October 13 title/boarded-save follow-up preserves one interrupting native prompt event
and later menu focus supersession. Ready text is published by the actual native title
phase producer; no queued timer can announce it after menu entry. The actual-source
title ingress/worker/Prism-adapter regression covers both same-frame cancellation and
a busy worker retaining older prompt ingress. The explicit V/controller vehicle query
uses ready native transport data on its owner before the existing interrupting say;
it does not create an automatic boarding announcement. Details and remaining runtime
confirmation are in [title evidence](TITLE_PROMPT.md#current-timing-and-hook-installation-follow-up-october-13)
and [threading](THREADING.md#hook-allocation-and-boarded-save-follow-up-october-13).

[Call-site inventory](speech_calls.tsv), generated by
`tools/re/speech_callsite_inventory.py`: current follow-up has 351 calls across 98 C++ source files, covering
core and all six game adapters. Syntax classes: 217 explicit interrupt, 86 default
interrupt, 34 queued and 14 context expressions. Header helpers and local wrappers
were reviewed separately. This is a source inventory, not runtime event tracing.

| Producer | Reviewed ordering / retained behavior |
| --- | --- |
| Entity categories / mod menu / owned prompts | Heading first, initial focus/choices/result queued; explicit subsequent input interrupts. |
| Battle dialogue / actor-action / hits / damage / status | Existing message/actor event guards and queued same-action chains retained. Defeat scroll/manual review may interrupt; ordinary continuation queues. |
| Results / review buffers / Autodetail | Native current-phase heading owns entry; same-page details queue. Later explicit review/details interrupts. No future reward disclosure or text deduplication. |
| Bestiary / equipment / jobs | Summary then initial entry, or character then slot, already queue correctly. Automatic job/detail output remains queued; manual details interrupt. |
| Music / frontend / gallery / configuration | Original/Arrangement heading then song queues, with existing native focus-swap guard. Other explicit focus/context actions retain interruption. |
| Puzzles / navigation altitude | Manual puzzle actions retain interruption; continuations queue. FF6 altitude retains the recent-landing queue guard. |
| FF1–FF3 naming | Owner-only initial-focus collection and entry-frame queue guard; existing local event guards and later cursor behavior retained. |

The context-expression inventory was inspected against these producer contracts, rather
than turning all speech into queued output. Backend serialization alone cannot prevent
a producer from starting a new interrupt generation at the wrong time.

## Native evidence and gaps

`tools/re/speech_priority_calls.py` reads matching dumps and GameAssembly PEs without
loading them. Local commercial disassembly and binary SHA-256 provenance remain in
ignored `build_check/re/ff1_speech_entry.txt` through `ff3_speech_entry.txt`, outside
public source exports. InitNameSelect RVAs: FF1 `4C4E80`, FF2 `3C4590`, FF3 `5029E0`;
NameContentListController.SetFocus: FF1 `3F32E0`, FF2 `2F17F0`, FF3 `32EB20`.
Existing signatures are `InitNameSelect(self, MethodInfo*)` and
`SetFocus(self, int32_t index, MethodInfo*)`.

The initializers update native active/name cursor state and displayed name strings.
SetEnbaleCursor tail-calls NameContentView.SetFocus; this does **not** prove a direct
nested NameContentListController.SetFocus call. The helper covers such an observed
callback if it occurs, and the entry-frame policy covers later initial focus. Runtime
callback order and actual spoken completion remain unconfirmed. No capture/retry or
new game launch permission was inferred; static source and the offline harness settle
the central ordering/cancellation defects without a runtime probe.

## Pipeline performance and bounds

The speech owner cleans/chunks owned messages and performs all Prism RPC, text logging
and diagnostic formatting. The native producer never waits for it. Ingress and chunk
queues each have 1,024 slots; overflow retires oldest pending output, preserving an
interrupt handoff without deduplication. Explicit input/silence generations retire
obsolete chunks before another SDK call. An RPC already in progress cannot be withdrawn;
only the next submission can apply the newer interrupt. Long blocked backend/audio
delivery therefore remains an external tail-delay limit, not solved by worker placement.

`SpeechTiming` logs first submission for browse/category/directions/mod-menu: request
to SDK submission, enqueue to submission, relevant snapshot age, SDK duration and return
code. QPC is initialized before hooks. Unknown age/time is `-1`, not claimed fresh.
Field age starts at the last published filter row capture; immutable label origins may
be older and a selected moving pose is still read live. Directions use the earliest
request observation conservatively; mod-menu text is ready at entry. Submission and
SDK success do **not** establish audible completion. Durations overlap and must not
be added as independent stages.

Comparable old/new evidence is the exact short-circuit carry expression regression
and actual-source out-of-order pending category callback replay. The prior ready mod
menu/text/confirmation imposed 150/300/100 ms timers; corrected ready output imposes
none. No old/new target-game audible benchmark was run. The previous navigation fix's
observed ~875 ms FF1 world scans and all-six world compute/scanner checks are recorded
in [threading](THREADING.md#navigation-latency-follow-up-october-13).

Final actual-source fake-SDK run: cold first submission 1.186 ms; worst of 50 warm requests
0.110 ms; 2,000 publications while the SDK was deliberately blocked 2.265 ms, latest
heading submitted 2.525 ms after that request batch began. This covers cold/warm/rapid
and queue pressure with real ingress/cleanup/chunking/dispatch/adapter code, but a fake
external SDK and ready owned text. It excludes native capture, real NVDA scheduling,
synthesizer delivery and target-game callback/aggregate frame/loading cost. These are
observations on this host, not universal latency guarantees. Real-game measurements
remain user-side; the logs now distinguish mod preparation/queue delay from SDK delay.

## Offline validation and delegation

- `speech_worker_check`: actual ingress, sequence generation/duplicates/blank parts,
  before/after carry-pressure behavior, first-output timestamps, later-input retirement,
  silence/braille/chunking/saturation and 8,000 MPMC transfers.
- `navigation_speech_check`: actual navigation producer with delayed readiness; a new
  category callback completes before an old browse callback, enum wrap is rejected,
  same-category repeated inputs survive, empty categories work and stale route replies
  are cancelled/rejected.
- `modal_input_check`: actual F8 menu/input source; immediate heading/first item and
  submenu sequences, controller/keyboard focus and native handback.
- `modal_speech_check`: actual say/speech/owned prompts and dispatcher through a fake
  Prism C API on a worker. Chained confirmations, text callbacks, duplicate native
  initial focus, zero stale timers, repeat state, cold/warm/blocked backend and pressure.
- `prism_check`: actual adapter preserves true/false across combined/separate output,
  plus UTF-8, capabilities, lifetime/error/stop and repeated-event checks.
- Canonical checks include existing results/battle/worker/filter/scanner/route/controller
  regressions and all-six native-name/wiring validation. One intermediate run failed
  the unchanged controller-worker check; the overwritten report was not retained and
  its cause is inconclusive. Subsequent full build and explicit normal/failure-backend
  rechecks both passed (local `build_check/controller_speech_recheck.txt`).

Luna (`gpt-6-luna`, medium, fresh bounded task) changed only `ui/mod_menu.cpp` following
the specified owned-sequence pattern. Sol reviewed the diff, call paths, ready-state and
later-focus behavior, then verified it with actual-source modal checks and the canonical
build. Sol added timing instrumentation and owns discovery, central fix, integration
and all verification. No remaining implementation issue from the handoff; real F8
speech confirmation remains user-side. No Astra was used.

Final canonical build passes: 7,061 native names, zero missing; wiring 120 used/323
defined/zero undefined. All checks above pass. All-six world benchmark covers 24 world
records and 16 representative far routes (eight records have no foot-start): worst
flood 7.899 ms, route 16.921 ms, zero failures, offline. Intermediate runs varied with
host load; these are compute observations, not target-game input or audible timings.
Isolated DLL smoke passes all 47 proxy exports/forwarding and boot logging. Deployment
completed in all six games; installed DLL hashes match the built DLL:
`AF4C94F08AF9D8F30C8E534279B2D225BC902746269DC721485709DFC7498DB7`.
Prism was checked read-only without changing its pinned dependency. No game was launched
or probed. [Accumulated user confirmation](../TESTING.md#speech-and-navigation-confirmation-october-13).

Subsequent October 13 user report confirms lag-free pathfinding, smooth battle
transitions and working FF1 results-buffer reading. FF2 is being tested; FF3–FF6
results are expected through shared code but await direct confirmation. The report
does not separately confirm category/F8/prompt/naming speech order or provide numeric
audible/frame timings. [Current coverage](../TESTING.md#gameplay-confirmations-october-13).

The later ship report limits responsiveness confirmation to tested land routes;
[boarded-vehicle corrections](THREADING.md#boarded-vehicles-and-wrapped-worlds-october-13)
await retest. I/controller West now reads the complete current-phase delta brief,
interrupting once as an explicit input, independent of cursor position. FF2's results
icon producer was corrected separately; [results evidence](RESULTS.md#weapon-labels-and-phase-wide-changes-october-13).
