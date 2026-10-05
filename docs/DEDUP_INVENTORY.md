# Deduplication inventory

Every check in the five old mods that stops repeated speech. This is the input for the central deduplicator.

Written 2026-10-05 for Phase 0 of `../UNIFIED_MOD_PLAN.md`.

## How this was made

- One research pass per game, reading the code with grep. Every row was checked against the code around it.
- Nothing was tested in game. The findings below are from reading code only.
- File paths in the tables are relative to each old mod's repo, for example `ff1/ff1-screen-reader`.
- "Compares" is one of:
  - `index`: a cursor or list position;
  - `object`: a game object, by reference;
  - `text`: the spoken text;
  - `index+text`: both;
  - `other`: an id, a flag, a set, a frame number or a tuple. The cell says which.
- Audio-only trackers (footsteps, wall tones, beacon timing) are not counted. Each game lists them under "Excluded".

## Counts

| Game | Deduplicator calls | Contexts | Scattered trackers | In files |
|---|---|---|---|---|
| FF1 | none | — | 37 | 22 |
| FF2 | none | — | 43 | 15 |
| FF3 | 36 | 28 | 34 | 20 |
| FF4 | 45 | 35 | 32 | 17 |
| FF5 | none | — | 59 | 23 |

What the rows compare:

| Game | index | object | text | index+text | other |
|---|---|---|---|---|---|
| FF1 | 11 | 2 | 9 | 0 | 15 |
| FF2 | 7 | 4 | 12 | 0 | 20 |
| FF3 | 11 | 3 | 30 | 0 | 26 (9 are gates) |
| FF4 | 12 | 1 | 38 | 2 | 24 |
| FF5 | 23 | 3 | 12 | 0 | 21 |

Why these differ from the plan's figures:

- These count every variable separately, including once-flags, frame stamps and "previous value" checks.
- FF3's 36 calls are 19 direct calls, 10 through the menu-state wrappers and 7 `AnnounceIfNew`.
  The plan's 51 also counted plumbing lines.
- FF4 has 45 calls and 47 `Reset` calls. The plan's 98 could not be reproduced.
- `AnnouncementDeduplicator.ResetAll` is never called in FF3 or FF4.
- FF3's 34 trackers include 9 gates. FF4's 32 include 8 open-read flags and 4 checks with no stored state.

## Findings to carry into the port

Each is a likely bug in today's mods, from reading code. Each is also a test item for the port.

1. **"Entering {map}" is never reset, in all five.** Loading a save on the last announced map is silent.
   FF1 does this on purpose. The others have a reset method that nothing calls.
2. **One shared slot for fade and scroll messages, never reset** (FF1, FF2).
   A later message identical to the last one is silent. FF3 uses a 2-second window instead, which is time-based.
   FF3 also shares its `Battle.Message` context across 6 hooks, some outside battle; it resets only at battle start.
3. **Vehicle and "Can land" trackers: reset methods with no caller.**
   - Movement state: FF2, FF3, FF4, FF5.
   - "Can land": FF1, FF3, FF4, FF5. FF2 resets it on scene load.
4. **"X's turn" keyed on the character id.** It resets only at battle start or end.
   A lone survivor hears "X's turn" once per battle (FF1, FF2, FF3; FF5 never resets it, by design).
5. **Config values compared as text with no row** (FF2, FF3, FF4, FF5).
   Set one row to "On", then another row to "On": the second is silent until the config menu closes.
6. **Status added keyed on "Name: Status" text** (FF3, FF4).
   Two same-named enemies getting the same status in a row: the second is silent.
7. **Empty spell slots.**
   - FF2 and FF3 store -1 for every Empty slot, so a second Empty in a row is silent.
   - FF1 does not record Empty, so moving spell, Empty, same spell makes the spell silent.
8. **Guards that never suppress anything:**
   - FF1 `announcedNoLearnersThisSession`;
   - FF2 magic target guard;
   - FF3 battle target index guard (reset before every check);
   - FF5 battle result `PointsAnnounced`, and the item and equip command-bar guards.
9. **Dead contexts.** FF3: `BattleResult.Data`, `JobMenu.Index`, `ConfigMenu.Slider`, `ConfigMenu.SliderController`,
   `NewGame.SlotName`, `Bestiary.DetailStat`. FF4: `ConfigMenu.TouchArrowValue`, `ConfigMenu.TouchSliderValue`,
   `Bestiary.DetailStat`.
10. **Contexts reset from outside their own screen** (FF4): `TitleMenu.Command` by the extras exits;
    `PartySetting.Select` shared by two lists; ally and enemy target contexts reset each other.
11. **Text compared where an identity was available.** FF1 status screen compares the character's name.
    FF4 `Shop.Item` compares the item name. FF2 magic bar and target compare text though the index is read.

## Proposal: unified context names

The core gets one deduplicator. Each check names a context and what it compares.
Names follow `Area.Thing`. They are grouped by the reset point that clears them.

Rules:

- **Prefer identity.** Compare an index, id or object where the game gives one. Use text only where nothing else exists.
- **Every context has a reset point.** "Never" is not allowed. The title screen resets everything.
- **Today's text checks move over as they are.** Replacing one with an identity check changes behaviour.
  The table marks these as "proposed"; each needs a decision during the port.
- **Gates are not deduplication.** Open-read pending flags and frame stamps belong to the focus-settle code.
  They are listed in the per-game tables, but get no context here.

### Reset on turn start (also battle start and end)

| Context | Compares | Replaces |
|---|---|---|
| `Battle.Turn` | object: the turn (actor plus turn number); proposed, today character id | `lastCharacterId` (1-3, 5), FF4 `Battle.Turn` |
| `Battle.Command` | index | FF1-3 `lastAnnouncedCmdMesId`, FF4 `BattleCommand.Select`, FF5 command cursor |
| `Battle.Target` | index plus side (FF5 model) | FF3 `BattleTarget.Player/Enemy`, FF4 `Battle.Target.Player/Enemy`, FF5 `_mode` + index |
| `Battle.TargetAll` | flag, "All allies" or "All enemies" | FF4 all-target flags |
| `Battle.ItemList` | index | FF3 `BattleItem.Select`, FF4 `BattleItem.Select`, FF5 item cursor |
| `Battle.SpellList` | index; proposed, today text in FF1, 3, 4 | FF1 battle magic tracker, FF3 `BattleMagic.Select`, FF4 `BattleAbility.Select`, FF5 ability cursor |

### Reset at battle start and battle end

| Context | Compares | Replaces |
|---|---|---|
| `Battle.Action` | object (the battle action) | FF2-5 action trackers, FF3-4 `BattleAction` |
| `Battle.Message` | text, one context per hook; proposed split | FF3 `Battle.Message`, FF4 `Battle.SetCommandMessage`, `Battle.CommandMessage`, FF5 message trackers |
| `Battle.StatusAdded` | unit plus condition id; proposed, today text in FF3, 4 | FF2 `_lastConditionByUnit`, FF3 status-added tracker, FF4 `Battle.ConditionAdd`, FF5 status set |
| `Battle.StatusRemoved` | unit plus condition id, per frame | status-removed trackers in all five |
| `Battle.Pause` | index | FF5 pause cursor |
| `Battle.Result.Points` | flag per result object | gil and EXP flags in all five |
| `Battle.Result.Items` | flag per result object | item-drop flags in all five |
| `Battle.Result.LevelUps` | set of characters per result object | level-up sets in all five |
| `Battle.Result.Skills` | flag per result object | FF2 weapon skills and spell levels, FF4 spells learned, FF5 abilities |

### Reset when a menu opens or closes

Each menu resets its own contexts through the menu-state registry. A list that is re-entered (an `Init` hook) resets its
list context too.

| Context | Compares | Games |
|---|---|---|
| `Menu.Cursor` | index plus menu | generic reader, all |
| `Title.Command` | index | all |
| `Item.Command`, `Item.List`, `Item.Target` | index | all |
| `Equip.Command`, `Equip.Slot`, `Equip.List`, `Equip.Character` | index | all |
| `Magic.Command`, `Magic.List`, `Magic.Target` | index; Empty slots by index, not -1 | 1 2 3 |
| `Ability.Command`, `Ability.List`, `Ability.Equip`, `Ability.Target` | index | 4 5 |
| `Job.List` | index | 3 5 |
| `Status.Character` | character id; proposed, today name in FF1 | all |
| `Status.Row` | index | all |
| `Config.Row` | index | all |
| `Config.Value` | row plus text; proposed, today text only | all |
| `Config.Slider` | row plus value | all |
| `Config.KeyBinding` | index | all |
| `Shop.Command`, `Shop.Item`, `Shop.Quantity` | index; quantity by value | all |
| `SaveLoad.Slot` | index | all |
| `Naming.Slot`, `Naming.Field`, `Naming.Suggestion` | index; suggestion by index plus text | all |
| `Words.List` | index | 2 |
| `Party.CharacterList`, `Party.SlotGrid` | index; proposed split of `PartySetting.Select` | 4 |
| `Bestiary.List`, `Bestiary.Minimap`, `Bestiary.Map`, `Bestiary.Formation` | index; also reset when each sub-view opens | all |
| `Gallery.List`, `MusicPlayer.List` | index; also reset on return from the detail view | all |
| `Puzzle.Tile` | index | 1 |
| `EventItem.List` | index | 3 |

### Reset when a popup opens or closes

| Context | Compares | Games |
|---|---|---|
| `Popup.Button` | index | all |
| `GameOver.Button`, `GameOver.Load` | index | all |
| `SaveLoad.Confirm` | index | all |

### Reset when the dialogue window closes or gets new content

| Context | Compares | Games |
|---|---|---|
| `Dialogue.Page` | index (page within the message) | all |
| `Dialogue.Speaker` | text | all |

### Reset on map change or save load

| Context | Compares | Games |
|---|---|---|
| `Map.Entering` | map id; also reset on save load | all |
| `Map.LocationBanner` | text, the banner that repeats "Entering X" | 1 2 3 5 |
| `System.Message` | text, fade and scroll messages outside battle | all |
| `Vehicle.State` | move state or transport id; also reset on getting off | all |
| `Vehicle.CanLand` | flag edge; also reset on getting on or off | all |

Not contexts: the F1 and F3 toggle checks (FF1-3 `GameToggleAnnouncer`) compare a setting before and after one call.
They keep no state between calls and need no deduplicator.

## Per-game inventory

The tables below come from the research pass, unchanged except for heading levels.

### FF1

Scattered trackers: 37 in 22 files.

