# Equipment, shops and result detail follow-up, October 13

Implemented, offline-verified and deployed/hash-verified in all six games.
The user confirmed Verbose/Brief modes work on October 13. Other new follow-up cases remain pending.
Sol owns implementation, native investigation and verification. No game was launched or probed.

## User screenshot evidence: FF2

The replacement chooser selects Spear (quantity 1) for Leila's right hand, currently Dagger.
Its character comparison panel shows actual before/after numbers as well as arrows:

| Statistic | Before | Proposed | Visible arrow |
|---|---:|---:|---|
| Attack | 26 | 30 | Green up |
| Defense | 21 | 21 | None |
| Magic Defense | 39 | 39% | None |
| Accuracy | 68 | 68% | None |
| Evasion | 59 | 53% | Red down |
| Magic Interference | 56 | 91 | Red up |

The percentage suffix is printed after the proposed value where shown; it applies to
that statistic. An increase in Magic Interference is undesirable, hence the red up arrow.
The bottom panel in the first screenshot is the item's own bonuses: Attack +11,
Accuracy +55, Evasion +2, Magic Interference +40. Those are not the replacement's net
changes (+4 attack, -6 evasion, +35 interference). The second screenshot changes only
this bottom panel to "A metal-tipped spear." The native Q / controller X/Square toggle
owns which panel text I reads.

The separate slot screenshot selects Dagger in R. Hand. L. Hand (Favored Hand) contains
Mythril Sword; Arms None, Head Gold Hairpin, Body Silver Cuirass. Current character
stats are Attack 33, Defense 21, Magic Defense 39%, Accuracy 68%, Evasion 59%, Magic
Interference 56. There are no proposed values or arrows here. Its bottom item panel
shows Attack +7, Accuracy +65, Evasion +2, Magic Interference +5. Slot focus must never
announce character comparisons: slot label plus item name with Autodetail off; those plus
current bottom panel with it on; I reads that panel in either case. The user's subsequent
screenshot shows R. Hand, L. Hand (Favored Hand), Arms, Head and Body as rendered text.
The displayed Favored Hand qualification belongs in the automatic slot readout in both
Autodetail states, including when both hands contain the same weapon.

Layout: the left menu has Items, Magic, Equip, Status, Formation, Key Terms,
Configuration, Quick Save, Save, Load. The top equipment commands are Equip, Optimal,
Remove All. In the replacement screenshot the inventory shows Mythril Spear 2,
Mythril Mace 3, Battle Axe 1, Longsword 1, Spear 1, Longbow 2. The right panel shows
Leila, the right-hand item being replaced, her sprite and her other equipment.
The comparison is below those panels; the switchable information panel is beneath it.

## Reader ownership and controls

- EquipmentInfoWindowController.SelectContent owns slot focus; set Focus::Slot before
  the original and read the rendered partText, optionText and equipTextView.nameText after
  it. FF5/FF6's displayed bothHandsText is also retained; hidden qualifications are omitted.
  Native nested
  EquipmentStatusController.SetDifferenceData callbacks cannot disclose deltas here.
- Replacement SelectContent / SetDifferenceData captures bounded rendered comparisons
  into the shared Information buffer. Arrows review it; WASD keeps native selection.
  Normal controller D-pad/left stick remains native selection; controller mod-mode
  D-pad/shoulders review the buffer. I/right-stick up/West details reads the current
  information panel, including while comparison capture/worker assembly is pending.
- Controller scripts may be disabled without hiding their graphics. Container ownership
  uses live active hierarchy; text/image eligibility still checks the renderer.
- Capture uses the shared 0.5 ms elapsed budget between native calls, at most 64 stat
  rows; only owned strings enter the review worker. Highlights/close/settings cancel
  obsolete capture/publication. No worker wait or new speech deduplication is added.
- Shop focus uses name only when Autodetail is off, name and its actual displayed
  ShopInfoView.descriptionText when on; I reads that same current toggled panel.
  Existing native item-index event guard remains. Shop party comparison supplies
  stronger/weaker/equipped/impossible icons, not numeric before/after statistics.
- Items -> equipment-details is a real native path in all six games, not only FF6.
  ItemWindowController.EquipmentInit activates it; UpdateView reaches detail SetData,
  SetPage refreshes the current page, and state Equipment is 7 in KeyInput UI.
  The extra buffer contains visible status, ability, magic and elemental-icon rows,
  plus the current parent description. Attribute SetAllContentEnable/SetEnableAttribute observe highlighted icons;
  disclosure also requires their rendered image. Four lists are bounded to 64 rows
  each, icons to 16 per row and observed row masks to 128 entries/scene. FF1–FF5 change icon color; FF6
  swaps enabled/disabled sprites. Gray icons are not item properties.

