#pragma once
#include <string>

// FF1 Field/MapNameResolver.cs: map ids to the game's own localized area names
// ("Chaos Shrine 1F"), from the Map and Area master data and MessageManager. Main thread.
// Also published as queries::text("MapNameResolver.GetCurrentMapName").
namespace field {

/// GetCurrentMapName: the current map's name, T("Unknown") when there is no map, or
/// "Map {0}" when the master data has no name for it.
std::wstring current_map_name();
/// GetMapExitName: a destination map's name (cached), "Map {0}" as the fallback, "" for <= 0.
std::wstring map_exit_name(int dest_map_id);
/// TryResolveMapNameById: "Area Title", "Area", "Title" or empty.
std::wstring resolve_map_name_by_id(int map_id);
/// GetCurrentMapAssetName / GetMapAssetName: "Map_20020" style asset names, or empty.
std::wstring current_map_asset_name();
std::wstring map_asset_name(int map_id);
/// ClearCache.
void clear_map_name_cache();

} // namespace field
