# Testing the native mod

Nothing here has been tested in a game yet. Test one game at a time. Report what you hear,
and attach `ffpr_screen_reader.log` from that game's folder if something is wrong.

## Install for testing

1. Close the game.
2. Move the old screen reader out of the game's `Mods` folder (for example to a `Mods-off`
   folder): `FFI_ScreenReader.dll` … `FFV_ScreenReader.dll`. MelonLoader itself can stay.
   If you forget, the new mod says "Old screen reader still installed" at start.
3. From `ffpr-screen-reader`: `deploy.bat ff1` (or ff2 … ff5, or no argument for all five).
   It copies `hid.dll`, `MinHook.x64.dll`, `Tolk.dll`, `nvdaControllerClient64.dll` and
   `SDL3.dll` into the game folder.
4. To undo: `uninstall.bat` (keeps Tolk and SDL3, which the old mods also use), then move the
   old DLL back into `Mods`.

## First start, every game

1. The game starts normally (if it does not, delete `hid.dll` from the game folder and tell me).
2. Speech at the title screen.
3. Your old settings are kept: the first start copies them from MelonPreferences once
   (`ffpr_screen_reader.ini` appears in the game folder).
4. F8 opens the mod menu; arrows move; Escape closes.
5. The log `ffpr_screen_reader.log` has a line "self-check (FFn): … N missing". Send me that
   line: it lists every game name the mod could not find in this game.
6. Keyboard and controller both work in the game and in the mod (hid.dll also carries the
   controller's HID traffic; this is the check the plan's Phase 1 wanted).

## FF1 (the base: run its full section of `..\TEST_LIST.md`)

New game naming and classes; the 15-puzzle; field entity keys and pathfinding; waypoints;
wall tones, footsteps, beacons; menus (items, equipment, magic, status, config, bestiary);
shops; battle (commands, targets, results with the EXP tick); save and load; dialogue and R.

## FF2

1. New game naming. 2. NPC keywords: Ask, Learn, then the field keys still work.
3. Words menu, then I. 4. Field magic: spells with level and percent; I.
5. Battle spell list and I; H says HP and MP. 6. Victory: gil, weapon skills, spell levels,
stat gains, no EXP. 7. Status details: all 23 stats. 8. U in a shop.

## FF3

1. Onion Knight naming. 2. Job menu, then I. 3. Magic menu: Learn with a tome, Use, a target,
I. 4. Battle spell list, I. 5. Status: arrows, Shift and Ctrl jumps, R. 6. Ship and chocobo,
V. 7. An NPC item-select event. 8. Story text that fades in line by line.

## FF4

1. Magic (Ability) menu: commands, spells, targets, I, F7. 2. Status details: Up/Down, Shift,
Ctrl, W/S, "Handed". 3. Party setting: switch a row; character lists say the row.
4. Namingway. 5. Battle spell list, I, target all enemies. 6. Hovercraft and chocobo, V, 9.
7. An escape timer: T and Shift+T.

## FF5

1. Jobs: level and ABP; I. 2. Abilities: spells with MP; equipped slots. 3. Status: arrows,
Shift, LB/RB. 4. Name change. 5. Shift+T, then T. 6. After a battle: L, arrows, Escape.

## Known gaps (details in `PORT_STATUS.md`)

- Pending per game: FF3 battle system messages; FF4 steal results and choice lists; FF5's own
  target reader, results-log pages beyond points, ABP totals.
- Missing translations: "hovercraft", "yellow chocobo" (FF4), "No data available" (FF5).
