"""One visually inspected conversion proof, staged outside the runtime library.

Requires Pillow only for the diagnostic contact sheet, never for modifying assets.
This is deliberately restricted to one hashed source with no props or falls.
"""
import json
from pathlib import Path
import pipeline

ROOT = Path(__file__).resolve().parents[2]
SOURCE_HASH = 'b09626e824fc32e111436a2dc96b67f8b363d577aad0eb3e4b2ef75cbedf2338'
SOURCE_HASHES = {'s': SOURCE_HASH,
    'nwe': 'aa82a81d7a77556b361eef59f224314a3b80ab6608c2abba56bf8b82af90d38b',
    'w': 'c1ebe0d747bbf8eab9ec2f113be419485df2d4fa543a145783fe0fdab7cb2635'}
# Explicit role mapping after viewing both original tilesheets. Bottom rock face
# uses its full authored 32px height (catalog TILE 17/16/18), extending below room.
RECTS = {
    (16, 0, 16, 16): ((96, 544, 16, 16), 'sand path becomes olive grass path'),
    (48, 0, 16, 16): ((96, 544, 16, 16), 'sand path variation becomes olive grass path'),
    (16, 16, 16, 16): ((16, 544, 16, 16), 'green grass floor'),
    (32, 16, 16, 16): ((16, 544, 16, 16), 'green grass floor'),
    (112, 16, 16, 16): ((176, 544, 16, 16), 'dark grass accent floor'),
    (48, 32, 16, 16): ((240, 496, 16, 16), 'grass tuft overlay'),
    (16, 304, 16, 16): ((304, 544, 16, 16), 'yellow flower cluster'),
    (48, 304, 16, 16): ((320, 576, 16, 16), 'contrasting teal flower cluster'),
    (0, 416, 16, 16): ((352, 704, 16, 16), 'fence upper left corner'),
    (16, 416, 16, 16): ((368, 704, 16, 16), 'fence upper horizontal edge'),
    (32, 416, 16, 16): ((384, 704, 16, 16), 'fence upper right corner'),
    (0, 432, 16, 16): ((352, 720, 16, 16), 'fence left vertical edge'),
    (32, 432, 16, 16): ((384, 720, 16, 16), 'fence right vertical edge'),
    (0, 448, 16, 16): ((352, 736, 16, 16), 'fence lower left corner'),
    (16, 448, 16, 16): ((368, 736, 16, 16), 'fence lower horizontal edge'),
    (32, 448, 16, 16): ((384, 736, 16, 16), 'fence lower right corner'),
    (64, 32, 16, 16): ((16, 544, 16, 16), 'continuous green grass floor'),
    (80, 32, 16, 16): ((16, 544, 16, 16), 'continuous green grass floor; subtle source variation consolidated'),
    (96, 32, 16, 16): ((16, 544, 16, 16), 'continuous green grass floor; subtle source variation consolidated'),
    (0, 16, 16, 16): ((16, 544, 16, 16), 'green grass floor'),
    (32, 0, 16, 16): ((96, 544, 16, 16), 'olive grass path'),
    (64, 208, 16, 16): ((352, 608, 16, 16), 'upper left wall corner'),
    (80, 208, 16, 16): ((368, 608, 16, 16), 'upper wall edge'),
    (96, 208, 16, 16): ((384, 608, 16, 16), 'upper right wall corner'),
    (64, 224, 16, 16): ((352, 624, 16, 16), 'left wall edge'),
    (96, 224, 16, 16): ((384, 624, 16, 16), 'right wall edge'),
    (64, 240, 16, 16): ((352, 640, 16, 32), 'lower left wall corner and face'),
    (80, 240, 16, 16): ((368, 640, 16, 32), 'lower wall edge and face'),
    (96, 240, 16, 16): ((384, 640, 16, 32), 'lower right wall corner and face'),
}


def gameplay_lines(text):
    result = []
    for line in text.splitlines():
        p = pipeline.tokens(line)
        if p and p[0] in ('ID', 'NAME', 'BIOME', 'TILESET'):
            continue
        if p and p[0] in pipeline.VISUAL:
            result.append([p[0], *p[7:]])
        else:
            result.append(p)
    return result


