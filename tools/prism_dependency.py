"""Explicit Prism installer and read-only prerequisite check. Never launches a game."""
from pathlib import Path
from contextlib import contextmanager
import argparse
import csv
import hashlib
import json
import msvcrt
import os
import shutil
import subprocess
import sys
import uuid

ROOT = Path(__file__).resolve().parents[1]
PACKAGE = ROOT / 'third_party/prism'
LOCK = ROOT / 'prism.lock.json'
STEAM = Path(os.environ.get('FFPR_GAME_ROOT', 'D:/Games/SteamLibrary/steamapps/common'))
GAMES = [('FINAL FANTASY PR', 'FINAL FANTASY.exe'),
         ('FINAL FANTASY II PR', 'FINAL FANTASY II.exe'),
         ('FINAL FANTASY III PR', 'FINAL FANTASY III.exe'),
         ('FINAL FANTASY IV PR', 'FINAL FANTASY IV.exe'),
         ('FINAL FANTASY V PR', 'FINAL FANTASY V.exe'),
         ('FINAL FANTASY VI PR', 'FINAL FANTASY VI.exe')]

def child(root, relative):
    root = Path(root).resolve()
    path = (root / relative).resolve()
    if path == root or not path.is_relative_to(root):
        raise ValueError('Prism destination leaves its owned directory')
    return path

def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def matches(directory, lock):
    for name, entry in lock['files'].items():
        path = child(directory, name)
        if not path.is_file() or digest(path) != entry['sha256']:
            return False
    return True

def receipt_path(game):
    return child(game, '.ffpr-deps/prism/prism-install.json')

def read_receipt(game):
    path = receipt_path(game)
    if not path.exists():
        return None
    return json.loads(path.read_text(encoding='utf-8'))

def owned(receipt, game):
    return receipt and receipt.get('mod') == 'FFPR-screen-reader' and \
        receipt.get('game_directory') == str(Path(game).resolve()) and receipt.get('architecture') == 'x64'

def same_package(receipt, lock):
    return receipt and receipt.get('schema') == 1 and receipt.get('package_id') == lock['package_id'] and \
        receipt.get('version') == lock['version'] and receipt.get('source_commit') == lock['source_commit'] and \
        receipt.get('files') == {name: entry['sha256'] for name, entry in lock['files'].items()}

def check(game, lock):
    receipt = read_receipt(game)
    if not owned(receipt, game) or not same_package(receipt, lock) or \
        not matches(child(game, '.ffpr-deps/prism'), lock):
        raise ValueError('Prism prerequisite missing or incompatible. Run install_prism.bat separately.')
    return receipt

@contextmanager
def install_lock(game):
    deps = child(game, '.ffpr-deps')
    deps.mkdir(exist_ok=True)
    path = child(deps, '.prism-install.lock')
    try:
        fd = os.open(path, os.O_WRONLY | os.O_CREAT | os.O_EXCL)
    except FileExistsError:
        pass
    else:
        with os.fdopen(fd, 'wb') as stream:
            stream.write(b'\0')
    with path.open('r+b') as stream:
        msvcrt.locking(stream.fileno(), msvcrt.LK_NBLCK, 1)
        try:
            yield deps
        finally:
            stream.seek(0)
            msvcrt.locking(stream.fileno(), msvcrt.LK_UNLCK, 1)

def write_receipt(game, lock):
    path = receipt_path(game)
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = child(path.parent, '.receipt-' + uuid.uuid4().hex + '.tmp')
    value = {'schema': 1, 'mod': 'FFPR-screen-reader', 'game_directory': str(Path(game).resolve()),
             'architecture': lock['architecture'], 'package_id': lock['package_id'],
             'version': lock['version'], 'source_commit': lock['source_commit'],
             'files': {name: entry['sha256'] for name, entry in lock['files'].items()}}
    temporary.write_text(json.dumps(value, indent=2) + '\n', encoding='utf-8')
    os.replace(temporary, path)