| # | Declared | Variable | Context | Compares | Checked at | Reset |
|---|---|---|---|---|---|---|
| 1 | Patches/BattleCommandPatches.cs:29 | lastCharacterId | Battle "X's turn" | other: character id | Patches/BattleCommandPatches.cs:478 (SetCommandData_Postfix) | Patches/BattleCommandPatches.cs:964 ResetState, called only from Patches/BattleResultPatches.cs:629 ClearAllBattleStates (victory points screen, title main menu); not on flee or defeat |
| 2 | Patches/BattleCommandPatches.cs:41 | lastAnnouncedCmdMesId | Battle command menu cursor | other: command mesId string | Patches/BattleCommandPatches.cs:699 (AnnounceCommandAt, from SetCursor_Postfix and focus reader) | Patches/BattleCommandPatches.cs:453 each turn handoff (SetCommandData_Prefix); :662 confirmed cancel-return (DeferredCommandReannounce); :966 ResetState (victory, title) |
| 3 | Patches/BattleMagicPatches.cs:32 | _lastSpellAnnouncement | Battle magic list (settle re-fire) | text (full line incl. "(X of Y)"), only while list already active | Patches/BattleMagicPatches.cs:192 (AnnounceMagicFocus) | Patches/BattleMagicPatches.cs:548 ResetState via Patches/BattleResultPatches.cs:630 (victory points screen, title) |
| 4 | Patches/BattleMessagePatches.cs:738 | _spokenThisFrame (+ _dedupFrame :737) | Battle "X: status removed" | other: (unit ptr, condition id) set, per frame | Patches/BattleMessagePatches.cs:839-844 (BattleConditionRemovalPatches.RemoveFunctionPredicate_Postfix) | Patches/BattleMessagePatches.cs:841-842 every new frame; :795-796 battle start (OnBattleStart, Patches/BattlePatches.cs:146) |
| 5 | Patches/BattleResultPatches.cs:29 | announcedPoints | Victory gil/XP | other: bool once per battle | Patches/BattleResultPatches.cs:148 (ShowPointsInit_Postfix) | Patches/BattleResultPatches.cs:647 ResetState: battle start (Patches/BattleStartPatches.cs:88); battle end (Patches/BattleStartPatches.cs:102) |
| 6 | Patches/BattleResultPatches.cs:30 | announcedItems | Victory item drops | other: bool once per battle | Patches/BattleResultPatches.cs:378 (ShowGetItemsInit_Postfix) | Patches/BattleResultPatches.cs:648 battle start; battle end |
| 7 | Patches/BattleResultPatches.cs:31 | announcedLevelUps | Victory level-ups | other: HashSet of "name_levelup" keys | Patches/BattleResultPatches.cs:455 (ShowStatusUpInit_Postfix) | Patches/BattleResultPatches.cs:649 battle start; battle end |
| 8 | Patches/BestiaryPatches.cs:104 | lastSelectState | Bestiary minimap open/close | other: list selectState (state change) | Patches/BestiaryPatches.cs:743 (CheckSelectStateChange, 1 frame after KeyHelpSetup_Postfix) | Patches/BestiaryPatches.cs:108 (set to 0) via ClearState :94 on bestiary exit (:370 GotoTitle state; :938 config-bestiary exit) |
| 9 | Patches/CommandBarPatches.cs:52 | _lastItemIndex | Item command bar (Use/Sort/Key Items) | index | Patches/CommandBarPatches.cs:245 (TryReadItem) | Patches/CommandBarPatches.cs:143 item window close; :157 bar (re)entry (Item_CommandSelectInit_Postfix); :233 read outside command state |
| 10 | Patches/CommandBarPatches.cs:53 | _lastEquipIndex | Equip command bar | index | Patches/CommandBarPatches.cs:288 (TryReadEquip) | Patches/CommandBarPatches.cs:144 equip window close; :164 bar (re)entry; :275 read outside command state |
| 11 | Patches/CommandBarPatches.cs:54 | _lastMagicIndex | Magic command bar (Use/Forget) | index | Patches/CommandBarPatches.cs:323 (TryReadMagic) | Patches/CommandBarPatches.cs:150 bar (re)entry (Magic_CommandInit_Postfix); :308 read outside command state |
| 12 | Patches/ConfigMenuPatches.cs:331 | lastSliderPtr | Config slider newly focused (skip first value) | object (pointer) | Patches/ConfigMenuPatches.cs:360 (ConfigCommandController_SetSliderValue_Patch.Postfix) | Patches/ConfigMenuPatches.cs:383 ResetTracking: config close (:602); title Options close (:633) |
| 13 | Patches/ConfigMenuPatches.cs:332 | lastPercentage | Config slider value | text | Patches/ConfigMenuPatches.cs:369 (ConfigCommandController_SetSliderValue_Patch.Postfix) | Patches/ConfigMenuPatches.cs:384 config close; title Options close |
| 14 | Patches/ConfigMenuPatches.cs:465 | lastTouchController | Config slider value (Touch mode) | object | Patches/ConfigMenuPatches.cs:490 (ConfigActualDetailsTouch_SwitchSliderType_Patch.Postfix) | never |
| 15 | Patches/ConfigMenuPatches.cs:1170 | lastDropDownLabel | Language dropdown focus | text | Patches/ConfigMenuPatches.cs:1187 (SetDropDownItemFocus_Postfix) | Patches/ConfigMenuPatches.cs:1184 only when the hook fires while config menu is closed (not on dropdown close) |
| 16 | Patches/EquipMenuPatches.cs:51 | EquipMenuState.LastTargetIndex | Equip menu character switch (name, job) | index (party target) | Patches/EquipMenuPatches.cs:302 (EquipmentInfoWindowController_SelectContent_Patch.Postfix) | Patches/EquipMenuPatches.cs:58 EQUIP_MENU reset handler: equip window close (:367); another menu activates; map transition |
| 17 | Patches/MagicMenuPatches.cs:415 | _lastSpellAnnouncement | Field magic spell list | text (full line incl. "(X of Y)") | Patches/MagicMenuPatches.cs:474 (AnnounceSpell) | Patches/MagicMenuPatches.cs:319 spell-list (re)entry (TryAnnounceInitialSpell); not on menu close or Empty slot |
| 18 | Patches/MapTransitionPatches.cs:28 | lastAnnouncedMapId | Map "Entering X" | other: map id | Patches/MapTransitionPatches.cs:236 (CheckMapTransition, from ChangeState_Postfix) | never (deliberate, comment at :266) |
| 19 | Patches/MessageWindowPatches.cs:25 | lastAnnouncedPageIndex | Dialogue page | index (page) | Patches/MessageWindowPatches.cs:372 (PlayingInit_Postfix) | Patches/MessageWindowPatches.cs:272 new message content (SetContent_Postfix); :452 window close (ClearState from Close_Postfix :408) |
| 20 | Patches/MessageWindowPatches.cs:29 | lastAnnouncedSpeaker | Dialogue speaker prefix | text | Patches/MessageWindowPatches.cs:384 (PlayingInit_Postfix) | Patches/MessageWindowPatches.cs:454 window close only |
| 21 | Patches/NewGamePatches.cs:32 | lastSlotNames | New game slot name change | text (per slot) | Patches/NewGamePatches.cs:365 (UpdateView_Postfix) | Patches/NewGamePatches.cs:275-276 character select opens (InitSelect_Postfix) |
| 22 | Patches/NewGamePatches.cs:35 | lastCharacterIndex | New game grid: Light Warrior | index | Patches/NewGamePatches.cs:223 (AnnounceFieldAfterDelay) | Patches/NewGamePatches.cs:196 cursor on Done; :272 character select opens |
| 23 | Patches/NewGamePatches.cs:36 | lastFieldType | New game grid: Name/Class field | other: field kind (name or class) | Patches/NewGamePatches.cs:224 (AnnounceFieldAfterDelay) | Patches/NewGamePatches.cs:197 cursor on Done; :273 character select opens |
| 24 | Patches/PopupPatches.cs:44 | lastCommonPopupCursorIndex | Common popup button | index | Patches/PopupPatches.cs:400 (CommonPopup_FocusChanged_Postfix) | Patches/PopupPatches.cs:589 CommonPopup open (PopupOpen_Postfix); :736 popup close; primed at :721 by open-read |
| 25 | Patches/PopupPatches.cs:45 | lastGameOverSelectCursorIndex | Game-over select popup button | index | Patches/PopupPatches.cs:474 (ReadGameOverButton, via GameOverSelectPopup_FocusChanged_Postfix :445 and retry :510) | Patches/PopupPatches.cs:737 popup close |
| 26 | Patches/PopupPatches.cs:46 | lastGameOverLoadCursorIndex | Game-over load popup button | index | Patches/PopupPatches.cs:474 (ReadGameOverButton, via GameOverLoadPopup_FocusChanged_Postfix :457 and retry :509) | Patches/PopupPatches.cs:738 popup close |
| 27 | Patches/PuzzlePatches.cs:36 | lastAnnouncedCursorPos | Sliding puzzle tile | index (board cell) | Patches/PuzzlePatches.cs:148 (AnnounceTile) | Patches/PuzzlePatches.cs:85 puzzle open (Show_Postfix); :230 puzzle close (PUZZLE_GAME reset handler) |
| 28 | Patches/ScrollMessagePatches.cs:18 | lastScrollMessage | Fade, scroll and line-fade system messages (one shared slot) | text | Patches/ScrollMessagePatches.cs:126 (FadeManagerPlay_Postfix); :197 (LineFadeWindowController_SetData_Postfix); :233 (ScrollManagerPlay_Postfix) | never |
| 29 | Patches/ShopMagicPatches.cs:30 | lastAnnouncedCharacterId | Magic shop slot grid: character name prefix | other: character id | Patches/ShopMagicPatches.cs:203 (MagicSelectContent_Postfix) | Patches/ShopMagicPatches.cs:120 grid focus (MagicTargetSetFocus_Postfix); :142 grid open; :302 ResetState: back to item list (Patches/ShopPatches.cs:243); shop close (Patches/ShopPatches.cs:688) |
| 30 | Patches/ShopMagicPatches.cs:33 | announcedNoLearnersThisSession | Magic shop "No characters can learn" | other: bool flag | Patches/ShopMagicPatches.cs:151 (MagicTargetShow_Postfix) | Patches/ShopMagicPatches.cs:143 same method just before the check (so it never suppresses); :303 ResetState |
| 31 | Patches/ShopPatches.cs:291 | _lastAnnouncedListIndex | Shop buy/sell item list | index | Patches/ShopPatches.cs:264 (AnnounceFocusedFromList, from SetDescription_Postfix) | Patches/ShopPatches.cs:216 shop state leaves buy/sell list; :568 quantity window opens; :676 shop close |
| 32 | Menus/StatusStatDefinitions.cs:53 | StatusNavigationTracker.CurrentCharacterData | Status details character switch | text (character Name) | Patches/StatusMenuPatches.cs:748 (DelayedPlayerSwitchInit, from SetNextPlayer/SetPrevPlayer postfix) | Menus/StatusStatDefinitions.cs:65 Reset via Patches/StatusMenuPatches.cs:622 status details exit (ExitDisplay_Postfix); overwritten on open :590 |
| 33 | Patches/VehicleLandingPatches.cs:27 | lastLandableState | Vehicle "Can land" | other: previous landable bool (false to true edge) | Patches/VehicleLandingPatches.cs:105 (SwitchLandable_Postfix) | never (ResetState at :123 has no callers) |
| 34 | Utils/LocationMessageTracker.cs:12 | lastMapTransitionMessage (+ inMapTransition :13) | Fade location name vs "Entering X" | text (substring) | Utils/LocationMessageTracker.cs:39-43 (ShouldAnnounceFadeMessage, from Patches/ScrollMessagePatches.cs:142); set at Patches/MapTransitionPatches.cs:255 | Utils/LocationMessageTracker.cs:88 Reset via Core/FFI_ScreenReaderMod.cs:353 every scene load |
| 35 | Core/Handlers/GameToggleAnnouncer.cs:29 | _encounterBefore | Encounters on/off toggle | other: setting value before vs after call | Core/Handlers/GameToggleAnnouncer.cs:79 (SetIsEnableEncount_Postfix) | Core/Handlers/GameToggleAnnouncer.cs:63 each prefix; :78 each postfix (single-call scope) |
| 36 | Core/Handlers/GameToggleAnnouncer.cs:30 | _autoDashBefore | Walk/Run toggle | other: setting value before vs after call | Core/Handlers/GameToggleAnnouncer.cs:108 (SetIsAutoDash_Postfix) | Core/Handlers/GameToggleAnnouncer.cs:92 each prefix; :107 each postfix (single-call scope) |
| 37 | Utils/MoveStateHelper.cs:33 | cachedMoveState | "On foot" on GetOn(Player) | other: cached move state (IsOnFoot) | Patches/VehicleLandingPatches.cs:269 (MovementSpeechPatches.GetOn_Postfix) | never (ResetState at Utils/MoveStateHelper.cs:222 has no callers); overwritten by GetOn/GetOff |

Excluded (not speech):
- Core/AudioLoopManager.cs:42 `lastBeaconTarget`: beacon target change, restarts beacon timing (audio).
- Core/AudioLoopManager.cs:49 `lastBeaconPlayedAt`: beacon ping interval (audio).
- Patches/MovementSoundPatches.cs:23 `lastBumpTime`: wall-bump sound cooldown (audio).
- Patches/MovementSoundPatches.cs:29 `lastCollisionPos`: same-spot collision count before a bump sound (audio).
- Patches/MovementSoundPatches.cs:43 `lastTilePosition`: footstep on tile change (audio).
- Utils/SoundPlayer.cs:33 `currentWallDirectionsMask`: wall-tone directions newly on/off (audio).
- Utils/SoundPlayer.cs:34 `lastWallToneVolume`: wall-tone volume change (audio).
- Core/EntityNavigationManager.cs:16 `lastScannedMapId`: full entity rescan on map change (no speech).
- Field/EntityScanner.cs:30 `lastScannedMapId`: same, scanner side (no speech).
- Core/InputManager.cs:288 `lastFieldScanFrame`: FindObjectOfType throttle (no speech).
- Core/FFI_ScreenReaderMod.cs:294 `previousSceneName`: battle-scene exit detection for state clearing (no speech).
- Core/NavigationTargetTracker.cs:12 `LastKind`: routes beacon/trigger to entity or waypoint (no speech).
- Field/EntityDetectionHelpers.cs:332 `loggedUnclassifiedKeys`: log-once set (log only).
- Utils/EntityTranslator.cs:27 `untranslatedNamesByMap`: untranslated-name logging (log only).
- Patches/BattlePatches.cs:33 `LastSelectedCommandIndex`: which command opened the magic/item list; drops stale callbacks (context gate, not a repeat check).
- Patches/NewGamePatches.cs:29 `lastTargetIndex`: fallback slot for UpdateView; lastSlotNames does the repeat check.
- Caches read by on-demand detail keys, never compared: Patches/BattleMagicPatches.cs:421 and Patches/BattleItemPatches.cs:279 `LastFocusedDescription`; Patches/ItemMenuPatches.cs:49 `LastSelectedItem`; Patches/MagicMenuState.cs:25 `_lastAnnouncedAbility`; Patches/ShopMenuTracker.cs:30-33 `LastItemName/LastItemPrice/LastItemContentId/LastItemContentType`; Patches/JobSelectionPatches.cs:31 `_lastController`; Patches/BestiaryPatches.cs:71 `CachedEntryName`.
- Write-only (never read): Utils/MoveStateHelper.cs:35 `lastAnnouncedState` (set only in AnnounceStateChange, which has no callers); Patches/BestiaryPatches.cs:863 `_previousState`.
- Ordering and arming one-shots (sequence an entry read or re-announce; no stored value is compared): Patches/BestiaryPatches.cs:69 `SuppressNextListEntry`, :459 `_formationAnnouncePending`; Patches/GalleryPatches.cs:23 `SuppressContentChange`, :25 `PreviousState`, :121 `_entryHeaderSpoken`; Patches/MusicPlayerPatches.cs:23 `SuppressContentChange`, :140 `_entryHeaderSpoken`, :141 `_entryPending`; Patches/PopupPatches.cs:53 `_suppressNextCommonFocus`; Patches/TitleScreenPatches.cs:36 `_pressPromptArmed` (+ :38, :40); Patches/BattleCommandPatches.cs:34 `commandTurnReady`, :54 `commandReannouncePending`; Patches/ConfigMenuPatches.cs:203 `_initialFocusPending`, :522 `_pendingConfigReannounce`; Patches/BattleMessagePatches.cs:734 `_battleEndCleanup`.
- Ownership flags (stop the generic cursor reader double-reading): Patches/NewGamePatches.cs:48 and Patches/JobSelectionPatches.cs:25 `IsHandlingCursor`; Patches/PopupState.cs:41 `HasOwnFocusReader`.
- Generation counters (supersede pending reads, no stored value is compared): Patches/BattleCommandPatches.cs:58, :62, :342; Patches/BattleMagicPatches.cs:38; Patches/CommandBarPatches.cs:60-62; Patches/ConfigMenuPatches.cs:523; Patches/FieldMenuPatches.cs:34; Patches/ItemMenuPatches.cs:438-439; Patches/MagicMenuPatches.cs:32.

### FF2

Scattered trackers: 43 in 15 files.

No AnnouncementDeduplicator class exists (only comments saying local guards replaced "the central deduplicator": Patches/ShopPatches.cs:289, Patches/NewGameNamingPatches.cs:50, Utils/MenuStateHelper.cs:7). "Battle end" below = FFII_ScreenReaderMod.ClearBattleState (Core/FFII_ScreenReaderMod.cs:1049: result screen, battle-exit callbacks, field return, scene load in battle, Tab self-heal). "X state cleared" = MenuStateRegistry reset handler firing on SetActive(key,false): window SetActive(false), another menu taking exclusive state, map-transition flag clear, title ResetAll.

