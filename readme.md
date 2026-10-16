# Final Fantasy Pixel Master Core Screen Reader V0.9.3

## Purpose

Adds speech, menu and battle reading, pathfinding, optional autowalk and audio navigation to FINAL FANTASY I-VI Pixel Remaster on Steam. One set of files supports all six games. Keyboard and controller controls are supported.

## Known Issues

V0.9.3 is an early combined release. Some game-specific screens, airships and scripted map exits still need gameplay testing. Intermittent stalls are still under investigation; automatic timing logs remain enabled. FFIII and FFVI automatic airship boarding is disabled: board and take the wheel manually before requesting a flying route. FFVI flight autowalk has passed offline checks but still needs gameplay confirmation. Report missing or repeated speech with the game's ffpr_screen_reader.log and a short description of what happened.

Timing diagnostics run automatically in every session and use that same log. If intermittent lag returns, note what you were doing and report it. The five previous log files are retained as ffpr_screen_reader.1.log through .5.log, so the affected run can still be checked after restarting.

## Install

Requires 64-bit Windows, a Steam Pixel Remaster game and a running screen reader such as NVDA or JAWS. Release bundles include Prism 0.18.3 and SDL3; builds with the soundscape also include OpenAL Soft for optional HRTF. MelonLoader, BepInEx, Python and separate dependency downloads are not required.

1. Close the game and extract the zip.
2. Open the game's installation folder using Steam's Manage, Browse local files command. This is the folder containing FINAL FANTASY.exe, FINAL FANTASY II.exe, and so on.
3. If an older screen reader is installed, remove its DLL from the game's Mods folder: FFI_ScreenReader.dll, FFII_ScreenReader.dll, FFIII_ScreenReader.dll, FFIV_ScreenReader.dll, FFV_ScreenReader.dll or FFVI_ScreenReader.dll, as appropriate. Keep your UserData folder.
4. Open the extracted Final Fantasy Pixel Master Core Screen Reader folder. Copy its contents into the game folder, including the .ffpr-deps folder. hid.dll and SDL3.dll must sit beside the game's EXE. Keep Prism at .ffpr-deps/prism/prism.dll and the soundscape build's OpenAL Soft at .ffpr-deps/openal-soft/soft_oal.dll, with their notices.
5. Start your screen reader, then the game. The title screen should speak. Repeat these steps for each game you use.

The included license files should stay with the mod. Other mods' MelonLoader or BepInEx installations may remain; remove only the old screen reader DLL.

Settings are saved separately for each game in ffpr_screen_reader.ini. Old settings are imported once from UserData/MelonPreferences.cfg. Existing waypoints are retained in UserData/waypoints.json, or UserData/ffvi_waypoints.json for FFVI.

To update, close the game and replace the release files. To uninstall, remove hid.dll. Your settings and waypoints can remain.

## Keys

Mod keys require the game window to have focus. Field controls require the field to be active with no menu open. Mod keys are disabled while typing in the game's name fields. There is no keyboard mod mode. Shared mod commands use the same defaults in all six games. The lists below describe those defaults.

### Remapping mod controls

Open F8, Navigation, Controls, then Keyboard bindings or Controller bindings. Select a command in its named context to hear its current binding. Each command has Capture, Key or Primary input, Modifiers or Modifier input, Unbind and Reset binding options. Left/Right changes the input fields; select Capture to enter a new binding directly. Keyboard bindings accept Control, Shift and Alt combinations, function keys through F24 and keypad keys. To capture a controller chord, hold the first input and press the second. Release a single controller input to finish capturing it. Buttons, triggers, stick directions and supported extra buttons can be assigned. Escape cancels capture; use the Key field to assign Escape itself.

Conflicts in overlapping contexts are announced; unbind the other command first. Reset all bindings restores every mod binding after a confirmation. Settings remain separate for each game. The Letters and Brackets navigation layouts remain selectable, and each layout has its own editable bindings. Mod-menu, confirmation and text-editing commands can also be changed; actual typed characters retain their normal meaning. The Global controller entry Toggle controller mod mode changes the button that enters and leaves Mod Mode. Each Mod mode context has separate bindings for its individual functions, including field information, battle information, results review and dialogue repeat. The old mode button is freed after reassignment; a function may use it once conflicts are cleared. Controller mod mode remains controller only. Configure the game's own controls in the game menu.

Opening and closing the mod menu have separate entries, both defaulting to F8 on keyboard. Keep a usable way to navigate and close the menu when changing its controls. If needed, with the game closed, remove only the lines beginning with `Binding.` from ffpr_screen_reader.ini to restore control defaults while preserving other settings.

