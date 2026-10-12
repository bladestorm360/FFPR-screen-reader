# Port status

October 12 results follow-up supersedes all historical L-log references below:
all six games use native current-phase buffers, concise displayed totals and
Autodetail-controlled details, with prefix input ownership. [Evidence](re/RESULTS.md).

Current shared audit: [AUDIT_2026_10.md](AUDIT_2026_10.md). Its renderer-backed readers,
native results phases, map-state projection, thread fixes and controls supersede historical
parity findings below. Implemented/offline-verified behavior still requires user gameplay testing.

Areas marked FINISHED are complete in the repo. Every core area is finished.
FF2–FF5 game modules: see the end of this file.

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

## Core: dialogue, popups, title, save, game state, cursor routing (`src/core/dialogue`, `src/core/system`) — FINISHED

MessageWindowPatches, ScrollMessagePatches, PopupPatches + PopupState, TitleMenuPatches,
TitleScreenPatches, SaveLoadPatches, SaveListPatches, SaveSlotReader, GameStatePatches,
CursorNavigationPatches, MenuTextDiscovery, KeyHelpReader, CommandBarPatches + Reader,
FieldMenuPatches; the main mod's ManualPatches (cursor routing) and menu-state clears.
- Title prompt fallback is always T("Press any button") (FF1's reflection lookup never
  resolved). Scroll-text pacing uses real time. FF1's 20-coroutine cap is not ported.
- The magic command bar uses Serial.FF1 classes: FF1 only.
- Fires DialogueClosed. Subscribes to SceneLoaded (title detection by scene name) and
  MapChanged (ClearMenuFlagsForMapTransition - fieldnav also does this: one should go).
- `FieldMap.<InitMenu>b__N_0` differs per game; name_check cannot read "<" names (false miss).

## Core: battle (`src/core/battle`) — FINISHED

BattlePatches (with BattleStateHelper and the MainMenuController Show/Close hooks),
BattleCommandPatches, BattleItemPatches, BattleMagicPatches, BattleMessagePatches,
BattlePausePatches, BattleResultPatches, BattleStartPatches. All names checked in all five.
- BattleStart fires at StartPreeMptiveMes; BattleEnd at a real end and on ForceClearBattleState.
- Status removal ("X: Poison removed"): FF1 `<RemoveFunction>b__0`, FF2 UpdateTempParamter,
  FF3/FF4 RemoveFunction (prefix), FF5 RemoveFunction + StateChange, each game-masked with its
  mod's rules. Multi-hit only where `multi_hit_setting`.
- Battle magic list: FF1 only (class exists only in FF1).
- Gil/EXP numbers use a comma separator (FF1 used the system culture).
- Shared bodies, class-checked: BattleTargetSelectController.ShowWindow (10 methods),
  ResultMenuController.EndWaitInit (2).
- FF4/FF5 (ATB) gaps: "X's turn" interrupts may cut action lines; FF5's extra damage types fall
  back to "damage"; FF4/FF5 ability lists, FF5 enemy letters, results log and
  action-announcement hook are not in the core (game modules).
- Steal results (FF3, FF4, FF5; FF1/FF2 data has no steal command or function 52): the game
  writes its own line ("Stole Potion!", "Couldn't steal.") to the command message window once
  per steal - BattleFunction52.ViewEntity (FF3/FF4), BattleFunction52FF5.IsFunctionEnd (FF5) ->
  BattleUIManager.SetCommadnMessage -> KeyInput BattleMenuController.SetCommadnMessage, a tail
  jump into BattleCommandMessageController.SetMessage - so the system-message reader above
  speaks it. No separate steal reader: it would repeat that line. Binary evidence only; not yet
  heard in game.

## Lead integration notes (open)

- `src/core/glue/scene_glue.cpp`: FF1's "left a battle scene" clear on SceneLoaded.
- Done: ClearMenuFlagsForMapTransition ran twice on a map change; the system copy no longer
  subscribes to MapChanged. MainMenuController Show/Close is hooked only by battle.
- Done: first build with every area (no errors, no warnings), smoke test PASS, name check
  4821/0 missing, wiring check: only the four Battle*State.ShouldSuppress queries are undefined,
  which falls back to the menu-state flags exactly as FF1 did.
- Four master-data readers and two `condition_name` copies: merge into core helpers.
- Not yet done: in-game tests, then the release zip (Phase 5) and archiving the old repos (Phase 6).

## Core: status, config, bestiary (`src/core/status`, `src/core/bestiary`) — FINISHED

