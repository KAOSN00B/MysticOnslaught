"""Forest layout families -> every other biome, using each biome's own art.

Same method as jungle_pool.py, generalised to the remaining biomes:
  * every Forest art reference is classified by ROLE (floor, path, patch,
    3x3 ring piece, overlay, bridge, prop, decor) after inspecting Forest.png;
  * each biome PALETTE names the inspected target art for every role;
  * props/decor are re-drawn into a per-biome atlas at the EXACT Forest
    footprint, keeping Forest's collision rectangles, so effective collision,
    fall zones, doors and room roles are unchanged.
Output is staged under docs/biome-content-audit/forest-variants/ only; the
authored Forest rooms are hash-checked and never modified.

    python tools/biome_content/forest_variants.py [biome-slug ...]
"""
import json
import shutil
import sys
from pathlib import Path

import pipeline

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'docs/biome-content-audit/forest-variants'
FOREST_DIR = ROOT / 'TestGame/Rooms/Forest/forest'


def R(sheet, x, y, w=16, h=16):
    return (sheet, (x, y, w, h))


# ---------------------------------------------------------------------------
# Forest roles (inspected on Forest.png)
# ---------------------------------------------------------------------------
FLOOR_ROLES = {
    (0, 16): 'floor_main', (16, 16): 'floor_main_b', (32, 16): 'floor_main_b',
    (64, 16): 'floor_alt1', (80, 16): 'floor_alt1', (96, 16): 'floor_alt1',
    (0, 32): 'floor_alt2', (16, 32): 'floor_alt2', (32, 32): 'floor_alt2',
    (64, 32): 'floor_alt3', (80, 32): 'floor_alt3', (96, 32): 'floor_alt3',
    (16, 0): 'path_sand', (32, 0): 'path_sand', (48, 0): 'path_sand',
    (80, 0): 'path_dirt', (96, 0): 'path_dirt', (112, 0): 'path_dirt',
    (64, 0): 'patch_a', (48, 16): 'patch_a', (48, 32): 'patch_a',
    (112, 16): 'patch_b', (112, 32): 'patch_b',
}
RING_POS = {(0, 0): 'TL', (1, 0): 'T', (2, 0): 'TR', (0, 1): 'L', (2, 1): 'R',
            (0, 2): 'BL', (1, 2): 'B', (2, 2): 'BR'}
# (origin x, origin y, ring kind): fence = room border fence; cliff = raised
# grass/rock rims around pools and islands.
RINGS = [(0, 416, 'fence'), (64, 112, 'cliff'), (64, 208, 'cliff'), (0, 112, 'cliff')]
OVERLAYS = [(0, 256), (32, 256), (48, 256), (64, 272), (64, 288), (0, 304), (16, 304),
            (32, 304), (48, 304), (0, 320), (16, 320), (32, 320), (48, 320), (64, 320),
            (32, 336), (16, 368), (32, 400)]
BRIDGE = {(x, y) for x in (64, 80, 96) for y in (416, 432, 448, 464, 496, 512)}
FITTED_TREE = {(176, 0): 0, (176, 16): 1}   # two painted halves of one 16x32 tree

# Forest prop/decor ids -> slot role (footprints come from the Forest catalog).
PROP_ROLES = {
    'prop_0': 'big', 'prop_1': 'mid', 'prop_2': 'small', 'prop_3': 'small2',
    'prop_4': 'rock', 'prop_5': 'rock2',
    'prop_168711230338000_18': 'tall', 'prop_168713593038100_19': 'tall2',
    'prop_173392298448400_1': 'pebble', 'prop_173393896656300_2': 'pebble2',
    'prop_173395190463900_3': 'pebble3', 'prop_173396521756000_4': 'pebble',
}
DECOR_ROLES = {'decor_0': 'grid', 'decor_1': 'grid', 'decor_2': 'row', 'decor_3': 'single',
               'decor_4': 'single', 'decor_5': 'single', 'decor_6': 'single',
               'decor_7': 'single', 'decor_8': 'single'}
WATER_IDS = ['anim_decor_169099280120400_20', 'anim_decor_169117514979800_21',
             'anim_decor_169126202018300_22', 'anim_decor_169135452198400_23',
             'anim_decor_169144723228100_24', 'anim_decor_169155040957600_25',
             'anim_decor_169166379563000_26', 'anim_decor_169173133546700_27',
             'anim_decor_169184488769900_28', 'anim_decor_169199441827800_29',
             'anim_decor_169211662238100_30']

