# ADR 0001: Character Mod Workspace as the primary workflow

Status: accepted

## Context

NTEBuildTool started as a set of focused tools for material instance creation, mesh-slot runtime toggles, and package jobs. That is not enough for full character replacement: a real replacement must coordinate a target appearance, main mesh, multiple attached meshes, material slots, runtime actions, Kawaii physics, UI preview assets, and packaging.

Source-game evidence also shows that attached meshes are not just loose editor assets. Runtime appearance data lives in `HTPlayerAppearance` / `MeshAsset_*`, while UI preview Blueprints duplicate attached mesh components for preview.

## Decision

The default workflow is now `Character Mod Workspace`, driven by `CharacterModSpec`.

`CharacterModSpec` is the single source of truth for:

- target appearance;
- main mesh;
- attached meshes;
- material operations;
- runtime actions;
- Kawaii presets;
- package settings.

The preferred attached mesh path is:

```text
HTPlayerAppearance / MeshAsset_*
  -> FashionMeshData
  -> ArrayFashionAttachedMeshData

PlayerUIShow_*
  -> SCS child HTSkeletalMeshComponentBudgeted preview components
```

Generated runtime logic should be modular. Attached mesh AnimBPs are the preferred host for attached mesh pose inheritance, Kawaii, and mesh-local runtime switching when that mesh needs it. The legacy PostProcess template runtime remains only as an adapter until the new workflow covers its use cases.

## Consequences

- The plugin needs a minimal `/Script/HTGame` stub module in the mirror project so editor-authored assets can save and cook with source-game script paths.
- Character package jobs must mark that they require the `HTGame` schema stub. The package build path should fail early if the Mirror Project explicitly restricts `NTEBuildTool` to Editor-only targets, because cook needs the plugin runtime module visible to the Game target.
- Appearance assembly and UI preview sync must be generated from the same spec data.
- Material operations must be planned and applied from `CharacterModSpec`, while reusing the same material module as standalone recipes and dialogs.
- UI and commandlets must consume the same domain modules.
- Old mesh-only and hardcoded experimental logic can be removed once superseded.
- Pure-pak remains the default. Native DLL work is only considered after pure-pak evidence proves a required path cannot be anchored through assets.

## 2026-07-11 implementation checkpoint

The first vertical path is now implemented and smoke-tested:

- `CharacterModSpec` drives `HTPlayerAppearance` creation/update.
- `ArrayFashionAttachedMeshData` supports multiple attached mesh entries, socket/transform data, anim classes, and `MeshComponentOwnedTags`.
- `NteAssetInspection` can verify generated `HTPlayerAppearance` data and Blueprint SCS nodes.
- `CharacterModSpec.MaterialOperations` now produces a `MaterialPlan`, supports `NteCharacterModSpec -ApplyMaterials`, accepts raw FModel `MaterialInstanceConstant` JSON arrays, contributes generated material instances/replacement textures to package planning, and is used by the Character Workspace material slot action.
- The Character Workspace UI can load/save `CharacterModSpec` JSON, edit core spec fields, show existing material operations, and upsert material slot actions back into the saved spec file.
- The Character Workspace Runtime Action UI now writes first-slice `MaterialSlotVisibility` entries to `CharacterModSpec.RuntimeActions` instead of invoking the legacy mesh-only toggle generator.
- Workspace package creation uses `CharacterModSpec` instead of selected Content Browser assets.
- A 004 Lacrimosa attached-mesh smoke spec cooked and packaged to pak/utoc/ucas.

The remaining unproven branch is real `PlayerUIShow` SCS synchronization, because the current Mirror Project test content does not contain a loadable source-style `PlayerUIShow_*` Blueprint.

## 2026-07-12 runtime-action writer checkpoint

The runtime-action path now has a spec-first asset writer:

- `NteCharacterModSpec -ApplyRuntimeActions` consumes `NteCharacterRuntimeActionPlan`.
- `NteCharacterRuntimeActionWriter` creates or updates generated SaveGame, Widget, and host AnimBP Blueprint assets.
- Generated assets store auditable runtime-action data variables and the condensed action-plan JSON.
- Runtime Blueprint package seeds are merged into `BuildPackagePlanFromCharacterModSpec`.
- First-create missing-package load noise is avoided by checking package existence before loading future Blueprint paths.

This checkpoint deliberately does not claim final hotkey/UI/material execution. The next ADR-significant proof is graph generation for `MaterialSlotVisibility` and `AttachedMeshVisibility` from the same `CharacterModSpec.RuntimeActions` model.

## 2026-07-12 runtime-action execution-graph checkpoint

The runtime-action writer now generates the first executable host AnimBP EventGraph slice directly from `CharacterModSpec.RuntimeActions`:

- host AnimBPs are created or repaired with the host mesh Skeleton and preview mesh;
- `BlueprintInitializeAnimation` applies default enabled states before the first hotkey press;
- unique host AnimBP paths generate hotkey polling and per-action enabled-state toggles;
- owning-component `MaterialSlotVisibility` calls `ShowMaterialSection`;
- owning-component `AttachedMeshVisibility` calls `SetVisibility`;
- shared AnimBP paths skip execution graph generation with a single warning because `GetOwningComponent` cannot safely distinguish multiple hosts using one class.

The remaining ADR-significant proofs are Widget button generation, SaveGame state, OwnerComponentByTags lookup, CopyPose/Kawaii AnimGraph generation, and a full practical package/runtime smoke test.
