#pragma once
#include <string>

// FF1 Utils/EntityTranslator.cs: Japanese map-object names (PropertyEntity.Name, GameObject
// names) translated to the game's language from the running game's own translation.json
// (data::game_json("translation")). Game content stays per game. Main thread.
namespace field {

/// EntityTranslator.Translate: the name in the game's language, or the name unchanged.
std::wstring translate_entity_name(const std::wstring& japanese_name);
/// EntityTranslator.ContainsJapanese (hiragana, katakana, CJK ideographs).
bool contains_japanese(const std::wstring& s);
/// EntityTranslator.ClearUntranslatedTracking (MapTransitionPatches.ResetMapTracking calls it).
void clear_untranslated_tracking();
/// EntityTranslator.TranslationCount.
int translation_count();
/// .NET char.IsDigit for the scripts the games use (ASCII and full-width digits).
bool is_decimal_digit(wchar_t c);

} // namespace field