### Game

- W, A, S, D or arrows: move and select.
- Enter: confirm.
- Backspace or Escape: cancel.
- Tab: open or close the game menu.
- Q: switch the current stats/description or Esper panel where available; random name on naming screens.
- 2 / 3: previous / next native page or character where shown. On equipment information screens these change the numbered page; Q changes the footer without advancing that page.
- F1: walk/run where the game permits it.
- F3: toggle random encounters.

### Mod

Choose a pathfinding layout in F8, Navigation, Navigation keymap. Letters is the default; only the selected layout is active. The choice is saved separately for each game.

Letters layout:

- J / L: previous / next destination.
- K: announce the selected destination.
- P: request directions or beacon navigation. With Autowalk enabled, start walking; press again to stop.
- Shift+J / Shift+L: previous / next destination category.
- Shift+K: reset to All.
- Shift+P: cycle Current floor, Current map with vehicles, Current map.
- Ctrl+P: toggle the layer transition filter.

Brackets and backslash layout:

- Left bracket / right bracket: previous / next destination.
- P: announce the selected destination. K is inactive in this layout.
- Backslash: request directions or beacon navigation; start/stop Autowalk when enabled.
- Shift+left bracket / Shift+right bracket: previous / next destination category.
- Shift+P: reset to All.
- Shift+Backslash: cycle Current floor, Current map with vehicles, Current map.
- Ctrl+Backslash: toggle the layer transition filter.

Common commands in both layouts:

- Backtick, above Tab: rescan the map.
- Shift+M: toggle the map exit filter, grouping exits by destination and preferring the nearest reachable entrance.
- Ctrl+arrow: teleport north, south, west or east of the selected destination, if Navigation / Teleport shortcuts is enabled (default off).
- Shift+G: start or repeat the current puzzle-guide step.
- Ctrl+G: stop the puzzle guide.
- G: announce gil.
- M: announce the current map.
- V: announce walking or the current vehicle.
- H: announce the active character's vitals in battle; party information elsewhere. FFVI also reports airship heading and altitude when applicable.
- T in battle: announce the active turn.
- 1-9 in battle: read the corresponding party member's vitals. In shops: read that party member's equipment comparison.
- R: repeat the current dialogue or message.
- I: read the highlighted item's, spell's or setting's details. In equipment and shops, read the current stats/description panel, following Q or X/Square. In the mod menu, I describes the highlighted setting.
- Shift+I: read the controls for the current screen.

Current map includes connected dungeon/town floors. Chests use generic labels unless Speech / Spoil treasure chest contents is enabled; opened state is included when known. Boat routes include the direction for boarding and landing, for example: 16 North, board the Ship North, 15 West, land South. The boarding and landing steps are not counted twice.
Every other-floor object keeps its floor name, including known opened/unopened chest state. An object on this floor that needs a detour through another floor says "Requires travelling through [floor] first". Browsing announces steps and direction only when a current route reaches the object directly, without another floor, warp, trap or intervening event.
Use Current map to keep eligible events and interactive objects available while travelling between floors. Tracking retains the original object and its floor through each leg. If the object becomes unavailable, tracking stops instead of switching to a different object.

Each individual map exit routes to its highlighted tile, including entrances that lead to the same floor. World map exit lists include reachable transition trigger tiles. The map exit filter updates the nearest entrance after movement; turn it off to browse individual reachable entrances.

### Waypoints (field only)

- Comma / Period: previous / next waypoint.
- Shift+Comma / Shift+Period: previous / next waypoint category.
- Slash: navigate to the selected waypoint; start/stop Autowalk when enabled.
- Shift+Slash: add a waypoint at your current location and name it.
- Ctrl+Period: rename the selected waypoint.
- Ctrl+Slash: remove the selected waypoint.
- Ctrl+Shift+Slash: clear this map's waypoints.

In the waypoint name window, type normally. Left/Right reviews letters, Up/Down reads the name, Home/End jumps to an end, Backspace deletes, Enter saves and Escape cancels. In confirmation windows, use Y/N or Left/Right and Enter; Escape cancels.

### Other toggles

