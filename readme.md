# FFPR Screen Reader

A screen reader mod for FINAL FANTASY I, II, III, IV and V Pixel Remaster on Steam.

## What it is

- It speaks the games through your screen reader: dialogue, menus, battles, the field and the title screen.
- It adds navigation aids: wall tones, footsteps, an audio beacon, a list of destinations with step by step directions, and waypoints you can name.
- One set of files works in all five games. The mod finds out by itself which game it is running in.
- It is one native file, hid.dll. MelonLoader is not needed.
- It replaces the five older screen reader mods, FFI_ScreenReader.dll to FFV_ScreenReader.dll. Your old settings and waypoints carry over.
- The keyboard and a game controller both work, for the game and for the mod.

Why hid.dll: every game loads a Windows file called hid.dll when it starts, to talk to controllers. The mod's hid.dll passes all of that on to the hid.dll that comes with Windows, so controllers keep working. Then it starts the screen reader.

## Requirements

- Windows, and one or more of the five games from Steam.
- A screen reader that is running, such as NVDA or JAWS. Speech goes through Tolk.
- The files in the zip:
  - hid.dll: the mod.
  - MinHook.x64.dll: lets the mod hook into the game.
  - Tolk.dll and nvdaControllerClient64.dll: speech.
  - SDL3.dll: the mod's sounds and controller support.
  - SAAPI64.dll, if the zip has it: lets Tolk talk to the System Access screen reader.

### Getting the games

- Create an account at store.steampowered.com. Then install the Steam desktop app and log into it.
- Buying is easiest on the Steam website while you are logged in. After you buy, Steam asks if you want to install the game, which opens the desktop app to finish.
- Buy the Pixel Remaster versions. The store page mentions the remaster in the description.
- For FINAL FANTASY V, do not buy FINAL FANTASY V Old Ver.

## Installing

Do this for each game. The same files go into every game.

1. Close the game.
2. Open the game folder. With Steam's default library, the folders are in C:\Program Files (x86)\Steam\steamapps\common, and they are called FINAL FANTASY PR, FINAL FANTASY II PR, FINAL FANTASY III PR, FINAL FANTASY IV PR and FINAL FANTASY V PR.
3. If your Steam library is on another drive, the path is drive letter, then the path to your library, then SteamLibrary\steamapps\common, then the game's folder.
4. If you used the old screen reader, take its file out of the game's Mods folder. Delete it, or move it somewhere else. See the list below.
5. Copy every file from the zip into the game folder, next to the game's exe. If Windows asks, replace the files that are already there.
6. Start the game. You hear speech at the title screen.

The old screen reader files:

- FINAL FANTASY: FFI_ScreenReader.dll
- FINAL FANTASY II: FFII_ScreenReader.dll
- FINAL FANTASY III: FFIII_ScreenReader.dll
- FINAL FANTASY IV: FFIV_ScreenReader.dll
- FINAL FANTASY V: FFV_ScreenReader.dll

If you forget to remove it, both mods speak at once, and the new mod says "Old screen reader still installed" when the game starts.

MelonLoader can stay if you use it for other mods. This mod does not need it.

The game exes are FINAL FANTASY.exe, FINAL FANTASY II.exe, FINAL FANTASY III.exe, FINAL FANTASY IV.exe and FINAL FANTASY V.exe.

The mod only works in these five games. In another game, such as FINAL FANTASY VI, it says "this game is not supported" and stays quiet.

## Uninstalling

- Delete hid.dll and MinHook.x64.dll from the game folder. Deleting hid.dll alone already turns the mod off: the game then uses the hid.dll that comes with Windows.
- If you want, also delete ffpr_screen_reader.ini, which holds your settings, and the ffpr_screen_reader log files.
- Tolk.dll, nvdaControllerClient64.dll and SDL3.dll can stay. The old screen readers use them too.
- To go back to an old screen reader, put its file back in the Mods folder. The old screen readers need MelonLoader.

## Settings and waypoints

- Settings are saved in ffpr_screen_reader.ini in the game folder. Each game has its own.
- Change them in the mod menu: F8, or Start on a controller. You do not need to edit the file.
- The first time the mod starts in a game, it copies your old screen reader's settings from UserData\MelonPreferences.cfg in the game folder. This happens once. The old file is not changed.
- If you delete ffpr_screen_reader.ini, the next start copies the old settings again, or uses the defaults if there are none.
- Waypoints are saved in UserData\waypoints.json in the game folder. It is the same file the old screen reader used, so your waypoints carry over. The mod creates the UserData folder if it is missing.

