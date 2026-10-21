"""Install validated generated Jungle assets without overwriting authored content."""
import hashlib
import json
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[2]
STAGED = ROOT / 'docs/biome-content-audit/jungle-pool'

def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def main():
    baseline = json.loads((ROOT / 'docs/biome-content-audit/baseline.json').read_text())
    for source in baseline['sources']:
        if digest(ROOT / source['path']) != source['sha256']:
            raise RuntimeError('Authored source changed: ' + source['path'])
    manifest = json.loads((STAGED / 'manifest.json').read_text())
    if len(manifest) != 23 or any(row['missing'] for row in manifest.values()):
        raise RuntimeError('Jungle mapping is incomplete')
    copies = []
    for name, row in manifest.items():
        source = STAGED / 'Rooms' / ('forest_' + name + '_jungle.mroom')
        if digest(source) != row['output_sha256']:
            raise RuntimeError('Generated room changed: ' + name)
        copies.append((source, ROOT / 'TestGame/Rooms/Generated/Jungle/forest_variants' / source.name))
    copies.extend([
        (STAGED / 'MapTilesets/Jungle_ForestVariants.png', ROOT / 'MapTilesets/Jungle_ForestVariants.png'),
        (STAGED / 'tilemapper_Jungle_ForestVariants.txt', ROOT / 'TestGame/tilemapper_Jungle_ForestVariants.txt'),
    ])
    for source, target in copies:
        if target.exists() and digest(source) != digest(target):
            raise RuntimeError('Refusing to overwrite changed runtime asset: ' + str(target))
    subprocess.run([str(ROOT / 'tools/biome_content/build/proof_runtime.exe')], cwd=ROOT, check=True)
    for source, target in copies:
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(source, target)
    receipt = {'originals_preserved': len(baseline['sources']), 'variants': 23,
               'files': {str(target.relative_to(ROOT)): digest(target) for _, target in copies}}
    (STAGED / 'promotion.json').write_text(json.dumps(receipt, indent=2) + '\n')
    print('Installed 23 Jungle variants; all original rooms preserved.')

if __name__ == '__main__':
    main()
