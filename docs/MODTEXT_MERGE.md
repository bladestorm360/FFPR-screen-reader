# Mod text merge

How the five old `mod_text.json` files became one shared file plus per-game overrides.

Written 2026-10-05 for Phase 0 of `../UNIFIED_MOD_PLAN.md`.

## Files

- `data/common/mod_text.json`: shared mod UI text. All 850 keys of the five mods.
- `data/ff1/mod_text.json` to `data/ff5/mod_text.json`: per-game overrides. Game terms only.
- `tools/merge_modtext.py`: makes all of the above from the old repos.
  It also rewrites the generated report at the end of this file.

Every entry has 12 languages: en, ja, fr, it, de, es, ko, zht, zhc, ru, th, pt.

## Lookup order

The C++ looks up a key in this order:

1. `data/ffN/mod_text.json` for the running game.
2. `data/common/mod_text.json`.
3. The key itself, as the old mods did.

Each step uses the player's language. An override entry is complete: it has all 12 languages.
So step 1 never needs to fall back per language.

## How to re-run

From the repo root:

```
python tools\merge_modtext.py
python tools\merge_modtext.py --check
```

- The first writes the data files and the report below.
- `--check` writes nothing. It fails if the data files are out of date or have gaps.
- `--src-root` points at another folder holding `ff1\` to `FF5\`. The default is this repo's parent.

The old repos are only read.

## Merge rules

1. **Key in FF1:** FF1's entry goes in the common file. This is the parity rule: FF1 is the default.
2. **Key not in FF1:** the first of FF3, FF2, FF4, FF5 that has it. This is the plan's porting order.
   The report lists each of these keys and its source.
3. **Missing language:** filled from the same key in another mod, in the order FF1, FF3, FF2, FF4, FF5.
   None was needed. No translation was written by hand.
4. **Override:** a game gets one when both are true:
   - the key is a game term (see below);
   - the game's own entry differs from the common entry.

   The override is that game's whole entry, copied unchanged.
5. **Wording drift:** any other difference. FF1, or the chosen source, wins. No override.

## What counts as a game term

A label the game itself defines, whose wording follows that game's own text:

- status ailment names, such as `Poison` and `Doom`;
- character stats and stat lines, such as `Stamina` and `Strength: {0}`;
- the job or class term, such as `Job` and `No unlocked jobs can equip`;
- game menu commands, such as `Item`, `Buy` and `Key Items`;
- vehicle names, such as `Airship`, `On canoe` and `Airship Landing`;
- equipment slots, weapon types and armour types;
- the battle row labels `Front Row` and `Back Row`.

The full list is `_CATEGORIES` in the script.

**HP and MP.** The abbreviation is a game term:

- FF1 uses LP (de), VIT (es) and ОЗ (ru).
- FF2 to FF4 use KP, PV and HP.
- FF5 uses HP and MP in it, de, es, ru and pt.

A key whose English contains HP or MP gets an override only when the game's abbreviation differs from
the common one in some language. A difference in the rest of the sentence is drift.

**`Magic` means two things.** It is the menu command in FF1 and FF2, and the magic stat in FF4 and FF5.
Each game keeps its own entry.

## Considered and left as drift

These looked like game terms but were not overridden. FF1, or the chosen source, wins.

- `Gained {0} gil`, `{0} gil`: gil is the same currency in every game. Only the wording differs.
- `{0} gained {1} XP`, `EXP Counter Sound`, `EXP Counter Volume`: EXP, XP and EP are mixed within each game.
  There is no consistent per-game term.
- MP sentences in FF2 to FF4, such as `{0}: Recovered {1} MP`: same PM abbreviation as FF1.
  Only the wording differs. FF5 does get overrides for these.
- `Low Flying`, `Walking`, `Dashing`, `Unique`, `Vehicle {0}`: mod descriptions of movement, not game labels.
- `Treasure Chest`, `Save Point`, `Map Exit` and the other entity categories: mod labels.
- `Game Over`, `Bestiary. Encountered: {0} of {1}`, `Formation view`, bestiary section labels: shared extras text.
- `Can equip: {0}`, `Status {0}`, `{0} learned {1}`: sentences without a game term.

## Not verified

- No override was checked against the games' own message tables.
  The rule keeps what each old mod shipped for its own game.
- Some of those entries may be wrong for their game. Examples:
  - FF5's `Slow` in French is "Somni";
  - FF3's `magical armor` says "magitek".

  They are kept as they were. Fixing them is a translation task, not part of this merge.

## Notes for the port

- **The common file holds every key, including game terms that only one game uses.**
  Examples: FF1 `Light Warrior {0}`, FF3 `Onion Knight {0}`, FF4 `Enterprise`, FF5 `ABP` and `Haste`.
  A game that never asks for the key never sees it.
- **Watch for a game-term key a game did not have before.** Shared core code can make a game ask for one.
  The common entry then carries another game's term.
  - Example: FF5's old mod has `airship` but not `Airship`. Common `Airship` is FF1's. FF1's French is "Vaisseau".
  - Fix: add an override to that game's file, taken from that game's own text.
  - Never copy a game term from another game.
- **Source format.** All five old files are UTF-8 without BOM, with CRLF endings. FF5's puts each entry on one line.
  The new files are UTF-8 without BOM, with LF endings and 2-space indentation, and are not ASCII-escaped.

## Other data files

- `data/ffN/translation.json`: each mod's entity-name file (game content), copied byte for byte.
  Every mod embeds its own as the resource `translation.json`. They stay per game.
- `EntityNames.json`: not copied. No mod embeds or loads one.
  - FF1 and FF3 have one at the repo root. These are stale dumps of Japanese names with empty values.
    FF1's `docs/debug.md` calls its copy legacy.
  - FF3 and FF5 write one to `UserData` with a developer dump hotkey. That is output, not input.
- `FF5/translation.generated.json`: not copied. It is identical to FF5's `translation.json`.
- `FF1_translations.json`, `FFII_translations.json`, `FFIII_translations.json`: not copied. These are legacy files that nothing loads.

<!-- BEGIN GENERATED: tools/merge_modtext.py -->

## Generated report

Written by `tools/merge_modtext.py`. Do not edit by hand.

### Counts

- Source keys: FF1 513, FF2 391, FF3 400, FF4 338, FF5 410.
- Keys in the common file: 850.
- Common entries taken from: FF1 513, FF3 86, FF2 48, FF4 92, FF5 111.
- Overrides: FF1 0, FF2 49, FF3 42, FF4 26, FF5 39.
- Languages filled from another mod: 0.
- Missing-language gaps after the merge: 0.
- Placeholder mismatches (reported, not fixed): 0.

### Overrides

#### FF1: 0

None.

#### FF2: 49

- `Enemy HP Display`: HP/MP abbreviation: de KP vs LP, es PV vs VIT, ru HP vs ОЗ.
- `Enemy HP: {0}`: HP/MP abbreviation: de KP vs LP, es PV vs VIT, ru HP vs ОЗ.
- `Ability`: game menu command label; differs in fr.
- `Back`: game menu command label; differs in ja, es, ko.
- `Config`: game menu command label; differs in ja, fr, es, zhc, ru, th, pt.
- `Equipment`: game menu command label; differs in ja, th, pt.
- `Interruption`: game menu command label; differs in ja, fr, it, de, es, ko, ru, th, pt.
- `Item`: game menu command label; differs in fr, it, de, es, ru, th, pt.
- `Job`: job/class term; differs in ko, pt.
- `Load`: game menu command label; differs in fr, zht.
- `Magic`: menu command in FF1/FF2, magic stat in FF4/FF5; differs in ja.
- `Save`: game menu command label; differs in fr, ko.
- `Sort`: game menu command label; differs in ja, fr, ko, zht, zhc, th, pt.
- `Status`: game menu command label; differs in fr, es.
- `Words`: game menu command label; differs in de, th.
- `{0} for party HP. {1} to cancel.`: HP/MP abbreviation: de KP vs LP, es PV vs VIT, ru HP vs ОЗ.
- `Controls how enemy HP appears in battle: numeric value, percentage of max, or hidden.`: HP/MP abbreviation: de KP vs LP, es PV vs VIT, ru HP vs ОЗ.
- `Accessory`: equipment slot label; differs in ko.
- `Accessory 2`: equipment slot label; differs in ja, ko, zht, zhc.
- `Accuracy`: character stat label; differs in ja, de, es, ko, zht, zhc.
- `Agility`: character stat label; differs in th.
- `Airship`: vehicle name; differs in fr, es, pt.
- `Airship Landing`: vehicle name; differs in fr, it, es, ko, ru, th.
- `Airship Landings`: vehicle name; differs in fr, it, es, ko, ru, th.
- `Attack`: character stat label; differs in de.
- `Body`: equipment slot label; differs in it, de, ko, ru.
- `Buy`: game menu command label; differs in ja, ko, zht, zhc.
- `Canoe`: vehicle name; differs in th.
- `Chocobo`: vehicle name; differs in th.
- `Equip`: game menu command label; differs in ja, it, de, es, ru.
- `Evasion`: character stat label; differs in ja, it, ko, zht, zhc.
- `HP`: HP/MP abbreviation: de KP vs LP, es PV vs VIT, ru HP vs ОЗ.
- `Head`: equipment slot label; differs in it, ru.
- `Intellect`: character stat label; differs in fr, es, ko, th.
- `Key Items`: game menu command label; differs in fr, it, de, es, zht, zhc, ru, th, pt.
- `Learn`: game menu command label; differs in ja, fr, it, de, es, ko, zht, zhc, ru, th, pt.
- `On airship`: vehicle name; differs in ja, fr, de, es, pt.
- `On canoe`: vehicle name; differs in ja, ru, th.
- `On ship`: vehicle name; differs in ja, fr, de, zht, zhc, pt.
- `Remove`: game menu command label; differs in ja, de, zht, zhc, ru, th.
- `Remove All`: game menu command label; differs in ja, fr, ko, zht, ru.
- `Sell`: game menu command label; differs in ja, ko, zht, zhc.
- `Ship`: vehicle name; differs in fr.
- `Stamina`: character stat label; differs in fr, de, es, th, pt.
- `Strength`: character stat label; differs in th.
- `Use`: game menu command label; differs in ja, ko.
- `Vitality`: character stat label; differs in ru, th.
- `{0}: HP {1}/{2}`: HP/MP abbreviation: de KP vs LP, es PV vs VIT, ru HP vs ОЗ.
- `{0}: Recovered {1} HP`: HP/MP abbreviation: de KP vs LP, es PV vs VIT, ru HP vs ОЗ.

#### FF3: 42

- `Enemy HP Display`: HP/MP abbreviation: de KP vs LP, es PV vs VIT, ru HP vs ОЗ.
- `Enemy HP: {0}`: HP/MP abbreviation: de KP vs LP, es PV vs VIT, ru HP vs ОЗ.
- `Ship`: vehicle name; differs in fr.
- `Airship`: vehicle name; differs in fr, es, th, pt.
- `Strength`: character stat label; differs in th.
- `Defense`: character stat label; differs in ja, de, ko, zht, zhc, th.
- `HP +{0}`: HP/MP abbreviation: de KP vs LP, es PV vs VIT, ru HP vs ОЗ.
- `Strength +{0}`: character stat label; differs in ja, de, es, zht, zhc, th.
- `Agility +{0}`: character stat label; differs in ja, de, es, ko, zht, zhc, th.
- `: HP {0}/{1}`: HP/MP abbreviation: de KP vs LP, es PV vs VIT, ru HP vs ОЗ.
- `Accuracy: {0}%`: character stat label; differs in de, es, ru.
- `Agility: {0}`: character stat label; differs in de, ko, th.
- `Back`: game menu command label; differs in ja, es, th.
- `Back Row`: battle row label; differs in fr, it, es, pt.
- `Buy`: game menu command label; differs in ja, zht, zhc.
- `Defense: {0}`: character stat label; differs in de.
- `Equipment`: game menu command label; differs in ja, th, pt.
- `Evasion: {0}%`: character stat label; differs in it, de, es, zhc, th.
- `Front Row`: battle row label; differs in fr, it, es.
- `HP {0}`: HP/MP abbreviation: de KP vs LP, es PV vs VIT, ru HP vs ОЗ.
- `HP {0}/{1}`: HP/MP abbreviation: de KP vs LP, es PV vs VIT, ru HP vs ОЗ.
- `HP: {0} / {1}`: HP/MP abbreviation: de KP vs LP, es PV vs VIT, ru HP vs ОЗ.
- `Intellect: {0}`: character stat label; differs in fr, th, pt.
- `Job: {0}`: job/class term; differs in ja, fr, es, ru, pt.
- `Next Level: {0}`: character stat label; differs in it, de, zht, zhc, th.
- `No unlocked jobs can equip`: job/class term; differs in ja, fr, it, de, es, ko, ru, th.
- `Remove`: game menu command label; differs in ja, de, zht, zhc, ru, th.
- `Sell`: game menu command label; differs in ja, zht, zhc.
- `Stamina: {0}`: character stat label; differs in fr, de, es, zht, zhc, th.
- `Status`: game menu command label; differs in fr, es, ru.
- `Strength: {0}`: character stat label; differs in th.
- `airship`: vehicle name; differs in fr, es, th, pt.
- `ship`: vehicle name; differs in fr.
- `{0}: HP {1}/{2}`: HP/MP abbreviation: de KP vs LP, es PV vs VIT, ru HP vs ОЗ.
- `{0}: Recovered {1} HP`: HP/MP abbreviation: de KP vs LP, es PV vs VIT, ru HP vs ОЗ.
- `Use`: game menu command label; differs in ja, ko.
- `Forget`: game menu command label; differs in ja, fr.
- `Exchange`: game menu command label; differs in ja, ru.
- `Head`: equipment slot label; differs in it, ru.
- `Body`: equipment slot label; differs in it, de, ko, ru.
- `Accessory`: equipment slot label; differs in ko.
- `Accessory 2`: equipment slot label; differs in ja, ko, zht, zhc.

#### FF4: 26

- `Front Row`: battle row label; differs in fr, it, ko, pt.
- `Back Row`: battle row label; differs in fr, it, ko.
- `Next Level`: character stat label; differs in fr, it.
- `HP`: HP/MP abbreviation: de KP vs LP, es PV vs VIT, ru HP vs ОЗ.
- `Strength`: character stat label; differs in th.
- `Agility`: character stat label; differs in ko, th.
- `Stamina`: character stat label; differs in fr, de, es, zht, zhc, th.
- `Intellect`: character stat label; differs in fr, it, de, es, th.
- `Spirit`: character stat label; differs in th.
- `Attack`: character stat label; differs in ja, de, ko, zht, zhc, th.
- `Accuracy`: character stat label; differs in ja, de, es, ko, zht, zhc.
- `Defense`: character stat label; differs in ja, de, ko, zht, zhc, th.
- `Evasion`: character stat label; differs in ja, it, ko, zht, zhc, th.
- `Magic Defense`: character stat label; differs in de, th.
- `Magic Evasion`: character stat label; differs in th.
- `Evade`: character stat label; differs in it, zht, zhc.
- `Magic`: menu command in FF1/FF2, magic stat in FF4/FF5; differs in ja, ko, zht, zhc, th.
- `Airship`: vehicle name; differs in fr, es, pt.
- `Chocobo`: vehicle name; differs in th.
- `Enemy HP Display`: HP/MP abbreviation: de KP vs LP, es PV vs VIT, ru HP vs ОЗ.
- `Enemy HP: {0}`: HP/MP abbreviation: de KP vs LP, es PV vs VIT, ru HP vs ОЗ.
- `{0} for party HP. {1} to cancel.`: HP/MP abbreviation: de KP vs LP, es PV vs VIT, ru HP vs ОЗ.
- `{0}: Recovered {1} HP`: HP/MP abbreviation: de KP vs LP, es PV vs VIT, ru HP vs ОЗ.
- `Airship Landing`: vehicle name; differs in fr, it, es, th, pt.
- `Airship Landings`: vehicle name; differs in fr, it, es, th, pt.
- `Status`: game menu command label; differs in fr, es, ru.

#### FF5: 39

- `No unlocked jobs can equip`: job/class term; differs in ja, fr, it, de, es, ko, zht, zhc, ru, th.
- `Enemy HP Display`: HP/MP abbreviation: it HP vs PV, de HP vs LP, es HP vs VIT, ru HP vs ОЗ, pt HP vs PV.
- `Enemy HP: {0}`: HP/MP abbreviation: it HP vs PV, de HP vs LP, es HP vs VIT, ru HP vs ОЗ, pt HP vs PV.
- `ship`: vehicle name; differs in fr.
- `airship`: vehicle name; differs in fr, es, th, pt.
- `magical armor`: weapon or armour type; differs in fr, it, de, es, ru, th, pt.
- `KO`: status ailment name; differs in fr, th.
- `Silence`: status ailment name; differs in it, de, th.
- `Sleep`: status ailment name; differs in th.
- `Paralysis`: status ailment name; English "Paralyze" (common "Paralysis").
- `Blind`: status ailment name; English "Darkness" (common "Blind").
- `Poison`: status ailment name; differs in zht, zhc.
- `Stone`: status ailment name; English "Petrify" (common "Stone").
- `Confusion`: status ailment name; English "Confuse" (common "Confusion").
- `Slow`: status ailment name; differs in fr, it, es, zht.
- `Stop`: status ailment name; differs in es, ru, pt.
- `Doom`: status ailment name; differs in fr, it, de, ko, th, pt.
- `{0}: Recovered {1} HP`: HP/MP abbreviation: it HP vs PV, de HP vs LP, es HP vs VIT, ru HP vs ОЗ, pt HP vs PV.
- `{0}: Recovered {1} MP`: HP/MP abbreviation: it MP vs PM, es MP vs PM, pt MP vs PM.
- `Knife`: weapon or armour type; differs in ja, ko, zht, zhc, ru, th.
- `Staff`: weapon or armour type; differs in es, pt.
- `{0} for party HP. {1} to cancel.`: HP/MP abbreviation: it HP vs PV, de HP vs LP, es HP vs VIT, ru HP vs ОЗ, pt HP vs PV.
- `Airship Landing`: vehicle name; differs in fr, it, es, ru, th.
- `Airship Landings`: vehicle name; differs in fr, it, es, ru, th.
- `HP`: HP/MP abbreviation: it HP vs PV, de HP vs LP, es HP vs VIT, ru HP vs ОЗ, pt HP vs PV.
- `MP`: HP/MP abbreviation: it MP vs PM, es MP vs PM, pt MP vs PM.
- `Job`: job/class term; differs in fr, it, de, es, ko, pt.
- `Attack`: character stat label; differs in de.
- `Magic`: menu command in FF1/FF2, magic stat in FF4/FF5; differs in ja, ko, zht, zhc, th.
- `Defense`: character stat label; differs in de.
- `Magic Defense`: character stat label; differs in ja, ko, zht, zhc, th.
- `Evasion`: character stat label; differs in ja, it, ko, zht, zhc.
- `Strength`: character stat label; differs in th.
- `Agility`: character stat label; differs in de, th.
- `Stamina`: character stat label; differs in fr, de, es, th, pt.
- `Status`: game menu command label; differs in fr, es.
- `Front Row`: battle row label; differs in it, pt.
- `Back Row`: battle row label; differs in it, pt.
- `{0}: {1} MP damage`: HP/MP abbreviation: it MP vs PM, es MP vs PM, pt MP vs PM.

### Language fills

None. Every chosen entry already had all 12 languages.

### Gaps

None.

### Placeholder mismatches

A translation whose `{n}` placeholders differ from its English. Reported only; the text is unchanged.

None.

### Keys not in FF1, by source

The common entry for these came from the game named. Order tried: FF3, FF2, FF4, FF5.

#### From FF3: 86

- `Beacon destination announcement {0}` (also in FF2, FF4)
- `Auto detail {0}` (also in FF2, FF4)
- `No entities found` (also in FF2)
- `Menu position announcements {0}`
- `Map name not available` (also in FF2)
- `Beacon navigation {0}` (also in FF2, FF4, FF5)
- `Chocobo` (also in FF2, FF4)
- `Unable to detect vehicle state` (also in FF4)
- `Vitality` (also in FF2)
- `Evade` (also in FF4)
- `No character data available` (also in FF2, FF4)
- `{0} leveled up to level {1}` (also in FF4)
- `Vitality +{0}`
- `Intelligence +{0}`
- `Spirit +{0}`
- `{0} {1} job level up to {2}`
- `{0} job level up to {1}`
- `{0} job level up`
- `Failed to dump entity names` (also in FF2)
- `Accuracy {0}`
- `Airship Landing` (also in FF2, FF4, FF5)
- `Airship Landings` (also in FF2, FF4, FF5)
- `Attack {0}`
- `Attack: {0} x {1}`
- `Can equip: {0}` (also in FF4, FF5)
- `Controller Settings` (also in FF5)
- `Defense {0}`
- `Evade: {0}`
- `Evasion {0}`
- `For attacks that hit several times, optionally say the number of hits before the damage, for example 4x120 damage.`
- `How enemy HP is read when targeting: numbers, percentage of maximum, or not at all.`
- `Job Level: {0}`
- `MP LV{0}: {1}`
- `Magic +{0}`
- `Magic Defense {0}`
- `Magic Defense: {0}`
- `Magic Evade: {0}`
- `Magic Evasion {0}`
- `Magic Evasion: {0}%`
- `Magic: {0}`
- `Miscellaneous` (also in FF2, FF4, FF5)
- `No item details available` (also in FF2)
- `Off. Every entity is listed, including unreachable ones.`
- `Off. Every map exit is listed.` (also in FF4)
- `Off. L3 toggles audio beacons and R3 the pathfinding filter; the game does not receive them.`
- `Off. List entries are read without their position.`
- `Off. Only names are read; press I for the description.`
- `Off. Restarting the beacon only pings.` (also in FF4)
- `Off. Stairs and ladders between floors are listed.`
- `Off. The EXP bar fills silently after battle.`
- `Off. The pathfind keys speak turn-by-turn directions.`
- `On. A click plays for each tile you walk.`
- `On. A ping sounds toward the selected destination, and the pathfind keys restart it instead of speaking directions.`
- `On. A rapid tick plays while the EXP bar fills after battle.`
- `On. Descriptions and stats are read automatically for items, spells, equipment and shop goods.`
- `On. Entity cycling lists only entities with a walkable path.`
- `On. Exits leading to the same map are merged into the closest one.`
- `On. L3 and R3 go to the game (auto-dash and encounters); their mod functions move to mod mode.`
- `On. List entries include their position, for example 3 of 12.`
- `On. Restarting the beacon also speaks the destination.` (also in FF4)
- `On. Stairs and ladders between floors are left out of the entity list.`
- `Onion Knight {0}: unnamed`
- `Onion Knight {0}: {1}`
- `Spirit: {0}`
- `Stealable Items` (also in FF2, FF5)
- `Stick click normalization {0}` (also in FF2, FF4)
- `Vitality: {0}`
- `Volume of the EXP counter tick, 0 to 100 percent.`
- `Volume of the audio beacon ping, 0 to 100 percent.`
- `Volume of the directional wall tones, 0 to 100 percent.`
- `Volume of the footstep click, 0 to 100 percent.`
- `Volume of the wall bump sound, 0 to 100 percent.`
- `magical armor` (also in FF5)
- `{0} for Gil. {1} for location. Right stick to teleport. {2} to cancel.` (also in FF2)
- `{0} for active character HP. {1} to cancel.`
- `{0} step` (also in FF4, FF5)
- `{0} steps` (also in FF4, FF5)
- `{0}, Job Level {1}`
- `{0}, Job Level {1}. Equipped.`
- `{0}, {1} of {2}`
- `Learn` (also in FF2)
- `Could not determine current map`
- `Entity data already exists for this map`
- `No untranslated names for this map`
- `Dumped {0} names for {1}`
- `Submarine` (also in FF2, FF4)

#### From FF2: 48

- `Error getting entity info`
- `Error cycling entities`
- `Unable to pathfind`
- `No path to {0}`
- `Pathfinding failed`
- `Unable to get player position`
- `Failed to add waypoint`
- `No data`
- `No description available` (also in FF5)
- `No details`
- `Not available here`
- `{0} (true)`
- `{0}. Details`
- `{0}. Page changed`
- `Any character can equip this`
- `Ask`
- `Axe` (also in FF5)
- `Bow` (also in FF5)
- `Cancel`
- `Character {0}: unnamed`
- `Character {0}: {1}`
- `Command {0}`
- `Evasion Count`
- `Formation {0}`
- `Intelligence`
- `Knife` (also in FF5)
- `Magic Defense` (also in FF4, FF5)
- `Magic Defense Count`
- `Magic Evasion` (also in FF4)
- `Magic Interference`
- `On chocobo`
- `Opened`
- `Shield` (also in FF5)
- `Spear` (also in FF5)
- `Spirit` (also in FF4)
- `Staff` (also in FF5)
- `Strongest`
- `Sword` (also in FF5)
- `ToLayer`
- `Unarmed`
- `Unopened`
- `{0} +{1} percent`
- `{0} for Gil. {1} for location. {2} for vehicle. {3} to cancel.`
- `{0} lv{1}`
- `{0} percent`
- `{0}, {1} percent`
- `{0}: HP {1} percent`
- `{0}: HP {1}/{2}, MP {3}/{4}`

#### From FF4: 92

- `{1} {2}` (also in FF5)
- `Chests` (also in FF5)
- `All allies` (also in FF5)
- `All enemies` (also in FF5)
- `Unknown Row Type {0}`
- `{0} gil` (also in FF5)
- `User data not available`
- `Rename {0}`
- `Waypoint created: {0}`
- `Renamed to {0}` (also in FF5)
- `Delete {0}?` (also in FF5)
- `Deleted {0}`
- `Canceled`
- `No waypoints to clear` (also in FF5)
- `Clear all {0} waypoints on this map?`
- `Are you sure? This cannot be undone.`
- `less than`
- `greater than`
- `Status Details`
- `Handed`
- `Timers frozen` (also in FF5)
- `Timers resumed` (also in FF5)
- `Error toggling timer freeze` (also in FF5)
- `No active timers` (also in FF5)
- `Error reading timers` (also in FF5)
- `Keyboard input active.`
- `Cleared {0} waypoints` (also in FF5)
- `{0}: {1}`
- `{0}: {1} / {2}`
- `{0}: {1}x {2}`
- `{0}: {1}%`
- `Right-handed`
- `Left-handed`
- `Ambidextrous`
- `{0} {1}`
- `minute` (also in FF5)
- `minutes` (also in FF5)
- `second` (also in FF5)
- `seconds` (also in FF5)
- `Timer: {0}` (also in FF5)
- `Magic Evade`
- `Hovercraft`
- `{0} +{1}`
- `{0} learned {1}` (also in FF5)
- `{0} flees`
- `{0} attacks` (also in FF5)
- `{0} uses {1}`
- `Status {0}` (also in FF5)
- `Choices: {0}` (also in FF5)
- `Path blocked by ledge`
- `Door/Trigger` (also in FF5)
- `Teleport` (also in FF5)
- `Group` (also in FF5)
- `stationary` (also in FF5)
- `wandering` (also in FF5)
- `patrolling` (also in FF5)
- `Enterprise`
- `Symbol`
- `Content`
- `Special Airship`
- `Yellow Chocobo`
- `Black Chocobo`
- `Boko`
- `Magical Armor`
- `Entering character list.`
- `Entering party slot grid.`
- `Party {0}, Position {1}: {2}`
- `Party {0}`
- `not assigned`
- `No party members can equip`
- `Equipment info unavailable`
- `Left and Right change the volume in steps of 5 percent. Enter mutes it, or sets it to 50 percent when muted.`
- `On. Tones sound toward any wall right next to you.`
- `Off. No wall tones.`
- `On. A click plays for every tile you move.`
- `Off. Movement is silent.`
- `On. Choosing a destination starts an audio beacon instead of spoken directions.`
- `Off. Choosing a destination speaks step-by-step directions.`
- `On. L3 and R3 go to the game; their mod functions move to mod mode.`
- `Off. L3 toggles beacon navigation and R3 the pathfinding filter.`
- `On. Menu entries end with their position, such as 3 of 12.`
- `Off. Menu entries are read without their position.`
- `On. Descriptions are read as soon as an item, spell or shop entry is focused.`
- `Off. Press I to hear the description of the focused entry.`
- `On. Entity lists skip anything you cannot walk to.`
- `Off. Entity lists include unreachable entities.`
- `On. Exits leading to the same place are merged into the closest one.`
- `On. Layer transition points are hidden from entity lists.`
- `Off. Layer transition points are listed.`
- `On. A beep ticks while the EXP bar fills after battle.`
- `Off. The EXP bar fills silently.`
- `How enemy HP is read while targeting: as numbers, as a percentage, or not at all.`

#### From FF5: 111

- `Landing Pings`
- `Auto Detail on`
- `Auto Detail off`
- `Landing Ping Volume`
- `Not in field`
- `and {0} more`
- `target {0} {1}`
- `landing spot`
- `Enemy Letters`
- `Enemy Letters on`
- `Enemy Letters off`
- `Unavailable during dialogue`
- `: HP: {0}/{1}`
- `Unavailable outside of battle`
- `{0}, status information not available`
- `EXP counter {0}`
- `Running`
- `wind drake`
- `black chocobo`
- `Stole {0}`
- `Stole {0} x{1}`
- `Stole item`
- `Job level up!`
- `and leveled up to level {0}`
- `Clear all {0} waypoints?`
- `Are you sure?`
- `Keyboard input active`
- `Cannot determine current map`
- `Error dumping entities`
- `Landing Pings {0}`
- `Blink`
- `Haste`
- `Protect`
- `Regen`
- `Old`
- `Mini`
- `Toad`
- `Float`
- `Berserk`
- `Shell`
- `Reflect`
- `{0} defends`
- `{0} uses item`
- `{0}: Job level up!`
- `{0}: Level up!`
- `Page {0} of {1}`
- `here`
- `of`
- `Stick Click Normalization on`
- `Stick Click Normalization off`
- `Are you absolutely sure?`
- `Rename waypoint`
- `Rename failed`
- `Rename cancelled`
- `Added {0} as {1}`
- `Removed {0}`
- `Waypoint creation cancelled`
- `Delete all {0} waypoints?`
- `{0} for battle results. {1} to cancel.`
- `Knight Sword`
- `Katana`
- `Hammer`
- `Rod`
- `Boomerang`
- `Harp`
- `Bell`
- `Whip`
- `Flail`
- `Dumped {0} entities for {1}`
- `{0} new entities for {1}`
- `No new entities for {0}`
- `No Japanese entities found on {0}`
- `Low flying`
- `Special movement`
- `Opened Treasure Chest`
- `Unopened Treasure Chest`
- `{0} exits`
- `EXP`
- `Next`
- `Job Level`
- `ABP`
- `Abilities`
- `Jobs`
- `Character Info`
- `Vitals`
- `Attributes`
- `Combat Stats`
- `Progression`
- `Commands`
- `Time {0}`
- `Gil`
- `Plays a tone toward any wall right next to you, so you can feel out corridors and doorways.`
- `Plays a click for each tile you move on foot.`
- `Pings the selected entity or waypoint, panned toward it and faster as you get closer. A lower pitch means there is no path. While on, backslash and P restart the beacon instead of reading directions.`
- `While riding a vehicle, pings the directions in which you can land or get off.`
- `When you restart the audio beacon, also reads the directions to its destination.`
- `Adds the position in the list to menu entries, for example 3 of 12.`
- `Reads descriptions automatically as you move through items, abilities, equipment and shops. When off, press I for details.`
- `Volume of the sound played when you walk into a wall.`
- `Volume of the footstep clicks.`
- `Volume of the wall tones.`
- `Volume of the audio beacon pings.`
- `Volume of the landing pings.`
- `Entity cycling skips anything you cannot currently walk to.`
- `Exits that lead to the same place are merged into the closest one.`
- `Hides layer transitions, the spots that move you between levels of the same map, from entity cycling.`
- `When on, stick clicks go to the game for walk or run and encounters, and their mod functions move to mod mode. When off, the right stick click toggles the pathfinding filter and the left stick click toggles audio beacons.`
- `Plays a ticking sound while experience counts up on the battle results screen.`
- `Volume of the experience counter ticking.`
- `How enemy HP is read when you target an enemy: as numbers, as a percentage, or not at all.`
- `Adds a letter such as A or B to enemies that share a name, so you can tell them apart.`

<!-- END GENERATED -->
