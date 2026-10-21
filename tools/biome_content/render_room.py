"""Preview renderer for .mroom files at 16 source px per cell (game proportions).

usage: python render_room.py out.png room1.mroom [room2.mroom ...]   -> contact sheet
Draw order follows TileRenderer: Ground visuals, ground-band decor, Visual
visuals + Door visuals, visual-band decor, y-sorted decor, y-sorted props.
Doors are drawn closed. Solid cells get a faint red dot, fall cells blue.
"""
import shlex
import sys
from pathlib import Path
from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[2]   # the project root (tools/biome_content/ -> ..)
SHEETS, CATALOGS = {}, {}


import os
EXTRA = [Path(d) for d in os.environ.get("MO_EXTRA_ART", "").split(";") if d]


def sheet(stem):
    if stem not in SHEETS:
        path = ROOT / "MapTilesets" / (stem + ".png")
        for d in EXTRA:
            if (d / "MapTilesets" / (stem + ".png")).exists():
                path = d / "MapTilesets" / (stem + ".png")
        SHEETS[stem] = Image.open(path).convert("RGBA") if path.exists() else None
    return SHEETS[stem]


def catalog(stem):
    if stem in CATALOGS:
        return CATALOGS[stem]
    defs = {"prop": {}, "decor": {}, "anim_prop": {}, "anim_decor": {}}
    path = ROOT / "TestGame" / f"tilemapper_{stem}.txt"
    for d in EXTRA:
        if (d / f"tilemapper_{stem}.txt").exists():
            path = d / f"tilemapper_{stem}.txt"
    counters = {k: 0 for k in defs}
    if path.exists():
        for line in path.read_text(encoding="utf-8-sig").splitlines():
            p = shlex.split(line) if line.strip() else []
            if not p:
                continue
            tag = p[0]
            base = tag.rstrip("23").rstrip("V") if tag.endswith(("V2", "V3")) else tag
            kind = {"PROP": "prop", "DECOR": "decor", "ANIMPROP": "anim_prop", "ANIMDECOR": "anim_decor"}.get(base)
            if not kind:
                continue
            v3, v2 = tag.endswith("V3"), tag.endswith("V2")
            i = 1
            if v2 or v3:
                ident = p[1]; i = 3
            else:
                ident = f"{kind}_{counters[kind]}"
            counters[kind] += 1
            src_sheet = stem
            if v3:
                src_sheet = p[i] or stem; i += 1
            if kind in ("prop", "decor"):
                x, y, w, h = map(float, p[i:i + 4])
                defs[kind][ident] = (src_sheet, (x, y, w, h))
            else:
                if v2 or v3:
                    i += 2  # playback, fps
                    if kind == "anim_prop":
                        i += 4  # collision
                else:
                    i += 4 if kind == "anim_prop" else 0
                    i += 1  # fps
                n = int(p[i]); i += 1
                frames = [tuple(map(float, p[i + 4 * f:i + 4 * f + 4])) for f in range(n)]
                defs[kind][ident] = (src_sheet, frames[0] if frames else (0, 0, 16, 16))
    CATALOGS[stem] = defs
    return defs


def crop(stem, rect):
    img = sheet(stem)
    if img is None:
        return None
    x, y, w, h = map(int, rect)
    return img.crop((x, y, x + w, y + h))


def parse(path):
    room = {"visuals": [], "placements": [], "solid": [], "fall": [], "fields": {}}
    lines = Path(path).read_text(encoding="utf-8-sig").splitlines()
    mode = None
    for line in lines:
        if line in ("SOLID_BEGIN", "FALL_BEGIN", "VISUALS_BEGIN", "PLACEMENTS_BEGIN", "TILES_BEGIN"):
            mode = line.split("_")[0]; continue
        if line.endswith("_END"):
            mode = None; continue
        if mode == "SOLID":
            room["solid"].append(line); continue
        if mode == "FALL":
            room["fall"].append(line); continue
        if mode == "TILES":
            continue
        p = shlex.split(line) if line.strip() else []
        if not p:
            continue
        if mode == "VISUALS":
            tag, stem = p[0], p[1]
            x, y, w, h, col, row = map(float, p[3:9])
            ax, ay = (float(p[9]), float(p[10])) if len(p) >= 11 else (0.0, 0.0)
            room["visuals"].append((tag, stem, (x, y, w, h), col, row, ax, ay))
        elif mode == "PLACEMENTS":
            band = int(p[5]) if len(p) > 5 else 0
            room["placements"].append((p[0], p[1], p[2], int(p[3]), int(p[4]), band))
        else:
            room["fields"][p[0]] = p[1:]
    return room


