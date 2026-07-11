Status: ready-for-agent

# CharacterModSpec material operations

## Goal

Make material creation/assignment a first-class Character Mod Workspace slice, driven by `CharacterModSpec.MaterialOperations`.

## Current state

Implemented:

- `NteCharacterMaterialPlan` resolves material operations to target mesh, slot, parent material, output material, and normalized source texture overrides.
- Missing parent material paths are derived from FModel source paths.
- Missing output material paths are derived under the target character root, usually `/Game/Characters/Player/<id>/mod/Materials`.
- `NteCharacterMaterialWriter` calls the shared material module.
- `NteCharacterModSpec` reports `MaterialPlan` and supports `-ApplyMaterials`.
- Raw FModel `MaterialInstanceConstant` export arrays are normalized into internal `Textures`, `Scalars`, `Colors`, and `Switches` parameter sections.
- Package seeds include generated material instances and replacement textures from the material plan.
- The Character Workspace can load/save a `CharacterModSpec` JSON, edit core spec fields, list existing material operations, and apply material slot actions through `NteCharacterMaterialPlan` / `NteCharacterMaterialWriter`.
- Workspace material actions now upsert by operation id and save the updated `MaterialOperations` entry back to the workspace spec file when available.
- The standalone `Create Material Instance From FModel Material JSON` menu remains an advanced direct material-tool adapter, but it now uses the same source-material loader and accepts raw FModel export arrays.

## Verified

- `PhyLabEditor Win64 Development` builds.
- Character Workspace material action compile smoke passes through the mirrored PhyLab plugin build.
- Workspace `CharacterModSpec` load/save and material-operation upsert compile smoke passes through the mirrored PhyLab plugin build.
- `RunUAT BuildPlugin -StrictIncludes` passes for `.scratch/PluginBuild_CharacterWorkspaceSpecPersistence`.
- Plan-only reports:
  - `.scratch/character-mod-workspace/071_chaos_material_plan.report.json`
  - `.scratch/character-mod-workspace/004_lacrimosa_material_plan.report.json`
- Safe apply report:
  - `.scratch/character-mod-workspace/004_lacrimosa_apply_materials_safe.report.json`
- Texture override apply report:
  - `.scratch/character-mod-workspace/004_lacrimosa_apply_materials_texture_override.report.json`

The texture override report confirms:

- `TextureOverrides=1`
- `SourceTextureOverrideGroups=1`
- no missing textures
- no unmatched source texture overrides
- material report `AssetLoads=true` for `/Game/Characters/Player/004_lacrimosa/T_player_004_lacrimosa_02_d`

## Next implementation

1. Make existing material operation rows editable, including remove/reorder and per-row reapply affordances.
2. Add an explicit `Apply & Assign` affordance for safe mesh slot assignment.
3. Decide how much scalar/vector/static-switch editing should enter the first Character Workspace UI pass.
4. Keep editor-only Material Proxy packages visible but excluded by default in package planning.
