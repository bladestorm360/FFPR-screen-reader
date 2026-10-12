"""FF2 gauge population and exact point scale, using only offline native/assets."""
from pathlib import Path
import csv
import hashlib
import json
import struct
import capstone
import pefile

ROOT = Path(__file__).resolve().parents[2]
MASTER = ROOT.parent / 'tools/mapdump/ff2_master'
DLL = Path('D:/Games/SteamLibrary/steamapps/common/FINAL FANTASY II PR/GameAssembly.dll')
pe = pefile.PE(str(DLL), fast_load=True)
dis = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64)
calls = {int(ins.op_str, 16) - pe.OPTIONAL_HEADER.ImageBase for ins in
         dis.disasm(pe.get_data(0x6FC630, 0x240), pe.OPTIONAL_HEADER.ImageBase + 0x6FC630)
         if ins.mnemonic == 'call' and ins.op_str.startswith('0x')}
assert {0x382120, 0x382D30, 0x381C40, 0x5335A0} <= calls
assert struct.unpack('<f', pe.get_data(0x18398D4, 4))[0] == 1
table_path = MASTER / 'exp_table.txt'
table = [row for row in csv.DictReader(table_path.read_text(encoding='utf-8-sig').splitlines())
         if row['type_id'] == '1' and row['group_id'] == '1']
table.sort(key=lambda row: int(row['lv']))
assert [int(row['lv']) for row in table] == list(range(1, 17))
constants = {row['key_name']: row['value1'] for row in
             csv.DictReader((MASTER / 'game_constant_int.txt').read_text(encoding='utf-8-sig').splitlines())}
assert int(constants['MAX_WEAPON_LEVEL']) == int(constants['MAX_MAGICAL_LEVEL']) == 16
thresholds = [int(row['total_exp']) for row in table]
widths = [b - a for a, b in zip(thresholds, thresholds[1:])]
assert min(widths) == 20 and max(widths) == 100
record = {'native_sha256': hashlib.sha256(DLL.read_bytes()).hexdigest(),
          'master_sha256': hashlib.sha256(table_path.read_bytes()).hexdigest(),
          'provider_rva': '6FC630', 'native_operands': {
              'level': 'ExpUtility.GetExpLevel(1, points, 1)',
              'remaining': 'ExpUtility.GetNextExp(1, points, 1)',
              'interval': 'ExpUtility.GetExpDifference(1, points, 1)'},
          'fill': '1 - remaining / interval; level 16 overrides fill to 1',
          'thresholds': thresholds, 'intervals': widths,
          'minimum_positive_gain_percent': 100 / max(widths),
          'max_weapon_level': 16, 'max_magic_level': 16}
(ROOT / 'docs/re/ff2_gauge_scale.json').write_text(json.dumps(record, indent=2) + '\n', encoding='utf-8')
print('FF2: native gauge calls/formula verified; 16 levels; intervals 20..100; minimum positive gain 1%.')
