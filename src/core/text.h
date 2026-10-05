#pragma once
#include <string>
#include <string_view>
#include <vector>
#include <initializer_list>

// Text helpers shared by every reader: encoding, the game's markup, whitespace, formatting.
namespace text {

std::wstring from_utf8(std::string_view s);
std::string to_utf8(std::wstring_view s);
/// Narrow ASCII/UTF-8 literal to wide (for names from tables).
std::wstring widen(const char* s);

/// Remove the game's rich-text and icon markup: <color=...>, </color>, <size>, <sprite ...>,
/// <icon ...>, <b>, <i>, and the PR games' inline icon tags such as [ICON_...]? are left as
/// text only when they carry words. Collapses the whitespace the tags leave behind.
std::wstring strip_markup(std::wstring_view s);
/// Collapse runs of whitespace (including newlines and full-width spaces) to one space; trim.
std::wstring collapse(std::wstring_view s);
std::wstring trim(std::wstring_view s);
bool is_blank(std::wstring_view s);
/// Markup stripped and collapsed: what the screen reader gets.
std::wstring clean(std::wstring_view s);

/// "{0} of {1}" style formatting (C# string.Format subset: {n} only).
std::wstring format(std::wstring_view pattern, std::initializer_list<std::wstring> args);
std::wstring num(long long v);

bool starts_with(std::wstring_view s, std::wstring_view prefix);
bool ends_with(std::wstring_view s, std::wstring_view suffix);
bool contains(std::wstring_view s, std::wstring_view part);
std::wstring replace_all(std::wstring s, std::wstring_view from, std::wstring_view to);
std::vector<std::wstring> split_lines(std::wstring_view s);
/// Join non-blank parts with `sep` (", " by default).
std::wstring join(const std::vector<std::wstring>& parts, std::wstring_view sep = L", ");
std::wstring lower(std::wstring_view s);

} // namespace text
