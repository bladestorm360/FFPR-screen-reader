// Ports FF1 Utils/EntityTranslator.cs.
//
// The C# regular expressions are written out by hand; each comment gives the pattern it
// implements. Language detection is modtext::lang() (the same Language-enum map FF1 used).
#include <map>
#include <set>
#include "entity_translator.h"
#include "field_internal.h"
#include "map_names.h"
#include "../log.h"
#include "../modtext.h"
#include "../text.h"

namespace field {
namespace {

bool g_initialized = false;
const json::Value* g_translations = nullptr;   // object: japanese -> { lang: text }
int g_count = 0;
std::map<std::wstring, std::set<std::wstring>> g_untranslated_by_map;

void initialize()
{
    if (g_initialized || !data::game_data_ready()) return;
    g_initialized = true;
    const json::Value& v = data::game_json("translation");
    if (!v.is_object()) {
        LOGF("[EntityTranslator] Embedded translation resource not found");
        return;
    }
    g_translations = &v;
    for (const auto& entry : v.obj) {
        if (!entry.second.is_object()) continue;
        for (const auto& lang : entry.second.obj)
            if (lang.second.is_string() && !lang.second.s.empty()) { ++g_count; break; }
    }
    LOGF("[EntityTranslator] Loaded %d translations from embedded resource", g_count);
}

bool circled(wchar_t c) { return c >= 0x2460 && c <= 0x2473; }   // ① .. ⑳

/// TryLookup: the entry in the game's language, else English.
bool try_lookup(const std::wstring& key, std::wstring& out)
{
    if (!g_translations) return false;
    const json::Value* entry = g_translations->get(key);
    if (!entry || !entry->is_object()) return false;
    const std::wstring& lang = modtext::lang();
    if (const json::Value* loc = entry->get(lang); loc && loc->is_string() && !loc->s.empty()) {
        out = loc->s;
        return true;
    }
    if (lang != L"en") {
        if (const json::Value* en = entry->get(L"en"); en && en->is_string() && !en->s.empty()) {
            out = en->s;
            return true;
        }
    }
    return false;
}

/// CircledDigitToNumber: "⑭" -> "14", several -> space separated.
std::wstring circled_to_number(const std::wstring& s)
{
    std::wstring out;
    for (wchar_t c : s) {
        if (!circled(c)) continue;
        if (!out.empty()) out.push_back(L' ');
        out += std::to_wstring((c - 0x2460) + 1);
    }
    return out;
}

/// NormalizeParens: full-width parentheses to half-width.
std::wstring normalize_parens(std::wstring s)
{
    for (wchar_t& c : s) {
        if (c == 0xFF08) c = L'(';
        else if (c == 0xFF09) c = L')';
    }
    return s;
}

size_t digit_run(const std::wstring& s, size_t from)
{
    size_t i = from;
    while (i < s.size() && is_decimal_digit(s[i])) ++i;
    return i - from;
}

/// EntityPrefixRegex ^((?:SC)?\d+[.:]) (ignore case): numeric or SC prefix such as "6:" "12."
/// "SC01:". Returns the prefix length, 0 when none.
size_t entity_prefix_length(const std::wstring& s)
{
    auto try_at = [&](size_t start) -> size_t {
        size_t n = digit_run(s, start);
        if (n == 0) return 0;
        size_t end = start + n;
        if (end < s.size() && (s[end] == L'.' || s[end] == L':')) return end + 1;
        return 0;
    };
    if (s.size() >= 2 && (s[0] == L'S' || s[0] == L's') && (s[1] == L'C' || s[1] == L'c'))
        if (size_t len = try_at(2)) return len;
    return try_at(0);
}

void strip_prefix(const std::wstring& name, std::wstring& prefix, std::wstring& base, bool& has_prefix)
{
    size_t len = entity_prefix_length(name);
    has_prefix = len > 0;
    prefix = has_prefix ? name.substr(0, len) : std::wstring();
    base = has_prefix ? name.substr(len) : name;
}

/// ParenSuffixRegex [(（][^)）]*[)）]$ removed (Regex.Replace), then Trim.
std::wstring strip_paren_suffix(const std::wstring& s)
{
    if (s.empty()) return text::trim(s);
    wchar_t last = s.back();
    if (last != L')' && last != 0xFF09) return text::trim(s);
    size_t end = s.size() - 1;
    // The match may not contain a closing parenthesis before its last character: it starts at
    // the first opening parenthesis after the previous closing one (the leftmost match).
    size_t after_close = 0;
    for (size_t i = 0; i < end; ++i)
        if (s[i] == L')' || s[i] == 0xFF09) after_close = i + 1;
    for (size_t i = after_close; i < end; ++i) {
        if (s[i] == L'(' || s[i] == 0xFF08)
            return text::trim(s.substr(0, i));
    }
    return text::trim(s);
}

// --- The other games' translators (2026-10-10) ---------------------------------------------
// FF3-FF6 each had their own EntityTranslator with rules FF1's lacks; their dictionaries were
// built against those rules. The union below runs only after FF1's tiers missed, so FF1's
// results are unchanged (tools/translation_audit.py mirrors every rule here; FF1 and FF4 audit
// at zero misses before and after).

bool is_jp(wchar_t c)
{
    return (c >= 0x3040 && c <= 0x309F) || (c >= 0x30A0 && c <= 0x30FF) || (c >= 0x4E00 && c <= 0x9FFF);
}
bool is_colon(wchar_t c) { return c == L':' || c == 0xFF1A; }

/// "sc_e_0091:" / "ev_e_1030：" style script references: `(sc_e|ev_e)_\d+(_\d+)?` from `from`,
/// returning the length matched (0 when none). Case-insensitive.
size_t script_ref_length(const std::wstring& s, size_t from)
{
    std::wstring low = text::lower(s.substr(from));
    if (!(low.rfind(L"sc_e_", 0) == 0 || low.rfind(L"ev_e_", 0) == 0)) return 0;
    size_t i = 5;
    size_t n = digit_run(low, i);
    if (n == 0) return 0;
    i += n;
    if (i < low.size() && low[i] == L'_') {
        size_t m = digit_run(low, i + 1);
        if (m > 0) i += 1 + m;
    }
    return i;
}

/// FF6 EventSuffixRegex [:：](?:ev_e|sc_e)_\d+(?:_\d+)?$ removed: "女の子:ev_e_1030" -> "女の子".
std::wstring strip_event_ref_suffix(const std::wstring& s)
{
    size_t p = s.find_last_of(L":\xFF1A");
    if (p == std::wstring::npos || p + 1 >= s.size()) return s;
    size_t len = script_ref_length(s, p + 1);
    if (len == 0 || p + 1 + len != s.size()) return s;
    return s.substr(0, p);
}

/// FF6 JapaneseColonSuffixRegex [:：][Japanese]+$ removed: "宝箱:イベント前" -> "宝箱".
std::wstring strip_jp_colon_suffix(const std::wstring& s)
{
    size_t p = s.find_last_of(L":\xFF1A");
    if (p == std::wstring::npos || p + 1 >= s.size()) return s;
    for (size_t i = p + 1; i < s.size(); ++i)
        if (!is_jp(s[i])) return s;
    return s.substr(0, p);
}

/// FF6 ParentheticalSuffixRegex [(（][Japanese]+[)）]$ removed: "扉(オブジェクト)" -> "扉".
std::wstring strip_jp_paren_suffix(const std::wstring& s)
{
    if (s.size() < 3) return s;
    wchar_t last = s.back();
    if (last != L')' && last != 0xFF09) return s;
    size_t end = s.size() - 1;
    for (size_t i = end; i-- > 0;) {
        if (s[i] == L'(' || s[i] == 0xFF08) return i + 1 == end ? s : s.substr(0, i);
        if (!is_jp(s[i])) return s;
    }
    return s;
}

/// FF6 EntityPrefixRegex, the forms FF1's entity prefix lacks: `(sc_e|ev_e)_\d+(_\d+)?[:：]` and
/// `NPC\d*_?` (ignore case). The length matched, 0 when none.
size_t extended_prefix_length(const std::wstring& s)
{
    size_t len = script_ref_length(s, 0);
    if (len > 0) return len < s.size() && is_colon(s[len]) ? len + 1 : 0;
    std::wstring low = text::lower(s.substr(0, 3));
    if (low == L"npc") {
        size_t i = 3 + digit_run(s, 3);
        // "NPC1_" (FF6's rule), and "NPC:" / "NPC：" (FF6 names its guards and the Thamasa
        // characters that way; the old translator missed them).
        if (i < s.size() && (s[i] == L'_' || is_colon(s[i]))) ++i;
        return i;
    }
    return 0;
}

} // namespace

bool is_decimal_digit(wchar_t c)
{
    return (c >= L'0' && c <= L'9') || (c >= 0xFF10 && c <= 0xFF19);
}

bool contains_japanese(const std::wstring& s)
{
    for (wchar_t c : s) {
        if ((c >= 0x3040 && c <= 0x309F) ||   // Hiragana
            (c >= 0x30A0 && c <= 0x30FF) ||   // Katakana
            (c >= 0x4E00 && c <= 0x9FFF))     // CJK
            return true;
    }
    return false;
}

void clear_untranslated_tracking() { g_untranslated_by_map.clear(); }

int translation_count()
{
    initialize();
    return g_count;
}

std::wstring translate_entity_name(const std::wstring& japanese_name)
{
    if (japanese_name.empty()) return japanese_name;
    initialize();
    if (g_count == 0) return japanese_name;

    // When the game is in Japanese, entity names are already Japanese.
    if (modtext::lang() == L"ja") return japanese_name;

    // Trailing enumeration suffix [①-⑳]+$ ("①"): lookups target the base name and the
    // suffix is re-appended to whichever tier succeeds.
    std::wstring enum_suffix;
    std::wstring trailing_raw;   // the suffix as written, for the dictionaries that key on it
    std::wstring core = japanese_name;
    {
        size_t i = core.size();
        while (i > 0 && circled(core[i - 1])) --i;
        if (i < core.size()) {
            trailing_raw = core.substr(i);
            enum_suffix = L" " + trailing_raw;
            core = core.substr(0, i);
        }
    }

    // Trailing plain-digit instance suffix \d+$ (index > 0: not a pure-numeric name).
    if (enum_suffix.empty()) {
        size_t i = core.size();
        while (i > 0 && is_decimal_digit(core[i - 1])) --i;
        if (i < core.size() && i > 0) {
            trailing_raw = core.substr(i);
            enum_suffix = L" " + trailing_raw;
            core = core.substr(0, i);
        }
    }

    // Exact match BEFORE leading-prefix stripping (keys that start with a digit, "2Fへワープ").
    std::wstring hit;
    if (try_lookup(core, hit)) return hit + enum_suffix;

    // Leading enumeration prefix ^[①-⑳]+ -> its number appended: "⑭村人（男性）" -> "Male Villager 14".
    std::wstring leading_suffix;
    {
        size_t n = 0;
        while (n < core.size() && circled(core[n])) ++n;
        if (n > 0) {
            leading_suffix = L" " + circled_to_number(core.substr(0, n));
            core = core.substr(n);
        }
    }

    // Leading plain-digit prefix ^\d+(?![\d.:]) ("15ルフェイン人" -> "Lufenian 15"); "1:村人" is
    // left to the entity prefix. Not a pure-numeric name.
    if (leading_suffix.empty()) {
        size_t n = digit_run(core, 0);
        bool ok = n > 0 && !(n < core.size() && (core[n] == L'.' || core[n] == L':'));
        if (ok && n < core.size()) {
            leading_suffix = L" " + core.substr(0, n);
            core = core.substr(n);
        }
    }

    // 1. Exact match
    if (try_lookup(core, hit)) return hit + leading_suffix + enum_suffix;

    // 1b. Full-width parentheses normalized
    std::wstring normalized = normalize_parens(core);
    if (normalized != core && try_lookup(normalized, hit)) return hit + leading_suffix + enum_suffix;

    // 2. Numeric/SC prefix stripped
    std::wstring prefix, base;
    bool has_prefix = false;
    strip_prefix(core, prefix, base, has_prefix);
    if (has_prefix && try_lookup(base, hit)) return prefix + L" " + hit + leading_suffix + enum_suffix;

    // 3. Parenthesized suffix stripped ("兵士(e_v_0002専用)" -> "兵士")
    const std::wstring& name_for_suffix = has_prefix ? base : core;
    std::wstring stripped = strip_paren_suffix(name_for_suffix);
    if (stripped != name_for_suffix && !stripped.empty() && try_lookup(stripped, hit))
        return (has_prefix ? prefix + L" " + hit : hit) + leading_suffix + enum_suffix;

    // 3b. Leading 真 ("true", "real") with a translated remainder: FF2's late-game variants such
    //     as 真ヒルダ, the real Hilda after the impostor. FF2 EntityTranslator step 6: the
    //     remainder's name with "(true)", so the distinction survives translation. The FF1
    //     translator the core ported had no such step (FF2 test, 2026-10-10).
    if (name_for_suffix.size() > 1 && name_for_suffix[0] == 0x771F) {
        std::wstring after = name_for_suffix.substr(1);
        std::wstring after_stripped = strip_paren_suffix(after);
        if (try_lookup(after, hit) || (after_stripped != after && !after_stripped.empty() && try_lookup(after_stripped, hit))) {
            std::wstring named = TF(L"{0} (true)", {hit});
            return (has_prefix ? prefix + L" " + named : named) + leading_suffix + enum_suffix;
        }
    }

    // --- The other games' tiers (the helpers above; FF1's results are decided by now) --------

    // E1. The leading-stripped name WITH its trailing suffix: FF3 keys its sprite variants on
    //     the digit ("1:女性4" -> "女性4"), FF2 too ("①フィンの町の店主色替え2").
    if (!trailing_raw.empty()) {
        if (try_lookup(core + trailing_raw, hit)) return hit + leading_suffix;
        if (has_prefix && try_lookup(base + trailing_raw, hit)) return prefix + L" " + hit + leading_suffix;
    }

    // E2. FF6's prefixes ("sc_e_0091:", "ev_e_1030：", "NPC1_", "NPC"), spoken before the name as
    //     its translator did ("NPC1 Merchant"); the rest with and without its suffix, and with
    //     a parenthesised suffix removed.
    {
        size_t len = extended_prefix_length(core);
        if (len > 0 && len < core.size()) {
            std::wstring pfx = text::trim(core.substr(0, len));
            if (!pfx.empty() && pfx.back() == L'_') pfx.pop_back();
            std::wstring rest = core.substr(len);
            if (!trailing_raw.empty() && try_lookup(rest + trailing_raw, hit)) return pfx + L" " + hit + leading_suffix;
            if (try_lookup(rest, hit)) return pfx + L" " + hit + leading_suffix + enum_suffix;
            std::wstring rest_stripped = strip_paren_suffix(rest);
            if (rest_stripped != rest && !rest_stripped.empty() && try_lookup(rest_stripped, hit))
                return pfx + L" " + hit + leading_suffix + enum_suffix;
        }
    }

    // E3. FF6's pre-strips: a script reference, a Japanese qualifier after a colon, a
    //     parenthesised Japanese word at the end. The stripped name runs through every tier
    //     again (one level: the strips are idempotent).
    {
        std::wstring pre = strip_jp_paren_suffix(strip_jp_colon_suffix(strip_event_ref_suffix(japanese_name)));
        if (pre != japanese_name && !pre.empty()) {
            std::wstring again = translate_entity_name(pre);
            if (again != pre) return again;
        }
    }

    // 4. Track the untranslated name by the current map (development aid).
    const std::wstring& tracking = has_prefix ? base : core;
    if (contains_japanese(tracking)) {
        std::wstring map_name = current_map_name();
        if (!map_name.empty()) g_untranslated_by_map[map_name].insert(tracking);
    }

    return japanese_name;
}

} // namespace field
