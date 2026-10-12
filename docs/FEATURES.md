# Features: the porting checklist

Every feature, hotkey and setting of the five old mods. The port is graded against this list.

Written 2026-10-05 for Phase 0 of `../UNIFIED_MOD_PLAN.md`.

## How to read this

- **Games:** which old mods have the feature. "All" means FF1 to FF5. "1 2 3" means FF1, FF2 and FF3.
- **Files:** the main source files, relative to each old mod's repo:
  - FF1: `ff1/ff1-screen-reader`
  - FF2: `ff2/ff2-screen-reader`
  - FF3: `ff3/ff3-screen-reader`
  - FF4: `FF4/ff4-screen-reader`
  - FF5: `FF5/ff5-screen-reader`
- A file named once, with no game, has that path in every listed game.
- **Setting:** the MelonPreferences entry. The categories are `FFI_ScreenReader` to `FFV_ScreenReader`.
- **Parity:** FF1 is the default for shared behaviour. The last section lists every difference the port must decide.

Sources: each mod's `Core/InputManager.cs`, `Core/ControllerRouter.cs`, `Core/PreferencesManager.cs`,
`Core/ModMenu.cs`, readme, `docs/plan.md` and `mod_text.json`.
Where a readme or plan disagreed with the code, the code was taken.

## 1. Speech and dialogue

| Feature | Games | Keys | Setting | Files |
|---|---|---|---|---|
| Screen-reader output through Tolk (NVDA, JAWS, SAPI) | All | — | — | `Tolk.cs`, `Utils/TolkWrapper.cs`; FF5 also `Utils/SpeechHelper.cs` |
| Dialogue, page by page, with speaker name | All | — | — | FF1-3 `Patches/MessageWindowPatches.cs`; FF4-5 `Patches/MessagePatches.cs` |
| Scrolling story text (intro, ending) | All | — | — | FF1-3 `Patches/ScrollMessagePatches.cs`; FF4 `Patches/MessagePatches.cs`; FF5 `Patches/GameStatePatches.cs` |
| Line-fade story text | All | — | — | FF3 `Patches/LineFadeMessagePatches.cs`; FF1-2 `Patches/ScrollMessagePatches.cs`; FF4-5 `Patches/MessagePatches.cs` |
| Repeat dialogue page | All | R; pad: mod mode + X | — | FF1 `Patches/MessageWindowPatches.cs`; FF2-5 `DialogueTracker` (in the message patches) |
| "Entering {map}" on map change | All | — | — | FF1 `Patches/MapTransitionPatches.cs`; FF2-5 `Patches/GameStatePatches.cs` |
| Popups and confirmations, Game Over | All | — | — | `Patches/PopupPatches.cs`; FF1 `Patches/PopupState.cs`; FF3 `Patches/GameOverPatches.cs` |
| Mod text in 12 languages | All | — | — | `Utils/ModTextTranslator.cs`, `mod_text.json` |
| Japanese entity names translated | All | — | — | `Utils/EntityTranslator.cs`, `translation.json` |
| Dump untranslated entity names (developer) | 3 | 0 | — | FF3 `Core/InputManager.cs`, `Utils/EntityTranslator.cs`. FF5's `EntityDump.DumpCurrentMap` has no caller |
| Repeated-speech guards | All | — | — | FF3-4 `Utils/AnnouncementDeduplicator.cs`; see `DEDUP_INVENTORY.md` |
| F1 walk/run and F3 encounters narrated | All | F1, F3 (game keys) | — | FF1-3 `GameToggleAnnouncer.cs`; FF4-5 `Patches/GameTogglePatches.cs` |

## 2. Menus by screen

