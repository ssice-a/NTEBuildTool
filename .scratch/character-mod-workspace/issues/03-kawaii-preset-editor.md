Status: ready-for-agent

# Kawaii preset import and editor workflow

## Goal

Support UE-side editing of KawaiiPhysics parameters for main and attached meshes, while allowing parameters to be seeded from source-game AnimBP JSON.

## Decisions

- Blender/DCC owns geometry, bone names, skeleton hierarchy, weights, UV, material IDs, and morph targets.
- UE owns Kawaii node parameters and visual editing.
- FModel AnimBP JSON can seed Kawaii presets.
- The game usmap is the serialized schema authority.
- Stock public KawaiiPhysics must not be assumed compatible.

## Implementation outline

1. Read Kawaii nodes from `Default__*_C.Properties.AnimGraphNode_KawaiiPhysics*`.
2. Preserve editable fields such as RootBone, ExcludeBones, curves, limits, collision references, gravity, wind, and NTE-specific fields.
3. Report fields that cannot be applied to the installed mirror Kawaii plugin.
4. Provide a Kawaii preset object/model that can be assigned by `CharacterModSpec`.
5. Apply presets to generated attached mesh AnimBPs after schema compatibility is confirmed.

## Tests

- Parse known source-game Kawaii AnimBP JSON.
- Verify editable fields survive round-trip into preset JSON/report.
- Fail or warn when NTE-only fields are missing from the mirror plugin schema.
