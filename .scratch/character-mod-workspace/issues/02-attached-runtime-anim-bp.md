Status: in-progress

# Attached mesh runtime AnimBP generator

## Goal

Generate attached mesh AnimBPs from `CharacterModSpec` / `AppearanceAssemblyPlan`.

Each attached runtime AnimBP should be able to:

- inherit the parent/main mesh pose when required;
- run KawaiiPhysics or future secondary-motion nodes in the AnimGraph;
- host simple runtime actions in the EventGraph when the attached mesh owns hotkey/UI switching;
- reference any Widget/SaveGame/action data assets required for cook reachability.

## Decisions

- Do not create a separate controller Blueprint by default.
- Do not make PostProcess AnimBP the new primary route.
- Keep the generated AnimBP contract small and inspectable.
- Runtime actions should come from one `CharacterModSpec.RuntimeActions` model, shared by hotkeys and UI buttons.
- Runtime component lookup should use stable tags, primarily `MeshComponentOwnedTags` from attached mesh definitions and action-level `TargetComponentTags`, not SCS node names or array indices.
- The first implementation step is the domain model/validation layer. Blueprint graph generation comes after the action model is stable.

## 2026-07-11 checkpoint

Implemented:

- `CharacterModSpec.RuntimeActions` now distinguishes:
  - `MaterialSlotVisibility`
  - `AttachedMeshVisibility`
  - `MaterialSwap`
  - `ScalarParameter`
  - `VectorParameter`
  - `MorphTarget`
- Runtime actions can carry `TargetComponentTags`, `MaterialSlots`, `MaterialPath`, `ParameterName`, `ScalarValue`, `VectorValue`, `MorphTargetName`, and `MorphValue`.
- Validation now rejects unsupported action types, invalid/duplicate hotkeys, duplicate/negative material slots, missing material swap materials, missing parameter names, missing morph target names, and attached mesh visibility actions without any runtime component tag source.
- The 071 example now uses `AttachedMeshVisibility` and `NTE.Attached.mask` tags instead of the old ambiguous `MeshVisibility` placeholder.
- Added a focused 004 runtime-action validation spec:
  - `.scratch/character-mod-workspace/004_lacrimosa_runtime_actions_validation.spec.json`
- Added `NteCharacterRuntimeActionPlan`:
  - groups actions by runtime host mesh / host AnimBP;
  - resolves attached targets through action `TargetComponentTags` plus attached mesh `MeshComponentOwnedTags`;
  - records whether an action is in the first Blueprint generation slice (`AttachedMeshVisibility`, `MaterialSlotVisibility`) or schema-only for now;
  - plans shared Widget and SaveGame Blueprint paths under `/mod/Runtime`.
- Added commandlet reports:
  - `.scratch/character-mod-workspace/004_lacrimosa_runtime_actions_plan.report.json`
  - `.scratch/character-mod-workspace/071_chaos_runtime_actions_plan.report.json`

## 2026-07-12 checkpoint

Implemented:

- Added `NteCharacterRuntimeActionWriter`.
- Added `NteCharacterModSpec -ApplyRuntimeActions`.
- Runtime-action package seeds now include generated SaveGame, Widget, and host AnimBP Blueprint paths.
- The writer creates or updates:
  - `/mod/Runtime/BP_NTE_CharacterActionSaveGame`
  - `/mod/Runtime/WBP_NTE_CharacterActions`
  - host AnimBPs from the runtime-action plan.
- The writer stores the condensed action plan and per-action data as Blueprint variables.
- The writer now creates host AnimBPs with the host mesh Skeleton/preview mesh and generates the first owning-component hotkey EventGraph executor slice:
  - `MaterialSlotVisibility` -> `WasInputKeyJustPressed` / modifier checks / toggle enabled variable / `ShowMaterialSection`;
  - `AttachedMeshVisibility` -> `WasInputKeyJustPressed` / toggle enabled variable / `SetVisibility`.
- Shared host AnimBP paths skip execution graph generation with one warning because `GetOwningComponent` would be ambiguous.
- First-create package load noise is avoided by checking loaded packages and `FPackageName::DoesPackageExist` before attempting to load future Blueprint paths.

Verified:

- `NteCharacterModSpec -ApplyRuntimeActions` on `.scratch/character-mod-workspace/004_lacrimosa_runtime_actions_validation.spec.json`.
- `.scratch/character-mod-workspace/004_lacrimosa_apply_runtime_actions.report.json`.
- Temporary first-create probe report `.scratch/character-mod-workspace/004_lacrimosa_apply_runtime_actions_create_probe.report.json`; generated probe uassets were removed after validation.
- `NteAssetInspection` confirms the formal generated runtime SaveGame, Widget, and AnimBP assets load:
  - `.scratch/character-mod-workspace/004_lacrimosa_runtime_actions_assets.json`
- `RunUAT BuildPlugin -StrictIncludes` succeeds for `.scratch/PluginBuild_CharacterRuntimeActionWriter_Strict3`.
- Fresh first-create graph probe regenerates `/Game/Characters/Player/004_lacrimosa/mod/RuntimeGraphProbe` without AnimBP missing-Skeleton compile errors.
- `NteCharacterModSpec -ApplyRuntimeActions` on `.scratch/character-mod-workspace/004_lacrimosa_runtime_actions_graph_probe.spec.json`.
- `NteAssetInspection` confirms the graph-probe generated SaveGame, Widget, main host AnimBP, and attached host AnimBP load with 0 errors / 0 warnings:
  - `.scratch/character-mod-workspace/004_lacrimosa_runtime_actions_graph_probe_assets.json`
- `RunUAT BuildPlugin -StrictIncludes` succeeds for `.scratch/PluginBuild_CharacterRuntimeActionGraph_Strict`.

## Blockers

- Need concrete template/graph-generation design for CopyPose + Kawaii + output pose.
- Need Widget button creation/click binding and SaveGame load/save graph generation.
- Need OwnerComponentByTags lookup for host != target actions.
- Need CopyPose AnimGraph generation before attached runtime AnimBPs are production-complete.
- Need Kawaii schema compatibility before final cooked physics AnimBPs are trusted.

## Tests

- BuildPlugin with strict includes.
- Commandlet inspection of generated AnimBP graph structure.
- Cook/package only after generated AnimBP compiles and is referenced by MeshAsset attached mesh data.
