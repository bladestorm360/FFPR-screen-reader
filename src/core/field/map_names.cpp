// Ports FF1 Field/MapNameResolver.cs.
//
// MasterManager.GetList<Map>() / GetList<Area>() are generic methods, which a native caller
// cannot invoke. They return MasterManager.masterList[DataType].DataList (a
// DataElement<T>.<DataList>k__BackingField Dictionary<int, T>), so the port reads that
// dictionary directly: DataType.Map = 0 and DataType.Area = 1 in all five games.
#include <map>
#include "entity_translator.h"
#include "field_internal.h"
#include "map_names.h"
#include "../core.h"
#include "../hooks.h"
#include "../il2cpp.h"
#include "../log.h"
#include "../modtext.h"
#include "../text.h"

namespace field {
namespace {

constexpr int kDataTypeMap = 0;    // MasterManager.DataType.Map
constexpr int kDataTypeArea = 1;   // MasterManager.DataType.Area

std::map<int, std::wstring> g_map_name_cache;

struct Find { int64_t want; void* found; };

void find_entry(uint64_t key, uint64_t value, void* state)
{
    auto* f = static_cast<Find*>(state);
    if (static_cast<int32_t>(key) == f->want) f->found = reinterpret_cast<void*>(value);
}

/// Value of Dictionary<int, T> (reference T) for `key`, or null.
void* dict_int_get(void* dict, int key)
{
    if (!dict) return nullptr;
    Find f{key, nullptr};
    unity::dict_for_each(dict, &find_entry, &f, 4, 8);
    return f.found;
}

/// MasterManager.GetList<T>() for the master of `data_type`: its Dictionary<int, T>, or null.
void* master_list(void* master_manager, int data_type)
{
    void* lists = ref::rd<void*>(master_manager, r::fMasterList);
    void* element = dict_int_get(lists, data_type);   // DataElement<T>
    if (!element) return nullptr;
    il2cpp::Class* k = il2cpp::object_class(element);
    if (!k) return nullptr;
    int32_t off = il2cpp::field_offset(k, "<DataList>k__BackingField");
    if (off < 0) {
        static bool logged = false;
        if (!logged) { logged = true; LOGF("[MapNameResolver] DataElement has no <DataList>k__BackingField"); }
        return nullptr;
    }
    return il2cpp::read_field<void*>(element, off);
}

std::wstring message(const std::wstring& key) { return get_message(key); }

/// A title from data/<game>/map_titles.json for a map the master leaves untitled. A string is
/// a message key of the game's own text ("MSG_MAP_NAME_11"). An object is a label inferred
/// from the map's baked contents (the user's rules, 2026-10-11: a named NPC's house, a
/// generic NPC's "House", else the event or key item the room holds, else nothing):
///   {"text": K}   the mod text K ("Church", "Pub", "Throne Room");
///   {"owner": N}  "N's House", {"room": N} "N's Room": N is the NPC's baked (Japanese) name,
///                 translated as the entity list translates it (data/<game>/translation.json),
///                 without a descriptor in parentheses ("Salina (Woman 2)" -> "Salina");
///   {"entity": E} the translated name of the baked object E that defines the room (FF3's
///                 "Resurrection" and "Healing Spring" points);
///   {"item": M}   "I Room" for the key item named by the game message M.
/// A list joins its parts with spaces ("Rebel Hideout Revival Chamber").
std::wstring without_descriptor(std::wstring name)
{
    for (wchar_t open : {L'(', L'（'}) {
        const size_t at = name.find(open);
        if (at != std::wstring::npos && at > 0) name = name.substr(0, at);
    }
    while (!name.empty() && (name.back() == L' ' || name.back() == L'　')) name.pop_back();
    return name;
}

std::wstring override_title(const json::Value& v)
{
    if (v.is_string()) return v.s.empty() ? L"" : message(v.s);
    if (v.type == json::Value::Type::Array) {
        std::wstring out;
        for (const json::Value& part : v.arr) {
            std::wstring t = override_title(part);
            if (t.empty()) continue;
            if (!out.empty()) out += L" ";
            out += t;
        }
        return out;
    }
    if (!v.is_object()) return L"";
    if (const json::Value* t = v.get(L"text"))
        if (t->is_string() && !t->s.empty()) return T(t->s);
    if (const json::Value* o = v.get(L"owner"))
        if (o->is_string() && !o->s.empty()) return TF(L"{0}'s House", {without_descriptor(translate_entity_name(o->s))});
    if (const json::Value* r = v.get(L"room"))
        if (r->is_string() && !r->s.empty()) return TF(L"{0}'s Room", {without_descriptor(translate_entity_name(r->s))});
    if (const json::Value* e = v.get(L"entity"))
        if (e->is_string() && !e->s.empty()) return without_descriptor(translate_entity_name(e->s));
    if (const json::Value* i = v.get(L"item"))
        if (i->is_string() && !i->s.empty()) return TF(L"{0} Room", {message(i->s)});
    return L"";
}

} // namespace

int current_map_id_or_user_data()
{
    // FieldMapProvisionInformation first - this tracks the actual displayed map
    void* info = ref::call_static<void*>(r::mProvisionGetInstance);
    if (info) {
        int map_id = ref::rd<int32_t>(info, r::fProvisionCurrentMapId);
        if (map_id > 0) return map_id;
    }
    // Fallback to UserDataManager
    void* udm = ref::call_static<void*>(r::mUserDataInstance);
    if (udm) return ref::rd<int32_t>(udm, r::fUserDataCurrentMapId);
    return -1;
}

std::wstring current_map_name()
{
    return hooks::guarded_r("MapNameResolver.GetCurrentMapName", T(L"Unknown"), [&] {
        int map_id = current_map_id_or_user_data();
        if (map_id <= 0) return T(L"Unknown");
        std::wstring resolved = resolve_map_name_by_id(map_id);
        if (!resolved.empty()) return resolved;
        return TF(L"Map {0}", {text::num(map_id)});
    });
}

std::wstring map_exit_name(int dest_map_id)
{
    if (dest_map_id <= 0) return L"";
    auto it = g_map_name_cache.find(dest_map_id);
    if (it != g_map_name_cache.end()) return it->second;

    std::wstring resolved = resolve_map_name_by_id(dest_map_id);
    if (!resolved.empty()) {
        g_map_name_cache[dest_map_id] = resolved;
        return resolved;
    }
    std::wstring fallback = TF(L"Map {0}", {text::num(dest_map_id)});
    g_map_name_cache[dest_map_id] = fallback;
    return fallback;
}

std::wstring resolve_map_name_by_id(int map_id)
{
    return hooks::guarded_r("MapNameResolver.TryResolveMapNameById", std::wstring(), [&]() -> std::wstring {
        void* mm = ref::instance(r::kMasterManager);
        if (!mm) {
            LOGF("[MapNameResolver] MasterManager.Instance is null");
            return L"";
        }
        if (!ref::instance(r::kMessageManager)) {
            LOGF("[MapNameResolver] MessageManager.Instance is null");
            return L"";
        }

        void* map_list = master_list(mm, kDataTypeMap);
        if (!map_list) {
            LOGF("[MapNameResolver] Could not get Map list from MasterManager");
            return L"";
        }
        void* map = dict_int_get(map_list, map_id);
        if (!map) return L"";

        int area_id = ref::call<int32_t>(r::mMapGetAreaId, map);
        void* area_list = master_list(mm, kDataTypeArea);
        void* area = dict_int_get(area_list, area_id);
        if (!area) return L"";

        // Localized area name ("Chaos Shrine", "Cornelia")
        std::wstring area_name_key = ref::call_str(r::mAreaGetAreaName, area);
        std::wstring area_name;
        if (!area_name_key.empty()) area_name = message(area_name_key);

        // An authored title first: data/<game>/map_titles.json, by map id, for a map the
        // master leaves untitled (FF2's late Altair rebel hideout rooms are "Altair" alone;
        // user, 2026-10-10) or titles wrongly (FF1's churches are "2F" in the master; user,
        // 2026-10-11): a message key of the game's own text or a label inferred from the
        // room's contents (override_title).
        std::wstring title;
        {
            const json::Value& overrides = data::game_json("maptitles");
            if (const json::Value* v = overrides.get(std::to_wstring(map_id))) title = override_title(*v);
        }

        // Localized map title ("1F", "B1", "Entrance")
        if (title.empty()) {
            std::wstring title_key = ref::call_str(r::mMapGetMapTitle, map);
            if (!title_key.empty() && title_key != L"None" && text::lower(title_key) != L"none")
                title = message(title_key);
        }

        // No title from the message table: the Floor field
        if (title.empty()) {
            int floor = ref::call<int32_t>(r::mMapGetFloor, map);
            if (floor > 0) title = std::to_wstring(floor) + L"F";
            else if (floor < 0) title = L"B" + std::to_wstring(-floor);
        }

        if (!area_name.empty() && !title.empty()) return area_name + L" " + title;
        if (!area_name.empty()) return area_name;
        if (!title.empty()) return title;
        return L"";
    });
}

std::wstring current_map_asset_name()
{
    int map_id = current_map_id_or_user_data();
    if (map_id <= 0) return L"";
    return map_asset_name(map_id);
}

std::wstring map_asset_name(int map_id)
{
    if (map_id <= 0) return L"";
    return hooks::guarded_r("MapNameResolver.GetMapAssetName", std::wstring(), [&]() -> std::wstring {
        void* mm = ref::instance(r::kMasterManager);
        if (!mm) return L"";
        void* map = dict_int_get(master_list(mm, kDataTypeMap), map_id);
        if (!map) return L"";
        return ref::call_str(r::mMapGetAssetName, map);
    });
}

void clear_map_name_cache() { g_map_name_cache.clear(); }

namespace {
int install()
{
    queries::define_text("MapNameResolver.GetCurrentMapName", [] { return current_map_name(); });
    return 0;
}
core::Install s_install{"Field.MapNameResolver", &install, 100};
} // namespace

} // namespace field
