# Input bindings

Source: `src/core/input/` (ports FF1 `InputManager`, `KeyBindingRegistry`, `ControllerRouter`,
`GamepadManager`, `Core/Handlers/*`, `InputPassthroughPatches`). Input never calls a feature
directly: every key and button runs a named action (`actions::run`), and every context check
reads a named query (`queries::is` / `queries::text`). An area that implements a feature defines
the exact name below. An undefined action logs "not available"; an undefined query reads false.

## Shared audit additions, October 11

Battle: T reads the active turn, H the active member's rendered statistics, and 1–9 the
visible party rows. Enemy HP's optional Numbers/Percentage/Hidden modes remain on F5.
Controller mod mode: North (Y/Triangle) reads the turn; shoulders review party members.

Results: Up/Down review current lines; Left/Right first/last; I reads current details.
On growth/FF2 weapon phases, I/controller West reads all current-phase deltas, regardless
of the highlighted row (or No stat changes when the phase has none).
Each native phase replaces the buffer; there is no separate L results log. Displayed
EXP/gil totals are always announced. Member EXP, item/drop rows and stat tables follow
Autodetail. Battle's persistent `Battle results: changed stats` toggle defaults on and
summarizes displayed changes without reading the entire stat table. FF2 weapon rows
retain character/hand, actual weapon type, level and gauge percentage; their automatic delta summary
requires the toggle even with Autodetail on. Other games retain member EXP rows instead
of the FF2 weapon buffer. Review input is consumed before both native input APIs, including held/release
keys and the interval before the buffer is ready. Ordinary confirm advances normally.
[Native phase/input evidence](re/RESULTS.md).
Controller mod mode: West (X/Square) reads details; shoulders or D-pad Up/Down review lines, and D-pad
Left/Right goes first/last. Status retains
its grouped arrow/Shift/Ctrl review and native page controls. No keyboard mod mode exists.

F8 settings opens on the field: select a category and then a setting. Escape/B returns to
categories and closes from the root; F8 closes directly. F1/F3 read actual native changes
after game input, including loaded encounter state and the world-map speed restriction.
The categories are Speech, Navigation, Navigation audio, Battle, followed by Close Menu.
Navigation contains autowalk, path speech, filters and stick-click normalization. Audio
volumes are beside their related settings; EXP counter sound and volume belong to Navigation audio.
All modal keyboard input requires game-window focus. Alt+Tab retains the current menu
position; held keys on focus return do not act until released and pressed again.

## October 14 visible-information follow-up

U and its menu/controller eligibility actions are removed in all games. Current
party equipment comparisons in shops retain their existing 1–9 bindings. Native
Items equipment-information pages provide their own review buffer, with visible
stats, eligibility names/colors and pager information; I/controller West repeats
that current buffer. Replacement comparisons retain I for the live native panel.
FF3 job I/Autodetail names its rendered Equippable weapon/armor icons.

## How a key is dispatched (each frame)

1. Gamepad and keyboard are polled. The keyboard is read only while the game window has the
   focus.
2. While the mod owns input (`ControllerRouter.SuppressGameInput`), `Input.ResetInputAxes` runs.
3. The context is computed (table below) and the controller router runs.
4. `MovementSoundPatches.PollFootsteps` runs if defined.
5. `ui::handle_modal_input()` (mod menu, text input, confirmation dialog). Its shared sampler
   checks foreground focus before any hardware-key read and seeds held keys on return.
   If a modal is open, no hotkey runs this frame.
6. Hotkeys run only while the game window is focused, a key went down, and no Unity
   `InputField` is selected.
7. F8, then F5 / F7 (no modifier held), then the registry. With Alt held, the registry is
   skipped.

The registry matches the modifier exactly (None, Shift, Ctrl, Ctrl+Shift). A binding runs when
its context is Global or the active context.

## Context

Checked in this order:

| Context | Query |
|---|---|
| KeyHelp | `KeyHelpReader.IsScreenActive` |
| Status | `StatusNavigationTracker.IsNavigationActive` and `StatusNavigationTracker.ValidateState` (if only one is defined, that one) |
| BestiaryDetail | `BestiaryNavigationTracker.IsNavigationActive` and `BestiaryNavigationTracker.ValidateState` (same rule) |
| Battle | `BattleStateHelper.IsInBattle` |
| Shop | `ShopMenuTracker.IsShopMenuActive` and `ShopMenuTracker.ValidateState` (native, 2026-10-10: the shop is open; Global keys still match) |
| Field | a `Last.Map.FieldPlayerController` with a `fieldPlayer`, and `menu_state::any_active()` false |
| Global | otherwise |

