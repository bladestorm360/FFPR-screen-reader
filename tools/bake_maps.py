"""
Bake one compact binary per game from the map dumps (tools/mapdump) for the native mod's
baked-map router: ffpr-screen-reader/data/<game>/maps.bin, embedded in hid.dll as MAPS_FFN.

Usage:
  python bake_maps.py <ff1|ff2|ff3|ff4|ff5|ff6|all> [--out <dir>] [--check]
  (plain `python`; reads tools/mapdump/<game>_grids.json, _objects.jsonl, _submaps.json and
   <game>_master/{map,transportation,transportation_group,landing_group}.txt)

What is baked, per sub-map (the Tiled sub-map "Map_20011_1"; a world map is "Map_10010"):
  * the collision grids per layer (Bottom 0, Ground 1, Upper 2) as OPEN / BLOCKED cells
    (CollisionId 0 is open; MapRouteSearcher.MakeRouteMapWithCollision marks EVERY non-zero
    id NoEnter: FF2 GameAssembly.dll 0xA987B0, decompiled 2026-10-10);
  * the hidden-passage grids (cells the player walks that the game's searcher never enters);
  * world maps: the terrain attribute ids (one per cell, 1-32) and the per-transportation-GROUP
    passability layers "Transportation01".."NN" (0 open, 1 blocked: the riding route grid);
  * the routing objects: entry points (PointIn), exits (GotoMap), layer changes (ToLayer),
    counters (RelayInteraction), talk targets (NPC, ShopNPC, Entity, AnimEntity,
    TreasureBox), teleports (TelepoPoint), vehicle pads (TransportationEventAction),
    events, open triggers, collision entities, save points and IgnoreRoute / MoveArea rects.
Plus the masters the router needs: map id -> sub-map (map.txt: id, map_name), the
transportation rows (id, group, landing group, type, message id), the transportation groups'
32 terrain-type flags and the landing groups' flags.

Binary layout (little-endian; every offset is from the start of the file):
  header   "FFPRMAP1" u32 version(1) u8 game u8[3] pad
           u32 strings_off u32 strings_count      (u32 off per string; UTF-8, NUL-terminated)
           u32 submaps_off u32 submap_count
           u32 maps_off    u32 map_count           ({i32 map_id, u32 submap_index})
           u32 transport_off u32 transport_count   ({i32 id, i32 group, i32 landing_group,
                                                     i32 type, u32 message_str})
           u32 tgroup_off  u32 tgroup_count        ({i32 id, u32 mask32}: bit n-1 = type n ok)
           u32 lgroup_off  u32 lgroup_count        ({i32 id, u32 mask32})
  submap   u32 name_str i32 first_map_id u16 w u16 h u8 flags(bit0 world) u8 layer_mask
           u8 transport_layers u8 pad
           u32 collision_off[3] u32 passage_off[3] u32 attribute_off u32 transport_off
           u32 objects_off u32 object_count
  grid     RLE of bytes: (u16 run_length, u8 value)* until w*h cells; offset 0 = absent.
           Layer order Bottom, Ground, Upper. The transport block is transport_layers grids
           back to back, each prefixed with u32 byte size, group 1 first.
  object   u8 type u8 layer i16 tx i16 ty u16 tw u16 th i32 a i32 b i32 c i32 d
           u32 name_str u32 group_str                (34 bytes; fields per type below)
Object fields: PointIn a=point(object_id) b=direction c=entity_id; GotoMap a=dest map_id
b=point_id c=entity_id d=direction; ToLayer a=target layer b=direction c=entity_id;
RelayInteraction a=object_id c=entity_id; NPC/ShopNPC/Entity/AnimEntity/TreasureBox
a=link_object_id b=accept_action_direction c=entity_id d=flags(1 collision, 2 all_layers,
4 script, 8 action, 16 wanders; a TreasureBox also carries content_id << 8 in d);
Warp (baked only: an Event / Entity whose script's Main runs ChangeMap, unless the destination's
continuation script changes map straight back, a cutscene) a=dest map_id b=dest point_id
c=entity_id d=step_on(1 Event, 0 examined); Event/TransportationEventAction a=script_id b=action_id c=entity_id
d=ignore_route_link_object_id; OpenTrigger a=link_object_id c=entity_id; TelepoPoint
a=goto_entity_id b=pointin_entity_id c=entity_id; CollisionEntity a=collision_type
b=through_condition_type c=entity_id, name_str=target_transportation_ids; SavePoint
c=entity_id; IgnoreRoute / MoveArea / NonEncountArea: the rect only.
Read-only on the dumps. The C++ reader is ffpr-screen-reader/src/core/field/baked_map.cpp.
"""
import json
import os
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
MAPDUMP = os.path.abspath(os.path.join(HERE, "..", "..", "tools", "mapdump"))
DEFAULT_OUT = os.path.join(os.path.dirname(HERE), "data")
LAYER_NAMES = ["Bottom", "Ground", "Upper"]
GAMES = ["ff1", "ff2", "ff3", "ff4", "ff5", "ff6"]

