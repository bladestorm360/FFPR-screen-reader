"""Bounded offline controller-selection/input audit for all six FFPR games.

No process access, launch, input or saved-game reads. Preserve matching binary/dump
hashes, native signatures/inheritance/aliases and actual mode master rows.
"""
import csv
import hashlib
import json
import os
from pathlib import Path
import re
import capstone
import pefile

ROOT = Path(os.path.abspath(__file__)).parents[2]
STEAM = Path('D:/Games/SteamLibrary/steamapps/common')
GAMES = ['FINAL FANTASY PR','FINAL FANTASY II PR','FINAL FANTASY III PR',
         'FINAL FANTASY IV PR','FINAL FANTASY V PR','FINAL FANTASY VI PR']
METHODS = {'Init','SwtichInputEnable','SwitchInputEnable','OnTouchPadCallback',
           'LateUpdateObserveInput','MoveToAxis','OnKey','OnKeyUp','UpdateRotateAccelerator'}


def main():
    out = ROOT/'build_check/autowalk_controls_2026_10_16/native'
    out.mkdir(parents=True, exist_ok=True)
    manifest = []
    for n, game in enumerate(GAMES,1):
        dump_path = ROOT.parent/f'ff{n}/dump.cs'
        dump = dump_path.read_text(encoding='utf-8-sig')
        binary = STEAM/game/'GameAssembly.dll'
        methods, classes, aliases, targets = {}, {}, {}, []
        for block in dump.split('// Namespace: ')[1:]:
            namespace = block.splitlines()[0].strip()
            match = re.search(r'^.*?\bclass ([\w.<>]+)([^\n]*)', block, re.M)
            if not match:
                continue
            cls = match[1]
            relevant = cls.startswith('FieldPlayer') and cls.endswith('Controller') and namespace=='Last.Map'
            if relevant:
                classes[cls] = {'declaration': match[0].strip(), 'fields': [l.strip() for l in block.split('// Methods')[0].splitlines() if '; // 0x' in l]}
            for entry in re.finditer(r'// RVA: 0x([A-F0-9]+)[^\n]*\n\s*((?:public|private|internal|protected) [^\n]*? ([\w.<>]+)\(([^\n]*)\))', block):
                rva = int(entry[1],16)
                name = f'{namespace}.{cls}.{entry[3]}'
                aliases.setdefault(rva,[]).append(name)
                methods[rva] = name
                if (relevant and entry[3] in METHODS) or (cls=='FieldController' and entry[3] in {'SwitchPlayerController','GetZAxisRotateBirdCamera'}) or (cls=='FieldMap' and entry[3]=='OnSwitchPlayerController'):
                    targets.append({'name':name,'rva':rva,'signature':entry[2]})
        pe = pefile.PE(str(binary),fast_load=True)
        image = pe.get_memory_mapped_image()
        boundaries = sorted(methods)
        decoder = capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_64)
        lines = []
        for target in targets:
            rva=target['rva'];end=next((v for v in boundaries if v>rva),rva+32768)
            assert end-rva<=32768,(n,target,end-rva)
            body=image[rva:end]
            target['alias_count']=len(aliases[rva])
            target['body_sha256']=hashlib.sha256(body).hexdigest()
            target['bytes']=len(body)
            lines.append('\n'+target['name']+' '+hex(rva)+' '+target['signature']+'\n')
            for instruction in decoder.disasm(body,rva):
                note=''
                if instruction.mnemonic in {'call','jmp'} and instruction.op_str.startswith('0x'):
                    labels=aliases.get(int(instruction.op_str,16),[])
                    note=' ; '+(' | '.join(labels) if len(labels)<=3 else f'{len(labels)} folded methods')
                lines.append(f'{instruction.address:X} {instruction.mnemonic} {instruction.op_str}{note}\n')
        transport_path=ROOT.parent/f'tools/mapdump/ff{n}_master/transportation.txt'
        with transport_path.open(encoding='utf-8-sig',newline='') as source:
            transports=list(csv.DictReader(source))
        manifest.append({'game':n,'game_sha256':hashlib.sha256(binary.read_bytes()).hexdigest(),
                         'dump_sha256':hashlib.sha256(dump_path.read_bytes()).hexdigest(),
                         'controller_classes':classes,'native_methods':targets,'transports':transports})
        (out/f'ff{n}_controllers.txt').write_text(''.join(lines),encoding='utf-8')
        print(f'FF{n}: {len(classes)} controller types, {len(targets)} native bodies, {len(transports)} transportation rows')
    (out/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf-8')


if __name__=='__main__':
    main()