## Keyboard

| Key | Action | Context |
|---|---|---|
| 1 to 9 | `ShopPartyReader.AnnounceSlot1` to `AnnounceSlot9`: active-party slot N's name and equipment comparison for the focused item (Stronger / Weaker / No change, Equipped, Cannot equip) | Shop |
| Down | `StatusNavigationReader.NavigateNext` | Status |
| Shift+Down | `StatusNavigationReader.JumpToNextGroup` | Status |
| Ctrl+Down | `StatusNavigationReader.JumpToBottom` | Status |
| Up | `StatusNavigationReader.NavigatePrevious` | Status |
| Shift+Up | `StatusNavigationReader.JumpToPreviousGroup` | Status |
| Ctrl+Up | `StatusNavigationReader.JumpToTop` | Status |
| Down | `BestiaryNavigationReader.NavigateNext` | BestiaryDetail |
| Shift+Down | `BestiaryNavigationReader.JumpToNextGroup` | BestiaryDetail |
| Ctrl+Down | `BestiaryNavigationReader.JumpToBottom` | BestiaryDetail |
| Up | `BestiaryNavigationReader.NavigatePrevious` | BestiaryDetail |
| Shift+Up | `BestiaryNavigationReader.JumpToPreviousGroup` | BestiaryDetail |
| Ctrl+Up | `BestiaryNavigationReader.JumpToTop` | BestiaryDetail |
| Down | `KeyHelpReader.NavigateNext` | KeyHelp |
| Ctrl+Down | `KeyHelpReader.JumpToBottom` | KeyHelp |
| Up | `KeyHelpReader.NavigatePrevious` | KeyHelp |
| Ctrl+Up | `KeyHelpReader.JumpToTop` | KeyHelp |
| W / S / A / D | the Up / Down / Left / Right binding, modifiers carried | Status, BestiaryDetail, KeyHelp only |
| J / L (Letters) or `[` / `]` (Brackets) | `Mod.CyclePrevious` / `Mod.CycleNext` | Field |
| Shift+J / Shift+L (Letters) or Shift+`[` / Shift+`]` (Brackets) | `Mod.CyclePreviousCategory` / `Mod.CycleNextCategory` | Field |
| K (Letters) or P (Brackets) | `Mod.AnnounceEntityOnly` | Field |
| Shift+K (Letters) or Shift+P (Brackets) | `Mod.ResetToAllCategory` | Field |
| P (Letters) or Backslash (Brackets) | `Mod.NavigateToEntity`: directions/beacons or start/stop autowalk | Field |
| Shift+P (Letters) or Shift+Backslash (Brackets) | `Mod.TogglePathfindingFilter` | Field |
| Ctrl+P (Letters) or Ctrl+Backslash (Brackets) | `Mod.ToggleToLayerFilter` | Field |
| Backtick | `Mod.ForceEntityRescan`: refresh navigation, no data dump | Field |
| `,` | `WaypointHandler.CyclePrevious` | Field |
| Shift+`,` | `WaypointHandler.CyclePreviousCategory` | Field |
| `.` | `WaypointHandler.CycleNext` | Field |
| Shift+`.` | `WaypointHandler.CycleNextCategory` | Field |
| Ctrl+`.` | `WaypointHandler.RenameCurrentWaypoint` | Field |
| `/` | `WaypointHandler.PathfindToCurrentWaypoint` | Field |
| Shift+`/` | `WaypointHandler.AddNewWaypointWithNaming` | Field |
| Ctrl+`/` | `WaypointHandler.RemoveCurrentWaypoint` | Field |
| Ctrl+Shift+`/` | `WaypointHandler.ClearAllWaypointsForMap` | Field |
| Ctrl+Up / Down / Left / Right | `Mod.TeleportNorth` / `Mod.TeleportSouth` / `Mod.TeleportWest` / `Mod.TeleportEast` | Field |
| `'` | `Mod.ToggleFootsteps` | Field |
| `;` | `Mod.ToggleWallTones` | Field |
| F6 | `Mod.ToggleAudioBeacons` | Field |
| Shift+F6 | `Mod.ToggleBeaconMode`: Follow route / Object location | Field |
| Shift+G | `PuzzleGuide.Next`: start the guide for this map, or the current step again with directions from here (`src/core/field/puzzle_guide.cpp`) | Field |
| Ctrl+G | `PuzzleGuide.Stop` | Field |
| G | `Mod.AnnounceGilAmount` | Global |
| H | `Mod.AnnounceCharacterStatus` | Global |
| M | `Mod.AnnounceCurrentMap` | Global |
| Shift+M | `Mod.ToggleMapExitFilter` | Global |
| Tab | clears a stuck battle flag: if `BattleStateHelper.IsInBattle` and not `BattleStateHelper.IsBattleControllerAlive`, runs `BattleStateHelper.ForceClearBattleState` | Global |
| V | `GlobalHotkeyHandler.AnnounceCurrentVehicle` (speaks text query `MoveStateHelper.GetMoveStateName`) | Global |
| R | `MessageWindowPatches.RepeatLastDialogue` if `MessageWindowPatches.IsInDialogue` | Global |
| I | `GlobalHotkeyHandler.HandleItemDetailsKey` (below) | Global |
| Shift+I | `ControllerRouter.AnnounceContextControls` | Global |
| F8 (no modifier) | mod menu (`ui::mod_menu::open`) if `ControllerRouter.IsFieldActive`, else `ControllerRouter.SpeakModMenuUnavailable` | any |
| F7 (no modifier) | `Mod.ToggleAutoDetail` | any |
| F5 (no modifier) | in battle: cycles setting `EnemyHPDisplay` (Numbers, Percentage, Hidden), says "Enemy HP: {0}", runs `Settings.Changed.EnemyHPDisplay`; otherwise `ControllerRouter.SpeakModMenuUnavailable` | any |

