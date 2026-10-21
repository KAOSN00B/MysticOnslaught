"""Deterministic, read-only authored room audit; conservative generator prerequisites.

No artwork equivalence is inferred from coordinates, names, sizes, or numeric IDs.
The audit never emits .mroom files. Mapping approval is only a prerequisite to
future runtime/visual verification, not evidence of completed room coverage.
"""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import shlex
import struct

BIOMES = ('Ancient Castle', 'Caverns', 'Demons Insides', 'Dream Realm', 'Forest',
          'Graveyard', 'Jungle', 'Lost City', 'The Sanctuary', 'Wastelands')
VISUAL = {'GROUND', 'VISUAL', 'DOOR'}
PLACEMENTS = {'PROP', 'ANIMPROP', 'DECOR', 'ANIMDECOR'}


def sha(data):
    return hashlib.sha256(data).hexdigest()


def tokens(line):
    return shlex.split(line, posix=True)


def ref_key(parts):
    return json.dumps(parts, separators=(',', ':'), ensure_ascii=False)


def room_inventory(path, root):
    data = path.read_bytes()
    lines = data.decode('utf-8-sig').splitlines()
    fields, refs, geometry, counts = {}, Counter(), [], Counter()
    in_tiles = False
    tile_types = set()
    for line in lines:
        parts = tokens(line)
        if not parts:
            continue
        tag = parts[0]
        counts[tag] += 1
        if tag in ('ID', 'NAME', 'BIOME', 'TILESET', 'ROOMTYPE', 'MROOM'):
            fields[tag] = parts[1]
        if tag == 'TILES_BEGIN':
            in_tiles = True
        elif tag == 'TILES_END':
            in_tiles = False
        elif in_tiles:
            tile_types.update(parts)
        if tag in VISUAL:
            # Source sheet, tile type, pixel rectangle. Placement/anchor remain geometry.
            refs[ref_key(parts[:7])] += 1
            geometry.append([tag, *parts[7:]])
        elif tag in PLACEMENTS:
            refs[ref_key(parts[:3])] += 1
            geometry.append([tag, *parts[3:]])
        elif tag not in ('ID', 'NAME', 'BIOME', 'TILESET'):
            geometry.append(parts)
    for tile in sorted(tile_types, key=int):
        refs[ref_key(['TILE_TYPE', fields['TILESET'], tile])] += 1
    return {'path': path.relative_to(root).as_posix(), 'sha256': sha(data),
            'id': fields['ID'], 'name': fields['NAME'], 'biome': int(fields['BIOME']),
            'tileset': fields['TILESET'], 'room_type': int(fields['ROOMTYPE']),
            'version': int(fields['MROOM']), 'geometry_sha256': sha(json.dumps(geometry).encode()),
            'reference_counts': dict(sorted(refs.items())),
            'placements': sum(counts[k] for k in PLACEMENTS),
            'visuals': sum(counts[k] for k in VISUAL),
            'custom_colliders': counts['COLLIDER'], 'fall_rectangles': counts['FALLRECT']}


def catalog_inventory(root):
    result = {}
    for image in sorted((root / 'MapTilesets').glob('*.png')):
        data = image.read_bytes()
        if data[:8] != b'\x89PNG\r\n\x1a\n':
            raise ValueError(f'not a PNG: {image}')
        metadata = root / 'TestGame' / ('tilemapper_' + image.stem + '.txt')
        records = [tokens(s) for s in metadata.read_text(encoding='utf-8-sig').splitlines()] if metadata.exists() else []
        records = [r for r in records if r]
        result[image.stem] = {'image': image.relative_to(root).as_posix(),
            'image_sha256': sha(data), 'image_size': list(struct.unpack('>II', data[16:24])),
            'metadata': metadata.relative_to(root).as_posix() if metadata.exists() else None,
            'metadata_sha256': sha(metadata.read_bytes()) if metadata.exists() else None,
            'record_counts': dict(sorted(Counter(r[0] for r in records).items())), 'records': records}
    return result


