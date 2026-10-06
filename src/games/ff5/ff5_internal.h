#pragma once
// Helpers shared by the FF5 module's source files (src/games/ff5). Not for the core.
//
// Ports FF5 Utils/MenuFocusAnnouncer.cs (the initial-focus settle read) and the small FF5
// helpers several FF5 screens lean on (MessageManager lookups, MenuManager.IsOpen, the FF5
// MenuStateRegistry keys, PreferencesManager.AutoDetailEnabled).
//
// Main thread only, like the C# originals.
#include <functional>
#include <string>

namespace ffpr::ff5 {

// --- FF5 MenuStateRegistry keys the core does not define (FF5 Utils/MenuStateRegistry.cs) ------
constexpr const char* JOB_MENU = "JobMenu";
constexpr const char* ABILITY_MENU = "AbilityMenu";
constexpr const char* ABILITY_SLOT_MENU = "AbilitySlotMenu";
constexpr const char* ABILITY_EQUIP_MENU = "AbilityEquipMenu";
constexpr const char* ITEM_USE = "ItemUse";
constexpr const char* NAMING_MENU = "NamingMenu";

// --- MenuFocusAnnouncer -----------------------------------------------------------------------
namespace focus {

/// MenuFocusAnnouncer.Request: from the next frame on, call `try_announce` once per frame until
/// it returns true (it spoke) or MaxSettleFrames (6) frames have passed. A newer request (any
/// tag) supersedes an older one (the global generation latch). Logs a read that took more than
/// 2 frames, and a give-up.
void request(const char* tag, std::function<bool()> try_announce);
/// MenuFocusAnnouncer.Cancel: drop any in-flight request.
void cancel();
/// MenuFocusAnnouncer.IsMenuOpen: MenuManager.Instance.IsOpen (false during a map/asset load
/// and before the manager exists).
bool menu_open();
/// MenuFocusAnnouncer.IsAlive: the component (or GameObject) is alive and active in the
/// hierarchy.
bool alive(void* component);

} // namespace focus

// --- small shared helpers ---------------------------------------------------------------------

/// MessageManager.Instance.GetMessage(id) (isReplace true, the C# default). Empty on any miss.
std::wstring message(const std::wstring& mes_id);
/// MessageManager.GetMessage on a managed string id (copied out first).
std::wstring message_from(void* il2cpp_string_id);
/// PreferencesManager.AutoDetailEnabled ("AutoDetail", default on).
bool auto_detail();
/// string.IsNullOrWhiteSpace.
bool blank(const std::wstring& s);
/// A UnityEngine.UI.Text's text, trimmed; empty for null. (`text?.text?.Trim() ?? ""`)
std::wstring text_of(void* ui_text);
/// TextUtils.GetTextSafe (job and timer readers): the trimmed text only when the Text's
/// GameObject is active in the hierarchy and the text is not blank, else empty.
std::wstring text_if_shown(void* ui_text);
/// Last.UI.Cursor.Index, -1 for null.
int cursor_index(void* cursor);

// --- CharacterStatusHelper (FF5 Utils/CharacterStatusHelper.cs) -------------------------------
/// GetConditionTypeName: T() of the whitelisted fallback name for a user-visible ConditionType,
/// empty when the type is internal/hidden.
std::wstring condition_type_name(int condition_type);
/// GetVitalsString: "HP 100/200, MP 50/100" for a CharacterParameterBase, empty on null/fault.
std::wstring vitals_string(void* parameter);
/// GetStatusConditions: "Poison, Blind" (whitelisted types only; the condition's own name,
/// icon markup stripped, else the fallback name), empty when none.
std::wstring status_conditions(void* parameter);
/// GetFullStatus: ", HP 100/200, MP 50/100" plus ", Status: Poison, Blind"; empty on null.
std::wstring full_status(void* parameter);

} // namespace ffpr::ff5