## Log file

- The log is ffpr_screen_reader.log in the game folder.
- The logs of the five starts before are kept as ffpr_screen_reader.1.log to ffpr_screen_reader.5.log. Number 1 is the most recent.
- If something goes wrong, send the log from that game's folder, with a short description of what you did and what you heard.

## Game keys

These are the game's own controls. The mod reads what they do.

### Keyboard

- W, A, S, D or arrow keys: move.
- Enter: confirm.
- Backspace or Escape: cancel.
- Q: on the naming screen, a random suggested name. In menus that have them, switch between statistics and description.
- F1: switch between walk and run. On the field the mod says "Walk" or "Run".
- F3: turn random encounters on or off. On the field the mod says "Encounters on" or "Encounters off".

### Game controller

This readme names buttons by their place on the controller, with the Xbox name first and the PlayStation name in brackets. A is the bottom face button, B the right one, X the left one and Y the top one. The mod detects Xbox, PlayStation and Nintendo controllers, and speaks the names printed on yours.

- Left stick: move on the field, and move in menus.
- D-pad: move in menus. On the field the mod uses the D-pad for waypoints.
- A (Cross): confirm.
- B (Circle): cancel.
- X (Square): the same as Q on the keyboard.
- Y (Triangle): open or close the field menu.
- LB and RB (L1 and R1): switch tabs in menus.
- LT (L2): page up in menus. On the field the mod uses LT for pathfinding.
- RT (R2): open the pause menu on the field and in battle. Page down in menus.
- L3 and R3 (the stick clicks): walk or run, and random encounters, but only when Stick Click Normalization is on. See "Stick clicks" under the mod's controller keys.

### FINAL FANTASY

- F1 works in towns and dungeons. The world map is walk only. Use vehicles to travel faster.

## Mod keys

### Before you start

- The mod reads keys only while the game window is in front.
- No mod key works while you hold Alt, so Alt+F4 is safe.
- F5, F7 and F8 work only without Shift, Ctrl or Alt.
- Field keys work on the field when no menu is open. Anywhere else they do nothing.
- While you type a name, the mod keys are off.

### Field navigation

These work on the field.

- Backtick, the key above Tab: scan the map again.
- Ctrl+Up, Ctrl+Down, Ctrl+Left or Ctrl+Right: teleport to the tile north, south, west or east of the selected destination.

### Destinations

The mod keeps a list of things on the map: chests, people, exits, events, vehicles, save points and more. They are sorted into categories. These keys work on the field, except Shift+M.

- J or Left bracket: previous destination.
- L or Right bracket: next destination.
- Shift+J, Shift+Left bracket or Minus: previous category.
- Shift+L, Shift+Right bracket or Equals: next category.
- Shift+K: back to the All category.
- K: say the selected destination again.
- Shift+Backslash or Shift+P: pathfinding filter on or off. When it is on, only destinations with a path are listed.
- Ctrl+Backslash or Ctrl+P: layer transition filter on or off. When it is on, stairs and other floor changes are hidden.
- Shift+M: map exit filter on or off. When it is on, exits that lead to the same place are merged into the nearest one. This key works anywhere.

### Waypoints

These work on the field.

- Comma: previous waypoint.
- Period: next waypoint.
- Shift+Comma: previous waypoint category.
- Shift+Period: next waypoint category.
- Slash: directions to the selected waypoint.
- Shift+Slash: add a waypoint where you stand. A window asks for its name.
- Ctrl+Period: rename the selected waypoint.
- Ctrl+Slash: delete the selected waypoint. You confirm first.
- Ctrl+Shift+Slash: delete all waypoints on this map. You confirm first.

In the name window:

- Type the name. Hold Shift for capital letters.
- Backspace deletes the letter before the cursor.
- Left and Right arrows move through the name, letter by letter.
- Up or Down arrow reads the whole name.
- Home and End jump to the start or the end.
- Enter saves. Escape cancels.

In the confirm window:

- Y for yes, N for no.
- Or Left and Right arrows to choose, then Enter.
- Escape cancels.

### Pathfinding

These work on the field.

- Backslash or P: directions to the selected destination. When beacon navigation is on, it restarts the beacon instead.
- Slash: directions to the selected waypoint.

### Audio cues

These work on the field.

