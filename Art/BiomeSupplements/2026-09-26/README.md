# Biome prop supplements

Created with the built-in image generation tool using the project's actual tilesheets as visual references. Original game art is unchanged. Both selected PNGs were verified as 1448 x 1086 RGBA with transparent pixels.

These are new reference-derived artwork sheets, not pixel-for-pixel extractions of the supplied art. They are saved outside MapTilesets deliberately: they still require normalization to the game's 16-pixel tile scale, sprite slicing, collision metadata and in-engine visual checking before use in released levels. Do not treat their high-resolution pixel-art appearance as an exact match to the original pixel grid. The lit props are static artwork, not animation strips.

## Ancient Castle

File: `ancient-castle-props-v1.png`
Reference: `MapTilesets/Ancient Castle.png`

Reading left to right, top to bottom:

1. Intact stone column
2. Broken column
3. Stone plinth
4. Crumbling masonry wall
5. Rubble pile
6. Fallen column
7. Stone barricade
8. Ruined archway
9. Unlit brazier
10. Lit brazier
11. Stone urn
12. Iron gate with stone posts

Final generation prompt:

> Generate a transparent-background PNG RGBA game prop sheet. Background MUST be alpha zero, do NOT paint checkerboard squares. Reference image is the game's existing castle tiles; match its blue-grey lavender stone, dark navy iron/outline, top-down RPG perspective and upper-left highlights. Exactly12 separate pixel-art props evenly arranged4 columns by3 rows, wide empty transparent gutters. Top row: square intact stone column; broken column; squat stone plinth; low crumbling masonry wall. Middle: small rubble pile; fallen column; stone barricade; small ruined archway. Bottom: unlit iron brazier on stone base; matching lit brazier with small amber flame; stone urn; short iron gate between stone posts. No text, no numbers, no visible grid, no ground plane. Low-resolution hard-edged pixel clusters with flat color steps, no smooth shading. Each object fully visible and separate. Transparent background is critical, with actual PNG alpha, never a checkerboard illustration.

Two earlier castle outputs with baked-in checkerboards were rejected and not installed in the project.

## Graveyard

File: `graveyard-props-v1.png`
Reference: `MapTilesets/Graveyard.png`

Reading left to right, top to bottom:

1. Broken headstone
2. Paired headstones
3. Sealed stone tomb
4. Cracked coffin
5. Ruined iron fence
6. Broken grave border
7. Mossy rubble
8. Dead stump
9. Candle pedestal
10. Empty pedestal
11. Skull and bones
12. Ruined crypt doorway

Final generation prompt:

> Create a supplemental game prop sprite sheet using this supplied Graveyard tilesheet as the visual reference. Exactly 12 distinct freestanding objects arranged evenly in 4 columns and 3 rows. Match muted warm grey stone, weathered olive moss, dark brown outlines and sparse amber candle accents. Top-down 2D RPG perspective with visible front faces, simple chunky low-resolution pixel clusters, upper-left light; no isometric diamonds, no soft painting, no gradients. Row1: broken headstone, paired old headstones, low sealed stone tomb, cracked stone coffin. Row2: short ruined iron fence, collapsed stone grave border, mossy rubble pile, short dead stump. Row3: small stone candle pedestal, empty stone candle pedestal, skull-and-bones pile, ruined crypt doorway small enough for a room prop. Keep all objects separate with wide empty margins for slicing. No words, no labels, no visible grid, no scene. REAL transparent PNG background with alpha channel, not a drawn checkerboard. Preserve the supplied tilesheet's quiet worn graveyard aesthetic and compact readable silhouettes. These are reusable room obstacles and decorations, not large buildings.
