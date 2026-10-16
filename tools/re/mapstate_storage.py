"""Bounded, read-only native evidence for the persistent navigation flag snapshot.

Reads the six matching PE files as data, never loads a game. Records constructors,
logical bit indexing and floor-local clearing separately. Existing pefile/capstone
packages only; outputs remain in ignored build_check/chest_state_2026_10_15.
"""
import pathlib,re,hashlib,json,os
import capstone,pefile
root=pathlib.Path(os.path.abspath(__file__)).parents[2]
output_dir=root/'build_check/chest_state_2026_10_15'
output_dir.mkdir(parents=True,exist_ok=True)
report=[]
folders=['ff1','ff2','ff3','FF4','FF5','ff6']
games=['FINAL FANTASY PR','FINAL FANTASY II PR','FINAL FANTASY III PR','FINAL FANTASY IV PR','FINAL FANTASY V PR','FINAL FANTASY VI PR']
for n,(folder,game) in enumerate(zip(folders,games),1):
    dump=(root.parent/folder/'dump.cs').read_text(encoding='utf-8-sig')
    sections=dump.split('// Namespace: ')[1:]
    names={}; targets={}
    for block in sections:
        cls=re.search(r'^(?:public|private|internal).*?\b(?:class|struct) ([\w.<>]+)',block,re.M)
        if not cls:continue
        for m in re.finditer(r'^\s*// RVA: 0x([0-9A-F]+)[^\n]*\n\s*(?:public|private|protected|internal) [^\n]*? ([\w.<>]+)\(([^\n]*)\)',block,re.M):
            rva=int(m[1],16);label=cls[1]+'.'+m[2]+'('+m[3]+')';names.setdefault(rva,[]).append(label)
            if cls[1]=='DataStorage' and m[2] in ['.ctor','GetFlag','get_Instance','ClearMapLocal','ClearAreaLocal']:targets[label]=rva
            if cls[1]=='DataStorage.Flags' and m[2] in ['.ctor','Import','Get','Initialize']:targets[label]=rva
    binary=pathlib.Path('D:/Games/SteamLibrary/steamapps/common')/game/'GameAssembly.dll'
    pe=pefile.PE(str(binary),fast_load=True);ordered=sorted(names);md=capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_64)
    output=['GameAssembly SHA256 '+hashlib.sha256(binary.read_bytes()).hexdigest()]
    decoded={}
    for name,rva in targets.items():
        end=next((x for x in ordered if x>rva),rva+4096)
        output.append('\n'+name+' RVA '+hex(rva))
        instructions=list(md.disasm(pe.get_data(rva,min(end-rva,4096)),pe.OPTIONAL_HEADER.ImageBase+rva))
        decoded[name]=instructions
        for ins in instructions:
            annotation=''
            if ins.mnemonic in ['call','jmp'] and ins.op_str.startswith('0x'):
                dest=int(ins.op_str,16)-pe.OPTIONAL_HEADER.ImageBase;labels=names.get(dest,[])
                annotation=' ; '+(' / '.join(labels) if len(labels)<5 else str(len(labels))+' folded aliases')
            output.append(f'{ins.address:X} {ins.mnemonic} {ins.op_str}{annotation}')
    (output_dir/f'ff{n}_storage_native.txt').write_text('\n'.join(output),encoding='utf-8')
    ctor=decoded['DataStorage..ctor()']
    assert sum(ins.mnemonic=='mov' and ins.op_str=='edx, 0x80' for ins in ctor)==2, f'FF{n}: native allocation changed'
    assert sum(ins.mnemonic=='mov' and ins.op_str.endswith('0x18], 0x400') for ins in ctor)==2, f'FF{n}: logical flag count changed'
    getter=decoded['DataStorage.Flags.Get(int index)']
    assert any(ins.mnemonic=='shr' and ins.op_str=='eax, 5' for ins in getter), f'FF{n}: word index changed'
    assert any(ins.mnemonic=='and' and ins.op_str=='ebx, 0x1f' for ins in getter), f'FF{n}: bit index changed'
    assert any('rax*4 + 0x20' in ins.op_str for ins in getter), f'FF{n}: uint array indexing changed'
    report.append({'game':n,'dll_sha256':hashlib.sha256(binary.read_bytes()).hexdigest(),
                   'dump_sha256':hashlib.sha256((root.parent/folder/'dump.cs').read_bytes()).hexdigest(),
                   'native_functions':targets,'logical_flags':1024,'allocated_uints':128,'used_uints':32})
    print(f'FF{n}: 1024 logical flags / 128 allocated uints / 32 used uints; {len(targets)} bounded functions')
(output_dir/'native_provenance.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
