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

## Blockers

- Need concrete template/graph-generation design for CopyPose + Kawaii + output pose.
- Need action executor graph generation for the first runtime slice:
  - `AttachedMeshVisibility`;
  - `MaterialSlotVisibility`.
- Need Kawaii schema compatibility before final cooked physics AnimBPs are trusted.

## Tests

- BuildPlugin with strict includes.
- Commandlet inspection of generated AnimBP graph structure.
- Cook/package only after generated AnimBP compiles and is referenced by MeshAsset attached mesh data.