- Apostrophe: toggle footsteps.
- Semicolon: toggle wall tones.
- Shift+Semicolon / Shift+Apostrophe: reduce / increase soundscape distance by five steps, from 5 to 25. These do nothing when Soundscape is off. They do not change the unshifted wall-tone and footstep toggles.
- Ctrl+Apostrophe: turn the soundscape on or off. Beacon navigation and spoken pathfinding remain available with either setting.
- F6: toggle beacon navigation.
- Shift+F6: switch the beacon between Follow route and Object location. The choice is saved even while the beacon is off.
- F5 in battle: cycle enemy HP Numbers, Percentage or Hidden. Enemy HP is an optional extra; the native target panel does not display it.
- F7: toggle Autodetail.
- F8 on the field: open the mod menu. F8 closes it directly.

The mod menu has Speech, Navigation, Navigation audio and Battle categories. Speech starts with Game speech (default on). Turning it off mutes game and gameplay announcements; the speech-only mod menu continues speaking, including its help and close announcement. Speech also includes Autodetail, Verbose/Brief stat deltas, Status damage (default on) and Spoil treasure chest contents (default off). Navigation includes Autowalk, Navigation keymap, Teleport shortcuts, Speak autowalk path, filters and Stick Click Normalization. Navigation audio starts with its master switch (default on), followed by HRTF, Soundscape and Soundscape settings. The audio master silences every mod sound while preserving individual preferences. It also contains the existing sound toggles and volumes, Beacon with directions, the EXP counter where supported, and Sound previews. Autowalk, Speak autowalk path, Beacon with directions, Soundscape and HRTF remain independently default off.

Landing Pings is available through Navigation audio in FFI, FFII, FFIII and FFV, default off. It identifies adjacent legal shores while sailing a ship or submarine, and also while using the canoe in FFI–FFIII. It works after loading a save aboard a watercraft. FFII's snowcraft and aircraft are excluded, as are FFIV and FFVI. The old field 9 beacon alias, Minus/Equals category aliases, FFIII status R and FFV Shift+Semicolon shortcuts are removed. F6 toggles beacons in every game. There are no diagnostic or data-dumping hotkeys. Backtick refreshes the navigation list without writing a data dump.

Teleport shortcuts require clear terrain and an unoccupied landing with a legal way out. A blocked, unknown or stale landing leaves you in place. Teleporting while riding is unavailable.

In the mod menu, Up/Down selects a category or setting, Enter/Space opens or toggles it, Left/Right changes a value, I reads its description, and Escape returns to the parent menu or closes from the root. Controller navigation uses the same menu and settings.

Beacon mode is also available in F8 > Navigation audio. Follow route (the default) guides you to each leg of the route. Object location pings the selected entity or waypoint directly, even through walls, without checking whether you can walk straight to it. Object mode works on the current floor; for a destination on another floor, use Follow route. On world maps, object bearings follow the shortest direction across the map edges. Both modes keep the existing arrival silence and resume when you move away. F6 stays the on/off switch, and Beacon with directions can still be used with either mode. Soundscape, the Autowalk setting, spoken directions and volume settings remain independent. Like other manual keys, Shift+F6 stops an active autowalk.

### Soundscape and spatial audio

In F8, Navigation audio, turn on Soundscape, or press Ctrl+Apostrophe on the field. Soundscape settings contains its master volume, distance (10 steps by default), and a switch, volume and preview for each cue. Volumes run from 0 to 100. Previews use your volume settings even if that individual cue is switched off; the Navigation audio master still applies. Soundscape, beacon navigation and spoken directions work independently and can be used together. Existing wall bumps, footsteps, wall tones, beacon, EXP counter and landing pings retain their original synthesis, envelopes and default levels; optional HRTF spatializes those original signals.

Each nearby entity has its own repeating cue, independent of the selected destination and beacon. Only entities already eligible for the pathfinder participate; entries on another floor, opened chests and ToLayer transitions are silent. Distance is straight-line tile distance, with wrapping at world-map edges.

- NPC: mid-range chirp.
- Chest: muted chime, without revealing contents.
- Map exit: rising harmonic triple tone.
- Exit door: synthesized knock when a verified map exit has a door on its tile or within one tile, including diagonals. A standalone door is silent. The exit owns the cue, so the door event does not add a second sound.
- Event: mid-range double beep for eligible story, interaction and progress events, including ordinary warps and trial pads.
- Trap: lower double beep for explicitly identified falling, collapsing or resetting hazards. It repeats faster within two steps. A warp or forced battle alone does not classify an event as a trap.
- Vehicle: lower double chirp.
- Save point: gentle two-note cue.

Sounds pan left/right. Without HRTF, the southern half of the map uses a lower pitch to distinguish behind from ahead. Nearby cues of the same kind vary slightly in pathfinder order: normal, higher, lower, then further small variations. Long replacement clips automatically leave a gap before repeating. Every in-range entity retains a voice schedule; crowded scenes defer starts rather than selecting only the nearest entities.