| Screen | Games | Extra keys | Files |
|---|---|---|---|
| Generic cursor reading, focus on open and back-out | All | — | `Menus/MenuTextDiscovery.cs`, `Utils/MenuPosition.cs`; FF1, 4, 5 `Patches/CursorNavigationPatches.cs`; FF5 `Utils/MenuFocusAnnouncer.cs` |
| Position "3 of 12" | All | — | `Utils/MenuPosition.cs`; setting `MenuPositionAnnouncements` |
| Title screen, "Press any button" | All | — | `Patches/TitleMenuPatches.cs`; FF1, 3 `Patches/TitleScreenPatches.cs`; FF4 `Menus/TitleMenuReader.cs` |
| New game, naming | All | Q random name (game) | FF1 `Patches/NewGamePatches.cs`; FF2-3 `Patches/NewGameNamingPatches.cs`; FF4-5 `Patches/NamingPatches.cs`; FF1, 3-5 `Menus/CharacterSelectionReader.cs` |
| Main (field) menu and command bar | All | — | FF1, 3, 5 `Patches/FieldMenuPatches.cs`; FF2, 4, 5 `Patches/MainMenuPatches.cs`; FF1, 2, 4 `Patches/CommandBarPatches.cs`; FF1-2 `Menus/CommandBarReader.cs` |
| Items | All | I details | `Patches/ItemMenuPatches.cs` |
| Magic | 1 2 3 | I details | `Patches/MagicMenuPatches.cs`; FF1 `Patches/MagicMenuState.cs`, `Patches/MagicTargetPatches.cs` |
| Abilities and spells (FF4, FF5 style) | 4 5 | I details | FF4 `Patches/AbilityMenuPatches.cs`; FF5 `Patches/JobAbilityPatches.cs` |
| Equipment | All | I details, U usable by | FF1-3, 5 `Patches/EquipMenuPatches.cs`; FF4 `Patches/ItemMenuPatches.cs`, `Patches/CommandBarPatches.cs` |
| Status screen, stat by stat | All | Up/Down, Shift+Up/Down group, Ctrl+Up/Down ends | `Menus/StatusDetailsReader.cs`; FF1-3 `Patches/StatusMenuPatches.cs`; FF4-5 `Patches/StatusDetailsPatches.cs`; FF1 `Menus/StatusNavigationReader.cs` |
| Repeat current stat | 3 | R on status screen | FF3 `Core/InputManager.cs` |
| Row (front, back) in status and target lists | 1 3 4 5 | — | FF1 `Patches/StatusMenuPatches.cs`; FF3 `Utils/CharacterDataHelper.cs`; FF4-5 `Menus/CharacterSelectionReader.cs` |
| Config menu, setting descriptions | All | I description | `Patches/ConfigMenuPatches.cs`; FF1, 3-5 `Menus/ConfigMenuReader.cs` |
| Controls list (keyboard, gamepad) | All | Up/Down, Ctrl+Up/Down | `Menus/KeyHelpReader.cs`; FF4-5 `Menus/KeyboardGamepadReader.cs` |
| Shops: buy, sell, quantity and total | All | I details, U usable by, Q stats (game) | `Patches/ShopPatches.cs`; FF2-3 `Menus/ShopCommandReader.cs`; FF1 `Patches/ShopMenuTracker.cs` |
| Save and load slots | All | — | `Patches/SaveLoadPatches.cs`; FF1, 3, 5 `Patches/SaveListPatches.cs`; FF1-4 `Menus/SaveSlotReader.cs` |
| Bestiary list and entry | All | Up/Down, Shift, Ctrl as on status | `Patches/BestiaryPatches.cs`, `Menus/BestiaryReader.cs`, `Menus/BestiaryNavigationReader.cs` |
| Gallery | All | — | `Patches/GalleryPatches.cs`, `Menus/GalleryReader.cs` |
| Music player | All | — | `Patches/MusicPlayerPatches.cs`, `Menus/MusicPlayerReader.cs` |
| Item details on demand | All | I; pad: right stick up | FF1 `Core/Handlers/GlobalHotkeyHandler.cs`; FF1, 3-5 `ItemDetailsAnnouncer.cs` (FF1 in `Menus/`, others in `Patches/`) |
| Usable by (who can equip) | All | U; pad: right stick left | FF1, 3-5 `Menus/UsableByAnnouncer.cs`; FF2 `Core/InputManager.cs` (always "any character can equip") |
| Screen controls on demand | All | Shift+I; pad: right stick down | `Core/ControllerRouter.cs` (`AnnounceContextControls`), `Menus/KeyHelpReader.cs` |

