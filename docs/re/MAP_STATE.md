# Persistent navigation state, October 15

The user's FFII log shows opened native chests on Deist B1 losing their state prefix
when browsed from B2. B2's native chests then say Opened while the remote B1 chest
says only Treasure Chest. That is an unknown-state result, not evidence of the game
forgetting an opened chest. The preserved log is
`build_check/chest_state_2026_10_15/ff2_latest.log`.

## Native storage and the faulty boundary

All six matching dumps declare `Last.Interpreter.DataStorage`: persistent scenario
and treasure `Flags` references at 0x10/0x18, global int array at 0x20. Its nested
`Flags` class holds `uint[] bits` at 0x10 and logical `length` at 0x18. The shared
singleton survives floor changes. Recovered `ClearMapLocal` clears the map-local
array and selections; `ClearAreaLocal` clears area/map locals and selections.
Neither clears the scenario/treasure banks. The dump also declares separate
scenario/treasure arrays in DataStorage.Records for save state.

The constructor allocates `ceil(bitLength / 8)` **uint elements**. For 1024 flags
that is 128 words, even though `Flags.Get` indexes `bits[index >> 5]` and extracts
`index & 31`. Therefore only the first 32 words contain the 1024 logical flags.
The mod incorrectly required the array length to equal 32 and silently discarded
the real 128-word banks. Both treasure and story flags were affected. The current
floor's native `FieldTresureBox.isOpen` reader bypasses this path, which explains
the local/remote discrepancy. Observing the current entity group likewise does
not prove that other-floor story flags were captured successfully.

`map_state.cpp::copy_flags` now accepts complete backing arrays of 32 through 128
words with exactly 1024 logical bits, and still reads only 32 words. Missing,
short, oversized or wrong-logical-length banks remain unknown. No native writes,
new polling, native calls on workers, extra copy volume or game-thread waits were
added. Remote projection takes a fresh owned snapshot on map/group rebuild;
native chest entities need not remain loaded for this state to be available.

## Matching evidence and checks

Sol read the installed PE files as data; no game was launched or probed.
`tools/re/mapstate_storage.py` exports ten bounded named functions per game:
DataStorage constructor/getter/GetFlag/local clearing, and Flags constructors/Get/
Import/Initialize. It asserts the two 128-word allocations, 1024 logical lengths
and uint/32-bit indexing in every game. Full instructions, native RVAs, matching
DLL/dump SHA256 hashes and manifests are retained in the ignored investigation
directory's `ffN_storage_native.txt` and `native_provenance.json`.

`tools/mapstate_native_check.cpp` includes the actual production native reader;
only reflection/getter/native memory boundaries are fake. The 128-word native
layout reproduces eight failures before the correction, including zero resolved
chests in every game. The corrected reader preserves all 1024 scenario/treasure
bits, signed globals, boundary bits, minimal backing arrays, unavailable/malformed
state and immutable copies after native storage changes. Its worker consumes
only owned flags and real baked data, with zero native API calls.

| Game | Real chest mappings through native snapshot | Unmapped, still unknown |
| --- | ---: | ---: |
| FF1 | 248 | 1 |
| FF2 | 235 | 0 |
| FF3 | 414 | 1 |
| FF4 | 395 | 1 |
| FF5 | 275 | 4 |
| FF6 | 319 | 1 |

Both unopened and opened results are checked for all 1,886 mapped baked chests.
The eight unmapped chest objects retain their conservative unknown state; the
fix does not fabricate mappings or reveal contents. The canonical build now
runs this regression in addition to the existing initializer, cross-floor
projection, scanner, formatter and route checks. Earlier synthetic-Flags tests
proved those consumers, but missed this native allocation boundary.

The corrected fake-native capture measured 0.262 ms aggregate / 0.004 ms peak for
100 snapshots. These observations exclude real native reflection/cache cost,
game scheduling, whole remote projection and audible response. No runtime
performance claim follows from them. Full gameplay confirmation remains user-side:
browse an already-opened Deist chest from another floor under Current map, then
repeat the tracked Conversation route with real story flags available.

Canonical build and isolated smoke pass: 6,736 name checks, zero missing, zero
undefined action/query references, all 47 exports. All six closed games received
and hash-verified DLL SHA256
`A29CADC0C78AB6D51500876F57A1A7894167482B8E397C9528E55F7BA85D8A0D`,
9,529,344 bytes. Settings, Prism and waypoints retain their previous hashes and
timestamps; SDL3 and the eight pinned HRTF payload files match source. Recovery
and before/deploy receipts are `build_check/chest_state_recovery_2026_10_15` and
`build_check/chest_state_{before,deploy}_2026_10_15.json`. Build/smoke/deploy logs,
the failed-before regression and final reports remain in the investigation directory.
Sol implemented, reviewed and verified this repair alone. No game launch/control/
probe, dependency installation, new commit/push or publication was performed.