Volume controls automatically remember separate HRTF and stereo levels. Use the same existing sliders with HRTF on or off; switching modes restores that mode's saved values. Each HRTF volume initially defaults to 50%, while your existing stereo levels are preserved. Active HRTF applies a single 2× output boost to every mod sound, including wall bumps, footsteps and previews. Zero volume still mutes.

HRTF is an independent, default-off setting for headphones. It places the wall tones, beacon, landing pings and soundscape in spatial audio, with map north ahead, south behind, east right and west left. This frame stays fixed when your character turns. With HRTF active, the renderer does not shift cue pitch for direction or for neighboring entities. Wall tones retain their distinct directional pitches. If HRTF is unavailable, stereo and pitch cues take over automatically. OpenAL Soft runs privately through the existing SDL output; no system OpenAL installation or audio-device change is needed. Its DLL is independently replaceable; retain the accompanying license and source notices.

### Replacing mod sounds

Create a folder named sounds beside the game's EXE. Put replacement WAV files directly inside it using the exact filenames below. Restart the game after changing files. The folder is optional: a missing folder, empty folder, missing file or invalid individual file uses that cue's embedded default. Do not put these files in the game's own audio data folders.

Use uncompressed 16-bit PCM WAV, mono or stereo, 8,000–96,000 Hz, at most four seconds per file. Stereo is mixed to mono so the mod can position the sound. WAV files with other encodings, including floating-point or compressed audio, fall back to the embedded cue. Keep replacements comfortably quiet; the menu's volume controls still apply. Continuous wall and EXP clips loop, so seamless starts and ends work best.

- footstep.wav: each tile of movement.
- wall_bump.wav: collision thud.
- wall_north.wav, wall_south.wav, wall_east.wav, wall_west.wav: continuous wall proximity tones. Retain distinct pitches if you want the existing direction cues.
- beacon.wav: ordinary route or object beacon; fallback rear-direction pitch is applied by the mod.
- beacon_blocked.wav: lower beacon for a blocked or distant route.
- exp_counter.wav: looping battle-results EXP tick, where supported.
- landing.wav: vehicle landing ping, where supported; fallback directional pitches are applied by the mod.
- npc.wav: NPC chirp.
- chest.wav: unopened chest chime.
- exit.wav: map-exit triple tone.
- door.wav: exit-door knock.
- event.wav: meaningful event double beep.
- trap.wav: hazard double beep.
- vehicle.wav: vehicle double chirp.
- save_point.wav: save-point cue.

These are all mod-generated sound effects. Native game music, footsteps and menu/battle sounds remain under the game's own audio settings. The embedded defaults are original procedural sounds; their source generator is included in the repository.

### Battle results and information buffers

At battle entry in all six games, the mod announces the enemy formation with each enemy name and count, for example, "Enemy formation. 3 Goblin. 2 Hornet." Native opening messages such as "Ambushed!" and "Pre-emptive strike!" are spoken before the formation, followed by the first character’s turn and command. Pressing Control can still stop this speech. The formation is announced once per battle, independently of Autodetail; Game speech controls it along with other gameplay speech.

- Up/Down: previous/next line.
- Left/Right: first/last line.
- I: details for the current page. On battle growth and weapon-progress pages, read all current-phase deltas.
- Enter: advance the native results phase normally.

Speech / Battle results: notifications once is off by default. Turn it on to announce a level-up or learned-ability notification once per battle, omitting its repeats from later results buffers. New notifications, displayed totals and manual review of the first buffer are preserved. Shops always announce the highlighted product’s displayed price, regardless of Autodetail or the native description/stat panel toggle.

Equipment replacement comparisons, extra item-detail screens and FFVI Esper information use an arrow-key buffer. W/A/S/D retains native equipment selection. I reads the current item panel for comparisons and the full current buffer on an extra item-detail screen. Equipment slots speak the slot label, displayed Favored Hand and item name; Autodetail adds the current panel. Extra item-detail entry includes the displayed quantity once; its buffer follows the current native page. The old U equipment-information key has been removed.

### Status, bestiary and controls lists

- Up/Down: previous/next entry; W/S also works.
- Shift+Up/Down: previous/next group where grouped; Shift+W/S also works.
- Ctrl+Up/Down: first/last entry; Ctrl+W/S also works.
- Native page and character-switch controls remain available.

### Fleeing from battle

