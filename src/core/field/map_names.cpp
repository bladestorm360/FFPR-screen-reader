// Ports FF1 Field/MapNameResolver.cs.
//
// MasterManager.GetList<Map>() / GetList<Area>() are generic methods, which a native caller
// cannot invoke. They return MasterManager.masterList[DataType].DataList (a
// DataElement<T>.<DataList>k__BackingField Dictionary<int, T>), so the port reads that
// dictionary directly: DataType.Map = 0 and DataType.Area = 1 in all five games.
#include <map>
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

        // Localized map title ("1F", "B1", "Entrance")
        std::wstring title_key = ref::call_str(r::mMapGetMapTitle, map);
        std::wstring title;
        if (!title_key.empty() && title_key != L"None" && text::lower(title_key) != L"none")
            title = message(title_key);

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
