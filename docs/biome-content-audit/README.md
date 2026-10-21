# Authored room biome coverage audit

**Historical baseline report.** September 24 update: 23 Forest-layout Jungle variants are now installed and native-validated. See `jungle-pool/promotion.json` and [current release status](../RELEASE_CANDIDATE_STATUS.md). All 55 originals are preserved; 472 of the requested cross-biome pairs remain unconverted. The counts below describe the original inventory.

55 preserved source identities; 550 required pairs.
55 original entries, 0 validated generated variants, 495 blocked pairs.

Original entries are coverage identities, not assertions that baseline content passed runtime or visual QA.
No .mroom files were created or edited. No cross-biome mapping is approved.

| Biome | Source rooms | Tile records | Prop/decor records |
|---|---:|---:|---:|
| Ancient Castle | 0 | 13 | 0 |
| Caverns | 23 | 19 | 25 |
| Demons Insides | 0 | 10 | 17 |
| Dream Realm | 0 | 10 | 13 |
| Forest | 23 | 10 | 22 |
| Graveyard | 9 | 8 | 25 |
| Jungle | 0 | 9 | 24 |
| Lost City | 0 | 10 | 16 |
| The Sanctuary | 0 | 10 | 3 |
| Wastelands | 0 | 8 | 25 |

## Baseline reference issues

None detected by the limited sheet/ID/rectangle audit.

## Next required work

1. Use mapping-worklist.json to inspect every distinct reference against the target sheet. Record visual role, orientation, footprint, anchors and explicit replacements; do not copy coordinates or match numeric IDs blindly.
2. Fill catalog gaps using existing art. Retain all props with equivalent effective collision, or author target-only multi-piece replacements.
3. Implement gated .mroom serialization and runtime BuildRoomLayout comparison, then prove representative source rooms including doors, falls, colliders and special roles.
4. Generate all 495 missing variants into a dedicated Generated subtree with hash provenance and edited-output protection.
5. Review every variant visually; verify geometry, door traversal, spawn/reward clearance and packaged native/web loading.

coverage.json contains all source and asset hashes, exact reference counts, catalog records, all 550 pairs and each missing mapping.
The script deliberately stops at inventory and mapping prerequisites. It is not a finished variant generator.