## 3. Battle

| Feature | Games | Keys | Setting | Files |
|---|---|---|---|---|
| Whose turn, commands | All | — | — | `Patches/BattleCommandPatches.cs` |
| Targets, with status | All | — | — | FF1-3 `Patches/BattleCommandPatches.cs`; FF4-5 `Patches/BattleTargetPatches.cs` |
| A, B, C letters on same-named enemies | 5 | — | `EnemyLetters` | FF5 `Core/FFV_ScreenReaderMod.cs`, battle target patches |
| Battle item and spell lists, descriptions | All | I | `AutoDetail…` | FF1-3 `Patches/BattleItemPatches.cs`, `Patches/BattleMagicPatches.cs`; FF4-5 ability and item patches |
| Actions, damage, healing, status added and removed, misses | All | — | — | `Patches/BattleMessagePatches.cs`; FF3 `Patches/BattleDamagePatches.cs`, `Patches/BattleSystemMessagePatches.cs` |
| Multi-hit damage as "14x1552" | 1 2 3 | — | `MultiHitDamage` | `Patches/BattleMessagePatches.cs` |
| Enemy HP: numbers, percentage, hidden | All | F5 | `EnemyHPDisplay` | `Core/InputManager.cs`, target patches |
| Steal results | 4 5 | — | — | `Patches/BattleMessagePatches.cs` |
| Battle start and escape | All | — | — | FF1, 3, 5 `Patches/BattleStartPatches.cs`; FF1 `Patches/BattlePatches.cs`; FF3 `Patches/BattleStatePatches.cs` |
| Pause menu (Space) | All | — | — | FF1, 3-5 `Patches/BattlePausePatches.cs`; FF2 special case in `Core/FFII_ScreenReaderMod.cs` (cursor path) |
| Active character HP, MP, status | All | H; pad: mod mode + X | — | `Core/FFn_ScreenReaderMod.cs` (`AnnounceCharacterStatus`); FF4 `Core/GameAnnouncementHelper.cs` |
| Results: gil, EXP, level-ups, items, skills | All | — | — | `Patches/BattleResultPatches.cs` |
| Current native results phase buffer | 1–6 | arrows/I; pad: mod mode + West/shoulders | Autodetail controls detail rows | Shared `battle_result.cpp` and `review_buffer.cpp`; [evidence](re/RESULTS.md) |
| EXP counter tick sound | 1 3 4 5 | — | `ExpCounter`, `ExpCounterVolume` | `Patches/BattleResultPatches.cs`, `Utils/SoundPlayer.cs` |
| Clear stale battle state | 1 2 3 4 | Tab | — | `Core/InputManager.cs` |

## 4. Field navigation and entity scanning