def render(path, overlay=True):
    room = parse(path)
    canvas = Image.new("RGBA", (28 * 16, 16 * 16), (10, 8, 12, 255))
    missing = []

    def paste(img, x, y):
        if img is not None:
            canvas.alpha_composite(img, (int(round(x)), int(round(y))))

    def visual(tag_filter):
        for tag, stem, rect, col, row, ax, ay in room["visuals"]:
            if tag in tag_filter:
                img = crop(stem, rect)
                if img is None:
                    missing.append(stem)
                paste(img, col * 16 - ax, row * 16 - ay)

    def placement_img(p):
        kind = {"PROP": "prop", "DECOR": "decor", "ANIMPROP": "anim_prop", "ANIMDECOR": "anim_decor"}[p[0]]
        entry = catalog(p[2])[kind].get(p[1])
        if entry is None:
            missing.append(f"{p[2]}:{p[1]}")
            return None, 16
        return crop(entry[0], entry[1]), entry[1][3]

    visual({"GROUND"})
    for p in room["placements"]:
        if p[0] in ("DECOR", "ANIMDECOR") and p[5] == 1:
            img, _ = placement_img(p); paste(img, p[3] * 16, p[4] * 16)
    visual({"VISUAL", "DOOR"})
    for p in room["placements"]:
        if p[0] in ("DECOR", "ANIMDECOR") and p[5] == 2:
            img, _ = placement_img(p); paste(img, p[3] * 16, p[4] * 16)
    for kinds in (("DECOR", "ANIMDECOR"), ("PROP", "ANIMPROP")):
        items = []
        for p in room["placements"]:
            if p[0] in kinds and (p[0] in ("PROP", "ANIMPROP") or p[5] == 0):
                img, h = placement_img(p)
                items.append((p[4] * 16 + h, img, p))
        for _, img, p in sorted(items, key=lambda t: t[0]):
            paste(img, p[3] * 16, p[4] * 16)
    if overlay:
        d = ImageDraw.Draw(canvas)
        for r, line in enumerate(room["fall"]):
            for c, ch in enumerate(line):
                if ch == "1":
                    d.rectangle([c * 16 + 6, r * 16 + 6, c * 16 + 9, r * 16 + 9], fill=(60, 140, 255, 255))
        for r, line in enumerate(room["solid"]):
            for c, ch in enumerate(line):
                if ch == "1":
                    d.rectangle([c * 16 + 7, r * 16 + 7, c * 16 + 8, r * 16 + 8], fill=(255, 40, 40, 200))
    return canvas, sorted(set(missing))


def contact(out, paths, overlay=True, cols=3, scale=2):
    tiles = []
    for p in paths:
        img, missing = render(p, overlay)
        label = Path(p).stem + ("  MISSING: " + ", ".join(missing[:3]) if missing else "")
        tiles.append((img.resize((img.width * scale, img.height * scale), Image.NEAREST), label))
    w, h = 28 * 16 * scale, 16 * 16 * scale + 18
    rows = (len(tiles) + cols - 1) // cols
    sheet_img = Image.new("RGB", (cols * (w + 8), rows * (h + 8)), (30, 30, 34))
    d = ImageDraw.Draw(sheet_img)
    for i, (img, label) in enumerate(tiles):
        x, y = (i % cols) * (w + 8), (i // cols) * (h + 8)
        d.text((x + 4, y + 2), label, fill=(255, 255, 0) if "MISSING" in label else (255, 255, 255))
        sheet_img.paste(img.convert("RGB"), (x, y + 18))
    sheet_img.save(out)
    print(out, sheet_img.size)


if __name__ == "__main__":
    contact(sys.argv[1], sys.argv[2:])