Exact all-six native/schema exports remain local in ignored build_check/re/:
ffN_equipment_schema.txt, ffN_equipment_detail_schema.txt,
ffN_equipment_party_airship.txt and ffN_vehicle_input.txt. The latter two exports
were produced by bounded Capstone reads of named, matching-dump native functions;
no whole-scene capture or runtime inspection was used.

## Battle party queries

BattlePlayerInfomationController.Initalize, UpdateView(players, isTarget), and
SetFocus(player, focused) identify the current native party owner and acting player.
Read its active ordinary/target rows and rendered name/HP/MP/condition graphics;
never use hidden parameter values as substitutes. H matches the native focused or
executing player. Number keys preserve list slot indices even if an intervening row
is hidden; cycles skip those empty slots. Party row count is bounded to nine.

## Speech preferences and essential messages

- Speech / Status damage: persistent default on. Only a preference check was added to
  the established deferred, visible field-damage producer. No new cooldown, content
  deduplication, damage suppression or eligibility guard was introduced.
- Speech / Stat delta detail: persistent Verbose (0, default) / Brief (1). Verbose
  retains before/after readings; Brief uses gains/losses from complete rendered
  numbers and their visible units, retaining character/hand/weapon identity. FF2
  EvasionRate/AbilityDefense tables use the primary percentage plus a secondary
  multiplier: a secondary-only gain uses x, not %. Raw item bonuses/descriptions
  are not replacement deltas and keep their own native panel text.
- Native displayed learned/level-up rows are essential summary/buffer entries in every
  Autodetail state. Job-mastery message fields replace numeric growth rows and are
  treated as essential messages, using native field eligibility, not a text heuristic.
  Current native phase/visibility/generation guards remain; no later rewards are exposed.

## Treasure contents approval

User reported remote Castle Deist entries revealing Hellfire, Egil's Torch and other
unopened rewards. Native live treasure detection and remote baked naming formerly used
contents. Default-off Speech / Spoil treasure chest contents now gates both name
resolution paths. Entity::display_name also masks cached chest names immediately when
off; selected route/beacon target names use that method. Live opened/unopened state,
remote floor names, native story filtering and distinct chest identities remain. Current
map now retains collected other-floor chests and labels their opened state from the copied
native treasure flags. Missing flag/mapping stays unspecified, rather than Unopened.
Opting in permits the actual derived PropertyTresureBox.ContentId and baked ContentId
names; it does not authorize unrelated hidden information.

## Delegation and verification status

Bounded gpt-6-luna, medium: preferences/menu/12-locale labels for Status damage,
Stat delta detail, Spoil treasure chest contents and two gain/loss format strings.
Luna reported JSON/locale/placeholder/diff checks, no builds or runtime checks. Sol
reviewed the changes and owns core behavior/integration and final regression checks.
No Astra delegation. Canonical build passes: 7,077 native-name checks, zero missing; 122 used / 327
defined action/query names, zero undefined. All actual-source offline regressions and
isolated DLL smoke pass; all six deployments match SHA-256 `B09ED487633FF7075C63480673D050A231C0635E7D511345DA48CCA715BE21E6`.
Prism prerequisites were verified read-only and unchanged. Gameplay is distinct and
remains user-side.

Actual-source checks cover all-six slot/candidate/extra-item/shop Autodetail and live-I
behavior, nested hidden previews, page refresh, visible elemental highlights versus gray
icons, disabled party scripts and stable number slots, status warnings default/on/off
with original damage calls and repeated valid events, and live/remote chest masking.
Results checks cover always-automatic learning/level-up text and buffer retention,
phase-wide I, Brief numeric/multiplier/weapon changes, optional-detail settings,
owner/generation cancellation and ten manager/listener input gates. Native-input
checks cover planned confirm/back, boarding waits/handback, exact action-cell rejection,
manual cancellation, inverse FF6 camera rotation and shoulder-state restoration.

The first fixtures exposed harness startup/signature omissions; those were repaired.
The all-six name check exposed FF6-only SetEnable; bounded native inspection established
color producers in FF1–FF5 and sprite producers in FF6, now handled through the common
SetAllContentEnable/SetEnableAttribute contract. On-demand tests caught a renamed action
key in the extra pane and it was corrected before deployment. All final checks pass.

