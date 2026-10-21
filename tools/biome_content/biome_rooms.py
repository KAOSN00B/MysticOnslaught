"""Biome-specific rooms: new layouts (not Forest geometry) in each biome's art.

The seven biomes that so far had only Forest-family reskins each get one room
with its own purpose (cover lanes, a ring arena, lava or water pits ...).

Everything reused is already validated in the game:
  * walls, door frames and door zones come from that biome's own four-door
    room (forest_4wayroom_<biome>), so wall art, sealing and door collision are
    the shipped ones; a design only switches door zones on or off;
  * the interior floor is that room's own floor tile;
  * props are that biome's existing prop records (<Biome>_ForestVariants),
    each with its recorded collision box;
  * pits are the 9-tile pool module the reskins already use (water, or lava
    in Demon's Insides), with fall cells only under the pool's middle.

Each room is checked before it is written: inside the room, no overlapping
collision, door lanes and the event-shrine centre left open, every enabled
door reachable from every other. A failing design is an error, never a file.

    python tools/biome_content/biome_rooms.py            # write rooms + previews
    python tools/biome_content/biome_rooms.py --check    # validate only
"""
import collections
import shlex
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
ROOMS = ROOT / 'TestGame/Rooms/Generated'
PREVIEWS = ROOT / 'docs/biome-content-audit/biome-rooms'
COLS, ROWS = 28, 16

# Prop ids in every <Biome>_ForestVariants sheet (same footprint everywhere).
BIG, MID, SMALL, SMALL2, ROCK, ROCK2 = 'prop_0', 'prop_1', 'prop_2', 'prop_3', 'prop_4', 'prop_5'
TALL, TALL2 = 'prop_168711230338000_18', 'prop_168713593038100_19'
PEBBLE, PEBBLE2, PEBBLE3, PEBBLE4 = ('prop_173392298448400_1', 'prop_173393896656300_2',
                                     'prop_173395190463900_3', 'prop_173396521756000_4')
# Pool module roles -> animated tile ids (the reskins' pond tiles).
POOL = {'TL': '169099280120400_20', 'T': '169117514979800_21', 'TR': '169126202018300_22',
        'L': '169135452198400_23', 'BL': '169144723228100_24', 'B': '169155040957600_25',
        'BR': '169166379563000_26', 'R': '169173133546700_27', 'C': '169184488769900_28'}

N, S, E, W = 1, 2, 4, 8
# Cells kept clear: a lane inside each enabled door, and the room centre where
# the event shrine, Zeph's camp and the crossroads expect open floor.
LANES = {N: (11, 1, 7, 4), S: (11, 11, 7, 4), W: (1, 6, 4, 5), E: (23, 6, 4, 5)}
CENTRE = (12, 5, 5, 4)