- Semicolon: wall tones on or off.
- Apostrophe: footsteps on or off.
- F6: beacon navigation on or off. When it is on, a sound plays from the direction of the selected destination, and turn by turn directions are off.

### Menus and details

- I: details of what is highlighted:
  - in the game's configuration menu, the setting's description;
  - in a shop, the item's stats or description;
  - in the equipment menu, the equipment's stats and description;
  - in the magic menu, the spell's description;
  - in the item menu, the item's description;
  - in battle, the highlighted spell's or item's description.
- Shift+I: read the controls shown on the screen.
- U: who can equip the highlighted weapon or armour. Works in shops and in the item menu.
- F7: Auto Detail on or off. When it is on, descriptions and stats are read as you move, so you rarely need I.

On a character's status screen, a bestiary entry, and the controls list in the configuration menu:

- Up and Down arrows, or W and S: next or previous entry.
- Shift+Up or Shift+Down: jump to the next or previous group. Status screen and bestiary only.
- Ctrl+Up or Ctrl+Down: jump to the first or last entry.

### Battle

- H: the active character's HP and status effects. Outside battle the mod says "Party status only available in battle".
- F5: change how enemy HP is read: numbers, percentage or hidden. Hidden is how the developers meant the game to be played. Works in battle only. You can also set it in the mod menu.
- Tab: if the mod still thinks a battle is going on after it has ended, Tab clears that. During a real battle it changes nothing.

### Global

These work anywhere.

- G: your gil.
- M: the current map.
- V: walking, running, or the vehicle you are on.
- R: repeat the current dialogue page, while a message is open.
- F8: open the mod menu. It opens on the field with no menu open. Anywhere else the mod says why not: "Unavailable in battle", "Unavailable in menu" or "Unavailable here".

### Controller

Names follow the same rule as in the game keys: A bottom, B right, X left, Y top.

#### Mod buttons

- Back (View, Select, Share, Create or Minus): mod mode. See "Mod mode" below.
- Start (Menu, Options or Plus): open the mod menu on the field. Press it again to close. The game never gets Start.
- A, B, X and Y also stop the current speech, then go to the game as usual.

#### On the field

- D-pad Up and Down: previous and next waypoint.
- D-pad Left and Right: previous and next waypoint category.
- Right stick Up and Down: previous and next destination.
- Right stick Left and Right: previous and next destination category.
- LT (L2, ZL): directions to the last destination or waypoint you selected. With beacon navigation on, it restarts the beacon. With nothing selected the mod says "No target selected".
- L3 and R3: see "Stick clicks" below.

#### In menus and battle

- Right stick Up: details, like I.
- Right stick Down: the controls shown on the screen, like Shift+I.
- Right stick Left: who can equip, like U.
- D-pad or left stick Up and Down: next or previous entry on a status screen, a bestiary entry and the controls list.

#### Mod mode

Press Back. The mod says "Mod". Then press one button. Press Back again to leave without doing anything; the mod says "Cancelled".

While a message is open:

- X (Square): repeat the message, like R.

In battle:

- X (Square): the active character's HP and status, like H.
- Right stick Down: read the mod mode controls.

On the field and in menus:

- X (Square): your gil.
- Y (Triangle): the current map.
- A (Cross): walking, running, or the vehicle you are on.
- Right stick Up, Down, Left or Right: teleport to the tile north, south, west or east of the selected destination. Field only.
- L3 or R3, when Stick Click Normalization is on: beacon navigation or the pathfinding filter on or off.

#### Stick clicks (L3 and R3)

Stick Click Normalization in the mod menu decides what the stick clicks do. It is off by default.

- L3 and R3 together, on the field: turn Stick Click Normalization on or off, whichever way it is set. Press both sticks in at once. You hear "Stick click normalization on" or "off", and nothing else happens.
- On the field, a single stick click acts when you let go of it, so that it can be part of the L3 and R3 chord.
- Normalization off: L3 turns beacon navigation on or off, and R3 turns the pathfinding filter on or off.
- Normalization on: L3 and R3 go to the game. L3 switches walk and run, and R3 turns random encounters on or off. The mod's two toggles move to mod mode: press Back, then L3 for beacon navigation or R3 for the pathfinding filter.
- Off the field, a stick click goes straight to the game.

#### In the mod menu

- D-pad or left stick Up and Down: move.
- D-pad or left stick Left and Right: change the value.
- A (Cross): toggle or choose.
- B (Circle) or Start: close.
- Right stick Down: read these controls.

### FINAL FANTASY

