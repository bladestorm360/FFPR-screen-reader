// Ports FF1 Menus/ItemDescriptionAnnouncer.cs: the on-demand detail key of the item menu
// (I key: the focused item's rendered description). All five games.
#include "menus_internal.h"
#include "../core.h"
#include "../field/object_cache.h"
#include "../modtext.h"
#include "../say.h"
#include "../text.h"
#include "../unity.h"
#include "../displayed_ui.h"

namespace ffpr::menus {
namespace {
ref::Class kItemWindowOwner{"Last.UI.KeyInput", "ItemWindowController"};
ref::Class kItemWindowView{"Last.UI", "ItemWindowView"};
ref::Field fItemWindowViewDescriptionText{kItemWindowView, "descriptionText"};

/// ItemDescriptionAnnouncer.AnnounceCurrentDescription: the live item panel's description
/// (ItemWindowView.descriptionText), exactly what the game rendered.
void announce_current_description()
{
    std::wstring description;
    if (void* view = displayed::field(field::cache::get(kItemWindowOwner), "view"))
        if (void* text_component = ref::rd<void*>(view, fItemWindowViewDescriptionText))
            description = displayed::text(text_component);
    if (detail::blank(description)) {
        ffpr::say(T(L"No description"), true);
        return;
    }
    ffpr::say(text::trim(detail::strip_icon(description)), true);
}

int install()
{
    actions::define("ItemDescriptionAnnouncer.AnnounceCurrentDescription", [] { announce_current_description(); });
    return 0;
}

core::Install s_install{"Menus.ItemDetails", &install, 120};

} // namespace
} // namespace ffpr::menus
