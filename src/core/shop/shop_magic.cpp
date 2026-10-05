// Ports FF1 Patches/ShopMagicPatches.cs.
//
// The magic shop's spell-slot grid (FF1's slot system): character and slot navigation, and the
// "no characters can learn this spell" notice. ShopMagicTargetSelectController and
// ShopGetMagicContentController exist with identical layouts in all five games, so the hooks are
// registered for all; they only fire where a game opens this grid. No hooked body is folded in
// any game.
#include "shop.h"
#include "shop_internal.h"
#include "../core.h"
#include "../dedup.h"
#include "../hooks.h"
#include "../log.h"
#include "../modtext.h"
#include "../say.h"
#include "../text.h"
#include "../ui_util.h"
#include "../unity.h"

namespace ffpr::shop {
namespace {

ref::Class  kTargetSelect{"Last.UI.KeyInput", "ShopMagicTargetSelectController"};
ref::Method mSelectContent{kTargetSelect, "SelectContent", 2};
ref::Method mSetFocus{kTargetSelect, "SetFocus", 1};
ref::Method mShow{kTargetSelect, "Show", 5};
ref::Field  fIsFoundEquipSlot{kTargetSelect, "isFoundEquipSlot"};

ref::Class  kSlot{"Last.UI.KeyInput", "ShopGetMagicContentController"};
ref::Field  fSlotCharacterId{kSlot, "<CharacterId>k__BackingField"};
ref::Field  fSlotType{kSlot, "<SlotType>k__BackingField"};
ref::Field  fSlotContentId{kSlot, "<ContentId>k__BackingField"};

ref::Class  kContent{"Last.Data.Master", "Content"};
ref::Method mContentMesIdName{kContent, "get_MesIdName", 0};

ref::Class  kUserDataManager{"Last.Management", "UserDataManager"};
ref::Method mUdmInstance{kUserDataManager, "Instance", 0};
ref::Method mGetOwnedCharactersClone{kUserDataManager, "GetOwnedCharactersClone", 1};
ref::Class  kOwnedCharacter{"Last.Data.User", "OwnedCharacterData"};
ref::Method mCharId{kOwnedCharacter, "get_Id", 0};
ref::Method mCharName{kOwnedCharacter, "get_Name", 0};

// FF1 lastAnnouncedCharacterId: the character name prefixes a slot only when the character
// changes. Session scope: cleared only by this file's resets, as in FF1.
constexpr const char* kCharacterCtx = "Shop.MagicCharacter";

// FF1 announcedNoLearnersThisSession. Kept as it is: Show resets it just before the check, so
// it never suppresses (DEDUP_INVENTORY.md, "Guards that never suppress anything").
bool g_announced_no_learners = false;

std::wstring character_name_by_id(int character_id)
{
    void* udm = ref::call_static<void*>(mUdmInstance);
    if (!udm) return {};
    void* party = ref::call<void*>(mGetOwnedCharactersClone, udm, false);
    if (!party) return {};
    int n = unity::list_count(party);
    for (int i = 0; i < n; ++i) {
        void* chara = unity::list_at(party, i);
        if (chara && ref::call<int32_t>(mCharId, chara) == character_id)
            return ref::call_str(mCharName, chara);
    }
    return {};
}

std::wstring spell_slot_name(void* slot)
{
    int content_id = ref::rd<int32_t>(slot, fSlotContentId);
    if (content_id == 0) return {};
    void* content = master_get_data(kContent, content_id);
    if (!content) return {};
    std::wstring mes_id_name = ref::call_str(mContentMesIdName, content);
    if (mes_id_name.empty()) return {};
    std::wstring ability_name = get_message(mes_id_name);
    if (ability_name.empty()) return {};
    return strip_icon_markup(ability_name);
}

// --- SetFocus(bool) postfix: grid focus resets the character tracking ------------------------
using SetFocus_t = void (*)(void* self, bool is_focus, const void* mi);
SetFocus_t o_SetFocus = nullptr;

void d_SetFocus(void* self, bool is_focus, const void* mi)
{
    o_SetFocus(self, is_focus, mi);
    hooks::guarded("ShopMagicPatches.SetFocus", [&] {
        if (!is_focus) return;
        set_shop_menu_active(true);
        set_in_magic_slot_selection(true);
        dedup::reset(kCharacterCtx);
    });
}

// --- Show(ShopProductData, List<OwnedCharacterData>, Action, Action<string>, Action) postfix ---
// The grid opens: isFoundEquipSlot false means no character can learn the spell.
using Show_t = void (*)(void* self, void* data, void* character_list, void* on_action, void* on_selected,
                        void* on_cancel, const void* mi);
Show_t o_Show = nullptr;

void d_Show(void* self, void* data, void* character_list, void* on_action, void* on_selected, void* on_cancel,
            const void* mi)
{
    o_Show(self, data, character_list, on_action, on_selected, on_cancel, mi);
    hooks::guarded("ShopMagicPatches.Show", [&] {
        if (!self) return;
        set_shop_menu_active(true);
        set_in_magic_slot_selection(true);
        dedup::reset(kCharacterCtx);
        g_announced_no_learners = false;

        bool found = ref::rd<bool>(self, fIsFoundEquipSlot);
        if (!found && !g_announced_no_learners) {
            g_announced_no_learners = true;
            ffpr::say(T(L"No characters can learn this spell"), true);
        }
    });
}

// --- SelectContent(Cursor, List<ShopGetMagicContentController>) postfix: slot navigation -----
using SelectContent_t = void (*)(void* self, void* target_cursor, void* content_list, const void* mi);
SelectContent_t o_SelectContent = nullptr;

void d_SelectContent(void* self, void* target_cursor, void* content_list, const void* mi)
{
    o_SelectContent(self, target_cursor, content_list, mi);
    hooks::guarded("ShopMagicPatches.SelectContent", [&] {
        set_shop_menu_active(true);
        set_in_magic_slot_selection(true);

        if (!self || !target_cursor || !content_list) return;

        int cursor_index = ui::cursor_index(target_cursor);
        int count = unity::list_count(content_list);
        if (cursor_index < 0 || cursor_index >= count) return;

        void* slot = unity::list_at(content_list, cursor_index);
        if (!slot) return;

        int character_id = ref::rd<int32_t>(slot, fSlotCharacterId);
        int slot_type = ref::rd<int32_t>(slot, fSlotType);

        std::wstring spell_name = spell_slot_name(slot);
        if (spell_name.empty()) spell_name = T(L"Empty");

        std::wstring announcement;
        if (dedup::index(kCharacterCtx, character_id, dedup::Scope::Session)) {
            std::wstring character_name = character_name_by_id(character_id);
            if (character_name.empty()) character_name = TF(L"Character {0}", {text::num(character_id)});
            announcement = TF(L"{0}: Slot {1}: {2}", {character_name, text::num(slot_type), spell_name});
        } else {
            announcement = TF(L"Slot {0}: {1}", {text::num(slot_type), spell_name});
        }

        ffpr::say(ui::menu_position(announcement, cursor_index, count), true);
    });
}

int install()
{
    int n = 0;
    n += hooks::attach(mSelectContent, &d_SelectContent, &o_SelectContent) ? 1 : 0;
    n += hooks::attach(mSetFocus, &d_SetFocus, &o_SetFocus) ? 1 : 0;
    n += hooks::attach(mShow, &d_Show, &o_Show) ? 1 : 0;
    LOGF("[Shop] %d magic shop hooks", n);
    return n;
}

core::Install s_install{"ShopMagicPatches", &install, 100};

} // namespace

/// ShopMagicPatches.ResetState (back to the item list, shop close).
void magic_reset_state()
{
    dedup::reset(kCharacterCtx);
    g_announced_no_learners = false;
}

} // namespace ffpr::shop
