"""Recover missing mapped grids from their owning Addressables bundles, offline.

The old dump can overwrite a complete submap with an unresolved cross-package
reference (FF5 Karnak Castle, FF4 Kaipo). Preserve its inputs before repair.
Uses the existing extractor and modifies only incomplete, master-referenced maps.
"""
import hashlib
import json
import re
import shutil
import sys
from pathlib import Path
import bake_maps as bake

sys.path.insert(0, str(Path(bake.MAPDUMP).parent))
import dump_map_objects as dump


def repair(game):
    root = Path(bake.MAPDUMP)
    info_path, grid_path = root / f"{game}_submaps.json", root / f"{game}_grids.json"
    info = json.loads(info_path.read_text(encoding="utf-8"))
    grids = json.loads(grid_path.read_text(encoding="utf-8"))
    header, rows = bake.read_csv(str(root / f"{game}_master/map.txt"))
    known = {row[header.index("map_name")] for row in rows}
    targets = [name for name, row in info.items() if name in known and not (row.get("width") and row.get("height"))]
    changed, unresolved, evidence = [], [], []
    for name in targets:
        owners = [re.match(r"Map_\d+", name)[0], info[name]["map"]]
        recovered = False
        for owner in dict.fromkeys(owners):
            for bundle in Path(dump.aa_path(game)).glob(owner.lower() + "_assets_all_*.bundle"):
                by_container, by_name = dump.load_texts(str(bundle))
                for package in by_name.get("package", []):
                    for row in json.loads(package).get("map", []):
                        if row.get("name") != name:
                            continue
                        tile = dump.resolve(row.get("tilemap") or "", by_container, by_name)
                        collision = dump.resolve(row.get("collision") or "", by_container, by_name)
                        if not tile:
                            continue
                        tile = json.loads(tile)
                        width, height = tile.get("width"), tile.get("height")
                        if not width or not height:
                            continue
                        grid = {"width": width, "height": height, "collision": {}, "tiles": {}}
                        dump.collect_tile_layers(tile, grid["tiles"])
                        if collision:
                            dump.collect_collision_layers(json.loads(collision), grid["collision"])
                        for key in ("hidden_passage", "attribute", "transportation", "damage"):
                            if row.get(key):
                                text = dump.resolve(row[key], by_container, by_name)
                                if text:
                                    grid[key] = {}
                                    dump.collect_collision_layers(json.loads(text), grid[key], wide=key != "transportation")
                        metadata = dict(info[name])
                        metadata.update(map=owner, width=width, height=height, tilewidth=tile.get("tilewidth"), tileheight=tile.get("tileheight"))
                        metadata["groups"] = (["entity_default"] if row.get("entity_default") else []) + [g["name"] for g in row.get("entity", [])]
                        metadata["scripts"] = [s["name"] for s in row.get("script", [])]
                        info[name], grids[name] = metadata, grid
                        changed.append(name)
                        evidence.append({"submap": name, "bundle": str(bundle), "sha256": hashlib.sha256(bundle.read_bytes()).hexdigest(), "width": width, "height": height})
                        recovered = True
                        break
                    if recovered:
                        break
                if recovered:
                    break
            if recovered:
                break
        if not recovered:
            unresolved.append(name)
    if changed:
        archive = root / "exit_audit_2026_10_13"
        archive.mkdir(exist_ok=True)
        for path in (info_path, grid_path):
            saved = archive / path.name
            if not saved.exists():
                shutil.copy2(path, saved)
        info_path.write_text(json.dumps(info, ensure_ascii=False, indent=1), encoding="utf-8")
        grid_path.write_text(json.dumps(grids, ensure_ascii=False), encoding="utf-8")
    print(game, "recovered", changed, "unresolved", unresolved)
    return {"game": game, "recovered": evidence, "unresolved": unresolved}


if __name__ == "__main__":
    report = [repair(game) for game in bake.GAMES]
    Path("build_check/exit_source_repair.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
