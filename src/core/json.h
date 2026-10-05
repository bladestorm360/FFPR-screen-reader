#pragma once
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

// A small JSON reader for the mod's own data files (mod_text.json, translation.json,
// EntityNames.json). Strings come back as UTF-16. Not a general-purpose library: no writer,
// numbers kept as double, duplicate keys keep the last value.
namespace json {

struct Value {
    enum class Type { Null, Bool, Number, String, Array, Object } type = Type::Null;
    bool b = false;
    double n = 0;
    std::wstring s;
    std::vector<Value> arr;
    std::map<std::wstring, Value> obj;

    bool is_object() const { return type == Type::Object; }
    bool is_string() const { return type == Type::String; }
    /// Member by key, or null when absent / not an object.
    const Value* get(const std::wstring& key) const;
};

/// Parse UTF-8 text (a BOM is skipped). False with `error` set on malformed input.
bool parse(std::string_view utf8, Value& out, std::string* error = nullptr);

} // namespace json