The persisted `NavigationKeymap` enum (F8 > Navigation) selects Letters (0, default) or
Brackets and backslash (1). Registry bindings carry a layout tag and exact modifier;
inactive entries never dispatch. P reads the destination in layout 1, where K is inactive.
Autowalk recognizes only the selected layout's navigation key or the common Slash waypoint key.
Minus/Equals category aliases and the FF3-FF6 field 9 beacon alias are removed; F6 is shared.
FF3's status R and FF5's landing-ping shortcut are removed to follow FF1. Landing pings
remain in the shared menu in FF1/FF2/FF3/FF5. F5/F6/F7/F8 retain their existing roles;
FF4-FF6 timer freezing now uses Shift+F5 rather than Shift+T. Native keys are not remapped.

The only remaining game-module registrations are T/Shift+F5 for FF4-FF6 timers and
FF6's field H extension (airship heading/altitude, otherwise the shared character status).
All common battle/shop/status/results/information/modal keys come from shared core files.
Ctrl+arrow and controller mod-mode stick teleport require `TeleportShortcuts` (default off).
The action itself rechecks the setting/readiness and validates a fresh owned landing snapshot.

### I key cascade (`GlobalHotkeyHandler.HandleItemDetailsKey`)

The first match runs:

| Query | Action |
|---|---|
| `PuzzleGameState.IsActive` | `PuzzlePatches.AnnouncePosition` |
| an active `ConfigActualDetailsControllerBase` (KeyInput or Touch) | speaks its `descriptionText` (input area) |
| `JobSelectionPatches.IsActive` | speaks text query `JobSelectionPatches.GetCurrentDescription` (or "No description") |
| `ShopMenuTracker.ValidateState` | `ShopDetailsAnnouncer.AnnounceCurrentItemDetails` |
| `EquipMenuState.IsActive` | `EquipDetailsAnnouncer.AnnounceCurrentItemDetails` |
| `MagicMenuState.IsSpellListActive` | `MagicDetailsAnnouncer.AnnounceCurrentSpellDescription` |
| `ItemMenuState.IsItemMenuActive` | `ItemDescriptionAnnouncer.AnnounceCurrentDescription` |
| `BattleMagicMenuState.IsActive` | `BattleMagicPatches.AnnounceCurrentDescription` |
| `BattleItemMenuState.IsActive` | `BattleItemPatches.AnnounceCurrentDescription` |

