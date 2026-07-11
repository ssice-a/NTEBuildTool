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

The Mirror Project currently does not contain `/Script/HTGame` stubs for `HTPlayerAppearance` or `HTSkeletalMeshComponentBudgeted`. This means the tool can plan appearance edits but must not pretend it can safely cook final MeshAsset/UIShow replacements yet.

## Evidence

Source-game examples show the target runtime shape:

- `MeshAsset_Player004_lacrimosa_fashion4` is `HTPlayerAppearance`.
- It contains `FashionMeshData.CharacterMesh`, `FashionMeshData.AnimInstance`, and `ArrayFashionAttachedMeshData`.
- Attached mesh entries contain `CharacterMesh`, `AnimInstance`, `MobileAnimInstance`, `SocketName`, `RelativeLocation`, `RelativeRotation`, and `RelativeScale3D`.
- `PlayerUIShow_004_fashion4` and `PlayerUIShow_010_fashion3` duplicate preview attached mesh components under the main `Mesh`.

The 071 mask gameplay chain is not sufficient as the attached mesh model. It is a gameplay cue/state toggle path, not a generic mesh attachment writer.

## Implementation outline

1. Generate or hand-author minimal HTGame editor stubs required to compile/cook:
   - `UHTPlayerAppearance`
   - `FHTFashionMeshData` or equivalent reflected struct
   - `FHTFashionAttachedMeshData` or equivalent reflected struct
   - `UHTSkeletalMeshComponentBudgeted`
2. Match serialized field names and types to the game usmap/FModel JSON, not guessed UE-friendly names.
3. Add an Appearance Assembly writer that consumes `FNteAppearanceAssemblyPlan`.
4. Writer output:
   - creates/updates `MeshAsset_PlayerXXX`;
   - writes main `FashionMeshData`;
   - writes `ArrayFashionAttachedMeshData`;
   - creates/updates `PlayerUIShow_XXX` SCS child components from the same attached mesh plan.
5. Add commandlet support:
   - load `CharacterModSpec`;
   - validate;
   - write appearance assets when `-ApplyAppearance` is passed;
   - otherwise only emit the plan.
6. Add package plan integration so `MeshAsset`, `PlayerUIShow`, attached meshes, runtime AnimBPs, material instances, and textures are included as candidates from the same spec.

## Tests

- BuildPlugin with `-StrictIncludes`.
- `PhyLabEditor` build after plugin sync.
- Commandlet plan-only report for example spec.
- Asset-level inspection after writing a real example once HTGame stubs exist.
- Cook/package test only after generated `HTPlayerAppearance` and `PlayerUIShow` load in the Mirror Project.
