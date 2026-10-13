# Equipment, shops and result detail follow-up, October 13

Implemented, offline-verified and deployed/hash-verified in all six games.
Gameplay confirmation for these changes remains pending.
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
announce character comparisons: name only with Autodetail off; name plus current bottom
panel with it on; I reads that panel in either case.

Layout: the left menu has Items, Magic, Equip, Status, Formation, Key Terms,
Configuration, Quick Save, Save, Load. The top equipment commands are Equip, Optimal,
Remove All. In the replacement screenshot the inventory shows Mythril Spear 2,
Mythril Mace 3, Battle Axe 1, Longsword 1, Spear 1, Longbow 2. The right panel shows
Leila, the right-hand item being replaced, her sprite and her other equipment.
The comparison is below those panels; the switchable information panel is beneath it.

## Reader ownership and controls

- EquipmentInfoWindowController.SelectContent owns slot focus; set Focus::Slot before
  the original and read the rendered equipTextView.nameText after it. Native nested
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
- Battle / Stat delta detail: persistent Verbose (0, default) / Brief (1). Verbose
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
remote floor names, native collected/story filtering and distinct chest identities remain.
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
