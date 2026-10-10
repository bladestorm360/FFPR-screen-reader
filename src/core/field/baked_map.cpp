// The baked-map reader: see baked_map.h and tools/bake_maps.py (the format).
#include <windows.h>
#include <cstring>
#include <map>
#include <unordered_map>
#include "baked_map.h"

namespace field::baked {
namespace {

#pragma pack(push, 1)
struct Header {
    char magic[8];
    uint32_t version;
    uint8_t game;
    uint8_t pad[3];
    uint32_t strings_off, strings_count;
    uint32_t submaps_off, submap_count;
    uint32_t maps_off, map_count;
    uint32_t transport_off, transport_count;
    uint32_t tgroup_off, tgroup_count;
    uint32_t lgroup_off, lgroup_count;
};
struct SubmapRec {
    uint32_t name;
    int32_t first_map_id;
    uint16_t width, height;
    uint8_t flags, layer_mask, transport_layers, pad;
    uint32_t collision_off[3];
    uint32_t passage_off[3];
    uint32_t attribute_off;
    uint32_t transport_off;
    uint32_t objects_off;
    uint32_t object_count;
};
struct MapRec { int32_t map_id; uint32_t submap; };
struct TransportRec { int32_t id, group, landing_group, type; uint32_t message; };
struct GroupRec { int32_t id; uint32_t mask; };
struct ObjectRec {
    uint8_t type, layer;
    int16_t tx, ty;
    uint16_t tw, th;
    int32_t a, b, c, d;
    uint32_t name, group;
};
#pragma pack(pop)
static_assert(sizeof(Header) == 64, "header layout");
static_assert(sizeof(SubmapRec) == 56, "submap layout");
static_assert(sizeof(ObjectRec) == 34, "object layout");

const uint8_t* g_data = nullptr;
size_t g_size = 0;
const Header* g_header = nullptr;
std::vector<Submap>* g_submaps = nullptr;
std::unordered_map<int, int>* g_by_map_id = nullptr;        // map id -> submap index
std::map<std::wstring, int>* g_by_name = nullptr;
std::vector<TransportRow>* g_transport = nullptr;
std::unordered_map<int, uint32_t>* g_tgroups = nullptr;
std::unordered_map<int, uint32_t>* g_lgroups = nullptr;
std::unordered_map<int, Grids*>* g_grids = nullptr;          // submap index -> decoded

bool fits(uint32_t off, size_t bytes) { return off <= g_size && bytes <= g_size - off; }

template <typename T>
const T* at(uint32_t off, size_t count = 1)
{
    return fits(off, sizeof(T) * count) ? reinterpret_cast<const T*>(g_data + off) : nullptr;
}

std::wstring utf8_to_wide(const char* s, int len)
{
    if (len <= 0) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s, len, nullptr, 0);
    if (n <= 0) return {};
    std::wstring out(static_cast<size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s, len, out.data(), n);
    return out;
}

/// RLE (u16 run, u8 value)* -> `cells` of width*height bytes. False when truncated.
bool decode_rle(uint32_t off, size_t cells, std::vector<uint8_t>& out)
{
    out.assign(cells, 0);
    size_t filled = 0;
    uint32_t p = off;
    while (filled < cells) {
        if (!fits(p, 3)) return false;
        uint16_t run;
        std::memcpy(&run, g_data + p, 2);
        uint8_t v = g_data[p + 2];
        p += 3;
        if (run == 0) return false;
        size_t n = run;
        if (filled + n > cells) n = cells - filled;
        std::memset(out.data() + filled, v, n);
        filled += n;
    }
    return true;
}

} // namespace

