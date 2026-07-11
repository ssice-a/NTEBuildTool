Status: ready-for-agent

# HTGame schema and appearance assembly writer

## Goal

Enable `CharacterModSpec` to write real runtime appearance assets:

- `HTPlayerAppearance` / `MeshAsset_PlayerXXX`
- `PlayerUIShow_XXX` SCS preview child components using `HTSkeletalMeshComponentBudgeted`

## Current state

The code now has:

- `FNteCharacterModSpec`
- JSON load/save/validation
- package seed collection
- `FNteAppearanceAssemblyPlan`
- commandlet report output for `AppearanceAssemblyPlan`
- a plugin runtime module named `HTGame`, producing `/Script/HTGame`
- minimal reflected stubs for:
  - `UHTPlayerAppearance`
  - `FHTFashionMeshData`
  - `FHTFashionAttachedMeshData`
  - `UHTSkeletalMeshComponentBudgeted`
- `FNteAppearanceAssemblyWriter`
- `-ApplyAppearance` support in `NteCharacterModSpec`
- `NteAssetInspection` support for `HTPlayerAppearance` field reports

The Mirror Project now compiles the plugin-provided `/Script/HTGame` stubs. MeshAsset creation/update is implemented for the minimal reflected field set.

`PlayerUIShow` SCS sync is implemented for existing Blueprint assets that can be loaded and edited. It deliberately does not create a missing UIShow Blueprint with a guessed parent class.

## Evidence

Source-game examples show the target runtime shape:

- `MeshAsset_Player004_lacrimosa_fashion4` is `HTPlayerAppearance`.
- It contains `FashionMeshData.CharacterMesh`, `FashionMeshData.AnimInstance`, and `ArrayFashionAttachedMeshData`.
- Attached mesh entries contain `CharacterMesh`, `AnimInstance`, `MobileAnimInstance`, `SocketName`, `RelativeLocation`, `RelativeRotation`, and `RelativeScale3D`.
- `PlayerUIShow_004_fashion4` and `PlayerUIShow_010_fashion3` duplicate preview attached mesh components under the main `Mesh`.

The 071 mask gameplay chain is not sufficient as the attached mesh model. It is a gameplay cue/state toggle path, not a generic mesh attachment writer.

## Implementation outline

1. Done: generate or hand-author minimal HTGame editor stubs required to compile/cook:
   - `UHTPlayerAppearance`
   - `FHTFashionMeshData` or equivalent reflected struct
   - `FHTFashionAttachedMeshData` or equivalent reflected struct
   - `UHTSkeletalMeshComponentBudgeted`
2. Done for the current minimal field set: match serialized field names and broad types to FModel JSON evidence.
3. Done: add an Appearance Assembly writer that consumes `FNteAppearanceAssemblyPlan`.
4. Done for MeshAsset runtime appearance data:
   - creates/updates `MeshAsset_PlayerXXX`;
   - writes main `FashionMeshData`;
   - writes `ArrayFashionAttachedMeshData`;
   - updates existing loadable `PlayerUIShow_XXX` SCS child components from the same attached mesh plan.
5. Done: add commandlet support:
   - load `CharacterModSpec`;
   - validate;
   - write appearance assets when `-ApplyAppearance` is passed;
   - otherwise only emit the plan.
6. Done: package seed/report integration uses the dedicated `BuildPackagePlanFromCharacterModSpec` API.

## Tests

- Done: BuildPlugin with `-StrictIncludes`.
- Done: `PhyLabEditor` build after plugin sync.
- Done: commandlet plan-only report for example spec.
- Done: MeshAsset smoke apply with 004 Lacrimosa:
  - spec: `.scratch/character-mod-workspace/004_lacrimosa_apply_meshasset_noloadspam.spec.json`
  - report: `.scratch/character-mod-workspace/004_lacrimosa_apply_meshasset_noloadspam.report.json`
  - generated asset: `/Game/Characters/Player/004_lacrimosa/mod/Generated/MeshAsset_Player004_NTE_NoLoadSpam`
  - inspection: `.scratch/character-mod-workspace/004_lacrimosa_generated_meshasset_inspection_detailed.json`
- Cook/package test only after generated `HTPlayerAppearance` and `PlayerUIShow` load in the Mirror Project.

## 2026-07-11 checkpoint

Verified:

- `RunUAT BuildPlugin -StrictIncludes` succeeds with the new `HTGame` module.
- `PhyLabEditor` builds with the mirrored plugin.
- `NteCharacterModSpec` plan-only report for the 071 example returns 0 errors / 0 warnings.
- `NteCharacterModSpec -ApplyAppearance` writes a new `HTPlayerAppearance` asset for the 004 smoke spec.
- First-create `LoadPackage: SkipPackage` noise was removed from the writer by checking `FPackageName::DoesPackageExist` before loading a future package.
- `NteAssetInspection` loads the generated asset and reports:
  - `Class = HTPlayerAppearance`
  - `FashionMeshData.CharacterMesh = /Game/Characters/Player/004_lacrimosa/player_004_lacrimosa_skin`
  - `FashionMeshData.AnimInstanceClass = /Game/Characters/Player/004_lacrimosa/mod/Runtime/ABP_NTE_ModToggle_PostProcess.ABP_NTE_ModToggle_PostProcess_C`

Remaining:

- Validate `PlayerUIShow` SCS sync against a real existing UIShow Blueprint in the Mirror Project.
- Broaden the HTGame stub only when new usmap/FModel evidence requires more fields.

## 2026-07-11 attached mesh checkpoint

Verified with `.scratch/character-mod-workspace/004_lacrimosa_apply_attached_mesh.spec.json`:

- `NteCharacterModSpec -ApplyAppearance` writes one attached mesh entry.
- `NteAssetInspection` confirms:
  - `Class = HTPlayerAppearance`
  - `AttachedMeshCount = 1`
  - `SocketName = Bip001-Head`
  - `MeshComponentOwnedTags = ["NTE.ToggleTarget", "NTE.Attached.smoke_attach"]`
- `NteCharacterModSpec -ApplyAppearance -BuildPackage` packages the generated MeshAsset and dependencies successfully after the Mirror Project exposes `NTEBuildTool` to the Game target.
