# Map labels for untitled sub-maps

Written 2026-10-11. The game's map master (`map.txt`) titles most interiors ("Salamand Items",
"Castle Fynn 1F"), but some rooms carry `map_title None` and floor 0, so the mod spoke only the
area name there: "Exit to Salamand" for Josef's house and for the church as well as for the town.
The user asked for labels by inference (2026-10-11), under these rules:

1. First look for data the game has (a message key the master forgot, as FF2's late Altair
   rebel hideout rooms: `MSG_MAP_NAME_11`).
2. A room with a named NPC is "<Name>'s House" (or "<Name>'s Room" where it plainly is not a
   house). A room with only generic NPCs (a villager, a man) is "House".
3. A room with no NPC is labelled by the event or point it holds (the resurrection point, the
   healing spring, the well).
4. A room with nothing in it stays unlabelled. A dungeon room without a floor title stays
   unlabelled too, unless a chest holds a key item that defines the room ("Pass Room").
5. Castle, airship, dungeon and cutscene rooms are "sections", not dwellings: they are labelled
   only by a defining NPC or event (the throne room, the prison, the engine room).
6. What the NPCs say decides a building's function before any name does (user, 2026-10-11): a
   bartender, or talk of drinks or a pub, makes it "Pub" (two such rooms in one town: the one
   with more NPCs is the pub); a nurse and a doctor make an infirmary; a priest who resurrects
   for gil makes a church. A named NPC's room is "<Name>'s House" only when nobody there says
   what the building is. Labels must read naturally: "House", never "Four Old Men's House".
7. An authored entry replaces the master's title when the master is wrong, not only when it is
   missing: FF1's churches are "2F" in the master.

