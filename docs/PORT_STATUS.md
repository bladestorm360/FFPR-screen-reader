# Port status

Areas marked FINISHED are complete in the repo. Still running: dialogue/system, status/
config/bestiary, battle (FF2–FF5 game modules started after the field area).

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

## Core: mod UI (`src/core/ui`) — FINISHED

ModMenu, TextInputWindow, ConfirmationDialog, NavigationBuffer, PreferencesManager.
- Item names are translated when spoken (FF1 translated them once at start).
- FF2 has no Battle Results entry (no EXP).
- Not added: FF5's extra menu items (Landing Pings, Enemy Letters).
- Settings import maps FF2–FF4's `AutoDetailOnFocus` to `AutoDetail`.

## Core: audio (`src/core/audio`) — FINISHED

AudioEngine, ToneGenerator (all nine tones match FF1 sample for sample), SoundPlayer,
SoundConstants, MovementSoundPatches. Hook: `FieldPlayerKeyController.OnTouchPadCallback`.
- Landing ping has no FF1 source: built from FF5's constants (660 Hz, 80 ms in 250 ms).

## FF1 module (`src/games/ff1`) — FINISHED

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

## Core: shops, gallery, music player (`src/core/shop`, `src/core/extras`) — FINISHED

ShopPatches, ShopMagicPatches, ShopMenuTracker (+ ShopDetailsAnnouncer), GalleryPatches,
GalleryReader, MusicPlayerPatches, MusicPlayerReader. All names exist in all five games
(29 classes, 31 fields, 22 methods); 14 hooks, none folded.
- FF4: `ExtraSoundListContentInfo.playTime` is a float there (read as float, fraction dropped).
- `shop_data.cpp` reads master data from MasterManager's own tables (core helper candidate,
  like FF1's `master_get()`).

## Core: waypoints, audio loops, map transitions (`src/core/fieldnav`) — FINISHED

WaypointController/Manager/Navigator/Entity, WaypointHandler, AudioLoopManager,
MapTransitionPatches, VehicleLandingPatches, MoveStateHelper, NavigationTargetTracker,
LocationMessageTracker; teleport and beacon restarts from the main mod class.
- Waypoints stay in `<game>\UserData\waypoints.json`, same format as the C# mod.
- Hooks: SubSceneManagerMainGame.ChangeState, MapUIManager.SwitchLandable, FieldPlayer.GetOn
  (2 args in FF1/FF2/FF4, 3 in FF3/FF5: two masked refs), FieldPlayer.GetOff. None folded.
- FF1 never played a landing sound (only "Can land"), so no landing ping is played.
- Vehicle names follow FF1's table in every game: FF3/FF5 Boko and Magical Armor are not
  announced yet (game modules).

## Core: input, gamepad, controller router (`src/core/input`) — FINISHED

InputManager, KeyBinding(+Registry), ControllerRouter, GamepadManager, ControllerLabels,
GlobalHotkeyHandler, GameToggleAnnouncer, InputHelper, InputPassthroughPatches,
WindowsFocusHelper; the main mod's toggles, gil and character status. Tables:
`INPUT_BINDINGS.md`. Game modules add keys with `input::add_binding`.
- Not ported: FieldInputHandler (FF1 never called it), FeedLeftStickToGame (unused).
- Keys are read only while the game window has focus.
- `Input.ResetInputAxes` resolved as an internal call (FF3's dump lacks the managed method).
- Hooks: InputSystemManager GetKeyDown/GetKey/GetKeyUp/GetAnyKey,
  CheatSettingsClient.SetIsEnableEncount, ConfigClient.SetIsAutoDash,
  BattleCommandSelectController.SetCommandData (current actor for H in battle). None folded.

## Core: item, equipment and magic menus (`src/core/menus`) — FINISHED

ItemMenuPatches, EquipMenuPatches, MagicMenuPatches, MagicMenuState, MagicTargetPatches,
Item/ItemDescription/MagicDetails/UsableBy announcers, CharacterSelectionReader.
43 classes, 42 fields, 68 methods checked in every game they apply to.
- Item, equipment, I and U, character selection: all five games. The magic spell list and
  spell targets: FF1 only (each game has its own Serial.FFn magic classes) - game modules.
- FF1's magic SetNextState shares its body with 8 other methods: the port class-checks
  (FF1's Harmony patch reset the magic state on all of them).
  `ItemUseController.SelectContent` shares its body with SetCursor in all five games (as FF1).
- The repeated-spell text check ("Magic.List") now also resets on menu open/close.
- Master data (weapons, armour, jobs, items) is read from MasterManager's own tables:
  three areas now have their own reader (FF1 module, shop, menus) - merge into one core
  helper.
- Two `condition_name` copies (menus, battle) - keep one.

## Core: field entities and pathfinder (`src/core/field`) — FINISHED

EntityScanner + all 13 detectors, EntityDetectionHelpers, NavigableEntity, FieldEntityState,
FilterContext, Filters, MapNameResolver, FieldNavigationHelper, EntityNavigationManager,
CategoryManager, EntityTranslator, GameObjectCache; the main mod's entity and map actions.
38 classes, 31 fields, 25 methods, all in all five games (`kAll`).
- FF3's HiddenPassageRouting runs when `has_hidden_passages`; FF2's vehicle-pad merge when
  `has_vehicle_pads`. Vehicle names: each game keeps its old mod's table (FF4/FF5 try the
  official message name first).
- Fires `events::Kind::SceneLoaded` from `SceneManager.Internal_SceneLoaded` (postfix).
  One rescan path: fieldnav schedules `field::request_rescan()` 0.5 s after it.
- Differences from FF1, for the user to judge:
  1. Detectors see the real class name (the C# mostly saw "FieldEntity"), so
     `SavePointEventEntity` objects now count as save points by type.
  2. Chest contents were never spoken in FF1 (lookup missed); still "Treasure Chest".
  3. The vehicle-type map drops entries for objects no longer on the map.
  4. Step counts always use "." as the decimal point.
- A fourth master-data reader (`masterList`) - merge with the others.