def baseline_issues(source, catalogs):
    issues = []
    for key in source['reference_counts']:
        ref = json.loads(key)
        sheet = ref[1] if ref[0] in VISUAL or ref[0] == 'TILE_TYPE' else ref[2]
        if not sheet:
            sheet = source['tileset']
        catalog = catalogs.get(sheet)
        if catalog is None:
            issues.append({'reference': key, 'issue': 'missing sheet: ' + sheet})
        elif ref[0] in VISUAL:
            x, y, w, h = map(float, ref[3:7])
            iw, ih = catalog['image_size']
            if min(x, y) < 0 or min(w, h) <= 0 or x + w > iw or y + h > ih:
                issues.append({'reference': key, 'issue': 'sprite rectangle out of bounds'})
        elif ref[0] in PLACEMENTS:
            records = [r for r in catalog['records'] if r[0] in (ref[0], ref[0] + 'V2', ref[0] + 'V3')]
            prefix = {'PROP': 'prop', 'DECOR': 'decor', 'ANIMPROP': 'anim_prop', 'ANIMDECOR': 'anim_decor'}[ref[0]]
            if not any((f'{prefix}_{i}' if r[0] == ref[0] else r[1]) == ref[1]
                       for i, r in enumerate(records)):
                issues.append({'reference': key, 'issue': 'asset ID absent from catalog'})
    return issues


def audit(root):
    root = Path(root)
    sources = [room_inventory(p, root) for p in sorted((root / 'TestGame/Rooms').rglob('*.mroom'))
               if 'Generated' not in p.relative_to(root / 'TestGame/Rooms').parts]
    catalogs = catalog_inventory(root)
    coverage = []
    for source in sources:
        source['baseline_issues'] = baseline_issues(source, catalogs)
        for index, biome in enumerate(BIOMES):
            original = index == source['biome'] and biome == source['tileset']
            coverage.append({'source_id': source['id'], 'source_path': source['path'],
                'source_sha256': source['sha256'], 'target_biome': biome,
                'status': 'original' if original else 'blocked',
                'output_path': source['path'] if original else None,
                'mapping_revision': None,
                'unmapped_references': [] if original else sorted(source['reference_counts']),
                'remaining_validation': ['runtime load', 'effective collision', 'traversability',
                    'spawn/reward clearance', 'visual review', 'native/web packaging']})
    return {'schema': 1, 'sources': sources, 'catalogs': catalogs, 'coverage': coverage,
            'summary': {'originals': len(sources), 'pairs': len(coverage),
                'original_pairs': sum(p['status'] == 'original' for p in coverage),
                'validated_generated_pairs': 0,
                'blocked_pairs': sum(p['status'] == 'blocked' for p in coverage)}}


def verify_baseline(baseline, current):
    before = {s['path']: s['sha256'] for s in baseline['sources']}
    after = {s['path']: s['sha256'] for s in current['sources']}
    if before != after:
        raise ValueError('source drift: ' + ', '.join(sorted(k for k in before.keys() | after.keys()
                                                         if before.get(k) != after.get(k))))


def validate_profile(source, profile):
    if profile['target'] not in BIOMES:
        raise ValueError('unknown target biome')
    mappings = profile.get('mappings', {})
    missing = sorted(set(source['reference_counts']) - mappings.keys())
    if missing:
        raise ValueError('unmapped references: ' + ', '.join(missing))
    for key in source['reference_counts']:
        entry = mappings[key]
        if not entry.get('visual_review') or not entry.get('geometry_review'):
            raise ValueError('unreviewed mapping: ' + key)
        if not entry.get('replacements'):
            raise ValueError('empty replacement would drop art: ' + key)
        for replacement in entry['replacements']:
            if replacement['sheet'] not in profile.get('permitted_sheets', []):
                raise ValueError('foreign sheet: ' + replacement['sheet'])
    return True


