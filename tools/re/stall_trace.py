"""Bounded FFII stall investigation. Default: offline plan only.

--pid attaches only after explicit task-specific user permission. Never starts or
controls a game, changes game state, reads saves/dialogue, or changes settings.
Existing Frida only. Rare native boundaries plus runtime-invoke/GC are timed;
bounded numeric timing records are delivered after detaching all observers.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import time

ROOT = Path(os.path.abspath(__file__)).parents[2]
GAME = Path(r'D:\Games\SteamLibrary\steamapps\common\FINAL FANTASY II PR')
TARGETS = {
    ('SubSceneManagerMainGame', 'ChangeState'),
    ('TitleWindowController', 'InitSelect'),
    ('TitleWindowController', 'UpdateNone'),
    ('BattleActExection', 'InitActionState'),
    ('BattleActExection', 'CreateDamageView'),
    ('BattleActExection', 'EndFinishingState'),
    ('BattleCommandMessageController', 'SetMessage'),
    ('MessageManager', 'GetMessage'),
    ('OwnedCharacterData', 'get_Name'),
    ('BattleEnemyData', 'GetMesIdName'),
}


def plan():
    dump_path = ROOT.parent / 'ff2/dump.cs'
    dump = dump_path.read_text(encoding='utf-8-sig')
    targets = []
    for block in dump.split('// Namespace: ')[1:]:
        cls = re.search(r'^(?:public|private|internal).*?\b(?:class|struct) ([\w.<>]+)', block, re.M)
        if not cls:
            continue
        for method in re.finditer(r'^\s*// RVA: 0x([0-9A-F]+)[^\n]*\n\s*(?:public|private|protected|internal) [^\n]*? ([\w.<>]+)\(([^\n]*)\)', block, re.M):
            if (cls[1], method[2]) in TARGETS:
                targets.append({'name': cls[1]+'.'+method[2], 'rva': int(method[1], 16)})
    # Deduplicate folded addresses; do not install multiple observers at one body.
    targets = list({t['rva']: t for t in targets}.values())
    assert len(targets) <= 20
    return {'targets': targets,
            'dump_sha256': hashlib.sha256(dump_path.read_bytes()).hexdigest(),
            'game_sha256': hashlib.sha256((GAME/'GameAssembly.dll').read_bytes()).hexdigest(),
            'mod_sha256': hashlib.sha256((GAME/'hid.dll').read_bytes()).hexdigest()}


JS = r'''
const plan = PLAN;
const ga = Process.getModuleByName('GameAssembly.dll');
const records = [];
const totals = {};
let dropped = 0;
const frequency = Memory.alloc(8);
const counter = Memory.alloc(8);
const kernel = Process.getModuleByName('kernel32.dll');
const qpf = new NativeFunction(kernel.getExportByName('QueryPerformanceFrequency'), 'int', ['pointer']);
const qpc = new NativeFunction(kernel.getExportByName('QueryPerformanceCounter'), 'int', ['pointer']);
qpf(frequency);
const hz = frequency.readS64().toNumber();
function now() {qpc(counter);return counter.readS64().toNumber()/hz;}
function observe(address, name, stateArgument) {
    Interceptor.attach(address, {
        onEnter(args) {this.started=now();this.tid=Process.getCurrentThreadId();this.state=stateArgument?args[1].toInt32():null;},
        onLeave() {
            const ms=(now()-this.started)*1000;
            const total=totals[name]||(totals[name]={count:0,ms:0,maximum:0});
            total.count++;total.ms+=ms;total.maximum=Math.max(total.maximum,ms);
            if(ms<2)return;
            if(records.length<512)records.push({name:name,thread:this.tid,started:this.started,ms:ms,state:this.state});else dropped++;
        }
    });
}
for(const target of plan.targets)observe(ga.base.add(target.rva),target.name,target.name.endsWith('.ChangeState'));
for(const name of ['il2cpp_runtime_invoke','il2cpp_gc_collect']) {
    const address=ga.findExportByName(name);if(address)observe(address,name,false);
}
rpc.exports.finish=function(){Interceptor.detachAll();return {records:records,totals:totals,dropped:dropped,ended:now()};};
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--pid', type=int)
    parser.add_argument('--wait', action='store_true', help='Wait at most90 seconds for the authorized FFII process; never launch it')
    parser.add_argument('--seconds', type=int, default=30)
    parser.add_argument('--output', type=Path, default=ROOT/'build_check/stall_2026_10_16/runtime.json')
    args = parser.parse_args()
    manifest = plan()
    if args.pid is None and not args.wait:
        print(json.dumps(manifest, indent=2))
        return
    assert 1 <= args.seconds <= 45
    import frida
    device = frida.get_local_device()
    if args.wait:
        deadline=time.monotonic()+90
        while args.pid is None and time.monotonic()<deadline:
            try: args.pid=device.get_process('FINAL FANTASY II.exe').pid
            except frida.ProcessNotFoundError: time.sleep(.5)
        assert args.pid is not None, 'FFII did not open within the bounded wait'
        # Allow the mod's existing hook installation to finish before attaching
        # observers. Match this process, never a stale previous-session log.
        deadline=time.monotonic()+20
        while time.monotonic()<deadline:
            log=(GAME/'ffpr_screen_reader.log').read_text(encoding='utf-8-sig',errors='replace')
            if 'pid='+str(args.pid)+' ===' in log and 'boot: startup complete' in log:break
            time.sleep(.25)
        else:raise RuntimeError('Current FFII boot readiness was not observed; no attachment attempted')
    process = next((p for p in device.enumerate_processes() if p.pid == args.pid), None)
    assert process and process.name.lower() == 'final fantasy ii.exe', 'Only explicitly selected FFII is supported'
    manifest.update(pid=args.pid, seconds=args.seconds)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    session=device.attach(args.pid)
    try:
        script = session.create_script(JS.replace('PLAN', json.dumps(manifest)))
        script.load()
        print('FFII timing observers active for '+str(args.seconds)+' seconds.', flush=True)
        time.sleep(args.seconds)
        manifest['trace'] = script.exports_sync.finish()
        script.unload()
    finally:
        session.detach()
    args.output.write_text(json.dumps(manifest, indent=2)+'\n', encoding='utf-8')
    print('Detached; bounded timings saved to '+str(args.output), flush=True)


if __name__ == '__main__':
    main()