# ---------------------------------------------------------------------------
# Biome palettes (each rectangle inspected on its sheet; see sheets/ renders)
# ---------------------------------------------------------------------------
CAV, GY, VG, DEM, DRM, LC, SAN, WST, AC, GT = (
    'Caverns', 'Graveyard', 'Village_Ground', 'Demons Insides', 'Dream Realm',
    'Lost City', 'The Sanctuary', 'Wastelands', 'Ancient Castle', 'Ground TIles')

PALETTES = {
    'Caverns': dict(
        index=1, folder='Caverns', sheet=CAV, liquid='water', permitted=[CAV],
        floor_main=[R(CAV, 144, 16)], floor_main_b=[R(CAV, 144, 64), R(CAV, 224, 16)],
        floor_alt1=[R(CAV, 144, 112)], floor_alt2=[R(CAV, 176, 48)], floor_alt3=[R(CAV, 176, 96)],
        path_sand=[R(CAV, 224, 16)], path_dirt=[R(CAV, 144, 80)],
        patch_a=[R(CAV, 176, 0)], patch_b=[R(CAV, 144, 112)],
        fence=(CAV, 0, 448), cliff='tiles', bridge=[R(CAV, 16, 432)],
        overlays=[R(CAV, 192, 144), R(CAV, 224, 144), R(CAV, 240, 160), R(CAV, 208, 176),
                  R(CAV, 80, 448), R(CAV, 96, 464), R(CAV, 272, 32), R(CAV, 320, 16)],
        flower=[R(CAV, 80, 448), R(CAV, 80, 464), R(CAV, 80, 480), R(CAV, 80, 464)],
        big=[R(CAV, 288, 80, 32, 48), R(CAV, 288, 80, 32, 48)], mid=[R(CAV, 288, 80, 32, 48)],
        small=[R(CAV, 400, 64, 16, 32)], small2=[R(CAV, 192, 320, 16, 32)],
        rock=[R(CAV, 336, 64, 32, 32)], rock2=[R(CAV, 416, 96, 32, 32)],
        tall=[R(CAV, 400, 64, 16, 32)], tall2=[R(CAV, 192, 320, 16, 32)],
        pebble=[R(CAV, 272, 48)], pebble2=[R(CAV, 304, 64)], pebble3=[R(CAV, 320, 64)]),
    'Graveyard': dict(
        index=5, folder='Graveyard', sheet=GY, liquid='water', permitted=[GY, VG],
        floor_main=[R(VG, 160, 80)], floor_main_b=[R(VG, 96, 80)],
        floor_alt1=[R(VG, 224, 80)], floor_alt2=[R(VG, 96, 80)], floor_alt3=[R(VG, 160, 80)],
        path_sand=[R(VG, 288, 80)], path_dirt=[R(VG, 224, 80)],
        patch_a=[R(VG, 352, 80)], patch_b=[R(VG, 32, 80)],
        fence=(GY, 0, 144), cliff=(GY, 0, 208), bridge=[R(VG, 224, 80)],
        overlays=[R(GY, 16, 0), R(GY, 0, 16), R(GY, 32, 16), R(GY, 32, 32), R(GY, 0, 32),
                  R(GY, 16, 32), R(GY, 16, 16)],
        flower=[R(GY, 16, 16), R(GY, 16, 16), R(GY, 16, 16), R(GY, 16, 16)],
        big=[R(GY, 304, 32, 32, 32), R(GY, 272, 32, 32, 32)], mid=[R(GY, 368, 32, 32, 32)],
        small=[R(GY, 256, 32, 16, 32)], small2=[R(GY, 176, 160, 16, 32)],
        rock=[R(GY, 272, 160, 32, 32)], rock2=[R(GY, 304, 160, 32, 32)],
        tall=[R(GY, 176, 160, 16, 32)], tall2=[R(GY, 192, 160, 16, 32)],
        pebble=[R(GY, 16, 0)], pebble2=[R(GY, 0, 16)], pebble3=[R(GY, 32, 16)]),
    'DemonsInsides': dict(
        index=2, folder='DemonsInsides', sheet=DEM, liquid='lava', permitted=[DEM, 'FD_Animated_Lava'],
        floor_main=[R(DEM, 48, 16)], floor_main_b=[R(DEM, 16, 16), R(DEM, 80, 16)],
        floor_alt1=[R(DEM, 32, 0)], floor_alt2=[R(DEM, 64, 0)], floor_alt3=[R(DEM, 96, 16)],
        path_sand=[R(DEM, 48, 0)], path_dirt=[R(DEM, 16, 32)],
        patch_a=[R(DEM, 0, 64)], patch_b=[R(DEM, 16, 112)],
        fence='tiles', cliff='tiles', bridge=[R(DEM, 48, 16)],
        overlays=[R(DEM, 368, 0), R(DEM, 384, 0), R(DEM, 400, 0), R(DEM, 368, 48),
                  R(DEM, 384, 64), R(DEM, 400, 80), R(DEM, 416, 80), R(DEM, 416, 32)],
        flower=[R(DEM, 384, 0), R(DEM, 384, 0), R(DEM, 384, 0), R(DEM, 384, 0)],
        big=[R(DEM, 256, 160, 32, 48), R(DEM, 256, 208, 32, 48)], mid=[R(DEM, 256, 160, 32, 48)],
        small=[R(DEM, 112, 128, 16, 32)], small2=[R(DEM, 144, 128, 16, 32)],
        rock=[R(DEM, 112, 0, 32, 32)], rock2=[R(DEM, 144, 0, 32, 32)],
        tall=[R(DEM, 112, 128, 16, 32)], tall2=[R(DEM, 144, 128, 16, 32)],
        pebble=[R(DEM, 368, 0)], pebble2=[R(DEM, 400, 0)], pebble3=[R(DEM, 416, 32)]),
    'DreamRealm': dict(
        index=3, folder='DreamRealm', sheet=DRM, liquid='water', permitted=[DRM],
        floor_main=[R(DRM, 16, 128)], floor_main_b=[R(DRM, 32, 0), R(DRM, 64, 0)],
        floor_alt1=[R(DRM, 144, 0)], floor_alt2=[R(DRM, 96, 0)], floor_alt3=[R(DRM, 112, 16)],
        path_sand=[R(DRM, 144, 128)], path_dirt=[R(DRM, 208, 0)],
        patch_a=[R(DRM, 96, 0)], patch_b=[R(DRM, 112, 16)],
        fence='tiles', cliff='tiles', bridge=[R(DRM, 144, 128)],
        overlays=[R(DRM, 96, 0), R(DRM, 112, 16), R(DRM, 64, 0), R(DRM, 32, 0)],
        flower=[R(DRM, 192, 272), R(DRM, 208, 272), R(DRM, 224, 272), R(DRM, 240, 272)],
        big=[R(DRM, 304, 112, 32, 32), R(DRM, 432, 112, 32, 32)], mid=[R(DRM, 272, 112, 32, 32)],
        small=[R(DRM, 304, 80, 16, 32)], small2=[R(DRM, 288, 80, 16, 32)],
        rock=[R(DRM, 304, 112, 32, 32)], rock2=[R(DRM, 432, 112, 32, 32)],
        tall=[R(DRM, 496, 32, 16, 32)], tall2=[R(DRM, 256, 112, 16, 32)],
        pebble=[R(DRM, 304, 48)], pebble2=[R(DRM, 320, 48)], pebble3=[R(DRM, 336, 48)]),
    'LostCity': dict(
        index=7, folder='LostCity', sheet=LC, liquid='water', permitted=[LC],
        floor_main=[R(LC, 64, 48)], floor_main_b=[R(LC, 80, 48), R(LC, 64, 64)],
        floor_alt1=[R(LC, 192, 32)], floor_alt2=[R(LC, 288, 32)], floor_alt3=[R(LC, 48, 64)],
        path_sand=[R(LC, 192, 32)], path_dirt=[R(LC, 16, 96)],
        patch_a=[R(LC, 288, 32)], patch_b=[R(LC, 192, 32)],
        fence='tiles', cliff='tiles', bridge=[R(LC, 192, 32)],
        overlays=[R(LC, 0, 112), R(LC, 16, 112), R(LC, 0, 128), R(LC, 16, 128), R(LC, 0, 96)],
        flower=[R(LC, 352, 336), R(LC, 368, 336), R(LC, 384, 336), R(LC, 400, 336)],
        big=[R(LC, 448, 176, 48, 32)], mid=[R(LC, 224, 144, 32, 32)],
        small=[R(LC, 224, 96, 16, 32)], small2=[R(LC, 224, 96, 16, 32)],
        rock=[R(LC, 448, 176, 32, 32)], rock2=[R(LC, 32, 200, 32, 24)],
        tall=[R(LC, 224, 96, 16, 32)], tall2=[R(LC, 224, 96, 16, 32)],
        pebble=[R(LC, 400, 144)], pebble2=[R(LC, 384, 144)], pebble3=[R(LC, 416, 144)]),
    'TheSanctuary': dict(
        index=8, folder='TheSanctuary', sheet=SAN, liquid='water', permitted=[SAN],
        floor_main=[R(SAN, 0, 0)], floor_main_b=[R(SAN, 16, 0), R(SAN, 32, 0)],
        floor_alt1=[R(SAN, 16, 16)], floor_alt2=[R(SAN, 0, 16)], floor_alt3=[R(SAN, 256, 0)],
        path_sand=[R(SAN, 256, 0)], path_dirt=[R(SAN, 272, 0)],
        patch_a=[R(SAN, 16, 32)], patch_b=[R(SAN, 32, 32)],
        fence='tiles', cliff='tiles', bridge=[R(SAN, 256, 0)],
        overlays=[R(SAN, 16, 16), R(SAN, 32, 16), R(SAN, 0, 32), R(SAN, 16, 32), R(SAN, 32, 32),
                  R(SAN, 0, 48), R(SAN, 16, 48), R(SAN, 32, 48), R(SAN, 0, 64), R(SAN, 16, 64)],
        flower=[R(SAN, 80, 176), R(SAN, 80, 176), R(SAN, 80, 176), R(SAN, 80, 176)],
        big=[R(SAN, 0, 112, 48, 48)], mid=[R(SAN, 96, 176, 32, 28)],
        small=[R(SAN, 128, 320, 16, 32)], small2=[R(SAN, 144, 320, 16, 32)],
        rock=[R(SAN, 128, 176, 32, 28)], rock2=[R(SAN, 160, 176, 32, 28)],
        tall=[R(SAN, 128, 320, 16, 32)], tall2=[R(SAN, 160, 320, 16, 32)],
        pebble=[R(SAN, 80, 176)], pebble2=[R(SAN, 80, 200)], pebble3=[R(SAN, 64, 128)]),
    'Wastelands': dict(
        index=9, folder='Wastelands', sheet=WST, liquid='water', permitted=[WST, GT],
        floor_main=[R(GT, 224, 80)], floor_main_b=[R(GT, 288, 80)],
        floor_alt1=[R(GT, 352, 80)], floor_alt2=[R(GT, 224, 208)], floor_alt3=[R(GT, 288, 208)],
        path_sand=[R(GT, 160, 208)], path_dirt=[R(GT, 352, 208)],
        patch_a=[R(GT, 32, 208)], patch_b=[R(GT, 96, 208)],
        fence='tiles', cliff='tiles', bridge=[R(GT, 160, 208)],
        overlays=[R(WST, 576, 304), R(WST, 592, 304), R(WST, 352, 304), R(WST, 336, 288),
                  R(WST, 304, 288), R(WST, 240, 288), R(WST, 240, 304), R(WST, 160, 288),
                  R(WST, 176, 304), R(WST, 432, 64), R(WST, 464, 80), R(WST, 384, 64)],
        flower=[R(WST, 336, 288), R(WST, 336, 288), R(WST, 336, 288), R(WST, 336, 288)],
        big=[R(WST, 256, 128, 64, 64)], mid=[R(WST, 448, 0, 32, 32)],
        small=[R(WST, 368, 0)], small2=[R(WST, 384, 0)],
        rock=[R(WST, 448, 0, 32, 32)], rock2=[R(WST, 480, 0, 32, 32)],
        tall=[R(WST, 416, 0)], tall2=[R(WST, 368, 0)],
        pebble=[R(WST, 368, 0)], pebble2=[R(WST, 384, 0)], pebble3=[R(WST, 416, 0)]),
    'AncientCastle': dict(
        # Ancient Castle has no registered props or decor; furniture comes from
        # Lost City, which shares the "Ancient Castle" family header in its
        # catalog. Columns are composed from the castle's own column art.
        index=0, folder='AncientCastle', sheet=AC, liquid='water', permitted=[AC, LC],
        floor_main=[R(AC, 16, 16)], floor_main_b=[R(AC, 16, 48), R(AC, 0, 48)],
        floor_alt1=[R(AC, 272, 16)], floor_alt2=[R(AC, 16, 80)], floor_alt3=[R(AC, 272, 64)],
        path_sand=[R(AC, 272, 16)], path_dirt=[R(AC, 0, 16)],
        patch_a=[R(AC, 0, 64)], patch_b=[R(AC, 16, 96)],
        fence='tiles', cliff='tiles', bridge=[R(AC, 272, 16)],
        overlays=[R(AC, 0, 48), R(AC, 16, 48), R(AC, 0, 64), R(AC, 16, 64), R(AC, 0, 80),
                  R(AC, 16, 80), R(AC, 0, 96), R(AC, 0, 112), R(AC, 16, 112)],
        flower=[R(LC, 352, 336), R(LC, 368, 336), R(LC, 384, 336), R(LC, 400, 336)],
        big=['column48', 'column48', 'column48'], mid=['column48', 'column48'],
        small=['column'], small2=['column'],
        rock=[R(LC, 448, 176, 32, 32)], rock2=[R(LC, 32, 200, 32, 24)],
        tall=['column'], tall2=['column'],
        pebble=[R(LC, 400, 144)], pebble2=[R(LC, 384, 144)], pebble3=[R(LC, 416, 144)]),
}
RING_TILE_TYPES = {'TL': 4, 'T': 3, 'TR': 5, 'L': 14, 'R': 15, 'BL': 17, 'B': 16, 'BR': 18}


