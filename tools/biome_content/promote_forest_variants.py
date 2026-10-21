"""Install staged Forest-family variants into the runtime content tree.

Copy-only and never destructive: refuses to overwrite a runtime file whose
content differs from the staged file (re-running with unchanged output is a
no-op), and checks every authored source room is still at its baseline hash.

    python tools/biome_content/promote_forest_variants.py [biome-slug ...]
"""
import hashlib
import json
import shutil
import sys
from pathlib import Path

import forest_variants

ROOT = forest_variants.ROOT
STAGED = forest_variants.OUT


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main(slugs):
    baseline = json.loads((ROOT / 'docs/biome-content-audit/baseline.json').read_text(encoding='utf-8'))
    for source in baseline['sources']:
        if digest(ROOT / source['path']) != source['sha256']:
            raise RuntimeError('Authored source changed: ' + source['path'])
    copies = []
    for slug in slugs:
        palette = forest_variants.PALETTES[slug]
        stage = STAGED / slug
        manifest = json.loads((stage / 'manifest.json').read_text())
        if len(manifest) != 23 or any(row['missing'] for row in manifest.values()):
            raise RuntimeError(f'{slug}: mapping is incomplete')
        rooms = ROOT / 'TestGame/Rooms/Generated' / palette['folder'] / 'forest_variants'
        for name, row in manifest.items():
            staged = stage / 'Rooms' / f'forest_{name}_{slug.lower()}.mroom'
            if digest(staged) != row['output_sha256']:
                raise RuntimeError('Staged room changed after generation: ' + str(staged))
            copies.append((staged, rooms / staged.name))
        stem = slug + '_ForestVariants'
        copies.append((stage / 'MapTilesets' / (stem + '.png'), ROOT / 'MapTilesets' / (stem + '.png')))
        copies.append((stage / f'tilemapper_{stem}.txt', ROOT / 'TestGame' / f'tilemapper_{stem}.txt'))
    for source, target in copies:
        if target.exists() and digest(source) != digest(target):
            raise RuntimeError('Refusing to overwrite changed runtime file: ' + str(target))
    installed = 0
    for source, target in copies:
        if target.exists():
            continue
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(source, target)
        installed += 1
    receipt = {'originals_preserved': len(baseline['sources']), 'biomes': slugs,
               'files': {str(t.relative_to(ROOT)): digest(t) for _, t in copies}}
    (STAGED / 'promotion.json').write_text(json.dumps(receipt, indent=2) + '\n')
    print(f'Installed {installed} new file(s) for {len(slugs)} biome(s); all authored rooms preserved.')


if __name__ == '__main__':
    main(sys.argv[1:] or list(forest_variants.PALETTES))
