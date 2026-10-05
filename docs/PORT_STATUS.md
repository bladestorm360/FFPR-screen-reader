# Port status

What each area of the native mod ports, and what the port found. Nothing here is tested in a
game yet. "Parity finding" = the C# mod behaved this way; the port keeps it, nothing fixed.

## Phase 0 (data)

- `data/common/mod_text.json`: 850 keys. Overrides: FF1 0, FF2 49, FF3 42, FF4 26, FF5 39.
  No missing languages. Details: `MODTEXT_MERGE.md`.
- `FEATURES.md` (15 cross-game differences to decide), `DEDUP_INVENTORY.md` (286 checks,
  11 likely bugs noted).
- For the user:
  - keys naming a one-game thing (FF5 "Haste", FF3 "Onion Knight {0}") are in the common file;
  - overrides are not checked against the games' own text (FF5 French "Slow" = "Somni"?);
  - proposals to turn text checks into index checks change behaviour and need an OK.

## Core: mod UI (`src/core/ui`)

ModMenu, TextInputWindow, ConfirmationDialog, NavigationBuffer, PreferencesManager.
- Item names are translated when spoken (FF1 translated them once at start).
- FF2 has no Battle Results entry (no EXP).
- Not added: FF5's extra menu items (Landing Pings, Enemy Letters).
- Settings import maps FF2–FF4's `AutoDetailOnFocus` to `AutoDetail`.

## Core: audio (`src/core/audio`)

AudioEngine, ToneGenerator (all nine tones match FF1 sample for sample), SoundPlayer,
SoundConstants, MovementSoundPatches. Hook: `FieldPlayerKeyController.OnTouchPadCallback`.
- Landing ping has no FF1 source: built from FF5's constants (660 Hz, 80 ms in 250 ms).

## FF1 module (`src/games/ff1`)

NewGamePatches + NewGameHelpers (8 hooks), JobSelectionPatches (1), PuzzlePatches +
PuzzleGameState (7). Phase 1 proof hook: NewGameWindowController.
- Parity findings:
  1. The start popup always says "Start game?" (the C# looked for a field that does not exist).
  2. "Select name" never added ". Current: name" (the C# lookup always returned null).
  3. `NameContentListController.SetFocus` has no direct callers in FF1; its hook may never fire.
  4. Job names are matched against an English list: class selection is likely silent in other
     languages.
- Shared engine classes masked to FF1 for now (candidates for the core): the KeyInput
  Character/Name/Job content list controllers, `Last.Data.NewGameSelectData`;
  `master_get()` (master-data row without `MasterManager.GetData<T>`).
