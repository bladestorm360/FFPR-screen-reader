"""Scratch-only installer/deployer checks. No Prism backend or game is loaded."""
from pathlib import Path
import hashlib
import json
import importlib.util
import contextlib
import io
import os
import shutil
import subprocess
import tempfile
import sys
import zipfile
import prism_dependency as dependency

ROOT = dependency.ROOT
scratch_parent = (ROOT / 'build_check').resolve()
scratch_parent.mkdir(exist_ok=True)
failures = 0

def expect(okay, name):
    global failures
    if not okay:
        failures += 1
        print('FAIL ' + name)

def snapshot(directory):
    return {p.relative_to(directory).as_posix(): (dependency.digest(p), p.stat().st_mtime_ns)
            for p in directory.rglob('*') if p.is_file()}

def rejects(fn):
    try:
        fn()
    except (ValueError, OSError):
        return True
    return False

with tempfile.TemporaryDirectory(prefix='prism-check-', dir=scratch_parent) as temporary:
    scratch = Path(temporary).resolve()
    assert scratch.is_relative_to(scratch_parent)
    package = scratch / 'package'
    package.mkdir()
    (package / 'prism.dll').write_bytes(b'fake version one; never loaded')
    (package / 'notice.txt').write_text('fake package notice', encoding='utf-8')
    lock = {'schema': 1, 'version': '0.18.3', 'architecture': 'x64', 'package_id': 'fake-one',
            'source_commit': 'fake-source-one', 'files': {
                'prism.dll': {'source': 'prism.dll', 'sha256': dependency.digest(package / 'prism.dll')},
                'LICENSES/notice.txt': {'source': 'notice.txt', 'sha256': dependency.digest(package / 'notice.txt')}}}
    game = scratch / 'game'
    game.mkdir()
    expect(rejects(lambda: dependency.check(game, lock)), 'missing prerequisite rejected')
    expect(not (game / '.ffpr-deps').exists(), 'read-only missing check creates no directories')
    dependency.install(game, lock, package)
    dependency.check(game, lock)
    before = snapshot(game / '.ffpr-deps')
    expect('zero writes' in dependency.install(game, lock, package), 'unchanged installer reports zero writes')
    expect(snapshot(game / '.ffpr-deps') == before, 'second unchanged installation preserves every hash and timestamp')
    receipt = dependency.receipt_path(game)
    dll = receipt.parent / 'prism.dll'
    stamp = dll.stat().st_mtime_ns
    receipt.unlink()
    expect('adopted' in dependency.install(game, lock, package), 'matching files adopt a missing receipt')
    expect(dll.stat().st_mtime_ns == stamp, 'adoption does not copy the DLL')
    dll.write_bytes(b'altered')
    before = snapshot(receipt.parent)
    expect(rejects(lambda: dependency.check(game, lock)), 'altered prerequisite rejected')
    expect(snapshot(receipt.parent) == before, 'incompatible check performs no repair or writes')
    dependency.install(game, lock, package)
    dependency.check(game, lock)
    good_receipt = receipt.read_bytes()
    foreign = json.loads(good_receipt); foreign['mod'] = 'another mod'
    receipt.write_text(json.dumps(foreign), encoding='utf-8')
    before = snapshot(receipt.parent)
    expect(rejects(lambda: dependency.install(game, lock, package)), 'foreign receipt cannot be replaced')
    expect(snapshot(receipt.parent) == before, 'foreign package files and receipt untouched')
    receipt.write_bytes(good_receipt)
    previous = snapshot(receipt.parent)
    new_package = scratch / 'new-package'
    shutil.copytree(package, new_package)
    (new_package / 'prism.dll').write_bytes(b'fake version two; never loaded')
    (new_package / 'notice.txt').write_text('new fake package notice', encoding='utf-8')
    update = json.loads(json.dumps(lock)); update['package_id'] = 'fake-two'; update['source_commit'] = 'fake-source-two'
    for entry in update['files'].values():
        entry['sha256'] = dependency.digest(new_package / entry['source'])
    replace = dependency.os.replace
    fail_once = [True]
    def interrupted(source, destination):
        if fail_once[0] and Path(destination) == receipt.parent / 'LICENSES/notice.txt':
            fail_once[0] = False
            raise OSError('simulated commit interruption')
        return replace(source, destination)
    dependency.os.replace = interrupted
    try:
        expect(rejects(lambda: dependency.install(game, update, new_package)), 'partial package commit failure is reported')
    finally:
        dependency.os.replace = replace
    expect(snapshot(receipt.parent) == previous, 'partial failure restores prior files, timestamps and receipt together')
    dependency.check(game, lock)
    (new_package / 'prism.dll').write_bytes(b'bad source')
    expect(rejects(lambda: dependency.install(game, update, new_package)), 'source checksum failure leaves installation alone')
    expect(snapshot(receipt.parent) == previous, 'bad source cannot modify installed package')
    expect(rejects(lambda: dependency.child(game, '../escape')), 'path traversal is refused')

    # Exercise the actual normal deploy script twice, adapting only its fixed
    # machine root for this scratch fixture. The dependency checker remains real.
    mod = scratch / 'mod'
    (mod / 'tools').mkdir(parents=True)
    (mod / 'out').mkdir()
    (mod / 'third_party/runtime').mkdir(parents=True)
    (mod / 'out/hid.dll').write_bytes(b'fake mod one; never loaded')
    (mod / 'third_party/runtime/SDL3.dll').write_bytes(b'fake SDL; never loaded')
    (mod / 'prism.lock.json').write_text(json.dumps(lock), encoding='utf-8')
    shutil.copy2(ROOT / 'tools/prism_dependency.py', mod / 'tools/prism_dependency.py')
    shutil.copy2(ROOT / 'tools/openal_dependency.py', mod / 'tools/openal_dependency.py')
    openal = mod / 'third_party/openal-soft'
    openal.mkdir()
    openal_files = {}
    for name in json.loads((ROOT / 'openal.lock.json').read_text(encoding='utf-8'))['files']:
        (openal / name).write_bytes(('fake OpenAL payload, never loaded: ' + name).encode())
        openal_files[name] = dependency.digest(openal / name)
    (mod / 'openal.lock.json').write_text(json.dumps({'version': 'fake', 'files': openal_files}), encoding='utf-8')
    steam = scratch / 'games'
    target = steam / dependency.GAMES[0][0]
    target.mkdir(parents=True)
    (target / dependency.GAMES[0][1]).write_bytes(b'fake executable; never launched')
    dependency.install(target, lock, package)
    before = snapshot(target / '.ffpr-deps/prism')
    script = (ROOT / 'deploy.bat').read_text(encoding='utf-8').replace(
        'set "STEAM=D:\\Games\\SteamLibrary\\steamapps\\common"', 'set "STEAM=' + str(steam) + '"')
    (mod / 'deploy.bat').write_text(script, encoding='utf-8')
    env = dict(os.environ, FFPR_GAME_ROOT=str(steam))
    previous_openal = None
    for version in [b'fake mod two', b'fake mod three']:
        (mod / 'out/hid.dll').write_bytes(version)
        result = subprocess.run(['cmd.exe', '/d', '/c', 'deploy.bat', 'ff1'], cwd=mod,
                                env=env, capture_output=True, text=True)
        expect(result.returncode == 0 and (target / 'hid.dll').read_bytes() == version, 'actual normal deploy copies mod and private OpenAL payload')
        expect(snapshot(target / '.ffpr-deps/prism') == before, 'actual normal deploy preserves every Prism hash and timestamp')
        actual_openal = snapshot(target / '.ffpr-deps/openal-soft')
        expect({name: value[0] for name,value in actual_openal.items()} == openal_files, 'actual deploy includes every pinned OpenAL runtime/notice')
        if previous_openal is not None:
            expect(actual_openal == previous_openal, 'unchanged OpenAL deployment performs zero writes')
        previous_openal = actual_openal
    (target / '.ffpr-deps/prism/prism.dll').unlink()
    old_mod = (target / 'hid.dll').read_bytes()
    result = subprocess.run(['cmd.exe', '/d', '/c', 'deploy.bat', 'ff1'], cwd=mod,
                            env=env, capture_output=True, text=True)
    expect(result.returncode != 0 and (target / 'hid.dll').read_bytes() == old_mod,
           'normal deploy fails for a missing prerequisite without copying the mod or reinstalling Prism')

    # Actual release assembly, using fake mod files in the same scratch layout.
    (mod / 'third_party/minhook').mkdir()
    (mod / 'third_party/minhook/LICENSE.txt').write_text('fake license', encoding='utf-8')
    (mod / 'readme.md').write_text('fake readme', encoding='utf-8')
    for name in ['Tolk.dll', 'prism.dll', 'nvdaControllerClient64.dll', 'SAAPI64.dll']:
        (mod / 'third_party/runtime' / name).write_bytes(b'fake old/prerequisite file')
    spec = importlib.util.spec_from_file_location('scratch_release', ROOT / 'tools/make_release.py')
    release = importlib.util.module_from_spec(spec); spec.loader.exec_module(release)
    release.ROOT = str(mod); release.RUNTIME = str(mod / 'third_party/runtime')
    argv = sys.argv
    try:
        sys.argv = ['make_release.py', 'offline-check']
        with contextlib.redirect_stdout(io.StringIO()):
            expect(release.main() == 0, 'actual mod-only archive assembly succeeds')
    finally:
        sys.argv = argv
    with zipfile.ZipFile(mod / 'releases/FFPR_ScreenReader_offline-check.zip') as archive:
        expected = {'hid.dll', 'SDL3.dll', 'MinHook_LICENSE.txt', 'readme.md'} | {'.ffpr-deps/openal-soft/' + name for name in openal_files}
        expect(set(archive.namelist()) == expected,
               'actual archive includes complete OpenAL, excludes Prism/Tolk/helpers/receipts')

print('Prism dependency: idempotence/adoption/conflict/rollback, actual deploy and archive separation: %d failures' % failures)
raise SystemExit(1 if failures else 0)
