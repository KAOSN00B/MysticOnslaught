"""Explicit Forest -> Jungle conversion using only existing target artwork.

Derived prop atlas fits inspected target assets to original authored footprints.
This preserves placement, collision definitions, draw band and occlusion height.
All output is staged outside the runtime content tree.
"""
import json
from pathlib import Path
import shutil
import pipeline
import proof

ROOT = proof.ROOT
OUT = ROOT / 'docs/biome-content-audit/jungle-pool'
ATLAS = 'Jungle_ForestVariants'

# Each selected source rectangle has been inspected on Jungle.png. No coordinate
# equality or nearest-size heuristic is used to infer any match.
ASSETS = {
    'prop_0': (0, 16, 64, 64, 'broad green Jungle tree'),
    'prop_1': (64, 16, 64, 64, 'narrow green Jungle tree'),
    'prop_2': (128, 0, 64, 80, 'small leafy Jungle tree'),
    'prop_3': (192, 0, 64, 80, 'small alternate leafy Jungle tree'),
    'prop_4': (432, 496, 32, 32, 'rounded mossy boulder'),
    'prop_5': (448, 528, 32, 32, 'wide mossy boulder'),
    'prop_168711230338000_18': (432, 496, 32, 32, 'tall fitted mossy boulder'),
    'prop_168713593038100_19': (448, 528, 32, 32, 'alternate tall fitted mossy boulder'),
    'prop_173392298448400_1': (352, 496, 16, 16, 'small rounded mossy rock'),
    'prop_173393896656300_2': (384, 496, 16, 16, 'small angular mossy rock'),
    'prop_173395190463900_3': (400, 496, 16, 16, 'small flat mossy rock'),
    'prop_173396521756000_4': (416, 496, 16, 16, 'small alternate mossy rock'),
    'decor_0': (288, 528, 32, 32, 'yellow Jungle flowers'),
    'decor_1': (304, 560, 32, 32, 'teal Jungle flowers'),
    'decor_2': (288, 544, 48, 16, 'wide yellow flower bed'),
    'decor_3': (304, 544, 16, 16, 'yellow flower cluster'),
    'decor_4': (320, 576, 16, 16, 'teal flower cluster'),
    'decor_5': (272, 544, 16, 16, 'small yellow flower pair'),
    'decor_6': (336, 544, 16, 16, 'small yellow flower trio'),
    'decor_7': (304, 576, 16, 16, 'teal flower trio'),
    'decor_8': (288, 544, 16, 16, 'yellow flower sprig'),
}

RECTS = dict(proof.RECTS)
# Other colors of the same authored wall roles become inspected Jungle stone.
for sy, ty in [(112, 608), (128, 624), (144, 640)]:
    for sx, tx in [(64, 352), (80, 368), (96, 384)]:
        if sy == 128 and sx == 80:
            continue
        RECTS[(sx, sy, 16, 16)] = ((tx, ty, 16, 32 if sy == 144 else 16), 'stone wall corresponding corner/edge')
# Continuous floor color variants (separate from foliage overlays).
for sx, sy in [(0,32),(16,32),(32,32),(64,16),(80,16),(96,16),(64,0),(80,0),(96,0),(112,0)]:
    RECTS[(sx,sy,16,16)] = ((16,544,16,16), 'continuous Jungle grass floor')
# Inspected foliage and flowers; visual bands and placement anchors are retained.
for sx, sy, tx, ty in [
    (48,16,256,496),(16,368,304,544),(32,400,320,576),
    (0,304,304,544),(32,336,320,576),(0,256,256,496),
    (32,256,272,496),(64,288,288,512),(0,320,288,544),
    (16,320,304,544),(32,304,320,544),(32,320,304,576),
    (48,256,240,512),(48,320,320,576),(64,272,320,512),(64,320,304,544)]:
    RECTS[(sx,sy,16,16)] = ((tx,ty,16,16), 'Jungle foliage/flower counterpart')
# Wooden platforms: 3x3 outer edges and center, then lower fence-backed variant.
for sy, ty in [(416,608),(432,624),(448,640),(464,608),(496,624),(512,640)]:
    for sx, tx in [(64,464),(80,480),(96,496)]:
        RECTS[(sx,sy,16,16)] = ((tx,ty,16,16), 'wood platform matching edge/corner')
# Grass island contours used as painted Ground (not physical collision).
for sy, ty in [(112,528),(128,544),(144,560)]:
    for sx, tx in [(0,0),(16,16),(32,32)]:
        RECTS[(sx,sy,16,16)] = ((tx,ty,16,16), 'rounded Jungle grass patch contour')
# Two painted halves share one fitted tree, preserving their seam and footprint.
FITTED_VISUALS = {(176,0,16,16):(480,448,16,16), (176,16,16,16):(480,464,16,16)}