bool load(const void* data, size_t size)
{
    if (!data || size < sizeof(Header)) return false;
    const Header* h = static_cast<const Header*>(data);
    if (std::memcmp(h->magic, "FFPRMAP1", 8) != 0 || h->version != 1) return false;
    g_data = static_cast<const uint8_t*>(data);
    g_size = size;
    g_header = h;

    const SubmapRec* subs = at<SubmapRec>(h->submaps_off, h->submap_count);
    const MapRec* maps = at<MapRec>(h->maps_off, h->map_count);
    const TransportRec* tr = at<TransportRec>(h->transport_off, h->transport_count);
    const GroupRec* tg = at<GroupRec>(h->tgroup_off, h->tgroup_count);
    const GroupRec* lg = at<GroupRec>(h->lgroup_off, h->lgroup_count);
    const uint32_t* strs = at<uint32_t>(h->strings_off, h->strings_count);
    if (!subs || !maps || !tr || !tg || !lg || !strs) {
        g_data = nullptr;
        g_header = nullptr;
        return false;
    }

    g_submaps = new std::vector<Submap>();
    g_by_map_id = new std::unordered_map<int, int>();
    g_by_name = new std::map<std::wstring, int>();
    g_transport = new std::vector<TransportRow>();
    g_tgroups = new std::unordered_map<int, uint32_t>();
    g_lgroups = new std::unordered_map<int, uint32_t>();
    g_grids = new std::unordered_map<int, Grids*>();

    g_submaps->reserve(h->submap_count);
    for (uint32_t i = 0; i < h->submap_count; ++i) {
        const SubmapRec& r = subs[i];
        Submap s{};
        s.name = r.name;
        s.first_map_id = r.first_map_id;
        s.width = r.width;
        s.height = r.height;
        s.world = (r.flags & 1) != 0;
        s.layer_mask = r.layer_mask;
        s.transport_layers = r.transport_layers;
        for (int l = 0; l < 3; ++l) {
            s.collision_off[l] = r.collision_off[l];
            s.passage_off[l] = r.passage_off[l];
        }
        s.attribute_off = r.attribute_off;
        s.transport_off = r.transport_off;
        s.objects_off = r.objects_off;
        s.object_count = r.object_count;
        g_submaps->push_back(s);
        (*g_by_name)[str(r.name)] = static_cast<int>(i);
    }
    for (uint32_t i = 0; i < h->map_count; ++i)
        if (maps[i].submap < h->submap_count) (*g_by_map_id)[maps[i].map_id] = static_cast<int>(maps[i].submap);
    for (uint32_t i = 0; i < h->transport_count; ++i)
        g_transport->push_back(TransportRow{tr[i].id, tr[i].group, tr[i].landing_group, tr[i].type, tr[i].message});
    for (uint32_t i = 0; i < h->tgroup_count; ++i) (*g_tgroups)[tg[i].id] = tg[i].mask;
    for (uint32_t i = 0; i < h->lgroup_count; ++i) (*g_lgroups)[lg[i].id] = lg[i].mask;
    return true;
}

bool loaded() { return g_header != nullptr; }
int game_number() { return g_header ? g_header->game : 0; }

std::wstring str(uint32_t index)
{
    if (!g_header || index >= g_header->strings_count) return {};
    const uint32_t* offs = at<uint32_t>(g_header->strings_off, g_header->strings_count);
    if (!offs) return {};
    uint32_t off = offs[index];
    if (off >= g_size) return {};
    const char* s = reinterpret_cast<const char*>(g_data + off);
    size_t max = g_size - off;
    size_t len = 0;
    while (len < max && s[len]) ++len;
    return utf8_to_wide(s, static_cast<int>(len));
}

const Submap* submap_by_map_id(int map_id)
{
    if (!g_by_map_id) return nullptr;
    auto it = g_by_map_id->find(map_id);
    return it == g_by_map_id->end() ? nullptr : &(*g_submaps)[static_cast<size_t>(it->second)];
}

const Submap* submap_by_name(const std::wstring& name)
{
    if (!g_by_name) return nullptr;
    auto it = g_by_name->find(name);
    return it == g_by_name->end() ? nullptr : &(*g_submaps)[static_cast<size_t>(it->second)];
}

const Submap* submap_at(int index)
{
    if (!g_submaps || index < 0 || index >= static_cast<int>(g_submaps->size())) return nullptr;
    return &(*g_submaps)[static_cast<size_t>(index)];
}

int submap_count() { return g_submaps ? static_cast<int>(g_submaps->size()) : 0; }

int submap_index(const Submap* s)
{
    if (!g_submaps || !s) return -1;
    return static_cast<int>(s - g_submaps->data());
}

std::wstring name_of(const Submap& s) { return str(s.name); }

