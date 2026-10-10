# Testing the native mod

Nothing here has been tested in a game yet. Test one game at a time. Report what you hear,
and attach `ffpr_screen_reader.log` from that game's folder if something is wrong.

## Install for testing

1. Close the game.
2. Move the old screen reader out of the game's `Mods` folder (for example to a `Mods-off`
   folder): `FFI_ScreenReader.dll` … `FFVI_ScreenReader.dll`. If you forget, the new mod says
   "Old screen reader still installed" at start.
   MelonLoader and BepInEx may stay (since 2026-10-07; see "With MelonLoader or BepInEx" below).
3. From `ffpr-screen-reader`: `deploy.bat ff1` (or ff2 … ff6, or no argument for all six).
   It copies `hid.dll`, `Tolk.dll`, `nvdaControllerClient64.dll` and `SDL3.dll` into the game
   folder (MinHook is built into `hid.dll`).
4. To undo: `uninstall.bat` (keeps Tolk and SDL3, which the old mods also use), then move the
   old DLL back into `Mods`.

## With MelonLoader or BepInEx (coexistence, 2026-10-07)

FF1 crashed on 2026-10-06 with MelonLoader installed. The built-in MinHook now stacks with
other loaders' hooks (`tools\coexist_check.cpp` passes offline; the stock MinHook reproduces
the crash). To test, in one game first (FF1, where it crashed):

1. Put MelonLoader back from `..\MelonLoader_backup\<game>`: `version.dll`, `MelonLoader`,
   `Plugins`, `UserLibs` and an EMPTY `Mods` (no old screen reader DLL).
2. Start the game: speech at the title; enter the field (a scene load: the 2026-10-06 crash
   point); a battle; a menu; save and load.
3. The log has "boot: MelonLoader present", and "hooks: coexistence: …". Any "already
   starts with a … jump" lines name functions MelonLoader hooked before us.
4. The first start with MelonLoader can be slow (it generates assemblies); the mod waits up to
   20 minutes for the game to start running.
5. BepInEx 6 (IL2CPP build): the same steps, with BepInEx in place of MelonLoader.

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

Re-test after the 2026-10-10 fixes (first FF2 session; details in `PORT_STATUS.md`):
9. Load a game: nothing but "Entering X" and the map name while the field appears (no "Use").
10. Magic, pick a character: the character once (name, level, HP, MP), then "Use" once.
    Back out of the spell list to the bar: "Use" once again. Equip, Status, Formation: the
    character's name once.
11. Win a battle with gil drops in the item list: each row as the window shows it ("4 Gil"),
    no "Potion".

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

## FF6

1. Title, new game, naming. 2. Field keys, waypoints
(your old FF6 waypoints load). 3. Skills menu: Magic, Lore, Blitz (I reads the input sequence),
Espers and esper details; Gogo's commands. 4. Status details: arrows, Shift, Ctrl, R. 5. Party
setting and Row; Skills sort. 6. Battle: commands, Items, Edgar's Tools, Blitz/Lore/Rage/Dance
lists, I, summons, target all; after battle, Magic AP and learned spells. 7. Airship: H for
heading and altitude. 8. A timer: T, Shift+T. 9. Coliseum: wager and challenger. 10. Opera.

## Known gaps (details in `PORT_STATUS.md`)

- Steal (FF3, FF4, FF5): the result is the game's own line in the battle message box ("Stole
  Potion!"), read by the shared battle-message reader after "Edge: Steal". Check it is heard
  once and not cut off by the next "X's turn".
- Pending per game: FF3 battle system messages; FF4 choice lists; FF5's own
  target reader, results-log pages beyond points, ABP totals.
- Mod text was checked against each game's own text on 2026-10-06 (docs/MODTEXT_MERGE.md);
  listen for terms that still sound wrong in your language.
