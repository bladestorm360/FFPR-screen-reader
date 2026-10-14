"""Full offline fresh-asset/bake comparison. Outputs remain in ignored build_check."""
import collections
import hashlib
import json
import pathlib
import shutil
import struct
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
import bake_maps as bake

DEST = ROOT / 'build_check/map_audit_2026_10_14'
FRESH = DEST / 'fresh'
OLD = pathlib.Path(bake.MAPDUMP)
SUB = struct.Struct('<IihhBBBB10I')

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def unpack_grid(data, off, size):
    result = bytearray()
    while len(result) < size:
        count, value = struct.unpack_from('<HB', data, off)
        assert count > 0 and len(result) + count <= size
        result.extend(bytes([value]) * count)
        off += 3
    return bytes(result)

report = []
for game in bake.GAMES:
    record = {'game': game, 'source_comparison': {}, 'rooms': [], 'mapped_native_no_grid': []}
    for suffix in ('submaps.json', 'grids.json', 'objects.jsonl', 'scripts.jsonl'):
        a, b = OLD / (game + '_' + suffix), FRESH / (game + '_' + suffix)
        if suffix.endswith('jsonl'):
            left = [json.loads(s) for s in a.read_text(encoding='utf-8').splitlines()]
            right = [json.loads(s) for s in b.read_text(encoding='utf-8').splitlines()]
        else:
            left = json.loads(a.read_text(encoding='utf-8'))
            right = json.loads(b.read_text(encoding='utf-8'))
        assert left == right, game + ' fresh shipped-asset extraction differs: ' + suffix
        record['source_comparison'][suffix] = {'records': len(left), 'retained_sha256': sha(a), 'fresh_sha256': sha(b)}
        del left, right
    master = FRESH / (game + '_master')
    if not master.exists():
        shutil.copytree(OLD / (game + '_master'), master)
    bake.MAPDUMP = str(FRESH)
    bake.bake_game(game, str(DEST / 'bakes'), check=True)
    path = DEST / 'bakes' / game / 'maps.bin'
    data = path.read_bytes()
    head = struct.unpack_from('<8sIB3s12I', data)
    strings = []
    for i in range(head[5]):
        off = struct.unpack_from('<I', data, head[4] + i*4)[0]
        strings.append(data[off:data.index(0, off)].decode('utf-8'))
    submaps = json.loads((FRESH / (game + '_submaps.json')).read_text(encoding='utf-8'))
    grids = json.loads((FRESH / (game + '_grids.json')).read_text(encoding='utf-8'))
    objects = collections.defaultdict(list)
    for s in (FRESH / (game + '_objects.jsonl')).read_text(encoding='utf-8').splitlines():
        o = json.loads(s); objects[o['submap']].append(o)
    ids = collections.defaultdict(list)
    for i in range(head[9]):
        mid, si = struct.unpack_from('<iI', data, head[8] + 8*i)
        assert 0 <= si < head[7]
        ids[si].append(mid)
    cells = 0
    for i in range(head[7]):
        row = SUB.unpack_from(data, head[6] + i*SUB.size)
        name, width, height = strings[row[0]], row[2], row[3]
        raw = grids[name]
        # Independently derive ParseMapRange's signed integer pixel quotient.
        rectangles = [o for o in objects[name] if o['type_name'] == 'MapRange' and o['group'] == 'entity_default' and o.get('w') and o.get('h')]
        if rectangles:
            rect = rectangles[0]
            bounds = tuple(int(bake.to_int(rect.get(k))/16) for k in ('x', 'y', 'w', 'h'))
        else:
            bounds = (0, 0, bake.to_int(submaps[name].get('width')), bake.to_int(submaps[name].get('height')))
        assert (width, height) == bounds[2:], (game, name, 'native dimensions')
        assert bounds[0] >= 0 and bounds[1] >= 0, (game, name, 'negative native crop')
        mask = 0
        for layer in range(3):
            rows = bake.collision_rows(raw.get('collision') or {}, layer)
            if rows is None:
                assert not row[8+layer]
                continue
            want = bytes(bake.collision_masks(bake.crop_rows(rows, bounds), width, height))
            assert unpack_grid(data, row[8+layer], width*height) == want, (game, name, layer, 'collision/stair parity')
            mask |= 1 << layer; cells += width*height
        assert row[5] == mask
        for layer_name, rows in (raw.get('hidden_passage') or {}).items():
            layer = bake.layer_index(layer_name)
            if layer < 0: continue
            want = bytes(bool(v) for v in bake.grid_cells(bake.crop_rows(rows, bounds, True), width, height, True))
            assert unpack_grid(data, row[11+layer], width*height) == want, (game, name, 'passage parity')
        attributes = raw.get('attribute') or {}
        assert bool(row[14]) == bool(attributes and width and height)
        if row[14]:
            want = bytes(min(255, v) for v in bake.grid_cells(bake.crop_rows(next(iter(attributes.values())), bounds, True), width, height, True))
            assert unpack_grid(data, row[14], width*height) == want, (game, name, 'terrain parity')
        transport = sorted((raw.get('transportation') or {}).items())
        assert row[6] == len(transport)
        off = row[15]
        for _, rows in transport:
            size = struct.unpack_from('<I', data, off)[0]; off += 4
            want = bytes(bool(v) for v in bake.grid_cells(bake.crop_rows(rows, bounds), width, height, False))
            assert unpack_grid(data, off, width*height) == want, (game, name, 'transport parity')
            off += size
        entry = {'name': name, 'map_ids': ids[i], 'bounds': bounds, 'layers': mask, 'world': bool(row[4]), 'objects': row[17]}
        record['rooms'].append(entry)
        if ids[i] and not mask and not row[14]:
            entry = dict(entry)
            entry['native_objects'] = dict(collections.Counter(o['type_name'] for o in objects[name]))
            record['mapped_native_no_grid'].append(entry)
    assert len(record['rooms']) == len(submaps)
    record['collision_cells_checked'] = cells
    record['fresh_bake_sha256'] = sha(path)
    record['previous_bake_sha256'] = sha(ROOT / 'data' / game / 'maps.bin')
    record['changed'] = record['fresh_bake_sha256'] != record['previous_bake_sha256']
    report.append(record)
    (DEST / 'report.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
    print(game, len(submaps), 'rooms,', cells, 'collision cells; every grid/variant checked; changed', record['changed'], flush=True)
print('PASS fresh assets, native MapRange, every collision/stair/passage/terrain/transport grid and map-id mapping')
