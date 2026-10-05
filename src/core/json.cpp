#include <cstdlib>
#include "json.h"
#include "text.h"

namespace json {
namespace {

struct Parser {
    std::string_view s;
    size_t i = 0;
    std::string err;

    void ws() { while (i < s.size() && (s[i] == ' ' || s[i] == '\t' || s[i] == '\r' || s[i] == '\n')) ++i; }
    bool fail(const char* m) { if (err.empty()) err = std::string(m) + " at byte " + std::to_string(i); return false; }

    static void put_utf8(std::string& o, unsigned cp)
    {
        if (cp < 0x80) o.push_back(static_cast<char>(cp));
        else if (cp < 0x800) { o.push_back(static_cast<char>(0xC0 | (cp >> 6))); o.push_back(static_cast<char>(0x80 | (cp & 0x3F))); }
        else if (cp < 0x10000) { o.push_back(static_cast<char>(0xE0 | (cp >> 12))); o.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F))); o.push_back(static_cast<char>(0x80 | (cp & 0x3F))); }
        else { o.push_back(static_cast<char>(0xF0 | (cp >> 18))); o.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F))); o.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F))); o.push_back(static_cast<char>(0x80 | (cp & 0x3F))); }
    }

    bool hex4(unsigned& v)
    {
        if (i + 4 > s.size()) return fail("short \\u escape");
        v = 0;
        for (int k = 0; k < 4; ++k) {
            char c = s[i++];
            v <<= 4;
            if (c >= '0' && c <= '9') v |= static_cast<unsigned>(c - '0');
            else if (c >= 'a' && c <= 'f') v |= static_cast<unsigned>(c - 'a' + 10);
            else if (c >= 'A' && c <= 'F') v |= static_cast<unsigned>(c - 'A' + 10);
            else return fail("bad \\u escape");
        }
        return true;
    }

    bool str(std::wstring& out)
    {
        if (s[i] != '"') return fail("expected string");
        ++i;
        std::string raw;
        while (i < s.size() && s[i] != '"') {
            char c = s[i++];
            if (c != '\\') { raw.push_back(c); continue; }
            if (i >= s.size()) return fail("bad escape");
            char e = s[i++];
            switch (e) {
            case '"': raw.push_back('"'); break;
            case '\\': raw.push_back('\\'); break;
            case '/': raw.push_back('/'); break;
            case 'b': raw.push_back('\b'); break;
            case 'f': raw.push_back('\f'); break;
            case 'n': raw.push_back('\n'); break;
            case 'r': raw.push_back('\r'); break;
            case 't': raw.push_back('\t'); break;
            case 'u': {
                unsigned cp = 0;
                if (!hex4(cp)) return false;
                if (cp >= 0xD800 && cp <= 0xDBFF && i + 6 <= s.size() && s[i] == '\\' && s[i + 1] == 'u') {
                    i += 2;
                    unsigned lo = 0;
                    if (!hex4(lo)) return false;
                    cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                }
                put_utf8(raw, cp);
                break;
            }
            default: return fail("bad escape");
            }
        }
        if (i >= s.size()) return fail("unterminated string");
        ++i;
        out = text::from_utf8(raw);
        return true;
    }

    bool value(Value& v, int depth)
    {
        if (depth > 64) return fail("too deep");
        ws();
        if (i >= s.size()) return fail("unexpected end");
        char c = s[i];
        if (c == '{') {
            v.type = Value::Type::Object;
            ++i;
            ws();
            if (i < s.size() && s[i] == '}') { ++i; return true; }
            for (;;) {
                ws();
                std::wstring key;
                if (i >= s.size() || !str(key)) return fail("expected key");
                ws();
                if (i >= s.size() || s[i] != ':') return fail("expected ':'");
                ++i;
                Value child;
                if (!value(child, depth + 1)) return false;
                v.obj[key] = std::move(child);
                ws();
                if (i < s.size() && s[i] == ',') { ++i; continue; }
                if (i < s.size() && s[i] == '}') { ++i; return true; }
                return fail("expected ',' or '}'");
            }
        }
        if (c == '[') {
            v.type = Value::Type::Array;
            ++i;
            ws();
            if (i < s.size() && s[i] == ']') { ++i; return true; }
            for (;;) {
                Value child;
                if (!value(child, depth + 1)) return false;
                v.arr.push_back(std::move(child));
                ws();
                if (i < s.size() && s[i] == ',') { ++i; continue; }
                if (i < s.size() && s[i] == ']') { ++i; return true; }
                return fail("expected ',' or ']'");
            }
        }
        if (c == '"') { v.type = Value::Type::String; return str(v.s); }
        if (s.compare(i, 4, "true") == 0) { v.type = Value::Type::Bool; v.b = true; i += 4; return true; }
        if (s.compare(i, 5, "false") == 0) { v.type = Value::Type::Bool; v.b = false; i += 5; return true; }
        if (s.compare(i, 4, "null") == 0) { v.type = Value::Type::Null; i += 4; return true; }
        size_t start = i;
        while (i < s.size() && (s[i] == '-' || s[i] == '+' || s[i] == '.' || s[i] == 'e' || s[i] == 'E' || (s[i] >= '0' && s[i] <= '9'))) ++i;
        if (start == i) return fail("unexpected character");
        v.type = Value::Type::Number;
        v.n = std::strtod(std::string(s.substr(start, i - start)).c_str(), nullptr);
        return true;
    }
};

} // namespace

const Value* Value::get(const std::wstring& key) const
{
    if (type != Type::Object) return nullptr;
    auto it = obj.find(key);
    return it == obj.end() ? nullptr : &it->second;
}

bool parse(std::string_view utf8, Value& out, std::string* error)
{
    if (utf8.size() >= 3 && static_cast<unsigned char>(utf8[0]) == 0xEF &&
        static_cast<unsigned char>(utf8[1]) == 0xBB && static_cast<unsigned char>(utf8[2]) == 0xBF)
        utf8.remove_prefix(3);
    Parser p{utf8};
    bool ok = p.value(out, 0);
    if (ok) {
        p.ws();
        if (p.i != utf8.size()) ok = p.fail("trailing data");
    }
    if (!ok && error) *error = p.err;
    return ok;
}

} // namespace json