def build_assets():
    from PIL import Image
    catalogs = pipeline.catalog_inventory(ROOT)
    jungle = Image.open(ROOT / 'MapTilesets/Jungle.png').convert('RGBA')
    atlas = Image.new('RGBA', (512, 512))
    records, mapping = [], {}
    x = y = row_h = 0
    for source in catalogs['Forest']['records']:
        if source[0] not in ('PROPV3', 'DECORV3') or source[1] not in ASSETS:
            continue
        ident = source[1]
        sx, sy, sw, sh, role = ASSETS[ident]
        width, height = map(int, source[6:8])
        if x + width > 512:
            x, y, row_h = 0, y + row_h, 0
        assert y + height <= 512
        art = jungle.crop((sx,sy,sx+sw,sy+sh)).resize((width,height), Image.Resampling.NEAREST)
        atlas.alpha_composite(art, (x,y))
        record = [source[0], ident, 'Jungle counterpart: '+role, ATLAS,
                  str(x),str(y),str(width),str(height),*source[8:]]
        records.append(record)
        mapping[ident] = {'source_rectangle': list(map(int,source[4:8])),
            'target_original_rectangle': [sx,sy,sw,sh], 'atlas_rectangle':[x,y,width,height],
            'role':role,'fit':'nearest-neighbor to exact original footprint',
            'collision': source[8:]}
        x += width
        row_h = max(row_h,height)
    for source in catalogs['Forest']['records']:
        if source[0] == 'ANIMDECORV3':
            # Original four-frame flower animation -> Jungle four-frame foliage.
            record = list(source[:7])
            record[2], record[3] = 'Jungle animated foliage', 'Jungle'
            for frame in range(int(record[6])):
                record.extend(map(str,(256,592+frame*16,16,16)))
            records.append(record)
            mapping[source[1]] = {'role':'animated Jungle foliage','frames':record[7:]}
    for source in catalogs['Village_Animations']['records']:
        if source[0] == 'ANIMDECORV3' and source[3] == 'FD_Animated_Water':
            records.append(source)
            mapping[source[1]] = {'role':'explicitly permitted shared water animation','frames':source[7:]}
    assets = OUT / 'MapTilesets'
    assets.mkdir(parents=True, exist_ok=True)
    assert y + row_h <= 448, 'Prop packing overlaps reserved fitted-tree area'
    tree = jungle.crop((0,16,64,80)).resize((16,32), Image.Resampling.NEAREST)
    atlas.alpha_composite(tree, (480,448))
    atlas.save(assets / (ATLAS+'.png'))
    for sheet in ('Jungle','FD_Animated_Water'):
        shutil.copyfile(ROOT/'MapTilesets'/(sheet+'.png'), assets/(sheet+'.png'))
    def serialize(r):
        return ' '.join(json.dumps(v) if i in (1,2,3) else v for i,v in enumerate(r))
    (OUT / ('tilemapper_'+ATLAS+'.txt')).write_text('\n'.join(serialize(r) for r in records)+'\n')
    shutil.copyfile(ROOT/'TestGame/tilemapper_Jungle.txt',OUT/'tilemapper_Jungle.txt')
    return mapping


def convert(source, assets):
    baseline = json.loads((ROOT/'docs/biome-content-audit/baseline.json').read_text())
    expected = next(s['sha256'] for s in baseline['sources'] if s['path'] == source.relative_to(ROOT).as_posix())
    if pipeline.sha(source.read_bytes()) != expected:
        raise ValueError('source hash drift')
    output, missing = [], set()
    for line in source.read_text().splitlines():
        p = pipeline.tokens(line)
        if not p:
            output.append(line); continue
        if p[0]=='ID': line = f'ID "variant_{p[1]}_jungle_pool_r1"'
        elif p[0]=='NAME': line = f'NAME "{p[1]} - Jungle"'
        elif p[0]=='BIOME': line = 'BIOME 6'
        elif p[0]=='TILESET': line = 'TILESET "Jungle"'
        elif p[0] in pipeline.VISUAL:
            key = tuple(map(int,p[3:7]))
            if p[1]=='Forest' and key in FITTED_VISUALS:
                line = f'{p[0]} "{ATLAS}" {p[2]} '+' '.join(map(str,FITTED_VISUALS[key]))+' '+' '.join(p[7:])
            elif p[1]!='Forest' or key not in RECTS:
                missing.add(pipeline.ref_key(p[:7]))
            else:
                rectangle, role = RECTS[key]
                line = f'{p[0]} "Jungle" {p[2]} '+' '.join(map(str,rectangle))+' '+' '.join(p[7:])
        elif p[0] in pipeline.PLACEMENTS:
            if p[1] not in assets:
                missing.add(pipeline.ref_key(p[:3]))
            else:
                line = f'{p[0]} "{p[1]}" "{ATLAS}" '+' '.join(p[3:])
        output.append(line)
    if missing:
        return None, sorted(missing)
    return '\n'.join(output)+'\n', []


def main():
    assets = build_assets()
    manifest_path = OUT/'manifest.json'
    prior = json.loads(manifest_path.read_text()) if manifest_path.exists() else {}
    coverage = {}
    for source in sorted((ROOT/'TestGame/Rooms/Forest/forest').glob('*.mroom')):
        data, missing = convert(source,assets)
        record = {'source_sha256':pipeline.sha(source.read_bytes()),'missing':missing,
                  'status':'unmapped' if missing else 'staged_unvalidated'}
        if data:
            path = OUT/'Rooms'/('forest_'+source.stem+'_jungle.mroom')
            record['output_sha256'] = pipeline.write_guarded(path,data.encode(),prior.get(source.stem,{}).get('output_sha256'))
        coverage[source.stem] = record
    manifest_path.write_text(json.dumps(coverage,indent=2)+'\n')
    (OUT/'mapping-profile.json').write_text(json.dumps({'permitted_sheets':['Jungle',ATLAS,'FD_Animated_Water'],
        'derived_assets':assets,'rectangles':{str(k):v for k,v in RECTS.items()},
        'fitted_visuals':{str(k):v for k,v in FITTED_VISUALS.items()}},indent=2)+'\n')
    print(json.dumps({k:len(v['missing']) for k,v in coverage.items()},indent=2))


if __name__=='__main__': main()