DESIGNS = [
    dict(biome='LostCity', index=7, design='colonnade', name='Colonnade', doors=N | S, patch=(10, 1, 8, 14),
         purpose='Two pillar rows split the hall into a central aisle and two flanks: cover against ranged enemies.',
         props=[(TALL, c, r) for c in (8, 19) for r in (2, 5, 8, 11)] +
               [(ROCK, 2, 2), (ROCK2, 23, 12), (PEBBLE, 4, 8), (PEBBLE2, 23, 6), (PEBBLE3, 3, 13), (PEBBLE4, 24, 2)]),
    dict(biome='AncientCastle', index=0, design='pillar_hall', name='Pillar Hall', doors=E | W, patch=(1, 5, 26, 6),
         purpose='An east-west hall between two rows of double columns: a long lane with pillars to break line of fire.',
         props=[(MID, c, 1) for c in (5, 10, 16, 21)] + [(MID, c, 11) for c in (5, 10, 16, 21)] +
               [(TALL, 13, 1), (TALL, 14, 1), (PEBBLE, 2, 2), (PEBBLE2, 25, 13), (ROCK2, 1, 12)]),
    dict(biome='TheSanctuary', index=8, design='basin_court', name='Basin Court', doors=N | S | E | W, patch=(10, 5, 8, 6),
         purpose='Four basins and four braziers ring an open court: an arena you circle rather than cross.',
         props=[(ROCK, 6, 3), (ROCK2, 20, 3), (ROCK2, 6, 11), (ROCK, 20, 11),
                (TALL, 9, 4), (TALL2, 18, 4), (TALL2, 9, 10), (TALL, 18, 10),
                (PEBBLE, 1, 1), (PEBBLE2, 26, 1), (PEBBLE3, 1, 14), (PEBBLE4, 26, 14)]),
    dict(biome='Wastelands', index=9, design='rubble_field', name='Rubble Field', doors=N | S | E | W,
         purpose='Scattered rubble and a dead tree: uneven, broken cover with no clean lines.',
         props=[(ROCK, 4, 2), (SMALL, 8, 5), (ROCK2, 5, 11), (MID, 9, 11), (MID, 18, 3), (ROCK, 21, 8),
                (ROCK2, 22, 12), (SMALL2, 19, 10), (BIG, 21, 1), (PEBBLE, 11, 9), (PEBBLE3, 2, 4), (PEBBLE2, 19, 13)]),
    dict(biome='DreamRealm', index=3, design='statue_circle', name='Statue Circle', doors=N | S | E | W, patch=(10, 5, 9, 6),
         purpose='Six gargoyles stand in a ring around a clearing, lamps at the corners: a ring arena with pillars to dance around.',
         props=[(TALL2, 9, 3), (TALL2, 19, 3), (TALL2, 6, 7), (TALL2, 22, 7), (TALL2, 9, 11), (TALL2, 19, 11),
                (TALL, 3, 2), (TALL, 24, 2), (TALL, 3, 12), (TALL, 24, 12), (PEBBLE, 6, 2), (PEBBLE4, 21, 13)]),
    dict(biome='DemonsInsides', index=2, design='lava_pools', name='Lava Pools', doors=N | S | E | W, liquid='lava',
         purpose='Four lava pools fill the corners and leave a cross of solid ground: fights happen on the cross, at the edge of the lava.',
         pools=[(3, 2, 6, 4), (19, 2, 6, 4), (3, 11, 6, 4), (19, 11, 6, 4)],
         props=[(TALL, 10, 4), (TALL2, 18, 4), (TALL2, 10, 9), (TALL, 18, 9)]),
    dict(biome='Jungle', index=6, design='sunken_pools', name='Sunken Pools', doors=E | W, liquid='water', patch=(1, 5, 26, 6),
         purpose='Two long sunken pools hem an east-west path: the fight is squeezed onto the path and its banks.',
         pools=[(6, 1, 16, 4), (6, 11, 16, 4)],
         props=[(SMALL, 2, 2), (SMALL2, 25, 2), (SMALL2, 2, 11), (SMALL, 25, 11), (PEBBLE, 9, 6), (PEBBLE3, 18, 9)]),
]


def prop_records(biome):
    stem = biome + '_ForestVariants'
    records = {}
    for line in (ROOT / 'TestGame' / f'tilemapper_{stem}.txt').read_text(encoding='utf-8').splitlines():
        p = shlex.split(line)
        if p and p[0] == 'PROPV3':
            w, h, cx, cy, cw, ch = map(int, p[6:12])
            records[p[1]] = (w, h, (cx, cy, cw, ch))
    return stem, records


