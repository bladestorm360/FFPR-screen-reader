# Testing the native mod

## Persistent cross-floor chest state, October 15

Test first in the next FFII session: choose F8 > Navigation > Current map, then browse
an already-opened Deist chest on another floor. Expect `Opened Treasure Chest ([floor])`.
An unopened chest should say Unopened and its floor; neither should reveal contents
with treasure spoilers off. Change floors and repeat: the state must remain available.
Then repeat the Conversation route below, now using correctly copied story flags.

The native flag-array mismatch was confirmed in all six games and reproduced/fixed in
the actual-reader offline check. See [persistent-state evidence](re/MAP_STATE.md).

## Event tracking across floors, October 15

Test this first when resuming the reported FFII route:

1. In F8 > Navigation, set the filter to Current map. Other-floor events are deliberately
   excluded by Current floor and Current map with vehicles.
2. Select Conversation on Deist B2 and follow the route into B1. Repeat the selected
   object: it should still be Conversation, with Deist Cavern B2 as its floor.
3. The B1 leg reaches a different B2 entrance. After entering B2 there, the remaining
   route should be three steps to the interaction, followed by facing guidance. It
   must not choose a new object or send you back around the same detour.
4. Try both spoken directions and Follow route beacon, with soundscape on if desired.
   An event in another connected multi-floor area should also retain its identity.
   After a story event genuinely disappears, tracking should stop. Explicit next/previous
   browsing should still work, without automatically selecting an unrelated target.

The production three-leg replay and all-six bake audit are offline evidence; the full
in-game traversal remains to be confirmed. See [event tracking evidence](re/EVENT_TRACKING.md).


## Object browsing floors and chest state, October 15

After the offline checks pass, test these first when a suitable dungeon is available:

1. Select Current map in Navigation. Browse an NPC, event, exit or other listed object
   on another floor: its floor should be included, with no steps or compass bearing.
2. Browse a same-floor object reached by going through another floor. It should say
   `Requires travelling through [floor] first`, with no distance. A nearby directly
   reachable object should still say its steps and direction.
3. Browse a destination requiring a warp, trap or intervening event: no direct distance.
   Requesting directions/beacon should still work normally. Rapid browsing or switching
   categories must not let an older object's result interrupt the new selection.
4. Open a chest, change floors, then browse it under Current map. Where saved state is
   known, expect `Opened Treasure Chest ([its floor]) - Treasure Chest (N of M)`.
   An unopened chest keeps both its state and floor too; unknown state reveals neither
   an invented state nor contents. Do not enable treasure spoilers for this check.