## Controller (SDL3)

Labels follow the pad family (A/B/X/Y, Cross/Circle/Square/Triangle, Nintendo).

**Any state**

| Button | Effect |
|---|---|
| Start | mod menu open (only when `ControllerRouter.IsFieldActive`) or close; else `ControllerRouter.SpeakModMenuUnavailable`. Never reaches the game. |
| Back / View | toggles mod mode ("Mod" / "Cancelled") |

**Normal state**

| Input | Field active | Elsewhere |
|---|---|---|
| A / B / X / Y | silence speech, pass to game | silence speech, pass to game |
| D-pad up / down / left / right | `WaypointHandler.CyclePrevious` / `CycleNext` / `CyclePreviousCategory` / `CycleNextCategory` (consumed) | game; in Status, BestiaryDetail, KeyHelp: up/down (also left stick) run `StatusNavigationReader`, `BestiaryNavigationReader`, `KeyHelpReader` `.NavigatePrevious` / `.NavigateNext` |
| Right stick up / down / left / right | `Mod.CyclePrevious` / `Mod.CycleNext` / `Mod.CyclePreviousCategory` / `Mod.CycleNextCategory` | up: `GlobalHotkeyHandler.HandleItemDetailsKey`; down: `KeyHelpReader.AnnounceKeyHelp` |
| Left trigger | `WaypointHandler.PathfindToCurrentWaypoint` if waypoint; `Mod.NavigateToEntity` if entity; else "No target selected". Both use the same autowalk/settings policy. | game (Page Up) |
| L3 (released alone) | Stick Click Normalization off: `Mod.ToggleAudioBeacons`; on: a one-frame L3 press to the game | game |
| R3 (released alone) | off: `Mod.TogglePathfindingFilter` (cycles the three filter modes); on: one-frame R3 press to the game | game |
| L3 + R3 | `Mod.ToggleStickClickNormalization` | game |
| LB / RB / RT | game | game |
| Puzzle (`PuzzleGameState.IsActive`) | right stick up: `PuzzlePatches.AnnouncePosition`; nothing else | same |

**Mod mode** (every button consumed; one action, then back to Normal)

| Input | Dialogue (`MessageWindowPatches.IsInDialogue`) | Battle | Field |
|---|---|---|---|
| X | `MessageWindowPatches.RepeatLastDialogue` | `Mod.AnnounceCharacterStatus` | `Mod.AnnounceGilAmount` |
| Y | | | `Mod.AnnounceCurrentMap` |
| A | | | `GlobalHotkeyHandler.AnnounceCurrentVehicle` |
| RB / LB | | | `PuzzleGuide.Next` / `PuzzleGuide.Stop` |
| R3 / L3 (normalization on) | | | `Mod.TogglePathfindingFilter` / `Mod.ToggleAudioBeacons` |
| Right stick up / down / left / right | | down: mod-mode controls | `Mod.TeleportNorth` / `South` / `West` / `East` |

**Mod menu** (every button consumed): D-pad or left stick up/down `ui::mod_menu::navigate_previous` /
`navigate_next`, left/right `adjust_current_item(-1 / +1)`, A `toggle_current_item`, B closes,
right stick down speaks the menu controls.

## Published by the input area

Actions: `Mod.TogglePathfindingFilter`, `Mod.ToggleMapExitFilter`, `Mod.ToggleToLayerFilter`,
`Mod.ToggleWallTones`, `Mod.ToggleFootsteps`, `Mod.ToggleAudioBeacons`, `Mod.ToggleAutoDetail`,
`Mod.ToggleAnnounceOnBeaconRestart`, `Mod.ToggleMenuPositionAnnouncements`, `Mod.ToggleExpCounter`,
`Mod.ToggleStickClickNormalization` (each: flip, "X on/off", then `Settings.Changed.<Key>` if
defined), `Mod.AnnounceGilAmount`, `Mod.AnnounceCharacterStatus`,
`ControllerRouter.AnnounceContextControls`, `ControllerRouter.SpeakModMenuUnavailable`,
`ControllerRouter.SyncWithModMenuOpen`, `ControllerRouter.SyncWithModMenuClosed`,
`ControllerRouter.Reset`, `GlobalHotkeyHandler.AnnounceCurrentVehicle`,
`GlobalHotkeyHandler.HandleItemDetailsKey`.

