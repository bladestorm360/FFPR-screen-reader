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

Second batch (same day):
12. World map while the Dreadnought flies: the entity list has no "Dreadnought" NPC (the parked
    one at Bafsk is a map exit, as before).
13. No "Level" anywhere: character pre-screens, the status list, save slots.
14. Late game, the real Hilda: "Hilda (true)", not Japanese.
15. In a weapon or armour shop, on an item: keys 1 to 4 say each party member's name and
    "Stronger", "Weaker", "No change" or "Cannot equip", plus "Equipped" when the mark shows.
    Outside a shop the number keys do nothing.
16. Any NPC name still spoken in Japanese: tell me the name; the dictionary audit says FF2
    has none left.
17. NPC list in a town with several identical NPCs: "Man (Blue Clothes) 1" to "4" and
    "Palamecian Soldier 1" to "5", the numbers unchanged after walking; "Borghen" with no
    number. FF6 with its language set to anything but English: field names in that language.

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

## Third batch, 2026-10-10: save popups, vehicles, baked maps, puzzle guides (all six games)

Built and smoke-tested, deployed, not tested in game. Steps for every game unless named.

1. **Save complete.** Save from the menu at a save point: the confirmation reads as before;
   after the write, "Save complete" (the game's own text) plus the button. Quick save: the
   same, "Save complete" after the write. Load popups unchanged.
2. **FF2 vehicles.** Pick up the snowcraft and ride onto the snowfield: "On Snowcraft" (the
   game's key-item name), V says "Snowcraft"; leaving it: "On foot". The canoe says "On
   Canoe". Ship and airship: the game's names ("On Ship", "On Airship"). FF3: the canoe is
   "Canoe", not "ship". FF4: "On Hovercraft", "On Enterprise", chocobos by the game's names.
3. **FF2 character slots.** Magic, Equip, Status, Formation: "Firion, HP 155/244, MP 10/10" -
   the name once, no second name, no level. The status list the same.
4. **FF2 Altair, late game.** In the rebel hideout rooms after the empire's attack: "Entering
   Altair Rebel Hideout" and the exits say the same, not "Altair" alone.
5. **Routing, any map.** Pick a chest or NPC far away (more than 32 tiles): directions come
   at once, no "No path". Long routes read six legs, then "and N more": walk them and ask
   again. A shopkeeper: the route ends on the tile in front of the counter. Stairs: the route
   goes up or down through them. A target behind a secret passage (FF3-FF5): the route goes
   through the passage, with wall tones at its mouth. Standing next to an NPC in a corridor:
   the route goes round, or through when there is no other way.
6. **World map.** On foot: a town across water gets "board the Ship, East 40, ..." (ship
   parked and owned) or "board the Airship, ..., land, ..."; with the canoe (FF1-FF3) or
   the snowcraft (FF2) owned, rivers / snow count as walkable in the directions. In a vehicle:
   routes stay on water or in the air. The pathfinding filter (Shift+\) hides targets across
   water while you are on foot (it never boards), as the old filter hid targets out of range.
7. **Puzzle guide.** Shift+G off a guided map: "No puzzle guide for this map". On one: "Puzzle
   guide: <area>", then "Step 1 of N: <what to do>. <directions>"; reaching the step: "Step
   done" and the next; the last: "Puzzle guide finished"; Ctrl+G: "Puzzle guide stopped".
   Guided maps (data/ffN/puzzles.json): FF1 Citadel of Trials (one step: the 3F warp pad;
   the directions take the 2F pads: "step on the warp"); FF3 Ur, Altar Cave B3, Mythril
   Mines, Tower of Owen 5F, Molten Cave hidden-door switches; FF4 Troia Castle B1 (switches 1
   to 4), Castle Baron switches, Devil's Road; FF5 Fire-Powered Ship engine room; FF6 Zozo
   clock (6:10:50), Darill's Tomb stones, Ancient Castle hidden switch and stairs, Cultists'
   Tower switch, Owzer's flower painting. Pad: mod mode + RB / LB.
8. **Across floors.** A guide step on another floor: "North 3, take the exit to Snow Cavern
   B3" (or "step on the warp"); after the transition, Shift+G (or the map change) gives the
   next leg from the new position.
9. **Nothing worse.** Wall tones, footsteps, beacons, waypoints (Slash) and teleports as
   before; a map the baked data does not know (log: "[BakedMap] ... using the game's
   searcher") falls back to the old routing.

## Known gaps (details in `PORT_STATUS.md`)

- Steal (FF3, FF4, FF5): the result is the game's own line in the battle message box ("Stole
  Potion!"), read by the shared battle-message reader after "Edge: Steal". Check it is heard
  once and not cut off by the next "X's turn".
- Pending per game: FF3 battle system messages; FF4 choice lists; FF5's own
  target reader, results-log pages beyond points, ABP totals.
- Mod text was checked against each game's own text on 2026-10-06 (docs/MODTEXT_MERGE.md);
  listen for terms that still sound wrong in your language.