| # | Declared | Variable | Context | Compares | Checked at | Reset |
|---|---|---|---|---|---|---|
| 1 | Patches/BattleCommandPatches.cs:119 | lastCharacterId | Battle "X's turn" | other: character id | Patches/BattleCommandPatches.cs:173 (SetCommandData_Postfix) | Patches/BattleCommandPatches.cs:192 battle end (ResetTurnState) |
| 2 | Patches/BattleCommandPatches.cs:128 | lastAnnouncedCmdMesId | Battle command cursor | other: command MesIdName | Patches/BattleCommandPatches.cs:283 (AnnounceCommandAt, from SetCursor_Postfix / DeferredCommandReannounce) | Patches/BattleCommandPatches.cs:152 every turn (SetCommandData_Prefix); Patches/BattleCommandPatches.cs:194 battle end; Patches/BattleCommandPatches.cs:264 confirmed cancel-return (DeferredCommandReannounce) |
| 3 | Patches/BattleCommandPatches.cs:317 | defaultTargetAnnounced | Battle initial target read | other: bool already-announced | Patches/BattleCommandPatches.cs:405 (DelayedInitialTarget) | Patches/BattleCommandPatches.cs:166 every turn (SetCommandData_Postfix); Patches/BattleCommandPatches.cs:386 target state entry (TargetInit_Prefix); Patches/BattleCommandPatches.cs:196 battle end |
| 4 | Patches/BattleMessagePatches.cs:637 | _lastConditionByUnit | Battle "X: status" added | other: text per unit pointer (Dictionary) | Patches/BattleMessagePatches.cs:754 (ConditionAdd_Postfix) | Patches/BattleMessagePatches.cs:655 battle end (ResetConditionDedup); Patches/BattleMessagePatches.cs:893 that condition removed (UpdateTempParamter_Postfix) |
| 5 | Patches/BattleMessagePatches.cs:639 | _lastActDataPtr | Battle "Actor: Action" | object (native BattleActData ptr) | Patches/BattleMessagePatches.cs:403 (CreateActFunction_Postfix) | Patches/BattleMessagePatches.cs:656 battle end (ResetConditionDedup) |
| 6 | Patches/BattleMessagePatches.cs:650 | _lastMessageText | Battle message line | text (with frame window #7) | Patches/BattleMessagePatches.cs:332 (SetMessage_Postfix) | Patches/BattleMessagePatches.cs:1043 battle end (ResetState) |
| 7 | Patches/BattleMessagePatches.cs:651 | _lastMessageFrame | Battle message line | other: frame window < 20, refreshed on each swallowed repeat | Patches/BattleMessagePatches.cs:332 (SetMessage_Postfix) | Patches/BattleMessagePatches.cs:1044 battle end (ResetState) |
| 8 | Patches/BattleMessagePatches.cs:800 | _lastRemovalUnit | Battle "X: status removed" | other: (unit ptr, id, frame) tuple with #9-#10 | Patches/BattleMessagePatches.cs:899 (UpdateTempParamter_Postfix) | Patches/BattleMessagePatches.cs:658 battle end (ResetConditionDedup) |
| 9 | Patches/BattleMessagePatches.cs:801 | _lastRemovalId | Battle "X: status removed" | other: condition id (tuple) | Patches/BattleMessagePatches.cs:899 (UpdateTempParamter_Postfix) | Patches/BattleMessagePatches.cs:659 battle end (ResetConditionDedup) |
| 10 | Patches/BattleMessagePatches.cs:802 | _lastRemovalFrame | Battle "X: status removed" | other: same-frame only (tuple) | Patches/BattleMessagePatches.cs:899 (UpdateTempParamter_Postfix) | Patches/BattleMessagePatches.cs:660 battle end (ResetConditionDedup) |
| 11 | Patches/BattleResultPatches.cs:44 | lastResultPtr | Victory screen; gates reset of #12-#16 | object (BattleResultData ptr) | Patches/BattleResultPatches.cs:91 (ResetTracking, from Show/ShowPointList/ShowSkillLevels/ShowGetItemsInit postfixes) | Patches/BattleResultPatches.cs:84 battle start (ResetForNewBattle from Patches/BattleMessagePatches.cs:953) |
| 12 | Patches/BattleResultPatches.cs:45 | announcedGil | Victory gil | other: bool already-announced | Patches/BattleResultPatches.cs:153 (AnnounceGil) | Patches/BattleResultPatches.cs:93 new result object (ResetTracking) |
| 13 | Patches/BattleResultPatches.cs:46 | announcedSkillLevels | Victory weapon skills | other: bool already-announced | Patches/BattleResultPatches.cs:187 (ShowSkillLevels_Postfix) | Patches/BattleResultPatches.cs:94 new result object (ResetTracking) |
| 14 | Patches/BattleResultPatches.cs:47 | announcedItems | Victory item drops | other: bool already-announced | Patches/BattleResultPatches.cs:376 (ShowGetItemsInit_Postfix) | Patches/BattleResultPatches.cs:95 new result object (ResetTracking) |
| 15 | Patches/BattleResultPatches.cs:48 | announcedSpellLevels | Victory spell level-ups | other: bool already-announced | Patches/BattleResultPatches.cs:429 (SetLevelUpList_Postfix; does not call ResetTracking itself) | Patches/BattleResultPatches.cs:96 new result object (ResetTracking) |
| 16 | Patches/BattleResultPatches.cs:50 | announcedStatusUps | Victory stat-gain page per character | other: HashSet of character-result ptrs | Patches/BattleResultPatches.cs:308 (StatusUpSetData_Postfix) | Patches/BattleResultPatches.cs:98 new result object (ResetTracking) |
| 17 | Patches/ConfigMenuPatches.cs:33 | lastAnnouncedText | Config row "Name: Value" | text | Patches/ConfigMenuPatches.cs:63 (ConfigMenuState.ShouldAnnounce, from AnnounceCommand Patches/ConfigMenuPatches.cs:667) | Patches/ConfigMenuPatches.cs:52 ClearDedup: config state cleared (handler Patches/ConfigMenuPatches.cs:38; close Core/FFII_ScreenReaderMod.cs:1376; title Patches/TitleMenuPatches.cs:56); Patches/ConfigMenuPatches.cs:820 title Configuration open; Patches/ConfigMenuPatches.cs:841 language screen open; Patches/BestiaryPatches.cs:781 config bestiary entry |
| 18 | Patches/ConfigMenuPatches.cs:34 | lastAnnouncedSettingName | Config row, value-only change muted | text (name before ": ") | Patches/ConfigMenuPatches.cs:70 (ShouldAnnounce) | Patches/ConfigMenuPatches.cs:53 ClearDedup (same triggers as #17) |
| 19 | Patches/ConfigMenuPatches.cs:250 | lastArrowRow | Config arrow toggle value | object (row native ptr), paired with #20 | Patches/ConfigMenuPatches.cs:883 (SwitchArrowSelectType_Postfix) | never |
| 20 | Patches/ConfigMenuPatches.cs:251 | lastArrowValue | Config arrow toggle value | text | Patches/ConfigMenuPatches.cs:883 (SwitchArrowSelectType_Postfix) | never |
| 21 | Patches/ConfigMenuPatches.cs:252 | lastSliderPercentage | Config slider value | text (percentage) | Patches/ConfigMenuPatches.cs:919 (SwitchSliderType_Postfix) | never |
| 22 | Patches/ConfigMenuPatches.cs:253 | lastController | Config slider row; first event on a new row is silent | object (managed wrapper ref, not .Pointer) | Patches/ConfigMenuPatches.cs:919; Patches/ConfigMenuPatches.cs:923 (SwitchSliderType_Postfix) | never |
| 23 | Patches/GameStatePatches.cs:20 | lastAnnouncedMapId | "Entering {map}" | other: map id | Patches/GameStatePatches.cs:115 (CheckMapTransition, from ChangeState_Postfix) | never |
| 24 | Patches/KeywordPatches.cs:95 | _lastIndex | Words menu keyword cursor | index | Patches/KeywordPatches.cs:115 (WordsMenuState.IsNewIndex, from WordsSetCommandSelectCursor_Postfix :393, AnnounceWordsFirstKeyword_Touch :439, WordsSetSelectContent_Touch_Postfix :466) | Patches/KeywordPatches.cs:101 words state cleared (close Core/FFII_ScreenReaderMod.cs:1401) |
| 25 | Patches/MagicMenuPatches.cs:44 | lastSpellId | Field magic spell list cursor | other: spell id (-1 = Empty) | Patches/MagicMenuPatches.cs:202 (ShouldAnnounceSpell, from AnnounceSpell :911 and AnnounceEmpty :887) | Patches/MagicMenuPatches.cs:56 magic state cleared (close Core/FFII_ScreenReaderMod.cs:1368); Patches/MagicMenuPatches.cs:88 list entry (OnSpellListFocused from ListInit_Prefix); Patches/MagicMenuPatches.cs:94 sort state (OnSpellListUnfocused from SelfOrderlyInit_Postfix) |
| 26 | Patches/MagicMenuPatches.cs:45 | lastTargetAnnouncement | Field magic target cursor | text (name, HP, MP, status) | Patches/MagicMenuPatches.cs:210 (ShouldAnnounceTarget, from TargetSetCursor_Postfix :825) | Patches/MagicMenuPatches.cs:57 magic state cleared; Patches/MagicMenuPatches.cs:103 OnTargetSelectionActive, called at Patches/MagicMenuPatches.cs:828 right after every spoken target; Patches/MagicMenuPatches.cs:109 OnTargetSelectionInactive (no callers) |
| 27 | Patches/MagicMenuPatches.cs:46 | lastCommandAnnouncement | Field magic Use/Forget bar | text (command name) | Patches/MagicMenuPatches.cs:138 (ShouldAnnounceCommand, from TryAnnounceFocusedCommand :612) | Patches/MagicMenuPatches.cs:58 magic state cleared; Patches/MagicMenuPatches.cs:118 command-bar entry (OnCommandMenuActive from CommandInit_Prefix :480 / CommandSelectCursor_Postfix :543); Patches/MagicMenuPatches.cs:124 OnCommandMenuInactive (no callers) |
| 28 | Patches/MessageWindowPatches.cs:31 | lastAnnouncedPageIndex | Dialogue page | index | Patches/MessageWindowPatches.cs:147 (DialogueTracker.AnnounceForPage, from PlayingInit_Postfix :555) | Patches/MessageWindowPatches.cs:86 new content (StoreMessages from SetContent_Postfix); Patches/MessageWindowPatches.cs:228 window close (Reset from Close_Postfix :568) |
| 29 | Patches/MessageWindowPatches.cs:35 | lastAnnouncedSpeaker | Dialogue speaker prefix | text | Patches/MessageWindowPatches.cs:159 (AnnounceForPage) | Patches/MessageWindowPatches.cs:230 window close (Reset); Patches/MessageWindowPatches.cs:240 line-fade text set (ClearLastAnnouncedSpeaker from Patches/ScrollMessagePatches.cs:330) |
| 30 | Patches/MovementSpeechPatches.cs:29 | lastTransportationId | Vehicle boarding / "On foot" | other: previous transport id (edge) | Patches/MovementSpeechPatches.cs:108 (ChangeTransportation_Postfix) | never (ResetState Patches/MovementSpeechPatches.cs:355 and SyncToOnFoot :365 have no callers) |
| 31 | Patches/MovementSpeechPatches.cs:30 | lastAnnouncedTransportId | Vehicle boarding / "On foot" | other: transport id | Patches/MovementSpeechPatches.cs:105 (ChangeTransportation_Postfix); Patches/MovementSpeechPatches.cs:272; Patches/MovementSpeechPatches.cs:283 (GetOn_Postfix); Patches/MovementSpeechPatches.cs:310 (GetOff_Postfix) | never (Patches/MovementSpeechPatches.cs:356 and :366 are in uncalled methods) |
| 32 | Patches/NewGameNamingPatches.cs:52 | _lastNameAnnounced | Naming suggested-name cycle | text | Patches/NewGameNamingPatches.cs:454 (SetFocus_Postfix) | Patches/NewGameNamingPatches.cs:205 character-select entry (InitSelect_Postfix); Patches/NewGameNamingPatches.cs:402 name-select entry (InitNameSelect_Postfix, re-seeded :424) |
| 33 | Patches/NewGameNamingPatches.cs:53 | _lastAutoIndex | Naming suggested-name cycle | index | Patches/NewGameNamingPatches.cs:441 (SetFocus_Postfix) | Patches/NewGameNamingPatches.cs:206 InitSelect_Postfix; Patches/NewGameNamingPatches.cs:403 InitNameSelect_Postfix (re-seeded :596) |
| 34 | Patches/NewGameNamingPatches.cs:59 | lastSlotNames | Naming slot name changed | text per slot (string[4]) | Patches/NewGameNamingPatches.cs:484 (UpdateView_Postfix) | Patches/NewGameNamingPatches.cs:209 character-select entry (InitSelect_Postfix) |
| 35 | Patches/NewGameNamingPatches.cs:60 | lastTargetIndex | Naming character slot cursor | index | Patches/NewGameNamingPatches.cs:289 (SetTargetSelectContent_Postfix) | Patches/NewGameNamingPatches.cs:210 character-select entry (InitSelect_Postfix) |
| 36 | Patches/PopupPatches.cs:71 | LastButtonIndex | Popup buttons (Yes/No) | index | Patches/PopupPatches.cs:470 (ReadCurrentButton); Patches/PopupPatches.cs:765 (DelayedGameOverSelectReturn, >= 0 means spoken) | Patches/PopupPatches.cs:80 popup open (PopupState.SetActive); Patches/PopupPatches.cs:53 popup state cleared (PopupClose_Postfix :688, Patches/SaveLoadPatches.cs:473, title ResetAll :940) |
| 37 | Patches/ScrollMessagePatches.cs:108 | lastScrollMessage | Fade and scroll messages (one shared slot) | text (raw message) | Patches/ScrollMessagePatches.cs:232 (FadeManagerPlay_Postfix); Patches/ScrollMessagePatches.cs:272 (ScrollManagerPlay_Postfix) | never |
| 38 | Patches/ShopPatches.cs:292 | _lastAnnouncedListIndex | Shop buy/sell item cursor | index | Patches/ShopPatches.cs:364 (AnnounceFocusedFromList, from SetDescription_Postfix) | Patches/ShopPatches.cs:298 shop state cleared (ResetGuards via handler :51; close Core/FFII_ScreenReaderMod.cs:1385; auto-clear :77/:84); Patches/ShopPatches.cs:322 non-list shop state; Patches/ShopPatches.cs:520 command bar focused |
| 39 | Patches/ShopPatches.cs:293 | _lastCommandIndex | Shop Buy/Sell command bar | index | Patches/ShopPatches.cs:536 (CommandSetCursor_Postfix) | Patches/ShopPatches.cs:299 shop state cleared; Patches/ShopPatches.cs:346 item list entered (AnnounceFocusedFromList) |
| 40 | Patches/ShopPatches.cs:294 | _lastQuantity | Shop trade quantity | other: quantity value | Patches/ShopPatches.cs:707 (AnnounceQuantity, from TradeUpdateArrowImage_Postfix / DeferredOpeningQuantity) | Patches/ShopPatches.cs:300 shop state cleared; Patches/ShopPatches.cs:347 item list entered; Patches/ShopPatches.cs:681 trade window open (DeferredOpeningQuantity) |
| 41 | Patches/VehicleLandingPatches.cs:24 | lastLandableState | "Can land" | other: bool previous landable (false to true edge) | Patches/VehicleLandingPatches.cs:92 (SwitchLandable_Postfix) | Patches/VehicleLandingPatches.cs:110 scene load (ResetState from Core/FFII_ScreenReaderMod.cs:353 OnSceneLoaded) |
| 42 | Utils/LocationMessageTracker.cs:13 | lastMapTransitionMessage | Fade location name vs "Entering X" | text (case-insensitive containment) | Utils/LocationMessageTracker.cs:44 (ShouldAnnounceFadeMessage, from Patches/ScrollMessagePatches.cs:244); set at Patches/GameStatePatches.cs:141 | Utils/LocationMessageTracker.cs:89 scene load (Reset from Core/FFII_ScreenReaderMod.cs:356 OnSceneLoaded) |
| 43 | Utils/LocationMessageTracker.cs:14 | inMapTransition | Fade location-name filter | other: bool, "Entering X" spoken since scene load | Utils/LocationMessageTracker.cs:40; Utils/LocationMessageTracker.cs:49 (ShouldAnnounceFadeMessage) | Utils/LocationMessageTracker.cs:90 scene load (Reset from Core/FFII_ScreenReaderMod.cs:356) |

Excluded (not speech):
- Core/AudioLoopManager.cs:42 `beaconSilenced`: beacon silence latch (audio)
- Core/AudioLoopManager.cs:43 `lastBeaconTarget`: a new beacon target clears the silence latch (audio)
- Core/AudioLoopManager.cs:48 `wallToneSuppressedUntil`: wall-tone mute window after scene load (audio)
- Core/AudioLoopManager.cs:51 `beaconSuppressedUntil`: beacon mute window after scene load (audio)
- Core/AudioLoopManager.cs:52 `lastBeaconPlayedAt`: beacon ping timing (audio)
- Patches/MovementSoundPatches.cs:25 `lastBumpTime`: wall-bump sound cooldown
- Patches/MovementSoundPatches.cs:31 `lastCollisionPos`: wall-bump same-position check (sound)
- Patches/MovementSoundPatches.cs:45 `lastTilePosition`: footstep once per tile
- Utils/SoundPlayer.cs:32 `currentWallDirectionsMask`: wall-tone direction streams
- Utils/SoundPlayer.cs:33 `lastWallToneVolume`: wall-tone volume
- Field/EntityScanner.cs:38 `lastScannedMapId`: forces entity rescan on map change (no speech gate)
- Core/GamepadManager.cs:34 `btnPrevious` (also :66, :76, :100): input edge detection
- Patches/BestiaryPatches.cs:69 `SuppressNextListEntry`: speech-adjacent bracket, not a last-value guard (open read speaks the focused entry instead of the hook)
- Patches/GalleryPatches.cs:22 and Patches/MusicPlayerPatches.cs:22 `SuppressContentChange`: speech-adjacent entry / list-swap brackets, same pattern
- Patches/GalleryPatches.cs:26 `PreviousState`: state machine; "Gallery" title only on first entry
- Patches/BestiaryPatches.cs:764 `_previousState`: written, never read
- Core/Handlers/GameToggleAnnouncer.cs:23 `autoDashBefore` and :26 `encounterBefore`: same-call prefix/postfix change check, not memory across events
- Patches/BattleCommandPatches.cs:133 `commandReannouncePending`, Patches/ConfigMenuPatches.cs:261 `_pendingConfigReannounce`, Patches/PopupPatches.cs:857 `isTitleScreenTextPending`, Patches/BestiaryPatches.cs:609 `_pending`, Patches/ItemMenuPatches.cs:472 and :474 `_pendingItemList` / `_pendingItemTarget`: one-shot arm flags for deferred reads
- Patches/BattleMessagePatches.cs:797 `_battleEnding`: mutes "removed" lines during battle-end cleanup
- Patches/BattleMessagePatches.cs:792 `_trackedConditions`: source data for "X removed" lines, not a dedup
- Patches/ShopPatches.cs:101 to :104 `LastItem*`, Core/MenuDetailCache.cs:13 `LastDetail`, Patches/ItemMenuPatches.cs:138 `LastSelectedItem` (written, never read): detail-key caches
- Patches/BattleResultPatches.cs:49 `spellLevelUpCalls`: diagnostic log counter

### FF3

Deduplicator calls: 36 (ShouldAnnounce 29, AnnounceIfNew 7, GetLast* 0); distinct contexts: 28. Reset calls: 21; ResetAll at: none. `AnnouncementDeduplicator.ResetAll` has no callers. The only ResetAll is `MenuStateRegistry.ResetAll` at Patches/TitleScreenPatches.cs:215, which runs when the title main menu is shown and fires every registered MenuStateHelper reset handler.

Notes on counting and shorthand:
- The 29 ShouldAnnounce calls are 19 direct calls plus 10 calls through MenuStateHelper wrappers (`XState.ShouldAnnounce` leads to Utils/MenuStateHelper.cs:65 for text and :75 for index).
- Plumbing calls are not rows. They are Utils/MenuStateHelper.cs:65 and :75, plus 7 one-line forwarders: Patches/BattleItemPatches.cs:82, Patches/BattleMagicPatches.cs:90, Patches/EquipMenuPatches.cs:65, Patches/ItemMenuPatches.cs:116, Patches/JobMenuPatches.cs:58, :59 and Patches/StatusMenuPatches.cs:44.
- 36 call sites + 9 plumbing calls + 6 overload definitions in AnnouncementDeduplicator.cs = 51. That matches the research note's count.
- The index+text overloads, `ShouldAnnounce(ctx,int,string)` and `AnnounceIfNew(ctx,int,text)`, have no callers.
- Four contexts are declared and reset but never checked: ConfigMenu.Slider, ConfigMenu.SliderController, NewGame.SlotName and Bestiary.DetailStat.
- "Registry reset" means the handler at Utils/MenuStateHelper.cs:36. It fires when the menu's MenuStateRegistry key goes inactive. That happens in four ways:
  - the menu's own close or back-out;
  - `SetActiveExclusive` of another active menu (Utils/MenuStateRegistry.cs:71);
  - a map change (Core/FFIII_ScreenReaderMod.cs:720, called from Patches/GameStatePatches.cs:173). This covers menu keys only, not battle keys;
  - returning to the title (Patches/TitleScreenPatches.cs:215).
- In the Reset and Checked cells, a bare `:N` means the row's own file, or the file named just before it in that cell.

| # | Call site | Method | Context string | Compares | What is announced | Reset |
|---|---|---|---|---|---|---|
| 1 | Patches/BattleCommandPatches.cs:625 | ShouldAnnounce | BattleTarget.Player | index | Battle target, party member: name, HP, position | :372 on a new actor's turn (:126) and in every SelectContent prefix (:797 via :571); :414 on target state Init (EnemysInit/PlayerInit); :688 when an enemy target is spoken; registry reset when BATTLE_TARGET goes inactive (target window hidden :581, controller gone :532/:551/:606/:615, battle end Patches/BattleResultPatches.cs:152, field state Patches/GameStatePatches.cs:213, command cursor exclusive Patches/BattleCommandPatches.cs:235, title) |
| 2 | Patches/BattleCommandPatches.cs:687 | ShouldAnnounce | BattleTarget.Enemy | index | Battle target, enemy: name, HP, position | same as #1; :626 when a party target is spoken |
| 3 | Patches/BattleMessagePatches.cs:39 | ShouldAnnounce | Battle.Message | text | Battle and system message text through GlobalBattleMessageTracker.TryAnnounce, from 6 hooks: Patches/BattleStartPatches.cs:168; Patches/BattleSystemMessagePatches.cs:132, :167, :213, :259, :287 | Patches/BattleMessagePatches.cs:73 at battle start (Patches/BattleStatePatches.cs:33) |
| 4 | Patches/BattleMessagePatches.cs:132 | ShouldAnnounce | BattleAction | object | "Actor: Action" per BattleActData (CreateActFunction) | Patches/BattleStatePatches.cs:34 at battle start |
| 5 | Patches/BattleResultPatches.cs:130 | ShouldAnnounce | BattleResult.Data | object (result ignored) | Nothing. The context is write-only; the announcedPoints flag does the gating | never |
| 6 | Patches/ConfigMenuPatches.cs:386 | ShouldAnnounce | ConfigMenu.Arrow | text | KeyInput config toggle value after left/right | registry reset when CONFIG_MENU goes inactive: config close :852, title Options close :879, config UI gone :66, scene load Core/FFIII_ScreenReaderMod.cs:275, map change, another menu exclusive, title |
| 7 | Patches/ConfigMenuPatches.cs:554 | ShouldAnnounce | ConfigMenu.TouchArrow | text | Touch config arrow value | same as #6 |
| 8 | Patches/ConfigMenuPatches.cs:623 | ShouldAnnounce | ConfigMenu.TouchSliderController | object | Detects a new touch slider row. The first change on a new row is silent | same as #6 |
| 9 | Patches/ConfigMenuPatches.cs:626 | ShouldAnnounce (seed) | ConfigMenu.TouchSlider | text | Seeds the percentage on a row change, no speech | same as #6 |
| 10 | Patches/ConfigMenuPatches.cs:631 | ShouldAnnounce | ConfigMenu.TouchSlider | text | Touch slider percentage | same as #6 |
| 11 | Patches/ConfigMenuPatches.cs:902 | ShouldAnnounce | ConfigMenu.KeysSetting | text (checked before the position is appended) | Controls screen row: action, key and button binding | same as #6 |
| 12 | Patches/GameOverPatches.cs:125 | ShouldAnnounce | Popup.GameOverLoadButton | index | Game-over load popup button on a cursor move | Patches/GameOverPatches.cs:156 on load popup Init; Patches/PopupPatches.cs:586 on any Popup.Close |
| 13 | Patches/GameOverPatches.cs:200 | ShouldAnnounce (seed) | Popup.GameOverLoadButton | index | Primes the focused button spoken with the open read | same as #12 |
| 14 | Patches/MagicMenuPatches.cs:442 | ShouldAnnounce | MagicTarget | text (checked before the position is appended) | Field magic target: name, HP, status | never |
| 15 | Patches/NewGameNamingPatches.cs:247 | ShouldAnnounce | NewGame.SelectedIndex | index | New-game slot "Onion Knight N: name" | Patches/NewGameNamingPatches.cs:183 on character selection Init |
| 16 | Patches/NewGameNamingPatches.cs:389 | ShouldAnnounce (seed) | NewGame.Name | text | Primes the suggested name spoken in "Select name" | :183 on character selection Init; :368 on name select Init |
| 17 | Patches/NewGameNamingPatches.cs:409 | ShouldAnnounce | NewGame.AutoNameIndex | index | Gates the name-cycling read (SetFocus) | :183; :368 |
| 18 | Patches/NewGameNamingPatches.cs:422 | ShouldAnnounce | NewGame.Name | text | Suggested name while cycling | :183; :368 |
| 19 | Patches/NewGameNamingPatches.cs:599 | ShouldAnnounce (seed) | NewGame.AutoNameIndex | index | Primes the current auto-name index (GetCurrentSuggestedName) | :183; :368 |
| 20 | Patches/BattleItemPatches.cs:125 | ShouldAnnounce via MenuStateHelper.cs:65 | BattleItem.Select | text (position included) | Battle item row | registry reset when BATTLE_ITEM goes inactive: battle end Patches/BattleResultPatches.cs:153, field state Patches/GameStatePatches.cs:214, another battle menu exclusive (e.g. Patches/BattleCommandPatches.cs:235), title |
| 21 | Patches/BattleMagicPatches.cs:131 | ShouldAnnounce via MenuStateHelper.cs:65 | BattleMagic.Select | text ("Empty", no position) | Empty battle spell slot | registry reset when BATTLE_MAGIC goes inactive: Patches/BattleResultPatches.cs:154, Patches/GameStatePatches.cs:215, another battle menu exclusive, title |
| 22 | Patches/BattleMagicPatches.cs:147 | ShouldAnnounce via MenuStateHelper.cs:65 | BattleMagic.Select | text (no position) | Battle spell: name, charges, description | same as #21 |
| 23 | Patches/EquipMenuPatches.cs:260 | ShouldAnnounce via MenuStateHelper.cs:65 | EquipMenu.Select | text (no position) | Equip slot "Slot: item" | registry reset when EQUIP_MENU goes inactive: back to command or close (Patches/EquipMenuPatches.cs:58, :136, :147), another menu exclusive, map change, title |
| 24 | Patches/EquipMenuPatches.cs:358 | ShouldAnnounce via MenuStateHelper.cs:65 | EquipMenu.Select | text (no position) | Equip candidate item: name, stats, description | same as #23 |
| 25 | Patches/ItemMenuPatches.cs:146 | ShouldAnnounce via MenuStateHelper.cs:65 | ItemMenu.Select | text (no position) | Item row "Name, qty: description" | Patches/ItemMenuPatches.cs:119 via :391 (item list Init) and :398 (target Init); registry reset when ITEM_MENU goes inactive (:63, :102, :113 back to command or close), another menu exclusive, map change, title |
| 26 | Patches/ItemMenuPatches.cs:187 | ShouldAnnounce via MenuStateHelper.cs:65 | ItemMenu.Select | text (no position) | Item-use target: name, HP, status | same as #25 |
| 27 | Patches/JobMenuPatches.cs:359 | ShouldAnnounce via MenuStateHelper.cs:65 | JobMenu.Select | text | Job name, job level, "Equipped" | registry reset when JOB_MENU goes inactive: close Patches/JobMenuPatches.cs:246, another menu exclusive, map change, title |
| 28 | Patches/JobMenuPatches.cs:384 | ShouldAnnounce via MenuStateHelper.cs:75 (IsNewJobIndex) | JobMenu.Index | index (result ignored) | Nothing. The context is write-only | same as #27 |
| 29 | Patches/StatusMenuPatches.cs:294 | ShouldAnnounce via MenuStateHelper.cs:65 | StatusMenu.Select | text (no position) | Party member: name, job, level, row, HP | registry reset when STATUS_MENU goes inactive: close Patches/StatusMenuPatches.cs:139, leave details :521, details open :461 (ResetAllExcept), another menu exclusive, map change, title |
| 30 | Patches/BestiaryPatches.cs:406 | AnnounceIfNew | Bestiary.State | text | Encounter summary when the bestiary opens | Patches/BestiaryPatches.cs:95 on bestiary exit (ClearState from :383 and :975) |
| 31 | Patches/BestiaryPatches.cs:458 | AnnounceIfNew | Bestiary.Map | text | "Map open: habitat" | :95 on bestiary exit only |
| 32 | Patches/BestiaryPatches.cs:544 | AnnounceIfNew | Bestiary.Formation | text (position included) | Formation read | :95 on bestiary exit; :374 on formation state entry; :564 on re-read after a party change |
| 33 | Patches/BestiaryPatches.cs:595 | AnnounceIfNew | Bestiary.ListEntry | text | List entry "No: name" (LibraryMenuController.Show) | :95 on bestiary exit; :343 on return to the list; :931 when the config bestiary returns to the list |
| 34 | Patches/BestiaryPatches.cs:626 | AnnounceIfNew | Bestiary.ListEntry | text | List entry (OnContentSelected), sharing the context with #33 | same as #33 |
| 35 | Patches/GalleryPatches.cs:205 | AnnounceIfNew | Gallery.ListEntry | text | Gallery item | Patches/GalleryPatches.cs:34 on leaving the gallery (:115); :104 on return from the image view |
| 36 | Patches/MusicPlayerPatches.cs:224 | AnnounceIfNew | MusicPlayer.ListEntry | text | Song entry | Patches/MusicPlayerPatches.cs:35 on leaving (:130); :278 on the Original/Arrangement switch |

Scattered trackers: 34 in 20 files. That is 25 value or once trackers plus 9 hand-off or frame-stamp gates, marked "gate" in the Compares column. The 34 trackers span 38 fields.

| # | Declared | Variable | Context | Compares | Checked at | Reset |
|---|---|---|---|---|---|---|
| 1 | Patches/BattleCommandPatches.cs:102 | lastCharacterId | "X's turn" | other: character id | Patches/BattleCommandPatches.cs:119 | :143 at battle start (Patches/BattleStatePatches.cs:36) only |
| 2 | Patches/BattleCommandPatches.cs:161 | lastAnnouncedCmdMesId | Battle command cursor | other: command MesId | Patches/BattleCommandPatches.cs:291 | :179 on turn handoff (SetCommandData prefix :91); :272 on the cancel-return re-read; :325 at battle end (Patches/BattleStatePatches.cs:51) |
| 3 | Patches/BattleCommandPatches.cs:384 | lastTargetSpokenFrame | Initial target read after EnemysInit/PlayerInit | gate: frame stamp | Patches/BattleCommandPatches.cs:407 | never (frame stamp; set :673, :778) |
| 4 | Patches/BattleDamagePatches.cs:230 | lastAnnouncement | Status added "Name: Status" | text | Patches/BattleDamagePatches.cs:288 | :238 at battle start (Patches/BattleStatePatches.cs:35); :247 when the same status's removal is spoken (:385) |
| 5 | Patches/BattleDamagePatches.cs:327 | spokenThisFrame (+ spokenFrame :328) | Status removed | other: (unit ptr, condition id) per frame | Patches/BattleDamagePatches.cs:377-382 | :380 each new frame; :334-335 at battle start (Patches/BattleStatePatches.cs:28) |
| 6 | Patches/BattleMessagePatches.cs:88 | LastActionName | Command-message window text that equals the action CreateActFunction already spoke | text | Patches/BattleStartPatches.cs:164 | never (overwritten :117) |
| 7 | Patches/BattlePausePatches.cs:77 | lastAnnouncedButtonIndex | CommonPopup button cursor, in and out of battle | index | Patches/BattlePausePatches.cs:115 | :90 on popup open; :173 when the open read starts; :183 seeded by the open read; :193 on popup close (Patches/PopupPatches.cs:585) and battle end (Patches/BattleResultPatches.cs:155) |
| 8 | Patches/BattlePausePatches.cs:81 | commonPopupReadPending | CommonPopup cursor while the open read is pending | gate: pending flag | Patches/BattlePausePatches.cs:110 | set :174 (Patches/PopupPatches.cs:408); cleared :184 (Patches/PopupPatches.cs:537, :547) and :194 |
| 9 | Patches/BattleResultPatches.cs:65 | announcedPoints | Gil and XP gained | other: once flag | Patches/BattleResultPatches.cs:163 | :125 when the results screen shows (:729); :139 at battle start (Patches/BattleStatePatches.cs:37) |
| 10 | Patches/BattleResultPatches.cs:66 | announcedItems | Items dropped | other: once flag | Patches/BattleResultPatches.cs:205 | :126 when the results screen shows; :140 at battle start |
| 11 | Patches/BattleResultPatches.cs:67 | announcedLevelUps | Level ups and job level ups | other: HashSet of "name_levelup" / "name_joblevelup" | Patches/BattleResultPatches.cs:270; :327 | :127 when the results screen shows; :141 at battle start |
| 12 | Patches/BestiaryPatches.cs:69 | SuppressNextListEntry | Bestiary list entry while the open read is pending | gate: pending flag | Patches/BestiaryPatches.cs:587; :618 | set :324, :925; cleared :443 when the open read is done, :84 on bestiary exit |
| 13 | Patches/BestiaryPatches.cs:111 | lastSelectState | "Minimap open" / "Minimap closed" | other: minimap state 0/1 | Patches/BestiaryPatches.cs:783 | :116 on bestiary exit (ResetUpdateControllerState from ClearState :94) |
| 14 | Patches/BestiaryPatches.cs:124 | formationReadPending | Formation opening read (state entry vs controller activation) | gate: pending flag | Patches/BestiaryPatches.cs:483 | set :377; cleared :307 on leaving the formation view, :506/:511/:517 after the read, :118 on bestiary exit |
| 15 | Patches/ConfigMenuPatches.cs:174 | lastRowSpokenFrame | Fallback config row re-reads (popup close, title Options open, bestiary return) | gate: frame stamp | Patches/ConfigMenuPatches.cs:218; :251; :310 (RowSpokenSince :176) | never (frame stamp; set :165) |
| 16 | Patches/ConfigMenuPatches.cs:231 | initialFocusPendingFrame | Title Options initial row read (ShowConfig + InitSelectLanguage in the same frame) | gate: frame stamp | Patches/ConfigMenuPatches.cs:242 | cleared :250 when the read runs; treated as stale after 10 frames |
| 17 | Patches/ConfigMenuPatches.cs:443 | lastSliderPtr (+ lastValue :444) | KeyInput slider percentage | other: slider ptr + text | Patches/ConfigMenuPatches.cs:505 | :451-452 on config close (:853) |
| 18 | Patches/ConfigMenuPatches.cs:445 | readPending | Slider value read, several changes per frame | gate: pending flag | Patches/ConfigMenuPatches.cs:484 | cleared :492 the next frame |
| 19 | Patches/EventItemSelectPatches.cs:57 | lastAnnouncement | NPC event item select row | text (checked before the position is appended) | Patches/EventItemSelectPatches.cs:204 | :224 on manager Close |
| 20 | Patches/GalleryPatches.cs:23 | SuppressContentChange | Gallery focus during the entry read | gate: pending flag | Patches/GalleryPatches.cs:191 | set :96; cleared :155/:166 when the entry item is read, :30 on leaving the gallery |
| 21 | Patches/GameStatePatches.cs:21 | lastAnnouncedMapId | "Entering X" | other: map id | Patches/GameStatePatches.cs:168; :195 | never (ResetMapTracking :223 has no callers) |
| 22 | Patches/GameToggleAnnouncer.cs:23 | lastEncounter | "Encounters on" / "Encounters off" | other: bool value | Patches/GameToggleAnnouncer.cs:49 (:65) | reseeded :42 after a scene load (Core/FFIII_ScreenReaderMod.cs:318) |
| 23 | Patches/GameToggleAnnouncer.cs:24 | lastRun | "Walk" / "Run" | other: bool value | Patches/GameToggleAnnouncer.cs:56 (:65) | reseeded :43 after a scene load |
| 24 | Patches/MagicMenuPatches.cs:39 | lastSpellId | Field magic spell list and spell tomes | other: spell id, text hash, or -1 for Empty | Patches/MagicMenuPatches.cs:110 (from :679, :790, :806, :839) | :67 when the spell list gains focus (:496); :46 registry reset when MAGIC_MENU goes inactive (close :376, back to the command bar :93/:471, map change, title) |
| 25 | Patches/MessageWindowPatches.cs:31 | lastAnnouncedPageIndex | Dialogue page | index | Patches/MessageWindowPatches.cs:160 | :87 on new message content (:513); :214 on window Close (:581) |
| 26 | Patches/MessageWindowPatches.cs:35 | lastAnnouncedSpeaker | Speaker prefix on a dialogue page | text | Patches/MessageWindowPatches.cs:172 | :216 on window Close; :226 on line-fade SetData (Patches/ScrollMessagePatches.cs:338) |
| 27 | Patches/MovementSpeechPatches.cs:45 | lastTransportationId | "On X" / "On foot" transition | other: previous transport id | Patches/MovementSpeechPatches.cs:135-149 | never (ResetState :378 and SyncToOnFoot :388 have no callers); overwritten :136, :313, :339 |
| 28 | Patches/MovementSpeechPatches.cs:46 | lastAnnouncedTransportId | Boarding and leaving a vehicle | other: transport id | Patches/MovementSpeechPatches.cs:132; :305; :333 | never (ResetState :379 and SyncToOnFoot :389 have no callers) |
| 29 | Patches/MusicPlayerPatches.cs:23 | SuppressContentChange | Song focus during the entry read and the Original/Arrangement switch | gate: pending flag | Patches/MusicPlayerPatches.cs:196 | set :122, :261; cleared :170/:181 when the entry song is read, :294 after the switch, :32 on leaving |
| 30 | Patches/NewGameNamingPatches.cs:33 | lastSlotNames | New-game slot name after assignment (UpdateView) | text per slot | Patches/NewGameNamingPatches.cs:457 | :186 on character selection Init; seeded :265 |
| 31 | Patches/ScrollMessagePatches.cs:31 | lastScrollMessage (+ lastScrollMessageTime :32) | Fade and scroll messages from 4 hooks | other: same text within 2 s | Patches/ScrollMessagePatches.cs:148; :181; :291; :313 (IsDuplicate :198) | never (2 s window) |
| 32 | Patches/ShopPatches.cs:169 | lastAnnouncedListIndex | Shop buy/sell list row | index | Patches/ShopPatches.cs:213 | :187 in a non-list state; :341 when the command bar gains focus; :364 when the trade window opens; :449 on registry reset of SHOP_MENU (close :440, controller gone :66, map change, title) |
| 33 | Patches/VehicleLandingPatches.cs:21 | lastLandableState | "Can land" | other: bool edge false to true | Patches/VehicleLandingPatches.cs:143; :168 | never (ResetState :186 has no callers) |
| 34 | Utils/LocationMessageTracker.cs:12 | lastMapTransitionMessage (+ inMapTransition :13) | Location fade or system message after "Entering X" | other: substring of the last transition text | Utils/LocationMessageTracker.cs:39-55 (from Patches/ScrollMessagePatches.cs:157, Patches/BattleSystemMessagePatches.cs:193, :239) | :88-89 on scene load (Core/FFIII_ScreenReaderMod.cs:288); set from Patches/GameStatePatches.cs:183 |

Excluded (not speech):
- Core/AudioLoopManager.cs:41 `lastBeaconTarget`: a beacon target change clears the silence latch
- Core/AudioLoopManager.cs:50 `lastBeaconPlayedAt`: beacon ping interval
- Core/AudioLoopManager.cs:120 `lastCheckTime`: wall-tone loop interval (local variable)
- Patches/MovementSoundPatches.cs:49 `lastBumpTime`: wall-bump sound cooldown
- Patches/MovementSoundPatches.cs:55 `lastCollisionPos`: same-position bump check
- Patches/MovementSoundPatches.cs:66 `lastTilePosition`: one footstep per tile
- Utils/SoundPlayer.cs:34 `lastWallToneVolume`: wall-tone volume cache
- Field/EntityScanner.cs:37 `lastScannedMapId`: entity rescan on a map change
- Field/EntityScanner.cs:319 `seenDestinations`: drops duplicate map exits from the navigation list
- Core/InputManager.cs:258 `lastRefreshFrame`: throttle for the player-controller cache refresh
- Core/FFIII_ScreenReaderMod.cs:255 `previousSceneName`: battle-exit detection on scene load
- Core/WaypointNavigator.cs:60 `previousSelectionId`: keeps the waypoint selection after a re-sort (local variable)

Not counted (speech flow, not a repeat check):
- Patches/BattleCommandPatches.cs:167 `commandReannouncePending`, Patches/ConfigMenuPatches.cs:273 `libraryReturnArmedFrame`: these arm a re-read, so they enable speech rather than suppress it
- Patches/BattleDamagePatches.cs:324 `battleEndCleanup`: silences the status clean-up at battle end
- Patches/GalleryPatches.cs:126, Patches/MusicPlayerPatches.cs:141 `entryHeaderSpoken`: makes the header speak before the entry item
- Patches/NewGameNamingPatches.cs:34 `lastTargetIndex`: slot pointer for UpdateView
- Patches/ConfigMenuPatches.cs:180 `lastDetailsController`: controller reference for re-reads
- Patches/BestiaryPatches.cs:903 `_previousState`: written but never read
- Menus/BestiaryNavigationReader.cs:31 `lastGroup`: local variable that marks stat-buffer group boundaries
- Generation counters that cancel stale coroutines:
  - Patches/BattleCommandPatches.cs:170, :379
  - Patches/ConfigMenuPatches.cs:269
  - Patches/ItemMenuPatches.cs:377
  - Patches/FieldMenuPatches.cs:49
  - Patches/TitleScreenPatches.cs:45

Findings (code reading only, nothing tested in game):
- **BattleTarget.Player/Enemy (#1-2): the index check never suppresses a cursor-driven read.** The SelectContent prefixes (Patches/BattleCommandPatches.cs:797, :824) call SetTargetSelectionActive(true), which resets both contexts (:571 then :372) before every postfix. The check only matters on the Init read path.
- **Contexts never reset:**
  - MagicTarget: text with HP and status, never reset. Re-entering target selection with the same focused text is silent.
  - BattleResult.Data: never reset, but it is write-only, so this is harmless.
  - Bestiary.Map: cleared only on bestiary exit, so reopening the map with the same first habitat is silent.
- **Dead contexts:**
  - BattleResult.Data (#5) and JobMenu.Index (#28) are written but never gate anything.
  - Four declared contexts are never checked: ConfigMenu.Slider, ConfigMenu.SliderController, NewGame.SlotName and Bestiary.DetailStat.
- **Battle.Message (#3) is shared by 6 hooks**, including the SystemMessageWindow Manager, Controller and View hooks, which have no battle check. It resets only at battle start, so two identical system messages in a row outside battle are spoken once.
- **Other shared contexts:**
  - ItemMenu.Select: item list and item-use target.
  - EquipMenu.Select: slot list and item list.
  - BattleMagic.Select: "Empty" and spells.
  - Bestiary.ListEntry: Show and OnContentSelected, which looks intended.
  - NewGame.Name and NewGame.AutoNameIndex: each has a seed call and a check.
- **Text compared where an index was available:**
  - ConfigMenu.Arrow and ConfigMenu.TouchArrow have no row identity. Changing a second row to the value last spoken (for example "On") is silent until the config menu closes.
  - BattleMagic "Empty" (#21) and MagicMenuState.lastSpellId -1 (Patches/MagicMenuPatches.cs:806): consecutive empty spell slots are silent.
  - BattleDamagePatches lastAnnouncement uses "Name: Status" with no unit id, so the same status on two same-named enemies is spoken once.
  - MagicTarget, KeysSetting, EventItemSelect, Equip, Item, Status and Job rows all compare text with the position appended afterwards.
- **Reset methods with no callers:**
  - GameStatePatches.ResetMapTracking (lastAnnouncedMapId)
  - MovementSpeechPatches.ResetState and SyncToOnFoot (transport trackers)
  - VehicleLandingPatches.ResetState
  - MessageWindowPatches.ResetTracking (window Close still resets)
- **lastCharacterId resets only at battle start.** Consecutive turns by the same character, such as a lone survivor, may never re-speak "X's turn".
- **LocationMessageTracker.inMapTransition** stays true until the next scene load. Outside a transition it drops any 1-4 word fade message without `.`, `!` or `?`.

### FF4

Deduplicator calls: 45 (ShouldAnnounce 36, AnnounceIfNew 7, GetLast* 2); distinct contexts: 35 (3 more constants are never queried: ConfigMenu.TouchArrowValue, ConfigMenu.TouchSliderValue, Bestiary.DetailStat). Reset calls: 47 (+2 through the EquipmentAnnouncementDeduplicator.Reset wrapper: Core/MenuState.cs:263, Patches/ItemMenuPatches.cs:305); ResetAll at: none (defined at Utils/AnnouncementDeduplicator.cs:168, never called).

| # | Call site | Method | Context string | Compares | What is announced | Reset |
|---|---|---|---|---|---|---|
| 1 | Patches/AbilityMenuPatches.cs:124 | ShouldAnnounce | AbilityMenu.Command | text | Ability menu command name (also the deferred CommandInit read, :271) | Patches/AbilityMenuPatches.cs:246 command bar Init (CommandInit prefix) |
| 2 | Patches/AbilityMenuPatches.cs:201 | ShouldAnnounce | AbilityMenu.Content | text | Spell name, MP, description (Auto Detail) | Patches/AbilityMenuPatches.cs:247 spell list Init (UseListInit prefix) |
| 3 | Patches/AbilityMenuPatches.cs:330 | ShouldAnnounce | AbilityMenu.UseTarget | text | Target name + HP/MP/status | Patches/AbilityMenuPatches.cs:248 target select Init (UseTargetInit prefix) |
| 4 | Patches/BattleCommandPatches.cs:72 | GetLastIndex | BattleCommand.Select | other: read only; -1 means first of turn | Chooses interrupt=false for the first command after "X's turn" | Patches/BattleMessagePatches.cs:849 new turn (SetCommandSelectTarget prefix); :774 battle start |
| 5 | Patches/BattleCommandPatches.cs:75 | ShouldAnnounce | BattleCommand.Select | index | Battle command name (index recorded before content is validated) | Patches/BattleMessagePatches.cs:849 new turn; :774 battle start |
| 6 | Patches/BattleCommandPatches.cs:208 | ShouldAnnounce | BattleItem.Select | text | Battle item name, count, description | Patches/BattleCommandPatches.cs:231 item list opens (ShowUseSelect); Patches/BattleMessagePatches.cs:774 battle start |
| 7 | Patches/BattleCommandPatches.cs:316 | ShouldAnnounce | BattleAbility.Select | text | Battle spell name, MP, description | Patches/BattleCommandPatches.cs:339 magic list opens (ShowUseSelect); Patches/BattleMessagePatches.cs:774 battle start |
| 8 | Patches/BattleMessagePatches.cs:168 | GetLastString | Battle.Turn | other: read only; equals attacker name | When the last turn holder's action executes, resets Battle.Turn so their next turn speaks | Patches/BattleMessagePatches.cs:170 turn holder acts; :774 battle start |
| 9 | Patches/BattleMessagePatches.cs:175 | ShouldAnnounce | BattleAction | object | "X attacks" / "X uses Y" / "X flees" (one BattleActData each) | never |
| 10 | Patches/BattleMessagePatches.cs:620 | ShouldAnnounce | Battle.ConditionAdd | text | "Target: Status" added (no enemy letter) | Patches/BattleMessagePatches.cs:731 any status removed; :774 battle start |
| 11 | Patches/BattleMessagePatches.cs:821 | ShouldAnnounce | Battle.SetCommandMessage | text | BattleMenuController.SetCommadnMessage text | Patches/BattleMessagePatches.cs:807 empty message (box cleared); :774 battle start |
| 12 | Patches/BattleMessagePatches.cs:882 | ShouldAnnounce | Battle.Turn | text | "{name}'s turn" (keyed on the name) | Patches/BattleMessagePatches.cs:170 turn holder acts; :774 battle start |
| 13 | Patches/BattleMessagePatches.cs:950 | ShouldAnnounce | Battle.Target.Player | index+text | Ally target name + HP/MP/status | Patches/BattleMessagePatches.cs:871 new turn; :981 target cursor activated; :1019 enemy target focused; Patches/BattleTargetPatches.cs:57 "All allies"; :95 target window shown |
| 14 | Patches/BattleMessagePatches.cs:1013 | ShouldAnnounce | Battle.Target.Enemy | index | Enemy target name, letter, HP | Patches/BattleMessagePatches.cs:871 new turn; :956 ally target focused; :981 target cursor activated; Patches/BattleTargetPatches.cs:72 "All enemies"; :95 target window shown |
| 15 | Patches/BattleMessagePatches.cs:1208 | ShouldAnnounce | Battle.CommandMessage | text | BattleCommandMessageController SetMessage/SetCommandMessage/SetSystemMessage text (raw, before cleanup) | Patches/BattleMessagePatches.cs:1248 via :781 battle start only |
| 16 | Patches/BestiaryPatches.cs:217 | AnnounceIfNew | Bestiary.State | text | Encounter summary when the bestiary list opens | Patches/BestiaryPatches.cs:95 bestiary exit (ClearState from :194 state 7, :741 config-bestiary exit) |
| 17 | Patches/BestiaryPatches.cs:272 | AnnounceIfNew | Bestiary.Map | text | "Map open: {first habitat}" | Patches/BestiaryPatches.cs:95 bestiary exit only |
| 18 | Patches/BestiaryPatches.cs:330 | AnnounceIfNew | Bestiary.Formation | text | Formation readout + position | Patches/BestiaryPatches.cs:189 formation view entered; :352 ReannounceFormation; :95 bestiary exit |
| 19 | Patches/BestiaryPatches.cs:392 | AnnounceIfNew | Bestiary.ListEntry | text | Bestiary list entry (LibraryMenuController.Show) | Patches/BestiaryPatches.cs:155 back to list from detail/map/formation; :695 config-bestiary back from detail; :95 bestiary exit |
| 20 | Patches/BestiaryPatches.cs:430 | AnnounceIfNew | Bestiary.ListEntry | text | Bestiary list entry on cursor move (OnContentSelected; index available) | Patches/BestiaryPatches.cs:155; :695; :95 (as row 19) |
| 21 | Patches/ConfigMenuPatches.cs:536 | ShouldAnnounce | ConfigMenu.Command | text | Config row "name: value" (keyed on the row name only) | Patches/ConfigMenuPatches.cs:405 via :210 config list gains focus (InitializeSelect, InitializeGameBoosterSetting, OptionController.InitConfig/InitSelect* prefixes) |
| 22 | Patches/ConfigMenuPatches.cs:605 | ShouldAnnounce | ConfigMenu.KeysSetting | text | Key-binding row: action + binding | never |
| 23 | Patches/ConfigMenuPatches.cs:776 | ShouldAnnounce | ConfigMenu.ArrowValue | text | Arrow option value after SetNextSelect (value text only, any row) | never |
| 24 | Patches/ConfigMenuPatches.cs:811 | ShouldAnnounce | ConfigMenu.ArrowValue | text | Arrow option value after SetPrevSelect (same context as row 23) | never |
| 25 | Patches/ConfigMenuPatches.cs:854 | ShouldAnnounce | ConfigMenu.SliderValue | text | Slider value text (selected row only) | never |
| 26 | Patches/GalleryPatches.cs:198 | AnnounceIfNew | Gallery.ListEntry | text | Gallery list entry (number + name) | Patches/GalleryPatches.cs:112 back from image; :38 gallery exit (ClearState) |
| 27 | Patches/ItemMenuPatches.cs:279 (wrapper; caller :650) | ShouldAnnounce | EquipmentMenu.Announcement | text | Equipment slot "Slot: item, params" | Patches/ItemMenuPatches.cs:284 via :305 slot list Init (InfoInit); Core/MenuState.cs:263 EquipmentMenuState.Reset: equip window close (Patches/ItemMenuPatches.cs:204), command bar focus (:236), any other menu state activated, scene change, main menu open |
| 28 | Patches/ItemMenuPatches.cs:414 | ShouldAnnounce | ItemMenu.List | text | Item name, count, description (also the deferred Init read, :475) | Patches/ItemMenuPatches.cs:450 list Init (UseSelectInit/ImportantSelectInit/OrganizeSelectInit) |
| 29 | Patches/ItemMenuPatches.cs:548 | ShouldAnnounce | EquipmentMenu.Select | text | Equipment candidate name, params, description | Patches/ItemMenuPatches.cs:316 candidate list opens (SelectInit) |
| 30 | Patches/ItemMenuPatches.cs:714 | ShouldAnnounce | ItemMenu.UseTarget | text | Item target name + HP/MP/status (also the deferred Init read, :767) | Patches/ItemMenuPatches.cs:745 target select starts (SingleInit/AllInit) |
| 31 | Patches/MusicPlayerPatches.cs:192 | AnnounceIfNew | MusicPlayer.ListEntry | text | Song entry | Patches/MusicPlayerPatches.cs:259 Original/Arrangement toggle; :40 music player exit (ClearState) |
| 32 | Patches/NamingPatches.cs:174 | ShouldAnnounce | Naming.Select | index | Character name on the naming screen | Patches/NamingPatches.cs:125 open; :134 close; :144 InitializeSelect; :45 ClearState (any other menu state activated, scene change, main menu open) |
| 33 | Patches/PartySettingPatches.cs:138 | ShouldAnnounce | PartySetting.Select | text | Character list entry | never |
| 34 | Patches/PartySettingPatches.cs:188 | ShouldAnnounce | PartySetting.Select | text | "Party N, Position M: occupant" slot (same context as row 33) | never |
| 35 | Patches/PopupPatches.cs:275 | ShouldAnnounce | CommonPopupButton | index | Focused CommonPopup button on move | Patches/PopupPatches.cs:352 shop popup open; :364 CommonPopup open; :445 any popup close |
| 36 | Patches/PopupPatches.cs:301 | ShouldAnnounce | CommonPopupButton | index | Focused button appended to the popup open read | Patches/PopupPatches.cs:352; :364; :445 (as row 35) |
| 37 | Patches/PopupPatches.cs:498 | ShouldAnnounce | GameOverSelect | index | Focused button appended to the "Game Over" open read | Patches/PopupPatches.cs:374 game-over popup open |
| 38 | Patches/PopupPatches.cs:515 | ShouldAnnounce | GameOverSelect | index | Game-over Load/Title button on move | Patches/PopupPatches.cs:374 game-over popup open |
| 39 | Patches/PopupPatches.cs:574 | ShouldAnnounce | GameOverLoad | index | Focused button appended to the load-confirm open read | Patches/PopupPatches.cs:547 load confirm opens (InitSaveLoadPopup) |
| 40 | Patches/PopupPatches.cs:607 | ShouldAnnounce | GameOverLoad | index | Load-confirm Yes/No on move | Patches/PopupPatches.cs:547 load confirm opens |
| 41 | Patches/SaveLoadPatches.cs:674 | ShouldAnnounce | SaveLoadPopupButton | index | Claims the guard for the focused button spoken by the save popup open read (result ignored) | Patches/SaveLoadPatches.cs:572 save popup opens |
| 42 | Patches/SaveLoadPatches.cs:707 | ShouldAnnounce | SaveLoadPopupButton | index | Save popup Yes/No on move | Patches/SaveLoadPatches.cs:572 save popup opens |
| 43 | Patches/ShopPatches.cs:261 | ShouldAnnounce | Shop.Item | index+text | "Empty" sell-list slot | Patches/ShopPatches.cs:194 back to command bar (empty description outside the sell list) |
| 44 | Patches/ShopPatches.cs:278 | ShouldAnnounce | Shop.Item | text | Shop item "name, price" (keyed on item name only; index known) | Patches/ShopPatches.cs:194 (as row 43) |
| 45 | Patches/TitleMenuPatches.cs:56 | ShouldAnnounce | TitleMenu.Command | other: command id | Title command name (also the deferred Init read, :113) | Patches/TitleMenuPatches.cs:92 title menu Init (InitSelect/InitializeOption/InitializeExtra); Patches/BestiaryPatches.cs:101, Patches/GalleryPatches.cs:39, Patches/MusicPlayerPatches.cs:41 extras exit |

Scattered trackers: 32 in 17 files (rows 1-20: value trackers in 10 files; rows 21-28: open-read or already-read flags; rows 29-32: stateless text checks with no field).

| # | Declared | Variable | Context | Compares | Checked at | Reset |
|---|---|---|---|---|---|---|
| 1 | Patches/BattleMessagePatches.cs:31 | lastMessage (GlobalBattleMessageTracker) | Battle scroll messages from PlayMessageId (:432) and PlayMessageValue (:463); ScrollMessageManager.Play (:400) does not use it | text | Patches/BattleMessagePatches.cs:46 | Patches/BattleMessagePatches.cs:120 via Core/MenuState.cs:169 BattleState.Reset: return to field (Patches/GameStatePatches.cs:75), scene change, main menu open, any other menu state activated; not at battle start |
| 2 | Patches/BattleMessagePatches.cs:35 | lastAnnouncedAction | Drops SetCommadnMessage (:815) and BattleCommandMessage (:1216) text equal to the action name CreateActFunction just spoke (set :62 via :206) | text (case-insensitive) | Patches/BattleMessagePatches.cs:87 | Patches/BattleMessagePatches.cs:122 (as row 1); ClearLastActor :115 has no callers |
| 3 | Patches/BattleMessagePatches.cs:670 | spokenThisFrame HashSet (+ lastFrame :669) | "X: Status removed" | other: (unit pointer, condition id) within one frame | Patches/BattleMessagePatches.cs:723 | Patches/BattleMessagePatches.cs:721 every new frame |
| 4 | Patches/BattleResultPatches.cs:34 | PointsAnnounced | Victory gil + EXP lines | other: bool already-announced | Patches/BattleResultPatches.cs:112 | Patches/BattleResultPatches.cs:87 via :57 battle start (Patches/BattleMessagePatches.cs:782) and :77 new BattleResultData pointer |
| 5 | Patches/BattleResultPatches.cs:35 | ItemsAnnounced | Dropped items | other: bool already-announced | Patches/BattleResultPatches.cs:368 | Patches/BattleResultPatches.cs:88 (as row 4) |
| 6 | Patches/BattleResultPatches.cs:36 | AbilitiesAnnounced | Spells learned | other: bool already-announced | Patches/BattleResultPatches.cs:413 | Patches/BattleResultPatches.cs:89 (as row 4) |
| 7 | Patches/BattleResultPatches.cs:37 | AnnouncedLevelUps HashSet<string> | Level-up + stat gains per character | text (character name set) | Patches/BattleResultPatches.cs:304 | Patches/BattleResultPatches.cs:90 (as row 4) |
| 8 | Patches/BattleTargetPatches.cs:21 | lastWasAllPlayers | "All allies" | other: bool already-announced | Patches/BattleTargetPatches.cs:52 | Patches/BattleTargetPatches.cs:35 via :90 target window show/hide; :69 "All enemies" spoken |
| 9 | Patches/BattleTargetPatches.cs:22 | lastWasAllEnemies | "All enemies" | other: bool already-announced | Patches/BattleTargetPatches.cs:67 | Patches/BattleTargetPatches.cs:36 via :90; :54 "All allies" spoken |
| 10 | Patches/ConfigMenuPatches.cs:888 | lastValues ConditionalWeakTable | Touch config arrow value; first value per controller is silent | text (per controller object) | Patches/ConfigMenuPatches.cs:908; :911 | never (entry lives as long as the controller) |
| 11 | Patches/ConfigMenuPatches.cs:941 | lastValues ConditionalWeakTable | Touch config slider value; first value per controller is silent | text (per controller object) | Patches/ConfigMenuPatches.cs:962; :965 | never (entry lives as long as the controller) |
| 12 | Patches/GameStatePatches.cs:30 | lastAnnouncedMapId | "Entering {map}"; first map seeded silently (:143) | other: map id | Patches/GameStatePatches.cs:121 | never (ResetMapTracking :155 has no callers) |
| 13 | Patches/MessagePatches.cs:33 | lastAnnouncedPageIndex | Dialogue page text | index (page) | Patches/MessagePatches.cs:182 | Patches/MessagePatches.cs:96 new dialogue content; :230 DialogueTracker.Reset: window close (:483), scene load with no window open (Core/FFIV_ScreenReaderMod.cs:227) |
| 14 | Patches/MessagePatches.cs:37 | lastAnnouncedSpeaker | "Speaker: " prefix only (the page is still spoken) | text | Patches/MessagePatches.cs:194 | Patches/MessagePatches.cs:232 (as row 13); :264 ClearLastAnnouncedSpeaker: scene load (Core/FFIV_ScreenReaderMod.cs:218), LineFade SetData (:708) |
| 15 | Patches/MovementSpeechPatches.cs:41 | lastTransportationId | ChangeTransportation "On {vehicle}" / "On foot" (boarding vs disembark edge) | other: previous transport id | Patches/MovementSpeechPatches.cs:190; :204 | Patches/MovementSpeechPatches.cs:472 SyncToOnFoot (Utils/MoveStateHelper.cs:94, interior map entered on a vehicle); ResetState :462 has no callers |
| 16 | Patches/MovementSpeechPatches.cs:42 | lastAnnouncedTransportId | Same speech from ChangeTransportation, GetOn, GetOff | other: transport id | Patches/MovementSpeechPatches.cs:187; :357; :385 | Patches/MovementSpeechPatches.cs:473 SyncToOnFoot (as row 15); :463 dead |
| 17 | Patches/MovementSpeechPatches.cs:484 | lastMoveState (FieldPlayer_ChangeMoveState_Patch) | Gates MoveStateHelper.AnnounceStateChange (vehicle / on-foot speech) | other: move state | Patches/MovementSpeechPatches.cs:497 | never |
| 18 | Patches/PartySettingPatches.cs:27 | lastState | "Entering character list." / "Entering party slot grid." | other: menu state enum | Patches/PartySettingPatches.cs:80 | never |
| 19 | Patches/VehicleLandingPatches.cs:18 | lastLandableState | "Can land" on false to true; not updated while on foot (:26) | other: bool edge | Patches/VehicleLandingPatches.cs:30 | never (ResetState :46 has no callers) |
| 20 | Utils/MoveStateHelper.cs:44 | lastAnnouncedState | "On Hovercraft/Chocobo/Airship", "On foot" from ChangeMoveState | other: move state | Utils/MoveStateHelper.cs:118 | set to the current state at :54, :64, :91 (interior map); -1 only at :74, reached only from the dead MovementSpeechPatches.ResetState; :282 caller chain dead |
| 21 | Core/MenuState.cs:195 | ShopState.EnteredEquipmentSubmenu | Skips the shop command read after returning from Equipment ("generic cursor already announced") | other: already-read flag | Patches/ShopPatches.cs:144 | Patches/ShopPatches.cs:146 on first use; set at Core/MenuState.cs:205 (Patches/ItemMenuPatches.cs:230); FullReset :228 has no callers |
| 22 | Patches/BestiaryPatches.cs:69 | BestiaryStateTracker.SuppressNextListEntry | Bestiary list entry while the open read (AnnounceListOpen) speaks it | other: open-read pending flag | Patches/BestiaryPatches.cs:384; :422 | Patches/BestiaryPatches.cs:256 open read done; :84 ClearState (bestiary exit) |
| 23 | Patches/GalleryPatches.cs:22 | GalleryStateTracker.SuppressContentChange | Gallery entry during the "Gallery" open read | other: open-read pending flag | Patches/GalleryPatches.cs:184 | Patches/GalleryPatches.cs:57 entry read; :154 error; :32 ClearState (gallery exit) |
| 24 | Patches/MusicPlayerPatches.cs:22 | MusicPlayerStateTracker.SuppressContentChange | Song entry during the open read and the Original/Arrangement swap (:241) | other: open-read pending flag | Patches/MusicPlayerPatches.cs:164 | Patches/MusicPlayerPatches.cs:59; :143; :275 toggle done; :35 ClearState (exit) |
| 25 | Patches/PopupPatches.cs:255 | commonPopupOpenReadPending | CommonPopup button hook while the open read is pending | other: open-read pending flag | Patches/PopupPatches.cs:267 | Patches/PopupPatches.cs:295 open read runs; :443 popup close |
| 26 | Patches/PopupPatches.cs:488 | gameOverSelectOpenReadPending | Game-over button hook while the "Game Over" open read is pending | other: open-read pending flag | Patches/PopupPatches.cs:507 | Patches/PopupPatches.cs:493 open read runs; :444 popup close |
| 27 | Patches/PopupPatches.cs:532 | gameOverLoadOpenReadPending | Load-confirm move while the open read is pending | other: open-read pending flag | Patches/PopupPatches.cs:605 | Patches/PopupPatches.cs:564 open read runs; :625 scene change (Core/FFIV_ScreenReaderMod.cs:248) |
| 28 | Patches/SaveLoadPatches.cs:79 | savePopupOpenReadPending | Save popup Yes/No move while the open read is pending | other: open-read pending flag | Patches/SaveLoadPatches.cs:705 | Patches/SaveLoadPatches.cs:590 open read runs; :565; :742; :752; :792 popup close; :800 scene change |
| 29 | Patches/MessagePatches.cs:541 | (none, stateless) | System message skipped when it equals the current map name ("Entering X" already spoke it) | text (case-insensitive) vs MapNameResolver.GetCurrentMapName() | Patches/MessagePatches.cs:542 | n/a |
| 30 | Patches/ConfigMenuPatches.cs:645 | (none, per call) textParts | Repeated action-name parts inside one key-binding announcement | text (within one utterance) | Patches/ConfigMenuPatches.cs:645 | n/a |
| 31 | Patches/ConfigMenuPatches.cs:681 | (none, per call) textParts | Repeated binding labels inside one key-binding announcement | text (within one utterance) | Patches/ConfigMenuPatches.cs:681 | n/a |
| 32 | Menus/MenuTextDiscovery.cs:342 | (none, per call) textParts | Repeated text parts inside one generic command-row read | text (within one utterance) | Menus/MenuTextDiscovery.cs:342 | n/a |

Excluded (not speech):
- Core/AudioLoopManager.cs:52 `lastBeaconTarget`: beacon target-change detection (audio)
- Core/AudioLoopManager.cs:72 `lastBeaconPlayedAt`: beacon ping timing
- Patches/FootstepPatches.cs:34 `lastFootstepTime`: footstep sound cooldown
- Patches/FootstepPatches.cs:35 `lastTilePosition`: footstep tile change
- Patches/WallBumpPatches.cs:20 `lastBumpTime`: wall-bump sound cooldown
- Utils/SoundPlayer.cs:34 `lastWallToneVolume`: wall-tone volume
- Patches/MapTransitionPatches.cs:24-25 `_lastFadeCheckTime`, `_lastFadeResult`: fade-poll throttle that mutes wall tones
- Core/EntityCache.cs:25 `lastScannedMapId`: entity rescan on map change
- Core/InputManager.cs:258 `lastFieldRefreshFrame`: field refresh throttle
- Field/GroupEntity.cs:27 `_lastCacheFrame`: per-frame group representative cache
- Core/GamepadManager.cs:34, 66, 76, 100 `btnPrevious`, `rstickPrevious`, `lstickPrevious`, `keyPrevious`; Core/ControllerRouter.cs:68 `stickChordFired`; Core/NavigationTargetTracker.cs:12 `LastKind`: input edges and navigation routing
- Patches/BattleResultPatches.cs:27 `ExpCounterPlaying`: EXP counter sound
- Not counted, speech-adjacent but not repeat gates: Patches/BattleMessagePatches.cs:34 `lastAnnouncedActor` (written, never read: HasRecentActionForActor :69 has no callers); Patches/BattleResultPatches.cs:40 `lastResultDataPtr` (only triggers the reset of trackers 4-7); Patches/BestiaryPatches.cs:660 `_previousState` (written, never read); Patches/ConfigMenuPatches.cs:391-392 `retryScheduled`/`focusedRowReadScheduled`, Patches/GalleryPatches.cs:27 and Patches/MusicPlayerPatches.cs:30 `entryReadScheduled` (coroutine re-entry guards); Patches/PopupPatches.cs:662 `isTitleScreenTextPending` (deferred one-shot prompt); details-key caches `LastDescription` (Patches/AbilityMenuPatches.cs:152, Patches/BattleCommandPatches.cs:29, Patches/ItemMenuPatches.cs:294), Patches/ShopPatches.cs:22-25 `LastItem*`, Core/MenuState.cs:240 `LastSelectedItem`; menu/scene state gates (MenuStateRegistry, BestiaryStateTracker.CurrentState, GalleryStateTracker.PreviousState, StatusMenuTracker.IsUserOpened, IsTargetSelectionActive, IsFleeInProgress)

### FF5

Scattered trackers: 59 in 23 files.

| # | Declared | Variable | Context | Compares | Checked at | Reset |
|---|---|---|---|---|---|---|
| 1 | Patches/BattleCommandPatches.cs:138 | _lastCharacterId | Battle turn ("X's turn") | other: character id | Patches/BattleCommandPatches.cs:169 (SetCommandData Postfix) | never (by design, comment :135) |
| 2 | Patches/BattleCommandPatches.cs:198 | _lastIndex | Battle command list cursor | index | Patches/BattleCommandPatches.cs:248 (Announce, from SetCursor Postfix and RequestFocusedRead) | Patches/BattleCommandPatches.cs:201 ClearLast via :212 RequestFocusedRead: same-character SetCommandData rebuild (:170), Normal/Change/Defence/ManipulateInit (:524); not at battle start/end |
| 3 | Patches/BattleCommandPatches.cs:304 | _lastIndex | Battle item list cursor | index | Patches/BattleCommandPatches.cs:383 (BattleItemInfomationController.SelectContent Postfix) | Patches/BattleCommandPatches.cs:307 ClearLast via :154 every SetCommandData (command window built) |
| 4 | Patches/BattleCommandPatches.cs:403 | _lastIndex | Battle magic/ability list cursor | index | Patches/BattleCommandPatches.cs:451 (BattleQuantityAbilityInfomationController.SelectContent Postfix) | Patches/BattleCommandPatches.cs:406 ClearLast via :155 every SetCommandData |
| 5 | Patches/BattleMessagePatches.cs:37 | _lastScrollMessage | Battle scroll/system message | text | Patches/BattleMessagePatches.cs:50 (ScrollMessageManager.Play Postfix) | Patches/BattleMessagePatches.cs:40 via :321 turn start (SetCommandSelectTarget); Patches/GameStatePatches.cs:46 battle start |
| 6 | Patches/BattleMessagePatches.cs:76 | _lastActDataPtr | Battle actor + action | object (BattleActData pointer) | Patches/BattleMessagePatches.cs:124 (CreateActFunction Postfix) | Patches/BattleMessagePatches.cs:105 via :322 turn start; Patches/GameStatePatches.cs:47 battle start |
| 7 | Patches/BattleMessagePatches.cs:77 | _lastSpokenActorName | Ally dual-wield second swing | text (actor name) | Patches/BattleMessagePatches.cs:151 (CreateActFunction Postfix) | Patches/BattleMessagePatches.cs:106 via :322 turn start; Patches/GameStatePatches.cs:47 battle start |
| 8 | Patches/BattleMessagePatches.cs:413 | _announcedConditions | Battle status applied | other: set of (unit ptr, condition id) | Patches/BattleMessagePatches.cs:512 (BattleConditionController.Add Postfix) | Patches/BattleMessagePatches.cs:416 via :323 turn start; Patches/GameStatePatches.cs:54 battle start; Patches/BattleMessagePatches.cs:422 Forget one pair when its removal is spoken (:652) |
| 9 | Patches/BattleMessagePatches.cs:558 | _spokenThisFrame | Battle status removed | other: set of (unit ptr, condition id) per frame | Patches/BattleMessagePatches.cs:650 (BattleConditionController.RemoveFunction Postfix) | Patches/BattleMessagePatches.cs:648 new frame (`_frame` :557, :645); :563 SetBattleOver(false) at battle start (Patches/GameStatePatches.cs:55, Patches/BattleMessagePatches.cs:698) |
| 10 | Patches/BattleMessagePatches.cs:710 | lastBattleCommandMessage | Battle command message window (defeat etc.) | text (raw message) | Patches/BattleMessagePatches.cs:823 (BattleCommandMessagePatches.SetMessage_Postfix) | Patches/BattleMessagePatches.cs:850 via Patches/GameStatePatches.cs:56 battle start only |
| 11 | Patches/BattlePausePatches.cs:25 | _lastIndex | Battle pause menu cursor | other: index + frame (with _lastFrame) | Patches/BattlePausePatches.cs:39 (BattlePauseController.SetCommandSelectCursor Postfix) | never (only a same-frame repeat is suppressed) |
| 12 | Patches/BattlePausePatches.cs:26 | _lastFrame | Battle pause menu cursor | other: frame number (with _lastIndex) | Patches/BattlePausePatches.cs:39 (SetCommandSelectCursor Postfix) | never (self-expiring) |
| 13 | Patches/BattleResultPatches.cs:30 | PointsAnnounced | Battle result EXP/ABP/Gil totals | other: bool one-shot | Patches/BattleResultPatches.cs:107 (ShowPointsInit Postfix) | Patches/BattleResultPatches.cs:44 via :83 ResetSequence in the SAME ShowPointsInit, before the check (guard can never fire) |
| 14 | Patches/BattleResultPatches.cs:31 | ItemsAnnounced | Battle result item drops | other: bool one-shot | Patches/BattleResultPatches.cs:725 (ShowGetItemsInit Postfix) | Patches/BattleResultPatches.cs:45 via :83 ShowPointsInit (new result sequence) |
| 15 | Patches/BattleResultPatches.cs:37 | AnnouncedAbilityTexts | Battle result learned abilities / job level | text (set of lines) | Patches/BattleResultPatches.cs:636 (AnnounceFromTransformCoroutine, from ShowGetAbilitysInit and ShowLevelUpAbilitysInit) | Patches/BattleResultPatches.cs:46 via :83 ShowPointsInit (new result sequence) |
| 16 | Patches/BattleTargetPatches.cs:46 | _mode | Battle target selection | other: (mode, index) pair with _index | Patches/BattleTargetPatches.cs:69 (TargetChanged, from :373 AnnouncePlayerTarget, :400 AnnounceEnemyTarget, :426 AnnounceAllPlayers, :436 AnnounceAllEnemies) | Patches/BattleTargetPatches.cs:59 ResetState via :590 ShowWindow open/close; Patches/BattleMessagePatches.cs:320 turn start; Patches/BattleTargetPatches.cs:51 registry handler (BATTLE_TARGET cleared, incl. field-state ResetAll Patches/GameStatePatches.cs:194) |
| 17 | Patches/BattleTargetPatches.cs:47 | _index | Battle target selection | index (with _mode) | Patches/BattleTargetPatches.cs:69 (TargetChanged) | Patches/BattleTargetPatches.cs:60, same triggers as _mode |
| 18 | Patches/BestiaryPatches.cs:523 | MinimapOpen | Bestiary "Minimap open/closed" | other: bool last open state | Patches/BestiaryPatches.cs:545 (LibraryMenuController.ChangeState Postfix) | Patches/BestiaryPatches.cs:528 via ClearState :94 on bestiary exit (:187 GotoTitle, :803 config Library exit) |
| 19 | Patches/BestiaryPatches.cs:524 | LastMapIndex | Bestiary minimap habitat map | index | Patches/BestiaryPatches.cs:592 (LibraryMenuHabitatController.OnContentSelected Postfix) | Patches/BestiaryPatches.cs:529 via ClearState :94 bestiary exit; primed :552 on minimap open |
| 20 | Patches/ConfigMenuPatches.cs:69 | _lastCommand | Config option row | text (option name) | Patches/ConfigMenuPatches.cs:242 (Announce, from SelectCommand Postfix, retry, AnnounceFocusedRow, Options page read) | Patches/ConfigMenuPatches.cs:31 ClearState (options close :926, Patches/MainMenuPatches.cs:44 main menu Show, Patches/TitleMenuPatches.cs:27 title SetCursor); Patches/ConfigMenuPatches.cs:147 popup close / Library exit re-read; :219 stale popup; :978 list gains focus (InitializeSelect, GameBooster, Options page Inits) |
| 21 | Patches/ConfigMenuPatches.cs:80 | _spokenCount | Config row re-announce after popup/Library | other: spoken-row counter vs snapshot | Patches/ConfigMenuPatches.cs:153 (AnnounceFocusedRow settle callback) | never (monotonic; snapshot at :148) |
| 22 | Patches/ConfigMenuPatches.cs:313 | _lastRow | Key/controller remap row | text (built row incl. binding and position) | Patches/ConfigMenuPatches.cs:339 (ConfigKeysSettingController.SelectContent Postfix) | Patches/ConfigMenuPatches.cs:316 via :32 ClearState only (config close) |
| 23 | Patches/ConfigMenuPatches.cs:636 | _lastValue | Config left/right toggle value | text | Patches/ConfigMenuPatches.cs:668 (SwitchArrowSelectTypeProcess Postfix) | Patches/ConfigMenuPatches.cs:639 via :33 ClearState only; not reset on row change |
| 24 | Patches/ConfigMenuPatches.cs:710 | _lastSliderPtr | Config slider value | object (slider pointer, with _lastValue) | Patches/ConfigMenuPatches.cs:772 (ConfigSliderValueListener.ReadNextFrame, Slider.onValueChanged) | Patches/ConfigMenuPatches.cs:718 via :34 ClearState |
| 25 | Patches/ConfigMenuPatches.cs:711 | _lastValue | Config slider value | text (with _lastSliderPtr) | Patches/ConfigMenuPatches.cs:772 (ReadNextFrame) | Patches/ConfigMenuPatches.cs:719 via :34 ClearState |
| 26 | Patches/ConfigMenuPatches.cs:794 | _lastValue | Touch-mode config arrow value | text | Patches/ConfigMenuPatches.cs:824 (Touch SwitchArrowTypeProcess Postfix) | Patches/ConfigMenuPatches.cs:797 via :35 ClearState |
| 27 | Patches/ConfigMenuPatches.cs:850 | _lastSliderController | Touch-mode config slider | object (ReferenceEquals on wrapper) | Patches/ConfigMenuPatches.cs:882 (Touch SwitchSliderTypeProcess Postfix) | Patches/ConfigMenuPatches.cs:856 via :36 ClearState |
| 28 | Patches/ConfigMenuPatches.cs:851 | _lastSliderValue | Touch-mode config slider | text | Patches/ConfigMenuPatches.cs:883 (Touch SwitchSliderTypeProcess Postfix) | Patches/ConfigMenuPatches.cs:857 via :36 ClearState |
| 29 | Patches/EquipMenuPatches.cs:102 | _lastCommandIndex | Equip command bar | index | Patches/EquipMenuPatches.cs:138 (AnnounceEquipCommand, only from CommandInit settle :480) | Patches/EquipMenuPatches.cs:118 via :466 CommandInit, right before its only check (no-op guard) |
| 30 | Patches/EquipMenuPatches.cs:103 | _lastSlotIndex | Equip slot pane | index | Patches/EquipMenuPatches.cs:235 (AnnounceEquipSlot, from :372 SelectContent Postfix and :272 LB/RB re-read); :263 (AnnounceCharacterSwitch) | Patches/EquipMenuPatches.cs:119 CommandInit (:466); :265 LB/RB character switch; :327 item-list pane focused |
| 31 | Patches/EquipMenuPatches.cs:104 | _lastSelectRowIndex | Equip item select list | index | Patches/EquipMenuPatches.cs:329 (AnnounceEquipSelect, from :351 SetCursor Postfix) | Patches/EquipMenuPatches.cs:120 CommandInit (:466); :233 slot pane focused |
| 32 | Patches/GameStatePatches.cs:242 | _lastAnnouncedMapId | Map change "Entering X" | other: map id | Patches/GameStatePatches.cs:259 (CheckMapTransition, from :196 ChangeState Postfix and Patches/MovementSpeechPatches.cs:127 FieldReady Postfix) | never (overwritten every call :260) |
| 33 | Patches/ItemMenuPatches.cs:74 | _lastItemIndex | Item list | index | Patches/ItemMenuPatches.cs:152 (AnnounceItemListData, from :211 SelectContent Postfix and :382 list Init read) | Patches/ItemMenuPatches.cs:84 via :343 list state Inits; :352 target Single/AllInit; :361 CommandSelectInit |
| 34 | Patches/ItemMenuPatches.cs:75 | _lastTargetIndex | Item use target | index | Patches/ItemMenuPatches.cs:173 (AnnounceItemUseTarget, from :239 ItemUseController.SelectContent Postfix and :414 Init read) | Patches/ItemMenuPatches.cs:85 via :343; :352; :361 |
| 35 | Patches/ItemMenuPatches.cs:76 | _lastCommandIndex | Item command bar | index | Patches/ItemMenuPatches.cs:107 (AnnounceItemCommand, only from :396 CommandSelectInit settle) | Patches/ItemMenuPatches.cs:86 via :343; :352; :361 (cleared right before its only check: no-op guard) |
| 36 | Patches/JobAbilityPatches.cs:185 | _lastIndex | Job list | index | Patches/JobAbilityPatches.cs:261 (JobChangeWindowController.SelectContent Announce) | Patches/JobAbilityPatches.cs:188 via :1227 job window hidden; :1232 ConfirmPopupInit / FixJobChangeInit |
| 37 | Patches/JobAbilityPatches.cs:295 | _lastIndex | Ability command bar slots | index | Patches/JobAbilityPatches.cs:325; :339 (AbilityCommandController.SelectContent Announce) | Patches/JobAbilityPatches.cs:298 via :1251 ability window hidden / NonInit; :1285 UseListInit; :1307 UseTargetInit |
| 38 | Patches/JobAbilityPatches.cs:364 | _lastIndex | Ability equip list | index | Patches/JobAbilityPatches.cs:440; :473; :484; :517 (AbilityChangeController.SelectContent Announce) | Patches/JobAbilityPatches.cs:378 via :1259 equip window hidden / NonInit / ChangedInit / ConfirmPopupInit; :1344 SelectCommandInit |
| 39 | Patches/JobAbilityPatches.cs:545 | _lastIndex | Ability equip command slot | index | Patches/JobAbilityPatches.cs:636 (AbilityChangeController.SelectCommand Announce) | Patches/JobAbilityPatches.cs:557 via :1258 same as row 38; :1362 SelectListInit |
| 40 | Patches/JobAbilityPatches.cs:666 | _lastIndex | Ability use target | index | Patches/JobAbilityPatches.cs:703 (AbilityUseContentListController.SelectContent Announce) | Patches/JobAbilityPatches.cs:669 via :1253 window hidden / NonInit; :1276 CommandInit; :1286 UseListInit; :1312 UseTargetInit; :1327 Single/AllInit |
| 41 | Patches/JobAbilityPatches.cs:727 | _lastIndex | Spell list | index | Patches/JobAbilityPatches.cs:771; :825 (AbilityContentListController.SetCursor Announce) | Patches/JobAbilityPatches.cs:730 via :1252 window hidden / NonInit; :1275 CommandInit; :1308 UseTargetInit |
| 42 | Patches/MessagePatches.cs:39 | lastAnnouncedPageIndex | Dialogue page | index (page) | Patches/MessagePatches.cs:216 (AnnounceForPage, from :576 MessageWindowManager.PlayingInit Postfix) | Patches/MessagePatches.cs:157 new message (SetContent -> StoreMessages); :295 window Close (Core/FFV_ScreenReaderMod.cs:942); :281 ForceReset (no caller) |
| 43 | Patches/MessagePatches.cs:43 | lastAnnouncedSpeaker | Dialogue speaker prefix | text | Patches/MessagePatches.cs:228 (AnnounceForPage) | Patches/MessagePatches.cs:297 window Close; :283 ForceReset (no caller) |
| 44 | Patches/MovementSpeechPatches.cs:47 | lastTransportationId | Vehicle "On X" / "On foot" | other: previous transport id | Patches/MovementSpeechPatches.cs:185 (ChangeTransportation_Postfix: wasOnVehicle and intermediate checks :193-:222) | overwritten Patches/MovementSpeechPatches.cs:375 GetOn; :401 GetOff; :455 SyncToOnFoot (interior map transition); :445 ResetState has no caller |
| 45 | Patches/MovementSpeechPatches.cs:48 | lastAnnouncedTransportId | Vehicle "On X" / "On foot" | other: transport id | Patches/MovementSpeechPatches.cs:179 (ChangeTransportation_Postfix) | overwritten Patches/MovementSpeechPatches.cs:374 GetOn; :400 GetOff; :456 SyncToOnFoot; :446 ResetState has no caller |
| 46 | Patches/MovementSpeechPatches.cs:468 | lastMoveState | Move-state backup announcement | other: moveState | Patches/MovementSpeechPatches.cs:480 (FieldPlayer.ChangeMoveState Postfix) | never (:514 ResetState only reachable from MovementSpeechPatches.ResetState :443, which has no caller) |
| 47 | Patches/NamingPatches.cs:28 | _lastCursorIndex | Naming screen character grid | index | Patches/NamingPatches.cs:217 (SelectContent_Postfix) | Patches/NamingPatches.cs:51 registry handler (NAMING_MENU cleared, incl. field ResetAll); :163 Open; :172 Close; :186 InitializeSelect; :152 ClearState (no caller) |
| 48 | Patches/PopupPatches.cs:496 | lastGameOverLoadIndex | Game-over Load popup buttons | index | Patches/PopupPatches.cs:559 (TryReadGameOverLoadMove, from Cursor.NextIndex/PrevIndex) | Patches/PopupPatches.cs:577 popup open (InitSaveLoadPopup prefix); primed :641 by open read |
| 49 | Patches/SaveLoadPatches.cs:135 | lastAnnouncedIndex | Save/load slot list | index | Patches/SaveLoadPatches.cs:510 (KeyInputSaveListSelectContent_Postfix); :553 (TouchSaveListSelectContent_Postfix) | Patches/SaveListPatches.cs:38 list SetActive(true); Patches/SaveLoadPatches.cs:1056; :1077; :1098 LoadGame/Load/Save window closed; primed :625; :1119 ClearSaveLoadMenuState (no caller) |
| 50 | Patches/SaveLoadPatches.cs:136 | lastPopupButtonIndex | Save/load/quick-save confirm popup Yes/No | index | Patches/SaveLoadPatches.cs:988 (TryReadSavePopupMove, from Cursor.NextIndex/PrevIndex) | Patches/SaveLoadPatches.cs:954 popup open; :965 ForgetSavePopup (popup, window or QuickSave close); primed :1164 |
| 51 | Patches/ShopPatches.cs:156 | _lastCommandIndex | Shop command bar (Buy/Sell) | other: index within 1-frame window (with _lastCommandFrame) | Patches/ShopPatches.cs:187 (AfterShopCommandSetCursor) | never (expires after 1 frame) |
| 52 | Patches/ShopPatches.cs:157 | _lastCommandFrame | Shop command bar (Buy/Sell) | other: frame number | Patches/ShopPatches.cs:187 (AfterShopCommandSetCursor) | never (self-expiring) |
| 53 | Patches/StatusDetailsPatches.cs:87 | _lastIndex | Status menu character list | index | Patches/StatusDetailsPatches.cs:128 (AnnounceCharacterRow, from :199 SelectContent delayed read and Patches/StatusMenuFocusPatches.cs:79 Init read) | Patches/StatusMenuFocusPatches.cs:54 ListInit / SelectInit |
| 54 | Patches/VehicleLandingPatches.cs:19 | lastLandableState | "Can land" | other: bool last landable | Patches/VehicleLandingPatches.cs:31 (MapUIManager.SwitchLandable Postfix) | never (:49 ResetState has no caller; not updated while on foot :27) |
| 55 | Patches/GalleryPatches.cs:24 | PreviousState | Gallery entry read vs return from Details | other: previous scene state | Patches/GalleryPatches.cs:83 (SubSceneManagerExtraGallery.ChangeState Postfix) | Patches/GalleryPatches.cs:34 ClearState on leaving (:105); set :96, :101 |
| 56 | Patches/GalleryPatches.cs:27 | TitleSpoken | "Gallery" title | other: bool already spoken | Patches/GalleryPatches.cs:49 (TryAnnounceEntry) | Patches/GalleryPatches.cs:35 leaving (:105); :87 first entry |
| 57 | Patches/MusicPlayerPatches.cs:29 | TitleSpoken | "Music Player" title | other: bool already spoken | Patches/MusicPlayerPatches.cs:52 (TryAnnounceEntry) | Patches/MusicPlayerPatches.cs:37 leaving (:100); :91 entering View |
| 58 | Utils/LocationMessageTracker.cs:14 | lastMapTransitionMessage | Location banner vs "Entering X" | text (contains, case-insensitive) | Utils/LocationMessageTracker.cs:33 (ShouldAnnounceFadeMessage, from Patches/MessagePatches.cs:604 FadeMessageManager.Play Postfix) | Utils/LocationMessageTracker.cs:35 after suppressing one banner; overwritten :19 per map change (Patches/GameStatePatches.cs:266) |
| 59 | Utils/MoveStateHelper.cs:46 | lastAnnouncedState | Move-state vehicle announcement | other: moveState | Utils/MoveStateHelper.cs:162 (AnnounceStateChange, from Patches/MovementSpeechPatches.cs:501) | overwritten Utils/MoveStateHelper.cs:57 GetOn; :67 GetOff; :138 interior map transition; :327 V-key sync; :77 ResetState unreachable |

Excluded (not speech):
- Patches/MovementSoundPatches.cs:20 `lastBumpTime`: wall-bump sound cooldown.
- Patches/MovementSoundPatches.cs:24 `lastCollisionTile`: require repeated collisions on one tile before the bump sound.
- Patches/MovementSoundPatches.cs:29 `lastTilePosition`: footstep tile-change detection.
- Patches/MovementSoundPatches.cs:31 `lastFootstepTime`: footstep sound cooldown.
- Core/AudioLoopManager.cs:73 `lastBeaconTarget`: clears the beacon silence latch when the target changes.
- Core/AudioLoopManager.cs:100 `lastBeaconPlayedAt`: beacon ping debounce.
- Utils/SoundPlayer.cs:44 `lastWallToneVolume`: wall-tone volume tracking.
- Utils/SoundPlayer.cs:49 `lastLandingPingVolume`: landing-ping volume tracking.
- Core/EntityCache.cs:20 `lastScannedMapId`: entity rescan on map change.
- Core/NavigationTargetTracker.cs:12 `LastKind`: routes beacon / trigger to entity or waypoint.
- Core/ControllerRouter.cs:56 `LastDevice`: last input device (keyboard or controller).
- Locals only, no state: Menus/BestiaryNavigationReader.cs:34 `lastGroup`, :120 `prevGroupIndex`; Menus/StatusDetailsReader.cs:347 `prevGroupIndex`; Core/WaypointNavigator.cs:61 `previousSelectionId`, :133 `prevVal`; Core/FFV_ScreenReaderMod.cs:510 `prevCategory`; Field/Routing/BreadcrumbRouteChainer.cs `seenG` (pathfinding).

Excluded (speech-adjacent, not last-value guards):
- Patches/BestiaryPatches.cs:69 `SuppressNextListEntry`: one-shot mute of the list-entry read (:362) while AnnounceListOpen speaks it; cleared :248, :84.
- Patches/BestiaryPatches.cs:723 `_previousState`: write-only (set :789, :802, never read).
- Patches/BestiaryPatches.cs:68 `CurrentState`: scene state; locals `previousState` (:112) / `previousBestiaryState` (:735) choose "list open" vs re-announce.
- Patches/GalleryPatches.cs:22 and Patches/MusicPlayerPatches.cs:22 `SuppressContentChange`, Patches/MusicPlayerPatches.cs:30 `EntryPending`: entry/toggle latches that route SetFocus into the one entry read.
- Patches/SaveLoadPatches.cs:144 `savePopupOpenReadPending`, Patches/PopupPatches.cs:534 `gameOverLoadOpening`: swallow button moves until the popup's open read has spoken.
- Patches/TitleMenuPatches.cs:21 `_announcePending` (+ :20 `_pendingText`): same-frame coalesce, last SetCursor wins.
- Patches/ConfigMenuPatches.cs:712 `_readPending` (one slider read per frame), :77 `_retryScheduled` (one retry), :75 `SuppressReads` (mute ResetCursor on hidden lists).
- Utils/MenuFocusAnnouncer.cs:43 `_gen`: generation latch, newest Request supersedes an in-flight settle read.
- Patches/BattleMessagePatches.cs:555 `_battleOver`: mutes "removed" after battle end; :557 `_frame`: frame stamp that clears `_spokenThisFrame`.
- Patches/EquipMenuPatches.cs:109 `_queueSlotFrame`: picks queue vs interrupt after LB/RB; never suppresses.
- Patches/MovementSpeechPatches.cs:51 `wasInIntermediateState` (+ :52 `preIntermediateTransportId`): scripted-dismount latch feeding "On foot".
- Patches/StatusDetailsPatches.cs:22 `IsUserOpened`: mutes load-time status-details read (:251); :23 `LastSelectTime`: write-only.
- Detail-key data, not gates: Patches/EquipMenuPatches.cs:45-48 `LastName`/`LastDescription`/`LastItemType`/`LastItemId`; Patches/ShopPatches.cs:26-31 `LastItemDescription`/`LastItemMpCost`/`LastItemName`/`LastContentId`; Patches/ItemMenuPatches.cs:28 `LastSelectedItem`; Patches/ConfigMenuPatches.cs:72 `_lastDetails`; Patches/JobAbilityPatches.cs:368, :548 `_cachedIndex`; Patches/GalleryPatches.cs:23 and Patches/MusicPlayerPatches.cs:23 `CachedFocusedPtr`; Patches/BestiaryPatches.cs:71 `CachedEntryName`.
- Stateless text check: Patches/MessagePatches.cs:686 SystemMessageWindowManager.SetMessage skips text equal to the current map name (no tracker).

Notes:
- No AnnouncementDeduplicator class exists in FF5, and the speech output path (`FFV_ScreenReaderMod.SpeakText` -> `TolkWrapper`) has no dedup.
- One row per variable; paired guards (index+frame, mode+index, pointer+value) get one row each.