# MapConstants.ObjectType (dump_map_objects.OBJ_TYPE_NAME); the baked type byte is the same id.
BAKED_TYPES = {
    "PointIn": 0, "GotoMap": 3, "ToLayer": 4, "Entity": 5, "NPC": 6, "TreasureBox": 7,
    "OpenTrigger": 8, "SavePoint": 9, "AnimEntity": 10, "MoveArea": 13,
    "RelayInteraction": 16, "ShopNPC": 17, "CollisionEntity": 19, "TelepoPoint": 20,
    "TransportationEventAction": 21, "IgnoreRoute": 22, "NonEncountArea": 23,
    "Event": 2, "RandomEvent": 28, "SwitchEvent": 30,
    "Warp": 100,   # baked only: an event whose script changes map (a=map_id b=point_id d=step_on)
}
TALK_TYPES = {"NPC", "ShopNPC", "Entity", "AnimEntity", "TreasureBox"}


def read_csv(path):
    """Header and rows. The dumped masters end their lines in CR CR LF (dump_masters.py wrote the
    game's CRLF text through a text-mode file), so blank lines appear between rows; they are
    dropped, and a row shorter than the header is padded (trailing empty columns)."""
    rows = [l.strip("\r\n").split(",") for l in open(path, encoding="utf-8") if l.strip()]
    header = rows[0]
    body = [r + [""] * (len(header) - len(r)) for r in rows[1:] if r and r[0].strip()]
    return header, body


def to_int(v, default=0):
    try:
        if v in (None, "", "None"):
            return default
        if isinstance(v, bool):
            return int(v)
        return int(v)
    except (TypeError, ValueError):
        try:
            return int(float(v))
        except (TypeError, ValueError):
            return default


class Strings:
    def __init__(self):
        self.list = [""]
        self.index = {"": 0}

    def add(self, s):
        s = s or ""
        if s not in self.index:
            self.index[s] = len(self.list)
            self.list.append(s)
        return self.index[s]


def rle(cells):
    """(u16 run, u8 value)* over a bytes-like of cell values."""
    out = bytearray()
    n = len(cells)
    i = 0
    while i < n:
        v = cells[i]
        j = i + 1
        while j < n and cells[j] == v and j - i < 65535:
            j += 1
        out += struct.pack("<HB", j - i, v)
        i = j
    return bytes(out)



def tile_row(o):
    """The top tile row of a dumped object.

    Tiled anchors a TILE object (one with a gid: NPCs, chests, exits, relays, switches) at its
    BOTTOM-left corner, a rectangle object at its top-left. dump_map_objects.py stores ty = y / 16
    for both, so every tile object came out one row too far south: Bafsk's clerks were baked at
    row 7 while the live entities stand at row 6, and no counter relay matched (user,
    2026-10-11: "Item Shop Clerk 2 ... No path"). The live positions were right all along.
    """
    y, h = to_int(o.get("y")), to_int(o.get("h"))
    if o.get("gid") is not None:
        return (y - h) // 16
    return y // 16

def layer_index(name):
    for i, n in enumerate(LAYER_NAMES):
        if (name or "").startswith(n):
            return i
    return -1


def collision_rows(coll, li):
    """The collision rows of layer index `li` in a sub-map's collision asset, or None.

    The layers are normally "BottomCollision" / "GroundCollision" / "UpperCollision", but the
    asset is the collision asset, so every tile layer in it is collision data whatever it is
    called: FF2's Map_20071_1 (Josef's house in Salamand) and 13 FF3 maps name theirs
    "GroundObject", and requiring the "Collision" suffix baked them with no open cell at all
    ("No path" to everything in the house; user, 2026-10-11). The suffixed name wins when a
    layer index has both.
    """
    exact = LAYER_NAMES[li] + "Collision"
    if coll.get(exact):
        return coll[exact]
    for ln, rows in coll.items():
        if layer_index(ln) == li and rows:
            return rows
    return None


