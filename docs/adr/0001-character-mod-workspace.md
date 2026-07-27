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

## 2026-07-12 RuntimeUi schema checkpoint

Runtime UI entry configuration is now represented directly in `CharacterModSpec.RuntimeUi` and projected into `NteCharacterRuntimeActionPlan`.

This keeps generated UI title/default visibility/show-hide hotkey data in the same spec-first model as `RuntimeActions`, while preserving a clean boundary: the schema and plan can validate UI/action hotkey collisions before the Widget generation slice exists.

Verified:

- strict plugin build passes for `.scratch/PluginBuild_RuntimeUiSpec_Strict`;
- `PhyLabEditor Win64 Development` builds after plugin sync;
- a plan-only `RuntimeUi` probe reports the expected Widget/SaveGame package seeds and `RuntimeUi` fields;
- a duplicate UI/action hotkey probe fails with `Duplicate runtime hotkey: H`.

This checkpoint deliberately does not claim Widget button binding, UI show/hide execution, or SaveGame persistence yet.

## 2026-07-12 RuntimeUi execution and SaveGame bridge checkpoint

Runtime action UI generation now reaches the same state/apply path as action hotkeys instead of maintaining a separate Widget-local toggle state.

Implemented:

- `NteCharacterRuntimeActionPlan` derives a deterministic `SaveSlotName` from the runtime root.
- Generated SaveGame, Widget, and host AnimBP assets all store the shared plan metadata and SaveSlotName.
- Host AnimBPs load or create the generated SaveGame object during `BlueprintInitializeAnimation`; if no save exists, they create one from Blueprint defaults and save it to the derived slot.
- Runtime UI Widget layout is generated from `RuntimeUi` and `RuntimeActions`, including one button/label pair per action.
- Widget button click events write the action enabled field on the generated SaveGame object and call `SaveGameToSlot`.
- Host AnimBP action hotkeys write the same SaveGame fields and save to the same slot.
- Host AnimBP update logic polls SaveGame state against local cached state and only applies mesh/material visibility when the value changes.
- Runtime UI show/hide hotkey writes `NTE_RuntimeUi_CurrentVisible` through SaveGame, saves it, and updates the generated Widget visibility.

Verified:

- `RunUAT BuildPlugin -StrictIncludes` succeeds for `.scratch/PluginBuild_RuntimeSaveBridge_Strict`.
- `PhyLabEditor Win64 Development` builds after syncing the plugin mirror.
- `NteCharacterModSpec -ApplyRuntimeActions` on `.scratch/character-mod-workspace/004_lacrimosa_runtime_ui_probe.spec.json` finishes with 0 errors.
- `NteAssetInspection` confirms:
  - `WBP_NTE_CharacterActions` binds `OnClicked` and writes `BP_NTE_CharacterActionSaveGame_C.NTE_Action_toggle_main_slot0_Enabled`, then calls `SaveGameToSlot`;
  - `ABP_NTE_MainRuntimeUiProbe` contains `DoesSaveGameExist`, `LoadGameFromSlot`, `CreateSaveGameObject`, `SaveGameToSlot`, `AddToViewport`, `SetVisibility`, `NotEqual_BoolBool`, and `ShowMaterialSection`.
- Full commandlet package test with `-ApplyAppearance -ApplyRuntimeActions -BuildPackage` produces pak/ucas/utoc in the target Mods directory and cooks the generated MeshAsset, host AnimBP, SaveGame, and Widget.

Remaining ADR-significant runtime-action proofs after this checkpoint were OwnerComponentByTags lookup, CopyPose/Kawaii AnimGraph generation, material swap/parameter changes, morph/animation actions, and an in-game runtime smoke test against the actual client.

## 2026-07-12 RuntimeAction OwnerComponentByTags checkpoint

Runtime action hosting now separates the host mesh from the target mesh. Actions default to `HostMeshId=main`, so one generated main host AnimBP can own hotkey/UI/SaveGame state and control attached mesh components by tag. A runtime action can still explicitly set `HostMeshId` to another mesh when a per-attached-mesh EventGraph host is wanted.

Implemented:

