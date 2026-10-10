# Input bindings

Source: `src/core/input/` (ports FF1 `InputManager`, `KeyBindingRegistry`, `ControllerRouter`,
`GamepadManager`, `Core/Handlers/*`, `InputPassthroughPatches`). Input never calls a feature
directly: every key and button runs a named action (`actions::run`), and every context check
reads a named query (`queries::is` / `queries::text`). An area that implements a feature defines
the exact name below. An undefined action logs "not available"; an undefined query reads false.

## How a key is dispatched (each frame)

1. Gamepad and keyboard are polled. The keyboard is read only while the game window has the
   focus.
2. While the mod owns input (`ControllerRouter.SuppressGameInput`), `Input.ResetInputAxes` runs.
3. The context is computed (table below) and the controller router runs.
4. `MovementSoundPatches.PollFootsteps` runs if defined.
5. `ui::handle_modal_input()` (mod menu, text input, confirmation dialog). If a modal is open,
   no hotkey runs this frame.
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
| `[` | `Mod.CyclePrevious` | Field |
| Shift+`[` | `Mod.CyclePreviousCategory` | Field |
| `]` | `Mod.CycleNext` | Field |
| Shift+`]` | `Mod.CycleNextCategory` | Field |
| `\` | `NavigationTargetTracker.MarkEntity`, then `Mod.RestartEntityBeacon` (AudioBeacons on) or `Mod.AnnounceCurrentEntity` | Field |
| Shift+`\` | `Mod.TogglePathfindingFilter` | Field |
| Ctrl+`\` | `Mod.ToggleToLayerFilter` | Field |
| `` ` `` | `Mod.ForceEntityRescan` | Field |
| J | `Mod.CyclePrevious` | Field |
| Shift+J | `Mod.CyclePreviousCategory` | Field |
| K | `Mod.AnnounceEntityOnly` | Field |
| Shift+K | `Mod.ResetToAllCategory` | Field |
| L | `Mod.CycleNext` | Field |
| Shift+L | `Mod.CycleNextCategory` | Field |
| P | same as `\` | Field |
| Shift+P | `Mod.TogglePathfindingFilter` | Field |
| Ctrl+P | `Mod.ToggleToLayerFilter` | Field |
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
| `=` | `Mod.CycleNextCategory` | Field |
| `-` | `Mod.CyclePreviousCategory` | Field |
| G | `Mod.AnnounceGilAmount` | Global |
| H | `Mod.AnnounceCharacterStatus` | Global |
| M | `Mod.AnnounceCurrentMap` | Global |
| Shift+M | `Mod.ToggleMapExitFilter` | Global |
| Tab | clears a stuck battle flag: if `BattleStateHelper.IsInBattle` and not `BattleStateHelper.IsBattleControllerAlive`, runs `BattleStateHelper.ForceClearBattleState` | Global |
| V | `GlobalHotkeyHandler.AnnounceCurrentVehicle` (speaks text query `MoveStateHelper.GetMoveStateName`) | Global |
| R | `MessageWindowPatches.RepeatLastDialogue` if `MessageWindowPatches.IsInDialogue` | Global |
| I | `GlobalHotkeyHandler.HandleItemDetailsKey` (below) | Global |
| Shift+I | `ControllerRouter.AnnounceContextControls` | Global |
| U | `UsableByAnnouncer.AnnounceForCurrentContext` | Global |
| F8 (no modifier) | mod menu (`ui::mod_menu::open`) if `ControllerRouter.IsFieldActive`, else `ControllerRouter.SpeakModMenuUnavailable` | any |
| F7 (no modifier) | `Mod.ToggleAutoDetail` | any |
| F5 (no modifier) | in battle: cycles setting `EnemyHPDisplay` (Numbers, Percentage, Hidden), says "Enemy HP: {0}", runs `Settings.Changed.EnemyHPDisplay`; otherwise `ControllerRouter.SpeakModMenuUnavailable` | any |

Game modules add their own keys with `input::add_binding(key, mod, context, "Action.Name", "description")`
from their install (for example FF3–FF6's 9 for beacons, FF4–FF6's T / Shift+T). FF6 also
binds H on the field (airship heading and altitude on the airship, else character status) and
Up/Down and Ctrl+Up/Down in the Global context for its item detail screen (they do nothing
elsewhere; a more specific context's binding, such as the field's Ctrl+arrow teleport, is tried
first).

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
| Right stick up / down / left / right | `Mod.CyclePrevious` / `Mod.CycleNext` / `Mod.CyclePreviousCategory` / `Mod.CycleNextCategory` | up: `GlobalHotkeyHandler.HandleItemDetailsKey`; down: `KeyHelpReader.AnnounceKeyHelp`; left: `UsableByAnnouncer.AnnounceForCurrentContext` |
| Left trigger | `WaypointHandler.PathfindToCurrentWaypoint` if `NavigationTargetTracker.IsWaypoint`; `Mod.RestartEntityBeacon` / `Mod.AnnounceCurrentEntity` if `NavigationTargetTracker.IsEntity`; else "No target selected" | game (Page Up) |
| L3 (released alone) | Stick Click Normalization off: `Mod.ToggleAudioBeacons`; on: a one-frame L3 press to the game | game |
| R3 (released alone) | off: `Mod.TogglePathfindingFilter`; on: one-frame R3 press to the game | game |
| L3 + R3 | `Mod.ToggleStickClickNormalization` | game |
| LB / RB / RT | game | game |
| Puzzle (`PuzzleGameState.IsActive`) | right stick up: `PuzzlePatches.AnnouncePosition`; nothing else | same |

**Mod mode** (every button consumed; one action, then back to Normal)

| Input | Dialogue (`MessageWindowPatches.IsInDialogue`) | Battle | Field |
|---|---|---|---|
| X | `MessageWindowPatches.RepeatLastDialogue` | `Mod.AnnounceCharacterStatus` | `Mod.AnnounceGilAmount` |
| Y | | | `Mod.AnnounceCurrentMap` |
| A | | | `GlobalHotkeyHandler.AnnounceCurrentVehicle` |
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
`GetAnyKey` (one RVA each in all five games). While `ControllerRouter.SuppressGameInput` is true
they return false. Otherwise a true keyboard result is kept and the SDL pad is added: Enter = A,
Cancel = B, Shortcut = X, Menu = Y, SwitchLeft/Right = LB/RB, PageUp = LT (not on the active
field), PageDown = RT, Start never, StickL/StickR = L3/R3, directions = left stick on the active
field and D-pad or left stick elsewhere. A button the router consumed never reaches the game.
Unity's own gamepad devices are disabled while SDL has a pad open.