Queries: `ControllerRouter.IsFieldActive`, `ControllerRouter.SuppressGameInput`,
`ControllerRouter.IsNormal`, `ControllerRouter.IsModMode`, `ControllerRouter.IsModMenu`,
`ControllerRouter.LastDeviceIsController`, `GamepadManager.IsAvailable`, `GamepadManager.IsSdlReady`.
Text queries: `ControllerRouter.State`, `ControllerRouter.CurrentGameContext`,
`ControllerLabels.GetButtonLabel.<South|East|West|North|Back|Guide|Start|LeftStick|RightStick|LeftShoulder|RightShoulder|DpadUp|DpadDown|DpadLeft|DpadRight>`,
`ControllerLabels.GetLeftTriggerLabel`, `ControllerLabels.GetRightTriggerLabel`,
`ControllerLabels.GetLeftStickClickLabel`, `ControllerLabels.GetRightStickClickLabel`.

Header `src/core/input/input.h`: key state (`key_pressed`, `key_held`, `shift_held`, `ctrl_held`),
gamepad state, labels, router state, `add_binding`, `save_and_announce`.

## Game input passthrough

Hooks on `System.Input.InputSystemManager` `GetKeyDown(InputActionType)`, `GetKey`, `GetKeyUp`,
`GetAnyKey` (one RVA each in all six games), plus InputListener GetKeyDown/GetKey/GetKeyUp
and AnyKey/AnyKeyDown/AnyKeyUp. Prefixes skip native checks for owned results gestures. While `ControllerRouter.SuppressGameInput` is true
they return false. Otherwise a true keyboard result is kept and the SDL pad is added: Enter = A,
Cancel = B, Shortcut = X, Menu = Y, SwitchLeft/Right = LB/RB, PageUp = LT (not on the active
field), PageDown = RT, Start never, StickL/StickR = L3/R3, directions = left stick on the active
field and D-pad or left stick elsewhere. A button the router consumed never reaches the game.
Unity's own gamepad devices are disabled while SDL has a pad open.

## Keyboard, flee and safe teleport audit, October 15

All direct keyboard samplers, registry additions, modal handlers and game modules were
reviewed. There is no entity-name dumper, map collector, diagnostic/export hotkey or F9-F12
binding in the active native mod. Old translation strings do not register actions. Backtick
only requests the event-bound navigation rescan. Ordinary logging and offline extraction
tools are separate from player input.

