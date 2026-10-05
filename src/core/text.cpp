#include <windows.h>
#include <cwctype>
#include "text.h"

namespace text {

std::wstring from_utf8(std::string_view s)
{
    if (s.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
    if (n <= 0) return {};
    std::wstring out(static_cast<size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), out.data(), n);
    return out;
}

std::string to_utf8(std::wstring_view s)
{
    if (s.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0, nullptr, nullptr);
    if (n <= 0) return {};
    std::string out(static_cast<size_t>(n), '\0');
    WideCharToMultiByte(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), out.data(), n, nullptr, nullptr);
    return out;
}

std::wstring widen(const char* s) { return s ? from_utf8(s) : std::wstring(); }

namespace {
bool space(wchar_t c) { return c == L' ' || c == L'\t' || c == L'\r' || c == L'\n' || c == 0x3000 || c == 0xA0; }
}

// FF1's two rules, kept exactly (TextUtils.cs): a single-token tag "<[^<>\s]+?>" is icon or
// rich-text markup (<ic_Drag>, <sprite=1>, <color=#fff>, </color>) and is removed; a '<' that
// is followed by whitespace before its '>' is conversational text and is kept.
std::wstring strip_markup(std::wstring_view s)
{
    std::wstring out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == L'<') {
            size_t j = i + 1;
            bool token = j < s.size() && s[j] != L'>';
            while (j < s.size() && s[j] != L'>') {
                if (s[j] == L'<' || space(s[j])) { token = false; break; }
                ++j;
            }
            if (token && j < s.size() && s[j] == L'>') { i = j; continue; }
        }
        out.push_back(s[i]);
    }
    return out;
}

std::wstring collapse(std::wstring_view s)
{
    std::wstring out;
    out.reserve(s.size());
    bool pending = false;
    for (wchar_t c : s) {
        if (space(c)) { pending = !out.empty(); continue; }
        if (pending) { out.push_back(L' '); pending = false; }
        out.push_back(c);
    }
    return out;
}

std::wstring trim(std::wstring_view s)
{
    size_t a = 0, b = s.size();
    while (a < b && space(s[a])) ++a;
    while (b > a && space(s[b - 1])) --b;
    return std::wstring(s.substr(a, b - a));
}

bool is_blank(std::wstring_view s)
{
    for (wchar_t c : s) if (!space(c)) return false;
    return true;
}

std::wstring clean(std::wstring_view s) { return collapse(strip_markup(s)); }

std::wstring format(std::wstring_view pattern, std::initializer_list<std::wstring> args)
{
    std::wstring out;
    out.reserve(pattern.size() + 32);
    for (size_t i = 0; i < pattern.size(); ++i) {
        if (pattern[i] == L'{') {
            size_t j = i + 1;
            int idx = 0;
            bool digits = false;
            while (j < pattern.size() && pattern[j] >= L'0' && pattern[j] <= L'9') {
                idx = idx * 10 + (pattern[j] - L'0');
                ++j;
                digits = true;
            }
            if (digits && j < pattern.size() && pattern[j] == L'}' && idx < static_cast<int>(args.size())) {
                out += *(args.begin() + idx);
                i = j;
                continue;
            }
        }
        out.push_back(pattern[i]);
    }
    return out;
}

std::wstring num(long long v) { return std::to_wstring(v); }

bool starts_with(std::wstring_view s, std::wstring_view p) { return s.size() >= p.size() && s.compare(0, p.size(), p) == 0; }
bool ends_with(std::wstring_view s, std::wstring_view p) { return s.size() >= p.size() && s.compare(s.size() - p.size(), p.size(), p) == 0; }
bool contains(std::wstring_view s, std::wstring_view p) { return s.find(p) != std::wstring_view::npos; }

std::wstring replace_all(std::wstring s, std::wstring_view from, std::wstring_view to)
{
    if (from.empty()) return s;
    size_t pos = 0;
    while ((pos = s.find(from, pos)) != std::wstring::npos) {
        s.replace(pos, from.size(), to);
        pos += to.size();
    }
    return s;
}

std::vector<std::wstring> split_lines(std::wstring_view s)
{
    std::vector<std::wstring> out;
    size_t start = 0;
    for (size_t i = 0; i <= s.size(); ++i) {
        if (i == s.size() || s[i] == L'\n') {
            std::wstring line(s.substr(start, i - start));
            if (!line.empty() && line.back() == L'\r') line.pop_back();
            out.push_back(std::move(line));
            start = i + 1;
        }
    }
    return out;
}

std::wstring join(const std::vector<std::wstring>& parts, std::wstring_view sep)
{
    std::wstring out;
    for (const auto& p : parts) {
        if (is_blank(p)) continue;
        if (!out.empty()) out += sep;
        out += p;
    }
    return out;
}

std::wstring lower(std::wstring_view s)
{
    std::wstring out(s);
    for (auto& c : out) c = static_cast<wchar_t>(std::towlower(c));
    return out;
}

} // namespace text
