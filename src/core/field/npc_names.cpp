// Shared, game-independent parsing and ordering for translated NPC labels.
#include "npc_names.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <map>

namespace field::npc_names {
namespace {

struct Rune {
    uint32_t value = 0;
    size_t begin = 0;
    size_t end = 0;
};

Rune rune_at(std::wstring_view text, size_t at)
{
    Rune result;
    result.begin = at;
    result.end = std::min(at + 1, text.size());
    result.value = static_cast<uint32_t>(text[at]);
    if constexpr (sizeof(wchar_t) == 2) {
        if (result.value >= 0xD800 && result.value <= 0xDBFF && result.end < text.size()) {
            const uint32_t low = static_cast<uint32_t>(text[result.end]);
            if (low >= 0xDC00 && low <= 0xDFFF) {
                result.value = 0x10000 + ((result.value - 0xD800) << 10) + (low - 0xDC00);
                ++result.end;
            }
        }
    }
    return result;
}

bool whitespace(uint32_t c)
{
    return (c >= 0x0009 && c <= 0x000D) || c == 0x0020 || c == 0x0085 || c == 0x00A0 ||
           c == 0x1680 || (c >= 0x2000 && c <= 0x200A) || c == 0x2028 || c == 0x2029 ||
           c == 0x202F || c == 0x205F || c == 0x3000;
}

bool numeric_prefix_separator(uint32_t c)
{
    return whitespace(c) || c == L':' || c == L'.' || c == 0xFF1A || c == 0xFF0E;
}

bool npc_separator(uint32_t c)
{
    return whitespace(c) || c == L'_' || c == L':' || c == 0xFF1A;
}

std::wstring_view trim(std::wstring_view text)
{
    size_t first = 0;
    while (first < text.size() && whitespace(rune_at(text, first).value))
        first = rune_at(text, first).end;
    size_t last = text.size();
    while (last > first) {
        size_t at = last - 1;
        if constexpr (sizeof(wchar_t) == 2) {
            if (at > first) {
                const uint32_t low = static_cast<uint32_t>(text[at]);
                const uint32_t high = static_cast<uint32_t>(text[at - 1]);
                if (low >= 0xDC00 && low <= 0xDFFF && high >= 0xD800 && high <= 0xDBFF)
                    --at;
            }
        }
        if (!whitespace(rune_at(text, at).value)) break;
        last = at;
    }
    return text.substr(first, last - first);
}

bool digit(uint32_t c, unsigned& value)
{
    if (c >= L'0' && c <= L'9') { value = c - L'0'; return true; }
    if (c >= 0xFF10 && c <= 0xFF19) { value = c - 0xFF10; return true; }
    return false;
}

bool decorated_number(uint32_t c, unsigned& value)
{
    if (c >= 0x2460 && c <= 0x2473) { value = c - 0x2460 + 1; return true; }
    if (c >= 0x3251 && c <= 0x325F) { value = c - 0x3251 + 21; return true; }
    if (c >= 0x32B1 && c <= 0x32BF) { value = c - 0x32B1 + 36; return true; }
    if (c >= 0x2474 && c <= 0x2487) { value = c - 0x2474 + 1; return true; }
    return false;
}

bool ascii_equal(uint32_t c, char expected)
{
    if (c >= 'a' && c <= 'z') c -= 'a' - 'A';
    return c == static_cast<unsigned char>(expected);
}

bool starts_ascii(std::wstring_view text, size_t at, const char* value)
{
    for (size_t i = 0; value[i]; ++i) {
        if (at + i >= text.size() || !ascii_equal(rune_at(text, at + i).value, value[i])) return false;
    }
    return true;
}

bool number_at(std::wstring_view text, size_t at, size_t& end, long long& value)
{
    size_t cursor = at;
    bool found = false;
    long long parsed = 0;
    while (cursor < text.size()) {
        const Rune r = rune_at(text, cursor);
        unsigned d = 0;
        if (!digit(r.value, d)) break;
        found = true;
        if (parsed > (std::numeric_limits<long long>::max() - static_cast<long long>(d)) / 10)
            parsed = std::numeric_limits<long long>::max();
        else if (parsed != std::numeric_limits<long long>::max())
            parsed = parsed * 10 + static_cast<long long>(d);
        cursor = r.end;
    }
    if (!found) return false;
    end = cursor;
    value = parsed;
    return true;
}

bool number_glyph_at(std::wstring_view text, size_t at, size_t& end, long long& value)
{
    if (at >= text.size()) return false;
    const Rune r = rune_at(text, at);
    unsigned parsed = 0;
    if (!decorated_number(r.value, parsed)) return false;
    end = r.end;
    value = parsed;
    return true;
}

std::wstring materialize(std::wstring_view text)
{
    return std::wstring(text.begin(), text.end());
}

bool parse_script_id(std::wstring_view text, size_t begin, size_t& id_end, long long& ordinal)
{
    const char* prefix = nullptr;
    if (starts_ascii(text, begin, "SC_E_")) prefix = "SC_E_";
    else if (starts_ascii(text, begin, "EV_E_")) prefix = "EV_E_";
    if (!prefix) return false;

    size_t after_id = begin + 5;
    size_t digits_end = 0;
    if (!number_at(text, after_id, digits_end, ordinal)) return false;
    after_id = digits_end;
    if (after_id < text.size() && rune_at(text, after_id).value == L'_') {
        size_t secondary_end = 0;
        long long ignored = 0;
        if (!number_at(text, after_id + 1, secondary_end, ignored)) return false;
        after_id = secondary_end;
    }
    id_end = after_id;
    return true;
}

bool parse_script_prefix(std::wstring_view text, size_t begin, size_t& base_begin, long long& ordinal)
{
    size_t after_id = 0;
    if (!parse_script_id(text, begin, after_id, ordinal) || after_id >= text.size()) return false;
    const uint32_t colon = rune_at(text, after_id).value;
    if (colon != L':' && colon != 0xFF1A) return false;
    base_begin = rune_at(text, after_id).end;
    return true;
}

bool parse_sc_prefix(std::wstring_view text, size_t begin, size_t& base_begin, long long& ordinal)
{
    if (!starts_ascii(text, begin, "SC")) return false;
    size_t end = 0;
    if (!number_at(text, begin + 2, end, ordinal) || end >= text.size()) return false;
    const uint32_t mark = rune_at(text, end).value;
    if (mark != L':' && mark != L'.' && mark != 0xFF1A && mark != 0xFF0E) return false;
    base_begin = rune_at(text, end).end;
    return true;
}

bool parse_npc_prefix(std::wstring_view text, size_t begin, size_t& base_begin, long long& ordinal)
{
    if (!starts_ascii(text, begin, "NPC")) return false;
    size_t after = begin + 3;
    while (after < text.size() && npc_separator(rune_at(text, after).value))
        after = rune_at(text, after).end;
    size_t digits_end = 0;
    long long parsed = -1;
    if (number_at(text, after, digits_end, parsed)) {
        ordinal = parsed;
        after = digits_end;
    }
    while (after < text.size() && npc_separator(rune_at(text, after).value))
        after = rune_at(text, after).end;
    base_begin = after;
    return true;
}

std::wstring_view strip_suffix_separators(std::wstring_view text)
{
    while (!text.empty()) {
        const size_t at = text.size() - 1;
        if (at && sizeof(wchar_t) == 2) {
            const uint32_t low = static_cast<uint32_t>(text[at]);
            const uint32_t high = static_cast<uint32_t>(text[at - 1]);
            if (low >= 0xDC00 && low <= 0xDFFF && high >= 0xD800 && high <= 0xDBFF) break;
        }
        const uint32_t c = rune_at(text, at).value;
        if (!(whitespace(c) || c == L'_')) break;
        text.remove_suffix(1);
    }
    return trim(text);
}

bool number_in_brackets(std::wstring_view text, size_t begin, size_t end, long long& ordinal)
{
    if (begin >= end) return false;
    size_t parsed_end = 0;
    if (number_at(text.substr(0, end), begin, parsed_end, ordinal) && parsed_end == end)
        return true;
    return number_glyph_at(text, begin, parsed_end, ordinal) && parsed_end == end;
}

bool strip_suffix_number(std::wstring_view text, std::wstring_view& base, long long& ordinal)
{
    size_t suffix_begin = text.size();
    bool found = false;
    long long parsed = -1;

    // Japanese FF5/FF6 names also append a full script identifier. Remove the whole
    // decoration, rather than leaving ':ev_e_' after stripping its numeric tail.
    const size_t colon = text.find_last_of(L":\xFF1A");
    size_t script_end = 0;
    if (colon != std::wstring_view::npos && parse_script_id(text, colon + 1, script_end, parsed)) {
        // The descriptive tag ends before this developer script reference. FF6 also
        // appends bare Japanese notes and ID ranges after it; those are metadata too.
        suffix_begin = colon;
        found = true;
    }

    // Strip only parentheses that contain a number by themselves. Parentheses in descriptions
    // such as "Soldier (guard)7" remain part of the base label.
    if (!found && !text.empty() && text.back() == L')') {
        const size_t open = text.find_last_of(L'(');
        if (open != std::wstring_view::npos && number_in_brackets(text, open + 1, text.size() - 1, parsed)) {
            suffix_begin = open;
            found = true;
        }
    }

    if (!found && !text.empty()) {
        size_t glyph_begin = text.size() - 1;
        if (sizeof(wchar_t) == 2 && glyph_begin > 0) {
            const uint32_t low = static_cast<uint32_t>(text[glyph_begin]);
            const uint32_t high = static_cast<uint32_t>(text[glyph_begin - 1]);
            if (low >= 0xDC00 && low <= 0xDFFF && high >= 0xD800 && high <= 0xDBFF)
                --glyph_begin;
        }
        size_t glyph_end = 0;
        if (number_glyph_at(text, glyph_begin, glyph_end, parsed) && glyph_end == text.size()) {
            suffix_begin = glyph_begin;
            found = true;
        }
    }

    if (!found && !text.empty()) {
        size_t digits_begin = text.size();
        while (digits_begin > 0) {
            const size_t at = digits_begin - 1;
            unsigned ignored = 0;
            if (!digit(rune_at(text, at).value, ignored)) break;
            digits_begin = at;
        }
        size_t parsed_end = 0;
        if (digits_begin < text.size() && number_at(text, digits_begin, parsed_end, parsed) && parsed_end == text.size()) {
            suffix_begin = digits_begin;
            found = true;
        }
    }

    if (!found) return false;
    const std::wstring_view candidate = strip_suffix_separators(text.substr(0, suffix_begin));
    if (candidate.empty()) return false;
    base = candidate;
    ordinal = parsed;
    return true;
}

Label parsed_base(std::wstring_view raw, std::wstring_view base, long long prefix_ordinal)
{
    base = trim(base);
    if (base.empty()) return {materialize(raw), -1};
    std::wstring_view suffix_base;
    long long suffix_ordinal = -1;
    const bool has_prefix_ordinal = prefix_ordinal >= 0;
    while (strip_suffix_number(base, suffix_base, suffix_ordinal)) {
        base = suffix_base;
        if (!has_prefix_ordinal) prefix_ordinal = suffix_ordinal;
    }
    return {materialize(base), prefix_ordinal};
}

float sort_coordinate(float value)
{
    return std::isfinite(value) ? value : std::numeric_limits<float>::infinity();
}

} // namespace

Label parse(std::wstring_view input)
{
    const std::wstring_view original = trim(input);
    if (original.empty()) return {std::wstring{}, -1};
    std::wstring_view name = original;
    long long ordinal = -1;
    // Some FF6 labels nest script and NPC prefixes. Every successful pass shortens the
    // remaining name; never consume a prefix whose descriptive remainder is empty.
    for (;;) {
        size_t base_begin = 0;
        long long next_ordinal = -1;
        const bool named_prefix = parse_script_prefix(name, 0, base_begin, next_ordinal) ||
                                  parse_sc_prefix(name, 0, base_begin, next_ordinal) ||
                                  parse_npc_prefix(name, 0, base_begin, next_ordinal);
        if (!named_prefix) {
            next_ordinal = -1;
            if (!number_glyph_at(name, 0, base_begin, next_ordinal) &&
                !number_at(name, 0, base_begin, next_ordinal)) break;
            while (base_begin < name.size() && numeric_prefix_separator(rune_at(name, base_begin).value))
                base_begin = rune_at(name, base_begin).end;
        }
        const std::wstring_view base = trim(name.substr(base_begin));
        if (base.empty()) {
            // FF6 Map_20110 contains a bare "NPC7". Its descriptive fallback is NPC;
            // keep the developer number only as ordering metadata, never in the label.
            if (named_prefix && starts_ascii(name, 0, "NPC"))
                return {L"NPC", ordinal >= 0 ? ordinal : next_ordinal};
            return {materialize(name), ordinal};
        }
        if (ordinal < 0) ordinal = next_ordinal;
        name = base;
    }
    return parsed_base(original, name, ordinal);
}

void renumber(std::vector<Entry>& entries)
{
    using Key = std::pair<int, std::wstring>;
    std::map<Key, std::vector<size_t>> groups;
    for (size_t i = 0; i < entries.size(); ++i)
        if (entries[i].output && !entries[i].label.base.empty())
            groups[{entries[i].map_id, entries[i].label.base}].push_back(i);

    for (auto& [key, indices] : groups) {
        const std::wstring& base = key.second;
        if (indices.size() == 1) {
            *entries[indices.front()].output = base;
            continue;
        }

        std::stable_sort(indices.begin(), indices.end(), [&](size_t left, size_t right) {
            const Entry& a = entries[left];
            const Entry& b = entries[right];
            const bool a_known = a.label.ordinal >= 0;
            const bool b_known = b.label.ordinal >= 0;
            if (a_known != b_known) return a_known;
            if (a_known && a.label.ordinal != b.label.ordinal) return a.label.ordinal < b.label.ordinal;
            if (a.layer != b.layer) return a.layer < b.layer;
            if (sort_coordinate(a.y) != sort_coordinate(b.y)) return sort_coordinate(a.y) < sort_coordinate(b.y);
            return sort_coordinate(a.x) < sort_coordinate(b.x);
        });

        for (size_t rank = 0; rank < indices.size(); ++rank)
            *entries[indices[rank]].output = base + std::to_wstring(rank + 1);
    }
}

} // namespace field::npc_names