_CATALOGS = {}


def catalogs():
    if not _CATALOGS:
        _CATALOGS.update(pipeline.catalog_inventory(ROOT))
    return _CATALOGS


def tile_rects(sheet):
    """TileType -> (sheet, rect) from the biome's curated tilemapper TILE records."""
    rects = {}
    for parts in catalogs()[sheet]['records']:
        if parts[0] in ('TILE', 'GTILE'):
            col, row, sc, sr, t = map(int, parts[1:6])
            rects[t] = (GT if parts[0] == 'GTILE' else sheet, (col * 16, row * 16, sc * 16, sr * 16))
    return rects


def ring_art(palette, kind):
    spec = palette[kind]
    if spec == 'tiles':
        rects = tile_rects(palette['sheet'])
        return {role: rects[t] for role, t in RING_TILE_TYPES.items()}
    sheet, ox, oy = spec
    return {role: (sheet, (ox + dx * 16, oy + dy * 16, 16, 16)) for (dx, dy), role in RING_POS.items()}


def pick(options, key):
    """Deterministic variant choice for one Forest rectangle."""
    return options[(key[0] // 16 * 7 + key[1] // 16 * 13) % len(options)]


def classify(palette):
    """Forest (x, y) -> target (sheet, rect) for every structural reference."""
    table = {}
    for key, role in FLOOR_ROLES.items():
        table[key] = (pick(palette[role], key), role)
    for ox, oy, kind in RINGS:
        art = ring_art(palette, kind)
        for (dx, dy), role in RING_POS.items():
            table[(ox + dx * 16, oy + dy * 16)] = (art[role], f'{kind} ring {role}')
        table[(ox + 16, oy + 16)] = (palette['floor_main'][0], f'{kind} ring centre -> floor')
    for i, key in enumerate(OVERLAYS):
        table[key] = (palette['overlays'][i % len(palette['overlays'])], 'overlay')
    for key in BRIDGE:
        table[key] = (pick(palette['bridge'], key), 'bridge causeway')
    return table


# ---------------------------------------------------------------------------
# Atlas: props/decor fitted to Forest footprints (collision kept)
# ---------------------------------------------------------------------------
def load_sheet(stem, cache={}):
    from PIL import Image
    if stem not in cache:
        cache[stem] = Image.open(ROOT / 'MapTilesets' / (stem + '.png')).convert('RGBA')
    return cache[stem]


def art(spec):
    from PIL import Image
    if spec in ('column', 'column48'):   # castle column: capital over 1-2 shaft pieces
        parts = [(128, 96), (128, 160)] if spec == 'column' else [(128, 96), (128, 144), (128, 160)]
        img = Image.new('RGBA', (16, 16 * len(parts)))
        for i, (cx, cy) in enumerate(parts):
            img.alpha_composite(load_sheet(AC).crop((cx, cy, cx + 16, cy + 16)), (0, i * 16))
        return img
    sheet, (x, y, w, h) = spec
    return load_sheet(sheet).crop((x, y, x + w, y + h))


def fit(specs, width, height):
    """Lay art left to right, bottom-aligned and centred in the slot; shrink
    uniformly (nearest) only when the art cannot fit at native size."""
    from PIL import Image
    pieces = [art(s) for s in specs]
    total = sum(p.width for p in pieces)
    tallest = max(p.height for p in pieces)
    scale = min(1.0, width / total, height / tallest)
    slot = Image.new('RGBA', (width, height))
    x = (width - int(total * scale)) // 2
    for p in pieces:
        if scale < 1.0:
            p = p.resize((max(1, int(p.width * scale)), max(1, int(p.height * scale))), Image.Resampling.NEAREST)
        slot.alpha_composite(p, (x, height - p.height))
        x += p.width
    return slot


def decor_art(palette, ident, width, height):
    from PIL import Image
    slot = Image.new('RGBA', (width, height))
    overlays = palette['overlays']
    seed = sum(ord(c) for c in ident)
    for i, (cx, cy) in enumerate((cx, cy) for cy in range(0, height, 16) for cx in range(0, width, 16)):
        slot.alpha_composite(art(overlays[(seed + i * 3) % len(overlays)]), (cx, cy))
    return slot


def build_atlas(slug, palette, stage):
    from PIL import Image
    stem = slug + '_ForestVariants'
    atlas = Image.new('RGBA', (512, 256))
    records, x, y, row_h = [], 0, 0, 0

    def place(img):
        nonlocal x, y, row_h
        if x + img.width > 512:
            x, y, row_h = 0, y + row_h, 0
        assert y + img.height <= 256, 'atlas full'
        atlas.alpha_composite(img, (x, y))
        spot = (x, y)
        x += img.width
        row_h = max(row_h, img.height)
        return spot

    forest = catalogs()['Forest']['records']
    for rec in forest:
        if rec[0] == 'PROPV3' and rec[1] in PROP_ROLES:
            w, h = int(rec[6]), int(rec[7])
            px, py = place(fit(palette[PROP_ROLES[rec[1]]], w, h))
            records.append(['PROPV3', rec[1], f'{slug} {PROP_ROLES[rec[1]]}', stem,
                            str(px), str(py), str(w), str(h), *rec[8:]])
        elif rec[0] == 'DECORV3' and rec[1] in DECOR_ROLES:
            w, h = int(rec[6]), int(rec[7])
            px, py = place(decor_art(palette, rec[1], w, h))
            records.append(['DECORV3', rec[1], f'{slug} decor', stem, str(px), str(py), str(w), str(h)])
    # Animated Forest flowers -> the biome's flower/flame frames.
    frames = palette['flower']
    rec = ['ANIMDECORV3', 'anim_decor_0', f'{slug} animated decor', '', '0', '4', str(len(frames))]
    for f in frames:
        px, py = place(art(f))
        rec.extend(map(str, (px, py, 16, 16)))
    rec[3] = stem
    records.append(rec)
    # Fitted painted tree (two 16px halves), drawn as a small prop.
    tree = fit(palette['small'], 16, 32)
    tx, ty = place(tree)
    fitted = {0: (tx, ty, 16, 16), 1: (tx, ty + 16, 16, 16)}
    # Lava presentation of the shared water animation (same frame layout).
    if palette['liquid'] == 'lava':
        for source in catalogs()['Village_Animations']['records']:
            if source[0] == 'ANIMDECORV3' and source[1] in WATER_IDS:
                records.append(['ANIMDECORV3', source[1] + '_lava', 'Lava', 'FD_Animated_Lava', *source[4:]])
    stage.mkdir(parents=True, exist_ok=True)
    (stage / 'MapTilesets').mkdir(exist_ok=True)
    atlas.save(stage / 'MapTilesets' / (stem + '.png'))

    def serialize(r):
        return ' '.join(json.dumps(v) if i in (1, 2, 3) else v for i, v in enumerate(r))
    (stage / f'tilemapper_{stem}.txt').write_text('\n'.join(serialize(r) for r in records) + '\n', encoding='utf-8')
    return stem, fitted


# ---------------------------------------------------------------------------
# Room conversion
# ---------------------------------------------------------------------------
def convert(source, slug, palette, table, stem, fitted):
    baseline = json.loads((ROOT / 'docs/biome-content-audit/baseline.json').read_text(encoding='utf-8'))
    expected = next(s['sha256'] for s in baseline['sources']
                    if s['path'] == source.relative_to(ROOT).as_posix())
    if pipeline.sha(source.read_bytes()) != expected:
        raise ValueError('source hash drift: ' + source.name)
    text = source.read_text(encoding='utf-8-sig')
    uses_water = any(w in text for w in WATER_IDS)
    out, missing = [], set()
    for line in text.splitlines():
        p = pipeline.tokens(line) if line.strip() and not line[0].isdigit() else []
        if not p:
            out.append(line)
            continue
        tag = p[0]
        if tag == 'ID':
            line = f'ID "variant_{p[1]}_{slug.lower()}_r1"'
        elif tag == 'NAME':
            line = f'NAME "{p[1]} - {slug}"'
        elif tag == 'BIOME':
            line = f'BIOME {palette["index"]}'
        elif tag == 'TILESET':
            line = f'TILESET "{palette["sheet"]}"'
        elif tag == 'FALLSURFACE' and palette['liquid'] == 'lava' and uses_water:
            line = 'FALLSURFACE 2'
        elif tag in pipeline.VISUAL:
            key = (int(p[3]), int(p[4]))
            if p[1] == 'Forest' and key in FITTED_TREE:
                rect = fitted[FITTED_TREE[key]]
                line = f'{tag} "{stem}" {p[2]} ' + ' '.join(map(str, rect)) + ' ' + ' '.join(p[7:])
            elif p[1] == 'Forest' and key in table:
                (sheet, rect), _ = table[key]
                line = f'{tag} "{sheet}" {p[2]} ' + ' '.join(map(str, rect)) + ' ' + ' '.join(p[7:])
            else:
                missing.add(pipeline.ref_key(p[:7]))
        elif tag in pipeline.PLACEMENTS:
            if p[2] == 'Forest' and (p[1] in PROP_ROLES or p[1] in DECOR_ROLES or p[1] == 'anim_decor_0'):
                line = f'{tag} "{p[1]}" "{stem}" ' + ' '.join(p[3:])
            elif p[2] == 'Village_Animations' and p[1] in WATER_IDS:
                if palette['liquid'] == 'lava':
                    line = f'{tag} "{p[1]}_lava" "{stem}" ' + ' '.join(p[3:])
            else:
                missing.add(pipeline.ref_key(p[:3]))
        out.append(line)
    return ('\n'.join(out) + '\n', sorted(missing))


def generate(slug):
    palette = PALETTES[slug]
    stage = OUT / slug
    table = classify(palette)
    stem, fitted = build_atlas(slug, palette, stage)
    manifest_path = stage / 'manifest.json'
    prior = json.loads(manifest_path.read_text()) if manifest_path.exists() else {}
    manifest = {}
    for source in sorted(FOREST_DIR.glob('*.mroom')):
        data, missing = convert(source, slug, palette, table, stem, fitted)
        row = {'source_sha256': pipeline.sha(source.read_bytes()), 'missing': missing,
               'status': 'unmapped' if missing else 'staged_unvalidated'}
        if not missing:
            path = stage / 'Rooms' / f'forest_{source.stem}_{slug.lower()}.mroom'
            if path.exists() and prior.get(source.stem, {}).get('output_sha256') != pipeline.sha(path.read_bytes()):
                raise ValueError('refusing to overwrite edited output: ' + str(path))
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(data.encode())
            row['output_sha256'] = pipeline.sha(data.encode())
        manifest[source.stem] = row
    manifest_path.write_text(json.dumps(manifest, indent=2) + '\n')
    profile = {'target': palette['sheet'], 'permitted_sheets': palette['permitted'] + [stem, 'FD_Animated_Water', 'FD_Animated_Lava'],
               'liquid': palette['liquid'],
               'structural': {f'{k[0]},{k[1]}': {'target': v[0], 'role': v[1]} for k, v in sorted(table.items())}}
    (stage / 'mapping-profile.json').write_text(json.dumps(profile, indent=2) + '\n')
    bad = {k: v['missing'] for k, v in manifest.items() if v['missing']}
    print(f'{slug}: {len(manifest) - len(bad)}/{len(manifest)} staged' + (f'; unmapped {bad}' if bad else ''))


if __name__ == '__main__':
    for slug in (sys.argv[1:] or PALETTES):
        generate(slug)