- In the 15-puzzle, I or right stick Up: the row and column of the highlighted tile.
- On the class choice at the start of a new game, I: the class description.

### FINAL FANTASY II

- I, in the keyword lists when you Ask or Learn, and in the Words menu: the keyword's description.
- U, or right stick Left, in shops and the equipment menu: "Any character can equip this". FINAL FANTASY II has no class restrictions.
- H also reads MP.

### FINAL FANTASY III

- 9: beacon navigation on or off, like F6. Field only.
- R, on a character's status screen: repeat the current statistic. Elsewhere R repeats dialogue.
- I, in the job menu: the job's description.

### FINAL FANTASY IV

- 9: beacon navigation on or off, like F6. Field only.
- T: read the active timers, such as escape countdowns.
- Shift+T: freeze or resume the timers.
- H also reads MP.

### FINAL FANTASY V

- 9: beacon navigation on or off, like F6. Field only.
- Shift+Semicolon: landing pings on or off. Field only.
- T: read the active timers.
- Shift+T: freeze or resume the timers.
- H also reads MP.
- I, in the Jobs and Abilities screens: the job's, ability's or spell's description.
- L, on the battle results screen: open the results log. It lists each character's EXP gained, EXP to the next level, and ABP to the next job level.

In the results log:

- Up and Down arrows: previous or next character. Controller: D-pad or left stick.
- Left and Right arrows: move between EXP, Next and ABP. Controller: D-pad or left stick.
- Page Up and Page Down: previous or next page. Controller: LB and RB.
- Enter or Home: read the current line again. Controller: A.
- Escape: close the log. Controller: B.

## Mod menu

Open it with F8 or Start, on the field with no menu open. The game gets no keys while it is open.

Keyboard:

- Up and Down arrows: move. Section names are skipped.
- Left and Right arrows: change the value. Volumes change in steps of 5 percent.
- Enter or Space: toggle or choose. On a volume, it mutes, or sets it back to 50 percent.
- I: describe the highlighted setting.
- Escape or F8: close.

Each entry is read with its value and its position in the menu.

The entries, by section, with their defaults:

### Audio Feedback

- Wall Tones, off: tones play as you approach walls.
- Footsteps, off: a click for each tile you move.
- Beacon Navigation, off: the audio beacon guides you, and turn by turn directions are off.
- Landing Pings, off: on a ship or submarine, pings the directions in which you can land. Not in FINAL FANTASY IV.

### Volume Controls

All from 0 to 100 percent, default 50.

- Wall Bump Volume.
- Footstep Volume.
- Wall Tone Volume.
- Beacon Volume.
- Landing Ping Volume. Not in FINAL FANTASY IV.

### Navigation Filters

- Pathfinding Filter, off.
- Map Exit Filter, off.
- Layer Transition Filter, off.
- Stick Click Normalization, off.

### Battle Settings

- Enemy HP Display, Numbers: Numbers, Percentage or Hidden.
- Enemy Letters, off: adds a letter such as A or B to enemies that share a name. FINAL FANTASY V only. It has no effect yet; see Known issues.
- Multi-hit Damage, With hit count: "With hit count" reads "14x1552 damage", "Total only" reads the total. FINAL FANTASY, II and III only.

### Battle Results

Not in FINAL FANTASY II, which has no experience points.

- EXP Counter Sound, on: a tick plays while the EXP bar fills.
- EXP Counter Volume, 50.

### Announcements

- Auto Detail, on: descriptions and stats are read as you move through items, magic, equipment and shops.
- Beacon Destination Announcement, off: restarting the beacon also says the destination.
- Menu Position Announcements, on: list entries say their position, such as "3 of 12".

### Close Menu

Closes the mod menu.

## Features

### Speech

- Dialogue, page by page, with the speaker's name.
- Story text that scrolls or fades in. In FINAL FANTASY III it is read line by line.
- "Entering" and the map's name when you change maps.
- Popups, confirmations and Game Over.
- "Press any button" on the title screen.
- The mod's own words follow the game's language: English, Japanese, French, Italian, German, Spanish, Korean, traditional and simplified Chinese, Russian, Thai and Portuguese.
- Some map objects only have Japanese names in the game. The mod translates them.

### Menus