def write_guarded(path, data, expected_hash):
    """Only for generated output: caller must separately enforce generated-root confinement."""
    path = Path(path)
    if path.exists() and (expected_hash is None or sha(path.read_bytes()) != expected_hash):
        raise ValueError('refusing edited or untracked output: ' + str(path))
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_suffix(path.suffix + '.tmp')
    if temporary.exists():
        raise ValueError('temporary output already exists: ' + str(temporary))
    with temporary.open('xb') as stream:
        stream.write(data)
    temporary.replace(path)
    return sha(data)


def render_report(result):
    summary = result['summary']
    lines = ['# Authored room biome coverage audit', '',
        f"{summary['originals']} preserved source identities; {summary['pairs']} required pairs.",
        f"{summary['original_pairs']} original entries, 0 validated generated variants, {summary['blocked_pairs']} blocked pairs.",
        '', 'Original entries are coverage identities, not assertions that baseline content passed runtime or visual QA.',
        'No .mroom files were created or edited. No cross-biome mapping is approved.', '',
        '| Biome | Source rooms | Tile records | Prop/decor records |', '|---|---:|---:|---:|']
    for biome in BIOMES:
        counts = result['catalogs'][biome]['record_counts']
        lines.append(f"| {biome} | {sum(s['tileset'] == biome for s in result['sources'])} | {counts.get('TILE', 0)} | {sum(n for k,n in counts.items() if k.startswith(('PROP', 'DECOR', 'ANIMPROP', 'ANIMDECOR')))} |")
    lines.extend(['', '## Baseline reference issues', ''])
    issues = [(s, i) for s in result['sources'] for i in s['baseline_issues']]
    lines.extend([f"- `{s['path']}`: {i['issue']}: `{i['reference']}`" for s,i in issues] or ['None detected by the limited sheet/ID/rectangle audit.'])
    lines.extend(['', '## Next required work', '',
        '1. Use mapping-worklist.json to inspect every distinct reference against the target sheet. Record visual role, orientation, footprint, anchors and explicit replacements; do not copy coordinates or match numeric IDs blindly.',
        '2. Fill catalog gaps using existing art. Retain all props with equivalent effective collision, or author target-only multi-piece replacements.',
        '3. Implement gated .mroom serialization and runtime BuildRoomLayout comparison, then prove representative source rooms including doors, falls, colliders and special roles.',
        '4. Generate all 495 missing variants into a dedicated Generated subtree with hash provenance and edited-output protection.',
        '5. Review every variant visually; verify geometry, door traversal, spawn/reward clearance and packaged native/web loading.', '',
        'coverage.json contains all source and asset hashes, exact reference counts, catalog records, all 550 pairs and each missing mapping.',
        'The script deliberately stops at inventory and mapping prerequisites. It is not a finished variant generator.', ''])
    return '\n'.join(lines)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument('--output', type=Path, default=Path('docs/biome-content-audit'))
    args = parser.parse_args()
    result = audit(args.root)
    output = args.root / args.output
    baseline = output / 'baseline.json'
    if baseline.exists():
        verify_baseline(json.loads(baseline.read_text(encoding='utf-8')), result)
    output.mkdir(parents=True, exist_ok=True)
    if not baseline.exists():
        baseline.write_text(json.dumps({'sources': result['sources']}, indent=2) + '\n', encoding='utf-8')
    (output / 'coverage.json').write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
    worklist = {}
    for biome in BIOMES:
        required = sorted({key for s in result['sources'] if s['tileset'] != biome for key in s['reference_counts']})
        worklist[biome] = {'target': biome, 'permitted_sheets': [biome], 'revision': 0,
            'mappings': {}, 'required_references': required}
    (output / 'mapping-worklist.json').write_text(json.dumps(worklist, indent=2) + '\n', encoding='utf-8')
    (output / 'README.md').write_text(render_report(result), encoding='utf-8')
    print(json.dumps(result['summary'], sort_keys=True))


if __name__ == '__main__':
    main()