def grid_cells(rows, w, h, wide):
    """Rows of hex digits (one per cell, or two with `wide`) -> list of ints, row-major."""
    cells = []
    for y in range(h):
        row = rows[y] if y < len(rows) else ""
        for x in range(w):
            if wide:
                t = row[2 * x:2 * x + 2]
                cells.append(int(t, 16) if len(t) == 2 and t != "??" else 0)
            else:
                c = row[x] if x < len(row) else "0"
                cells.append(int(c, 16) if c != "?" else 1)
    return cells


def bake_game(game, out_dir, check=False):
    grids = json.load(open(os.path.join(MAPDUMP, f"{game}_grids.json"), encoding="utf-8"))
    submaps = json.load(open(os.path.join(MAPDUMP, f"{game}_submaps.json"), encoding="utf-8"))
    objects = [json.loads(l) for l in open(os.path.join(MAPDUMP, f"{game}_objects.jsonl"), encoding="utf-8")]
    master = os.path.join(MAPDUMP, f"{game}_master")

    strings = Strings()
    names = sorted(submaps.keys())
    sub_index = {n: i for i, n in enumerate(names)}

    # --- masters -------------------------------------------------------------------------
    h, rows = read_csv(os.path.join(master, "map.txt"))
    col = {c: i for i, c in enumerate(h)}
    maps = []
    first_map_id = {}
    submap_of_mapid = {}   # map master id -> sub-map name (map.txt map_name)
    mapids_of_submap = {}  # sub-map name -> every map master id that uses it
    for r in rows:
        mid, mname = to_int(r[col["id"]]), r[col["map_name"]]
        if mname in sub_index:
            maps.append((mid, sub_index[mname]))
            first_map_id.setdefault(mname, mid)
            submap_of_mapid[mid] = mname
            mapids_of_submap.setdefault(mname, set()).add(mid)
    maps.sort()

    h, rows = read_csv(os.path.join(master, "transportation.txt"))
    col = {c: i for i, c in enumerate(h)}
    transport = []
    for r in rows:
        transport.append((to_int(r[col["id"]]), to_int(r[col["transportation_group_id"]]),
                          to_int(r[col["landing_group_id"]]), to_int(r[col["type_id"]]),
                          strings.add("" if r[col["message_id"]] == "None" else r[col["message_id"]])))

    def flag_rows(fname):
        hh, rr = read_csv(os.path.join(master, fname))
        out = []
        for r in rr:
            mask = 0
            for i in range(1, min(len(r), 33)):
                if r[i].strip() == "1":
                    mask |= 1 << (i - 1)
            out.append((to_int(r[0]), mask))
        return out

    tgroups = flag_rows("transportation_group.txt")
    lgroups = flag_rows("landing_group.txt")

    # --- warps: an event whose script changes map (2026-10-10) ------------------------------
    # script.txt maps an object's script_id to its asset name; a script whose Main segment
    # runs ChangeMap [map_id, point_id] is a teleport: FF1's Citadel of Trials pads
    # (sc_war_30071_N), trapdoors ("落とし穴"), Pandaemonium's warp zones. A step-on Event is
    # a hazard for ordinary routing (the player would be whisked away) and an edge for the
    # cross-map router; an examined Entity is only an edge.
    #
    # NOT a warp (2026-10-11): a cutscene that comes straight back. FF1's opening-credits Event
    # on the Cornelia bridge (Map_10010 (145,145)) runs ChangeMap to the scene map 124, whose
    # arrival script sc_e_0006_1 runs ChangeMap back to the world map: the player ends where
    # they stepped. Baked as a step-on warp it closed the only bridge cell, and every place
    # north of the bridge (Matoya's Cave, Pravoka) was "Requires vehicle" (user, 2026-10-11).
    # The rule: when the ChangeMap's continuation script (its string operand), looked up on the
    # destination map, itself runs ChangeMap to a map id of the ORIGIN sub-map, the event is a
    # round trip and stays a plain Event. Trapdoors, warp pads and script exits ("to the world
    # map") never come back, and a one-way story warp (FF5 "to the Interdimensional Rift",
    # FF4 Devil's Road) goes on to another map: all of those stay warps.
    script_names = {}
    try:
        hs, rs = read_csv(os.path.join(master, "script.txt"))
        cs = {c: i for i, c in enumerate(hs)}
        for r in rs:
            script_names[to_int(r[cs["id"]])] = r[cs["script_name"]]
    except (OSError, KeyError):
        pass
    scripts_by_key = {}   # (submap, script name) -> the dumped script
    scripts_path = os.path.join(MAPDUMP, f"{game}_scripts.jsonl")
    if os.path.exists(scripts_path):
        for l in open(scripts_path, encoding="utf-8"):
            s = json.loads(l)
            scripts_by_key[(s["submap"], s["name"])] = s

    def main_change_map(s):
        """(map_id, point_id, continuation script name) of the first ChangeMap in the script's
        Main segment, or None."""
        segs = s.get("segments") or []
        main_count = next((seg.get("Count", 0) for seg in segs if seg.get("Label") == "Main"), None)
        ms = s.get("mnemonics") or []
        for m in (ms[:main_count] if main_count else ms):
            if m.get("mnemonic") == "ChangeMap":
                ops = m.get("operands") or {}
                iv = ops.get("iValues") or []
                sv = ops.get("sValues") or []
                if len(iv) >= 2 and iv[0] > 0:
                    return int(iv[0]), int(iv[1]), (str(sv[0]) if sv else "")
        return None

    def change_map_targets(s):
        """Every map id a ChangeMap anywhere in the script (any segment) goes to."""
        out = set()
        for m in s.get("mnemonics") or []:
            if m.get("mnemonic") == "ChangeMap":
                iv = (m.get("operands") or {}).get("iValues") or []
                if iv and iv[0] > 0:
                    out.add(int(iv[0]))
        return out

    def round_trip(origin_submap, dest_map_id, continuation):
        """True when the continuation script on the destination map changes map back to a map
        id of the origin sub-map: a cutscene, not a warp. Unknown continuation: False (a warp)."""
        if not continuation:
            return False
        dest_sub = submap_of_mapid.get(dest_map_id)
        cont = scripts_by_key.get((dest_sub, continuation)) if dest_sub else None
        if cont is None:
            return False
        return bool(change_map_targets(cont) & mapids_of_submap.get(origin_submap, set()))

    warps = 0
    scenes = 0

    # --- objects per sub-map ---------------------------------------------------------------
    by_sub = {}
    for o in objects:
        t = o["type_name"]
        if t not in BAKED_TYPES:
            continue
        by_sub.setdefault(o["submap"], []).append(o)
        sid = to_int((o["props"] or {}).get("script_id"))
        if sid and t in ("Event", "Entity", "AnimEntity"):
            s = scripts_by_key.get((o["submap"], script_names.get(sid, "")))
            dest = main_change_map(s) if s else None
            if dest and round_trip(o["submap"], dest[0], dest[2]):
                scenes += 1   # a cutscene that returns: the cell stays open, no edge
            elif dest:
                w = dict(o)
                w["type_name"] = "Warp"
                w["props"] = {"entity_id": (o["props"] or {}).get("entity_id"), "map_id": dest[0],
                              "point_id": dest[1], "step_on": 1 if t == "Event" else 0}
                by_sub[o["submap"]].append(w)
                warps += 1

    def pack_object(o):
        t = o["type_name"]
        p = o["props"] or {}
        a = b = c = d = 0
        name = o.get("name") or ""
        c = to_int(p.get("entity_id"))
        if t == "PointIn":
            a, b = to_int(p.get("object_id")), to_int(p.get("direction"))
        elif t == "GotoMap":
            a, b, d = to_int(p.get("map_id")), to_int(p.get("point_id")), to_int(p.get("direction"))
        elif t == "ToLayer":
            a, b = to_int(p.get("layer"), -1), to_int(p.get("direction"))
        elif t == "RelayInteraction":
            a = to_int(p.get("object_id"))
        elif t in TALK_TYPES:
            a = to_int(p.get("link_object_id"))
            b = to_int(p.get("accept_action_direction"), 15) or 15
            flags = 0
            if p.get("collision") is True or (t in ("NPC", "ShopNPC") and p.get("collision") is not False):
                flags |= 1
            if p.get("all_layers"):
                flags |= 2
            if to_int(p.get("script_id")):
                flags |= 4
            if to_int(p.get("action_id")):
                flags |= 8
            if t in ("NPC", "ShopNPC") and to_int(p.get("move_type")) not in (0, 1) and p.get("move_step"):
                flags |= 16   # Native None/Stamp remain on their tile; Area/Route/Following travel.
            d = flags
            if t == "TreasureBox":
                # The chest's content id above the flag bits (2026-10-11): a chest on another
                # floor is listed by its contents, as the live list names chests.
                d |= max(0, to_int(p.get("content_id"))) << 8
        elif t in ("Event", "TransportationEventAction", "RandomEvent", "SwitchEvent"):
            a, b = to_int(p.get("script_id")), to_int(p.get("action_id"))
            d = to_int(p.get("ignore_route_link_object_id"), -1)
            if t == "TransportationEventAction":
                name = str(p.get("target_transportation_ids") or "") + "|" + name
        elif t == "OpenTrigger":
            a = to_int(p.get("link_object_id"))
        elif t == "TelepoPoint":
            a, b = to_int(p.get("goto_entity_id")), to_int(p.get("pointin_entity_id"))
        elif t == "CollisionEntity":
            a, b = to_int(p.get("collision_type")), to_int(p.get("through_condition_type"))
            name = str(p.get("target_transportation_ids") or "")
        elif t == "Warp":
            a, b, d = to_int(p.get("map_id")), to_int(p.get("point_id")), to_int(p.get("step_on"))
        layer = layer_index(o.get("layer"))
        if layer < 0:
            layer = 1
        tw = max(1, to_int(o.get("w")) // 16)
        th = max(1, to_int(o.get("h")) // 16)
        return struct.pack("<BBhhHHiiiiII", BAKED_TYPES[t], layer, to_int(o.get("tx")), tile_row(o),
                           tw, th, a, b, c, d, strings.add(name), strings.add(o.get("group") or ""))

    # --- sub-map blobs -------------------------------------------------------------------
    blobs = bytearray()          # every grid / object table, appended; offsets patched later
    sub_recs = []
    stats = {"submaps": 0, "world": 0, "cells": 0, "objects": 0, "bytes": 0}

    def put(data):
        off = len(blobs)
        blobs.extend(data)
        return off

    for n in names:
        info = submaps[n]
        g = grids.get(n) or {}
        w, h_ = to_int(info.get("width")), to_int(info.get("height"))
        coll = g.get("collision") or {}
        passage = g.get("hidden_passage") or {}
        attribute = g.get("attribute") or {}
        transport_layers = g.get("transportation") or {}
        is_world = bool(attribute) and not coll
        layer_mask = 0
        coll_off = [0, 0, 0]
        pass_off = [0, 0, 0]
        if w and h_:
            for li in range(3):
                rows = collision_rows(coll, li)
                if rows is None:
                    continue
                cells = grid_cells(rows, w, h_, False)
                coll_off[li] = put(rle(bytes(1 if c else 0 for c in cells)))
                layer_mask |= 1 << li
                stats["cells"] += w * h_
            for ln, rows in passage.items():
                li = layer_index(ln)
                if li < 0:
                    continue
                cells = grid_cells(rows, w, h_, True)
                pass_off[li] = put(rle(bytes(1 if c else 0 for c in cells)))
        attr_off = 0
        if attribute and w and h_:
            rows = next(iter(attribute.values()))
            attr_off = put(rle(bytes(min(255, c) for c in grid_cells(rows, w, h_, True))))
        tr_off = 0
        tr_count = 0
        if transport_layers and w and h_:
            # "Transportation01".."NN": group n is layer n (1-based), in order.
            ordered = sorted(transport_layers.items(), key=lambda kv: kv[0])
            block = bytearray()
            for ln, rows in ordered:
                data = rle(bytes(1 if c else 0 for c in grid_cells(rows, w, h_, False)))
                block += struct.pack("<I", len(data)) + data
                tr_count += 1
            tr_off = put(bytes(block))
        objs = by_sub.get(n) or []
        obj_blob = b"".join(pack_object(o) for o in objs)
        obj_off = put(obj_blob) if objs else 0
        sub_recs.append((strings.add(n), first_map_id.get(n, -1), w, h_, 1 if is_world else 0, layer_mask,
                         tr_count, coll_off, pass_off, attr_off, tr_off, obj_off, len(objs)))
        stats["submaps"] += 1
        stats["world"] += 1 if is_world else 0
        stats["objects"] += len(objs)

    # --- assemble ------------------------------------------------------------------------
    HEADER = struct.calcsize("<8sIB3sIIIIIIIIIIII")
    SUBMAP = struct.calcsize("<IihhBBBBIIIIIIIIII")
    sub_table_off = HEADER
    maps_off = sub_table_off + SUBMAP * len(sub_recs)
    transport_off = maps_off + 8 * len(maps)
    tgroup_off = transport_off + 20 * len(transport)
    lgroup_off = tgroup_off + 8 * len(tgroups)
    strings_off = lgroup_off + 8 * len(lgroups)
    # string table: u32 offsets then the UTF-8 bytes
    str_bytes = bytearray()
    str_offs = []
    base = strings_off + 4 * len(strings.list)
    for s in strings.list:
        str_offs.append(base + len(str_bytes))
        str_bytes += s.encode("utf-8") + b"\0"
    blob_base = base + len(str_bytes)
    blob_base += (-blob_base) % 4

    out = bytearray()
    out += struct.pack("<8sIB3sIIIIIIIIIIII", b"FFPRMAP1", 1, GAMES.index(game) + 1, b"\0\0\0",
                       strings_off, len(strings.list), sub_table_off, len(sub_recs), maps_off, len(maps),
                       transport_off, len(transport), tgroup_off, len(tgroups), lgroup_off, len(lgroups))
    assert len(out) == HEADER

    def rel(off):
        return blob_base + off if off else 0

    for (name_i, mid, w, h_, flags, lmask, trc, coff, poff, aoff, troff, ooff, ocount) in sub_recs:
        out += struct.pack("<IihhBBBBIIIIIIIIII", name_i, mid, w, h_, flags, lmask, trc, 0,
                           rel(coff[0]), rel(coff[1]), rel(coff[2]), rel(poff[0]), rel(poff[1]), rel(poff[2]),
                           rel(aoff), rel(troff), rel(ooff) if ocount else 0, ocount)
    for mid, si in maps:
        out += struct.pack("<iI", mid, si)
    for row in transport:
        out += struct.pack("<iiiiI", *row)
    for gid, mask in tgroups:
        out += struct.pack("<iI", gid, mask)
    for gid, mask in lgroups:
        out += struct.pack("<iI", gid, mask)
    assert len(out) == strings_off
    for o in str_offs:
        out += struct.pack("<I", o)
    out += str_bytes
    out += b"\0" * (blob_base - len(out))
    out += blobs
    stats["bytes"] = len(out)

    os.makedirs(os.path.join(out_dir, game), exist_ok=True)
    path = os.path.join(out_dir, game, "maps.bin")
    with open(path, "wb") as f:
        f.write(out)
    print(f"{game}: {stats['submaps']} sub-maps ({stats['world']} world), {stats['objects']} objects "
          f"({warps} warps, {scenes} round-trip cutscenes kept as events), {stats['cells']} collision cells "
          f"-> {stats['bytes']} bytes: {path}")
    if check:
        verify(path, grids, names)


def verify(path, grids, names):
    """Decode every collision grid back and compare with the dump (the RLE round trip)."""
    data = open(path, "rb").read()
    hdr = struct.unpack_from("<8sIB3sIIIIIIIIIIII", data, 0)
    sub_off, count = hdr[6], hdr[7]
    SUBMAP = struct.calcsize("<IihhBBBBIIIIIIIIII")
    bad = 0
    for i in range(count):
        rec = struct.unpack_from("<IihhBBBBIIIIIIIIII", data, sub_off + i * SUBMAP)
        w, h = rec[2], rec[3]
        n = names[i]
        coll = (grids.get(n) or {}).get("collision") or {}
        for li, ln in enumerate(LAYER_NAMES):
            off = rec[8 + li]
            rows = collision_rows(coll, li)
            if not off:
                if rows:
                    bad += 1
                continue
            cells = []
            p = off
            while len(cells) < w * h:
                run, v = struct.unpack_from("<HB", data, p)
                p += 3
                cells.extend([v] * run)
            want = [1 if c else 0 for c in grid_cells(rows, w, h, False)]
            if cells != want:
                bad += 1
                print("  mismatch:", n, ln)
    print("  verify:", "OK" if not bad else f"{bad} grid(s) differ")


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    if not args:
        print(__doc__)
        sys.exit(1)
    out_dir = DEFAULT_OUT
    if "--out" in sys.argv:
        out_dir = sys.argv[sys.argv.index("--out") + 1]
    games = GAMES if args[0] == "all" else [args[0]]
    for g in games:
        bake_game(g, out_dir, check="--check" in sys.argv)


if __name__ == "__main__":
    main()
