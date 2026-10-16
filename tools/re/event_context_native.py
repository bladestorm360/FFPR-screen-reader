"""Bounded offline PE evidence for objective creation, FFII target focus and shop prices.
Reads matching installed binaries as files; never loads, attaches to, or starts a game.
Uses existing pefile/capstone. Outputs are ignored, with native/dump SHA256 provenance.
"""
import os
from pathlib import Path
import re,hashlib,capstone,pefile,json
root=Path(os.path.abspath(__file__)).parents[2];out=root/'build_check/event_audit_2026_10_16';out.mkdir(parents=True,exist_ok=True)
games=['FINAL FANTASY PR','FINAL FANTASY II PR','FINAL FANTASY III PR','FINAL FANTASY IV PR','FINAL FANTASY V PR','FINAL FANTASY VI PR'];folders=['ff1','ff2','ff3','FF4','FF5','ff6'];report=[]
# Reviewed ObjectType20 switch destinations in MapManager.CreateEntity.
null_branches=[0x2c4ab3,0x2b9e33,0x2ad3df,0x2a7db4,0x2a27b0,0x330a8e]
for n,(game,folder) in enumerate(zip(games,folders),1):
 dump=(root.parent/folder/'dump.cs').read_text(encoding='utf-8-sig');names={};targets={}
 for block in dump.split('// Namespace: ')[1:]:
  ns=block.splitlines()[0];cls=re.search(r'^(?:public|private|internal).*?\b(?:class|struct) ([\w.<>]+)',block,re.M)
  if not cls:continue
  for m in re.finditer(r'^\s*// RVA: 0x([0-9A-F]+)[^\n]*\n\s*(?:public|private|protected|internal) [^\n]*? ([\w.<>]+)\(([^\n]*)\)',block,re.M):
   rva=int(m[1],16);label=ns+'.'+cls[1]+'.'+m[2]+'('+m[3]+')';names.setdefault(rva,[]).append(label)
   if m[2] in ['CreateEntity','CreateTelepoPointList','FindRelatedEntitiesInTelepoPoint','OnCacheTelepoPoint'] and ns=='Last.Map':targets[label]=rva
   if n==2 and cls[1]=='AbilityUseContentListController' and ns=='Serial.FF2.UI.KeyInput' and m[2] in ['SetCursor','SetFocus','UpdateView']:targets[label]=rva
   if ns=='Last.UI.KeyInput' and ((cls[1]=='ShopListMainContentController' and m[2] in ['SetCursor','SelectContent','CreateItemList']) or (cls[1]=='ShopInfoController' and m[2]=='SetDescription') or (cls[1]=='ShopListItemContentView' and m[2] in ['UpdateView','SetPrice'])):targets[label]=rva
 binary=Path('D:/Games/SteamLibrary/steamapps/common')/game/'GameAssembly.dll';pe=pefile.PE(str(binary),fast_load=True);ordered=sorted(names);md=capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_64);output=[]
 for name,rva in targets.items():
  end=next((x for x in ordered if x>rva),rva+16384);output.append('\n'+name+' RVA '+hex(rva))
  for ins in md.disasm(pe.get_data(rva,min(end-rva,32768)),pe.OPTIONAL_HEADER.ImageBase+rva):
   annotation=''
   if ins.mnemonic in ['call','jmp'] and ins.op_str.startswith('0x'):
    ls=names.get(int(ins.op_str,16)-pe.OPTIONAL_HEADER.ImageBase,[]);annotation=' ; '+(' / '.join(ls) if len(ls)<5 else str(len(ls))+' folded aliases')
   output.append(f'{ins.address:X} {ins.mnemonic} {ins.op_str}{annotation}')
 branch=null_branches[n-1];epilogue=list(md.disasm(pe.get_data(branch,8),pe.OPTIONAL_HEADER.ImageBase+branch))
 assert epilogue[0].mnemonic=='xor' and epilogue[0].op_str=='eax, eax' and epilogue[1].mnemonic=='jmp',f'FF{n}: reviewed null-return branch changed'
 output.append('\nReviewed ObjectType20 factory branch '+hex(branch)+': '+ '; '.join(i.mnemonic+' '+i.op_str for i in epilogue[:2]))
 (out/f'ff{n}_event_native.txt').write_text('\n'.join(output),encoding='utf-8');report.append({'game':n,'dll_sha256':hashlib.sha256(binary.read_bytes()).hexdigest(),'dump_sha256':hashlib.sha256((root.parent/folder/'dump.cs').read_bytes()).hexdigest(),'functions':targets,'telepo_factory_branch_rva':branch,'telepo_factory_epilogue':[i.mnemonic+' '+i.op_str for i in epilogue[:2]]});print('FF'+str(n),len(targets),'bounded native functions')
(out/'native_provenance.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
