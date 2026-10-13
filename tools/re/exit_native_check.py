"""Offline provenance for the recovered collision tables and baked map inputs."""
import hashlib
import json
import struct
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT / "tools"))
import bake_maps as bake

FOLDERS = ["FINAL FANTASY PR", "FINAL FANTASY II PR", "FINAL FANTASY III PR",
           "FINAL FANTASY IV PR", "FINAL FANTASY V PR", "FINAL FANTASY VI PR"]
EXPECTED = {hashlib.sha1(struct.pack("<14i",*values)).hexdigest().upper(): values
            for values in (bake.COLLISION_FILTER,bake.DIRECT_COLLISION)}


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


report = []
for game,folder in zip(bake.GAMES,FOLDERS):
    install = Path("D:/Games/SteamLibrary/steamapps/common") / folder
    metadata_path = next(install.glob("**/global-metadata.dat"))
    metadata = metadata_path.read_bytes()
    assert struct.unpack_from("<I",metadata)[0] == 0xFAB11BAF
    fields,count = struct.unpack_from("<II",metadata,0x40)
    data,size = struct.unpack_from("<II",metadata,0x48)
    found = {}
    for offset in range(fields,fields+count,12):
        field,type_id,index = struct.unpack_from("<iii",metadata,offset)
        if index < 0 or index+56 > size:
            continue
        values = metadata[data+index:data+index+56]
        digest = hashlib.sha1(values).hexdigest().upper()
        if digest in EXPECTED:
            assert tuple(struct.unpack("<14i",values)) == EXPECTED[digest]
            found[digest] = {"field": field,"data_index": index}
    assert set(found) == set(EXPECTED), game
    record = {"game": game,"dll_sha256": sha(install/"GameAssembly.dll"),
              "metadata_sha256": sha(metadata_path),"tables": found,"source_sha256": {}}
    for suffix in ("grids.json","submaps.json","objects.jsonl","scripts.jsonl"):
        path = Path(bake.MAPDUMP) / f"{game}_{suffix}"
        record["source_sha256"][path.name] = sha(path)
    record["bake_sha256"] = sha(ROOT / "data" / game / "maps.bin")
    report.append(record)
    print(game,"matching native collision tables and source/bake hashes recorded")
(ROOT / "build_check/exit_native_provenance.json").write_text(json.dumps(report,indent=2),encoding="utf-8")
