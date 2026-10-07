#pragma once
// FINAL FANTASY VI field ability menus (magic, espers, Blitz / Lore / Rage / Dance / Tools ...,
// Gogo's command change): what the ability_*.cpp and blitz_data.cpp files share.
//
// Ported from FF6ScreenReader (BlindGuyNW/FF6ScreenReader, by ZKline), with its author's
// permission. Main thread only, like the C# originals.
#include <string>

namespace ffpr::ff6 {

/// BlitzData.TryGetBlitzSequence: "Blitz input: Left, Right, Left, then Confirm" for a Blitz
/// ability's MesIdName (MSG_MAGIC_NAME_109 ... 116). False (and `sequence` empty) for any other id.
bool try_get_blitz_sequence(const std::wstring& mes_id_name, std::wstring& sequence);

// --- ability_menu.cpp ---------------------------------------------------------------------------

/// PreferencesManager.AutoDetailEnabled ("AutoDetail", default on), as FF1/FF4/FF5 use it.
bool auto_detail();

/// The focused entry for the I key: `controller` (identity, re-checked alive and active) and the
/// description the details key reads (empty = "No description"). A Blitz's MesIdName makes the
/// I key speak its input sequence instead.
void set_ability_focus(void* controller, const std::wstring& description, const std::wstring& blitz_mes_id = {});

// --- ability_espers.cpp -------------------------------------------------------------------------

/// MagicStoneDetailsController_Show_Patch.TryReannounceEsperDetails, as text: the last full esper
/// details line while that details controller is still active in the hierarchy; empty otherwise.
std::wstring esper_details_reannounce_text();

/// True when `cursor` (a Last.UI.Cursor) is one of the esper screens' own cursors (list or
/// details), for the generic cursor reader's suppressor.
bool is_esper_cursor(void* cursor);

/// Forget the esper list's last line (the list is being (re)entered).
void reset_esper_list_guard();

} // namespace ffpr::ff6
