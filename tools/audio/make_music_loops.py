"""Build seamless OGG music loops from Ovani "Main" WAVs.

Ovani tracks end with a reverb tail (the "RT" seconds in each folder name).
Looping the raw file leaves the tail ringing into silence before the restart.
Here the tail is mixed back over the start of the loop (overlap-add), so the
loop point is musically continuous, then the result is encoded as OGG Vorbis.

Requires: soundfile + numpy (pip install --target <dir> soundfile numpy, then
PYTHONPATH=<dir>). Source packs are never modified.

    python tools/audio/make_music_loops.py
"""
import hashlib
import json
from pathlib import Path

import numpy as np
import soundfile as sf

ROOT = Path(__file__).resolve().parents[2]
PACKS = Path(r"C:\Users\rober\OneDrive\Desktop\GameDev\Music And Sounds")
FANTASY = PACKS / "Fantasy Music Pack Vol. 7" / "Fantasy Music Pack Vol. 7"

# (folder in the pack, reverb tail seconds, output file under Music/, role)
TRACKS = [
    ("Mystwood Reverie (RT 4.186)", 4.186, "Forest_MystwoodReverie.ogg", "Forest"),
    ("Feysong Fields (RT 3.857)", 3.857, "Jungle_FeysongFields.ogg", "Jungle"),
    ("Tomb of Kings (RT 5.625)", 5.625, "LostCity_TombOfKings.ogg", "Lost City"),
    ("Enemy Grounds (RT 3.396)", 3.396, "Wastelands_EnemyGrounds.ogg", "Wastelands"),
    ("Black Doves (RT 2.723)", 2.723, "Graveyard_BlackDoves.ogg", "Graveyard"),
    ("Above (RT 4.184)", 4.184, "Sanctuary_Above.ogg", "The Sanctuary"),
    ("Denmark (RT 3.115)", 3.115, "Village_Denmark.ogg", "Village"),
    ("Stormborn Destiny (RT 6.4)", 6.4, "WorldMap_StormbornDestiny.ogg", "World map"),
    ("Elder Awakening (RT 2.769)", 2.769, "FinalBoss_ElderAwakening.ogg", "Final domain boss"),
]


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def is_failed_leftover(path):
    """True for a zero-frame/unreadable file left by an encoder crash."""
    try:
        return sf.info(str(path)).frames == 0
    except Exception:
        return True


def write_ogg(path, data, rate):
    # libsndfile's Vorbis encoder crashes on very large single writes, so the
    # loop is streamed in blocks into a temp file and renamed on success.
    temp = path.with_suffix('.ogg.tmp')
    with sf.SoundFile(str(temp), 'w', rate, data.shape[1], format='OGG', subtype='VORBIS',
                      compression_level=0.35) as f:
        for start in range(0, data.shape[0], 4096):
            f.write(data[start:start + 4096])
    temp.replace(path)


def main():
    out_dir = ROOT / "Music"
    records = []
    for folder, tail_seconds, out_name, role in TRACKS:
        title = folder.split(" (RT")[0]
        source = FANTASY / folder / f"Fantasy Vol7 {title} Main.wav"
        data, rate = sf.read(str(source), dtype="float32", always_2d=True)
        tail = int(round(tail_seconds * rate))
        loop_len = data.shape[0] - tail
        loop = data[:loop_len].copy()
        loop[:tail] += data[loop_len:]          # fold the reverb tail over the loop start
        peak = float(np.max(np.abs(loop)))
        if peak > 0.999:
            loop *= 0.999 / peak               # never clip after the fold
        target = out_dir / out_name
        if target.exists() and not is_failed_leftover(target):
            raise SystemExit(f"refusing to overwrite {target}")
        write_ogg(target, loop, rate)
        check, check_rate = sf.read(str(target), dtype="float32", always_2d=True)
        assert check_rate == rate and abs(check.shape[0] - loop_len) <= rate // 10, out_name
        records.append({"role": role, "output": f"Music/{out_name}", "source": str(source.relative_to(PACKS)),
                        "source_sha256": sha(source), "reverb_tail_seconds": tail_seconds,
                        "loop_seconds": round(loop_len / rate, 3), "output_sha256": sha(target),
                        "output_bytes": target.stat().st_size,
                        "license": "Ovani Sound royalty-free (commercial use allowed; credit appreciated, "
                                   "not required; no standalone redistribution; no AI training)"})
        print(f"{role:18s} {out_name:34s} {loop_len / rate:6.1f}s  {target.stat().st_size / 1e6:5.1f} MB")
    manifest = out_dir / "music_manifest.json"
    manifest.write_text(json.dumps({"tracks": records}, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
