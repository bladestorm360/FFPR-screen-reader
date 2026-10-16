# Character creation and exact exit points, October 16

The user withdrew the pathfinder-initialization report after identifying the default
navigation layout. The own FFI log had already confirmed 129 maps loaded and 47 live
entities scanned; no initialization fallback or input-map change was made. Snapshot:
`build_check/navigation_baseline_2026_10_16/ff1_reported.log`, Oct16 12:59:50 PID34812.

The user's new persistent rule is to audit every reported bug across all six games,
fixing each applicable occurrence. The temporary release hold was explicitly lifted:
finish these two tasks, commit/push, and overwrite the existing V0.9.3 release. Other
bugs are deferred; gameplay confirmation is not claimed by offline checks.

## Creation class field

FFI's working class-choice list is separate from its creation summary. The summary
tried a CharacterStatus list and a hand-written master-dictionary/Job reconstruction;
both could return empty while the rendered class text was already present. No reader
observed that existing text, so it announced Unknown. The actual all-six SetData and
SetJobName bodies assign CommandList[1].text. The command Data.Id is Job=1.

The reader now uses that visible current-slot field, validating the four-slot/two-command
layout and Job identity. It neither scans masters/hierarchies nor invents an English
fallback, uses another character's slot, or retains a UI pointer on a worker. Existing
one-frame native readiness, slot/field event guards, names, Done, class-choice list,
Autodetail and central speech controls remain. Main-owner capture is bounded to the
selected slot and text; speech cleanup/output stays on the existing worker.

All-six audit: shared field layouts/native setters match in I–VI. Only I's active
creation window has InitJobSelect. II's and III's new-game adapters read names without
an editable class field; IV–VI use native naming flows without this class-selection
summary. No duplicate broken master-name chain exists in those adapters, so no
unnecessary reader was installed there. Job/status menus elsewhere are outside this
specific creation-field defect. `tools/re/creation_fields.py` preserves matched dump/PE
hashes, signatures, fields and native producer body hashes in
[creation_fields_native.json](creation_fields_native.json); bounded disassemblies/schema
records remain in the ignored investigation folder. No game launch or runtime probe.

The production-reader fixture exercises all four slots, six classes, German/Japanese
rendered names, changed class, wrong command identity, hidden/malformed layouts,
neighbor rejection, names/Done and no master/native method invocation. One fixture-only
link failure was corrected by supplying its existing speech-sequence boundary stub.

## Exact transition goals

The old shared router deliberately merged nearby GotoMap siblings with matching
floor/arrival IDs and every cell of their trigger footprints. Its route goal could
therefore differ from the browsed position. Explicit routes, browse routes and exit
eligibility now use the selected x/y only; world cells retain actual native terrain/
transport admissibility and indoor goals retain their valid native layer alternatives.
Closed decorative town walls do not become goals. Interaction facing and intermediate
cross-floor transition selection retain their existing semantics.

Exit grouping still chooses a reachable representative when enabled, but that actual
representative's displayed point is also the explicit route goal. With grouping off,
each entrance stays independent. Unreachable/invalid selected points no longer borrow
a sibling's valid entrance. No engine map write, live native worker access or native
route fallback was introduced; all computation uses the owned baked worker pipeline.

Cornelia's six actual world triggers are x144/146, y154–156, destination map4. From
the central corridor x145, each west trigger must say 1 West and each east trigger
1 East. The first regression incorrectly treated the central corridor (145,156) as
an entrance; the real bake proved it is not one. The corrected regression uses all
six actual triggers, rather than relaxing the exact-endpoint requirement.

All-six checks cover 8,710 eligible selected transition cells across sibling arcs and
multi-cell footprints. The actual route worker also exercises 571 real world exit
objectives (I44, II149, III142, IV74, V76, VI86), plus the six Cornelia cases and existing
cross-floor, live-state, vehicle, trap and cancellation regressions. Final build,
package, deployment and publication evidence follows in the release record.