- The highlighted entry is read as you move, with its position, such as "3 of 12".
- Items, equipment, magic, status, configuration, the controls list, shops with quantity and total, save and load, the bestiary, the gallery and the music player.
- FINAL FANTASY: the class choice at a new game, spell charges per level, spell shops, the 15-puzzle.
- FINAL FANTASY II: keywords, with Ask, Learn and the Words menu; weapon skill levels; spell levels with their percentage.
- FINAL FANTASY III: job change with job level, Onion Knight naming, the magic menu with Learn, Use, Remove, Forget and Exchange, and items that people ask you for.
- FINAL FANTASY IV: the Ability menu, party setting with front and back row, and naming with Namingway.
- FINAL FANTASY V: jobs with job level and ABP, abilities and ability slots, spells with MP, and name change.

### Battle

- Whose turn it is, the commands, and targets with their status. "All allies" and "All enemies" in FINAL FANTASY IV and V.
- Item and spell lists, with descriptions.
- Actions, damage, healing, misses, and statuses added and removed.
- Multi-hit attacks as "14x1552 damage", in FINAL FANTASY, II and III.
- The pause menu.
- Results: gil, EXP, level-ups and items. In FINAL FANTASY II: gil, weapon skills, spell levels and stat gains.
- A tick sound while the EXP bar fills, except in FINAL FANTASY II.

### Field

- The destination list: chests, opened or not, people, exits, events, vehicles and save points.
- Boarding and leaving vehicles, and "Can land" where you can land.
- Timers in FINAL FANTASY IV and V.

### Pathfinding

- Step by step directions to a destination or a waypoint.
- Long routes: when the game cannot find a far route, the mod joins several shorter ones. A long route reads up to 6 legs, then "and" the number of legs left.
- The pathfinding filter and the beacon only check short routes. With the filter on, a far destination may be left out of the list.
- FINAL FANTASY III, IV and V: the pathfinder knows secret passages, and leads through one when there is no other way. Wall tones may still sound at the start of a passage, as if it were a wall. Follow the spoken directions into it.

### Audio cues

- Wall bump: a sound when you walk into a wall.
- Footsteps: a click for each tile.
- Wall tones: a tone for each direction with a wall near you.
- Beacon: a sound from the direction of the selected destination.
- Landing pings: on a ship or submarine, pings toward places where you can land. Not in FINAL FANTASY IV.
- EXP tick: while the EXP bar fills after a battle.

### Controller

- Xbox, PlayStation and Nintendo controllers, through SDL3. The mod speaks the button names of your controller.
- The keyboard and the controller work together.

## Known issues

### All games

- This is the first version of the combined mod. If something the old screen reader read is now silent, or is read twice, please report it and send the log.
- In shops, the shopkeeper stands behind a counter. Talk to the counter to open the shop. It is usually straight north from the door.

### FINAL FANTASY and FINAL FANTASY II

- Secret passages, even when opened, do not show on the pathfinder. Use wall bumps and estimation to find them. They are usually near the thing that opens them.

### FINAL FANTASY

- On the class choice at a new game, the class may not be read in languages other than English.

### FINAL FANTASY III

- Some of the game's battle system messages are not read yet.
- On the status screen, Attack reads one number. The old mod's "hits times power" is not in yet.

### FINAL FANTASY IV and FINAL FANTASY V

- When a new turn is announced, it can cut off the line before it.
- Steal results are not read yet.

### FINAL FANTASY IV

- The Falcon and the Lunar Whale are called "airship".
- "Hovercraft" and "yellow chocobo" are spoken in English in every language.
- The old mod's special pathfinding on the moon is not in yet.

### FINAL FANTASY V

- The Enemy Letters setting has no effect yet, and some statuses on battle targets are not read yet.
- The results log has one page, with EXP, Next and ABP. Level-ups, learned abilities and items are not in it yet.
- Mod mode then B does not open the results log. Use L.
- ABP totals on the victory screen are not read yet.
- Some kinds of damage are read as plain "damage".
- Boko and some other vehicles may not be named correctly yet.

## Credits

- bladestorm360, for the mod.
- ZKline, for making the original FF6 mod, without which I likely never would have tried modding these games.
- Stirlock and UnexplainedEntity, for testing.
- Stirlock, for obtaining Japanese entity names so they could be translated.
- G-rad, for testing FINAL FANTASY IV and collecting entity names for translation.

The mod uses:

- Tolk, by Davy Kager, under the GNU Lesser General Public License version 3.
- MinHook, by Tsuda Kageyu, under the BSD 2-clause license.
- SDL3, by Sam Lantinga and the SDL contributors, under the zlib license.
- The NVDA Controller Client, by NV Access.