Native default flee: FF1-FF3 expose command type 5 (Flee) in their command tables.
FF4-FF6 instead display Press and Hold: Flee; hold keyboard Alpha2+Alpha3, Xbox LB+RB,
PlayStation L1+R1 or Nintendo L+R in normal mode. The local extracted command tables and
system_en.txt confirm these distinctions (FF5's command 93 is a monster action).
`BattleMenuController.IsEscapeInput` reads both held `System.Input.Key.SwitchRight`
(static offset 0x50) and SwitchLeft (0x48), using InputListener.GetKey with device=3.
InputDeviceUtility's native keyboard-default table maps GameKey 8/9 to Unity KeyCode 50/51
(Alpha2/Alpha3); its three controller-default tables map them to actions 8/9. The mod's
SDL manager maps those actions to LB/RB. User remaps retain native ownership.

| Game | IsEscapeInput RVA | InputDeviceUtility.cctor RVA |
| --- | --- | --- |
| FF1 | 439E50 | 903060 |
| FF2 | 62ED40 | A4D9D0 |
| FF3 | 6A1790 | A00A50 |
| FF4 | 39AFB0 | 4EE2D0 |
| FF5 | 43D8F0 | A751F0 |
| FF6 | 8B9BB0 | CBB820 |

Disassembly evidence is local under build_check/ffN_flee_input.txt and
ffN_input_defaults.txt, from the installed matching GameAssembly DLLs and sibling dumps.
The direct InputListener battle path bypassed the manager's SDL injection. The held listener
now forwards only SwitchLeft/Right in battle for controller/all-device queries. Native true
results, consumed buttons and mod-mode/modal suppression retain priority. No escape command,
eligibility, timing or success rule is changed. Results/information passthrough is retained.

Teleport clearance uses the existing native-owner EntityCursor/CaptureBudget and owned
ExitSnapshot on the Navigation worker; no engine pointer or cursor is sent to it. It checks
prepared terrain, current movement layer/rules, copied NPC/chest/gate/directional blockers,
active event/trap cells and a legal neighboring escape using the production routing predicates.
No route search or position write occurs during capture. Same-floor on-foot requests only;
missing data, riding, blocked landings, >8192 collection slots, changed map/player/source cell,
selection, focus/readiness/settings, movement, dialogue, controller ownership or a result older than 500 ms fail
closed. An expiry task cancels queued work without waiting. The native owner reacquires the
transform and preserves z before its single approved relocation. These are feature-specific
stale/work limits, not measured gameplay latency guarantees. Moving actors can change after
observation, as with ordinary native movement; gameplay confirmation remains separate.

Canonical build and all included offline regressions pass, including 6,736 native-name checks
(zero missing), 121 used/316 defined action/query names (zero undefined), both keymaps,
all-six menu rows and held flee injection, and the real Fynn clearance snapshot. The terrain
fixture rejects walls, enclosed tiles, direction-mask traps, occupied landing/escape cells,
event triggers, pitfalls, bounds and invalid layers. The actual teleport request fixture
verifies deferred single relocation, preserved z, disabled/unsafe/riding rejection and
expiry/setting/map/selection/source/focus/transition/movement cancellation. Worker-owner
checks pass. Isolated DLL smoke passes; no runtime performance/gameplay claim is made.
The local audit receipt is build_check/input_audit_2026_10_15.json (including all-six binary
and dump hashes, the seven remaining intentional module bindings and 14 x 12 translations). Luna handled only
the known menu/prefs/12-locale strings and modal checks; Sol reviewed and integrated those changes,
implemented dispatch/controller/clearance and owns all verification. No game launch/probe. Deployment is explicitly held by the user pending another issue;
installed games/dependencies and the public release are unchanged.

Checked local DLL: `97F3865A178EC511EEEBD6E99BB55543A75059D861C340093B2CD2DEE8F695E1` (9,153,024 bytes). Build and isolated smoke pass.
Deployment is held; installed DLLs and dependencies were not written. No commit/push or release publication.
# Soundscape and masters — October 15

Shared field bindings in every game/layout: Shift+Semicolon runs `Soundscape.RangeDown`;
Shift+Apostrophe runs `Soundscape.RangeUp`. Both are no-ops when Soundscape is disabled.
Ctrl+Apostrophe runs `Mod.ToggleSoundscape` in both layouts, using the same persistent
master switch as F8. It does not change AudioBeacons or spoken-route preferences.
Distance defaults to 10, clamps to 5–25 and changes by five. No existing shifted binding
was displaced. The unshifted wall-tone/footstep keys and F6 beacon remain independent.

F8/controller settings now begin Speech with Game speech (default on), and Navigation audio
with Navigation audio master (default on), HRTF (default off), Soundscape (default off),
then its nested settings page. The latter has master volume, distance and all eight entity
cue toggle/volume/preview groups. Sound previews covers the other ten effects, conditional
on each game's existing EXP/landing support. Back restores immediate parent focus.
The speech-only mod menu has explicit central `say_menu` ingress and remains audible when
gameplay speech is muted. No backend bypass, keyboard mode or diagnostic binding was added.
[Implementation/evidence](re/SOUNDSCAPE.md), [pending user checks](TESTING.md).


## Beacon targeting modes — October 15

Shift+F6 switches the persistent BeaconMode setting in both navigation layouts and all
six games: 0 Follow route (default), 1 Object location. F6 retains AudioBeacons on/off.
The same choice is under F8 > Navigation audio, after Beacon Volume. A mode change
announces the selected mode and resets beacon cadence/arrival state without enabling
an off beacon or changing soundscape, autowalk, spoken directions or HRTF preferences.
Existing manual-input safety still stops an active autowalk on Shift+F6.
Object mode uses current selected entity/waypoint coordinates, with world-edge wrapping,
and makes no follow/route/quick-search request. Another-floor projected entities have no
local bearing; an explicit navigation request explains the current-floor requirement.
Follow route preserves its current route-corner/cross-floor behavior. Mode is shared
across the HRTF/stereo volume banks. [Evidence](re/SOUNDSCAPE.md#beacon-targeting-modes--october-15).