Current whole-world offline benchmark: 24 worlds, 16 far routes; worst flood 6.413 ms
and route 50.861 ms. The boarded-world suite covers 103 eligible world/transport cases
(85 have no eligible start/landing destination): worst route 26.751 ms, filter 10.637 ms.
These are fixture compute measurements with ordinary run-to-run/machine-load variance,
not comparable target-game/unmodded baselines or audible/Unity callback timings. No
universal microsecond response claim is made; native capture, worker queue and audible
response remain distinct. The user has already confirmed prior pathfinding responsiveness
and smooth battle transitions; this does not confirm new automated vehicle controls.

## Subsequent slot/settings/treasure/Wyvern correction, October 13

User gameplay evidence confirms Verbose and Brief modes work; the setting moved from
Battle to Speech because it also governs equipment comparisons. StatDeltaMode key,
options, default and setting-change handlers are unchanged; saved Brief stays Brief.
Actual-source modal fixtures check the single Speech entry and saved preference in all six.

The previous item-name-only slot policy omitted rendered labels. The new native owner
read captures partText and optionText in all six, plus bothHandsText in FF5/FF6, before
the item name. Render eligibility owns Favored Hand and both-hands qualifications.
Autodetail adds only the current native item panel; I remains panel-only. Existing slot
comparison exclusion and hidden nested-preview guards remain. Actual-source fixtures
cover all five supplied slot labels, identical hand items, visible/hidden Favored Hand,
both-hands labels, both Autodetail states, I/page toggles and no slot deltas.

RemoteObject now carries optional opened state from Resolver::treasure_opened, matched
by baked map/entity ID/story group to the extracted native GetFlag treasure bit. The
already-owned flag snapshot enters the Navigation worker; no extra native traversal,
worker engine read or synchronous pathfinder operation is added. Current map used to
exclude collected remote chests entirely; it now retains and labels them. Unknown state
retains the generic chest name; content disclosure remains independently default off.
Live FieldTresureBox.isOpen continues to own same-floor state. All-six real bake/flag
fixtures exercise zero, set and unavailable bits; real connected-floor projection checks
retain an opened chest on every game without native worker access.

FF2's raw-name placeholder policy excluded every 飛竜/Wyvern, including actual NPCs.
The extracted Deist room is Map_30061_5, map master 180: active native NPCs have action 2
and script IDs 189 (sc_e_0166) or 65 (sc_e_0050). The matching extracted scripts contain
3 and 5 Msg commands respectively. An actionable FieldNonPlayer with a script or direct
message now bypasses the name-only placeholder filter; native CanAction, active scene,
vehicle, lifetime and story-group gates remain. Remote NPC projection carries the existing
baked script flag for the same positive inclusion. Scriptless/message-less Wyvern vehicle
and cutscene placeholders remain excluded. Classification does not inspect future dialogue
text or disclose it before interaction.

Sunfire's 太陽の炎トリガー用 is a native Entity with script 38 (sc_e_0034), Goddess's Bell's
女神のベルトリガー用 is Entity with script 29 (sc_e_0028). Both scripts contain Msg, but those
objects retain the Event category and safe approved object labels; dialogue alone does not
turn objects into NPCs. Actual-source scanner fixtures cover scripted/message-only Wyvern,
non-actionable NPCs, placeholder exclusion, keyword objects and remote naming/conversion.
The source/script JSONL and matching masters remain in ../tools/mapdump; no runtime probe.

Sol implemented and reviewed this narrow follow-up independently; no new delegation.
Final follow-up canonical build passes: 7,077 native names, zero missing; 122 used/327
defined action/query names, zero undefined. All actual-source checks pass, including
all-six connected-floor state, 1,886 saved chest mappings (8 unmapped chests stay unknown),
slot/favored-hand/detail behavior, Speech setting persistence and the Wyvern/keyword rules.
The matching FF2 metadata confirms PropertyNpc -> PropertyTalk -> PropertyEvent, so
script/message reads use actual inherited field ownership. Initial fake-native fixtures
omitted the property/getter contract and supplied the wrong keyword-object name; corrected
fixtures now pass. A missing timing-header include and a move-only fixture result were also
caught and repaired before deployment; the production handoff retains copyable owned data.
Isolated smoke passes all 47 exports/forwarding/boot/logger checks. All six deployments
match SHA-256 `E3F6C174CAE0FE65D969E3258B72936035EBA579BE051084B1BBB0845D0602BE`.
Prism was verified read-only and unchanged; no game was launched or probed.
[Loading dispatcher evidence and remaining Deist retest](THREADING.md#large-map-loading-dispatcher-follow-up-october-13).
