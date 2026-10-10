#pragma once
#include <cstdint>
#include <string>
#include <vector>

// The baked maps: every sub-map's collision, hidden-passage, terrain-attribute and
// per-vehicle-group grids, its routing objects, and the transportation masters, extracted
// offline from the game's Addressables (tools/dump_map_objects.py) and packed by
// tools/bake_maps.py into data/<game>/maps.bin (embedded as MAPS_FFN). The format is
// documented in bake_maps.py; this reader and that writer change together.
//
// Pure data: no game pointers, no IL2CPP. Usable in offline checks (tools/route_check.cpp).
// Loaded once per process from the embedded resource (navigation.cpp) or from a file
// (the check). Main thread only.
namespace field::baked {

/// MapConstants.ObjectType values that are baked (bake_maps.py BAKED_TYPES).
enum class ObjType : uint8_t {
    PointIn = 0, Event = 2, GotoMap = 3, ToLayer = 4, Entity = 5, NPC = 6, TreasureBox = 7,
    OpenTrigger = 8, SavePoint = 9, AnimEntity = 10, MoveArea = 13, RelayInteraction = 16,
    ShopNPC = 17, CollisionEntity = 19, TelepoPoint = 20, TransportationEventAction = 21,
    IgnoreRoute = 22, NonEncountArea = 23, RandomEvent = 28, SwitchEvent = 30,
    /// Baked only: an event whose script changes map (ChangeMap in its Main segment): a warp
    /// pad, a trapdoor. a = destination map id, b = destination PointIn point id, d = 1 when
    /// stepping on it fires (an Event), 0 when it is examined (an Entity).
    Warp = 100,
};

/// One baked routing object (34 bytes in the file). Fields a-d per type: see bake_maps.py.
struct Object {
    ObjType type;
    uint8_t layer;      // 0 Bottom, 1 Ground, 2 Upper
    int16_t tx, ty;     // tile position (top-left for rects)
    uint16_t tw, th;    // size in tiles (1x1 for points)
    int32_t a, b, c, d;
    uint32_t name;      // string index
    uint32_t group;     // string index: "entity_default" or the conditional "ev_e_NNNN"

    int entity_id() const { return c; }
    bool contains(int x, int y) const { return x >= tx && y >= ty && x < tx + tw && y < ty + th; }
};

struct TransportRow { int id, group, landing_group, type; uint32_t message; };

struct Submap {
    uint32_t name;          // string index ("Map_20011_1")
    int32_t first_map_id;   // the lowest map master id using it (-1 when none)
    uint16_t width, height;
    bool world;             // a world map: no collision, attribute + vehicle group grids
    uint8_t layer_mask;     // collision layers present (bit 0 Bottom, 1 Ground, 2 Upper)
    uint8_t transport_layers;
    uint32_t collision_off[3];
    uint32_t passage_off[3];
    uint32_t attribute_off;
    uint32_t transport_off;
    uint32_t objects_off;
    uint32_t object_count;
};

/// Decoded grids of one sub-map, built on first use and kept (a few hundred KB at most).
struct Grids {
    int width = 0, height = 0;
    std::vector<uint8_t> collision[3];   // 1 = blocked, empty = layer absent
    std::vector<uint8_t> passage[3];     // 1 = hidden-passage cell
    std::vector<uint8_t> attribute;      // world maps: terrain attribute id per cell
    std::vector<std::vector<uint8_t>> transport;   // [group - 1]: 1 = blocked for the group
    std::vector<Object> objects;
    std::vector<uint8_t> hazard;         // 1 = a step-on warp / trapdoor cell: never crossed by
                                         // an ordinary route, only entered as its goal
};

/// Bind the baked data (the bytes must outlive the process; a resource does). False when the
/// header is not FFPRMAP1 / the sizes do not fit.
bool load(const void* data, size_t size);
bool loaded();
/// The game byte of the file (1-6), 0 when not loaded.
int game_number();

const Submap* submap_by_map_id(int map_id);
const Submap* submap_by_name(const std::wstring& name);
const Submap* submap_at(int index);
int submap_count();
int submap_index(const Submap* s);
std::wstring name_of(const Submap& s);
std::wstring str(uint32_t index);

/// Decoded grids (cached per sub-map).
const Grids& grids(const Submap& s);

/// Cell tests. A layer absent from the sub-map is closed.
bool in_bounds(const Grids& g, int x, int y);
/// Open for walking on `layer` (collision 0), or a hidden-passage cell when `passages`.
bool walkable(const Grids& g, int layer, int x, int y, bool passages);
bool is_passage(const Grids& g, int layer, int x, int y);
/// World maps: open for a transportation group (its Transportation layer), group 1-based.
bool vehicle_open(const Grids& g, int group, int x, int y);
/// World maps: the terrain attribute id of a cell (0 when none).
int attribute(const Grids& g, int x, int y);

// --- masters ---------------------------------------------------------------------------------
const TransportRow* transport(int transportation_id);
const std::vector<TransportRow>& transports();
/// transportation_group.txt: the group may cross cells of this attribute (1-32).
bool group_crosses(int group, int attribute);
/// landing_group.txt: the group may land on cells of this attribute.
bool group_lands(int landing_group, int attribute);

} // namespace field::baked
