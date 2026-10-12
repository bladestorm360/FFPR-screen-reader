#pragma once
#include <initializer_list>
#include <string>
#include <string_view>
#include <optional>
#include "json.h"

// Mod UI text (shared by all five games) and access to each game's embedded data.
//
// Lookup order for T(key): data/ffN/mod_text.json (the running game's overrides, for keys
// that contain game terms) -> data/common/mod_text.json -> English -> the key itself.
// Both files are built into the DLL as resources (src/data.rc.in).
//
// The language is the game's own (MessageManager.currentLanguage), read on the main thread
// and cached; refresh_language() runs at each title/config change.
namespace modtext {
/// Immutable localization selection for a copied worker job. Never reads native language.
class LocaleScope {
public:
    explicit LocaleScope(std::wstring language);
    ~LocaleScope();
    LocaleScope(const LocaleScope&) = delete;
private:
    std::optional<std::wstring> previous_;
};

/// Boot thread: parse the embedded files for the running game.
void init();

/// Mod UI text in the game's language.
std::wstring T(std::wstring_view key);
/// T(key) with {0}, {1}... filled.
std::wstring TF(std::wstring_view key, std::initializer_list<std::wstring> args);
/// True when the key exists in the common file or the game's overrides.
bool has(std::wstring_view key);

/// "ja" "en" "fr" "it" "de" "es" "ko" "zht" "zhc" "ru" "th" "pt" (FF1's map of the game's
/// Language enum: 1 = ja ... 12 = pt). "en" until the game has said otherwise.
const std::wstring& lang();
/// Main thread: re-read MessageManager.currentLanguage.
void refresh_language();

} // namespace modtext

// Short names for readers, as in the C# mods (`using static ModTextTranslator`).
inline std::wstring T(std::wstring_view key) { return modtext::T(key); }
inline std::wstring TF(std::wstring_view key, std::initializer_list<std::wstring> args) { return modtext::TF(key, args); }

namespace data {

/// Raw UTF-8 of an embedded resource for the running game: kind is "translation",
/// "entitynames", "modtext" (game overrides). Empty when the game ships no such file.
std::string game_file(const char* kind);
/// Raw UTF-8 of a shared resource ("MODTEXT_COMMON").
std::string resource(const char* name);
/// Worker-only preparation, with one atomic publication of immutable JSON tables.
void prepare_game_json();
bool game_data_ready();
/// Parsed game file; null when preparation is pending, absent or malformed. Never waits/parses.
const json::Value& game_json(const char* kind);

} // namespace data
