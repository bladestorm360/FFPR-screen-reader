"""Pinned private OpenAL Soft payload. No download, game launch or global install."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil

ROOT = Path(__file__).resolve().parents[1]

def payload(root=ROOT):
    lock = json.loads((root / 'openal.lock.json').read_text(encoding='utf-8'))
    base = (root / 'third_party/openal-soft').resolve()
    result = []
    for name, digest in lock['files'].items():
        source = (base / name).resolve()
        if source.parent != base or hashlib.sha256(source.read_bytes()).hexdigest() != digest:
            raise ValueError('OpenAL Soft pinned file mismatch: ' + name)
        result.append((source, '.ffpr-deps/openal-soft/' + name))
    return result

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('command', choices=['check', 'deploy'])
    parser.add_argument('--game', type=Path)
    args = parser.parse_args()
    files = payload()
    if args.command == 'deploy':
        if not args.game or not args.game.is_dir():
            raise ValueError('existing game root required')
        game = args.game.resolve()
        for source, name in files:
            target = (game / name).resolve()
            if not target.is_relative_to(game):
                raise ValueError('dependency path escaped game root')
            if target.is_file() and target.read_bytes() == source.read_bytes():
                continue
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(source, target)
    print(f'OpenAL Soft pinned payload verified: {len(files)} files')
    return 0

if __name__ == '__main__':
    try:
        raise SystemExit(main())
    except (OSError, ValueError) as error:
        print('FAIL: ' + str(error))
        raise SystemExit(1)
