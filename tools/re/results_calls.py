"""Offline result phase/input evidence; never loads the six GameAssembly DLLs."""
import pathlib
import re
import capstone
import pefile
from collections import defaultdict

ROOT = pathlib.Path(__file__).resolve().parents[2]
STEAM = pathlib.Path('D:/Games/SteamLibrary/steamapps/common')
GAMES = ['FINAL FANTASY PR', 'FINAL FANTASY II PR', 'FINAL FANTASY III PR',
         'FINAL FANTASY IV PR', 'FINAL FANTASY V PR', 'FINAL FANTASY VI PR']
for n, folder in enumerate(GAMES, 1):
    names, targets = defaultdict(list), {}
    source = (ROOT.parent / ('ff%d' % n) / 'dump.cs').read_text(encoding='utf-8-sig')
    for block in source.split('// Namespace: ')[1:]:
        namespace = block.splitlines()[0]
        cls = re.search(r'^(?:public|private|internal).*?\b(?:class|struct) ([\w.<>]+)', block, re.M)
        if not cls:
            continue
        for entry in re.finditer(r'^\s*// RVA: 0x([0-9A-F]+)[^\n]*\n\s*(?:public|private|protected|internal) [^\n]*? ([\w.<>]+)\(', block, re.M):
            rva, method = int(entry[1], 16), entry[2]
            name = '%s.%s.%s' % (namespace, cls[1], method)
            names[rva].append(name)
            if (namespace.endswith('UI.KeyInput') and cls[1].startswith('Result')
                    and ('Init' in method or 'Update' in method or 'SetData' in method or 'Action' in method
                         or 'Show' in method or 'Close' in method or 'Performance' in method
                         or 'EndWait' in method or method.startswith('<'))):
                targets[name] = rva
            if namespace == 'System.Input' and cls[1] in ('InputSystemManager', 'InputListener') and (method.startswith('Get') or method.startswith('AnyKey')):
                targets[name + '.%X' % rva] = rva
    pe = pefile.PE(str(STEAM / folder / 'GameAssembly.dll'), fast_load=True)
    ordered = sorted(names)
    dis = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64)
    output, calls = [], []
    for name, rva in targets.items():
        following = next((x for x in ordered if x > rva), rva + 4096)
        output.append('\n%s RVA %X' % (name, rva))
        for ins in dis.disasm(pe.get_data(rva, min(following - rva, 16384)), pe.OPTIONAL_HEADER.ImageBase + rva):
            annotation = ''
            if ins.mnemonic in ('call', 'jmp') and ins.op_str.startswith('0x'):
                destination = int(ins.op_str, 16) - pe.OPTIONAL_HEADER.ImageBase
                labels = names.get(destination, [])
                relevant = [label for label in labels if label.startswith('System.Input.') or '.UI.KeyInput.Result' in label]
                label_text = ' / '.join(labels) if len(labels) <= 5 else 'folded (%d aliases): %s%s' % (len(labels), ' / '.join(relevant[:6]), ' ...' if len(relevant) > 6 else '')
                annotation = ' ; ' + label_text
                if labels:
                    calls.append('%s\t%X\t%s' % (name, ins.address,
                        label_text))
            output.append('%X %s %s%s' % (ins.address, ins.mnemonic, ins.op_str, annotation))
    (ROOT / 'docs/re' / ('ff%d_results_calls.txt' % n)).write_text('\n'.join(line.rstrip() for line in output) + '\n', encoding='utf-8')
    (ROOT / 'docs/re' / ('ff%d_results_edges.tsv' % n)).write_text('\n'.join(line.rstrip() for line in calls) + '\n', encoding='utf-8')
    (ROOT / 'docs/re' / ('ff%d_results_manifest.tsv' % n)).write_text(
        ''.join('%X\t%s\n' % (pe.OPTIONAL_HEADER.ImageBase + rva, name) for name, rva in targets.items()), encoding='utf-8')
    ns = 'Serial.FF%d.UI.KeyInput' % (0 if n == 4 else n)
    required = [ns + '.ResultPointController.PointsInit', ns + '.ResultStatusUpController.SetData',
                ns + '.ResultCharacterListController.<PlayPerformance>b__6_0',
                ns + '.ResultCharacterListController.ForcedEndPerformance',
                'Last.UI.KeyInput.ResultMenuController.Show', 'Last.UI.KeyInput.ResultMenuController.Close',
                'Last.UI.KeyInput.ResultMenuController.ShowGetItemsInit']
    assert all(name in targets for name in required), 'FF%d result producer hook missing' % n
    if n == 2:
        def native_block(name):
            position = output.index(next(line for line in output if line.startswith('\n' + name + ' RVA')))
            following = next((i for i in range(position + 1, len(output)) if output[i].startswith('\n')), len(output))
            return '\n'.join(output[position:following])
        show = native_block('Last.UI.KeyInput.ResultMenuController.Show')
        combined = native_block('Last.UI.KeyInput.ResultMenuController.ShowLevelUpAbilitysInit')
        assert '[rcx + 0x10], 0' in show and 'mov eax, 6' in show, 'FF2 skill-level first-phase dispatch changed'
        assert '[r10 + 0x1a0]' in combined and '.ResultSkillController.SetLevelUpList' in combined, 'FF2 weapons and spell levels no longer share the native phase'
    def edge(source, destination):
        return any(line.split('\t')[0] == source and destination in line.split('\t')[2] for line in calls)
    assert edge(ns + '.ResultPointController.PointsUpdate', 'System.Input.InputListener.AnyKey'), 'FF%d points input producer changed' % n
    assert edge(ns + '.ResultPointController.StatusUpUpdate', 'System.Input.InputListener.AnyKey'), 'FF%d growth input producer changed' % n
    assert edge('Last.UI.KeyInput.ResultItemController.ShowListUpdate', 'System.Input.InputListener.AnyKey'), 'FF%d item input producer changed' % n
    assert edge('Last.UI.KeyInput.ResultSkillController.ShowListUpdate', 'System.Input.InputListener.GetKeyDown'), 'FF%d learning input producer changed' % n
    status_source = ns + '.ResultPointController.' + ('StatusUpAction' if n == 5 else 'StatusUpUpdate')
    assert edge(status_source, ns + '.ResultStatusUpController.SetData'), 'FF%d member growth producer changed' % n
    assert not any('.ResultPointController.' in line.split('\t')[0] and '.IsEndPerformance' in line.split('\t')[2] for line in calls), 'FF%d completion wrapper is now called; review callback contract' % n
    paged = 'Last.UI.KeyInput.ResultSkillController.UpdateContentPage' in targets
    assert paged == (n not in (1, 3)), 'FF%d learning-page hook mask changed' % n
    print('FF%d: native phase/input/member/completion paths verified; %d entry points' % (n, len(targets)))
