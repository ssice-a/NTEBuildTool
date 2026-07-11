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

## Verified

- `PhyLabEditor Win64 Development` builds.
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

1. Expose material operations in the Character Workspace UI as slot-centered rows.
2. Let users pick source FModel material JSON, output path, and replacement textures without hand-writing paths.
3. Add an explicit `Apply & Assign` affordance for safe mesh slot assignment.
4. Decide how much scalar/vector/static-switch editing should enter the first Character Workspace UI pass.
5. Keep editor-only Material Proxy packages visible but excluded by default in package planning.
