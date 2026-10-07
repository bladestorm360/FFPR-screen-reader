# Mod text merge

How the five old `mod_text.json` files became one shared file plus per-game overrides.

Written 2026-10-05 for Phase 0 of `../UNIFIED_MOD_PLAN.md`.

## Files

- `data/common/mod_text.json`: shared mod UI text. All 850 keys of the five mods, plus new keys.
- `data/ff1/mod_text.json` to `data/ff5/mod_text.json`: per-game overrides. Game terms only.
- `tools/merge_modtext.py`: makes all of the above from the old repos.
  It also rewrites the generated report at the end of this file.
- `tools/modtext_fixes.py`: corrections to each game's own text, and new keys. The merge applies them last,
  so they survive a re-merge. See "Official-text fixes" below.
- `tools/official_text.py`: reads a game's message tables in all 12 languages (`find`, `id`, `dump`).

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
- Both stop if a row of `tools/modtext_fixes.py` no longer matches what the merge produced
  (an old mod changed). Update or remove that row.
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

**HP and MP.** The abbreviation is a game term. The merge kept each old mod's abbreviation:
KP, PV and HP in FF2 to FF4, and HP and MP in FF5.
The games' own tables say otherwise: all five use the same abbreviations (checked 2026-10-06).

| | fr | it | de | es | ru | pt |
|---|---|---|---|---|---|---|
| HP | PV | PV | LP | VIT | ОЗ | PV |
| MP | PM | PM | MP | PM | ОМ | PM |

`tools/modtext_fixes.py` applies this table to every key whose English contains HP or MP.
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
  Only the wording differs.
- `Low Flying`, `Walking`, `Dashing`, `Unique`, `Vehicle {0}`: mod descriptions of movement, not game labels.
- `Treasure Chest`, `Save Point`, `Map Exit` and the other entity categories: mod labels.
- `Game Over`, `Bestiary. Encountered: {0} of {1}`, `Formation view`, bestiary section labels: shared extras text.
- `Can equip: {0}`, `Status {0}`, `{0} learned {1}`: sentences without a game term.

## Official-text fixes (2026-10-06)

The merge kept what each old mod shipped. On 2026-10-06 the game terms were checked against each game's
own message tables, in all 12 languages, and corrected where they differ. The generated report below
lists every change: file, key, language, old -> new, and the game table it comes from.

### Sources

- `system` table of each game's `message_assets_all_*.bundle`, read with `tools/official_text.py`.
  Status names follow the game's own mapping: the `condition` master's `mes_id_name`, or else the
  bestiary label `MONSTER_BOOK_CONDITION_TYPE_n` in `message_conclusion`.
  For example, FF5's Poison is `MSG_SYSTEM_414` (猛毒), not the element "Poison" `MSG_SYSTEM_334` (毒).
- Where a game has no label for a vehicle, the word its dialogue (`story_mes`) uses for it:
  the airship in FF3 and FF4, the ship in FF3 (French).
- FF5's French `Slow` is "Somni" in the game too. It was right and is unchanged.

### Rules

- Only the game term changes. Mod wording around it, placeholders and suffixes stay.
  Stat lines such as `Strength: {0}` change only the label part.
- Each game gets its own word. A fix to the game a common entry came from goes in the common file.
  Other games get an override. An override that now equals the common entry is removed.
- Lower-case keys (`airship`, `ship`, `chocobo`, `black chocobo`, `wind drake`, `hovercraft`) use the game
  word in lower case in en, fr, it, es, pt and ru.
- Full-width and half-width forms (ＨＰ, HP) and case-only differences are not changes.
- English changes too where the game's English differs: `Item` is "Items", `Config` is "Configuration",
  `Interruption` is "Quick Save", FF2's `Words` is "Key Terms", `Blind` is "Darkness" in FF1 and FF4,
  `Confusion` is "Confuse", and `Paralysis` and `Stone` are "Paralyze" and "Petrify" in FF3 and FF4.
- Hand edits, marked in the report: the airship word inside `On airship`, `On ship` and `Airship Landing(s)`;
  and article or verb agreement after the HP change (es "la VIT", it "i PV", pt "os PV", ru plural verbs).

### New keys

- `hovercraft` (FF4 file): FF4's own word, `MSG_SYSTEM_288`, in lower case as FF4's dialogue writes it.
- `yellow chocobo` (FF4 file): FF4 has no name for the yellow chocobo. Composed from FF4's `Chocobo`
  (`MSG_SYSTEM_343`) and the pattern of its `Black Chocobo` (`MSG_SYSTEM_285`). Common `Yellow Chocobo`
  was aligned to the same pattern.
- `No data available` (common): mod UI text, not game text. Translated by hand in the style of the
  neighbouring "No ... available" keys.

### Game text not adopted

