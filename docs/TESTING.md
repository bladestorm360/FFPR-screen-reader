# Testing the native mod

Offline checks pass; user gameplay testing has identified regressions recorded in the
[October 11 follow-up](FOLLOWUP_2026_10_11.md). Test one game at a time. Report what you hear,
and attach `ffpr_screen_reader.log` from that game's folder if something is wrong.

## October 11 audit test

Follow-up checks: confirmation bodies speak on entry before their Yes/No choices;
check save, load, quicksave, return to title and quit. Check the title prompt and repeat
world/town/floor transitions for delay. Ordinary FF2 world exits must not announce an
event, and field audio/navigation should recover after loading.

Build, native-name, DLL host, scheduler and baked-route checks are offline validation.
They do not confirm gameplay or measure game startup time.

1. Load each game and note any delay or lag compared with the previous build. Field
   audio and navigation must stop as soon as the battle transition starts.
2. In battle, T reads the active turn; H reads that member's displayed statistics.
   Keys 1–9 review displayed party rows. Controller mod mode provides turn/party review.
3. F5 cycles enemy HP Numbers, Percentage and Hidden. Hidden omits HP. This optional
   disclosure is the user-approved exception to displayed-only speech.
4. Use healing, MP recovery and status cures, then spells/abilities. Targets read only
   their displayed fields. Feedback follows rendered vitals and removed condition icons;
   battle popups read actual displayed amounts and hit counts.
5. Check multi-hit attacks and poison/venom. Field status animation is described without
   an invented numeric damage amount.
6. Win battles with stat gains, job levels or learned abilities. Each native phase/page
   announces on entry. I or L reads current details; arrows review current lines. Speech's
   Auto Detail reads the current phase automatically. FF2 includes displayed evasion/
   accuracy multipliers and skill gains; unused hidden statistics stay absent.
7. Highlight equipment and shop items: read native comparisons/arrows. Number keys review
   displayed multi-member shop comparisons. Bestiary secret item rows must stay secret.
8. F1/F3 report the native post-input speed/encounter settings after loading. F1 on the
   world map explains the speed restriction.
9. Request directions while adjacent to any chest, NPC or interactible: hear the facing
   direction immediately. Far away, hear movement first and facing only on arrival.
10. Check the Dreadnought captain detour, an event-gated world exit, and multi-floor maps.
    Required events do not prune every exit; routes warn about triggers; permanent NPCs
    remain blocked after battle; Events use neutral labels.

Future-floor groups are projected from current save flags. Unsupported initializer control
flow stays conservative; a live current map supplies its actual group and obstacles.
For route failures, report the game, map, target and relevant story state.

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
4. F8 opens the mod menu; select a category, then a setting. Escape returns to categories,
   and Escape there closes. F8 closes directly.
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
stat gains, no EXP. 7. Status details: all currently displayed rows, including evasion and
weapon skills. 8. U in a shop.

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
    In battle, number keys read the currently displayed party rows.
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
   the route goes around. A permanently solid NPC never becomes a walk-through shortcut.
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

## Fourth batch, 2026-10-11: routes kept, facing, town entrances, beacon on the route, labels

Built, smoke-tested, deployed to FF1 and FF3-FF6; FF2 was running (`deploy.bat ff2` once it is
closed). FF2 Salamand is the reference; every step applies to all six games.

1. **A route is kept.** Ask for directions to a house door, walk part of the first leg, ask
   again: the rest of the same route from where you stand, never a leg back the way you came.
   Step off the route and ask: a fresh route from there. Select another target: a new route.
2. **A villager in the street** no longer sends you round the block: the route goes straight
   through its cell (wait a moment for it to move) unless going round costs six steps or less.
   Chests still block.
