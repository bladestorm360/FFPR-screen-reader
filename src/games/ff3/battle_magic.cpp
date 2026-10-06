// Ports FF3 Patches/BattleMagicPatches.cs (BattleMagicMenuState, BattleMagicSelectContent_Patch).
//
// The battle spell list, FF3's class Serial.FF3.UI.KeyInput.BattleFrequencyAbilityInfomationController.
// The core's battle magic (src/core/battle/battle_magic.cpp) is FF1's class only: in FF3 the
// SetCursor override FF1 hooks is an empty stub folded with ~2,500 methods, so FF3 hooks
// SelectContent(List<BattleAbilityInfomationContentController> contents, int index, Cursor)
// (0x4FA220, unique), as the FF3 mod did.
//
// Parity decisions (FF1 wording): "Fire Lv1: MP: 3/5. description (2 of 8)" (FF3 said
// "Fire: MP: 3/5. description"); the line check is FF1's: a fresh entry always speaks, a
// re-fire of the same line while the list is already focused is skipped ("BattleMagic.Spell").
// Empty slots read "Empty" and are compared the same way.
// State: FF1's ClearOtherMenuStates("BattleMagic") then BATTLE_MAGIC active. The command
// window's back-out re-announce (FF3 NotifyCommandSubmenuActive) is run through
// "BattleCommandPatches.NotifyCommandSubmenuActive" when the battle area defines it.
#include <cstdint>
#include <string>
#include "../../core/core.h"
#include "../../core/dedup.h"
#include "../../core/hooks.h"
#include "../../core/log.h"
#include "../../core/modtext.h"
#include "../../core/refs.h"
#include "../../core/say.h"
#include "../../core/settings.h"
#include "../../core/text.h"
#include "../../core/ui_util.h"
#include "../../core/unity.h"
#include "../../core/menus/menus.h"
#include "../../core/status/status.h"

namespace ffpr::ff3 {
namespace {

ref::Class kFreqBase{"Serial.Template.UI.KeyInput", "BattleAbilityInfomationControllerBase", game::kFF3};
ref::Field fSelectedPlayer{kFreqBase, "selectedBattlePlayerData"};
ref::Class kFreq{"Serial.FF3.UI.KeyInput", "BattleFrequencyAbilityInfomationController", game::kFF3};
ref::Method mSelectContent{kFreq, "SelectContent", 3};

ref::Class kAbilityContent{"Last.UI.KeyInput", "BattleAbilityInfomationContentController", game::kFF3};
ref::Field fContentData{kAbilityContent, "<Data>k__BackingField"};

ref::Class kBattlePlayer{"", "BattlePlayerData", game::kFF3};
ref::Field fOwnedCharacter{kBattlePlayer, "<ownedCharacterData>k__BackingField"};

constexpr const char* kCtxSpell = "BattleMagic.Spell";

std::wstring g_last_description;   // BattleMagicMenuState.LastFocusedDescription

void run_if_defined(const char* action)
{
    if (actions::defined(action)) actions::run(action);
}

/// "Name Lv1: MP: c/m. description"; `description` gets the stripped description.
std::wstring format_spell(void* controller, void* ability, std::wstring& description)
{
    const std::wstring name = ffpr::menus::spell_name(ability);
    if (name.empty()) return {};
    std::wstring line = name;
    const int level = ffpr::menus::spell_level(ability);
    if (level > 0 && level <= 8) {
        line += L" " + TF(L"Lv{0}", {text::num(level)});
        void* player = ref::rd<void*>(controller, fSelectedPlayer);
        void* owned = (player && kBattlePlayer.is(player)) ? ref::rd<void*>(player, fOwnedCharacter) : nullptr;
        if (owned) {
            int current = 0, max = 0;
            ffpr::menus::charges_for_level(owned, level, current, max);
            if (max > 0) line += L": " + TF(L"MP: {0}/{1}", {text::num(current), text::num(max)});
        }
    }
    description = ffpr::menus::spell_description(ability);
    if (settings::get_bool("AutoDetail", true) && !text::is_blank(description)) line += L". " + description;
    return line;
}

void on_select_content(void* self, void* contents, int index)
{
    if (!self || !contents) return;
    const int count = unity::list_count(contents);
    if (index < 0 || index >= count) return;
    void* content = unity::list_at(contents, index);
    void* ability = content ? ref::rd<void*>(content, fContentData) : nullptr;

    std::wstring description;
    std::wstring line = ability ? format_spell(self, ability, description) : T(L"Empty");
    if (line.empty()) return;
    line = ui::menu_position(line, index, count);

    const bool was_active = menu_state::is_active(menu_state::BATTLE_MAGIC);
    if (was_active) {
        if (!dedup::text(kCtxSpell, line, dedup::Scope::Session)) return;
    } else {
        dedup::reset(kCtxSpell);
        dedup::text(kCtxSpell, line, dedup::Scope::Session);
    }
    run_if_defined("Mod.ClearOtherMenuStates.BattleMagic");
    menu_state::set_active(menu_state::BATTLE_MAGIC, true);
    g_last_description = description;
    run_if_defined("BattleCommandPatches.NotifyCommandSubmenuActive");
    ffpr::say(line, true);
}

using SelectContent_t = void (*)(void* self, void* contents, int32_t index, void* cursor, const void* mi);
SelectContent_t o_SelectContent;

void d_SelectContent(void* self, void* contents, int32_t index, void* cursor, const void* mi)
{
    o_SelectContent(self, contents, index, cursor, mi);
    hooks::guarded("FF3.BattleMagic.SelectContent", [&] { on_select_content(self, contents, index); });
}

int install()
{
    const int n = hooks::attach(mSelectContent, &d_SelectContent, &o_SelectContent) ? 1 : 0;
    if (!n) LOGF("[FF3 BattleMagic] SelectContent not hooked - battle spell list silent");
    actions::define("BattleMagicPatches.AnnounceCurrentDescription", [] {
        ffpr::say(text::is_blank(g_last_description) ? T(L"No description") : text::trim(g_last_description), true);
    });
    events::on(events::Kind::BattleStart, [] { g_last_description.clear(); });
    return n;
}

core::Install s_install{"FF3.BattleMagic", &install, 506, game::kFF3};

} // namespace
} // namespace ffpr::ff3
