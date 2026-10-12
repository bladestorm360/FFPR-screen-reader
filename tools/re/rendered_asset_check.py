"""Read-only all-six glyph/prefab evidence. Never loads or controls a game."""
from pathlib import Path
import json
import UnityPy

ROOT = Path(__file__).resolve().parents[2]
STEAM = Path('D:/Games/SteamLibrary/steamapps/common')
GAMES = ['FINAL FANTASY PR', 'FINAL FANTASY II PR', 'FINAL FANTASY III PR',
         'FINAL FANTASY IV PR', 'FINAL FANTASY V PR', 'FINAL FANTASY VI PR']
records = []
expected = {'Battle_Common_DamageNumber_%d' % d for d in range(10)}
for n, game in enumerate(GAMES, 1):
    folder = STEAM / game
    data = next(p for p in folder.iterdir() if p.name.endswith('_Data') and p.is_dir())
    bundles = data / 'StreamingAssets/aa/StandaloneWindows64'
    views = list(bundles.glob('view_damage_assets_all_*.bundle'))
    hits = list(bundles.glob('hit_count_assets_all_*.bundle'))
    assert len(views) == len(hits) == 1
    names, damage_palettes, hit_palettes = set(), [], []
    for path in views + hits:
        env = UnityPy.load(str(path))
        for obj in env.objects:
            if obj.type.name == 'Sprite':
                names.add(obj.read().m_Name)
            elif obj.type.name == 'MonoBehaviour':
                try:
                    tree = obj.read_typetree()
                except Exception:
                    continue
                if 'sprites' in tree and 'prefab' in tree:
                    damage_palettes.append(len(tree['sprites']))
                if 'sprites' in tree and 'numRenderers' in tree:
                    hit_palettes.append({'glyphs': len(tree['sprites']), 'slots': len(tree['numRenderers'])})
    assert expected <= names, 'FF%d digit naming changed' % n
    assert damage_palettes and all(count == 10 for count in damage_palettes)
    assert hit_palettes and all(row['glyphs'] == 10 for row in hit_palettes)
    records.append({'game': n, 'bundles': [p.name for p in views + hits],
                    'glyphs': sorted(expected), 'damage_palettes': damage_palettes,
                    'hit_palettes': hit_palettes})
    print('FF%d: ten exact digit names and damage/hit palettes verified offline' % n)
(ROOT / 'docs/re/rendered_digit_assets.json').write_text(
    json.dumps(records, indent=2) + '\n', encoding='utf-8')
