# Spoiler-free Events labels

All six games' extracted event, interactive-object, vehicle-action, layer-transition and switch candidates are indexed by `tools/audit_event_labels.py`. Inference records are in `docs/re/ffN_event_label_audit.json`; generated safe labels are in `data/ffN/eventlabels.json`.

The runtime Events category uses these mod descriptions exclusively. It never falls back to a developer name, future dialogue or a translated outcome. NPC names and ordinary exit destination names retain their separate readers.

- Conversation identifiers or scripts containing the `Msg` presentation command become **Conversation**. The script's dialogue, participants and outcomes are withheld.
- Explicit party-chat identifiers become **Party chat**.
- Bare inspection-object nouns retain their object description, such as **Clock**, **Door** or **Bookshelf**. A narrative containing the same noun does not qualify.
- Known native transition/action types become **Layer transition**, **Warp tile**, **Vehicle interaction** or **Switch trigger**.
- Other scripted triggers become **Scene trigger**; other inspection targets become **Interactive object**. Shared identifiers with conflicting inferences become **Event trigger**.

These labels deliberately avoid predicting a battle, a character's arrival/departure, an item obtained/lost or any event outcome. Position/direction remains available to distinguish nearby triggers. `event_label_overrides.json` may supply reviewed exact-name descriptions from the safe vocabulary; the generator checks that vocabulary. New/unmatched identifiers keep a neutral runtime fallback.

The audit covers 12,513 extracted candidate objects, including scenery later excluded by the native scanner. Generated labels cover every nonempty candidate identifier. Gameplay confirmation of their usefulness remains user-side.
