# Final Fantasy Pixel Master Core Screen Reader V0.9.2

## Purpose

Adds speech, menu and battle reading, pathfinding, optional autowalk and audio navigation to FINAL FANTASY I-VI Pixel Remaster on Steam. One set of files supports all six games. Keyboard and controller controls are supported.

## Known Issues

V0.9.2 is an early combined release. Some game-specific screens, airships and scripted map exits still need gameplay testing. FFIII and FFVI automatic airship boarding is disabled: board and take the wheel manually before requesting a flying route. FFVI flight autowalk has passed offline checks but still needs gameplay confirmation. Report missing or repeated speech with the game's ffpr_screen_reader.log and a short description of what happened.

## Install

Requires 64-bit Windows, a Steam Pixel Remaster game and a running screen reader such as NVDA or JAWS. Prism 0.18.3 and SDL3 are included in this V0.9.2 bundle. MelonLoader, BepInEx, Python and separate dependency downloads are not required.

1. Close the game and extract the zip.
2. Open the game's installation folder using Steam's Manage, Browse local files command. This is the folder containing FINAL FANTASY.exe, FINAL FANTASY II.exe, and so on.
3. If an older screen reader is installed, remove its DLL from the game's Mods folder: FFI_ScreenReader.dll, FFII_ScreenReader.dll, FFIII_ScreenReader.dll, FFIV_ScreenReader.dll, FFV_ScreenReader.dll or FFVI_ScreenReader.dll, as appropriate. Keep your UserData folder.
4. Open the extracted Final Fantasy Pixel Master Core Screen Reader V0.9.2 folder. Copy its contents into the game folder, including the .ffpr-deps folder. hid.dll and SDL3.dll must sit beside the game's EXE, and Prism must remain at .ffpr-deps/prism/prism.dll.
5. Start your screen reader, then the game. The title screen should speak. Repeat these steps for each game you use.

The included license files should stay with the mod. Other mods' MelonLoader or BepInEx installations may remain; remove only the old screen reader DLL.

Settings are saved separately for each game in ffpr_screen_reader.ini. Old settings are imported once from UserData/MelonPreferences.cfg. Existing waypoints are retained in UserData/waypoints.json, or UserData/ffvi_waypoints.json for FFVI.

To update, close the game and replace the release files. To uninstall, remove hid.dll. Your settings and waypoints can remain.

## Keys

Mod keys require the game window to have focus. Field controls require the field to be active with no menu open. Mod keys are disabled while typing in the game's name fields. There is no keyboard mod mode.

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

- J or left bracket: previous pathfinder destination.
- L or right bracket: next destination.
- Shift+J, Shift+left bracket or Minus: previous destination category.
- Shift+L, Shift+right bracket or Equals: next category.
- Shift+K: reset to All.
- K: repeat the selected destination.
- Backslash or P: request directions or beacon navigation. With Autowalk enabled, start walking; press again to stop.
- Shift+Backslash or Shift+P: cycle Current floor, Current map with vehicles, Current map.
- Ctrl+Backslash or Ctrl+P: toggle the layer transition filter.
- Backtick, above Tab: rescan the map.
- Shift+M: toggle the map exit filter, grouping exits by destination and preferring the nearest reachable entrance.
- Ctrl+arrow: teleport north, south, west or east of the selected destination.
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

Current map includes connected dungeon/town floors. Chests use generic labels unless Speech / Spoil treasure chest contents is enabled; opened state is included when known.

World map exit lists include reachable transition trigger tiles. The map exit filter updates the nearest entrance after movement; turn it off to browse individual reachable entrances.

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
- F6: toggle beacon navigation.
- F5 in battle: cycle enemy HP Numbers, Percentage or Hidden. Enemy HP is an optional extra; the native target panel does not display it.
- F7: toggle Autodetail.
- F8 on the field: open the mod menu. F8 closes it directly.

The mod menu has Speech, Navigation, Navigation audio and Battle categories. Speech includes Autodetail, Verbose/Brief stat deltas, Status damage (default on) and Spoil treasure chest contents (default off). Navigation includes Autowalk, Speak autowalk path, filters and Stick Click Normalization. Navigation audio contains sound toggles and their volume controls, including Beacon with directions and the EXP counter sound where supported. Autowalk, Speak autowalk path and Beacon with directions remain independently default off.

In the mod menu, Up/Down selects a category or setting, Enter/Space opens or toggles it, Left/Right changes a value, I reads its description, and Escape returns to categories or closes from the root.

### Battle results and information buffers

- Up/Down: previous/next line.
- Left/Right: first/last line.
- I: details for the current page. On battle growth and weapon-progress pages, read all current-phase deltas.
- Enter: advance the native results phase normally.

Equipment replacement comparisons, extra item-detail screens and FFVI Esper information use an arrow-key buffer. W/A/S/D retains native equipment selection. I reads the current item panel for comparisons and the full current buffer on an extra item-detail screen. Equipment slots speak the slot label, displayed Favored Hand and item name; Autodetail adds the current panel. Extra item-detail entry includes the displayed quantity once; its buffer follows the current native page. The old U equipment-information key has been removed.

### Status, bestiary and controls lists

- Up/Down: previous/next entry; W/S also works.
- Shift+Up/Down: previous/next group where grouped; Shift+W/S also works.
- Ctrl+Up/Down: first/last entry; Ctrl+W/S also works.
- Native page and character-switch controls remain available.

### Game-specific keys

- FFI: I in the 15-puzzle reads the tile's row and column; I on new-game class selection reads the class description. F1 runs in towns/dungeons; the world map is walk-only.
- FFII: I in Ask/Learn and key-term lists reads the selected term's description.
- FFIII: R on status repeats the current stat; I in job selection reads the displayed Equippable equipment types.
- FFIII-VI: 9 on the field toggles beacon navigation, like F6.
- FFIV-VI: T outside battle reads active timers; Shift+T freezes/resumes them.
- FFV: Shift+Semicolon toggles landing pings on the field.
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
- Field: right stick directions teleport beside the selected destination.

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
