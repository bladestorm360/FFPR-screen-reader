// FF2's entry in the core's details key (I / right stick up): the keyword and Words menus
// speak the cached description (MenuDetailCache), so it is reachable with Auto Detail off.
#include "../../core/core.h"
#include "../../core/game.h"

namespace {

int install()
{
    queries::define("GameModule.HasDetails", [] {
        return queries::is("KeywordMenuState.IsActive") || queries::is("WordsMenuState.IsActive");
    });
    queries::define_text("GameModule.Details", [] { return queries::text("MenuDetailCache.LastDetail"); });
    return 0;
}

core::Install s_install{"FF2.DetailsKey", &install, 545, game::kFF2};

} // namespace