3. **Josef's house, Salamand.** From the door: Nelly "North 3, East 8, North 3", the Adventurer
   (Ponytail) and the exit all route; "No path" is gone there and in FF3's thirteen maps with
   the same layer name (Ur's houses among them).
4. **Fewer legs.** Across an open room: two legs ("East 6, North 1"), not a staircase of single
   steps. Long routes still read six legs and "and N more".
5. **Facing.** Standing beside an NPC or chest: "face North" (its direction), not "No movement
   needed". A route whose last leg does not already point at the target ends ", face West".
   A shopkeeper's counter: "North 7" alone (the last leg faces the keeper).
6. **Town entrances, pinpoint.** Salamand from the south: "... North N" onto the entrance tile
   itself, entering the town; the same for Altair (enter from the south only) and every town.
   Never "North 1" into a wall beside the town.
7. **Beacon on the route.** Select a target, beacons on: the ping points at the first corner of
   the spoken route (not across walls at the target), speeds up as the corner nears, moves on
   silently at the corner, and stops on the route's last cell. Wander off: it re-plans silently.
   A target with no route: the old low-pitched ping toward it.
8. **Labels for untitled rooms.** "Exit to Salamand Josef's House", "... Salamand Church",
   Fynn "Pub" and "Scott's Room", Poft "Pub" (Cid's room), Castle Fynn "Throne Room", the
   Dreadnought "Prison" and "Engine Room", Bafsk Cave "Pass Room", late Altair "Rebel Hideout
   Revival Chamber"; FF1 "Cornelia Church" (was "Cornelia 2F"), the same in Pravoka, Elfheim,
   Crescent Lake, Onrac and Gaia; FF3 "Topapa's House", "Resurrection", "Healing Spring"; FF4
   "Rosa's House", Troia Castle "Infirmary", Agart "Well"; FF5 Moore "Item Shop", "Boko's
   Room"; FF6 "Village Chief's House", "Rachel's House", Narshe "Back Room". In another
   language the house of Josef reads "Maison de Joseph" (the entity list's name).
   `docs/MAP_LABELS.md` lists every label with its evidence.
9. **Log.** `ffpr_screen_reader.log` shows "[BakedNav] route planned ... / route kept ... / no
   route ..." with start cell, target cell, map id, steps and blockers for every request.
10. **Counters and chests, exact.** In any shop: the clerk routes to the tile in front of its
    counter ("North 3", no facing needed) and never "No path". A chest: the route ends on the
    tile beside it with the facing. A town on the world map: the final leg steps onto one of
    the three tiles of its entrance arc, through the gap in the walls.
11. **Through another floor.** FF2 Kashuan Keep from the entrance, "Exit to Kashuan Keep 2F":
    once the door seal is broken, a route ending ", take the exit to Kashuan Keep 1F" (the
    door north of the entrance); after the transition, ask again: across the other part to
    its exit; after that, the stairs. Before the seal is broken: "Story gated".
12. **Why blocked.** A locked or sealed passage: "Story gated". A guard or other NPC that
    never moves standing in the only corridor: "Blocked by an NPC" (a wandering villager is
    still walked through). FF1 before the bridge: Pravoka says "Story gated"; after it, a
    route. On a world map, a town across the sea: "Requires Ship"; an island: "Requires
    Airship"; FF2 Semitt Falls without the canoe: "Requires Canoe". The vehicle is named
    whether or not you own it yet.
13. **Pathfinding filter, three ways.** Shift+\ (or R3) cycles "Pathfinding filter: Current
    floor" / "Current map with vehicles" / "Current map", and the mod menu shows the same
    three under Navigation Filters. Current floor: only targets walking reaches on this
    sub-map. Current map with vehicles: on the world map with the ship parked here, towns
    across the sea join the list. Current map: everything, including targets routed through
    another floor. Your old on/off setting became Current floor (on) or Current map (off).

## Fifth batch, 2026-10-11: the FF1 bridge, other floors in the list, routes that continue

Built, smoke-tested, deployed to FF1 and FF3-FF6; FF2 was running (`deploy.bat ff2` once it is
closed). `PORT_STATUS.md` items 37-39.

1. **FF1, Cornelia to Matoya's Cave and Pravoka.** From Cornelia on foot with the bridge
   built: a route to each ("North 13, East 3, ..."; about 74 and 92 steps), never "Requires
   vehicle". Before the bridge is built: "Story gated". A trapdoor or a Citadel of Trials pad
   is still never walked across by a route.
2. **Other floors in the list.** Filter on "Current map" (Shift+\ or R3). In a town or a
   dungeon, cycle past the live entries: entries such as "Potion (Kashuan Keep 2F) - Treasure
   Chest", "Nelly (Josef's House) - NPC", "Exit to World Map (Kashuan Keep 1F) - Map Exit"
   follow, floor by floor, nearest floor first, without steps or direction. "Current floor"
   and "Current map with vehicles" do not list them; on the world map nothing is added. A
   chest already opened on another floor is still listed by its contents.
3. **A route to another floor.** Select such an entry and press the directions key: the first
   leg, ending ", take the exit to <floor>". With beacons on, the ping leads along that leg.
4. **The route continues.** Take the exit. After "Entering <floor>", about a second later:
   "<name> (<floor>): <the next leg>" from where you arrived, unasked. The entry stays
   selected: the directions key repeats the rest, the beacon continues. On the destination's
   own floor the last leg ends beside the chest with its "face North", and nothing more is
   spoken at later map changes.
5. **Cut off on this floor, continued.** FF2 Kashuan Keep from the entrance, "Exit to Kashuan
   Keep 2F" (seal broken): "..., take the exit to Kashuan Keep 1F"; after the transition the
   next leg is spoken without asking, and so on to the stairs.
6. **Dropping it.** Cycling to another entry or changing category forgets the remembered
   destination: nothing is spoken after the next map change.
7. **Log.** "[BakedNav] other floors of map N: F floors, O objects", "[EntityScanner] other
   floors: E entries", "[BakedNav] route to map N (x,y) from map M: ... goes on" / "ends
   there", "[EntityScanner] destination kept through the transition: <name>".

## Known gaps (details in `PORT_STATUS.md`)

- Steal (FF3, FF4, FF5): the result is the game's own line in the battle message box ("Stole
  Potion!"), read by the shared battle-message reader after "Edge: Steal". Check it is heard
  once and not cut off by the next "X's turn".
- Pending per game: FF3 battle system messages; FF4 choice lists; FF5's own
  target reader, results-log pages beyond points, ABP totals.
- Mod text was checked against each game's own text on 2026-10-06 (docs/MODTEXT_MERGE.md);
  listen for terms that still sound wrong in your language.