def cells_of(col, row, rect):
    """Cells a pixel rectangle (relative to the prop's top-left cell) touches."""
    x, y, w, h = rect
    if w <= 0 or h <= 0:
        return set()
    return {(col + c, row + r) for c in range(x // 16, (x + w - 1) // 16 + 1)
            for r in range(y // 16, (y + h - 1) // 16 + 1)}


def rect_cells(rect):
    c, r, w, h = rect
    return {(c + i, r + j) for i in range(w) for j in range(h)}


def pool_tiles(pool):
    c0, r0, w, h = pool
    tiles = {}
    for c in range(c0, c0 + w):
        for r in range(r0, r0 + h):
            top, bottom, left, right = r == r0, r == r0 + h - 1, c == c0, c == c0 + w - 1
            role = ('TL' if top and left else 'TR' if top and right else 'BL' if bottom and left else
                    'BR' if bottom and right else 'T' if top else 'B' if bottom else 'L' if left else
                    'R' if right else 'C')
            tiles[(c, r)] = role
    return tiles


def check(design, records):
    """Raise ValueError when the design breaks a placement rule."""
    name = f"{design['biome']} {design['name']}"
    interior = {(c, r) for c in range(1, COLS - 1) for r in range(1, ROWS - 1)}
    keep_open = rect_cells(CENTRE)
    for side, lane in LANES.items():
        if design['doors'] & side:
            keep_open |= rect_cells(lane)
    blocked, fall, pool_cells = set(), set(), set()
    for pool in design.get('pools', []):
        tiles = pool_tiles(pool)
        if pool[2] < 3 or pool[3] < 3 or not set(tiles) <= interior:
            raise ValueError(f'{name}: pool {pool} is too small or outside the room')
        if set(tiles) & pool_cells:
            raise ValueError(f'{name}: pools {pool} overlap')
        pool_cells |= set(tiles)
        fall |= {cell for cell, role in tiles.items() if role == 'C'}
    if pool_cells & keep_open:
        raise ValueError(f'{name}: a pool covers a door lane or the centre: {sorted(pool_cells & keep_open)}')
    for prop, col, row in design['props']:
        if prop not in records:
            raise ValueError(f'{name}: {design["biome"]} has no prop {prop}')
        w, h, coll = records[prop]
        sprite = cells_of(col, row, (0, 0, w, h))
        solid = cells_of(col, row, coll)
        if not sprite <= interior:
            raise ValueError(f'{name}: {prop} at ({col},{row}) leaves the room')
        if solid & (blocked | pool_cells):
            raise ValueError(f'{name}: {prop} at ({col},{row}) collides with another prop or a pool')
        if solid & keep_open:
            raise ValueError(f'{name}: {prop} at ({col},{row}) blocks a door lane or the centre')
        blocked |= solid
    # Every enabled door must reach every other (and the centre) on foot.
    walk = interior - blocked - fall
    start = (CENTRE[0], CENTRE[1])
    seen, queue = {start}, collections.deque([start])
    while queue:
        c, r = queue.popleft()
        for nxt in ((c + 1, r), (c - 1, r), (c, r + 1), (c, r - 1)):
            if nxt in walk and nxt not in seen:
                seen.add(nxt)
                queue.append(nxt)
    for side, lane in LANES.items():
        if design['doors'] & side and not rect_cells(lane) <= seen:
            raise ValueError(f'{name}: the door lane on side {side} is cut off')
    return blocked, fall, len(walk), len(seen)


def build(design):
    biome = design['biome']
    stem, records = prop_records(biome)
    blocked, fall, walkable, reached = check(design, records)
    base_path = ROOMS / biome / 'forest_variants' / f'forest_4wayroom_{biome.lower()}.mroom'
    lines = base_path.read_text(encoding='utf-8-sig').splitlines()
    section, tiles, solid, visuals, doorzones, header = None, [], [], [], [], {}
    for line in lines:
        if line.endswith('_BEGIN'):
            section = line[:-6]
            continue
        if line.endswith('_END'):
            section = None
            continue
        if section == 'TILES':
            tiles.append(line.split())
        elif section == 'SOLID':
            solid.append(list(line))
        elif section == 'VISUALS':
            visuals.append(line)
        elif section is None and line.startswith('DOORZONE'):
            doorzones.append(line.split())
        elif section is None:
            tag = line.split(' ', 1)[0]
            header[tag] = line
    sheet = shlex.split(header['TILESET'])[1]

    # Floor, from the base room's own interior: its ground tile everywhere, its
    # centre-patch tile where the design wants a court / aisle / path, and its
    # small floor decals where they do not end up under a prop or a pool.
    def cell_of(visual):
        p = shlex.split(visual)
        return int(p[7]), int(p[8]), p
    def is_interior(c, r):
        return 0 < c < COLS - 1 and 0 < r < ROWS - 1
    def record(kind, tile, c, r):
        return f'{kind} "{tile[0]}" {tile[1]} {tile[2]} {tile[3]} {tile[4]} {tile[5]} {c} {r} 0 0'
    ground_tiles, overlay_tiles = collections.Counter(), collections.Counter()
    decals = []
    for v in visuals:
        c, r, p = cell_of(v)
        if is_interior(c, r):
            (ground_tiles if p[0] == 'GROUND' else overlay_tiles)[tuple(p[1:7])] += 1
            if p[0] == 'VISUAL':
                decals.append((tuple(p[1:7]), c, r))
    floor = ground_tiles.most_common(1)[0][0]
    patch_tile = overlay_tiles.most_common(1)[0][0]
    covered = set()
    for prop, col, row in design['props']:
        w, h, _ = records[prop]
        covered |= cells_of(col, row, (0, 0, w, h))
    for pool in design.get('pools', []):
        covered |= set(pool_tiles(pool))
    patch = rect_cells(design['patch']) - covered if 'patch' in design else set()
    border = [v for v in visuals if not is_interior(*cell_of(v)[:2])]
    ground = [record('GROUND', floor, c, r) for r in range(1, ROWS - 1) for c in range(1, COLS - 1)]
    overlays = [record('VISUAL', patch_tile, c, r) for c, r in sorted(patch, key=lambda cr: (cr[1], cr[0]))]
    overlays += [record('VISUAL', tile, c, r) for tile, c, r in decals
                 if tile != patch_tile and (c, r) not in covered and (c, r) not in patch]

    liquid = design.get('liquid')
    placements = []
    for pool in design.get('pools', []):
        for (c, r), role in sorted(pool_tiles(pool).items()):
            ident = f'anim_decor_{POOL[role]}' + ('_lava' if liquid == 'lava' else '')
            placements.append(f'ANIMDECOR "{ident}" "{stem}" {c} {r} 0')
    for prop, col, row in design['props']:
        placements.append(f'PROP "{prop}" "{stem}" {col} {row} 0')

    door_flags = [design['doors'] & N, design['doors'] & S, design['doors'] & E, design['doors'] & W]
    out = ['MROOM 7',
           f'ID "biome_{biome.lower()}_{design["design"]}_r1"',
           f'NAME "{design["name"]} - {biome}"',
           f'BIOME {design["index"]}',
           header['TILESET'],
           'ROOMTYPE 0',
           f'FALLSURFACE {2 if liquid == "lava" else 0}',
           'DOORS ' + ' '.join('1' if f else '0' for f in door_flags),
           header['WALL_DEPTHS'],
           'TILES_BEGIN']
    for r in range(ROWS):
        out.append(' '.join(tiles[r][c] if (r in (0, ROWS - 1) or c in (0, COLS - 1)) else '0' for c in range(COLS)))
    out += ['TILES_END', 'PLACEMENTS_BEGIN', *placements, 'PLACEMENTS_END', 'FALL_BEGIN']
    for r in range(ROWS):
        out.append(''.join('1' if (c, r) in fall else '0' for c in range(COLS)))
    out += ['FALL_END', 'SOLID_BEGIN']
    for r in range(ROWS):
        out.append(''.join(solid[r][c] if (r in (0, ROWS - 1) or c in (0, COLS - 1)) else '0' for c in range(COLS)))
    out += ['SOLID_END', 'VISUALS_BEGIN', *border, *ground, *overlays, 'VISUALS_END']
    order = {'0': N, '1': S, '2': W, '3': E}   # DOORZONE index (top, bottom, left, right) -> door bit
    for zone in doorzones:
        zone[2] = '1' if design['doors'] & order[zone[1]] else '0'
        out.append(' '.join(zone))
    target = ROOMS / biome / 'biome_rooms' / f'{design["design"]}.mroom'
    return target, '\n'.join(out) + '\n', walkable, reached, len(fall), sheet


def main():
    check_only = '--check' in sys.argv
    written = []
    for design in DESIGNS:
        target, text, walkable, reached, pits, sheet = build(design)
        print(f'{design["biome"]:14} {design["name"]:14} doors={design["doors"]:2} walkable={walkable} '
              f'reached={reached} pits={pits}  -> {target.relative_to(ROOT)}')
        if not check_only:
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_text(text, encoding='utf-8', newline='\n')
            written.append(target)
    if written:
        PREVIEWS.mkdir(parents=True, exist_ok=True)
        subprocess.run([sys.executable, str(Path(__file__).with_name('render_room.py')),
                        str(PREVIEWS / 'biome_rooms_contact.png'), *map(str, written)], check=True)
        for target in written:
            subprocess.run([sys.executable, str(Path(__file__).with_name('render_room.py')),
                            str(PREVIEWS / (target.parent.parent.name + '_' + target.stem + '.png')), str(target)],
                           check=True)


if __name__ == '__main__':
    main()
