"""Import replacement SFX from packs with a verified license.

Game SFX Treasury v1.1 has no license file, so its clips are not shipped;
these Ovani Sound replacements (royalty-free: commercial use allowed, credit
appreciated) take their place. Copies are byte-identical (hash-checked) and
recorded in Sounds/MysticExpansion/manifest.json. Never overwrites.

    python tools/audio/import_licensed_clips.py
"""
import hashlib
import json
import shutil
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
PACKS = Path(r"C:\Users\rober\OneDrive\Desktop\GameDev\Music And Sounds")
INV = Path("Inventory Sound FX Pack Vol. 2") / "Inventory Sound FX Pack Vol. 2"
MED = Path("Medieval Fantasy Sound FX Pack Vol. 4") / "Medieval Sound FX Pack Vol. 4"
GOTHIC = Path("Gothic Fantasy Sound FX Pack Vol. 1") / "Gothic Fantasy Sound FX Pack Vol. 1"
OVANI = ("Ovani Sound royalty-free (commercial use allowed; credit appreciated, not required; "
         "no standalone redistribution; no AI training)")

CLIPS = [
    (INV / "Item Types" / "Item Accessory A.wav", "Licensed/Coin_Accessory_A.wav", "Coin pickup"),
    (INV / "Item Types" / "Item Accessory B.wav", "Licensed/Coin_Accessory_B.wav", "Coin pickup"),
    (INV / "Player Cues" / "Encumbered Cue A.wav", "Licensed/Denied_Cue_A.wav", "Refused action / back"),
    (INV / "Open & Close" / "Open Whoosh A.wav", "Licensed/Dash_Whoosh_A.wav", "Player dash"),
    (INV / "Open & Close" / "Open Whoosh B.wav", "Licensed/Dash_Whoosh_B.wav", "Player dash"),
    # Kill crunch: immediate transients (peak within 35 ms) layered under creature deaths.
    (GOTHIC / "Combat" / "Medieval Stomp 002.wav", "Licensed/Kill_Stomp_A.wav", "Kill crunch layer"),
    (GOTHIC / "Combat" / "Medieval Stomp 003.wav", "Licensed/Kill_Stomp_B.wav", "Kill crunch layer"),
    (GOTHIC / "Combat" / "Medieval Stomp 004.wav", "Licensed/Kill_Stomp_C.wav", "Kill crunch layer"),
    (GOTHIC / "Combat" / "Execution 001.wav", "Licensed/Kill_Execution_A.wav", "Elite kill flourish"),
]


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def find_heal_clips():
    found = sorted(p for p in (PACKS / MED).rglob("Infantry Upgrade Health *.wav") if "__MACOSX" not in p.parts)
    return [(found[0].relative_to(PACKS), "Licensed/Heal_Upgrade_A.wav", "Heal pickup / heal ability"),
            (found[1].relative_to(PACKS), "Licensed/Heal_Upgrade_B.wav", "Heal pickup / heal ability")]


def main():
    library = ROOT / "Sounds" / "MysticExpansion"
    manifest_path = library / "manifest.json"
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    known = {r["destination"] for r in manifest["records"]}
    added = 0
    for relative, dest, use in CLIPS + find_heal_clips():
        source = PACKS / relative
        target = library / dest
        destination = "Sounds/MysticExpansion/" + dest
        if target.exists():
            if sha(target) != sha(source):
                raise SystemExit(f"refusing to overwrite a different file: {target}")
        else:
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(source, target)
        assert sha(target) == sha(source)
        if destination not in known:
            manifest["records"].append({"source": str(relative), "destination": destination,
                                        "category": "Licensed", "kind": "sfx", "suggested_use": use,
                                        "sha256": sha(target), "license": OVANI})
            added += 1
    note = ("Game SFX Treasury v1.1 ships no license file; its clips are kept for reference but not "
            "referenced by the game. Licensed replacements live in Licensed/.")
    notes = manifest.setdefault("notes", [])
    manifest["notes"] = [n for i, n in enumerate(notes) if n not in notes[:i]]   # drop repeats
    if note not in manifest["notes"]:
        manifest["notes"].append(note)
    manifest_path.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    print(f"imported {added} licensed clip(s)")


if __name__ == "__main__":
    main()