The dialogue scans (session scripts: each NPC's `message_key` and the messages its script
shows, read from the `story_mes` table, with `story_cha` giving the speaker's role) covered
the rooms themselves and, at the user's suggestion, the whole town around them: FF2 Fynn 26
has a "Bartender" speaker; FF2 Poft: a pirate in town says "Ye be lookin' for Cid? He's
probably over in the pub"; FF3 Amur: "If you're looking for info, I suggest the tavern"; FF4
Troia Castle 100: "I'm the nurse. She's the doctor"; FF4 Dwarven Castle: "He's sleeping in
the infirmary as we speak" (Cid); FF1: the priests' resurrection line; FF6 Figaro Castle 34:
Edgar speaks as "King". Nothing pub-like in FF5's untitled rooms.

The data: `data/<game>/map_titles.json`, read by `src/core/field/map_names.cpp`
(`override_title`). Formats: a string is a game message key; `{"text": K}` is the mod text K
(`data/common/mod_text.json`, 12 languages); `{"owner": N}` and `{"room": N}` take the NPC's
baked (Japanese) name and translate it as the entity list does (`data/<game>/translation.json`),
dropping a descriptor in parentheses; `{"entity": E}` is the translated name of the baked object
E; `{"item": M}` is "<item> Room" for the key item named by message M; a list joins its parts.

The facts below come from `tools/mapdump/<game>_objects.jsonl` and `<game>_master/map.txt`
(report script in the session notes: untitled sub-maps with their NPCs, entities, chests, events
and exits). "Generic" means the NPC's official name is a description ("Man (Blue Hair)").

Mod text added for these labels (translations written by the assistant, 2026-10-11, not the
game's): "{0}'s House", "{0}'s Room", "{0} Room", "Church", "Pub", "Throne Room", "House",
"Prison", "Engine Room", "Revival Chamber", "Back Room", "Bedroom", "Infirmary"; "Well"
already existed.

## FF1

The ten untitled non-world maps are the ending cutscene maps of Castle Cornelia
(`Map_40000`-`Map_40004`, `Map_20170`, `Map_20180`) and the world-map variants. The churches are
titled by the floor field instead ("Cornelia 2F"; user, 2026-10-11): each has a priest NPC
(神父, or an unnamed NPC in the later towns) whose dialogue is the game's resurrection service,
MSG_MAP0A01_06 "It will cost (KINGAKU) gil to resurrect (SITAI). Proceed?".

| id | sub-map | label | evidence |
|---|---|---|---|
| 9 | Map_20021_7 Cornelia | Church | priest 1:神父, the resurrection line |
| 18 | Map_20041_2 Pravoka | Church | priest 神父, the resurrection line |
| 27 | Map_20061_2 Elfheim | Church | priest 神父, the resurrection line |
| 46 | Map_20111_4 Crescent Lake | Church | unnamed NPC with the resurrection line |
| 55 | Map_20131_2 Onrac | Church | unnamed NPC with the resurrection line |
| 65 | Map_20151_7 Gaia | Church | unnamed NPC with the resurrection line |

Melmond has no church any more ("The vampire attacked, destroying our church").

## FF2

| id | sub-map | label | evidence |
|---|---|---|---|
| 4 | Map_20011_10 Altair | Church | entity 教会 "Church", no NPC |
| 19 | Map_20021_5 Gatrea | Church | entity 教会 |
| 26 | Map_20031_5 Fynn | Pub | occupied Fynn: eight Palamecian soldiers, a man in blue; its back door leads to 27 (Scott) - the pub where the password is given |
| 27 | Map_20031_6 Fynn | Scott's Room | NPC スコット Scott, entity "Ring's Glow", three potions: the back room where Scott dies |
| 43 | Map_20041_7 Castle Fynn | Throne Room | Hilda (Opening / Ending), Gordon, Leila, the hidden passage; exits to 3F |
| 53 | Map_20061_1 Poft | Pub | Cid with nine 飛空艇乗り ("Cid's Underling"); the user knows the room as Poft's pub (rule 6) |
| 60 | Map_20071_1 Salamand | Josef's House | NPCs ヨーゼフ Josef, Nelly (Josef's Daughter), Adventurer (Ponytail) - the user's own example |
| 64 | Map_20071_5 Salamand | Church | entity 協会 (the devs' homophone of 教会): no NPC |
| 69 | Map_20081_2 Bafsk | Church | entity 協会 |
| 96 | Map_20101_6 Mysidia | Church | entity 協会 |
| 97 | Map_20101_7 Mysidia | House | entity Bookshelf only: a dwelling with an examinable, no NPC |
| 98 | Map_20101_8 Mysidia | Masked Figure's House | NPC 仮面　色変え1 "Masked Figure", entity Goddess Statue |
| 116 | Map_20161_1 Poft (late) | Pub | the same room after Cid's death |
| 138 | Map_30021_3 Bafsk Cave | Pass Room | chest = key item MSG_KEY_NAME_03 "Pass", plus the warp zone |
| 169 | Map_30051_3 Dreadnought | Prison | Hilda and Cid behind "Iron Bars", "Locked Iron Bars" |
| 175 | Map_30051_9 Dreadnought | Engine Room | entities "Dreadnought Engine", "Dreadnought Demolition Trigger", Dark Knight |
| 179 | Map_30061_4 Castle Deist | House | generic NPCs Mom and Child (Blond), a Cottage chest: a dwelling in the castle |
| 180 | Map_30061_5 Castle Deist | Wyvern's Room | NPC 飛竜 Wyvern, entity Wyvern Egg |
| 229 | Map_30111_2 Leviathan | Ricard's Room | NPC リチャード Ricard with the swallowed villagers |
| 230 | Map_30111_3 Leviathan | Big Horn's Room | NPC ビックホーン Big Horn (the boss), two chests |
| 325-329 | Map_20131_6..10 Altair | Rebel Hideout | MSG_MAP_NAME_11, the early rooms' title (user, 2026-10-10) |
| 327 | Map_20131_8 Altair | Rebel Hideout Revival Chamber | event 生き返りの間 "room of revival", Magic Circle, ten elders |
| 330 | Map_20131_11 Altair (late) | Church | entity 教会 |
| 331 | Map_20141_5 Gatrea (late) | Church | entity 教会 |

Left unlabelled on purpose: Fynn 30 (`Map_20031_9`, an entity "Boundary" and nothing else);
Castle Fynn 32, 44, 45 (empty), 38 and 42 (treasure rooms: golden equipment, tomes), 89 and 100
(cutscenes); Snow Cavern 140, 142, 148 (empty side rooms); Dreadnought 164-174 except the two
above (soldiers, chests, the deck); Castle Deist 176 (entrance hall), 177, 178 (treasure), 181,
182 (empty); Coliseum 193-199; Leviathan 228 (chests); Chocobo Forest, TheEnd, world variants.
The snowcraft room the user mentioned is titled by the game, so nothing was needed there.
`translation.json`: 協会 now reads "Church" in every language (copied from 教会; it read
"Association").

## FF3

Towns revive at a point the game names いきかえり / 生き返り "Resurrection"; those rooms are
labelled with that word (rule 3). FF3 has no "Item Shop" title key, so Doga's Manor's moogle
shop (132) stays unlabelled rather than borrowing "Inn & Item Shop".

| id | sub-map | label | evidence |
|---|---|---|---|
| 11 | Map_20011_6 Ur | Healing Spring | entities 回復の泉 Healing Spring, 生き返りの泉 Resurrection Spring; one generic NPC |
| 12 | Map_20011_7 Ur | Topapa's House | NPCs トパパ Topapa, Nina, Homac |
| 19 | Map_20021_6 Kazus | House | generic elder, candle, a potion |
| 20 | Map_20021_7 Kazus | Recovery | entity 回復 "Recovery", no NPC |
| 27 | Map_20041_5 Castle Sasune | Sara's Room | NPC サラ Sara, Water Splash |
| 38 | Map_20071_2 Caanan | Salina's House | NPC サリーナ（女性2） Salina, a woman |
| 42 | Map_20071_6 Caanan | Resurrection | entity いきかえり |
| 43 | Map_20071_7 Caanan | Cid's House | NPCs シド Cid, Mrs. Cid, Doga |
| 46 | Map_20081_1 Tozus | Shelco's House | NPC シェルコ Shelco (twice), hidden door |
| 54 | Map_20091_4 Vikings' Cove | Resurrection | entity いきかえり, an elder |
| 56, 57, 58 | Map_20101_1..3 Tokkul | House | generic villagers only |
| 62 | Map_20111_2 Village of the Ancients | Resurrection | entity いきかえり |
| 82 | Map_20151_5 Gysahl | Resurrection | entity いきかえり |
| 94 | Map_20181_2 Wrecked Ship | Aria's Room | NPC エリア Aria with an elder |
| 98 | Map_20201_1 Amur | Resurrection | entity いきかえり |
| 99 | Map_20201_2 Amur | Pub | a townsman says "If you're looking for info, I suggest the tavern"; this is the room with the NPCs (Gill and the Four Old Men), the other untitled room is the resurrection point (rule 6) |
| 117 | Map_20221_2 SW Saronia | Fat Chocobo's House | NPC デブチョコボ Fat Chocobo, an evoker, eleven Gysahl Greens |
| 118 | Map_20221_3 SW Saronia | Resurrection | entity 生き返り |
| 141 | Map_20281_3 Replito | House | generic Elder 4, hidden door, an elixir |
| 146 | Map_20311_2 Falgabard | Resurrection | entity 生き返り |
| 150 | Map_20311_6 Falgabard | House | generic Elder Chief 2 |
| 336 | Map_20011_8 Ur | House | generic elder, hidden door, antidotes |

Left unlabelled: Castle Argus 73 and Castle Saronia 105, 110, 114 (castle rooms: a white mage
and treasure, the gate, black mages, scholars with the Nautilus event), Dwarven Hollows 85 (the
village hall that leads to every shop) and 90 (a passage), Temple of Water 96, Doga's Manor
132 and 133, Chocobo Woods (one per area), the cutscene maps and world variants.

## FF4

The master titles nearly every room ("(ROZA)'s House", "Item Shop"); the untitled rest are
town and castle main maps, cutscene maps and these:

| id | sub-map | label | evidence |
|---|---|---|---|
| 37 | Map_20031_5 Kaipo | Rosa's House | NPCs ローザ Rosa, Edward, Rydia (Child), entity "For Desert Fever..." |
| 100 | Map_20111_4 Troia Castle | Infirmary | Edward in bed; the NPCs say "I'm the nurse. She's the doctor" and "a sick man resting here" (rule 6) |
| 109 | Map_20121_3 Agart | Well | entity "Use Magma Rock at Agart Village Well", no NPC |
| 126 | Map_20151_8 Dwarven Castle | Infirmary | Cid resting (Sleep Mark); a dwarf in the castle says "He's sleeping in the infirmary as we speak" (rule 6) |
| 148 | Map_20171_4 Tomra | House | generic Dwarf Soldier, six chests |

## FF5

| id | sub-map | label | evidence |
|---|---|---|---|
| 49 | Map_20031_1 Walse | House | generic man, Silver Specs |
| 94 | Map_20061_7 Karnak | House | generic scholar |
| 137, 139 | Map_20131_1, _3 Moogle Village | House | moogles (generic), chests |
| 138 | Map_20131_2 Moogle Village | Moogle Costume | entity モーグリの着ぐるみ, no NPC |
| 150 | Map_20161_3 Moore | Item Shop | ShopNPC with product_group 35 = "Items" (道具屋); MSG_ARA_NAME_02 |
| 170 | Map_20191_3 Castle of Bal | Item Shop | ShopNPC with product_group 36 = "Items"; MSG_ARA_NAME_02 |
| 185, 535, 536, 537, 540 | Phantom Village | House | generic Man (Blue Turban), one with a Healing Jar |
| 188 | Map_20211_2 ??? | House | generic Wise Old Man |
| 197 | Map_30021_6 Pirates' Hideout | Boko's Room | NPC ボコ Boko, event "Meet Boko" |
| 209 | Map_30041_8 Wind Shrine | Crystal | entity クリスタル, the Wind Crystal event, King Tycoon |
| 313 | Map_30221_3 Castle Exdeath | Healing Spring | entity 回復の泉 |
| 382 | Map_30421_1 Forest of Moore | Healing Spring | event 回復の泉, moogles |
| 471 | Map_30611_10 Kingdom of the Dwarves | House | generic dwarf |

Left unlabelled: the town main maps (Tule, Carwen, Karnak, Jachol, Crescent, Regole, Moore,
Quelb ...), Castle Tycoon 70, 71, 80, 81, Castle of Bal 167, Surgate 153, Wind Shrine 202 and 204,
save-point rooms, North Mountain, the Meteorite, Pirates' Hideout 195 and 196, the Rift, the
airship, Ghido's Cave, and the cutscene and world variants.

## FF6

South Figaro, Mobliz, Kohlingen, Thamasa and Maranda exist twice (World of Balance and World
of Ruin maps), so several rooms are labelled twice. Castle, airship (Blackjack, Falcon), Opera
House, Imperial Palace, Esper World, dungeon, dreamscape and cutscene rooms stay unlabelled.

| id | sub-map | label | evidence |
|---|---|---|---|
| 10 | Map_20011_7 Narshe | Back Room | reached only from the Weapon Shop; elder and Magicite, the resting event |
| 14 | Map_20011_11 Narshe | Village Chief's House | NPC NP村長 Village Chief, Banon, Jun, guards |
| 15 | Map_20011_12 Narshe | House | generic elder |
| 16 | Map_20011_13 Narshe | House | Lone Wolf is held there, but the house is nobody's: generic man, six chests |
| 17 | Map_20011_14 Narshe | Jun's House | event sc_e_0093 "conversation at the old man (Jun)'s house", Slave Crown |
| 25, 251 | Map_20051_4, Map_20371_4 Figaro Castle | Bedroom | two women, event "asked whether to rest" |
| 46, 56, 60, 61, 63, 65 | South Figaro (WoB) | House | generic villagers; 63 is Duncan's wife (a description, not a name); 65 the merchants |
| 83 | Map_20101_8 Doma Castle | Throne Room | NPC ドマ王 King of Doma with a soldier |
| 88, 89, 90 | Mobliz (WoB) | House | generic men, the letters |
| 96 | Map_20121_4 Nikeah | House | generic sailor |
| 100 | Map_20131_3 Kohlingen | Rachel's House | NPC レイチェル Rachel, Locke, the flashback event "at Rachel's house" |
| 102, 267 | Kohlingen | House | the red-clothed elder's house (Rachel's body); generic owner |
| 142, 278 | Maranda | Lola's House | NPC ローラ Lola, the flowers and letter |
| 145, 151 | Vector | House | generic old woman with guards |
| 185, 310 | Thamasa | Strago's House | NPC ストラゴス Strago, Relm, Interceptor |
| 186, 311 | Thamasa | Relm's Room | Relm, the Memento Ring, Strago as NPC: the upstairs room |
| 187, 312 | Thamasa | House | generic elder |
| 191 | Map_20291_1 Solitary Island | Cid's House | NPC シド Cid (bed), "Cid's Letter" |
| 210 | Map_20341_6 Mobliz (WoR) | Duane's House | NPC ディーン Duane, the dog and hidden passage |
| 212 | Map_20341_8 Mobliz (WoR) | Terra's House | NPC ティナ Terra with Katarin, Duane and the children |
| 225, 227, 237, 242, 244 | South Figaro (WoR) | House | generic villagers |
| 329 | Map_30031_1 South Figaro Cave | Healing Spring | entity 回復の泉 |
| 352 | Map_30421_2 Phantom Forest | Healing Spring | entity 回復の泉 |
| 34 | Map_20051_13 Figaro Castle | Throne Room | Edgar speaks there as "King" (speaker role), with the chancellor, Terra and Locke |
| 46 | Map_20071_8 South Figaro | Bedroom | event "a draft is heard in the master's room": the room off the mansion's titled 2F with the hidden passage |
| 611 | Map_20151_8 Jidoor | Owzer's Mansion | the paintings and the impresario; a townsman: "The house at the northern end of town belongs to Owzer"; MSG_MAP_NAME_40 is the game's name for it in the World of Ruin |

Town talk that was checked and led nowhere new: Nikeah's "thieves hanging out in the pub" names
the titled pub (217), not the untitled docks (218) where Gerad boards; Albrook's "lodging at
the inn" is said in Leo's room, the inn itself is titled; South Figaro's "mansion" is titled
"1F" and "2F" by the game.
