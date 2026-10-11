// Ports FF1 Core/Handlers/GlobalHotkeyHandler.cs.
//
// Helpers shared by the keyboard dispatch and the controller router: the vehicle announce and
// the I-key cascade (puzzle position, config tooltip, class description, shop / equipment /
// magic / item / battle descriptions).
#include "input_internal.h"
#include "../core.h"
#include "../refs.h"
#include "../unity.h"
#include "../say.h"
#include "../modtext.h"
#include "../text.h"
#include "../log.h"

namespace input {
namespace {

// Config menu tooltip (Last.UI.KeyInput / Last.UI.Touch ConfigActualDetailsControllerBase).
ref::Class kConfigDetailsKeyInput{"Last.UI.KeyInput", "ConfigActualDetailsControllerBase"};
ref::Field fDescriptionTextKeyInput{kConfigDetailsKeyInput, "descriptionText"};
ref::Class kConfigDetailsTouch{"Last.UI.Touch", "ConfigActualDetailsControllerBase"};
ref::Field fDescriptionTextTouch{kConfigDetailsTouch, "descriptionText"};

/// The active config details controller, or null. `key_input` says which of the two.
void* active_config_controller(bool& key_input)
{
    key_input = false;
    void* c = unity::find_object(kConfigDetailsKeyInput);
    if (c && unity::active_in_hierarchy(c)) {
        key_input = true;
        return c;
    }
    c = unity::find_object(kConfigDetailsTouch);
    if (c && unity::active_in_hierarchy(c)) return c;
    return nullptr;
}

void announce_config_tooltip(void* controller, bool key_input)
{
    if (!controller) return;
    void* text = ref::rd<void*>(controller, key_input ? fDescriptionTextKeyInput : fDescriptionTextTouch);
    if (!text) return;
    const std::wstring s = unity::ui_text(text);
    if (!text::is_blank(s)) ffpr::say(text::trim(s));
}

int install()
{
    actions::define("GlobalHotkeyHandler.AnnounceCurrentVehicle", [] { global_hotkeys::announce_current_vehicle(); });
    actions::define("GlobalHotkeyHandler.HandleItemDetailsKey", [] { global_hotkeys::handle_item_details_key(); });
    return 0;
}
core::Install s_install{"GlobalHotkeyHandler", &install, 60};

} // namespace

namespace global_hotkeys {

void announce_current_vehicle()
{
    // MoveStateHelper.GetMoveStateName(MoveStateHelper.GetCurrentMoveState()).
    ffpr::say(queries::text("MoveStateHelper.GetMoveStateName"));
}

void handle_item_details_key()
{
    if (queries::is("ReviewBuffer.IsActive")) { actions::run("ReviewBuffer.Details"); return; }
    // 15-puzzle: current cursor row/column.
    if (queries::is("PuzzleGameState.IsActive")) {
        actions::run("PuzzlePatches.AnnouncePosition");
        return;
    }

    // A game module's own screen with details (FF2's keyword and Words menus).
    if (queries::is("GameModule.HasDetails")) {
        const std::wstring desc = queries::text("GameModule.Details");
        ffpr::say(text::is_blank(desc) ? T(L"No description") : text::trim(desc), true);
        return;
    }

    // Config menu tooltip.
    bool key_input = false;
    if (void* config = active_config_controller(key_input)) {
        announce_config_tooltip(config, key_input);
        return;
    }

    // New-game class selection: the focused class description.
    if (queries::is("JobSelectionPatches.IsActive")) {
        const std::wstring desc = queries::text("JobSelectionPatches.GetCurrentDescription");
        ffpr::say(text::is_blank(desc) ? T(L"No description") : text::trim(desc), true);
        return;
    }

    // Shop item details.
    if (queries::is("ShopMenuTracker.ValidateState")) {
        actions::run("ShopDetailsAnnouncer.AnnounceCurrentItemDetails");
        return;
    }

    // Equipment menu details (stats/description from the game panel).
    if (queries::is("EquipMenuState.IsActive")) {
        actions::run("EquipDetailsAnnouncer.AnnounceCurrentItemDetails");
        return;
    }

    // Magic menu spell description.
    if (queries::is("MagicMenuState.IsSpellListActive")) {
        actions::run("MagicDetailsAnnouncer.AnnounceCurrentSpellDescription");
        return;
    }

    // Item menu description (equip requirements are on U).
    if (queries::is("ItemMenuState.IsItemMenuActive")) {
        actions::run("ItemDescriptionAnnouncer.AnnounceCurrentDescription");
        return;
    }

    // Battle magic description on demand (the only way to it with Auto Detail off).
    if (queries::is("BattleMagicMenuState.IsActive")) {
        actions::run("BattleMagicPatches.AnnounceCurrentDescription");
        return;
    }

    // Battle item description on demand.
    if (queries::is("BattleItemMenuState.IsActive")) {
        actions::run("BattleItemPatches.AnnounceCurrentDescription");
        return;
    }
}

} // namespace global_hotkeys
} // namespace input