StatusMenuPatches, StatusNavigationReader, StatusDetailsReader, StatusStatDefinitions,
StatusNavigationHandler, ConfigMenuPatches, ConfigMenuReader, BestiaryPatches, BestiaryReader,
BestiaryNavigationReader.
- The status details screen is FF1 only (FF1's own controller and stat list). FF2–FF5 need
  their own controller hooks and stat lists: listed at the top of `status_navigation.cpp`.
- Config library states: 19/20 in FF1, 18/19 in FF2, 17/18 in FF3–FF6
  (`bestiary::config_handle_state_change(int)`).
- `StatusDetailsController.ExitDisplay` shares its body with 10 methods in FF1: class-checked.
- Language names ("English", "Japanese"…) stay English literals, as in FF1.

## FF3 module (`src/games/ff3`) — FINISHED (first pass)

Job change (Phase 1 proof hook JobChangeWindowController), Onion Knight naming, NPC item
select, line-fade story text, vehicles (Boko, Magical Armor; content 5 no longer "canoe"),
magic command bar (Learn, Use, Remove, Forget, Exchange, tomes, targets), battle spell list
(hooks SelectContent: FF1's SetCursor is a stub folded with ~2,500 methods in FF3), status
details with FF3's 26 stats. Keys: 9 toggles beacons; R on the status screen repeats the stat.
- Disabled in configure(): LineFadeMessageWindowController.SetData, FieldPlayer.GetOn/GetOff
  (FF3 replacements). Folded, class-checked: AbilityWindowController.SetNextState (9),
  StatusDetailsController.ExitDisplay (10).
- Superseded by the core, not ported: GameOverPatches, ShopCommandReader, GameInfoAnnouncer,
  CursorSuppressionCheck, BattleDamagePatches, BattleStatePatches, helpers.
- Pending:
  1. BattleSystemMessagePatches: decided 2026-10-06 - not ported. Everything it hooked already
     reaches the core's SetMessage reader; its other hooks fire only for map-title banners.
  2. Key 0 (dump untranslated names) needs a dump function in the core field area.
  3. Status Attack reads "Attack: {0}"; FF3's "hits x power" is not ported.
  4. Item, equip, shop, config, bestiary, save, title, popups, gallery, music and the field
     scanner were not compared file by file against FF3's old mod (all names resolve in FF3).
- Lead: menu_state reset handlers are now additive (a game's handler runs with the core's);
  battle exposes "BattleCommandPatches.NotifyCommandSubmenuActive".

## FF5 module (`src/games/ff5`) — FINISHED (first pass)

Jobs and abilities (JobAbilityPatches, JobEquipData, MenuFocusAnnouncer; Phase 1 proof hook
AbilityChangeController), FF5 magic, status details with FF5's stat list, naming, timers
(T reads, Shift+T freezes), FF5 keys (9 beacons, Shift+; landing pings, Enemy Letters toggle),
battle targets ("All allies/enemies"), results log on L (points page only).
- Exceptions to the rules, for the user/lead to approve:
  1. `Timer.Update` is hooked with MinHook directly and enabled only while timers are frozen
     (as the old mod did) - the one hook outside `hooks::attach`.
  2. The results log reads its own keys while open and blocks the game's input itself.
- Pending: FF5's own target reader (enemy letters, target statuses) - the core's SelectContent
  target hooks would double it; results-log pages for level-ups, abilities, items; battle
  totals with ABP, FF5 names for unnamed statuses ("Enemy: Float"), ability lists; mod-mode + B
  for the results log; Boko and transport types 11/12 in the vehicle table (to verify).
- Not ported: BattleStealItemPlug.StealItem ("Stole {0}"). It repeated the game's own result
  line, which the core speaks (see "Core: battle").
- Kept as the core has them (FF1 behaviour): StatusMenuFocusPatches, GameTogglePatches,
  KeyboardGamepadReader, MessagePatches, the H key.
- Mod_text key "No data available" added 2026-10-06 (line unreachable for now). English kept from the
  C#: "Empty slot", "Slot N:", ", equipped", ", Not learned", "Lv.", "Mastered!".
- FF5 navigation additions: moved into the core (see "Core: FF5 navigation additions").

## FF4 module (`src/games/ff4`) — FINISHED (first pass)

Ability menu (FF4's magic lists; Phase 1 proof hook AbilityCharaStatusController.UpdateView),
status details (17 stats), party setting and rows (row appended to character-slot reads),
naming, timers (T, Shift+T), battle spell list with I, battle targets ("All allies/enemies"),
FF4 vehicle words (boarding speech and V). Key 9 = beacons.
- Disabled in configure(): FieldPlayer.GetOn/GetOff (re-attached with FF4's words).
- Folded, class-checked: StatusDetailsController.ExitDisplay (8),
  AbilityUseContentListController.SelectContent, BattleTargetSelectController.ShowWindow (10).
- Covered by the core, not ported: MainMenuPatches, TitleMenuReader, KeyboardGamepadReader,
  GameTogglePatches, footsteps, wall bumps, GameAnnouncementHelper, MenuState.
- Steal results: spoken by the core (see "Core: battle"); the old SetCommadnMessage hook is not
  ported because SetCommadnMessage tail-jumps into the SetMessage the core already hooks.
- Pending, for the user: choice lists ("Choices: …") and system message windows (left out to
  match FF1); the status character
  list has no MP or row; Falcon and Lunar Whale are spoken as "airship"; item/equip, shop,
  popup, save/load, gallery/music, config and bestiary not compared file by file.
- Mod_text keys "hovercraft" and "yellow chocobo" added 2026-10-06 from FF4's own text.
- Navigation gaps against FF4's old mod: Moon pathfinding (map 3 step checks); rescan 0.5 s after
  scene load instead of on MainGame.set_FieldReady; clear-all waypoints asks once, not twice.

## FF2 module (`src/games/ff2`) — FINISHED (first pass)

Keywords (Ask/Learn/Key Items, Words menu), naming, shop command reader, field magic
(Use/Forget, "Fire lv3, 45 percent", targets with HP/MP), battle spell list with levels and I,
battle results (gil, weapon skills, spell level-ups, stat gains; no EXP), status details
(23 stats with weapon skills), H with MP, U ("Any character can equip this").
- Disabled in configure(): ResultMenuController.ShowPointsInit and ShowStatusUpInit (FF1's EXP
  and level-up phases). Folded, class-checked: StatusDetailsController.ExitDisplay (10).
- The keyword state also clears at SecretWordController.NoneInit/Close (a leftover state would
  block every field key).
- Not ported: MovementSpeech's ChangeTransportation (it calls FieldPlayer.GetOn, which the core
  already speaks); the touch battle spell list (never worked in the C# mod).
- Done (lead): `Flags::has_mp` (FF2, FF4, FF5) adds MP to the core's status character list,
  item-use targets and battle player targets. Config, bestiary, dialogue, popup,
  title, save, shop, item and equip compared by feature only.

## Lead changes after the game modules

- The details key (I) has a game-module entry: queries "GameModule.HasDetails" / text
  "GameModule.Details" (FF2's keyword and Words menus use it).
- `has_mp` flag: MP joins HP in the status character list, item-use targets and battle player
  targets for FF2, FF4 and FF5.
- Core hotkeys and game input pause while FF5's results log is open
  (`BattleResultNavigator.IsOpen`).
- `hooks::disable` now also applies to address hooks with a "Class.Method" label (FF5 can switch
  off the core's BattleTargetSelectController.SelectContent hooks for its own target reader).

## Core: FF5 navigation additions — FINISHED

- Breadcrumb chaining (all five games, `field/breadcrumb_chain.cpp`): when the game's route
  search fails beyond ~31 cells, `field::find_path_to` chains searches; a chained route reads at
  most 6 legs, then "and N more". The pathfinding filter and the beacon use
  `find_path_to_quick` (never chains); \ and / chain. For the user: FF5 also capped ordinary
  routes at 6 legs; FF1 never did, so ordinary routes are unchanged.
- Landing pings (FF1, FF2, FF3, FF5 - FF4 has no ship): an audio loop on a ship or submarine,
  `fieldnav/landing_spots.cpp`. Settings LandingPings (off) and LandingPingVolume (50). Each
  direction has FF5's own pitch (660/440/550/520 Hz), pan and level; the directions take turns
  every 250 ms (FF5 looped them together).
- Map-exit grouping: FF1's MapExitFilter already does this (same setting, nearest exit kept);
  FF5's "(N exits)" suffix is not added.
- Mod menu: Landing Pings and Landing Ping Volume in every game except FF4; Enemy Letters in FF5
  (no effect until FF5's target reader is ported).

## FF6 module (`src/games/ff6`) — FINISHED (first pass, 2026-10-07)

FF6ScreenReader (ZKline, BlindGuyNW/FF6ScreenReader) ported with its author's permission.
`game::kAll` now includes FF6, so every shared core feature runs in FF6; FF6-only screens are in
`src/games/ff6`. Shared features keep the core's (FF1) behaviour; FF6-only screens follow FF1/FF4/
FF5 conventions (AutoDetail, "(n of N)", I key, repeat checks reset on open), per the parity rule.
Name check: 0 missing in all six games. Not tested in game.
- Infrastructure: game detection, `kFF6`, CMake, data (FF6 mod_text merged: 102 new common keys,
  23 FF6 game-term overrides; `translation.json`), settings import from `[FFVI_ScreenReader]`,
  waypoints stay in `UserData\ffvi_waypoints.json`, deploy/uninstall. Landing Pings hidden (no
  ship). Flags: has_mp, atb, timer_freeze, has_hidden_passages (map data not checked).
- Core fixes for FF6 signatures: KeyInput `BattleCommandMessageController.SetMessage` and Touch
  `SetSystemMessage` take `(string, bool)`; `FieldPlayer.GetOn` takes 3 args (as FF3/FF5);
  library-return lambda `FieldMap.<InitMenu>b__99_0`. Every other core hook checked: same
  signature as FF1. `StatusWindowController.SelectContent` and `battle_item.cpp` masked to FF1-FF5
  (FF6 variants: row on the status list; Items and Tools share one controller).
- FF6 screens: ability menus (magic, Lore, Blitz input sequence on I, Gogo's commands, espers and
  esper details), battle items and Tools, battle abilities and "Summon, X", "All allies/enemies"
  incl. side attacks, battle speaker, Magic AP and learned spells after a battle, status details
  (18 stats, Commands, Magicite, At Level Up), status list with row, party setting, rows, Skills
  sort, item detail navigator, U = which party members can equip, Coliseum, airship heading and
  altitude (H on the airship; event hooks, not per-frame), timers (T, Shift+T), Opera
  success/failure/out of time, chocobo on V.
- Covered by the core, not ported: dialogue, popups (incl. the magicite popup), title, config,
  bestiary, music, gallery, save slots, cursor routing, item/equip/shop menus, item targets,
  damage/status/message box, field scanner, filters, waypoints, map names, footsteps.
- Folded, class-checked: `StatusDetailsController.ExitDisplay` (9), `SetSpeaker` (KeyInput and
  Touch share 0x493220), `AbilityUseContentListController.SelectContent`/`SetCursor`,
  `BattleTargetSelectController.ShowWindow` (10).
- For the user:
  1. Bestiary steal items (user, 2026-10-07): listed only where the game's bestiary lists them,
     the games with a Steal ability - FF4, FF5 ("probably", per the user), FF6
     (`Flags::bestiary_steal`); never FF1-FF3. "Stealable Items" comes before "Dropped Items".
  2. Name entry: FF6's ". Press Enter for default name" after the game's popup is not ported.
  3. Hit counts: FF6 has no "Multi-hit Damage" setting, so "N hits" is never spoken (the old mod
     said it whenever the game made a hit count; whether FF6 ever does is unverified).
  4. FF6 now says "X: Poison removed" (core behaviour; the old mod never did).
  5. FF6's world-map entrance grouping (Phoenix Cave, Kefka's Tower: hard-coded boxes and English
     names) is not ported; F1/F3/F5, 0 for "All", "Can not land", NPC movement words and vehicle
     footsteps follow FF1.
  6. Row on the status list is FF6 only; FF4's list still has none (FF4 gap above).
  7. The core's battle item reader requires Item to be command 3 (FF1's gate): FF2-FF5 battle
     item reads may be silent where Item is elsewhere (FF4). Not changed.
  8. Mod text key "Low Flying Airship": translated by hand on 2026-10-07 (no game text names
     it; built from FF6's "Special Airship" and "Low altitude" wording), in modtext_fixes.py.
  9. HpAbs/MpAbs/Golem hit types are read as "N damage", as the old mod did.

## User rule: game text over mod text (2026-10-06)

"You don't need mod-side results and game-side results for anything, just read game results
where available and only keep mod-side results if information is icon or picture
representation only." Applied first to the battle action line: the game's message-box text
("Steal") and the mod's actor are paired into one line ("Edge: Steal"); a basic attack, which the
game names nowhere, keeps the mod's "Actor: Attack". Damage numbers and status icons are
picture-only, so their mod lines stay. A whole-mod audit for other doubles followed.

Doubles fixed by the audit (each stops one producer; none compares text):
1. Battle enemy target, FF2-FF5: first enemy spoken twice on opening (EnemysInit calls
   SelectContent itself) - now once.
2. Item-use target, all five: first target twice on entry, a stray party member on exit - now
   once, nothing on exit (new hook ItemUseController.SingleExit, unshared in all five).
3. FF5 field ability target: twice - now once.
4. Game-over Load popup, all five: each button read by the popup and the menu reader - now once,
   the game's label.
5. Name entry, all five: the mod's "Enter name for X…"/"Keyboard input active" plus the game's
   popup - now the game's popup only (the mod line stays for Steam's gamepad keyboard).
6. FF4/FF5 Name Change: two names on opening and a repeat on confirm - now one, no repeat.
7. FF5 Name Change moves: mod name plus menu reader - now the mod's name only (as FF4).
8. FF1/FF3 suggested names: twice per move - now the game's on-screen name with its position
   (from FF2's in-game note, not code proof).
9. Shop command bar: "Buy" cut off and repeated - now once.

Mod-composed where the game shows text (no double; the user's call whether to switch):
"Entering X" (the map-title banner is not hooked), battle results, status list and details
(FF1-FF4), save slots, shop names, command bars and quantity, item/magic/ability lists and
targets, bestiary/music/gallery rows, new-game prompts (FF1's "Start game?"), battle command
names. Suspected doubles, not proven: "On foot" twice, FF2 Learn keyword, a config row re-read
after answering No, the title Options first row.

## First FF2 in-game session (2026-10-10): five reports, fixed, not yet re-tested

Evidence: FF2's `ffpr_screen_reader.log` of 2026-10-10 04:40 and the user's reports.

1. **"Use" on the world map right after loading a game (all six games).** The game pre-loads
   every UI scene on the first field (CampMenu, SecondMenu, Battle, ShopMenu, ...) and
   initialises their controllers while the objects are still active; the item window came up
   in its command-select state with its cursor set, so the command-bar reader's one-frame read
   found "an active window in state 1" and spoke. A window's own state machine cannot tell this
   from a real open; the main-game state can (`SubSceneManagerMainGame.State`: Init, ChangeMap,
   FieldReady, Player, Transport = 0-4 in all six dump.cs files; Menu 5, Shop 9, Battle 13).
   New `system::is_field_state()` / query `GameStatePatches.IsFieldState` (true also while no
   state is known yet). Every command-bar read skips on it: the core's item, equipment and FF1
   magic bars and `IsCommandBarActive`, FF2's and FF3's magic bars, FF4's, FF5's and FF6's
   ability bars (`announce_command`).
2. **FF2 magic bar "Use" twice on entry.** Two producers: the game sets the bar's cursor once
   more while opening the window (SetActive, ResetController), with the state machine still on
   Command from the last visit and BEFORE CommandInit runs; the C# and the port took that as an
   entry and spoke, then CommandInit forgot the guard and the entry read spoke again. The
   cursor-set hook now speaks MOVES only (never activates the bar itself); the entry is
   CommandInit's, as in FF1. A log line names the skipped early cursor-set so the next log
   confirms the order. FF3 (deferred reads superseded per frame, FF1's design) and FF4/FF6
   (guard reset before the init body) were checked and left alone.
3. **Character pre-screens: "Firion, Firion, Level 1, HP .., MP .."** (magic, equip, status,
   formation). The core's character-slot reader is FF1's: it walks the slot's texts by object
   name, and FF2's job text carries the character's name again (FF2 has no jobs). A job text
   equal to the name is dropped. Not a dedup: one producer, one field removed.
4. **MP.** The same reader read MP texts in every game; it now reads them only with
   `Flags::has_mp` (FF2, FF4, FF5, FF6), so FF1 and FF3 never speak an MP stat anywhere (their
   spell-charge lines keep their "MP: a/b" wording, FF1's). **FF2 keeps MP:** FF2 has MP (spell
   level = cost); the FF2 C# read "Name, HP a/b, MP c/d" here and its status menu said so
   ("MP (FF2 specific - unlike FF3 which uses spell charges)"). The user asked for MP to go in
   FF2 as well; raised, awaiting the user's decision. Open question for the user: FF2 has no
   character levels, yet "Level 1" is read from the slot's level text (the C# FF2 never read a
   level on these screens).
5. **Gil drops read as "Potion x4".** FF2 lists one gil row per monster in the victory item
   list ("4 Gil", "18 Gil"); a gil row's content id has no item behind it, and FF1's lookup
   (`GetMesIdItemName(contentId)`) named every row after item 1. The core now asks the game for
   the rows it renders, `ListItemFormatter.GetContentDataList(List<DropItemData>, MessageManager)`
   (the FF2 C#'s way; three two-argument overloads, picked by the drop list's own class through
   `il2cpp::find_method_overload`), and speaks each row's Name and Count; FF1's lookup stays as
   the fallback when the method is unresolved.

Re-test steps: `TESTING.md`, FF2 items 9-11.

## FF2 session, second batch (2026-10-10): built, not yet re-tested

6. **The Dreadnought listed as an NPC on the world map.** FF2's world map (Map_10010) carries
   it as NPC objects in conditional groups with an action but no script and no message; the
   game's CanAction is true, and the core's scenery rule only sees objects without an action.
   A "no script and no message" rule for NPCs is unsafe: FF3's townsfolk talk through
   map-script handlers and look the same offline (tools/mapdump counts: 285 such NPCs in FF3,
   505 in FF4). The old FF2 mod never listed it because its EntityScanner had a name list
   (IsPlaceholderEntity: vehicle spawn NPCs, location markers, dev triggers), which the port
   had dropped. New `game::Flags::placeholder_entity`, a per-game function asked with the raw
   property name in the scanner's NPC, event-trigger and interactive branches (where the C#
   asked); FF2's list is ported whole in `src/games/ff2/module.cpp`. FF4/FF5's C# placeholder
   filters (`tools/audit_events.py` mirrors them) are still not ported: no report yet.
7. **No character level in FF2** (user: "it isn't used"). New `Flags::has_levels` (false for
   FF2): the character-slot reader, the status character list, the status details line and
   the save slots skip the level. FF2's weapon and spell skill levels are unaffected (they are
   the game's).
8. **"真ヒルダ" spoken in Japanese.** The core's translator is FF1's; the FF2 C# had a step for
   a leading 真 ("true"): the remainder's name with "(true)". Ported: "Hilda (true)".
9. **Dictionary audit, all six games.** New `tools/translation_audit.py`: for every object the
   mod names by its property name it mirrors the translator's lookup against the game's
   `data/<game>/translation.json`. First run: FF1 0, FF2 3, FF3 108, FF4 0, FF5 15, FF6 707
   untranslated names. The misses were the translator, not the dictionaries: FF3 keys its
   sprite variants with the trailing digit ("女性4") that FF1's tiers strip first; FF6's
   (ZKline's) translator strips "NPC1_", "sc_e_0091:" / "ev_e_1030：" prefixes and
   ":ev_e_1030", ":イベント前", "(オブジェクト)" suffixes. The union is now in
   `entity_translator.cpp` as extra tiers after FF1's (FF1's results unchanged), plus "NPC:"
   with a colon, which the FF6 translator also missed. After: FF1-FF4 0, FF5 15, FF6 75; the
   remaining names have no dictionary entry at all (`tools/mapdump/<game>_untranslated.txt`):
   FF5 event descriptions ("タイクーン王探し④", chains in the tower) and two shop NPCs; FF6
   event descriptions, dev markers ("セーフ位置") and a few NPCs. Adding them means writing
   twelve languages: the user's call.
10. **FF6 map data extracted** (`tools/mapdump/ff6_*`): 267 bundles, 638 sub-maps, 15957
    objects, 2841 scripts, 104 master tables, 0 errors; same Addressables layout as FF1-FF5,
    two extra object types (29 ChangeAnimationKeyArea, 30 SwitchEvent). The baked-map work
    (OPEN_ISSUES.md) starts from this.
11. **Shop party panel on the number row, all six games** (user request). The game draws, per
    active-party member, an up or down arrow, an equipped mark and a "cannot equip" text for
    the focused item (`ShopPartyController.activeContentList` ->
    `ShopCharaStatusContentView`, identical in the six dumps; it carries no stat numbers).
    New `src/core/shop/shop_party.cpp` and key context `Shop` (open shop; Global keys still
    match): 1-9 read slot N as "Name, Stronger / Weaker / No change[, Equipped]" or "Name,
    Cannot equip". Rendered text is read as drawn (the mark's text, the impossible text) unless
    it is a bare symbol; the arrows are icons, so their words are the mod's. Five new
    mod-text keys (Stronger, Weaker, No change, Cannot equip, Equipped), translations written
    by the port, not by a native speaker.

12. **"Save complete" not spoken (all six games).** The completion popup re-shows the same
    SavePopup and fills its text after the write; the C# and the port read it once, two frames
    after SetPopupActive(true), and found it empty ("[SaveLoad] MainMenuSave: Message is
    empty" in the FF2 log). The read now retries each frame until the text is there (bounded,
    30 frames), one announcement; the quick-save popup shares the path.
13. **FF5 and FF6 dictionaries completed** for the names the audit listed (15 and 75), in all
    twelve languages (`tools/add_translation.py` appends without touching the file's layout:
    FF5's file is LF, FF6's CRLF). Proper nouns are the games' own names in every language
    (Baram, Imp, Nikeah, Magitek Factory, Serpent Trench, Injured Lad, Returner, Wedge, from
    the message tables dumped with `dump_masters.py --all-langs`; the characters from the
    dictionaries); the descriptive words are the port's own translations, not a native
    speaker's. The audit is now at zero in all six games. Still open: 1250 of FF6's 1574
    entries are English-only (ZKline's dictionary; the translator falls back to English for
    them), a separate and much larger job.
14. **MP stays in FF2** (user, 2026-10-10: "that was my bad"). Item 4 above is closed.
15. **FF6 dictionary fully localized** (user: "ff6 should have full localization support like
    the other games"). The 1250 English-only entries of ZKline's dictionary now carry the ten
    other languages (12,500 values). Method, kept in `..\tools\ff6_localization\`: each entry's
    English became a template with its numbering as `{n}` (680 distinct templates for 1250
    keys); proper nouns are placeholders resolved from a glossary of the game's own names in
    every language (the message tables and the already-translated entries: Terra, Narshe,
    Magicite = fr "Magilithe", Imperial Soldier, ...); the descriptive words are the port's
    translations, consistent per batch (fr/it/de/es/ko/zht/zhc/ru/th/pt), not a native
    speaker's. `tools\fill_translation.py` merged them without touching the file's layout
    (CRLF kept; `git diff -w` shows only value changes). The audit tool reports zero, and no
    entry in any of the six dictionaries is English-only any more.
16. **NPC numbers (all six games).** "Palamecian Soldier 7 ... Borghen 11": the names end in
    the developers' per-map object numbers (shared across every NPC on the map), which the
    translator keeps as " N" and FF1's reader spoke as they were. User's rule (2026-10-10): a
    number only when several NPCs share a name, 1..n within that group; a unique name has none.
    `renumber_npcs` in `entity_scanner.cpp` runs on every scan from each entity's converted name
    (new `Entity::converted_name`), orders a group by the developers' numbers (then top-to-
    bottom, left-to-right position) so a number never moves with the player, and NPC only
    (events, exits and chests are named otherwise).
    October 12 follow-up replaces the trailing-digit-only logic with shared live/remote
    normalization of prefixes, suffixes, Japanese full-width/circled numbering and nested
    script/NPC metadata. Duplicate suffixes now follow the requested Soldier1/Soldier2
    wording. [Current all-map/language evidence](FOLLOWUP_2026_10_12.md).

Re-test steps: `TESTING.md`, FF2 items 12-16.

## Third batch (2026-10-10): save popups, vehicles, job slot, baked maps, puzzle guides

Built, smoke-tested and deployed; not tested in game. Re-test: `TESTING.md`, "Third batch".

17. **"Save complete" still silent** (user). The wait for `messageText` (item 12) could never
    end: the completion popup is the same SavePopup with a different layout.
    `SaveWindowController.CompleteInit` (FF2 GameAssembly.dll 0x8EC3B0, read with capstone and
    the Il2CppDumper script.json) sets `titleText` to MENU_SAVE_COMPLETED, `contentTitleText`
    to the "Save" heading, and `messageText` / `subMessageText` to `string.Empty` with their
    objects deactivated. A blank message now falls back to the title. The quick save's
    `InterruptionWindowController.InitComplite` (0x782220) rewrites the OPEN popup without
    `SetEnablePopup`, so no popup hook fired for it at all: it is hooked (every game has it).
18. **FF2's snowcraft spoken as "On canoe"** (user). FF1's per-type words named every type-5
    ("Content") vehicle a canoe; FF2 has two (canoe id 2, snowcraft id 5), FF3's canoe has no
    message_id either. One resolver, `field::vehicle_display_name`: the transportation master's
    message_id (the GPS labels' keys: "Ship", "Airship", "Chocobo" ...), else the game
    module's key-item message for a row without one (`Flags::vehicle_message_id`: FF2
    MSG_KEY_NAME_02 / _05, FF3 MSG_KEY_NAME_01), else the old per-type words. Used by the
    boarding line, the V key (`move_state::current_name`) and the Vehicles category. The
    boarding hooks read `TransportationController.CurrentTransportation`, which the game
    switches before `FieldPlayer.GetOn` (FF2 `FieldController.ChangeTransportation`:
    TransportationController.ChangeTransportation at +0x227, GetOn at +0x606). FF3's module
    compared a transportation MASTER id with the type constants; it now reads the controller.
19. **FF2 character slots still read the job text** (user). `Flags::reads_job_slot` (false in
    FF2): the slot reader and the status list skip the job text entirely.
20. **Altair's rebel hideout "lost its labelling"** (user). The late-game hideout rooms
    (map ids 325-329, `map_20131_6..10`, the rooms of `map_20011_1..5` after the empire's
    attack) have `map_title None`, floor 0 in the map master, so the resolver said "Altair"
    alone; the early rooms carry MSG_MAP_NAME_11 "Rebel Hideout". New `data/ffN/map_titles.json`
    (map id -> message key, so the text stays the game's), applied when the master has no
    title. Every map's offline label was computed from the masters (tools/mapdump): the other
    bare labels (Castle Fynn rooms, the Dreadnought, Castle Deist, the Coliseum, Leviathan ...)
    are bare in the game's own data; candidates with a titled layout twin are listed in
    OPEN_ISSUES.md for the user to confirm.
21. **Baked maps (the user's plan).** `tools/bake_maps.py` packs `tools/mapdump` into
    `data/ffN/maps.bin` (3 MB for the six games, embedded as MAPS_FFN): every sub-map's
    collision per layer (every non-zero id is closed, as the game's
    `MapRouteSearcher.MakeRouteMapWithCollision` does: FF2 0xA987B0 decompiled read-only with
    `tools/ghidra/decomp.bat`), hidden passages, world attribute grids, the world maps'
    `transportation` asset (one 0/1 layer per transportation GROUP: where that group's
    vehicles move; found 2026-10-10, dumped by `dump_map_objects.py`), the routing objects and
    the transportation masters. `src/core/field/baked_map` reads it, `baked_router` is the A*
    (4-connected, ToLayer cells change layer, hidden passages optional, a closed goal cell is
    entered as the game opens GotoMap cells, step-on warps are crossed only as the goal),
    `baked_nav` is the game-side glue: the live map id -> sub-map, the player's layer, the
    counter rule (a service NPC's `link_object_id` -> its RelayInteraction tile), live NPC
    blockers (explicit routes only), the current vehicle's group, the owned auto-vehicles
    (type 5: canoe, snowcraft: their terrain joins walking), and boarding a parked ship or
    airship when walking cannot reach the target ("board the Ship", "land"). Quick mode (the
    filter, the beacon) floods once per player cell. `field::find_path_to` uses it first; the
    game's searcher and the breadcrumb chainer stay only for a map the data does not know.
    Routes longer than the old window are capped at six legs with "and N more" (FF5's rule).
    Offline: `tools/route_check.cpp` (build.bat) checks a Fynn shop counter, a layer change,
    the FF2 world map on foot, by canoe, by ship and snowcraft, and a town tile as a goal.
22. **Across floors and puzzle guides.** `bake_maps.py` also bakes warps: an Event or Entity
    whose script's Main segment runs `ChangeMap [map, point]` (script.txt maps script_id to
    the asset: FF1's Citadel pads sc_war_30071_N, trapdoors, Pandaemonium's zones).
    `baked_nav::find_across` runs Dijkstra over the sub-map graph (GotoMap exits and warps,
    each arriving at the destination's PointIn) and speaks only the first leg, ending at the
    exit or warp to take ("take the exit to Snow Cavern B3", "step on the warp"); the player
    continues after the transition (user, 2026-10-10). `src/core/field/puzzle_guide.cpp`
    follows authored step lists (`data/ffN/puzzles.json`: map id, cell, examine flag, a
    mod-text key) with live directions, advancing on arrival (checked on each movement input,
    `FieldPlayerKeyController.OnTouchPadCallback`) and re-announcing after a map change.
    Shift+G / Ctrl+G; pad mod mode + RB / LB. Authored from the dumps' developer labels: FF1
    Citadel of Trials; FF3 five hidden-door switches; FF4 Troia Castle B1 (the label says
    1→2→3→4), Castle Baron, Devil's Road; FF5 the Fire-Powered Ship engine room; FF6 the Zozo
    clock (6:10:50), Darill's Tomb, the Ancient Castle, the Cultists' Tower, Owzer's painting.
    FF2 has none (no switch or ordering puzzles in its dumps). 22 new mod-text keys, the
    port's translations.
23. **Waypoints** route through the same `find_path_to`, so they are on the baked router too.

## Fourth batch (2026-10-11): routes kept, pinpoint goals, facing, the beacon on the route, map labels

Built, smoke-tested, deployed to FF1 and FF3-FF6 (FF2 was running: `deploy.bat ff2` is still
due). From the user's first baked-map session in FF2.

24. **A route is planned once and followed** (user: "the route should be built once and then
    followed to completion"; a re-plan four steps in said "East 4, North 1, East 2", straight
    back). `baked_nav` keeps the active route per (map, target cell); asking again from any
    cell on it speaks the rest from there; a new target, another map or a cell off the route
    plans afresh. Cause of the flip: a villager standing in the street at planning time was a
    live blocker and sent the route thirty steps round the block. Now a chest is a wall but an
    NPC is walked round only when that costs at most six extra steps (`kMaxNpcDetour`);
    otherwise the route goes straight and the player waits a moment. Log lines "[BakedNav]
    route planned / route kept / no route" give start, target, map, steps and blockers.
25. **"No path" in Josef's house (Salamand).** The sub-map names its only collision layer
    "GroundObject"; `bake_maps.py` accepted only "...Collision" and baked the house with no
    open cell. FF3 had 13 such maps. `collision_rows` now takes any layer of the collision
    asset by its Bottom/Ground/Upper prefix; all six games re-baked; `route_check.cpp` routes
    the door to Nelly (14 steps). The adventurer the user doubted is baked there.
26. **Turn penalty** (BDFFHD's `navgrid.cpp` model, the user's choice): the A* state is the
    cell plus the direction it was entered by; a step costs 10, a turn 15. An open room is
    crossed in two legs, never "West 1, South 1, West 1, South 1"; the Salamand-Semitt Falls
    canoe route fell to ten legs. `tools/route_check.cpp` asserts the two-leg crossing.
27. **Where a route ends** (`baked_router.h` `GoalKind`, `field_api.h` `TargetKind`; user:
    pinpoint, never "within 1 step"). Interact (NPC, chest, switch): the open 4-neighbours the
    NPC accepts (`accept_action_direction`: bit 1 north side, 2 east, 4 south, 8 west - FF2
    `IsAcceptActionDirection` 0x180C4AC90 decompiled read-only, and the dumps place the
    excluded side against a wall), the counter relay first; the route ends with "face North"
    unless the last leg already points that way, and a zero-step route says only "face North"
    instead of "No movement needed". Transition (exit, town tile, warp pad): the exit's own
    cells and its sibling tiles (same destination and point within two cells: a town's arc of
    three), and on a world map only those whose terrain attribute the current transport crosses
    (`transportation_group.txt`): Salamand's side tiles (123,34) and (125,34) and Altair's
    (90,117), (92,117) stand on wall attribute 19 and were sending the player to a corner
    ("North 1" into a wall); the entrance tiles (124,33) and (91,116) are plain ground. Spot
    (waypoint, vehicle, save point): the cell itself. Infer (events) by what is baked there.
    Callers pass the kind (`entity_nav.cpp` by entity kind, the beacon by category, waypoints
    Spot). The eight-neighbour goal set is gone. New mod text "face {0}" (12 languages).
28. **The beacon follows the route** (the FFXII mod's `audio_beacon.cpp` model, user request).
    `baked_nav::follow` gives the next corner of the active route from the player's cell,
    planning silently when there is none or the player left it (a failed plan retried after
    two seconds); `audio_loops.cpp` pans and paces the ping toward that corner, moves on at
    the corner and goes quiet on the last cell. The spoken route and the beacon share one plan.
    A map the baked data does not know keeps the old crow-flies ping with the Quick path test.
29. **Map labels by inference** (user rules, 2026-10-11; `docs/MAP_LABELS.md` has every
    entry's evidence). `data/<game>/map_titles.json` values are now a message key, `{"text"}`,
    `{"owner"}` ("N's House"), `{"room"}`, `{"entity"}`, `{"item"}` ("I Room") or a list
    (`map_names.cpp` `override_title`); an authored entry now wins over the master's title
    too, for maps the master titles wrongly. What the NPCs say decides a building's function
    before any name (user), the whole town's talk included: FF2 Poft's Cid room is the pub
    ("He's probably over in the pub"), FF3 Amur's four-old-men room the tavern, FF4 Troia's
    sickroom and the Dwarven Castle's Cid room infirmaries (nurse, doctor, "sleeping in the
    infirmary"), FF6 Figaro's hall where Edgar speaks as King the throne room; FF1's six
    churches were "2F" (the priest's "It will cost N gil to resurrect" line names them). FF1
    6 labels, FF2 27 (churches, Fynn's pub and Scott's room, Castle Fynn's throne room,
    Josef's and the Masked Figure's houses, the Dreadnought's prison and engine room, the Pass
    room, Altair's revival chamber ...), FF3 24, FF4 5, FF5 18, FF6 43. 13 mod-text keys added
    with the assistant's translations. FF2's
    `translation.json` 協会 (the devs' homophone of 教会) reads "Church", was "Association".
31. **Every tile object was baked one row too far south** (user, 2026-10-11: "Item Shop Clerk
    2 ... No path"; the log showed the live clerks at row 6 and the bake at row 7, FF1's Chaos
    Shrine and Caravan exits the same). Tiled anchors a tile object (one with a gid: NPCs,
    chests, exits, arrival points, relays, switches) at its BOTTOM-left corner and a rectangle
    object at its top-left; `dump_map_objects.py` stored `y / 16` for both. `bake_maps.py`
    `tile_row` now takes `(y - h) / 16` for tile objects (the dumps are unchanged, `route.py`
    applies the same correction), all six games are re-baked, and every puzzle step cell moved
    up one row (they were authored from the same dumps). Consequences: a shopkeeper's live
    position now matches its baked tile, so the counter relay is found again; a town's exit
    arc sits one row higher than item 27 said (Salamand (123,33) (124,32) (125,33), all on the
    arc attribute 20, so all three are goals and the wall tiles below them never are);
    `route_check.cpp` derives its cells from the baked objects instead of hardcoding rows.
32. **Through another floor** (user, 2026-10-11: Kashuan Keep's "Exit to 2F" said "No path"
    from the entrance; the way is the sealed door into the other 1F part, across it and back
    in at the upper door). When the direct route on a sub-map fails, `find` runs the
    cross-map search (`find_across`, Dijkstra over exits and warps) with the same goal cells
    and speaks the first leg ending at the exit to take ("North 20, take the exit to Kashuan
    Keep 1F"); the player asks again on the other side. Offline, `route.py` confirms the
    chain: entrance to door 20 steps, part B 34 steps, upper arrival to the 2F stairs 41.
33. **Why a route is blocked** (user). Live obstacles are sorted by what they are
    (`live_blockers`): chests are walls; a collision entity or collision object active now
    is a story gate (FF1's bridge before it is built, the keep's door seal, "no entry until
    after the event"), read through `PropertyEntity.EntityId` against the baked object and
    its footprint; an NPC that stands still (bake flag 16 off: no move steps) is a wall while
    it stands there; a wandering NPC is walked round within six steps as before. With no
    route, `PathInfo.reason` says "Story gated" when the gates alone cut the target off (the
    cross-map search without them succeeds too), "Blocked by an NPC" when the standing NPCs
    do, or on a world map "Requires Ship" / "Requires Airship" / "Requires Canoe" naming the
    cheapest vehicle whose terrain group reaches the target (the game's vehicle name), owned
    or not. The announce key and waypoints speak the reason instead of "No path".
34. **Three-way pathfinding filter** (user; `PathfindingFilterMode`, menu enumeration, Shift+\
    and R3 cycle it, the old on/off value seeds the mode): "Current floor" lists what walking
    reaches on this sub-map; "Current map with vehicles" also what a vehicle the party has on
    this map reaches (`find_path_to_quick(..., with_vehicles)`: the parked vehicles' groups
    join walking as the canoe does); "Current map" lists everything, and item 32 routes the
    cut-off ones. Six mod-text keys added with the assistant's translations.
35. **FF1 Cornelia to Pravoka** was never the failure: the log's "no route" was the Caravan
    at the far north-west; Pravoka routes offline on the walking layer and the terrain
    attributes alike. The bridge is a live collision entity until it is built: item 33 says
    "Story gated" for every target beyond it until then.
36. **Only owned vehicles are named** (user): "Requires Ship" when the party has the ship (an
    auto-vehicle it owns, or a vehicle parked on this map); "Requires vehicle" otherwise, and
    the readme says that means a vehicle not owned yet. The filter's reachability test now
    counts a goal reached when a cell beside it is (the router enters a closed goal cell from
    its neighbour, and the cached flood was built for another target's goals): the likely
    reason FF1's Pravoka was filtered out while Cornelia and the Chaos Shrine were not.
    **Known issues after this session:** the "with vehicles" mode trusts a parked vehicle's
    whole terrain group; gates and standing NPCs are known on the current floor only, so the
    cross-map leg can still lead to a closed door on another floor; a wandering NPC is walked
    through; a map the data lacks keeps the crow-flies beacon; the inferred room labels are
    the assistant's reading of the dumps (`docs/MAP_LABELS.md`), untested in game; FF1
    Pravoka's filtering needs a re-test with this build.
37. **FF1 Matoya's Cave and Pravoka "Requires vehicle" from Cornelia** (user, 2026-10-11,
    every vehicle owned). The log: `no route: (145,156) L1 -> (161,110) ... 30 gates ...
    Requires vehicle`. Not the gates (the reason check removes them) and not the walking
    layer (Transportation01 reaches both: an offline flood from Cornelia reaches (161,111)
    and (203,143)). The bridge cell (145,145), the only walking cell between Cornelia and the
    north, carries the opening-credits Event (sc_e_0006: `Branch ScenarioFlag ... ChangeMap
    [124, 1] sc_e_0006_1`), and the bake made every Event with a ChangeMap in Main a step-on
    `Warp`, a hazard the router never crosses. `bake_maps.py round_trip`: when the ChangeMap's
    continuation script, on the destination map, itself runs ChangeMap back to a map id of
    the origin sub-map, the event is a cutscene and stays a plain Event. All six games are
    re-baked; the rule keeps 3 FF1 (the bridge, the Ryukahn airship scene, the Mirage Tower
    window), 3 FF2, 1 FF3, 11 FF4 (the prologue, Mt. Ordeals' entrance, the first camp, Cid),
    54 FF5 (the pirate-ship swap, the ship sinking, the barrier, tents) and 22 FF6 (Figaro
    surfacing, levers, "OK lines") events as events; trapdoors, warp pads and "to the world
    map" script exits stay warps (FF1: 47 step-on). `tools/route_check.cpp` now also takes
    `data/ff1/maps.bin` (build.bat): the bridge is open and no hazard, the unbuilt bridge's
    CollisionEntity remains on it (the live story gate), and Cornelia reaches Matoya's Cave
    in 74 steps and Pravoka in 92. `route.py` `world_types` no longer crashes on FF1's
    attribute_group.txt (blank rows). Also: the counter-relay rule applies to NPCs only (a
    chest's link_object_id is its own object id and names no relay).
38. **The other floors' destinations are listed** (user, 2026-10-11: "current map should
    include all dungeon floors, town floors etc"). Under the "Current map" filter the list
    appends, after the live entities, the talk targets, chests, save points and outward exits
    of every sub-map connected to this one through exits and warps without crossing a world
    map (`baked_nav::connected_objects`, breadth-first from here; `entity_scanner.cpp`
    `build_remote`, once per map). Named as the live list names them: chests by their baked
    content id (`bake_maps.py` packs it above the flag bits of `d`) through the game's item
    names, NPCs through the translator and numbered within their floor, exits "Exit to X"
    only when X lies outside the group; spoken "{name} ({floor}) - {type}" without steps or
    direction. Their live state is unknown from here: an opened chest or a story-hidden NPC is
    still listed, and the game's conditional object groups ("ev_*" variants) fold into one
    entry per name and cell. The directions key routes to one through
    `baked_nav::route_to_goal` (the cross-map Dijkstra; the first leg ends ", take the exit
    to X"); the beacon follows that leg (`follow_goal`). Nothing is added on a world map.
39. **A route continues after the transition** (user: "the player is told which transition
    to take, but then loses the destination"). A route that goes on past this floor (another
    floor's entry, or a target cut off on this floor) is remembered
    (`baked_nav::remember_destination`; `PathInfo::next_map_id` says a route goes on). 0.8 s
    after "Entering X", `continue_pending` speaks "<name>: <the next leg from where the player
    arrived>" (`entity_nav.cpp` subscribes to MapChanged), and the scanner re-selects the
    destination on the new floor (`select_pending_destination`: a live entity now, or still
    another floor's entry) so the directions key and the beacon keep leading to it. Cleared
    when a leg ends at the destination, when nothing reaches it ("<name>: Story gated" or
    "No path"), or when another entity is selected (cycling, a category change). A puzzle
    guide step and this may both speak after a map change; both queue after the map name.
    **Known issues after this session:** the other floors' entries ignore story state
    (opened chests listed, variants folded); their NPC "scenery" test is the bake's
    script-or-action flag, not the game's CanAction; a big town or dungeon gives a long list
    (the categories narrow it); a one-way story warp that has already fired is still a hazard
    (the live Event's state is not consulted); FF2's copy of this build waits for the game to
    close (`deploy.bat ff2`).
30. **The game's searcher** is still called only when the baked data does not know a map
    (`navigation.cpp` `find_path`, with `search_target_and_neighbours`,
    `search_with_passages_open` and `breadcrumb_chain.cpp`). The user will retire it once the
    beacon is confirmed in game; nothing else depends on `MapRouteSearcher`.