- FF1 `Confusion`: es "Confu" is the Confuse spell name cut short; th is an unrelated string ("sort weapons
  first"). Both kept.
- `Save` in zht: the game's text is the placeholder "(save)" in FF1, FF2, FF4 and FF5. Kept "儲存".
- FF5 `Remove All` in es: the game says "Quitar", the same as Remove. Kept "Quitar todo".

### Not verified

No matching game label, or the label's context differs. These keep the old mods' text:

- `Back`: also the shop's exit command, whose game label is "Exit".
- `Equip`, `Equipment`: the game's main-menu label is "Equip"; the equip-screen and shop commands may use others.
- `Magic`: one key for the menu command and the magic stat.
- `Job`, `Job: {0}`, `No unlocked jobs can equip`: the games show only the plural menu label "Jobs"; FF1 says "Class".
- `Next Level`, `Next Level: {0}`: the game label is "Next Level in", with a trailing colon in es, pt, zht and zhc.
- `Front Row`, `Back Row`: no row label in any game's tables.
- `Accessory`, `Accessory 2` in FF1 to FF4: the game's fifth slot is "Arms"; the slot mapping is not confirmed.
- FF2: `Ability`, `Vitality`, `Learn`, `Unarmed`, and `Blind` and `Stone` (FF2 does not name these ailments).
- FF3: `Forget`, `magical armor`. FF5: `Knife`, `Staff` (no weapon-type labels), `magical armor`.
- Core fallback status names that every game names itself: `Critical`, `Aging`, `Venom`, `Curse`, `Gradual Petrify`.
- Stat-line keys the code does not use, such as `Attack {0}` and `Spirit +{0}`.

## Notes for the port

- **The common file holds every key, including game terms that only one game uses.**
  Examples: FF1 `Light Warrior {0}`, FF3 `Onion Knight {0}`, FF4 `Enterprise`, FF5 `ABP` and `Haste`.
  A game that never asks for the key never sees it.
- **Watch for a game-term key a game did not have before.** Shared core code can make a game ask for one.
  The common entry then carries another game's term.
  - Example: FF5's old mod has `airship` but not `Airship`. Common `Airship` is FF1's, whose pt is "Helibarco".
    FF5's own word is "Navio Voador", so FF5 now has an override.
  - Fix: add a row to `tools/modtext_fixes.py`, taken from that game's own text, and re-run the merge.
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

- Source keys: FF1 513, FF2 391, FF3 400, FF4 338, FF5 410, FF6 300.
- Keys in the common file: 954.
- Common entries taken from: FF1 513, FF3 86, FF2 48, FF4 92, FF5 111, FF6 102.
- Overrides from the merge: FF1 0, FF2 49, FF3 42, FF4 26, FF5 39, FF6 23.
- Official-text fixes: 653 rows, 4 new keys.
- Overrides after the fixes: FF1 2, FF2 53, FF3 53, FF4 56, FF5 55, FF6 23.
- Languages filled from another mod: 0.
- Missing-language gaps after the merge: 0.
- Placeholder mismatches (reported, not fixed): 0.

### Overrides from the merge

Before the official-text fixes below.

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

#### FF6: 23

- `Enemy HP Display`: HP/MP abbreviation: de KP vs LP, es PV vs VIT, ru HP vs ОЗ.
- `Airship Landing`: vehicle name; differs in ja, fr, it, es, ko, ru, th, pt.
- `Airship Landings`: vehicle name; differs in ja, fr, it, es, ko, ru, th, pt.
- `Enemy HP: {0}`: HP/MP abbreviation: de KP vs LP, es PV vs VIT, ru HP vs ОЗ.
- `HP: {0} / {1}`: HP/MP abbreviation: de KP vs LP, es PV vs VIT, ru HP vs ОЗ.
- `Next Level: {0}`: character stat label; differs in ja, fr, it, de, ko, zht, zhc, ru, th.
- `HP`: HP/MP abbreviation: de KP vs LP, es PV vs VIT, ru HP vs ОЗ.
- `Strength`: character stat label; differs in th.
- `Agility`: character stat label; differs in de, ko, th.
- `Stamina`: character stat label; differs in fr, de, es, zht, zhc, th.
- `Magic`: menu command in FF1/FF2, magic stat in FF4/FF5; differs in ja, ko, zht, zhc.
- `Attack`: character stat label; differs in ja, de, ko, zht, zhc, th.
- `Defense`: character stat label; differs in ja, de, ko, zht, zhc, th.
- `Evasion`: character stat label; differs in ja, it, ko, zht, zhc, th.
- `Magic Defense`: character stat label; differs in de.
- `Magic Evasion`: character stat label; differs in it, de, zht, zhc.
- `Strength: {0}`: character stat label; differs in th.
- `Agility: {0}`: character stat label; differs in de, ko, th.
- `Stamina: {0}`: character stat label; differs in fr, de, es, zht, zhc, th.
- `Defense: {0}`: character stat label; differs in ja, de, ko, zht, zhc, th.
- `Poison`: status ailment name; differs in zht, zhc.
- `Front Row`: battle row label; differs in fr, it, de, es, pt.
- `Back Row`: battle row label; differs in fr, it, de, es.

### Official-text fixes

Applied by `tools/modtext_fixes.py` after the merge. Each line: language, old -> new, then the game table it comes from.

#### common: 122 rows in 46 keys, 2 new keys

- `MP`: ru "MP" -> "ОМ". Source: FF1-FF5 system HP/MP labels (FF1 MSG_SYSTEM_053/283, FF5 MSG_SYSTEM_142/143).
- `Item`: en "Item" -> "Items". Source: FF1 system MSG_SYSTEM_042.
- `Config`: en "Config" -> "Configuration". Source: FF1 system MSG_SYSTEM_047.
- `Interruption`: en "Interruption" -> "Quick Save". Source: FF1 system MSG_SYSTEM_048.
- `Remove`: fr "Retirer" -> "Enlever"; de "Entfernen" -> "Ablegen"; zht "移除" -> "卸除"; zhc "移除" -> "卸下"; ru "Убрать" -> "Снять"; th "นำออก" -> "ถอด". Source: FF1 system MSG_SYSTEM_067.
- `chocobo`: th "โชโกโบ" -> "โชโคโบะ". Source: FF2 system MSG_SYSTEM_255.
- `{0}: {1} MP damage`: ru "{0}: {1} урона MP" -> "{0}: {1} урона ОМ". Source: FF1-FF5 system HP/MP labels (FF1 MSG_SYSTEM_053/283, FF5 MSG_SYSTEM_142/143).
- `{0}: Recovered {1} MP`: ru "{0}: восстановлено {1} MP" -> "{0}: восстановлено {1} ОМ". Source: FF1-FF5 system HP/MP labels (FF1 MSG_SYSTEM_053/283, FF5 MSG_SYSTEM_142/143).
- `MP {0}`: ru "MP {0}" -> "ОМ {0}". Source: FF1-FF5 system HP/MP labels (FF1 MSG_SYSTEM_053/283, FF5 MSG_SYSTEM_142/143).
- `MP {0}/{1}`: ru "MP {0}/{1}" -> "ОМ {0}/{1}". Source: FF1-FF5 system HP/MP labels (FF1 MSG_SYSTEM_053/283, FF5 MSG_SYSTEM_142/143).
- `MP: {0}/{1}`: ru "MP: {0}/{1}" -> "ОМ: {0}/{1}". Source: FF1-FF5 system HP/MP labels (FF1 MSG_SYSTEM_053/283, FF5 MSG_SYSTEM_142/143).
- `Blind`: en "Blind" -> "Darkness". Source: FF1 system MSG_SYSTEM_035.
- `Poison`: ko "독" -> "중독"; ru "Яд" -> "Отравление". Source: FF1 system MSG_SYSTEM_034.
- `Confusion`: en "Confusion" -> "Confuse". Source: FF1 system MSG_SYSTEM_336.
- `Slow`: fr "Lenteur" -> "Somni"; it "Lentezza" -> "Adagio"; es "Lentitud" -> "Freno"; zht "減速" -> "緩慢"; th "ช้า" -> "สโลว์". Source: FF1 system MSG_MAGIC_NAME_40.
- `Stop`: es "Stop" -> "Paro"; ru "Стоп" -> "Остановка"; pt "Parar" -> "Catatonia". Source: FF4 system MSG_SYSTEM_129.
- `Chocobo`: th "โชโกโบ" -> "โชโคโบะ". Source: FF3 system MSG_SYSTEM_522.
- `Airship Landing`: th "จุดลงจอดเรือบิน" -> "จุดลงจอดเรือเหาะ"; pt "Pouso da aeronave" -> "Pouso do navio voador". Source: FF3 story_mes dialogue word for 飛空艇 (airship word only).
- `Airship Landings`: th "จุดลงจอดเรือบิน" -> "จุดลงจอดเรือเหาะ"; pt "Pousos da aeronave" -> "Pousos do navio voador". Source: FF3 story_mes dialogue word for 飛空艇 (airship word only).
- `How enemy HP is read when targeting: numbers, percentage of maximum, or not at all.`: de "Wie Feind-KP beim Anvisieren gelesen werden: als Zahlen, als Prozent des Maximums oder gar nicht." -> "Wie Feind-LP beim Anvisieren gelesen werden: als Zahlen, als Prozent des Maximums oder gar nicht." (FF1-FF5 system HP/MP labels (FF1 MSG_SYSTEM_053/283, FF5 MSG_SYSTEM_142/143)); es "Cómo se leen los PV enemigos al apuntar: en números, en porcentaje del máximo o nada." -> "Cómo se lee la VIT enemiga al apuntar: en números, en porcentaje del máximo o nada." (HP/MP labels as above, with article/verb agreement fixed by hand); ru "Как читать HP врагов при выборе цели: числами, процентом от максимума или не читать вовсе." -> "Как читать ОЗ врагов при выборе цели: числами, процентом от максимума или не читать вовсе." (FF1-FF5 system HP/MP labels (FF1 MSG_SYSTEM_053/283, FF5 MSG_SYSTEM_142/143)).
- `MP LV{0}: {1}`: ru "MP ур. {0}: {1}" -> "ОМ ур. {0}: {1}". Source: FF1-FF5 system HP/MP labels (FF1 MSG_SYSTEM_053/283, FF5 MSG_SYSTEM_142/143).
- `Magic Defense: {0}`: ko "마법 방어: {0}" -> "마법 방어력: {0}"; th "ป้องกันเวท: {0}" -> "การป้องกันเวทมนตร์: {0}". Source: FF3 system MSG_SYSTEM_297 (label part).
- `Magic Evasion: {0}%`: it "Elusione magica: {0}%" -> "Destrezza magica: {0}%"; de "Magie-Ausweichrate: {0} %" -> "Magieausweichen: {0} %"; zhc "魔法闪避率：{0}%" -> "魔法回避率：{0}%"; th "อัตราหลบหลีกเวท: {0}%" -> "การหลีกหนีเวทมนตร์: {0}%". Source: FF3 system MSG_SYSTEM_298 (label part).
- `Spirit: {0}`: de "Geist: {0}" -> "Animus: {0}"; ko "정신: {0}" -> "정신력: {0}"; pt "Espírito: {0}" -> "Vigor: {0}". Source: FF3 system MSG_SYSTEM_303 (label part).
- `{0} for active character HP. {1} to cancel.`: de "{0}: KP der aktiven Figur. {1}: Abbrechen." -> "{0}: LP der aktiven Figur. {1}: Abbrechen." (FF1-FF5 system HP/MP labels (FF1 MSG_SYSTEM_053/283, FF5 MSG_SYSTEM_142/143)); es "{0} para los PV del personaje activo. {1} para cancelar." -> "{0} para la VIT del personaje activo. {1} para cancelar." (HP/MP labels as above, with article/verb agreement fixed by hand); ru "{0} — HP активного персонажа. {1} — отмена." -> "{0} — ОЗ активного персонажа. {1} — отмена." (FF1-FF5 system HP/MP labels (FF1 MSG_SYSTEM_053/283, FF5 MSG_SYSTEM_142/143)).
- `Learn`: ja "覚える" -> "もつ"; it "Impara" -> "Apprendi"; ko "배우기" -> "소지". Source: FF3 system MSG_SYSTEM_276.
- `Axe`: it "Ascia" -> "Asce". Source: FF2 system MSG_SYSTEM_102.
- `Bow`: it "Arco" -> "Archi"; th "ธนู" -> "คันธนู". Source: FF2 system MSG_SYSTEM_104.
- `Knife`: ja "短剣" -> "ナイフ"; it "Pugnale" -> "Pugnali"; de "Dolch" -> "Messer"; es "Daga" -> "Cuchillo"; ko "단검" -> "나이프"; zht "短劍" -> "匕首"; zhc "短剑" -> "匕首"; ru "Кинжал" -> "Нож"; th "มีดสั้น" -> "มีด"; pt "Adaga" -> "Faca". Source: FF2 system MSG_SYSTEM_099.
- `Magic Defense`: fr "Défense magique" -> "Psyché"; ko "마법 방어" -> "마법 방어력"; th "ป้องกันเวทมนตร์" -> "การป้องกันเวทมนตร์". Source: FF2 system MSG_SYSTEM_085.
- `Shield`: it "Scudo" -> "Scudi". Source: FF2 system MSG_SYSTEM_105.
- `Spear`: it "Lancia" -> "Lance"; ru "Копьё" -> "Копье". Source: FF2 system MSG_SYSTEM_103.
- `Spirit`: fr "Esprit" -> "Sagesse"; de "Geist" -> "Wille"; ko "정신" -> "정신력"; th "จิตใจ" -> "จิตวิญญาณ"; pt "Espírito" -> "Vigor". Source: FF2 system MSG_SYSTEM_088.
- `Staff`: it "Bastone" -> "Aste"; es "Báculo" -> "Vara". Source: FF2 system MSG_SYSTEM_101.
- `Sword`: it "Spada" -> "Spade". Source: FF2 system MSG_SYSTEM_100.
- `{0}: HP {1} percent`: de "{0}: KP {1} Prozent" -> "{0}: LP {1} Prozent"; es "{0}: PV {1} por ciento" -> "{0}: VIT {1} por ciento"; ru "{0}: HP {1}%" -> "{0}: ОЗ {1}%". Source: FF1-FF5 system HP/MP labels (FF1 MSG_SYSTEM_053/283, FF5 MSG_SYSTEM_142/143).
- `{0}: HP {1}/{2}, MP {3}/{4}`: de "{0}: KP {1}/{2}, MP {3}/{4}" -> "{0}: LP {1}/{2}, MP {3}/{4}"; es "{0}: PV {1}/{2}, PM {3}/{4}" -> "{0}: VIT {1}/{2}, PM {3}/{4}"; ru "{0}: HP {1}/{2}, MP {3}/{4}" -> "{0}: ОЗ {1}/{2}, ОМ {3}/{4}". Source: FF1-FF5 system HP/MP labels (FF1 MSG_SYSTEM_053/283, FF5 MSG_SYSTEM_142/143).
- `Hovercraft`: ja "ホバークラフト" -> "ホバー船"; it "Hovercraft" -> "Eliscafo"; ru "Судно на воздушной подушке" -> "Автолет"; th "โฮเวอร์คราฟต์" -> "เรือโฮเวอร์คราฟต์"; pt "Hovercraft" -> "Aerobote". Source: FF4 system MSG_SYSTEM_288.
- `Enterprise`: fr "Enterprise" -> "Entreprise"; ru "Энтерпрайз" -> "«Эскапада»"; th "เอนเตอร์ไพรส์" -> "เรือเหาะเอ็นเตอร์ไพรส์". Source: FF4 system MSG_SYSTEM_286.
- `Yellow Chocobo`: zht "黃色陸行鳥" -> "黃陸行鳥"; zhc "黄色陆行鸟" -> "黄陆行鸟"; ru "Жёлтый чокобо" -> "Желтый чокобо"; th "โชโคโบสีเหลือง" -> "โชโคโบะเหลือง". Source: composed: FF4 MSG_SYSTEM_343 Chocobo + MSG_SYSTEM_285 pattern.
- `Black Chocobo`: zht "黑色陸行鳥" -> "黑陸行鳥"; zhc "黑色陆行鸟" -> "黑陆行鸟"; ru "Чёрный чокобо" -> "Черный чокобо"; th "โชโคโบสีดำ" -> "โชโคโบะดำ". Source: FF4 system MSG_SYSTEM_285.
- `How enemy HP is read while targeting: as numbers, as a percentage, or not at all.`: de "Wie Gegner-KP beim Zielen vorgelesen werden: als Zahlen, als Prozent oder gar nicht." -> "Wie Gegner-LP beim Zielen vorgelesen werden: als Zahlen, als Prozent oder gar nicht." (FF1-FF5 system HP/MP labels (FF1 MSG_SYSTEM_053/283, FF5 MSG_SYSTEM_142/143)); es "Cómo se leen los PV enemigos al apuntar: en números, en porcentaje o nada." -> "Cómo se lee la VIT enemiga al apuntar: en números, en porcentaje o nada." (HP/MP labels as above, with article/verb agreement fixed by hand); ru "Как читается HP врагов при выборе цели: числом, в процентах или никак." -> "Как читаются ОЗ врагов при выборе цели: числом, в процентах или никак." (HP/MP labels as above, with article/verb agreement fixed by hand).
- `: HP: {0}/{1}`: it ": HP {0}/{1}" -> ": PV {0}/{1}"; de ": HP {0}/{1}" -> ": LP {0}/{1}"; es ": HP {0}/{1}" -> ": VIT {0}/{1}"; ru ": HP {0}/{1}" -> ": ОЗ {0}/{1}"; pt ": HP {0}/{1}" -> ": PV {0}/{1}". Source: FF1-FF5 system HP/MP labels (FF1 MSG_SYSTEM_053/283, FF5 MSG_SYSTEM_142/143).
- `wind drake`: fr "dragon du vent" -> "dragon"; de "Winddrache" -> "Drache"; es "dragón del viento" -> "guiverno"; pt "dragão do vento" -> "dragonete do vento". Source: FF5 system MSG_SYSTEM_392.
- `black chocobo`: zht "黑色陸行鳥" -> "黑陸行鳥"; zhc "黑色陆行鸟" -> "黑陆行鸟"; ru "чёрный чокобо" -> "черный чокобо"; th "โชโกโบดำ" -> "โชโคโบะดำ". Source: FF5 system MSG_SYSTEM_391.
- `How enemy HP is read when you target an enemy: as numbers, as a percentage, or not at all.`: it "Come vengono letti gli HP del nemico quando lo prendi di mira: in numeri, in percentuale o per niente." -> "Come vengono letti i PV del nemico quando lo prendi di mira: in numeri, in percentuale o per niente." (HP/MP labels as above, with article/verb agreement fixed by hand); de "Wie die HP eines Gegners beim Anvisieren vorgelesen werden: als Zahlen, als Prozentsatz oder gar nicht." -> "Wie die LP eines Gegners beim Anvisieren vorgelesen werden: als Zahlen, als Prozentsatz oder gar nicht." (FF1-FF5 system HP/MP labels (FF1 MSG_SYSTEM_053/283, FF5 MSG_SYSTEM_142/143)); es "Cómo se leen los HP del enemigo al seleccionarlo: en números, en porcentaje o nada." -> "Cómo se lee la VIT del enemigo al seleccionarlo: en números, en porcentaje o nada." (HP/MP labels as above, with article/verb agreement fixed by hand); ru "Как зачитывается HP врага при выборе цели: числами, в процентах или никак." -> "Как зачитываются ОЗ врага при выборе цели: числами, в процентах или никак." (HP/MP labels as above, with article/verb agreement fixed by hand); pt "Como o HP do inimigo é lido ao selecioná-lo: em números, em porcentagem ou não é lido." -> "Como os PV do inimigo são lidos ao selecioná-lo: em números, em porcentagem ou não são lidos." (HP/MP labels as above, with article/verb agreement fixed by hand).
- `No data available`: new key. Source: mod UI text, translated by hand (not game text) in the style of the neighbouring No ... available keys.
- `Old screen reader still installed: {0}. Remove it from the Mods folder.`: new key. Source: mod UI text (native mod's startup warning), translated by hand (not game text).

#### ff1: 4 rows in 2 keys

- `Airship Landing`: th "จุดลงจอดเรือบิน" -> "จุดลงจอดเรือเหาะ"; pt "Pouso da aeronave" -> "Pouso do helibarco". Source: FF1 system MSG_SYSTEM_230 (airship word only).
- `Airship Landings`: th "จุดลงจอดเรือบิน" -> "จุดลงจอดเรือเหาะ"; pt "Pousos da aeronave" -> "Pousos do helibarco". Source: FF1 system MSG_SYSTEM_230 (airship word only).

#### ff2: 130 rows in 56 keys

- `Enemy HP Display`: override removed (now equal to the common entry).
- `Enemy HP: {0}`: override removed (now equal to the common entry).
- `Item`: override removed (now equal to the common entry).
- `Load`: override removed (now equal to the common entry).
- `Save`: override removed (now equal to the common entry).
- `Sort`: override removed (now equal to the common entry).
- `Status`: override removed (now equal to the common entry).
- `Accuracy`: override removed (now equal to the common entry).
- `Agility`: override removed (now equal to the common entry).
- `Buy`: override removed (now equal to the common entry).
- `Chocobo`: override removed (now equal to the common entry).
- `HP`: override removed (now equal to the common entry).
- `Sell`: override removed (now equal to the common entry).
- `Ship`: override removed (now equal to the common entry).
- `Use`: override removed (now equal to the common entry).
- `{0}: HP {1}/{2}`: override removed (now equal to the common entry).
- `Config`: en "Config" -> "Configuration"; ja "設定" -> "コンフィグ"; fr "Configuration" -> "Réglages"; es "Configuración" -> "Preferencias"; zhc "设置" -> "设定"; ru "Настройки" -> "Конфигурация"; th "ตั้งค่า" -> "การตั้งค่า"; pt "Configurações" -> "Configuração". Source: FF2 system MSG_SYSTEM_050.
- `Interruption`: en "Interruption" -> "Quick Save"; ja "中断" -> "ちゅうだん"; fr "Interruption" -> "Sauvegarde rapide"; it "Interruzione" -> "Salvataggio rapido"; de "Unterbrechung" -> "Schnellspeichern"; es "Interrupción" -> "Guardado rápido"; ko "중단" -> "중단 저장"; ru "Прерывание" -> "Быстрое сохранение"; th "การขัดจังหวะ" -> "บันทึกด่วน"; pt "Interrupção" -> "Salvamento rápido". Source: FF2 system MSG_SYSTEM_051.
- `Words`: en "Words" -> "Key Terms"; fr "Mots" -> "Mots clés"; it "Parole" -> "Appunti"; es "Palabras" -> "Claves"; ko "단어" -> "용어"; zht "詞語" -> "關鍵字"; zhc "词语" -> "关键词"; ru "Слова" -> "Важные понятия"; th "คำศัพท์" -> "คำศัพท์หลัก"; pt "Palavras" -> "Termos-chave". Source: FF2 system MSG_SYSTEM_049.
- `{0} for party HP. {1} to cancel.`: de "{0} für Gruppen-KP. {1} zum Abbrechen." -> "{0} für Gruppen-LP. {1} zum Abbrechen." (FF1-FF5 system HP/MP labels (FF1 MSG_SYSTEM_053/283, FF5 MSG_SYSTEM_142/143)); es "{0} para los PV del grupo. {1} para cancelar." -> "{0} para la VIT del grupo. {1} para cancelar." (HP/MP labels as above, with article/verb agreement fixed by hand); ru "{0} — HP группы. {1} — отмена." -> "{0} — ОЗ группы. {1} — отмена." (FF1-FF5 system HP/MP labels (FF1 MSG_SYSTEM_053/283, FF5 MSG_SYSTEM_142/143)).
- `Controls how enemy HP appears in battle: numeric value, percentage of max, or hidden.`: de "Legt fest, wie die Gegner-KP im Kampf erscheinen: Zahlenwert, Prozent des Maximums oder verborgen." -> "Legt fest, wie die Gegner-LP im Kampf erscheinen: Zahlenwert, Prozent des Maximums oder verborgen." (FF1-FF5 system HP/MP labels (FF1 MSG_SYSTEM_053/283, FF5 MSG_SYSTEM_142/143)); es "Controla cómo aparecen los PV enemigos en combate: valor numérico, porcentaje del máximo u oculto." -> "Controla cómo aparece la VIT enemiga en combate: valor numérico, porcentaje del máximo u oculta." (HP/MP labels as above, with article/verb agreement fixed by hand); ru "Определяет, как HP врага отображается в бою: числом, процентом от максимума или скрыто." -> "Определяет, как ОЗ врага отображаются в бою: числом, процентом от максимума или скрыто." (HP/MP labels as above, with article/verb agreement fixed by hand).
- `Airship`: fr "Aéronef" -> "Vaisseau"; es "Aeronave" -> "Barco volador". Source: FF2 system MSG_SYSTEM_202.
- `Airship Landing`: fr "Atterrissage d'aéronef" -> "Atterrissage de vaisseau"; es "Aterrizaje de aeronave" -> "Aterrizaje de barco volador". Source: FF2 system MSG_SYSTEM_202 / MSG_SYSTEM_200 (vehicle word only).
- `Airship Landings`: fr "Atterrissages d'aéronef" -> "Atterrissages de vaisseau"; es "Aterrizajes de aeronave" -> "Aterrizajes de barco volador". Source: FF2 system MSG_SYSTEM_202 / MSG_SYSTEM_200 (vehicle word only).
- `Attack`: de "Angriff" -> "Attacke"; th "พลังโจมตี" -> "โจมตี". Source: FF2 system MSG_SYSTEM_081.
- `Body`: ko "몸" -> "몸통"; ru "Тело" -> "Торс". Source: FF2 system MSG_SYSTEM_079.
- `Evasion`: ja "回避" -> "回避率"; it "Evasione" -> "Schivata"; ko "회피" -> "회피율"; zht "迴避" -> "閃避率"; zhc "回避" -> "回避率"; th "การหลบหลีก" -> "การหลีกหนี". Source: FF2 system MSG_SYSTEM_084.
- `Intellect`: es "Intelecto" -> "Inteligencia"; ko "지성" -> "지력". Source: FF2 system MSG_SYSTEM_090.
- `Key Items`: fr "Objets rares" -> "Objets clés"; it "Oggetti chiave" -> "Rarità"; de "Schlüsselgegenstände" -> "Wertvolles"; es "Objetos clave" -> "Importantes"; zht "重要物品" -> "貴重物品"; zhc "重要物品" -> "贵重物品"; ru "Ключевые предметы" -> "Важные предметы"; th "ไอเทมสำคัญ" -> "ไอเท็มหลัก". Source: FF2 system MSG_SYSTEM_059.
- `On airship`: fr "À bord de l'aéronef" -> "À bord du vaisseau"; es "A bordo de la aeronave" -> "A bordo del barco volador". Source: FF2 system MSG_SYSTEM_202 / MSG_SYSTEM_200 (vehicle word only).
- `On ship`: fr "À bord du navire" -> "À bord du bateau". Source: FF2 system MSG_SYSTEM_202 / MSG_SYSTEM_200 (vehicle word only).
- `Remove`: ja "外す" -> "はずす"; fr "Retirer" -> "Enlever"; zht "卸下" -> "卸除"; th "ถอด" -> "ถอดออก". Source: FF2 system MSG_SYSTEM_072.
- `Remove All`: ja "全部外す" -> "すべてはずす"; fr "Tout retirer" -> "Tout enlever"; de "Alles ablegen" -> "Alle ablegen"; ko "전부 해제" -> "모두 해제"; zht "全部卸下" -> "全部卸除"; ru "Снять всё" -> "Снять все". Source: FF2 system MSG_SYSTEM_073.
- `Stamina`: fr "Endurance" -> "Vigueur"; de "Ausdauer" -> "Konstitution"; es "Aguante" -> "Entereza"; th "ความอึด" -> "ความแข็งแกร่ง"; pt "Vigor" -> "Resistência". Source: FF2 system MSG_SYSTEM_087.
- `Strength`: th "พลัง" -> "พละกำลัง". Source: FF2 system MSG_SYSTEM_086.
- `{0}: Recovered {1} HP`: de "{0}: {1} KP wiederhergestellt" -> "{0}: {1} LP wiederhergestellt" (FF1-FF5 system HP/MP labels (FF1 MSG_SYSTEM_053/283, FF5 MSG_SYSTEM_142/143)); es "{0}: recupera {1} PV" -> "{0}: recupera {1} de VIT" (HP/MP labels as above, with article/verb agreement fixed by hand); ru "{0}: восстановлено {1} HP" -> "{0}: восстановлено {1} ОЗ" (FF1-FF5 system HP/MP labels (FF1 MSG_SYSTEM_053/283, FF5 MSG_SYSTEM_142/143)).
- `Silence`: it "Silenzio" -> "Mutismo"; th "ใบ้" -> "เป็นใบ้". Source: FF2 system MSG_SYSTEM_040.
- `Sleep`: th "หลับ" -> "นอนหลับ". Source: FF2 system MSG_SYSTEM_041.
- `Paralysis`: th "ชา" -> "อัมพาต". Source: FF2 system MSG_SYSTEM_038.
- `Poison`: en "Poison" -> "Venom"; fr "Poison" -> "Venin"; es "Veneno" -> "Ponzoña"; ko "독" -> "중독"; pt "Veneno" -> "Tóxico". Source: FF2 system MSG_SYSTEM_042.
- `Confusion`: de "Konfus" -> "Konfusion"; th "สับสน" -> "ความสับสน". Source: FF2 system MSG_SYSTEM_037.
- `Slow`: fr "Lenteur" -> "Somni"; it "Lentezza" -> "Adagio"; es "Lentitud" -> "Freno"; zht "減速" -> "緩慢". Source: FF2 system MSG_MAGIC_NAME_35.
- `Mini`: es "Mini" -> "Minimalia". Source: FF2 system MSG_SYSTEM_039.
- `Defense`: th "พลังป้องกัน" -> "การป้องกัน". Source: FF2 system MSG_SYSTEM_082.
- `Forget`: zht "遺忘" -> "忘記"; zhc "遗忘" -> "忘记". Source: FF2 system MSG_SYSTEM_069.
- `airship`: pt "helibarco" -> "aeronave". Source: FF2 system MSG_SYSTEM_202.
- `Strength: {0}`: th "ความแข็งแกร่ง: {0}" -> "พละกำลัง: {0}". Source: FF2 system MSG_SYSTEM_086 (label part).
- `Stamina: {0}`: th "พละกำลัง: {0}" -> "ความแข็งแกร่ง: {0}". Source: FF2 system MSG_SYSTEM_087 (label part).
- `Intellect: {0}`: fr "Sagesse : {0}" -> "Intelligence : {0}"; th "ความฉลาด: {0}" -> "สติปัญญา: {0}". Source: FF2 system MSG_SYSTEM_090 (label part).
- `Attack: {0}`: th "พลังโจมตี: {0}" -> "โจมตี: {0}". Source: FF2 system MSG_SYSTEM_081 (label part).
- `Defense: {0}`: th "พลังป้องกัน: {0}" -> "การป้องกัน: {0}". Source: FF2 system MSG_SYSTEM_082 (label part).
- `Evasion: {0}%`: th "การหลบหลีก: {0}%" -> "การหลีกหนี: {0}%". Source: FF2 system MSG_SYSTEM_084 (label part).
- `Strength +{0}`: th "ความแข็งแกร่ง +{0}" -> "พละกำลัง +{0}". Source: FF2 system MSG_SYSTEM_086 (label part).
- `Stamina +{0}`: th "พละกำลัง +{0}" -> "ความแข็งแกร่ง +{0}". Source: FF2 system MSG_SYSTEM_087 (label part).
- `Intellect +{0}`: fr "Sagesse +{0}" -> "Intelligence +{0}"; th "ความฉลาด +{0}" -> "สติปัญญา +{0}". Source: FF2 system MSG_SYSTEM_090 (label part).
- `Evasion +{0}`: th "การหลบหลีก +{0}" -> "การหลีกหนี +{0}". Source: FF2 system MSG_SYSTEM_084 (label part).

#### ff3: 127 rows in 55 keys

- `Enemy HP Display`: override removed (now equal to the common entry).
- `Enemy HP: {0}`: override removed (now equal to the common entry).
- `Ship`: override removed (now equal to the common entry).
- `: HP {0}/{1}`: override removed (now equal to the common entry).
- `Buy`: override removed (now equal to the common entry).
- `HP {0}`: override removed (now equal to the common entry).
- `HP {0}/{1}`: override removed (now equal to the common entry).
- `HP: {0} / {1}`: override removed (now equal to the common entry).
- `Sell`: override removed (now equal to the common entry).
- `Status`: override removed (now equal to the common entry).
- `ship`: override removed (now equal to the common entry).
- `{0}: HP {1}/{2}`: override removed (now equal to the common entry).
- `Use`: override removed (now equal to the common entry).
- `Airship`: es "Aeronave" -> "Barco volador"; th "เรือบิน" -> "เรือเหาะ"; pt "Aeronave" -> "Navio voador". Source: FF3 story_mes (dialogue).
- `Strength`: th "พลัง" -> "พละกำลัง". Source: FF3 system MSG_SYSTEM_299.
- `Defense`: de "Verteidigung" -> "Abwehr"; ko "방어" -> "방어력"; th "ป้องกัน" -> "การป้องกัน". Source: FF3 system MSG_SYSTEM_295.
- `HP +{0}`: de "KP +{0}" -> "LP +{0}"; es "PV +{0}" -> "VIT +{0}"; ru "HP +{0}" -> "ОЗ +{0}". Source: FF1-FF5 system HP/MP labels (FF1 MSG_SYSTEM_053/283, FF5 MSG_SYSTEM_142/143).
- `Strength +{0}`: th "พลัง +{0}" -> "พละกำลัง +{0}". Source: FF3 system MSG_SYSTEM_299 (label part).
- `Agility +{0}`: de "Schnelligkeit +{0}" -> "Agilität +{0}"; ko "민첩 +{0}" -> "민첩성 +{0}"; ru "Ловкость +{0}" -> "Скорость +{0}"; th "ความคล่องตัว +{0}" -> "ความเร็ว +{0}". Source: FF3 system MSG_SYSTEM_300 (label part).
- `Accuracy: {0}%`: de "Treffsicherheit: {0} %" -> "Präzision: {0} %"; es "Precisión: {0}%" -> "Puntería: {0}%"; ru "Меткость: {0}%" -> "Точность: {0}%". Source: FF3 system MSG_SYSTEM_294 (label part).
- `Agility: {0}`: de "Schnelligkeit: {0}" -> "Agilität: {0}"; ko "민첩: {0}" -> "민첩성: {0}"; ru "Ловкость: {0}" -> "Скорость: {0}"; th "ความคล่องตัว: {0}" -> "ความเร็ว: {0}". Source: FF3 system MSG_SYSTEM_300 (label part).
- `Defense: {0}`: ja "防御力: {0}" -> "防御: {0}"; de "Verteidigung: {0}" -> "Abwehr: {0}"; zht "防禦力：{0}" -> "防禦：{0}"; zhc "防御力：{0}" -> "防御：{0}"; th "พลังป้องกัน: {0}" -> "การป้องกัน: {0}". Source: FF3 system MSG_SYSTEM_295 (label part).
- `Evasion: {0}%`: it "Elusione: {0}%" -> "Destrezza: {0}%"; de "Ausweichrate: {0} %" -> "Ausweichen: {0} %"; zhc "闪避率：{0}%" -> "回避率：{0}%"; th "อัตราหลบหลีก: {0}%" -> "การหลีกหนี: {0}%". Source: FF3 system MSG_SYSTEM_296 (label part).
- `Intellect: {0}`: it "Intelligenza: {0}" -> "Saggezza: {0}"; pt "Inteligência: {0}" -> "Intelecto: {0}". Source: FF3 system MSG_SYSTEM_302 (label part).
- `Remove`: ja "外す" -> "はずす"; zht "卸下" -> "卸除"; ru "Снять" -> "Убрать"; th "ถอด" -> "ถอดออก". Source: FF3 system MSG_SYSTEM_284.
- `Stamina: {0}`: fr "Endurance : {0}" -> "Vitalité : {0}"; de "Ausdauer: {0}" -> "Konstitution: {0}"; es "Aguante: {0}" -> "Entereza: {0}"; zht "耐力：{0}" -> "體力：{0}"; zhc "耐力：{0}" -> "体力：{0}"; th "ความอดทน: {0}" -> "ความแข็งแกร่ง: {0}". Source: FF3 system MSG_SYSTEM_301 (label part).
- `Strength: {0}`: th "พลัง: {0}" -> "พละกำลัง: {0}". Source: FF3 system MSG_SYSTEM_299 (label part).
- `airship`: es "aeronave" -> "barco volador"; th "เรือบิน" -> "เรือเหาะ"; pt "aeronave" -> "navio voador". Source: FF3 story_mes (dialogue).
- `{0}: Recovered {1} HP`: de "{0}: {1} KP wiederhergestellt" -> "{0}: {1} LP wiederhergestellt" (FF1-FF5 system HP/MP labels (FF1 MSG_SYSTEM_053/283, FF5 MSG_SYSTEM_142/143)); es "{0}: recupera {1} PV" -> "{0}: recupera {1} de VIT" (HP/MP labels as above, with article/verb agreement fixed by hand); ru "{0}: восстановлено {1} HP" -> "{0}: восстановлено {1} ОЗ" (FF1-FF5 system HP/MP labels (FF1 MSG_SYSTEM_053/283, FF5 MSG_SYSTEM_142/143)).
- `Exchange`: ja "入れ替え" -> "こうかん"; es "Intercambiar" -> "Cambiar"; ru "Поменять" -> "Обмен"; th "สลับ" -> "แลกเปลี่ยน". Source: FF3 system MSG_SYSTEM_277.
- `Body`: ko "몸" -> "몸통"; ru "Тело" -> "Торс". Source: FF3 system MSG_SYSTEM_291.
- `KO`: fr "KO" -> "Invalidité"; th "ต่อสู้ไม่ได้" -> "น็อกเอ้าต์". Source: FF3 system MSG_SYSTEM_243.
- `Silence`: fr "Mutisme" -> "Silence". Source: FF3 system MSG_SYSTEM_239.
- `Sleep`: th "หลับ" -> "นอนหลับ". Source: FF3 system MSG_SYSTEM_235.
- `Paralysis`: en "Paralysis" -> "Paralyze"; th "ชา" -> "เป็นอัมพาต". Source: FF3 system MSG_SYSTEM_234.
- `Blind`: fr "Aveugle" -> "Cécité"; de "Dunkel" -> "Blind"; ru "Тьма" -> "Слепота". Source: FF3 system MSG_SYSTEM_241.
- `Poison`: ko "독" -> "중독"; th "พิษ" -> "ติดพิษ". Source: FF3 system MSG_SYSTEM_242.
- `Stone`: en "Stone" -> "Petrify"; fr "Fossile" -> "Pétrification"; th "หิน" -> "กลายเป็นหิน". Source: FF3 system MSG_SYSTEM_237.
- `Confusion`: en "Confusion" -> "Confuse"; fr "Folie" -> "Confusion". Source: FF3 system MSG_SYSTEM_236.
- `Agility`: ru "Ловкость" -> "Скорость". Source: FF3 system MSG_SYSTEM_300.
- `Stamina`: fr "Vigueur" -> "Vitalité"; th "พละกำลัง" -> "ความแข็งแกร่ง". Source: FF3 system MSG_SYSTEM_301.
- `Intellect`: fr "Sagesse" -> "Intelligence"; it "Intelligenza" -> "Saggezza"; th "ความฉลาด" -> "สติปัญญา". Source: FF3 system MSG_SYSTEM_302.
- `Spirit`: de "Geist" -> "Animus"; ko "정신" -> "정신력"; th "จิตใจ" -> "จิตวิญญาณ"; pt "Espírito" -> "Vigor". Source: FF3 system MSG_SYSTEM_303.
- `Attack`: ja "攻撃力" -> "攻撃"; zht "攻擊力" -> "攻擊"; zhc "攻击力" -> "攻击"; th "พลังโจมตี" -> "โจมตี". Source: FF3 system MSG_SYSTEM_293.
- `Evasion`: it "Schivata" -> "Destrezza"; th "การหลบหลีก" -> "การหลีกหนี". Source: FF3 system MSG_SYSTEM_296.
- `Magic Defense`: ko "마법 방어" -> "마법 방어력"; th "ป้องกันเวทมนตร์" -> "การป้องกันเวทมนตร์". Source: FF3 system MSG_SYSTEM_297.
- `Magic Evasion`: ja "魔法回避" -> "魔法回避率"; it "Evasione magica" -> "Destrezza magica"; ko "마법 회피" -> "마법 회피율"; zht "魔法迴避" -> "魔法閃避率"; zhc "魔法回避" -> "魔法回避率"; th "หลบหลีกเวทมนตร์" -> "การหลีกหนีเวทมนตร์". Source: FF3 system MSG_SYSTEM_298.
- `Config`: en "Config" -> "Configuration"; th "ตั้งค่าระบบ" -> "การตั้งค่า". Source: FF3 system MSG_SYSTEM_252.
- `Key Items`: fr "Objets clés" -> "Objets-clefs"; pt "Itens principais" -> "Itens-chave". Source: FF3 system MSG_SYSTEM_263.
- `Sort`: fr "Classer" -> "Trier". Source: FF3 system MSG_SYSTEM_265.
- `Remove All`: fr "Tout enlever" -> "Tout retirer"; ru "Снять все" -> "Убрать все". Source: FF3 system MSG_SYSTEM_285.
- `Attack: {0}`: ja "攻撃力: {0}" -> "攻撃: {0}"; zht "攻擊力：{0}" -> "攻擊：{0}"; zhc "攻击力：{0}" -> "攻击：{0}"; th "พลังโจมตี: {0}" -> "โจมตี: {0}". Source: FF3 system MSG_SYSTEM_293 (label part).
- `Stamina +{0}`: fr "Vigueur +{0}" -> "Vitalité +{0}"; th "พละกำลัง +{0}" -> "ความแข็งแกร่ง +{0}". Source: FF3 system MSG_SYSTEM_301 (label part).
- `Intellect +{0}`: fr "Sagesse +{0}" -> "Intelligence +{0}"; it "Intelligenza +{0}" -> "Saggezza +{0}"; th "ความฉลาด +{0}" -> "สติปัญญา +{0}". Source: FF3 system MSG_SYSTEM_302 (label part).
- `Evasion +{0}`: it "Schivata +{0}" -> "Destrezza +{0}"; th "การหลบหลีก +{0}" -> "การหลีกหนี +{0}". Source: FF3 system MSG_SYSTEM_296 (label part).

#### ff4: 143 rows in 53 keys, 2 new keys

- `HP`: override removed (now equal to the common entry).
- `Accuracy`: override removed (now equal to the common entry).
- `Chocobo`: override removed (now equal to the common entry).
- `Status`: override removed (now equal to the common entry).
- `Strength`: th "พลัง" -> "พละกำลัง". Source: FF4 system MSG_SYSTEM_175.
- `Agility`: fr "Agilité" -> "Rapidité"; ko "민첩" -> "민첩성"; ru "Ловкость" -> "Скорость"; th "ความคล่องแคล่ว" -> "ความเร็ว". Source: FF4 system MSG_SYSTEM_176.
- `Stamina`: fr "Endurance" -> "Vigueur"; de "Ausdauer" -> "Konstitution"; es "Resistencia" -> "Entereza"; zht "耐力" -> "體力"; zhc "耐力" -> "体力"; ru "Выносливость" -> "Стойкость"; th "ความอดทน" -> "ความแข็งแกร่ง". Source: FF4 system MSG_SYSTEM_177.
- `Intellect`: fr "Intelligence" -> "Magie"; it "Intelletto" -> "Intelligenza"; de "Intellekt" -> "Intelligenz"; es "Intelecto" -> "Inteligencia". Source: FF4 system MSG_SYSTEM_178.
- `Spirit`: de "Geist" -> "Animus"; ko "정신" -> "정신력"; th "จิตวิญญาณ" -> "วิญญาณ"; pt "Espírito" -> "Vigor". Source: FF4 system MSG_SYSTEM_179.
- `Attack`: de "Angriff" -> "Attacke"; ko "공격" -> "공격력"; th "โจมตี" -> "การโจมตี". Source: FF4 system MSG_SYSTEM_180.
- `Defense`: de "Verteidigung" -> "Abwehr"; ko "방어" -> "방어력"; th "ป้องกัน" -> "การป้องกัน". Source: FF4 system MSG_SYSTEM_182.
- `Evasion`: ja "回避" -> "回避率"; fr "Esquive" -> "Parade"; it "Evasione" -> "Schivata"; ko "회피" -> "회피율"; zht "迴避" -> "閃避率"; zhc "回避" -> "回避率"; th "หลบหลีก" -> "การหลีกหนี". Source: FF4 system MSG_SYSTEM_183.
- `Magic Defense`: de "Magieverteidigung" -> "Magieabwehr"; ko "마법 방어" -> "마법 방어력"; th "ป้องกันเวทย์" -> "การป้องกันเวทมนตร์". Source: FF4 system MSG_SYSTEM_184.
- `Magic Evasion`: ja "魔法回避" -> "魔法回避率"; fr "Esquive magique" -> "Parade magique"; it "Evasione magica" -> "Schivata magica"; de "Magieausweichen" -> "Magieresistenz"; ko "마법 회피" -> "마법 회피율"; zht "魔法迴避" -> "魔法閃避率"; zhc "魔法回避" -> "魔法回避率"; th "หลบหลีกเวทย์" -> "การหลีกหนีเวทมนตร์". Source: FF4 system MSG_SYSTEM_185.
- `Airship`: es "Aeronave" -> "Barco volador". Source: FF4 story_mes (dialogue).
- `Enemy HP Display`: de "Gegner-KP-Anzeige" -> "Gegner-LP-Anzeige" (FF1-FF5 system HP/MP labels (FF1 MSG_SYSTEM_053/283, FF5 MSG_SYSTEM_142/143)); es "Mostrar PV enemigos" -> "Mostrar VIT enemiga" (HP/MP labels as above, with article/verb agreement fixed by hand); ru "Отображение HP врагов" -> "Отображение ОЗ врагов" (FF1-FF5 system HP/MP labels (FF1 MSG_SYSTEM_053/283, FF5 MSG_SYSTEM_142/143)).
- `Enemy HP: {0}`: de "Gegner-KP: {0}" -> "Gegner-LP: {0}" (FF1-FF5 system HP/MP labels (FF1 MSG_SYSTEM_053/283, FF5 MSG_SYSTEM_142/143)); es "PV enemigos: {0}" -> "VIT enemiga: {0}" (HP/MP labels as above, with article/verb agreement fixed by hand); ru "HP врагов: {0}" -> "ОЗ врагов: {0}" (FF1-FF5 system HP/MP labels (FF1 MSG_SYSTEM_053/283, FF5 MSG_SYSTEM_142/143)).
- `{0} for party HP. {1} to cancel.`: de "{0} für die KP der Gruppe. {1} zum Abbrechen." -> "{0} für die LP der Gruppe. {1} zum Abbrechen." (FF1-FF5 system HP/MP labels (FF1 MSG_SYSTEM_053/283, FF5 MSG_SYSTEM_142/143)); es "{0} para los PV del grupo. {1} para cancelar." -> "{0} para la VIT del grupo. {1} para cancelar." (HP/MP labels as above, with article/verb agreement fixed by hand); ru "{0} — HP отряда. {1} — отмена." -> "{0} — ОЗ отряда. {1} — отмена." (FF1-FF5 system HP/MP labels (FF1 MSG_SYSTEM_053/283, FF5 MSG_SYSTEM_142/143)).
- `{0}: Recovered {1} HP`: de "{0}: {1} KP wiederhergestellt" -> "{0}: {1} LP wiederhergestellt" (FF1-FF5 system HP/MP labels (FF1 MSG_SYSTEM_053/283, FF5 MSG_SYSTEM_142/143)); es "{0}: recupera {1} PV" -> "{0}: recupera {1} de VIT" (HP/MP labels as above, with article/verb agreement fixed by hand); ru "{0}: восстановлено {1} HP" -> "{0}: восстановлено {1} ОЗ" (FF1-FF5 system HP/MP labels (FF1 MSG_SYSTEM_053/283, FF5 MSG_SYSTEM_142/143)).
- `Airship Landing`: es "Zona de aterrizaje de aeronave" -> "Zona de aterrizaje de barco volador". Source: FF4 story_mes dialogue word for 飛空艇 (airship word only).
- `Airship Landings`: es "Zonas de aterrizaje de aeronave" -> "Zonas de aterrizaje de barco volador". Source: FF4 story_mes dialogue word for 飛空艇 (airship word only).
- `KO`: fr "KO" -> "Coma"; th "ต่อสู้ไม่ได้" -> "น็อกเอาต์". Source: FF4 system MSG_SYSTEM_121.
- `Silence`: fr "Mutisme" -> "Silence"; it "Silenzio" -> "Mutismo". Source: FF4 system MSG_SYSTEM_116.
- `Sleep`: th "หลับ" -> "นอนหลับ". Source: FF4 system MSG_SYSTEM_124.
- `Paralysis`: en "Paralysis" -> "Paralyze"; th "ชา" -> "อัมพาต". Source: FF4 system MSG_SYSTEM_125.
- `Blind`: en "Blind" -> "Darkness"; fr "Aveugle" -> "Cécité"; de "Dunkel" -> "Blind"; th "ตาบอด" -> "ความมืด". Source: FF4 system MSG_SYSTEM_115.
- `Stone`: en "Stone" -> "Petrify"; fr "Fossile" -> "Pierre"; th "หิน" -> "แข็ง"; pt "Petrificação" -> "Petrificar". Source: FF4 system MSG_SYSTEM_120.
- `Confusion`: en "Confusion" -> "Confuse"; fr "Folie" -> "Confusion"; it "Caos" -> "Confusione". Source: FF4 system MSG_SYSTEM_123.
- `Slow`: it "Lentezza" -> "Adagio"; es "Lentitud" -> "Freno"; zht "減速" -> "緩慢". Source: FF4 system MSG_MAGIC_NAME_07.
- `Doom`: fr "Condamné" -> "Glas"; it "Destino" -> "Condanna"; de "Verdammnis" -> "Tod"; ko "죽음의선고" -> "죽음의 선고"; th "คำสาป" -> "พิพากษา"; pt "Perdição" -> "Condenação". Source: FF4 system MSG_SYSTEM_128.
- `Toad`: it "Rospo" -> "Ranocchio". Source: FF4 system MSG_SYSTEM_119.
- `Float`: es "Lévita" -> "Levitación"; ru "Левитация" -> "Полет". Source: FF4 system MSG_SYSTEM_126.
- `Berserk`: ja "バーサク" -> "狂戦士"; it "Furia" -> "Furore"; ko "버서크" -> "광전사". Source: FF4 system MSG_SYSTEM_122.
- `Config`: en "Config" -> "Configuration"; th "ตั้งค่าระบบ" -> "การตั้งค่า". Source: FF4 system MSG_SYSTEM_140.
- `Interruption`: en "Interruption" -> "Quick Save"; th "ควิกเซฟ" -> "บันทึกด่วน". Source: FF4 system MSG_SYSTEM_141.
- `Key Items`: fr "Objets clés" -> "Objets clefs"; pt "Itens principais" -> "Itens-chave". Source: FF4 system MSG_SYSTEM_148.
- `Remove`: fr "Retirer" -> "Enlever"; de "Entfernen" -> "Ablegen"; zht "移除" -> "卸除"; zhc "移除" -> "卸下"; ru "Убрать" -> "Снять"; th "นำออก" -> "ถอดออก". Source: FF4 system MSG_SYSTEM_164.
- `Remove All`: th "ถอดทั้งหมด" -> "ถอดออกทั้งหมด". Source: FF4 system MSG_SYSTEM_165.
- `Head`: it "Elmo" -> "Testa"; ru "Шлем" -> "Голова". Source: FF4 system MSG_SYSTEM_170.
- `Body`: it "Armatura" -> "Corpo"; de "Brust" -> "Körper"; es "Cuerpo" -> "Torso"; ru "Доспехи" -> "Торс". Source: FF4 system MSG_SYSTEM_171.
- `airship`: fr "vaisseau" -> "aéronef"; pt "helibarco" -> "aeronave". Source: FF4 story_mes (dialogue).
- `Strength: {0}`: th "ความแข็งแกร่ง: {0}" -> "พละกำลัง: {0}". Source: FF4 system MSG_SYSTEM_175 (label part).
- `Agility: {0}`: fr "Agilité : {0}" -> "Rapidité : {0}"; ru "Ловкость: {0}" -> "Скорость: {0}". Source: FF4 system MSG_SYSTEM_176 (label part).
- `Stamina: {0}`: ru "Выносливость: {0}" -> "Стойкость: {0}"; th "พละกำลัง: {0}" -> "ความแข็งแกร่ง: {0}". Source: FF4 system MSG_SYSTEM_177 (label part).
- `Intellect: {0}`: fr "Sagesse : {0}" -> "Magie : {0}"; th "ความฉลาด: {0}" -> "สติปัญญา: {0}". Source: FF4 system MSG_SYSTEM_178 (label part).
- `Attack: {0}`: ja "攻撃力: {0}" -> "攻撃: {0}"; zht "攻擊力：{0}" -> "攻擊：{0}"; zhc "攻击力：{0}" -> "攻击：{0}"; th "พลังโจมตี: {0}" -> "การโจมตี: {0}". Source: FF4 system MSG_SYSTEM_180 (label part).
- `Defense: {0}`: ja "防御力: {0}" -> "防御: {0}"; zht "防禦力：{0}" -> "防禦：{0}"; zhc "防御力：{0}" -> "防御：{0}"; th "พลังป้องกัน: {0}" -> "การป้องกัน: {0}". Source: FF4 system MSG_SYSTEM_182 (label part).
- `Evasion: {0}%`: fr "Esquive : {0} %" -> "Parade : {0} %"; th "การหลบหลีก: {0}%" -> "การหลีกหนี: {0}%". Source: FF4 system MSG_SYSTEM_183 (label part).
- `Strength +{0}`: th "ความแข็งแกร่ง +{0}" -> "พละกำลัง +{0}". Source: FF4 system MSG_SYSTEM_175 (label part).
- `Agility +{0}`: fr "Agilité +{0}" -> "Rapidité +{0}"; ru "Ловкость +{0}" -> "Скорость +{0}". Source: FF4 system MSG_SYSTEM_176 (label part).
- `Stamina +{0}`: ru "Выносливость +{0}" -> "Стойкость +{0}"; th "พละกำลัง +{0}" -> "ความแข็งแกร่ง +{0}". Source: FF4 system MSG_SYSTEM_177 (label part).
- `Intellect +{0}`: fr "Sagesse +{0}" -> "Magie +{0}"; th "ความฉลาด +{0}" -> "สติปัญญา +{0}". Source: FF4 system MSG_SYSTEM_178 (label part).
- `Evasion +{0}`: fr "Esquive +{0}" -> "Parade +{0}"; th "การหลบหลีก +{0}" -> "การหลีกหนี +{0}". Source: FF4 system MSG_SYSTEM_183 (label part).
- `hovercraft`: new key. Source: FF4 system MSG_SYSTEM_288, lower case as the FF4 story_mes dialogue writes it.
- `yellow chocobo`: new key. Source: composed: FF4 has no yellow-chocobo name; FF4 MSG_SYSTEM_343 Chocobo + the MSG_SYSTEM_285 Black Chocobo pattern.

#### ff5: 127 rows in 44 keys

- `ship`: override removed (now equal to the common entry).
- `Confusion`: override removed (now equal to the common entry).
- `Stop`: override removed (now equal to the common entry).
- `HP`: override removed (now equal to the common entry).
- `MP`: override removed (now equal to the common entry).
- `Status`: override removed (now equal to the common entry).
- `Enemy HP Display`: it "Visualizzazione HP nemico" -> "Visualizzazione PV nemico" (FF1-FF5 system HP/MP labels (FF1 MSG_SYSTEM_053/283, FF5 MSG_SYSTEM_142/143)); de "Gegner-HP-Anzeige" -> "Gegner-LP-Anzeige" (FF1-FF5 system HP/MP labels (FF1 MSG_SYSTEM_053/283, FF5 MSG_SYSTEM_142/143)); es "Pantalla de HP enemigo" -> "Pantalla de VIT enemiga" (HP/MP labels as above, with article/verb agreement fixed by hand); ru "Отображение HP врага" -> "Отображение ОЗ врага" (FF1-FF5 system HP/MP labels (FF1 MSG_SYSTEM_053/283, FF5 MSG_SYSTEM_142/143)); pt "Exibição de HP do inimigo" -> "Exibição de PV do inimigo" (FF1-FF5 system HP/MP labels (FF1 MSG_SYSTEM_053/283, FF5 MSG_SYSTEM_142/143)).
- `Enemy HP: {0}`: it "HP nemico: {0}" -> "PV nemico: {0}" (FF1-FF5 system HP/MP labels (FF1 MSG_SYSTEM_053/283, FF5 MSG_SYSTEM_142/143)); de "Gegner-HP: {0}" -> "Gegner-LP: {0}" (FF1-FF5 system HP/MP labels (FF1 MSG_SYSTEM_053/283, FF5 MSG_SYSTEM_142/143)); es "HP enemigo: {0}" -> "VIT enemiga: {0}" (HP/MP labels as above, with article/verb agreement fixed by hand); ru "HP врага: {0}" -> "ОЗ врага: {0}" (FF1-FF5 system HP/MP labels (FF1 MSG_SYSTEM_053/283, FF5 MSG_SYSTEM_142/143)); pt "HP do inimigo: {0}" -> "PV do inimigo: {0}" (FF1-FF5 system HP/MP labels (FF1 MSG_SYSTEM_053/283, FF5 MSG_SYSTEM_142/143)).
- `airship`: fr "aéronef" -> "vaisseau"; es "aeronave" -> "barco volador"; th "เรือบิน" -> "เรือเหาะ"; pt "aeronave" -> "navio voador". Source: FF5 system MSG_MAP_NAME_02.
- `Poison`: ja "毒" -> "猛毒"; ko "독" -> "맹독"; zht "毒" -> "猛毒"; zhc "毒" -> "猛毒"; ru "Яд" -> "Отравление". Source: FF5 system MSG_SYSTEM_414.
- `{0}: Recovered {1} HP`: it "{0}: {1} HP recuperati" -> "{0}: {1} PV recuperati" (FF1-FF5 system HP/MP labels (FF1 MSG_SYSTEM_053/283, FF5 MSG_SYSTEM_142/143)); de "{0}: {1} HP wiederhergestellt" -> "{0}: {1} LP wiederhergestellt" (FF1-FF5 system HP/MP labels (FF1 MSG_SYSTEM_053/283, FF5 MSG_SYSTEM_142/143)); es "{0}: {1} HP recuperados" -> "{0}: recupera {1} de VIT" (HP/MP labels as above, with article/verb agreement fixed by hand); ru "{0}: восстановлено {1} HP" -> "{0}: восстановлено {1} ОЗ" (FF1-FF5 system HP/MP labels (FF1 MSG_SYSTEM_053/283, FF5 MSG_SYSTEM_142/143)); pt "{0}: {1} de HP recuperado" -> "{0}: {1} de PV recuperado" (FF1-FF5 system HP/MP labels (FF1 MSG_SYSTEM_053/283, FF5 MSG_SYSTEM_142/143)).
- `{0}: Recovered {1} MP`: it "{0}: {1} MP recuperati" -> "{0}: {1} PM recuperati"; es "{0}: {1} MP recuperados" -> "{0}: {1} PM recuperados"; ru "{0}: восстановлено {1} MP" -> "{0}: восстановлено {1} ОМ"; pt "{0}: {1} de MP recuperado" -> "{0}: {1} de PM recuperado". Source: FF1-FF5 system HP/MP labels (FF1 MSG_SYSTEM_053/283, FF5 MSG_SYSTEM_142/143).
- `{0} for party HP. {1} to cancel.`: it "{0} per gli HP del gruppo. {1} per annullare." -> "{0} per i PV del gruppo. {1} per annullare." (HP/MP labels as above, with article/verb agreement fixed by hand); de "{0} für Gruppen-HP. {1} zum Abbrechen." -> "{0} für Gruppen-LP. {1} zum Abbrechen." (FF1-FF5 system HP/MP labels (FF1 MSG_SYSTEM_053/283, FF5 MSG_SYSTEM_142/143)); es "{0} para HP del grupo. {1} para cancelar." -> "{0} para la VIT del grupo. {1} para cancelar." (HP/MP labels as above, with article/verb agreement fixed by hand); ru "{0} — HP отряда. {1} — отмена." -> "{0} — ОЗ отряда. {1} — отмена." (FF1-FF5 system HP/MP labels (FF1 MSG_SYSTEM_053/283, FF5 MSG_SYSTEM_142/143)); pt "{0} para HP do grupo. {1} para cancelar." -> "{0} para PV do grupo. {1} para cancelar." (FF1-FF5 system HP/MP labels (FF1 MSG_SYSTEM_053/283, FF5 MSG_SYSTEM_142/143)).
- `Airship Landing`: es "Aterrizaje de aeronave" -> "Aterrizaje de barco volador"; ru "Посадка дирижабля" -> "Посадка воздушного корабля"; pt "Pouso da aeronave" -> "Pouso do navio voador". Source: FF5 system MSG_MAP_NAME_02 and story_mes (airship word only).
- `Airship Landings`: es "Aterrizajes de aeronave" -> "Aterrizajes de barco volador"; ru "Места посадки дирижабля" -> "Места посадки воздушного корабля"; pt "Pousos da aeronave" -> "Pousos do navio voador". Source: FF5 system MSG_MAP_NAME_02 and story_mes (airship word only).
- `Attack`: ja "攻撃力" -> "攻撃"; de "Angriff" -> "Attacke"; zht "攻擊力" -> "攻擊"; zhc "攻击力" -> "攻击"; th "พลังโจมตี" -> "การโจมตี". Source: FF5 system MSG_SYSTEM_190.
- `Defense`: ja "防御力" -> "防御"; de "Verteidigung" -> "Abwehr"; zht "防禦力" -> "防禦"; zhc "防御力" -> "防御"; th "พลังป้องกัน" -> "การป้องกัน"; pt "Defesa" -> "Defesa física". Source: FF5 system MSG_SYSTEM_192.
- `Magic Defense`: ja "魔法防御力" -> "魔法防御"; fr "Défense magique" -> "Psyché"; zht "魔法防禦力" -> "魔法防禦"; zhc "魔法防御力" -> "魔法防御"; ru "Магическая защита" -> "Маг. защита"; th "พลังป้องกันเวทมนตร์" -> "การป้องกันเวทมนตร์". Source: FF5 system MSG_SYSTEM_193.
- `Evasion`: it "Evasione" -> "Schivata"; ko "회피" -> "회피율"; zht "迴避" -> "閃避"; th "การหลบหลีก" -> "การหลีกหนี". Source: FF5 system MSG_SYSTEM_191.
- `Agility`: de "Beweglichkeit" -> "Agilität"; ru "Ловкость" -> "Скорость"; th "ความว่องไว" -> "ความเร็ว". Source: FF5 system MSG_SYSTEM_186.
- `Stamina`: fr "Endurance" -> "Vigueur"; de "Ausdauer" -> "Konstitution"; es "Vigor" -> "Entereza"; ru "Выносливость" -> "Стойкость"; th "ความอดทน" -> "ความแข็งแกร่ง"; pt "Vigor" -> "Resistência". Source: FF5 system MSG_SYSTEM_187.
- `{0}: {1} MP damage`: it "{0}: {1} danni MP" -> "{0}: {1} danni PM"; es "{0}: {1} de daño a MP" -> "{0}: {1} de daño a PM"; ru "{0}: {1} урона MP" -> "{0}: {1} урона ОМ"; pt "{0}: {1} de dano de MP" -> "{0}: {1} de dano de PM". Source: FF1-FF5 system HP/MP labels (FF1 MSG_SYSTEM_053/283, FF5 MSG_SYSTEM_142/143).
- `Config`: en "Config" -> "Configuration"; th "ตั้งค่าระบบ" -> "การตั้งค่า". Source: FF5 system MSG_SYSTEM_136.
- `Interruption`: en "Interruption" -> "Quick Save"; ru "Быстрое сохранение" -> "Быстр. сохранение"; th "ควิกเซฟ" -> "บันทึกด่วน". Source: FF5 system MSG_SYSTEM_137.
- `Save`: ru "Сохранить" -> "Сохранение". Source: FF5 system MSG_SYSTEM_138.
- `Key Items`: pt "Itens principais" -> "Itens-chave". Source: FF5 system MSG_SYSTEM_145.
- `Sort`: fr "Classer" -> "Trier". Source: FF5 system MSG_SYSTEM_147.
- `Remove`: de "Entfernen" -> "Ablegen"; zht "移除" -> "卸除"; zhc "移除" -> "卸下"; ru "Убрать" -> "Снять"; th "นำออก" -> "ถอดออก". Source: FF5 system MSG_SYSTEM_173.
- `Remove All`: fr "Tout enlever" -> "Tout retirer". Source: FF5 system MSG_SYSTEM_174.
- `Head`: it "Elmo" -> "Testa"; ru "Шлем" -> "Голова". Source: FF5 system MSG_SYSTEM_180.
- `Body`: it "Armatura" -> "Corpo"; ru "Доспехи" -> "Торс". Source: FF5 system MSG_SYSTEM_181.
- `Airship`: pt "Helibarco" -> "Navio Voador". Source: FF5 system MSG_MAP_NAME_02.
- `Accessory`: ja "アクセサリ" -> "アクセサリー"; ru "Аксессуар" -> "Аксессуары"; th "เครื่องประดับ" -> "อุปกรณ์เสริม". Source: FF5 system MSG_SYSTEM_182.
- `Boko`: th "โบโค" -> "โบโกะ". Source: FF5 system MSG_SYSTEM_390.
- `Strength: {0}`: th "ความแข็งแกร่ง: {0}" -> "พละกำลัง: {0}". Source: FF5 system MSG_SYSTEM_185 (label part).
- `Agility: {0}`: ru "Ловкость: {0}" -> "Скорость: {0}". Source: FF5 system MSG_SYSTEM_186 (label part).
- `Stamina: {0}`: ru "Выносливость: {0}" -> "Стойкость: {0}"; th "พละกำลัง: {0}" -> "ความแข็งแกร่ง: {0}". Source: FF5 system MSG_SYSTEM_187 (label part).
- `Attack: {0}`: ja "攻撃力: {0}" -> "攻撃: {0}"; zht "攻擊力：{0}" -> "攻擊：{0}"; zhc "攻击力：{0}" -> "攻击：{0}"; th "พลังโจมตี: {0}" -> "การโจมตี: {0}". Source: FF5 system MSG_SYSTEM_190 (label part).
- `Defense: {0}`: ja "防御力: {0}" -> "防御: {0}"; zht "防禦力：{0}" -> "防禦：{0}"; zhc "防御力：{0}" -> "防御：{0}"; th "พลังป้องกัน: {0}" -> "การป้องกัน: {0}"; pt "Defesa: {0}" -> "Defesa física: {0}". Source: FF5 system MSG_SYSTEM_192 (label part).
- `Evasion: {0}%`: ja "回避率: {0}%" -> "回避: {0}%"; zht "閃避率：{0}%" -> "閃避：{0}%"; zhc "回避率：{0}%" -> "回避：{0}%"; th "การหลบหลีก: {0}%" -> "การหลีกหนี: {0}%". Source: FF5 system MSG_SYSTEM_191 (label part).
- `Strength +{0}`: th "ความแข็งแกร่ง +{0}" -> "พละกำลัง +{0}". Source: FF5 system MSG_SYSTEM_185 (label part).
- `Agility +{0}`: ru "Ловкость +{0}" -> "Скорость +{0}". Source: FF5 system MSG_SYSTEM_186 (label part).
- `Stamina +{0}`: ru "Выносливость +{0}" -> "Стойкость +{0}"; th "พละกำลัง +{0}" -> "ความแข็งแกร่ง +{0}". Source: FF5 system MSG_SYSTEM_187 (label part).
- `Evasion +{0}`: ja "回避率+{0}" -> "回避+{0}"; zht "閃避率+{0}" -> "閃避+{0}"; zhc "回避率+{0}" -> "回避+{0}"; th "การหลบหลีก +{0}" -> "การหลีกหนี +{0}". Source: FF5 system MSG_SYSTEM_191 (label part).

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
- `No character data available` (also in FF2, FF4, FF6)
- `{0} leveled up to level {1}` (also in FF4)
- `Vitality +{0}`
- `Intelligence +{0}`
- `Spirit +{0}`
- `{0} {1} job level up to {2}`
- `{0} job level up to {1}`
- `{0} job level up`
- `Failed to dump entity names` (also in FF2)
- `Accuracy {0}`
- `Airship Landing` (also in FF2, FF4, FF5, FF6)
- `Airship Landings` (also in FF2, FF4, FF5, FF6)
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
- `Magic Defense: {0}` (also in FF6)
- `Magic Evade: {0}`
- `Magic Evasion {0}`
- `Magic Evasion: {0}%`
- `Magic: {0}` (also in FF6)
- `Miscellaneous` (also in FF2, FF4, FF5, FF6)
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
- `{0}. Details` (also in FF6)
- `{0}. Page changed` (also in FF6)
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
- `Magic Defense` (also in FF4, FF5, FF6)
- `Magic Defense Count`
- `Magic Evasion` (also in FF4, FF6)
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
- `Chests` (also in FF5, FF6)
- `All allies` (also in FF5)
- `All enemies` (also in FF5)
- `Unknown Row Type {0}` (also in FF6)
- `{0} gil` (also in FF5, FF6)
- `User data not available` (also in FF6)
- `Rename {0}`
- `Waypoint created: {0}`
- `Renamed to {0}` (also in FF5, FF6)
- `Delete {0}?` (also in FF5, FF6)
- `Deleted {0}`
- `Canceled`
- `No waypoints to clear` (also in FF5, FF6)
- `Clear all {0} waypoints on this map?`
- `Are you sure? This cannot be undone.`
- `less than`
- `greater than`
- `Status Details`
- `Handed`
- `Timers frozen` (also in FF5, FF6)
- `Timers resumed` (also in FF5, FF6)
- `Error toggling timer freeze` (also in FF5, FF6)
- `No active timers` (also in FF5, FF6)
- `Error reading timers` (also in FF5, FF6)
- `Keyboard input active.`
- `Cleared {0} waypoints` (also in FF5, FF6)
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
- `{0} learned {1}` (also in FF5, FF6)
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
- `Not in field` (also in FF6)
- `and {0} more`
- `target {0} {1}`
- `landing spot`
- `Enemy Letters`
- `Enemy Letters on`
- `Enemy Letters off`
- `Unavailable during dialogue`
- `: HP: {0}/{1}`
- `Unavailable outside of battle`
- `{0}, status information not available` (also in FF6)
- `EXP counter {0}` (also in FF6)
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
- `Are you absolutely sure?` (also in FF6)
- `Rename waypoint` (also in FF6)
- `Rename failed` (also in FF6)
- `Rename cancelled` (also in FF6)
- `Added {0} as {1}` (also in FF6)
- `Removed {0}` (also in FF6)
- `Waypoint creation cancelled` (also in FF6)
- `Delete all {0} waypoints?` (also in FF6)
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
- `Character Info` (also in FF6)
- `Vitals` (also in FF6)
- `Attributes` (also in FF6)
- `Combat Stats` (also in FF6)
- `Progression` (also in FF6)
- `Commands` (also in FF6)
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

#### From FF6: 102

- `Wall Bumps`
- `Category: {0}, {1} {2}`
- `, {0} of {1}`
- `, no path`
- `Ground level`
- `Low altitude`
- `Cruising altitude`
- `High altitude`
- `Maximum altitude`
- `HP {0} of {1}`
- `MP {0} of {1}`
- `Status: {0}`
- `Airship status not available`
- `Not on airship`
- `Heading {0} degrees`
- `Error reading airship status`
- `Facing {0}`
- `Rising. `
- `Descending. `
- `Path to {0}: {1}`
- `No path to {0}. {1}`
- `Please select a category.`
- `MP: {0} / {1}`
- `Commands: {0}`
- `Magicite: {0}`
- `Bonus: {0}`
- `Details`
- `Name and Class`
- `At Level Up`
- `Next Level in: {0}`
- `Evasion: {0}`
- `Magic Evasion: {0}`
- `Commands: none`
- `Magicite: none`
- `Magicite: N/A`
- `At Level Up: none`
- `At Level Up: {0}`
- `At Level Up: N/A`
- `, Level `
- `, Front Row`
- `, Back Row`
- `, HP {0}/{1}`
- `, HP {0}`
- `, MP {0}/{1}`
- `, MP {0}`
- `Bottom`
- `Top`
- `Properties: `
- `Magic: `
- `none`
- `Element {0}`
- `Fire`
- `Ice`
- `Lightning`
- `Wind`
- `Holy`
- `Earth`
- `Water`
- `No characters can equip`
- `, learning, {0} percent`
- `, equipped`
- `Slot {0}: Empty`
- `, fixed`
- ` - NOT OBTAINED`
- ` - EQUIPPED`
- `, MP Cost {0}`
- `Not equipped`
- `EQUIPPED to {0}`
- `Equipped to {0}`
- `Teaches {0} abilities: `
- `Mastered`
- `Level up bonus: {0}`
- `, HP {0}/{1}, MP {2}/{3}`
- ` ({0} of {1})`
- `Success`
- `Failed`
- `Out of time`
- `{0} minute`
- `{0} minutes`
- `{0} second`
- `{0} seconds`
- `Timer: `
- `Wager {0}`
- `, Win {0}`
- `, Fight {0}`
- `. Yes or No`
- `{0} Magic AP`
- ` and leveled up to level {0}`
- `Summon, {0}`
- `Left`
- `Right`
- `Up`
- `Down`
- `Down-Left`
- `Down-Right`
- `Up-Left`
- `Up-Right`
- `L2`
- `R2`
- `then Confirm`
- `Blitz input: {0}`
- `Press Enter for default name`

<!-- END GENERATED -->