def install(game, lock, package=PACKAGE):
    game = Path(game).resolve()
    if not game.is_dir():
        raise ValueError('Game directory is missing')
    for entry in lock['files'].values():
        if digest(child(package, entry['source'])) != entry['sha256']:
            raise ValueError('Prism source package checksum failed; installation untouched')
    with install_lock(game) as deps:
        current = read_receipt(game)
        if current and not owned(current, game):
            raise ValueError('Prism receipt belongs to another installation; installation untouched')
        destination = child(deps, 'prism')
        if matches(destination, lock):
            if same_package(current, lock):
                return 'already verified; zero writes'
            write_receipt(game, lock)
            return 'matching files adopted; receipt only, zero DLL copies'
        if destination.exists() and any(p.is_file() for p in destination.rglob('*')) and current is None:
            raise ValueError('Unrecognized Prism files; installation untouched')
        token = uuid.uuid4().hex
        stage = child(deps, 'prism-stage-' + token)
        backup = child(deps, 'prism-backups/' + token)
        stage.mkdir(parents=True)
        written = []
        try:
            for name, entry in lock['files'].items():
                path = child(stage, name)
                path.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(child(package, entry['source']), path)
            if not matches(stage, lock):
                raise ValueError('Prism stage checksum failed')
            if current:
                backup.mkdir(parents=True)
                shutil.copy2(receipt_path(game), child(backup, 'prism-install.json'))
                for name in current['files']:
                    old = child(destination, name)
                    if old.exists():
                        copy = child(backup, name)
                        copy.parent.mkdir(parents=True, exist_ok=True)
                        shutil.copy2(old, copy)
            destination.mkdir(exist_ok=True)
            for name in lock['files']:
                target = child(destination, name)
                existed = target.exists()
                target.parent.mkdir(parents=True, exist_ok=True)
                # Existing owned files absent from an old manifest also need rollback.
                if existed and not child(backup, name).exists():
                    saved = child(backup, name)
                    saved.parent.mkdir(parents=True, exist_ok=True)
                    shutil.copy2(target, saved)
                os.replace(child(stage, name), target)
                written.append((name, existed))
            write_receipt(game, lock)
            check(game, lock)
        except Exception:
            for name, existed in reversed(written):
                target = child(destination, name)
                if existed:
                    shutil.copy2(child(backup, name), target)
                elif target.exists():
                    target.unlink()
            if current:
                shutil.copy2(child(backup, 'prism-install.json'), receipt_path(game))
            elif receipt_path(game).exists():
                receipt_path(game).unlink()
            raise
        finally:
            # This exact stage was created above; verify its canonical containment
            # before deleting temporary package copies. Never delete installed files.
            if stage.exists() and stage.parent == deps and stage.name == 'prism-stage-' + token:
                shutil.rmtree(stage)
        return 'installed verified package'

def running(exe):
    result = subprocess.run(['tasklist', '/FI', 'IMAGENAME eq ' + exe, '/FO', 'CSV', '/NH'],
                            capture_output=True, text=True, check=True)
    return any(row and row[0].casefold() == exe.casefold() for row in csv.reader(result.stdout.splitlines()))

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('mode', choices=['check', 'install'])
    parser.add_argument('game', nargs='?', choices=['ff%d' % n for n in range(1, 7)])
    parser.add_argument('--game-root', type=Path, default=STEAM, help='Steam steamapps/common directory')
    args = parser.parse_args()
    lock = json.loads(LOCK.read_text(encoding='utf-8'))
    failed = False
    for n, (folder, exe) in enumerate(GAMES, 1):
        label = 'ff%d' % n
        if args.game and args.game != label:
            continue
        game = args.game_root / folder
        if not (game / exe).exists():
            print(label + ': not installed')
            failed = failed or bool(args.game)
            continue
        try:
            if args.mode == 'install':
                if running(exe):
                    raise ValueError(exe + ' is running. Close the game first.')
                message = install(game, lock)
            else:
                check(game, lock)
                message = 'Prism prerequisite verified (read-only)'
            print(label + ': ' + message)
        except Exception as error:
            print(label + ': ' + str(error))
            failed = True
    return 1 if failed else 0

if __name__ == '__main__':
    sys.exit(main())
