// Ports FF1 Core/WaypointManager.cs (WaypointData, WaypointFileData, WaypointManager) and
// Field/WaypointEntity.cs (categories, FormatDescription).
//
// File: <game folder>\UserData\waypoints.json (FF6: ffvi_waypoints.json), the C# location (AppDomain.BaseDirectory is the
// game folder under MelonLoader) and the C# format byte for byte: two-space indents, CRLF
// lines, UTF-8 without BOM, invariant floats, ISO-8601 "o" timestamps, lowercase GUID ids.
// Saved after every change, as in C#.
//
// Reading: the file is valid JSON when the C# mod wrote it, so the core JSON reader parses it
// (and unescapes names properly). A file it rejects falls back to a port of the C# mod's own
// lenient scanner, so a hand-edited file loads as much as the C# mod would have loaded.
// The category string is kept as read, so FF2-FF5's own categories ("AirshipLandings",
// "Miscellaneous") survive a save even though FF1's rules show them as Landing Sites.
#include <windows.h>
#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <random>
#include <string>
#include <utility>
#include <vector>
#include "waypoints.h"
#include "../game.h"
#include "../json.h"
#include "../log.h"
#include "../modtext.h"
#include "../text.h"
#include "../field/field_api.h"

namespace ffpr::fieldnav::waypoints {

// --- WaypointEntity ---------------------------------------------------------------------

std::wstring category_display_name(int category)
{
    switch (category) {
    case Landmarks: return T(L"Landmark");
    case Docks: return T(L"Dock");
    case LandingZones: return T(L"Landing Site");
    default: return T(L"Waypoint");
    }
}

std::wstring category_name(int category)
{
    // Resolved per call (not cached) so the names follow a mid-session language change.
    switch (category) {
    case All: return T(L"All");
    case Landmarks: return T(L"Landmarks");
    case Docks: return T(L"Docks");
    case LandingZones: return T(L"Landing Sites");
    default: return std::wstring();
    }
}

namespace {

float vec_distance(unity::Vec3 a, unity::Vec3 b)
{
    float dx = a.x - b.x, dy = a.y - b.y, dz = a.z - b.z;
    return static_cast<float>(std::sqrt(static_cast<double>(dx * dx + dy * dy + dz * dz)));
}

/// C# "{steps:F1}".
std::wstring f1(float v)
{
    char buf[64];
    auto r = std::to_chars(buf, buf + sizeof(buf), v, std::chars_format::fixed, 1);
    return text::widen(std::string(buf, r.ptr).c_str());
}

} // namespace

std::wstring Entity::format_description(unity::Vec3 player_pos) const
{
    float dist = vec_distance(player_pos, position);
    std::wstring dir = ::field::direction(player_pos, position);
    float steps = ::field::distance_to_steps(dist);
    std::wstring step_label = std::fabs(steps - 1.0f) < 0.1f ? T(L"step") : T(L"steps");
    std::wstring cat = category_display_name(category);
    return name + L" (" + cat + L") (" + f1(steps) + L" " + step_label + L" " + dir + L")";
}

// --- WaypointManager --------------------------------------------------------------------

namespace store {
namespace {

/// WaypointData: one record as stored in the file.
struct Data {
    std::wstring id;
    std::wstring name;
    std::wstring category;   // the enum name as written ("LandingZones"), kept as read
    float x = 0, y = 0, z = 0;
    std::wstring created;
};

/// WaypointFileData. Map ids keep their order (the C# Dictionary kept insertion order).
struct FileData {
    int version = 1;
    std::vector<std::pair<std::wstring, std::vector<Data>>> waypoints;
};

FileData g_file;
std::vector<EntityPtr> g_entities;    // the entity cache, in insertion order
std::wstring g_path;

// --- small helpers -------------------------------------------------------------------------

std::vector<Data>* map_list(const std::wstring& map_id)
{
    for (auto& kv : g_file.waypoints)
        if (kv.first == map_id) return &kv.second;
    return nullptr;
}

void erase_map(const std::wstring& map_id)
{
    auto& w = g_file.waypoints;
    w.erase(std::remove_if(w.begin(), w.end(), [&](const auto& kv) { return kv.first == map_id; }), w.end());
}

EntityPtr find_entity(const std::wstring& id)
{
    for (const EntityPtr& e : g_entities)
        if (e->id == id) return e;
    return nullptr;
}

/// WaypointData.GetCategory: Enum.TryParse<WaypointCategory>, LandingZones when it fails.
int parse_category(const std::wstring& raw)
{
    std::wstring s = text::trim(raw);
    if (s == L"All") return All;
    if (s == L"Landmarks") return Landmarks;
    if (s == L"Docks") return Docks;
    if (s == L"LandingZones") return LandingZones;
    // Enum.TryParse also accepts a number
    if (!s.empty()) {
        std::string n = text::to_utf8(s);
        int v = 0;
        auto r = std::from_chars(n.data(), n.data() + n.size(), v);
        if (r.ec == std::errc() && r.ptr == n.data() + n.size()) return v;
    }
    return LandingZones;
}

/// WaypointCategory.ToString().
std::wstring category_string(int category)
{
    switch (category) {
    case All: return L"All";
    case Landmarks: return L"Landmarks";
    case Docks: return L"Docks";
    case LandingZones: return L"LandingZones";
    default: return std::to_wstring(category);
    }
}

/// Guid.NewGuid().ToString(): lowercase, hyphenated, version 4.
std::wstring new_guid()
{
    static std::mt19937_64 rng{[] {
        LARGE_INTEGER qpc{};
        QueryPerformanceCounter(&qpc);
        FILETIME ft{};
        GetSystemTimePreciseAsFileTime(&ft);
        uint64_t t = (static_cast<uint64_t>(ft.dwHighDateTime) << 32) | ft.dwLowDateTime;
        return static_cast<uint64_t>(qpc.QuadPart) ^ (t * 0x9E3779B97F4A7C15ULL) ^
               (static_cast<uint64_t>(GetCurrentProcessId()) << 40) ^ GetTickCount64();
    }()};
    uint8_t b[16];
    for (int i = 0; i < 16; i += 8) {
        uint64_t r = rng();
        for (int j = 0; j < 8; ++j) b[i + j] = static_cast<uint8_t>(r >> (j * 8));
    }
    b[6] = static_cast<uint8_t>((b[6] & 0x0F) | 0x40);
    b[8] = static_cast<uint8_t>((b[8] & 0x3F) | 0x80);
    static const wchar_t* hex = L"0123456789abcdef";
    std::wstring s;
    for (int i = 0; i < 16; ++i) {
        if (i == 4 || i == 6 || i == 8 || i == 10) s += L'-';
        s += hex[b[i] >> 4];
        s += hex[b[i] & 0x0F];
    }
    return s;
}

/// DateTime.UtcNow.ToString("o"): 2026-10-05T12:34:56.1234567Z
std::wstring utc_now_iso()
{
    FILETIME ft{};
    GetSystemTimePreciseAsFileTime(&ft);
    ULARGE_INTEGER t{};
    t.LowPart = ft.dwLowDateTime;
    t.HighPart = ft.dwHighDateTime;
    SYSTEMTIME st{};
    FileTimeToSystemTime(&ft, &st);
    unsigned frac = static_cast<unsigned>(t.QuadPart % 10000000ULL);

    auto pad = [](unsigned v, int width) {
        std::wstring s = std::to_wstring(v);
        while (static_cast<int>(s.size()) < width) s.insert(s.begin(), L'0');
        return s;
    };
    return pad(st.wYear, 4) + L"-" + pad(st.wMonth, 2) + L"-" + pad(st.wDay, 2) + L"T" +
           pad(st.wHour, 2) + L":" + pad(st.wMinute, 2) + L":" + pad(st.wSecond, 2) + L"." +
           pad(frac, 7) + L"Z";
}

/// C# float formatted with InvariantCulture ("{0}"): shortest round-trip, plain notation.
std::string float_str(float v)
{
    char buf[128];
    auto r = std::to_chars(buf, buf + sizeof(buf), v, std::chars_format::fixed);
    if (r.ec != std::errc()) return "0";
    return std::string(buf, r.ptr);
}

/// EscapeJsonString.
std::wstring escape_json(const std::wstring& s)
{
    std::wstring out;
    out.reserve(s.size());
    for (wchar_t c : s) {
        switch (c) {
        case L'\\': out += L"\\\\"; break;
        case L'"': out += L"\\\""; break;
        case L'\n': out += L"\\n"; break;
        case L'\r': out += L"\\r"; break;
        case L'\t': out += L"\\t"; break;
        default: out += c; break;
        }
    }
    return out;
}

// --- file IO ---------------------------------------------------------------------------------

bool read_file(const std::wstring& path, std::string& out)
{
    HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    LARGE_INTEGER size{};
    if (!GetFileSizeEx(h, &size) || size.QuadPart > (16 << 20)) { CloseHandle(h); return false; }
    out.resize(static_cast<size_t>(size.QuadPart));
    DWORD got = 0;
    BOOL ok = out.empty() ? TRUE : ReadFile(h, out.data(), static_cast<DWORD>(out.size()), &got, nullptr);
    CloseHandle(h);
    if (!ok) return false;
    out.resize(got);
    return true;
}

bool write_file(const std::wstring& path, const std::string& data)
{
    std::wstring tmp = path + L".tmp";
    HANDLE h = CreateFileW(tmp.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    DWORD wrote = 0;
    BOOL ok = data.empty() ? TRUE : WriteFile(h, data.data(), static_cast<DWORD>(data.size()), &wrote, nullptr);
    CloseHandle(h);
    if (!ok || wrote != data.size()) { DeleteFileW(tmp.c_str()); return false; }
    return MoveFileExW(tmp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING) != 0;
}

// --- parsing: the core JSON reader -------------------------------------------------------

bool parse_with_json(const std::string& utf8, FileData& out)
{
    json::Value root;
    if (!json::parse(utf8, root) || !root.is_object()) return false;

    if (const json::Value* v = root.get(L"version"); v && v->type == json::Value::Type::Number)
        out.version = static_cast<int>(v->n);

    const json::Value* wps = root.get(L"waypoints");
    if (!wps || !wps->is_object()) return true;

    for (const auto& kv : wps->obj) {
        if (kv.second.type != json::Value::Type::Array) continue;
        std::vector<Data> list;
        for (const json::Value& o : kv.second.arr) {
            if (!o.is_object()) continue;
            Data d;
            auto str = [&](const wchar_t* key, std::wstring fallback) {
                const json::Value* v = o.get(key);
                return (v && v->is_string()) ? v->s : fallback;
            };
            auto num = [&](const wchar_t* key) {
                const json::Value* v = o.get(key);
                return (v && v->type == json::Value::Type::Number) ? static_cast<float>(v->n) : 0.0f;
            };
            d.id = str(L"id", L"");
            if (d.id.empty() && !(o.get(L"id") && o.get(L"id")->is_string())) d.id = new_guid();
            d.name = str(L"name", L"Unnamed");
            d.category = str(L"category", L"LandingZones");
            d.x = num(L"x");
            d.y = num(L"y");
            d.z = num(L"z");
            d.created = str(L"created", L"");
            if (!(o.get(L"created") && o.get(L"created")->is_string())) d.created = utc_now_iso();
            list.push_back(std::move(d));
        }
        if (!list.empty())
            out.waypoints.emplace_back(kv.first, std::move(list));
    }
    return true;
}

// --- parsing: port of the C# mod's lenient scanner (fallback) ----------------------------

bool extract_string(const std::wstring& j, const std::wstring& key, std::wstring& out)
{
    size_t k = j.find(L"\"" + key + L"\"");
    if (k == std::wstring::npos) return false;
    size_t colon = j.find(L':', k);
    if (colon == std::wstring::npos) return false;
    size_t vs = j.find(L'"', colon);
    if (vs == std::wstring::npos) return false;
    size_t ve = j.find(L'"', vs + 1);
    if (ve == std::wstring::npos) return false;
    out = j.substr(vs + 1, ve - vs - 1);
    return true;
}

float extract_float(const std::wstring& j, const std::wstring& key)
{
    size_t k = j.find(L"\"" + key + L"\"");
    if (k == std::wstring::npos) return 0.0f;
    size_t colon = j.find(L':', k);
    if (colon == std::wstring::npos) return 0.0f;
    size_t vs = colon + 1;
    while (vs < j.size() && (j[vs] == L' ' || j[vs] == L'\t')) ++vs;
    size_t ve = vs;
    while (ve < j.size() && ((j[ve] >= L'0' && j[ve] <= L'9') || j[ve] == L'.' || j[ve] == L'-')) ++ve;
    std::string s = text::to_utf8(j.substr(vs, ve - vs));
    float v = 0.0f;
    auto r = std::from_chars(s.data(), s.data() + s.size(), v);
    return (r.ec == std::errc() && r.ptr == s.data() + s.size()) ? v : 0.0f;
}

void parse_lenient(const std::string& utf8, FileData& out)
{
    std::wstring json = text::trim(text::from_utf8(utf8));
    if (json.empty() || json.front() != L'{' || json.back() != L'}') return;

    size_t version_idx = json.find(L"\"version\"");
    if (version_idx != std::wstring::npos) {
        size_t colon = json.find(L':', version_idx);
        size_t comma = colon == std::wstring::npos ? std::wstring::npos : json.find(L',', colon);
        if (comma == std::wstring::npos && colon != std::wstring::npos) comma = json.find(L'}', colon);
        if (colon != std::wstring::npos && comma != std::wstring::npos && comma > colon) {
            std::string vs = text::to_utf8(text::trim(json.substr(colon + 1, comma - colon - 1)));
            int v = 0;
            auto r = std::from_chars(vs.data(), vs.data() + vs.size(), v);
            if (r.ec == std::errc() && r.ptr == vs.data() + vs.size()) out.version = v;
        }
    }

    size_t wi = json.find(L"\"waypoints\"");
    if (wi == std::wstring::npos) return;
    size_t ws = json.find(L'{', wi);
    if (ws == std::wstring::npos) return;
    int depth = 1;
    size_t we = ws + 1;
    while (we < json.size() && depth > 0) {
        if (json[we] == L'{') ++depth;
        else if (json[we] == L'}') --depth;
        ++we;
    }
    std::wstring wj = json.substr(ws, we - ws);

    size_t key_start = 0;
    while ((key_start = wj.find(L'"', key_start)) != std::wstring::npos) {
        size_t key_end = wj.find(L'"', key_start + 1);
        if (key_end == std::wstring::npos) break;
        std::wstring map_id = wj.substr(key_start + 1, key_end - key_start - 1);
        size_t as = wj.find(L'[', key_end);
        if (as == std::wstring::npos) break;
        size_t ae = wj.find(L']', as);
        if (ae == std::wstring::npos) break;
        std::wstring arr = wj.substr(as, ae - as + 1);

        std::vector<Data> list;
        size_t os = 0;
        while ((os = arr.find(L'{', os)) != std::wstring::npos) {
            size_t oe = arr.find(L'}', os);
            if (oe == std::wstring::npos) break;
            std::wstring obj = arr.substr(os, oe - os + 1);
            Data d;
            if (!extract_string(obj, L"id", d.id)) d.id = new_guid();
            if (!extract_string(obj, L"name", d.name)) d.name = L"Unnamed";
            if (!extract_string(obj, L"category", d.category)) d.category = L"LandingZones";
            d.x = extract_float(obj, L"x");
            d.y = extract_float(obj, L"y");
            d.z = extract_float(obj, L"z");
            if (!extract_string(obj, L"created", d.created)) d.created = utc_now_iso();
            list.push_back(std::move(d));
            os = oe + 1;
        }
        if (!list.empty()) {
            // result.waypoints[mapId] = waypoints (a repeated key replaces the earlier one)
            bool replaced = false;
            for (auto& kv : out.waypoints)
                if (kv.first == map_id) { kv.second = std::move(list); replaced = true; break; }
            if (!replaced) out.waypoints.emplace_back(map_id, std::move(list));
        }
        key_start = ae + 1;
    }
}

// --- serialization -------------------------------------------------------------------------

std::string serialize(const FileData& data)
{
    std::string sb;
    auto line = [&](const std::string& s) { sb += s; sb += "\r\n"; };
    auto u8 = [](const std::wstring& s) { return text::to_utf8(s); };

    line("{");
    line("  \"version\": " + std::to_string(data.version) + ",");
    line("  \"waypoints\": {");
    for (size_t m = 0; m < data.waypoints.size(); ++m) {
        const auto& kv = data.waypoints[m];
        line("    \"" + u8(kv.first) + "\": [");
        for (size_t w = 0; w < kv.second.size(); ++w) {
            const Data& wp = kv.second[w];
            line("      {");
            line("        \"id\": \"" + u8(wp.id) + "\",");
            line("        \"name\": \"" + u8(escape_json(wp.name)) + "\",");
            line("        \"category\": \"" + u8(wp.category) + "\",");
            line("        \"x\": " + float_str(wp.x) + ",");
            line("        \"y\": " + float_str(wp.y) + ",");
            line("        \"z\": " + float_str(wp.z) + ",");
            line("        \"created\": \"" + u8(wp.created) + "\"");
            line(w + 1 < kv.second.size() ? "      }," : "      }");
        }
        line(m + 1 < data.waypoints.size() ? "    ]," : "    ]");
    }
    line("  }");
    line("}");
    return sb;
}

EntityPtr make_entity(const Data& d, const std::wstring& map_id)
{
    auto e = std::make_shared<Entity>();
    e->id = d.id;
    e->name = d.name;
    e->position = unity::Vec3{d.x, d.y, d.z};
    e->map_id = map_id;
    e->category = parse_category(d.category);
    return e;
}

void rebuild_entity_cache()
{
    g_entities.clear();
    for (const auto& kv : g_file.waypoints) {
        for (const Data& d : kv.second) {
            EntityPtr e = make_entity(d, kv.first);
            // waypointEntities[data.id] = entity (a duplicate id replaces the earlier entry)
            bool replaced = false;
            for (EntityPtr& existing : g_entities)
                if (existing->id == d.id) { existing = e; replaced = true; break; }
            if (!replaced) g_entities.push_back(e);
        }
    }
}

void ensure_path()
{
    if (!g_path.empty()) return;
    std::wstring dir = std::wstring(game::dir()) + L"UserData";
    if (GetFileAttributesW(dir.c_str()) == INVALID_FILE_ATTRIBUTES)
        CreateDirectoryW(dir.c_str(), nullptr);
    // FF6's C# mod named its file ffvi_waypoints.json; keep it so saved waypoints carry over.
    g_path = dir + (game::is(game::Id::FF6) ? L"\\ffvi_waypoints.json" : L"\\waypoints.json");
}

} // namespace

const std::wstring& file_path()
{
    ensure_path();
    return g_path;
}

void load()
{
    ensure_path();
    g_file = FileData{};
    std::string raw;
    if (GetFileAttributesW(g_path.c_str()) != INVALID_FILE_ATTRIBUTES) {
        if (!read_file(g_path, raw)) {
            LOGF("[Waypoints] Error loading: cannot read the file (err=%lu)", GetLastError());
        } else if (!parse_with_json(raw, g_file)) {
            LOGF("[Waypoints] waypoints.json is not strict JSON; reading it the C# mod's lenient way");
            g_file = FileData{};
            parse_lenient(raw, g_file);
        }
    }
    rebuild_entity_cache();
    size_t n = g_entities.size();
    LOGF("[Waypoints] %zu waypoint(s) loaded from %ls", n, g_path.c_str());
}

void save()
{
    ensure_path();
    if (!write_file(g_path, serialize(g_file)))
        LOGF("[Waypoints] Error saving (err=%lu)", GetLastError());
}

std::vector<EntityPtr> for_map(const std::wstring& map_id)
{
    std::vector<EntityPtr> out;
    for (const EntityPtr& e : g_entities)
        if (e->map_id == map_id) out.push_back(e);
    return out;
}

std::vector<EntityPtr> for_category(const std::wstring& map_id, int category)
{
    if (category == All)
        return for_map(map_id);
    std::vector<EntityPtr> out;
    for (const EntityPtr& e : g_entities)
        if (e->map_id == map_id && e->category == category) out.push_back(e);
    return out;
}

EntityPtr add(const std::wstring& name, unity::Vec3 position, const std::wstring& map_id, int category)
{
    Data d;
    d.id = new_guid();
    d.name = name;
    d.category = category_string(category);
    d.x = position.x;
    d.y = position.y;
    d.z = position.z;
    d.created = utc_now_iso();

    std::vector<Data>* list = map_list(map_id);
    if (!list) {
        g_file.waypoints.emplace_back(map_id, std::vector<Data>{});
        list = &g_file.waypoints.back().second;
    }
    list->push_back(d);

    auto e = std::make_shared<Entity>();
    e->id = d.id;
    e->name = name;
    e->position = position;
    e->map_id = map_id;
    e->category = category;
    g_entities.push_back(e);

    save();
    return e;
}

bool remove(const std::wstring& waypoint_id)
{
    EntityPtr e = find_entity(waypoint_id);
    if (!e) return false;

    std::wstring map_id = e->map_id;
    if (std::vector<Data>* list = map_list(map_id)) {
        list->erase(std::remove_if(list->begin(), list->end(), [&](const Data& d) { return d.id == waypoint_id; }),
                    list->end());
        if (list->empty())
            erase_map(map_id);
    }

    g_entities.erase(std::remove_if(g_entities.begin(), g_entities.end(),
                                    [&](const EntityPtr& x) { return x->id == waypoint_id; }),
                     g_entities.end());
    save();
    return true;
}

bool rename(const std::wstring& waypoint_id, const std::wstring& new_name)
{
    if (text::is_blank(new_name))
        return false;

    EntityPtr e = find_entity(waypoint_id);
    if (!e) return false;

    if (std::vector<Data>* list = map_list(e->map_id)) {
        for (Data& d : *list)
            if (d.id == waypoint_id) { d.name = new_name; break; }
    }

    // A new entity (the C# entity was immutable): the beacon sees a new target.
    auto renamed = std::make_shared<Entity>(*e);
    renamed->name = new_name;
    for (EntityPtr& x : g_entities)
        if (x->id == waypoint_id) x = renamed;

    save();
    return true;
}

int clear_map(const std::wstring& map_id)
{
    int count = static_cast<int>(for_map(map_id).size());
    if (count == 0) return 0;

    if (map_list(map_id)) {
        g_entities.erase(std::remove_if(g_entities.begin(), g_entities.end(),
                                        [&](const EntityPtr& x) { return x->map_id == map_id; }),
                         g_entities.end());
        erase_map(map_id);
    }

    save();
    return count;
}

int count_for_map(const std::wstring& map_id) { return static_cast<int>(for_map(map_id).size()); }

std::wstring next_waypoint_name(const std::wstring& map_id)
{
    int count = count_for_map(map_id) + 1;
    return TF(L"Waypoint {0}", {std::to_wstring(count)});
}

} // namespace store
} // namespace ffpr::fieldnav::waypoints