Offline ownership/fixtures and limits: [browsing evidence](re/THREADING.md#object-browsing-floors-and-direct-distance-october-15).


## Keyboard, flee and safe teleport follow-up, October 15

Implemented, offline verified and deployed/hash-verified in all six games on October 15
after the user lifted the hold. The public release is unchanged. Check these first
where your current save permits:

1. On an active field, press F6 twice: beacon state should change once per press. Field 9
   should not toggle it. F7 and F8 retain Autodetail/settings behavior in every game.
2. F8 > Navigation > Navigation keymap: Letters uses J/L to browse, K to announce and P to
   navigate. Brackets and backslash uses brackets to browse, P to announce and backslash
   to navigate; K must do nothing. Shift+P resets All in the brackets layout; Shift+backslash
   cycles its filter. Close/reopen settings to confirm the selection remains saved.
3. Teleport shortcuts defaults off: Ctrl+arrow and mod-mode right stick must not teleport.
   Enable it in Navigation, while standing on foot near an accessible entity. A wall or
   occupied side should say no safe landing and leave you where you were. An open side
   should announce the teleport and allow normal movement afterward. Try a different side
   when refused; the mod does not choose an undisclosed fallback location.
4. In an escapable FF4-FF6 battle, hold the main keyboard 2+3, or both shoulder buttons
   (Xbox LB+RB, PlayStation L1+R1, Nintendo L+R), with controller mod mode off. The native
   escape attempt should run. A boss/no-escape battle cannot confirm this check. FF1-FF3
   retain their Flee menu command. Controller mod-mode shoulders still review the party.
5. When an FF4-FF6 timer is available, T announces it; Shift+F5 freezes/resumes it.
   Shift+T no longer changes that setting. Landing Pings remains in Navigation audio for
   FF1/FF2/FF3/FF5.

Failure symptoms: two layouts acting together, brackets-layout P navigating, 9 changing
beacons, an off/blocked/stale teleport moving you, movement stuck after a clear landing,
or held shoulders failing only with the mod. Retain the affected game's own session log
and the selected keymap/destination. No broad recording or tester-log access is requested.
Offline evidence covers actual dispatch/menu/listener code, production terrain/blocker
clearance, queued-worker cancellation and native-owner position writes. Moving actors
can change after observation; actual native timing and gameplay remain unconfirmed.
[Controls, native evidence, ownership and bounds](INPUT_BINDINGS.md#keyboard-flee-and-safe-teleport-audit-october-15).

Latest trap follow-up: deployed/hash-verified in all six. Full build/offline checks and isolated smoke pass for safe-first
drop routing, repeat protection, same-map recovery, selected-event warnings and Deist
guidance. Final deployment status is in [TRAPS.md](re/TRAPS.md). Test these first
when progression permits; new gameplay confirmation remains pending:

1. FF2 Deist: select the former Scene trigger. It should say Trap. Request directions
   to it; it should not add an event-along-route warning. Trigger it and wait for normal
   control on B4. P and destination browsing should work again.
2. From B3 select chests on other floors, or press Shift+G. Routes should use safe
   stairs across floors rather than send you into that avoidable pitfall. Follow the
   guide to B5; it should finish on arrival.
3. When a destination truly needs a drop, expect a brief trap warning. After falling,
   the next route should move toward the destination, without taking stairs back to
   the same trap. Repeat this on available FF5/FF6 pits; same-map falls should recover.
4. F8 > Speech > Mod warnings: turn Off. Directions should still work and the route
   warning should be quiet. Turn On to restore it.
5. During loading, map changes and battle transitions, press/hold navigation keys or
   LT. No mod action should fire while loading or replay a held press on return;
   release and press again once control returns.

Failure symptoms: permanent loading state, missing safe stairs, repeated fall,
incorrect landing directions, warning for the selected event, or warning-Off hiding
directions. Record the floor/target and retain that game's own log. Unnamed/conditional
mechanics and live all-area coverage remain gameplay checks; do not infer universal
coverage from the extracted named-trap inventory. World-map refresh latency still
has a separate diagnosed fix plan below.

Latest October 14: the new transition input guard is built/offline-verified and
deployed/hash-verified in all six after FF2 closed. Intermittent world-map refresh
lag is diagnosed and its broader fix is still planned. Do not treat this build as that
complete lag fix. [Scoped status and pending checks](#world-map-refresh-and-transition-input-october-14).

Offline checks pass; user gameplay testing has identified regressions recorded in the
[October 11 follow-up](FOLLOWUP_2026_10_11.md). Test one game at a time. Report what you hear,
and attach `ffpr_screen_reader.log` from that game's folder if something is wrong.

Prioritize the [South Figaro stairs and grouped world exits](re/MAP_EXITS.md#prioritized-gameplay-checks)
when progression permits. These October 13 fixes are installed in all six games;
offline checks passed and gameplay confirmation is pending. Then continue the
[hook/title/boarded-save checks](#hook-installation-title-and-boarded-save-retest-october-13)
and earlier FF2 results checks.

## Gameplay confirmations, October 13

Initial user report after deployment (`a2f3b81`, DLL
`AF4C94F08AF9D8F30C8E534279B2D225BC902746269DC721485709DFC7498DB7`):
the table also incorporates subsequent FF2 ship/results and Verbose/Brief retests.
The latest follow-up DLL/deployment is recorded in [MAP_EXITS.md](re/MAP_EXITS.md#installed-build-and-recovery).

| Feature | Gameplay status |
| --- | --- |
| Land pathfinding responsiveness | Confirmed by the user: working perfectly with no lag in the tested session. Later FF2 ship report is tracked separately below. |
| FF2 Castle Deist to world-map loading | User confirms the transition lag seems fixed after 0e717ab and accepts it for V0.9 release preparation. This is qualitative gameplay confirmation, not instrumented timings for all maps/games. |
| Battle transitions | Confirmed by the user: smooth in the tested session. |
| FF1 battle-results buffer reading | Confirmed by the user: reads correctly. Specific Autodetail/input/detail cases were not separately enumerated. |
| FF2 battle-results buffer | User's later example includes character, hand, weapon type, skill level and progress. Screenshot confirms hand headings, icon type, level numeral and bar are displayed. Phase-wide I and other phase/control cases remain unconfirmed. |
| FF2 boarded ship routing | Subsequent user retest confirms ship pathfinding and boarding/disembarking across vehicle/foot route legs work. Other vehicles remain untested; no numerical timing or every loaded-save/story case is implied. |
| Verbose/Brief stat deltas | User confirms both work during the FF2 testing session. Setting relocation to Speech is subsequently implemented/offline verified; other games are not directly gameplay confirmed. |
| FF3–FF6 battle-results buffers | Expected by the user to work through the shared implementation; direct gameplay confirmation pending. |

These confirmations supersede earlier blanket pending statements for the reported
features. They are qualitative gameplay evidence; numeric input-to-speech/frame/load
measurements and unreported speech-order/control cases remain distinct. FF2 results
are the next active check; its first-page/weapon-row procedure is below. The original
confirmation was recorded without changing code; subsequent FF2 reports required the
fixes below. No agent game launch was performed.

The later FF2 screenshot/source review is recorded in
[results evidence](re/RESULTS.md#ff2-screenshot-review-october-13). The displayed numeral
is skill level, not the internal weapon-type identifier. The spoken percentage is a
numerical description of the visible bar under the existing approved progress contract.

## October 11 audit test

Follow-up checks: confirmation bodies speak on entry before their Yes/No choices;
check save, load, quicksave, return to title and quit. Check the title prompt and repeat
world/town/floor transitions for delay. Ordinary FF2 world exits must not announce an
event, and field audio/navigation should recover after loading.

Title prompt follow-up: in each game, let the visible button prompt appear before
pressing a button; hear its actual wording. FF5/FF6 add the intro-wait instruction
after that wording. Check initial boot, letting the cinematic play, and returning
to title. Opening the title menu should cancel any pending prompt speech.
User has confirmed fast startup and map transitions with the preceding loading fix;
the later entering-map announcement was accepted.

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
   announces on entry. I reads current details; arrows review current lines. Speech's
   Auto Detail reads the current phase automatically. FF2 includes displayed evasion/
   accuracy multipliers and skill gains; unused hidden statistics stay absent.
   With Auto Detail off, hear EXP/gil totals and phase headings, without individual
   EXP/drop rows. FF2 has gil and weapon/spell gains rather than character EXP.
   Battle's `Battle results: changed stats` toggle (default on) adds a concise changed-stat
   rundown; toggle it off to hear only growth headings. The full table remains in arrows.
   From any position in a growth or FF2 weapon buffer, I/controller West reads every
   change in the current phase; a phase without changes says No stat changes. FF2 weapon
   rows contain each displayed character/hand, actual weapon type, level and gauge
   percentage. Autodetail reads that table but weapon delta summaries still require the
   changed-stats toggle. Check percentage-only evasion gains and weapon level rollover.
   No EXP total or member EXP row should appear in FF2. With Autodetail on, hear the current
   phase's details. Hold/release
   each arrow: it must review without skipping a native page or its animation.
   Confirm still advances. Check regular status entry with Auto Detail off too:
   only its character/level headline should be automatic; I/arrows read the table.
   Hit and damage speech now decodes visible digit sprites. Attack sound count does not
   define the hit count. Report the spoken count and action if a rendered mismatch remains.
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
3. Install the pinned Prism prerequisite separately once: `install_prism.bat ff1`
   (or ff2 … ff6, or no argument for all six). Unchanged verified installs perform zero writes.
4. From `FFPRCore-Screen-Reader`: `deploy.bat ff1` (or ff2 … ff6, or no argument for all six).
   It checks Prism read-only, then copies only `hid.dll` and `SDL3.dll` into the game folder.
   Prism, licenses and its receipt are not copied or repaired by normal deployment.
5. To undo: `uninstall.bat`, then restore the old DLL/dependencies before using an old
   Tolk-based reader. This user's requested migration retires game-root Tolk.dll; recovery
   copies are kept in the development workspace.

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
    three under Navigation audio. Current floor: only targets walking reaches on this
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

## Autowalk and combined navigation, 2026-10-11

Implemented in the shared core for FF1–FF6. Offline replay passed 2,292 sampled baked
map routes; all-six movement ABI and native-name checks passed. Gameplay confirmation
remains user-side. Evidence and action boundaries: [autowalk](re/AUTOWALK.md).

Final facing/transition build, hook coexistence, route/map-state/scheduler checks, 7,016 native-name checks,
zero undefined wiring names and isolated DLL smoke passed. Deployed to all six games;
each installed DLL matches SHA-256
`7EA63358FBF982023AF9B17E198223A0A408AD056F223614436A6A1BA855FDBF`.
No games launched. Native runtime hooks and movement/handback still need user confirmation.
Additional regressions passed: native facing in all six binaries, all four arrival headings,
zero movement/counters, transition readiness/cancellation and six real floor-route handoffs.

1. On a field map, open F8 → Navigation audio. Enable Autowalk. Select a nearby destination
   and press Backslash: hear "Autowalk started", walk there, then hear "Arrived". Confirm
   immediately without a direction key: the character should already face the interactible.
   Repeat from an adjacent cell while facing away, and with an NPC across a shop counter.
2. Start another walk. Press Backslash again, then separately try a movement key,
   opening a menu and entering a battle. Each must stop the walk and hand control back.
3. Enable Speak autowalk path. Start a longer walk: hear the complete current-map route.
   Disable it: hear the start/arrival messages, without route speech.
4. Try stairs and a destination on another floor. The walk should continue after the
   expected transition and fade, without another navigation request, following live gates.
   Try a route through two or more floors, including one that leaves a floor and returns
   to another part of it. The character should face the final interactible on arrival.
   Cancel during loading and verify it stays stopped after the new floor becomes ready.
   At a boarding/landing/examine action it
   should stop and give the instruction; perform the action and request navigation again.
5. Disable Autowalk and Beacon Navigation. Enable Beacon with directions; Beacon Navigation
   should also become enabled. Backslash
   should both ping and read directions. Disable Beacon with directions: beacon only.
6. Controller: select a destination with the right stick and press LT. Repeat LT to stop;
   move the left stick to take control. Try a waypoint with the D-pad/LT or Comma/Period/Slash.
7. Close/reopen the game and verify the three settings persist. Repeat basic walk,
   cancellation and combined-mode checks in each of the six games and on a world map.

Special flight controllers, boarding and interaction confirmations remain manual.

## Settings focus and NPC labels, October 12

1. On the field, press F8. Confirm Speech, Navigation audio, Battle, then Close Menu.
   Audio volumes follow their related options; EXP sound/volume are in Battle except FF2.
2. Open a setting, Alt+Tab to another application, and use arrows, Enter, Space, I, Escape
   and F8 there. Return to the game: selection and value must be unchanged. Held keys must
   do nothing until released and pressed again. Test controller menu entry/return as usual.
3. Cycle town NPCs. Unique names have no developer number; exact duplicates use sequential
   suffixes, such as Soldier1/Soldier2. Move and cycle again: labels retain their assignment.
   Check other-floor entries too; each floor has its own numbering group.
4. In a Japanese-language session, confirm circled/full-width prefixes and suffixes follow
   the same rule. Test waypoint naming and confirmation after Alt+Tab as well.

Offline evidence: [October 12 follow-up](FOLLOWUP_2026_10_12.md). Gameplay confirmation pending.

## FF2 tutorial-room labels, October 12

Enter the Rebel Hideout room with the tutorial NPCs: its name should include Tutorial
Chamber. Cycle its Events entries: the circle should be Magic Circle. NPC lessons and
the circle's ordinary interaction should behave as before. Both early and later room
versions use the same labels. [Source evidence](MAP_LABELS.md#tutorial-room-context-correction-2026-10-12).

## Results and Prism deployment, October 12

The preceding results/Prism deployment used SHA-256
`7339C5113508C6B5830401F771A867F47BB70FECA4D38729DE2AE6C6A3C978F6`.
Canonical build, shared reader/input regressions, native PE phase/input assertions,
7,047 native-name checks (zero missing), wiring checks and isolated DLL smoke passed.
Prism 0.18.3 x64 was installed separately in all six games; game-root Tolk.dll was removed
after successful deployment. Fake API, package/SDK, dependency idempotence/rollback,
actual mod-only deployment and scratch archive checks passed. [Migration record](PRISM_MIGRATION.md).
Gameplay confirmation is pending. Use item 6 above to check Autodetail both ways,
arrow hold/release and normal confirm; also check ordinary status entry and title/menu speech.

## Worker/baked-only and FF2 first-page follow-up, October 12

Offline implementation/evidence: [thread ownership](re/THREADING.md) and
[native FF2 result phase](re/RESULTS.md#ff2-first-page-retest-and-loading-follow-up-october-12).
The canonical build and isolated DLL host pass. Runtime confirmation remains user-side.
The follow-up is deployed/hash-verified in all six games:
`DA45D46CA76664B2E722D20AA898B7AD10389642F97E009D4D41D2B7B818C95F`.

1. Win an FF2 battle without spell gains. The first page must announce gil; arrows
   review every displayed equipped weapon/level/progress row. With Autodetail off,
   it must not automatically read the full table. I on a weapon/stat row reads its
   all changes in that current phase; Battle's changed-stats toggle alone enables automatic change summaries.
2. Hold/release arrows on every results page. They must never advance the native
   screen, including while the buffer is preparing. Confirm advances normally.
   Only actual stat-change pages get buffers; each character's page is separate.
3. Request a long baked route or one through another floor. Cancel pending autowalk
   with ordinary manual input; no movement should start afterward. Verify the same
   destination remains selected after a planned transition. Missing map data is a
   reported bake bug, with no native route fallback.
4. Check title/save/shop/config/item/equipment/ability/bestiary screens and return
   navigation, then controller focus/hotplug/Alt+Tab. Native lifecycle caches replaced
   scene searches, so any screen-specific missing read should identify that screen.
5. Compare the same save with the mod and without it: game loading, a map transition
   and battle entry. Report a repeatable slowdown and where it occurs. Offline stall
   tests do not measure actual game loading or frame responsiveness.

## Navigation latency follow-up, October 13

Offline evidence and implementation details: [thread ownership and latency record](re/THREADING.md#navigation-latency-follow-up-october-13).
Canonical build and isolated DLL smoke pass. Deployed/hash-verified in all six games:
`ED888BC45404857EA010B80991E38DAD8107F9047FC1D8F34075BC6E9421C7C2`.
User now confirms lag-free pathfinding and smooth battle transitions in the tested
session; [current coverage](#gameplay-confirmations-october-13). Numeric timings and
the specific movement/cancellation/loading cases below remain separately unconfirmed.

1. On a world map, rapidly browse targets and cycle categories. Browsing should remain
   responsive while a live refresh runs.
2. Move, change category, then request directions. Confirm the selected target is still
   current, nearest-first category selection uses your new position, and directions match.
3. Request a long or cross-floor route, cancel with manual input, then try autowalk
   continuation. Confirm cancellation prevents later movement and manual input wins.
4. Repeat during a map transition/loading. Check that the game stays responsive and
   navigation becomes available when the map is ready.

## Speech and navigation confirmation, October 13

Current accumulated procedure for the latency and speech-order follow-ups. Implementation
and actual-source offline checks are complete; [evidence and runtime gaps](re/SPEECH_ORDER.md).
Canonical build and isolated DLL smoke pass. Deployed/hash-verified in all six games:
`AF4C94F08AF9D8F30C8E534279B2D225BC902746269DC721485709DFC7498DB7`.
No game was launched or probed by the agent. Pathfinding responsiveness and FF1 results
reading are now user-confirmed; FF2 results testing is in progress. After FF2, prioritize
remaining category/F8 speech-order checks when progression permits. The later naming
check can wait until you naturally start a new game.

1. **Categories and browsing, on an available field/world map.** Change category once:
   hear its complete name, then its first destination. Browse repeatedly, then rapidly
   change categories. The final reply must describe the final category, with no older
   destination interrupting it. Ordinary browsing should respond promptly; report any
   pause measured in seconds and whether it occurs only after entry/loading or also warm.
   Offline: actual callbacks reordered, enum wrap/stale route/pressure tests, large scanner
   and all-six world route checks pass. Real audible response and native frame cost remain
   unmeasured. Keep the usual movement/nearest-target/directions/cancel checks above.
2. **F8 settings, in a safe existing screen.** Open F8, enter Speech or Navigation audio,
   then return. Hear each heading completely, followed by its first option. Move to
   another option immediately after entry: the new selection should win; no delayed
   old first option should return. Offline actual menu/input and zero-timer checks pass.
3. **Waypoint prompts, when waypoints are available.** Create or rename a waypoint:
   hear the prompt then initial text. Confirm or cancel; hear that answer before any
   completion message. For a chained delete confirmation, hear Yes/No before the next
   question. A later key still interrupts normally. Offline actual prompts/callbacks and
   fake-SDK order pass; stale prompts or cut-off answers are failure symptoms.
4. **Existing ordinary screens and battle, as reached.** Check dialogue, menus, battle
   feedback and results. Manual movement/details must remain responsive; Autodetail
   must queue behind the current heading without reading later rewards. Follow the
   outstanding FF2 first-page/weapon-row procedure above. Source priorities and existing
   results/input regressions pass; any new interruption should identify the screen/action.
5. **FF1–FF3 naming, on a future new-game entry.** Enter name selection and hear Select
   name before the initial offered name. Later cursor moves remain normal. Offline
   helper order/signatures pass; actual native callback order remains unconfirmed.

If a delay or interruption remains, report the game, screen/category, triggering input
and whether it was the first request or repeated browsing. The normal log now records
request-to-Prism submission, queue/data age and SDK duration for navigation and F8;
SDK success is not proof that speech finished audibly. Compare loading/map/battle
transitions using the same save with and without the mod if a game slowdown is observed.

## Current FF2 results and vehicle retest, October 13

Canonical build and isolated DLL smoke pass; installed hashes match in all six games:
`FA433F55BD5743D3FFAA57450528E796C39FB875495BF329C5015E0F95A7D99E`.
[Weapon/I evidence](re/RESULTS.md#weapon-labels-and-phase-wide-changes-october-13),
[vehicle/world-wrap evidence](re/THREADING.md#boarded-vehicles-and-wrapped-worlds-october-13).
Earlier working land routing, smooth battle transitions and FF1 results reading retain
the user's confirmation. These corrections need gameplay retest:

1. **FF2 ship first, when available.** While onboard, select a world-map land
   destination and request directions. Hear sailing, land and walking instructions
   instead of No path. Try Altair and Deist if available; both logged failures pass
   offline. With the pathfinding filter enabled, reachable land destinations remain
   listed. Repeat from land to check ordinary routing.
2. **Next FF2 battle.** Each hand row must include the actual weapon type, level and
   progress, for example R. Hand Sword 5, 66%. Change summaries name the same type.
3. **Current results phase, any game.** Press I from the heading, a changed row and
   an unchanged row. Each reads all changes for this phase/character, matching the
   automatic summary. Repeat with that toggle off: I still works. A phase without
   changes says No stat changes. Arrows read individual rows; confirm advances normally.
4. **Other boarded vehicles, as progression permits.** Check destinations and legal
   landing/walking instructions. World-edge directions must wrap correctly; autowalk
   stops for manual landing. Special flight controls remain manual. The 103 offline
   vehicle cases are representative world/transport combinations, not every story state.

Report the game, vehicle, destination and whether failure occurs before or after landing
if a route still fails. Unreported category/F8/prompt/naming speech-order checks above
remain separate from confirmed features. No agent game launch or runtime probe occurred.

## Hook installation, title and boarded-save retest, October 13

Subsequent user confirmation: FF2 ship routing and boarding/disembarking work. Retain
the steps below for unreported loaded-save/V cases; other vehicles and title-prompt
timing remain unconfirmed. No further ship workaround or broad retest is requested.

Built, isolated-smoke tested and deployed to all six; installed DLL hashes independently
match `473F868AB8BCDBE5CC91F4FB41F654C9674ECDB88521F087E50B6DBD0BD31FD4`.
Prism prerequisites were verified without changes. No agent game launch or probe.
The user's latest FF2 session had 199 failed hooks, including the field/player/title
observers. This explains its no-op navigation/Walking report; the earlier valid ship
No path reports remain the separate riding/landing/wrap correction above.
[Allocation/load evidence](re/THREADING.md#hook-allocation-and-boarded-save-follow-up-october-13),
[title producer evidence](re/TITLE_PROMPT.md#current-timing-and-hook-installation-follow-up-october-13).

1. **FF2, load the same save already aboard the ship first.** Once loading finishes,
   press V: hear Ship/the actual vehicle name. Shift+`]` changes category; `]` browses
   destinations. These must answer promptly. Select a known land destination and press
   `\`: hear a legal sailing/landing/walking route, or the configured navigation audio.
   No disembark/reboard workaround should be needed. Walking/no-op input means the
   loaded-owner problem remains; No path is a different route-planning symptom.
2. **FF1–FF4 title, when opening each game.** Wait for Press any button to be spoken,
   then press a button. Hear initial menu focus normally. Return to title and repeat.
   If you press immediately, a pending prompt must never speak over the opened menu.
   Report the game/boot or return path if the prompt is silent or delayed. FF5/FF6 keep
   their existing game-owned intro timing and prompt-then-intro instruction.
3. **Continue the FF2 weapon rows and phase-wide I checks above.** Those corrections
   are retained, along with the user's confirmed land responsiveness, smooth battle
   transitions and FF1 results reading. Other vehicle/story states remain checks for
   when progression makes them available, not confirmed coverage.

Sol will check the ordinary game log for successful observer installation if any
symptom remains; the user does not need to inspect hook counts or code. Offline tests
reproduce old slot-31 allocation failure and pass 384 constrained/512 spacious hooks,
actual loaded-state observers, explicit vehicle queries and actual title/speech pipeline
with fake owners/SDK. These are not measurements of real visible-to-audible delivery.

## Equipment, gains, warnings, chest labels and autowalk retest, October 13

The complete follow-up is built, smoke-tested, deployed and hash-verified in all six.
No game was launched/probed by the agent. Prior user confirmations of fast pathfinding,
smooth battle transitions, FF1 result review and FF2 ship/cross-vehicle route directions
remain documented separately. The user now confirms Verbose and Brief modes work.
Slot-label restoration, settings relocation, remote chest state and Wyvern inclusion still
need gameplay confirmation; other outstanding checks below remain applicable.
Test these first when progression permits; vehicle/learning opportunities need not delay
unrelated play. [Offline evidence and screenshot explanation](re/MENU_DETAILS.md).

1. Leave Speech / Spoil treasure chest contents off (default). Browse local and other-
   floor chests: hear Treasure Chest, floor/location and opened/unopened state where
   applicable, with no reward names. Current map must retain opened chests on other floors
   and indicate their saved state where known. Opening a chest may still read the game's reward
   message. The explicit spoil setting on permits contents; turn it off again to restore
   generic labels immediately.
2. With Autodetail off, equipment slots speak the slot label (including displayed Favored
   Hand) and item name; candidates and shop items speak the item name.
   I/right-stick up reads the current information panel. Toggle native Q/X/Square, then
   use I: raw item bonuses and description must follow that page. Slots never read
   character deltas. In a replacement chooser, arrows review comparisons; WASD changes
   the chosen item. Normal pad selection remains native; mod-mode pad/shoulders review.
   Autodetail on adds slot/shop panels and candidate comparisons. If Items -> equipment
   details is available, its extra displayed stats, abilities and highlighted elements
   should be in the buffer too; gray elements must not be announced as properties.
3. During an FF2 character's command turn, H reads that character's displayed vitals;
   1–4 reads each party slot. Results with Autodetail off must still announce actual
   level-up/learned-spell/ability messages and retain them in the buffer. I from any row
   reads every current-phase delta. Switch Speech / Stat delta detail to Brief: gains/
   losses replace before/after values in automatic speech and review. Verbose restores
   the full comparison. Native learning messages remain automatic in both modes.
4. When field status damage occurs, Speech / Status damage on (default) announces it.
   Turn it off: warnings stop while the game's damage/flash continues. On restores all
   valid warnings, with no new cooldown or text suppression.
5. Enable Autowalk. Request a route from a ship to a land destination: it should advance
   through the shore tile and keep walking. From land, a route requiring a parked ship
   should walk aboard and later disembark. Repeat with controller LT. Explicit-mount
   vehicles use native Confirm; FF6 airship should move with either camera heading and
   land using native Back. Manual movement, menus, battle and focus loss must release
   the walker. Do not count prior path-direction confirmation as this automation test.

6. FF2 Castle Deist: Wyvern's Room: NPC category must list Wyvern. Current map should
   also include it from another connected floor when the story makes it available. Follow
   the route and confirm to talk normally. Sunfire and Goddess's Bell keep their object/
   Event classification and native keyword interaction when progression permits.

Failure symptoms to report: reward names with spoil off; missing remote opened state;
missing Wyvern; missing slot/Favored Hand labels; slot character comparisons;
I ignoring the native page; arrows changing the item; missing automatic learning/gains;
wrong/absent party member; repeated before/after in Brief; silent/native-wrong boarding,
landing or motion after cancellation. Other vehicles, story restrictions and target-game
callback/first-audible timings remain unmeasured.

## FF2 Deist-to-world loading retest, October 13

The completed follow-up is deployed/hash-verified in all six, with all offline checks
and isolated smoke passing; no game was launched/probed. Leave Castle Deist for the world map
as in the reported stall. The game should finish its native transition without waiting
for queued mod work. Once ready, category/destination inputs should remain prompt and
ship/walking routes should still work. Report whether the long pause recurs; Sol will
read FrameTiming/LoadTiming in the ordinary mod log to separate mod work, native state
processing and frame gaps. No user code review, timing tools or visual check is required.
Offline callback budgets, held-worker-lock/native continuation and stale-result disposal
checks are separate from this gameplay confirmation.


## October 14 follow-up: test these first when available

All implementation and offline checks are complete. Live game behavior still needs
confirmation; no code review is requested.

1. FF4 battle: open White Magic, cancel to commands, move between commands, reopen
   White Magic. Expect the focused spell and returned command, then normal speech.
   Silence after cancel is the failure symptom. Repeat in FF5 and other games.
2. FF3 battle: Cure and an ally-targeted item must announce the first ally and each
   target move; Spark must say All enemies when targeting opens. FF6 single enemies
   must speak once while idle, and again after changing focus or reopening targeting.
3. Field magic: choose Use. FF3 should announce its first focused spell (for example
   Poisona, Lv. 1, MP 25/25). Target Cure with a lone member in an otherwise empty
   party; expect that member immediately and again on deliberate arrows. FF4's
   three-member target order must match Rydia, Cecil, Tellah in the supplied screenshot.
   Repeat initial-use/target tests in each game as progression permits.
4. FF6: equipment candidates should include displayed quantities (Growth Egg 4),
   Esper focus should add Equipped for a marked entry, empty formation slots should
   say Empty, and the status buffer should include Commands before the list.
5. FF3: job I/Autodetail should read Equippable equipment types. Items -> use a piece
   of equipment should create a buffer for its current information/eligibility page;
   page changes replace the buffer and I repeats it. Repeat in FF4–FF6. U is removed.
6. Load menu: browse ordinary, quicksave, empty and corrupted slots. Expect displayed
   leader/level/location/date/time/playtime where present plus full party names;
   corrupted data must speak its rendered error. This check does not load or alter saves.
7. FF6 Falcon: boarding stops for manual deck/wheel control. When Take the wheel opens,
   its initial choice must speak. After manually taking the wheel, request an autowalk
   route with a turn. Expect flight along it, stopping at arrival; manual input must
   cancel. FF3 airship boarding likewise remains manual. No automatic wheel interaction
   is implemented. Native flight timing still needs this test.
8. FF5/FF6 title: expect native Press any button, followed only by Wait to watch the
   intro cinematic. The EXP results reader is unchanged: Confirm can finish rolling
   numbers without advancing to a new results phase.

Map bake coverage and remaining progression-dependent script/state checks are in
[MAP_EXITS.md](re/MAP_EXITS.md). Static geometry is checked for every extracted room,
including both South Figaro variants; this does not certify every puzzle/story state.


## Subsequent October 14 retest: target rows, flight and Esper pages

Confirmed by the user before this revision: FF6 repeated enemy spell-target announcements
are fixed; FF4 cancelling battle magic restores command speech. Preserve both confirmations.
Prior installed ally targeting is still silent in FF4/FF5/FF6 and FF4 battle Items is silent.
The following new checks are gameplay-pending after the subsequent deployment:

1. In FF4, FF5 and FF6 battle, choose Cure or an ally item. The first highlighted ally
   should speak; up/down should speak each displayed ally's name and rendered vitals.
   FF4 shows HP current/max and current MP, with no level or MP maximum in those rows.
2. FF4 battle Items: the first item and arrow highlights should speak name/count;
   cancel back to commands, then move once. Both focused commands should speak.
   Repeat with other games when available, including FF6 Tools. Recall has no I description.
3. FF6: board manually, take the wheel, choose a town and start autowalk while flying.
   Flight should continue, turn correctly and release input on stop/manual movement.
   Both Current floor and Current map with vehicles should retain land-reachable towns.
   Check Falcon first, then Blackjack when available; do not test automatic deck boarding.
4. Walk/move, then immediately request Backslash directions to the selected target.
   List refresh should not cause the prior half-to-one-second wait. Crow-flies and route
   directions should use the current position. Browse/category changes must still update
   the filtered list and its distance order. Large FF4/FF5 worlds remain useful timing cases.
5. FF6 Espers: highlight an equipped Esper; hear Equipped before pressing Q. On the
   acquired-magic page, arrows browse headings, each displayed progress/rate, level-up
   bonus, description, MP cost and visible character header. Mastered still includes 100%.
   Return to the list: its buffer must contain only the displayed list-panel information.
6. Formation/Order: empty slots say Empty; FF4 follows Rydia, Cecil, Tellah in the supplied
   layout and row changes reflect the displayed result. Other character/target menus never
   add row. F8 has Speech, Navigation, Navigation audio and Battle, followed by Close Menu.

All source/native evidence and offline checks are Sol's responsibility; the user need only
report what speaks, whether flight stops, and whether the perceived directions lag remains.


## Latest October 14 retest: blank Espers, equipment pages, group targets

Build/offline checks pass and this revision is installed in all six games. These gameplay
checks remain unconfirmed; do them first when the relevant screen is available.

1. FF6 Espers: highlight a blank slot. Expect Empty and an Empty-only arrow/I buffer.
   Move back to the prior Esper; expect its name again. Toggle Q (controller Square/X)
   twice without changing Esper; each buffer must match its current panel, with no Terra
   or other active-character line appended. Failure: old spell/description/cost survives,
   a page requires scrolling before updating, or return to the name is silent.
2. Items -> use equipment: expect name and displayed quantity once on entry. Q toggles
   description versus stat-summary footer; both supplied screenshots remain page 1/2.
   Use the numbered previous/next-page controls (2/3 in the screenshots) to change the
   actual page; the buffer must then show the new page number and only its visible rows.
   Failure: duplicate name, old description/eligibility, or permanently stale page number.
   The shared source is checked in FF1-FF6; FF6's competing reader is removed.
3. FF4 battle: enter Cecil's Darkness target screen. Expect All enemies immediately once.
   Cancel and reopen; expect it again. Check group-only spells/items in other games when
   available: All allies, All enemies or All allies and enemies as selected. FF6 side
   attacks qualify a one-side group. Failure: silence until movement or repeated idle speech.

Offline evidence: docs/FOLLOWUP_2026_10_14.md, final blank/page/group-target section.
No runtime measurements or automatic gameplay tests were performed.


### Equipment pager and mastery clarification: confirmed

User confirms FF4 All enemies entry and FF6 equipment content/icon equivalents. Numbered
page updates are also confirmed: the apparent stale count was using Q instead of the
native numbered page controls. In the supplied keyboard layout, 3 changes to the next
numbered page and 2 returns; Q toggles the footer description/stat summary without
advancing that pager. No additional test is requested for this resolved report. Unneeded
completion hooks were removed; rebuilt/smoke-tested confirmed behavior is restored in
all six games. Other earlier unconfirmed features retain their own checklist status.

Esper mastery explanation is verified: the star means all that Esper's taught spells are
learned for the selected character, independent of having equipped that specific Esper.
The supplied Terra/Valigarmanda screenshot displays all three spells at 100% with stars;
the mod's reading matches it. No reader change is required for that screenshot.


## World-map refresh and transition input, October 14

Transition guard: implemented/offline-verified and **deployed/hash-verified in all six**
after FF2 closed; gameplay confirmation pending. Broader world-map lag fix: diagnosed
and planned, not implemented.
[Evidenced causes, guard ownership and fix plan](re/THREADING.md#intermittent-world-refresh-and-transition-input-october-14).
All-six hit speech is renderer-based; asset/high-count regressions pass. No additional
hit-count gameplay test is required merely to confirm absence of mod hit mathematics.

Prioritize these short guard checks when progression permits:

1. During a map change or battle entry/exit, tap P/backslash, brackets/J/L, Shift+brackets
   and backtick; include Shift+M. Expected: no pathfinder speech/settings effect or added
   input-caused stall. Once control returns, a fresh press works normally. Failure: delayed
   old key speech, a changed filter or immediate loading lag caused by the key.
2. Controller: hold LT across one transition, then release it and press again. Expected:
   the held trigger starts nothing after loading; the fresh trigger navigates. Pending
   stick-click gestures must also be discarded. Native game controls remain usable.
3. If a mod menu can remain open through a native transition, held arrows/confirm must
   neither change a setting during loading nor replay on return. Fresh input works afterward.

For the future complete refresh fix, retain the user's current world-map scenario as
its first performance confirmation: select a destination near Deist, walk several steps,
request directions, finish a battle and immediately request again. Include repeated
requests during refresh, a parked ship and one dungeon comparison. Expected: current
usable directions and prompt post-battle readiness, without waiting for full list rebuilds;
old requests cancel safely. Continued ~one-second waits or several-second silence are
failures. Existing own-session log plus new stage correlation will separate blocked native
readiness from registry/filter work, queue wait and speech submission. Exact audible and
comparable modded/unmodded timing remain user-side; the present guard is not certification
of the future full fix. No broad capture, game automation or code review is requested.
# Soundscape and masters — October 15

This build is installed and hash-verified in all six games. Test these first when the relevant locations are available.
All six games share these controls. Agent game launch/control was not used.

1. F8, Speech: turn Game speech off. The mod menu, I descriptions and close announcement
   should still speak. Game announcements should stop. Reopen F8 and restore Game speech.
   Repeat menu navigation with the controller if available.
2. Navigation audio: turn its first master off. Every mod sound, including previews,
   should stop; native game sounds remain. Turn it on: individual saved settings remain.
3. Enable Soundscape, then open Soundscape settings. Preview each sound and adjust its
   volume/master volume. Set a category to zero or off: only that category is silent.
   Escape returns to the Soundscape settings entry in the parent menu.
4. Near several NPCs/chests, listen while moving around them. Each eligible nearby entity
   should have a repeating positioned cue; opened chests stop. Shift+semicolon reduces
   range and Shift+apostrophe increases it, by five from 5 to 25. Default is 10. With
   Soundscape off, both shifted keys do nothing; wall/beacon choices still work.
5. At a doorway/map exit, expect a knock; a door without an adjacent exit is silent.
   An exit without a door uses a rising triple tone. ToLayer stays silent. When available,
   compare a real pitfall/collapsing hazard (lower double beep, quicker within two steps)
   with a progress warp or FF1 trial pad (ordinary event cue). Report the map and entity
   label for a wrong classification; no broad database re-audit is required beforehand.
6. With headphones, enable HRTF and repeat the spatial check: map north is ahead,
   south behind, east right and west left, independent of facing. Category/directional
   pitch shifts stop; the four wall tones retain their pitches. Turn HRTF off to compare.
7. Optional replacements: follow README filenames/PCM format, replace one cue and restart.
   That cue changes; absent or invalid replacements retain defaults. Check long clips do
   not overlap themselves. Menu/battle/loading/focus changes should not leave field audio
   playing or replay an old ping. Crowded scenes should remain playable.

Offline evidence and precise performance limits: [SOUNDSCAPE.md](re/SOUNDSCAPE.md).


## Soundscape follow-up — October 15

User has confirmed the entity soundscape and cadence. After this correction, check these
first when convenient; no code review or agent runtime capture is needed.

1. In FF2, turn Footsteps On with apostrophe; walk several tiles and bump a wall.
   Expect the familiar click and thud at the existing 50% default, with HRTF Off and On.
   Footsteps was last saved Off after the user's test; deployment preserves that choice.
2. FF2 is set to the user's Brackets/backslash layout at deployment. First browse with
   brackets and request directions with backslash while Soundscape remains On. Expect
   spoken selection/directions; if still silent, report that separately. Ctrl+Apostrophe
   stops/restarts only the entity soundscape. Enable F6 and request a
   destination using P (Letters) or backslash (Brackets), or controller LT. Beacon-only,
   spoken directions and Beacon with directions should remain usable with soundscape On.
   Unexpected silence or a changed navigation preference is a failure.
3. When riding a ship/canoe in FF1–3 or a ship/submarine in FF5, enable F8 > Navigation
   audio > Landing Pings. Adjacent legal shores ping in their directions; open water and
   walls do not. Check a save loaded aboard a craft as well as boarding normally.
   FF2 snowcraft, aircraft, FF4 and FF6 stay silent. Settings remain default Off.

The restored stereo samples are verified offline; physical audibility/levels and these
watercraft cases still need user confirmation. See [evidence](re/SOUNDSCAPE.md#legacy-effect-and-watercraft-follow-up--october-15).


## HRTF contextual volume follow-up — October 15

The user confirms the previous fixes work well. After the new build, test these first:

1. In F8 > Navigation audio with HRTF Off, set Wall Bump Volume to 30. Turn HRTF On:
   the same slider uses its independent HRTF value (initially 50). Set it to 70.
   Switching Off should restore 30; switching On should restore 70. Restart once to
   confirm both saved values remain. Existing stereo levels are retained by deployment.
2. Bump a wall and preview footsteps/wall tones with HRTF On. All HRTF sounds now have
   one 2x output boost. Volume zero should still mute. Turning HRTF Off retains the
   original stereo output level. Adjust normal sliders to taste.
3. Soundscape master and individual cue volumes should remember both modes in the
   same way. The soundscape switch/range, beacon choices and other settings stay shared.

Offline checks compare actual HRTF output for all 18 cues and retain original stereo
sample parity. Physical speaker/headphone audibility and saved-value restart behavior
remain for user confirmation; no agent game launch/control was used.


## Route/object beacon modes — October 15

After deployment, test these first when a suitable destination is available:

1. Select an entity with a wall or turn between you and it. Turn beacons On with F6.
   Follow route (default) should point toward each route leg. Shift+F6 announces Object
   location and should point directly at the entity instead. Press again to restore
   Follow route. F6 continues to turn either mode off/on. Use P or backslash for an
   explicit restart according to your selected keymap.
2. F8 > Navigation audio > Beacon mode shows the same choice. Switching while beacons
   are off should save the choice without enabling them. Both HRTF modes, their volumes,
   soundscape and Beacon with directions remain available. Manual keys still stop an
   active autowalk; the Autowalk setting remains unchanged.
3. A waypoint uses the same two behaviors. An object on another floor has no direct
   bearing: requesting it in Object mode explains the current-floor requirement. Switch
   to Follow route for cross-floor guidance. No repeating warning should occur.
4. Object mode should stop within the existing arrival radius and resume when you move
   away. On a world-map edge it should point across the nearer wrapped edge. Menus,
   loading, dialogue, lost focus and the navigation-audio master retain suppression.

Offline evidence uses the actual beacon loop, action registry, menu and input dispatch;
user gameplay/physical audio confirmation remains separate. No tester log or tester save
was accessed. See [implementation/evidence](re/SOUNDSCAPE.md#beacon-targeting-modes--october-15).

## Event and navigation follow-up — October16

After deployment, prioritize these when progression permits. Offline checks pass;
these confirm actual gameplay and speech timing.

1. In Deist, browse the Dragoon before and after triggering it. The one-off event
   should disappear after use, including when browsing from another floor. A reusable
   door, warp or NPC interaction should remain available after use.
2. Follow a real cross-floor destination. Its identity/floor must stay fixed through
   each leg, including a legitimate B2–B1–different B2 region route. A stale repeated
   crossing should stop with an explanation. Metadata-only Warp Tile ghosts should
   no longer appear. Already-opened remote chests should retain Opened and the floor.
3. On a large world map, select a distant destination requiring a ship. Move and
   request directions several times while lists refresh. Expect the newest request
   to respond without waiting for old refresh work. Near shore, guidance must say
   which way to board and land. Battle/map loading should retire pending navigation.
4. In FFII, highlight each ally while targeting Cure out of battle. Expect the actual
   selected ally; casting should retain its existing correct vitals announcement.
   In available item/weapon/armor/magic/relic shops, highlight a purchase and expect
   its displayed price with either AutoDetail setting or native detail panel.
5. F8 > Speech > Battle results: notifications once defaults Off. Turn it On and
   finish a battle with a growth notification. It should speak once across that
   battle's result pages, while new notifications/totals/rows remain available.
   A subsequent battle may announce the same notification again. Off restores repeats.

Report the map, destination and symptom if a check fails. No code review, broad
capture, tester files or agent game launch is needed. [Evidence and limits](re/EVENT_CONTEXT.md).

## Intermittent lag diagnostics — October16

Automatic timing diagnostics are installed in all six games and run from startup.
No toggle, hotkey or special test procedure is needed. If lag returns, report the
game and action; the affected ffpr_screen_reader.log can be checked directly.
Restarting retains the prior run in .1.log; five previous logs are kept.

The user confirmed a clean FFII run during the explicitly authorized short profile.
That did not establish the intermittent cause. The new diagnostic build passed
offline overhead/backpressure checks and isolated smoke; its actual-game evidence
will come from ordinary play. [Scope, provenance and limitations](re/STALL_2026_10_16.md).

## Battle entry and formations — October16

Check battle-entry timing first during normal play. No special key enables tracing.

1. In FFII, request directions, then enter a normal encounter. Native entrance audio
   should start promptly; navigation must not run or replay while battle loads.
   If the pause returns, report the action and game so the automatic log can be read.
2. In each of I–VI as progression permits, enter a battle with two or more enemy
   groups. Hear `Enemy formation`, each current enemy name/count, then the first
   turn/command without cutting off the roster. The roster must not repeat on
   cursor moves, damage, enemy deaths or later UI redraws.
3. When a preemptive/ambush battle naturally occurs, hear its native opening message
   before the formation, then the first turn/command. Let speech finish without Control;
   separately confirm Control still stops the opening speech. Start another battle
   with a different group; no previous roster should
   carry over. Later scripted reinforcements must not repeat the opening roster.
4. With Game speech off, battle/formation speech should stop and the mod menu should
   still speak. Autodetail off should retain the roster when Game speech is on.

Offline native/body/asset/ownership checks are separate from these audible/gameplay
checks. The full one-second stall cause remains unproven. [Current evidence](re/BATTLE_ENTRY.md).

## Autowalk modes and remapping — October 16

Check ordinary autowalk and a remapped route request first. These are remaining gameplay
checks, separate from the completed offline controller, steering, input and map replays.

1. In Navigation, turn Autowalk on. Select a nearby reachable object and use the selected
   layout's route key. Walk, stop with the same key, restart, then stop with manual movement.
   Enter a battle or game menu: movement must stop immediately and must not restart later.
2. In Navigation → Controls → Keyboard bindings, select the current layout's navigation
   request and capture Control+F6. Close the menu. Control+F6 should start/stop the walk;
   the old route key should do nothing. F6 alone should still toggle the beacon. Reset
   that command afterwards. Both Letters and Brackets layouts must remain selectable.
3. With a controller, remap `Navigate to selected destination` in the Field context from
   LT to RT. RT should start/stop the walk, including while held briefly. The mod must not
   also send its game action. Restore that binding, then try a two-input chord. A held
   input on reconnect or focus return must not fire until released and pressed again.
4. In Controller bindings → Global, change Toggle controller mod mode to a free button.
   Use it to enter and cancel Mod Mode. The old button must no longer toggle the mode.
   Remap Read Gil in Mod mode: field to that old button; enter with the new mode button,
   then use the old one to hear Gil. Reset the function before resetting the mode button.
5. Try assigning an occupied shortcut: hear the conflicting command and keep the old
   binding. Check Unbind, individual Reset and Reset all. Reopen/restart to confirm saved
   values. With Game speech off, the editor, capture and reset confirmation must still speak.
6. As normal progression permits in I–VI, test a route while already aboard available
   watercraft, mounts and airships. Turning mounts should align before moving forward;
   corners must not drift to another tile. III/VI airship boarding remains manual.
   Normal free movement should not produce the old generic unsupported-mode message.
   A native up/down choice asks you to select an option first. Report any remaining failure
   with the game/mode/action and that run's log; no diagnostic key is required.

Offline findings and limits: [AUTOWALK](re/AUTOWALK.md) and [binding contracts](INPUT_BINDINGS.md).


V0.9.3 follow-up: user reports autowalk now appears to work and is still testing
remapping; this does not confirm every vehicle/controller profile. During remap
checks, hold each assigned stick direction briefly: it must not fire the opposite
command on the following frame. The intermittent lag has reproduced in the own FFII
log; targeted wall/owner-CPU diagnostics now run automatically. Its full cause is
still open. See STALL_2026_10_16.md for the exact observed versus unobserved intervals.