| Feature | Games | Keys | Setting | Files |
|---|---|---|---|---|
| Entity scan: chests, NPCs, exits, events, vehicles, save points | All | — | — | FF1 `Field/EntityScanner.cs` + `Field/EntityDetectors/`; FF2-3 `Field/EntityScanner.cs`; FF4-5 `Core/EntityCache.cs`, `Core/EntityNavigator.cs`, `Field/EntityFactory.cs` |
| Next / previous entity | All | ] [ or L J; pad: right stick down/up | — | as above; FF1 `Core/EntityNavigationManager.cs` |
| Next / previous category | All | Shift+] [, Shift+L J, = -; pad: right stick right/left | — | FF1 `Core/CategoryManager.cs`; FF2-5 `Core/Filters/CategoryFilter.cs` |
| Back to All category | All | Shift+K | — | as above |
| Announce selected entity | All | K | — | main mod file |
| Rescan entities | All | backtick | — | main mod file |
| Pathfinding filter (reachable only) | All | Shift+\ or Shift+P; pad: R3 | `PathfindingFilter` | `Core/Filters/PathfindingFilter.cs` (FF1-4); FF5 `Core/Filters/EntityFilters.cs` |
| Layer transition filter | All | Ctrl+\ or Ctrl+P | `ToLayerFilter` | `Core/Filters/ToLayerFilter.cs` (FF1-4); FF5 `Core/Filters/EntityFilters.cs` |
| Map exit filter (merge exits to one place) | All | Shift+M | `MapExitFilter` | FF4-5 `Core/Filters/MapExitGroupingStrategy.cs`; FF1-3 `Field/EntityScanner.cs` |
| Grouping strategies (exits, vehicle pads) | 4 5 | — | — | `Core/Filters/IGroupingStrategy.cs`, `MapExitGroupingStrategy.cs`, `VehiclePadGroupingStrategy.cs`, `Field/GroupEntity.cs` |
| Opened / unopened chests | All | — | — | `Field/NavigableEntity.cs` |
| Current map name | All | M; pad: mod mode + Y | — | `Field/MapNameResolver.cs` |
| Gil | All | G; pad: mod mode + X | — | main mod file |
| Vehicle or movement state | All | V; pad: mod mode + A (not FF3) | — | FF1 `Core/Handlers/GlobalHotkeyHandler.cs`; FF2-5 `Patches/MovementSpeechPatches.cs`; `Utils/MoveStateHelper.cs` |
| Boarding, leaving and landing announcements, "Can land" | All | — | — | `Patches/VehicleLandingPatches.cs` |
| Teleport beside selected entity | All | Ctrl+Arrows; pad: mod mode + right stick | — | main mod file (`TeleportInDirection`) |
| Timers (escape countdowns) | 4 5 | T read, Shift+T freeze | — | `Patches/TimerPatches.cs` |

## 5. Pathfinding

| Feature | Games | Keys | Setting | Files |
|---|---|---|---|---|
| Directions to selected entity | All | \ or P; pad: LT | — | `Field/FieldNavigationHelper.cs` |
| Hidden-passage route grid | 3 4 5 | — | — | `Field/HiddenPassageRouting.cs` |
| Long-range routing: breadcrumb chaining, riding search | 5 | — | — | FF5 `Field/Routing/BreadcrumbRouteChainer.cs`, `PathSpaceNormalizer.cs`, `RoutingAdapter.cs` |
| Baked-map router (native only, 2026-10-10): the whole sub-map, layers, hidden passages, shop counters, world-map vehicles and boarding, no 31-tile window; the game's searcher only for unknown maps | All | same keys | — | `src/core/field/baked_map.cpp`, `baked_router.cpp`, `baked_nav.cpp`; data `data/ffN/maps.bin` from `tools/bake_maps.py` |
| Puzzle guide (native only): authored step lists with live directions, across floors (exits and script warps) | All (FF2: none authored) | Shift+G / Ctrl+G; pad: mod mode + RB / LB | — | `src/core/field/puzzle_guide.cpp`, `data/ffN/puzzles.json` |
| Moon pathfinding | 4 | — | — | FF4 `Field/FieldNavigationHelper.cs` |
| Navigation target memory (last entity or waypoint) | All | — | — | `Core/NavigationTargetTracker.cs`; FF4 `Core/NavigationStateManager.cs`; FF5 `Core/NavigationStateSnapshot.cs` |

## 6. Waypoints

All five games. Field only. Stored in `UserData/waypoints.json`.

| Action | Keys | Pad | Notes |
|---|---|---|---|
| Next / previous waypoint | . , | D-pad down / up | |
| Next / previous category | Shift+. Shift+, | D-pad right / left | |
| Directions to waypoint | / | LT (last target) | |
| Add waypoint, with name | Shift+/ | — | text input window |
| Rename | Ctrl+. | — | |
| Delete | Ctrl+/ | — | Y/N confirm |
| Clear all on this map | Ctrl+Shift+/ | — | FF1-3 one confirm; FF4-5 two confirms |

Files: `Core/WaypointManager.cs`, `Core/WaypointNavigator.cs`, `Field/WaypointEntity.cs`, `Core/TextInputWindow.cs`,
`Core/ConfirmationDialog.cs`. Plus FF1-3, 5 `Core/WaypointController.cs`; FF1 `Core/Handlers/WaypointHandler.cs`; FF4 `Core/WaypointFacade.cs`.

## 7. Audio cues

| Feature | Games | Keys | Settings | Files |
|---|---|---|---|---|
| Wall bump sound | All | — | `WallBumpVolume` | FF1-3, 5 `Patches/MovementSoundPatches.cs`; FF4 `Patches/WallBumpPatches.cs` |
| Footsteps, one click per tile | All | ' | `Footsteps`, `FootstepVolume` | FF1-3, 5 `Patches/MovementSoundPatches.cs`; FF4 `Patches/FootstepPatches.cs` |
| Wall tones, four directions | All | ; | `WallTones`, `WallToneVolume` | `Core/AudioLoopManager.cs`, `Utils/ToneGenerator.cs` |
| Audio beacon toward target | All | F6; 9 in FF3-5; pad: L3 | `AudioBeacons`, `BeaconVolume` | `Core/AudioLoopManager.cs` |
| Re-speak destination on beacon restart | All | — | `AnnounceOnBeaconRestart` | main mod file |
| Landing pings while on a vehicle | 5 | Shift+; | `LandingPings`, `LandingPingVolume` | FF5 `Core/AudioLoopManager.cs`, `Field/FieldNavigationHelper.cs` |
| EXP counter tick | 1 3 4 5 | — | `ExpCounter`, `ExpCounterVolume` | `Patches/BattleResultPatches.cs` |
| Audio engine (SDL3 streams) | All | — | — | `Utils/AudioEngine.cs`, `Utils/SoundPlayer.cs`, `Utils/SoundConstants.cs`, `Core/SDL3.cs` |

## 8. Mod menu and settings

The mod menu opens with F8 or Start. Field only in FF1-4; field and field menus in FF5.

- Keys: Up/Down move, Left/Right change, Enter or Space toggle, I describes, Escape or F8 closes.
- Pad: D-pad or left stick move and change, A toggles, B or Start closes. FF5: right stick up describes.
- Files: `Core/ModMenu.cs`, `Core/PreferencesManager.cs`.

Current native layout (user, 2026-10-12): Speech, Navigation audio, Battle, then Close Menu.
Navigation audio holds audio options, each paired with its volume, and navigation settings.
EXP counter sound/volume belong to Battle. Settings retain their existing keys and values;
all modal keyboard input is foreground-focus gated. [Current implementation/evidence](FOLLOWUP_2026_10_12.md).

Historical managed menu layouts, in order:

- **FF1:** Audio Feedback (Wall Tones, Footsteps, Beacon Navigation); Volume Controls (4); Navigation Filters (3 filters, Stick Click Normalization); Battle Settings (Enemy HP Display, Multi-hit Damage); Battle Results (EXP Counter Sound, EXP Counter Volume); Announcements (Auto Detail, Beacon Destination Announcement, Menu Position Announcements); Close Menu.
- **FF2:** as FF1, without Battle Results.
- **FF3:** Audio Feedback (adds Beacon Destination Announcement); Volume Controls; Navigation Filters (3); Battle Settings; Battle Results; Announcements (Auto Detail, Menu Position Announcements); Controller Settings (Stick Click Normalization); Close Menu.
- **FF4:** Audio Feedback (Wall Tones, Footsteps, Beacon Navigation, Beacon Destination Announcement, Stick Click Normalization, Menu Position Announcements, Auto Detail); Volume Controls; Navigation Filters; Battle Results; Battle Settings (Enemy HP Display); Close Menu.
- **FF5:** Audio Feedback (as FF4 minus Stick Click Normalization, plus Landing Pings); Volume Controls (5, adds Landing Ping Volume); Navigation Filters; Controller Settings; Battle Results; Battle Settings (Enemy HP Display, Enemy Letters); Close Menu.

### Settings table

| Setting key | Type | Default | Games | Menu label | Hotkey |
|---|---|---|---|---|---|
| `PathfindingFilter` | bool | off | All | Pathfinding Filter | Shift+\ |
| `MapExitFilter` | bool | off | All | Map Exit Filter | Shift+M |
| `ToLayerFilter` | bool | off | All | Layer Transition Filter | Ctrl+\ |
| `WallTones` | bool | off | All | Wall Tones | ; |
| `Footsteps` | bool | off | All | Footsteps | ' |
| `AudioBeacons` | bool | off | All | Beacon Navigation | F6 (9 in FF3-5) |
| `LandingPings` | bool | off | 5 | Landing Pings | Shift+; |
| `AnnounceOnBeaconRestart` | bool | off | All | Beacon Destination Announcement | — |
| `MenuPositionAnnouncements` | bool | on | All | Menu Position Announcements | — |
| `AutoDetail` | bool | on | 1 5 | Auto Detail | F7 |
| `AutoDetailOnFocus` | bool | on | 2 3 4 | Auto Detail | F7 |
| `StickClickNormalization` | bool | off | All | Stick Click Normalization | L3+R3 together |
| `ExpCounter` | bool | on | 1 3 4 5 | EXP Counter Sound | — |
| `EnemyLetters` | bool | off | 5 | Enemy Letters | — |
| `EnemyHPDisplay` | int 0-2 | 0 numbers | All | Enemy HP Display (1 percent, 2 hidden) | F5 |
| `MultiHitDamage` | int 0-1 | 1 with hit count | 1 2 3 | Multi-hit Damage (0 total only) | — |
| `WallBumpVolume` | int 0-100 | 50 | All | Wall Bump Volume | — |
| `FootstepVolume` | int 0-100 | 50 | All | Footstep Volume | — |
| `WallToneVolume` | int 0-100 | 50 | All | Wall Tone Volume | — |
| `BeaconVolume` | int 0-100 | 50 | All | Beacon Volume | — |
| `LandingPingVolume` | int 0-100 | 50 | 5 | Landing Ping Volume | — |
| `ExpCounterVolume` | int 0-100 | 50 | 1 3 4 5 | EXP Counter Volume | — |

Notes for the settings import:

- **Two names for Auto Detail.** FF2-4 store it as `AutoDetailOnFocus`. Their old `AutoDetail` entry is obsolete
  (old default off) and must be ignored. FF1 and FF5 use `AutoDetail`.
- **`DamageDisplay` is obsolete** in FF1-3. `MultiHitDamage` replaced it. Ignore the old entry.
- Each game's settings come from its own category, `FFI_ScreenReader` to `FFV_ScreenReader`.
- Waypoints live in `UserData/waypoints.json`, not in MelonPreferences.

## 9. Controller support

All five games. SDL3 gamepad, with Xbox, PlayStation and Nintendo labels detected.
Files: `Core/GamepadManager.cs`, `Core/ControllerRouter.cs`, `Core/ControllerLabels.cs`, `Patches/InputPassthroughPatches.cs`.

Normal state, on the field:

- Right stick up/down: previous / next entity. Left/right: category.
- D-pad up/down: previous / next waypoint. Left/right: waypoint category.
- LT: directions to the last target, or restart the beacon.
- L3: toggle beacon. R3: toggle pathfinding filter. A single click acts on release.
- L3 and R3 together: toggle Stick Click Normalization. When it is on, L3 and R3 go to the game
  (walk/run, encounters), and the mod functions move to mod mode.
- Start: mod menu.

Normal state, in menus:

- Right stick up: details (I). Down: screen controls (Shift+I). Left: usable by (U).
- D-pad or left stick up/down: step through status, bestiary and controls lists.

Mod mode (press Back, then one button; Back again cancels):

| Context | Button | Action | Games |
|---|---|---|---|
| Dialogue | X | Repeat page | All |
| Battle | X | Active character HP and status | All |
| Field | X | Gil | All |
| Field | Y | Map name | All |
| Field | A | Vehicle or movement state | 1 2 4 5 |
| Field | right stick | Teleport beside the selected entity | All |
| Field | L3 / R3 | Beacon / pathfinding filter (normalization on) | All |
| Battle | right stick down | Screen controls | 1 3 4 |
| Off the field | right stick down | Screen controls | 2 |
| Battle or dialogue | right stick down | Screen controls | 5 |
| Battle results | B | Results detail log | 5 |

## 10. Full hotkey table

The Games column makes this the per-game table. Keys not listed belong to the game.

| Key | Context | Action | Games |
|---|---|---|---|
| [ or J | field | Previous entity | All |
| ] or L | field | Next entity | All |
| Shift+[ or Shift+J, - | field | Previous category | All |
| Shift+] or Shift+L, = | field | Next category | All |
| Shift+K | field | Back to All category | All |
| K | field | Announce selected entity | All |
| \ or P | field | Directions, or restart beacon | All |
| Shift+\ or Shift+P | field | Pathfinding filter | All |
| Ctrl+\ or Ctrl+P | field | Layer transition filter | All |
| backtick | field | Rescan entities | All |
| ' | field | Footsteps | All |
| ; | field | Wall tones | All |
| Shift+; | field | Landing pings | 5 |
| F6 | field | Audio beacon | All |
| 9 | field | Audio beacon | 3 4 5 |
| , and . | field | Previous / next waypoint | All |
| Shift+, and Shift+. | field | Waypoint category | All |
| / | field | Directions to waypoint | All |
| Shift+/ | field | Add waypoint | All |
| Ctrl+. | field | Rename waypoint | All |
| Ctrl+/ | field | Delete waypoint | All |
| Ctrl+Shift+/ | field | Clear waypoints on map | All |
| Ctrl+Arrows | field | Teleport beside entity | All |
| G | anywhere | Gil | All |
| H | anywhere (battle) | Active character HP, MP, status | All |
| M | anywhere | Map name | All |
| Shift+M | anywhere | Map exit filter | All |
| V | anywhere | Vehicle or movement state | All |
| R | anywhere | Repeat dialogue | All |
| R | status screen | Repeat current stat | 3 |
| I | anywhere | Details of highlighted entry | All |
| Shift+I | anywhere | Screen controls | All |
| U | anywhere | Usable by | All |
| T | anywhere | Timers | 4 5 |
| Shift+T | anywhere | Freeze or resume timers | 4 5 |
| Tab | anywhere | Clear stale battle state | 1 2 3 4 |
| 0 | anywhere | Dump untranslated names | 3 |
| I / arrows | battle results | Current phase details / line review | 1–6 |
| Up/Down, Shift, Ctrl | status, bestiary | Stat, group, ends | All |
| Up/Down, Ctrl | controls list | Entry, ends | All |
| W and S | status, bestiary, controls list | Same as Up and Down | All |
| F5 | battle (FF1-4), field (FF5) | Enemy HP display | All |
| F7 | anywhere | Auto Detail | All |
| F8 | field | Mod menu | All |

Rules shared by all five:

- F5, F7 and F8 act only with no modifier held, so Alt+F4 and similar pass through.
- With Alt held, no hotkey fires.
- In the text input and confirm windows, the window owns the keyboard (Y/N, Enter, Escape, arrows, Home/End, Backspace).

## 11. Game-only screens and features

| Game | Feature | Files |
|---|---|---|
| FF1 | Class choice at new game | `Patches/JobSelectionPatches.cs`, `Patches/NewGamePatches.cs`, `Patches/NewGameHelpers.cs` |
| FF1 | Spell charges per level (x/y) | `Patches/MagicMenuPatches.cs`, `Patches/MagicMenuState.cs` |
| FF1 | Spell shops | `Patches/ShopMagicPatches.cs` |
| FF1 | 15-puzzle: tile number, I for row and column | `Patches/PuzzlePatches.cs`, `Patches/PuzzleGameState.cs` |
| FF2 | Keywords: Words menu, Ask and Learn in dialogue | `Patches/KeywordPatches.cs` |
| FF2 | Weapon skill levels and spell levels | `Menus/StatusDetailsReader.cs`, `Patches/MagicMenuPatches.cs` |
| FF2 | Phased battle results: skill and stat gains | `Patches/BattleResultPatches.cs` |
| FF3 | Job change, job level, "Equipped" | `Patches/JobMenuPatches.cs` |
| FF3 | NPC item selection events | `Patches/EventItemSelectPatches.cs` |
| FF3 | Onion Knight naming | `Patches/NewGameNamingPatches.cs` |
| FF4 | Abilities menu | `Patches/AbilityMenuPatches.cs` |
| FF4 | Party setting and formation rows | `Patches/PartySettingPatches.cs`, `Patches/FormationRowPatches.cs` |
| FF4 | Handedness in status | `Menus/StatusDetailsReader.cs` |
| FF4 | Moon pathfinding | `Field/FieldNavigationHelper.cs` |
| FF4 | Timers | `Patches/TimerPatches.cs` |
| FF5 | Jobs and abilities: change, equip, ABP | `Patches/JobAbilityPatches.cs`, `Utils/JobEquipData.cs` |
| All six | Current-phase results buffer | Shared `battle_result.cpp`, `review_buffer.cpp` |
| FF5 | Enemy letters, landing pings, breadcrumb routing | see sections 3, 7 and 5 |
| FF5 | Timers | `Patches/TimerPatches.cs` |

## 12. Differences the port must decide

Each is a place where the games behave differently today. FF1 is the default unless the difference is game content.

1. **F5 timing.** FF1-4: battle only. FF5: field and field menus only, not in battle.
2. **Mod menu availability.** FF1-4: field only. FF5: field and field menus.
3. **9 for the beacon.** FF3-5 only. FF1-2 have F6 only.
4. **Tab clears stale battle state.** FF1-4 only.
5. **Clear all waypoints.** FF1-3 ask once. FF4-5 ask twice.
6. **Mod mode A for vehicle state.** Missing in FF3.
7. **Mod mode right stick down.** Battle in FF1, 3, 4. Off the field in FF2. Battle or dialogue in FF5.
8. **Mod menu layout.** Five different orders. FF1's is the default.
9. **Auto Detail setting name.** `AutoDetail` in FF1, 5; `AutoDetailOnFocus` in FF2-4.
10. **EXP counter.** Not in FF2.
11. **Multi-hit damage setting.** FF1-3 only.
12. **R on the status screen.** FF3 only.
13. **0 dumps untranslated names.** FF3 only. It is a developer tool.
14. **Two navigation designs.** FF1-3 entity scanner; FF4-5 entity cache, navigator and grouping strategies.
    The plan keeps FF1's design and moves FF5's additions into the core.
15. **FF1's `docs/plan.md` hotkey table is stale.** It lists 0 for "All category" and 9 for beacons.
    The code uses Shift+K and F6.
