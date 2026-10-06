#pragma once
// FF2 module helpers shared by the files of src/games/ff2. Ports the small FF2 pieces the core
// does not have: Core/MenuDetailCache.cs, the FF2-only MenuStateRegistry keys and its
// SetActiveExclusive (which keeps MAIN_MENU), and Utils/LocalizationUtility.GetMessage.
// Main thread only.
#include <string>
#include <string_view>

namespace ffpr::ff2 {

// FF2 MenuStateRegistry keys that FF1 does not have.
constexpr const char* KEYWORD_MENU = "KeywordMenu";
constexpr const char* WORDS_MENU = "WordsMenu";
constexpr const char* NEW_GAME_NAMING = "NewGameNaming";

/// FF2 MenuStateRegistry.SetActiveExclusive: every other active state goes inactive (its reset
/// handler runs) except MAIN_MENU, then `key` is set active. The shop tracker is not in the
/// registry here, so it is cleared separately, as the core's ClearOtherMenuStates does.
void set_active_exclusive(const char* key);

/// LocalizationUtility.GetMessage(mesId, stripIcons: false): MessageManager.GetMessage(id, false),
/// empty on a miss or a blank result.
std::wstring message(const std::wstring& mes_id);
/// TextUtils.StripIconMarkup: tags without whitespace inside removed, then trimmed.
std::wstring strip_icons(std::wstring_view s);
/// MenuManager.Instance.IsOpen (false when there is no manager).
bool menu_manager_open();

/// MenuDetailCache: the description the I key reads for FF2's own lists (keywords, Words).
void set_menu_detail(const std::wstring& detail);
const std::wstring& menu_detail();

} // namespace ffpr::ff2