const Grids& grids(const Submap& s)
{
    static Grids empty;
    int idx = submap_index(&s);
    if (idx < 0) return empty;
    auto it = g_grids->find(idx);
    if (it != g_grids->end()) return *it->second;

    Grids* g = new Grids();
    g->width = s.width;
    g->height = s.height;
    const size_t cells = static_cast<size_t>(s.width) * s.height;
    if (cells) {
        for (int l = 0; l < 3; ++l) {
            if (s.collision_off[l] && !decode_rle(s.collision_off[l], cells, g->collision[l])) g->collision[l].clear();
            if (s.passage_off[l] && !decode_rle(s.passage_off[l], cells, g->passage[l])) g->passage[l].clear();
        }
        if (s.attribute_off && !decode_rle(s.attribute_off, cells, g->attribute)) g->attribute.clear();
        if (s.transport_off && s.transport_layers) {
            uint32_t p = s.transport_off;
            for (int i = 0; i < s.transport_layers; ++i) {
                const uint32_t* len = at<uint32_t>(p);
                if (!len) break;
                std::vector<uint8_t> layer;
                if (!decode_rle(p + 4, cells, layer)) break;
                g->transport.push_back(std::move(layer));
                p += 4 + *len;
            }
        }
    }
    if (s.objects_off && s.object_count) {
        const ObjectRec* recs = at<ObjectRec>(s.objects_off, s.object_count);
        if (recs) {
            g->objects.reserve(s.object_count);
            for (uint32_t i = 0; i < s.object_count; ++i) {
                const ObjectRec& r = recs[i];
                Object o{};
                o.type = static_cast<ObjType>(r.type);
                o.layer = r.layer > 2 ? 1 : r.layer;
                o.tx = r.tx;
                o.ty = r.ty;
                o.tw = r.tw ? r.tw : 1;
                o.th = r.th ? r.th : 1;
                o.a = r.a;
                o.b = r.b;
                o.c = r.c;
                o.d = r.d;
                o.name = r.name;
                o.group = r.group;
                g->objects.push_back(o);
            }
        }
    }
    // Step-on warps and trapdoors: cells an ordinary route must not cross.
    if (cells) {
        for (const Object& o : g->objects) {
            if (o.type != ObjType::Warp || o.d != 1) continue;
            if (g->hazard.empty()) g->hazard.assign(cells, 0);
            for (int y = o.ty; y < o.ty + o.th; ++y)
                for (int x = o.tx; x < o.tx + o.tw; ++x)
                    if (x >= 0 && y >= 0 && x < g->width && y < g->height)
                        g->hazard[static_cast<size_t>(y) * g->width + x] = 1;
        }
    }
    (*g_grids)[idx] = g;
    return *g;
}

bool in_bounds(const Grids& g, int x, int y) { return x >= 0 && y >= 0 && x < g.width && y < g.height; }

bool walkable(const Grids& g, int layer, int x, int y, bool passages)
{
    if (layer < 0 || layer > 2 || !in_bounds(g, x, y)) return false;
    const std::vector<uint8_t>& c = g.collision[layer];
    if (c.empty()) return false;
    const size_t i = static_cast<size_t>(y) * g.width + x;
    if (c[i] == 0) return true;
    return passages && !g.passage[layer].empty() && g.passage[layer][i] != 0;
}

bool is_passage(const Grids& g, int layer, int x, int y)
{
    if (layer < 0 || layer > 2 || !in_bounds(g, x, y) || g.passage[layer].empty()) return false;
    return g.passage[layer][static_cast<size_t>(y) * g.width + x] != 0;
}

bool vehicle_open(const Grids& g, int group, int x, int y)
{
    if (group < 1 || group > static_cast<int>(g.transport.size()) || !in_bounds(g, x, y)) return false;
    return g.transport[static_cast<size_t>(group - 1)][static_cast<size_t>(y) * g.width + x] == 0;
}

int attribute(const Grids& g, int x, int y)
{
    if (g.attribute.empty() || !in_bounds(g, x, y)) return 0;
    return g.attribute[static_cast<size_t>(y) * g.width + x];
}

const TransportRow* transport(int transportation_id)
{
    if (!g_transport) return nullptr;
    for (const TransportRow& r : *g_transport)
        if (r.id == transportation_id) return &r;
    return nullptr;
}

const std::vector<TransportRow>& transports()
{
    static const std::vector<TransportRow> none;
    return g_transport ? *g_transport : none;
}

bool group_crosses(int group, int attr)
{
    if (!g_tgroups || attr < 1 || attr > 32) return false;
    auto it = g_tgroups->find(group);
    return it != g_tgroups->end() && (it->second & (1u << (attr - 1))) != 0;
}

bool group_lands(int landing_group, int attr)
{
    if (!g_lgroups || attr < 1 || attr > 32) return false;
    auto it = g_lgroups->find(landing_group);
    return it != g_lgroups->end() && (it->second & (1u << (attr - 1))) != 0;
}

} // namespace field::baked