- FFI-III: choose Flee from the battle command menu.
- FFIV-VI: hold the main keyboard 2 and 3 keys together. On Xbox hold LB+RB; on PlayStation hold L1+R1; on Nintendo hold L+R. Use normal game mode, with controller mod mode off. Keep both held to attempt escape; some battles do not allow fleeing.

These are the native default previous/next-character controls. If you remapped the keyboard, hold the corresponding previous/next-character keys instead.

### Game-specific keys

- FFI: I in the 15-puzzle reads the tile's row and column; I on new-game class selection reads the class description. F1 runs in towns/dungeons; the world map is walk-only.
- FFII: I in Ask/Learn and key-term lists reads the selected term's description.
- FFIII: I in job selection reads the displayed Equippable equipment types.
- FFIV-VI: T outside battle reads active timers; Shift+F5 freezes/resumes them.
- FFI-VI: extra equipment/item detail screens support arrow-key review; native page controls remain available.

### Game controller

Button names below use Xbox, with PlayStation in brackets. The mod announces labels appropriate to Xbox, PlayStation or Nintendo controllers.

- Left stick: movement and native menu selection.
- A (Cross): confirm.
- B (Circle): cancel.
- X (Square): native panel toggle where available.
- Y (Triangle): game menu.
- LB/RB (L1/R1): native page/character switching.
- RT (R2): native pause/page-down where available.

### Mod controller

- Back/View/Select: enter or cancel controller mod mode.
- Start/Menu/Options: open the mod menu on the active field, or close it.

#### Normal-state mod actions

- Field D-pad Up/Down: previous/next waypoint.
- Field D-pad Left/Right: previous/next waypoint category.
- Field right stick Up/Down: previous/next destination.
- Field right stick Left/Right: previous/next destination category.
- Field LT (L2/ZL): navigate to the last selected destination or waypoint; start/stop Autowalk when enabled.
- In menus, right stick Up: details, like I.
- In menus, right stick Down: screen controls, like Shift+I.
- Status/bestiary/controls lists: D-pad or left stick Up/Down reviews entries.

#### Mod mode combos

Press Back/Select, then the action button. Simple actions return to normal mode; party/buffer review remains in mod mode until cancelled.

- Dialogue: X (Square) repeats the message.
- Battle: X (Square) reads the active character; Y (Triangle) reads the active turn; LB/RB reviews party members.
- Results/information buffers: LB/RB or D-pad Up/Down reviews lines; D-pad Left/Right goes first/last; X (Square) reads details. On growth/weapon phases, details reads all current-phase deltas.
- Field: X (Square) reads gil; Y (Triangle) reads the map; A (Cross) reads vehicle state.
- Field: RB (R1) starts/repeats the puzzle guide; LB (L1) stops it.
- Field: right stick directions teleport beside the selected destination only when Navigation / Teleport shortcuts is enabled.

#### Stick clicks

- L3+R3 together on the field: toggle Stick Click Normalization.
- Normalization off: L3 toggles beacons; R3 cycles the pathfinding filter.
- Normalization on: native L3/R3 controls pass to the game. In mod mode, L3 toggles beacons and R3 cycles the filter.

#### Mod menu navigation

- D-pad or left stick Up/Down: select.
- D-pad or left stick Left/Right: adjust a value.
- A (Cross): open or toggle.
- B (Circle): return to categories, or close from the root.
- Start/Menu/Options: close directly.
- Right stick Down: read mod-menu controls.
- Keyboard I: describe the selected setting.

## Credits

- bladestorm360: mod author.
- ZKline / BlindGuyNW: original FINAL FANTASY VI screen reader and permission to include its work; inspiration for these mods.
- Stirlock: FINAL FANTASY and FINAL FANTASY III translation help, testing and obtaining Japanese entity names.
- UnexplainedEntity: FINAL FANTASY II translations and testing.
- Lethal Lawn Mower: FINAL FANTASY V translation help.
- G-rad: FINAL FANTASY VI translations, FINAL FANTASY IV testing and entity-name collection.
- Tsuda Kageyu and Vyacheslav Patkov: MinHook and Hacker Disassembler Engine. MinHook 1.3.4 is compiled into the mod; its license is included as MinHook_LICENSE.txt.
- Sam Lantinga and SDL contributors: SDL3; license included as SDL3_LICENSE.txt.
- ethindp and Prism contributors: Prism 0.18.3 speech/braille support. Its upstream license and third-party notices are included under .ffpr-deps/prism/LICENSES.

Mod source: https://github.com/bladestorm360/FFPRCore-Screen-Reader
Prism source: https://github.com/ethindp/prism/tree/94329ebeccbaee7f70b3494a290c9dfaac0184ee
