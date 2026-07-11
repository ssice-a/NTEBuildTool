Status: in-progress

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

1. Done for the domain layer: `CharacterModSpec.KawaiiPresets` can preserve editable fields such as RootBone, ExcludeBones, additional root bones, physics settings, limits, collision references, gravity, wind, and NTE-specific source-field notes.
2. Read Kawaii nodes from `Default__*_C.Properties.AnimGraphNode_KawaiiPhysics*`.
3. Report fields that cannot be applied to the installed mirror Kawaii plugin.
4. Provide a UE editor panel/object model that edits the same `CharacterModSpec` fields.
5. Apply presets to generated attached mesh AnimBPs after schema compatibility is confirmed.

## 2026-07-11 checkpoint

Implemented:

- `CharacterModSpec.KawaiiPresets` now stores:
  - `SourceAnimBlueprintJson`
  - `SourceNodeName`
  - `SchemaStatus`
  - `RootBone`
  - `ExcludeBones`
  - structured `AdditionalRootBones`
  - `PhysicsSettings`
  - `DummyBoneLength`
  - `BoneForwardAxis`
  - `PlanarConstraint`
  - `LimitsDataAssetPath`
  - `PhysicsAssetForLimitsPath`
  - `BoneConstraintsDataAssetPath`
  - structured `CollisionLimits`
  - `Gravity`
  - `EnableWind`
  - `WindScale`
  - `UseRelativeMove`
  - `IgnoreBones`
  - `IgnoreBoneNamePrefix`
  - `KawaiiPhysicsTag`
  - `UnsupportedSourceFields`
- Package seeds include Kawaii limit/physics/bone-constraint asset references.
- Added focused validation spec:
  - `.scratch/character-mod-workspace/004_lacrimosa_kawaii_preset_validation.spec.json`
- Validation report:
  - `.scratch/character-mod-workspace/004_lacrimosa_kawaii_preset_validation.report.json`

Not done yet:

- automatic conversion from `FFModelKawaiiAnimLayerAnalysis` into `CharacterModSpec.KawaiiPresets`;
- UE details panel / visual editing;
- actual Kawaii AnimGraph node generation/application.

## Tests

- Parse known source-game Kawaii AnimBP JSON.
- Verify editable fields survive round-trip into preset JSON/report.
- Fail or warn when NTE-only fields are missing from the mirror plugin schema.