- `CharacterModSpec.RuntimeActions` now accepts optional `HostMeshId`.
- `NteCharacterRuntimeActionPlan` records both `TargetMeshPath` and `HostMeshPath`, preventing host AnimBP skeleton/preview repair from accidentally using a cross-component target mesh.
- Attached mesh `MeshComponentOwnedTags` are merged with action-level `TargetComponentTags` for tag lookup.
- `NteCharacterRuntimeActionWriter` supports `OwnerComponentByTags` for first-slice `AttachedMeshVisibility` and `MaterialSlotVisibility`: generated graph uses `GetOwningComponent -> GetOwner -> FindComponentByTag -> DynamicCast`, then applies `SetVisibility` or `ShowMaterialSection`.
- The old warning that attached targets needed `EnableRuntimeActions=true` was removed from the main-host path; tags are the real requirement for cross-component lookup.

Verified:

- `RunUAT BuildPlugin -StrictIncludes` succeeds for `.scratch/PluginBuild_OwnerComponentByTags_Strict`.
- `PhyLabEditor Win64 Development` builds after syncing the mirror plugin copy.
- `NteCharacterModSpec -ApplyAppearance -ApplyRuntimeActions` on `.scratch/character-mod-workspace/004_lacrimosa_runtime_actions_graph_probe.spec.json` reports 0 errors and shows `toggle_smoke_attach` as `HostMeshId=main`, `TargetLookupMode=OwnerComponentByTags`.
- `NteAssetInspection` of `/Game/Characters/Player/004_lacrimosa/mod/RuntimeGraphProbe/ABP_NTE_MainGraphProbe` confirms `GetOwner`, `FindComponentByTag`, `NTE.Attached.smoke_attach`, `DynamicCast`, and `SetVisibility` nodes are generated for the attached visibility action.
- Full commandlet package test with `-ApplyAppearance -ApplyRuntimeActions -BuildPackage` produces `lacrimosa004_runtime_actions_graph_probe_P.pak/.ucas/.utoc` under `F:/Neverness To Everness/Client/WindowsNoEditor/HT/Content/Paks/Mods`.

Remaining ADR-significant runtime-action proofs are CopyPose/Kawaii AnimGraph generation, material swap/parameter changes, morph/animation actions, and an in-game runtime smoke test against the actual client.

## 2026-07-13 Kawaii native edit/sync checkpoint

The Character Workspace Kawaii flow now uses native UE/Kawaii asset editing without creating a second source of truth.

Decision:

- `CharacterModSpec.KawaiiPresets` remains the persistent model for imported, manual, template-seeded, and UE-edited Kawaii settings.
- Generated attached Runtime AnimBPs and generated Limits/Constraints DataAssets are editable UE assets, but edits must be synchronized back into the spec before the next destructive apply/package run.
- The editor UI exposes this explicitly as `Apply`, `AnimBP`, `Limits`, `Constraints`, and `Sync`.
- `Apply` writes from spec to generated assets; open actions do not overwrite; `Sync` reads generated assets back to spec.

Implemented:

- `NteCharacterKawaiiAssetSync` reads the generated Kawaii AnimBP node and generated DataAssets through reflection.
- `NteCharacterModSpec` supports `-SyncKawaiiFromAssets`, optional `-KawaiiPresetId`, and `-WriteUpdatedSpec` / `-SaveSpec`.
- Generated Kawaii nodes carry `NTE Character Kawaii Generated:<PresetId>` comments so sync can target the right preset even when multiple Kawaii nodes share a Runtime AnimBP.

Verified:

- strict plugin build passes for `.scratch/PluginBuild_KawaiiNativeEditSync2_Strict`;
- `PhyLabEditor Win64 Development` builds after syncing the plugin mirror;
- sync to `.scratch/kawaii-native-edit-sync.synced.spec.json`, reapply, full package, and `NteAssetInspection` all complete with 0 commandlet errors;
- the inspected attached Runtime AnimBP still reports `HasExpectedAttachedKawaiiChain=true`, one CopyPose node, one Kawaii node, one Local/Component conversion pair, and `CopyPoseNodes[0].UseAttachedParent=true`.

Remaining ADR-significant Kawaii proof: main-mesh Kawaii source-pose preservation is still intentionally not implemented.