def convert(source):
    source_hash = pipeline.sha(source.read_bytes())
    if source_hash not in SOURCE_HASHES.values():
        raise ValueError('source hash changed; review mapping again')
    profile = {'target': 'Jungle', 'revision': 1, 'source_sha256': source_hash,
        'permitted_sheets': ['Jungle'], 'mappings': {},
        'scope': f'Forest {source.stem}.mroom only; no props, animations, custom colliders or falls',
        'visual_review': 'Original Forest.png and Jungle.png visually inspected; see contact.png.',
        'runtime_status': 'Candidate; consult runtime-validation.txt; not enabled in RoomLibrary.'}
    lines = []
    for line in source.read_text(encoding='utf-8-sig').splitlines():
        p = pipeline.tokens(line)
        if p[0] == 'ID':
            line = f'ID "variant_{p[1]}_jungle_r1"'
        elif p[0] == 'NAME':
            line = f'NAME "{p[1]} - Jungle proof"'
        elif p[0] == 'BIOME':
            line = 'BIOME 6'
        elif p[0] == 'TILESET':
            line = 'TILESET "Jungle"'
        elif p[0] in pipeline.VISUAL:
            rect, role = RECTS[tuple(map(int, p[3:7]))]
            key = pipeline.ref_key(p[:7])
            profile['mappings'][key] = {'visual_review': role,
                'geometry_review': 'Only art rectangle changes; placement/anchor/band preserved; no collision-bearing assets.',
                'replacements': [{'sheet': 'Jungle', 'rectangle': list(rect)}]}
            line = f'{p[0]} "Jungle" {p[2]} ' + ' '.join(map(str, rect)) + ' ' + ' '.join(p[7:])
        lines.append(line)
    for tile in ('0', '2'):
        profile['mappings'][pipeline.ref_key(['TILE_TYPE', 'Forest', tile])] = {
            'visual_review': 'Logical geometry retained; complete authored floor/wall visuals supply target art.',
            'geometry_review': 'Tile enum is unchanged.',
            'replacements': [{'sheet': 'Jungle', 'tile_type': int(tile)}]}
    return '\n'.join(lines) + '\n', profile


def contact(source_text, target_text, output):
    from PIL import Image, ImageDraw
    def render(text):
        canvas = Image.new('RGBA', (448, 288), (24, 24, 28, 255))
        sheets = {}
        for line in text.splitlines():
            p = pipeline.tokens(line)
            if not p or p[0] not in pipeline.VISUAL:
                continue
            sheet = sheets.setdefault(p[1], Image.open(ROOT / 'MapTilesets' / (p[1] + '.png')).convert('RGBA'))
            x, y, w, h, col, row = map(int, p[3:9])
            ax, ay = map(float, p[9:11]) if len(p) >= 11 else (0, 0)
            sprite = sheet.crop((x, y, x + w, y + h))
            canvas.alpha_composite(sprite, (int(col * 16 - ax), int(row * 16 - ay)))
        return canvas
    sheet = Image.new('RGBA', (912, 324), (20, 20, 24, 255))
    draw = ImageDraw.Draw(sheet)
    draw.text((8, 8), 'Original Forest (closed door)', fill='white')
    draw.text((464, 8), 'Jungle proof (closed door)', fill='white')
    sheet.alpha_composite(render(source_text), (0, 28))
    sheet.alpha_composite(render(target_text), (464, 28))
    sheet.convert('RGB').save(output)


def generate(name):
    source = ROOT / f'TestGame/Rooms/Forest/forest/{name}.mroom'
    data, profile = convert(source)
    pipeline.validate_profile(pipeline.room_inventory(source, ROOT), profile)
    destination = ROOT / 'docs/biome-content-audit/proof'
    destination.mkdir(parents=True, exist_ok=True)
    manifest_path = destination / f'{name}-manifest.json'
    prior = json.loads(manifest_path.read_text()) if manifest_path.exists() else {}
    output = destination / f'forest_{name}_jungle.mroom'
    if name == 's' and not manifest_path.exists() and (destination / 'manifest.json').exists():
        prior = json.loads((destination / 'manifest.json').read_text())
    digest = pipeline.write_guarded(output, data.encode(), prior.get('output_sha256'))
    manifest_path.write_text(json.dumps({'source_sha256': SOURCE_HASHES[name], 'output_sha256': digest,
        'profile_sha256': pipeline.sha(json.dumps(profile, sort_keys=True).encode()),
        'status': 'staged_candidate_not_runtime_enabled'}, indent=2) + '\n')
    (destination / f'{name}-profile.json').write_text(json.dumps(profile, indent=2) + '\n')
    contact(source.read_text(), data, destination / f'{name}-contact.png')
    print('Staged candidate: ' + str(output))


if __name__ == '__main__':
    for name in SOURCE_HASHES:
        generate(name)
