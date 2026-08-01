# Kawaii AnimLayer JSON Parsing

> Historical/current-implementation reference for CharacterModSpec commandlets. The target package-first workflow is defined in `docs/post-process-kawaii-workflow.md` and `docs/pakmod-project-refactor.md`; in that workflow Apply and Build are separate, and Build never requires Apply in the same invocation.

FModel exports animation-layer blueprints as a JSON array of Unreal exports. KawaiiPhysics node settings are not top-level objects. They live on the class default object:

```text
Default__*_C.Properties.AnimGraphNode_KawaiiPhysics*
```

The analysis step should read every property whose name is either:

```text
AnimGraphNode_KawaiiPhysics
AnimGraphNode_KawaiiPhysics_<number>
```

## Important Fields

These fields describe the editable KawaiiPhysics behavior and should be preserved for import:

- `RootBone`
- `ExcludeBones`
- `AdditionalRootBones`
- `DummyBoneLength`
- `BoneForwardAxis`
- `PhysicsSettings`
- `DampingCurveData`
- `StiffnessCurveData`
- `WorldDampingLocationCurveData`
- `WorldDampingRotationCurveData`
- `RadiusCurveData`
- `LimitAngleCurveData`
- `SphericalLimits`
- `CapsuleLimits`
- `BoxLimits`
- `PlanarLimits`
- `LimitsDataAsset`
- `PhysicsAssetForLimits`
- `BoneConstraints`
- `BoneConstraintsDataAsset`
- `Gravity`
- `bEnableWind`
- `WindScale`
- `bAllowWorldCollision`
- `bOverrideCollisionParams`
- `CollisionChannelSettings`
- `bIgnoreSelfComponent`
- `IgnoreBones`
- `IgnoreBoneNamePrefix`
- `KawaiiPhysicsTag`

## Runtime Cache Fields

These fields are generated or cached at runtime and should not be treated as source configuration:

- `ModifyBones`
- `DeltaTime`
- `PreSkelCompTransform`
- `bPhysicsSettingsInitialized`
- `ComponentPose`
- `ActualAlpha`

Alpha fields such as `Alpha`, `AlphaInputType`, and `AlphaScaleBias` still matter because they are part of the anim node's editable blend behavior.

## Version Differences

Some exported NTE KawaiiPhysics nodes contain fields that are not present in stock public KawaiiPhysics:

- `PhysicsSettings.ForwardMoveOffset`
- `bUseRelativeMove`
- `MovementReferenceDisplacement`
- `CapsuleLimits[].SphereRadius`
- `CapsuleLimits[].OffSetLocation`

The analyzer preserves these fields in neutral structs where possible. Final DataAsset/AnimBP writing requires the mirror project's `/Script/KawaiiPhysics` module to expose the same reflected fields as the game usmap.

## Unified Import Contract

FModel JSON import is not a separate authoring mode. It is only one way to seed the same editable Kawaii preset model that manual authoring uses.

Required importer behaviour:

- Parse every `AnimGraphNode_KawaiiPhysics*` node into a neutral node candidate.
- Preserve source identity such as source JSON path, generated class, class default object, graph node name, and original target asset path when available.
- Allow the user to select which candidates become nodes in `NteKawaiiPreset`.
- Allow an imported node to be copied into a different target mesh preset, including copying a main/merged Physics AnimLayer node into an attached mesh preset after bone remapping.
- Allow a manually created node to reference an imported node, another saved preset, or a built-in template as a parameter seed.
- After import or manual editing, package generation must only read the resolved `NteKawaiiPreset`; it must not branch on whether a node came from JSON or was authored manually.

This contract is necessary because source-game examples are mixed:

- Some Kawaii nodes live in attached-mesh AnimBPs, where the JSON naturally maps to one attached mesh.
- Some Kawaii nodes live in shared Physics AnimLayer assets with many `AnimGraphNode_KawaiiPhysics*` entries for hair, skirt, cloth, breast, accessories, and other chains.
- A replacement mod may merge formerly separate pieces into the main mesh, or split a source-main chain into an attached mesh. The importer should expose node candidates; the user decides the final target mesh and bone mapping.

## Implemented Commandlet Import Slice

`NteCharacterModSpec` now has a first unified import slice. It imports FModel AnimBP/AnimLayer JSON into `CharacterModSpec.KawaiiPresets`; it does not generate AnimBP nodes directly.

Example:

```text
UnrealEditor-Cmd.exe PhyLab.uproject -run=NteCharacterModSpec ^
  -Spec=path/to/character.spec.json ^
  -ImportKawaiiJson=F:/F-model/Output/Exports/HT/Content/Characters/AnimInterface/Physics/Physics_AnimLayer_female051_BP_UI.json ^
  -KawaiiTargetMeshId=main ^
  -KawaiiPresetPrefix=female051 ^
  -WriteUpdatedSpec=path/to/character.with-kawaii.spec.json ^
  -Output=path/to/kawaii-import-report.json
```

Supported import fields are written into the same preset model used by manual authoring:

- source identity: `SourceKind=ImportedJson`, source JSON path, generated class, class default object, graph node name;
- chain roots: `RootBone`, `ExcludeBones`, `AdditionalRootBones`;
- simulation settings: `PhysicsSettings`, framerate, warmup, teleport thresholds, planar constraint, relative-move fields;
- curves: Damping/Stiffness/WorldDampingLocation/WorldDampingRotation/Radius/LimitAngle curve references and inline key counts;
- collision limits: spherical/capsule/box/planar limits flattened into `CollisionLimits`;
- external data references and counts: LimitsDataAsset, PhysicsAssetForLimits, BoneConstraintsDataAsset, limits-data counts, bone-constraint counts;
- NTE-specific fields currently visible in FModel/usmap, including `ForwardMoveOffset`, `bUseRelativeMove`, `MovementReferenceDisplacement`, `SphereRadius`, and `OffSetLocation`.

The commandlet report includes:

- `KawaiiImportResult`: imported count, added/replaced count, source class names, and imported preset preview;
- `KawaiiPresetPlan`: all resolved presets currently in the spec, whether imported or manual;
- `KawaiiPlan`: target mesh resolution, derived runtime AnimBP/DataAsset/CurveFloat output paths, package seeds, and schema/application warnings;
- `NormalizedSpec`: the full spec after import/upsert.

Imported presets intentionally default `SchemaStatus` to `NeedsNteSchemaCheck`. This keeps packaging/apply warnings visible until the mirror project's KawaiiPhysics plugin is proven to serialize the same fields as the game usmap.

`KawaiiPlan` derives missing output paths under the target mesh's mod folder. A main-mesh preset with id `female051_kawaii_10` can therefore resolve to paths such as:

```text
/Game/Characters/Player/051_female/mod/Kawaii/Anim/ABP_NTE_main_Kawaii
/Game/Characters/Player/051_female/mod/Kawaii/Data/DA_NTE_female051_kawaii_10_KawaiiLimits
/Game/Characters/Player/051_female/mod/Kawaii/Data/DA_NTE_female051_kawaii_10_KawaiiConstraints
/Game/Characters/Player/051_female/mod/Kawaii/Curves/CF_NTE_female051_kawaii_10_Damping
```

Derived paths are normal and are represented with `Derived*` booleans in the report rather than as warnings.

Apply/write behavior:

```text
UnrealEditor-Cmd.exe PhyLab.uproject -run=NteCharacterModSpec ^
  -Spec=path/to/character.with-kawaii.spec.json ^
  -ApplyKawaii ^
  -Output=path/to/kawaii-apply-report.json
```

`-ApplyKawaii` consumes the resolved `KawaiiPlan`; it does not care whether a preset was imported from FModel JSON, copied from another preset, or manually authored. The writer creates or updates editable `CurveFloat` assets, Kawaii `LimitsDataAsset`, `BoneConstraintsDataAsset`, and attached-mesh Kawaii Runtime AnimBPs when their output paths are resolved. Kawaii DataAsset and AnimBP writing are gated by a schema probe.

The schema probe checks for the NTE/game-specific reflected fields before writing final Kawaii assets:

- `KawaiiPhysicsSettings.ForwardMoveOffset`
- `AnimNode_KawaiiPhysics.bUseRelativeMove`
- `AnimNode_KawaiiPhysics.MovementReferenceDisplacement`
- `CapsuleLimit.SphereRadius`
- `CollisionLimitBase.OffSetLocation`
- `/Script/KawaiiPhysicsEd.AnimGraphNode_KawaiiPhysics`, for generated editor-side AnimGraph nodes

If any required game/NTE field is missing, the report marks the schema incompatible and blocks final Kawaii DataAsset/AnimBP writing. Public-plugin-only fields such as `SimulationSpace`, `SimulationBaseBone`, `SkelCompMoveScale`, or `bUpdatePhysicsSettingsInGame` are reported as warnings because they indicate the mirror plugin still has public-Kawaii surface area that is not visible in the game usmap, but they do not block DataAsset writing by themselves.

When a spec contains Kawaii presets, `-WritePackageJob` / `-BuildPackage` requires a successful `-ApplyKawaii` run in the same commandlet invocation. This keeps package seeds and actual generated assets synchronized.

## Implemented Attached-Mesh AnimGraph Writer

The current AnimGraph writer supports attached mesh targets. It intentionally blocks main-mesh Kawaii graph generation until the tool has an explicit source-pose strategy that preserves the game's character animation instead of replacing it.

For each resolved attached-mesh Runtime AnimBP path, the writer groups all presets sharing that path and rebuilds the generated Kawaii slice:

```text
CopyPoseFromMesh(bUseAttachedParent=true, bCopyCurves=true, bCopyCustomAttributes=true)
  -> LocalToComponentSpace
  -> KawaiiPhysics node 1
  -> KawaiiPhysics node 2
  -> ...
  -> ComponentToLocalSpace
  -> Output Pose
```

The writer creates graph nodes by reflection:

- `/Script/AnimGraph.AnimGraphNode_CopyPoseFromMesh`
- `/Script/AnimGraph.AnimGraphNode_LocalToComponentSpace`
- `/Script/KawaiiPhysicsEd.AnimGraphNode_KawaiiPhysics`
- `/Script/AnimGraph.AnimGraphNode_ComponentToLocalSpace`
- `/Script/AnimGraph.AnimGraphNode_Root`

Each Kawaii node is configured from the same `KawaiiPlan` preset item used to write DataAssets:

- chain roots: `RootBone`, `ExcludeBones`, `AdditionalRootBones`;
- simulation fields: physics settings, dummy bone length, forward axis, frame rate, warmup, teleport thresholds, planar constraint;
- constraints/collision flags and generated `LimitsDataAsset` / `BoneConstraintsDataAsset`;
- gravity, wind, relative move, world collision, ignore bones/prefixes, and `KawaiiPhysicsTag`;
- generated or referenced `CurveFloat` assets for supported Kawaii curve kinds.

`AppearanceAssemblyPlan` now resolves an attached mesh's `AnimInstancePath` from the Kawaii plan when the attached mesh references a `KawaiiPresetId`, or when a preset targets that attached mesh. This means the generated MeshAsset can point the attached component at `/Game/.../mod/Kawaii/Anim/ABP_NTE_<meshId>_Kawaii` without reusing the source-game `AnimBlueprintPath`.

## Non-Blocking Bone and Tag Diagnostics

`KawaiiPlan` now reports target-skeleton and GameplayTag diagnostics before the writer runs. This is deliberately a preview/reporting feature, not a hard validator: users may still save, apply, cook, and package while iterating on skeletons, tags, or imported presets.

Per preset, the report includes:

- `TargetMeshLoaded`
- `TargetSkeletonPath`
- `ReferencedBones`
- `MissingBones`
- `KawaiiPhysicsTagChecked`
- `KawaiiPhysicsTagValid`

Referenced bones are collected from:

- `RootBone`
- `ExcludeBones`
- `AdditionalRootBones.RootBone`
- `AdditionalRootBones.OverrideExcludeBones`
- collision-limit `DrivingBone`
- `IgnoreBones`

Missing bones and invalid Kawaii tags are also mirrored into `Warnings`, so the Character Workspace UI can display them without parsing UE compiler logs. This matches the desired UX: the tool explains what will likely fail or warn in Kawaii preview, but it does not block advanced users from packaging experimental assets.

The Character Workspace Kawaii list consumes these fields directly. Each preset row shows the resolved target mesh kind, target skeleton short name, `bones ok` or `missing N bone(s)`, and tag state. The row tooltip includes the full target mesh path, skeleton path, referenced-bone count, missing-bone list, Kawaii tag validity, and the resolved Runtime AnimBP path.

Attached-mesh rows also expose `Apply/Open`. That action resolves the selected preset through `KawaiiPlan`, writes the presets that share its Runtime AnimBP through `NteCharacterKawaiiWriter`, then opens the generated AnimBP in the editor. This keeps imported JSON presets and manually authored presets on the same path: both become `CharacterModSpec.KawaiiPresets`, both are planned by `KawaiiPlan`, and both are written/opened by the same writer. Main-mesh Kawaii rows stay disabled until the source-pose preservation strategy is defined.

## Mirror KawaiiPhysics Patch Checkpoint

The PhyLab mirror plugin at:

```text
F:/NTE/PhyLab/Plugins/KawaiiPhysics
```

has been patched far enough for the current Kawaii writer slice to create game-schema Kawaii DataAssets:

- `FKawaiiPhysicsSettings.ForwardMoveOffset`
- `FAnimNode_KawaiiPhysics.TargetFrameRate`
- `FAnimNode_KawaiiPhysics.bUseRelativeMove`
- `FAnimNode_KawaiiPhysics.MovementReferenceDisplacement`
- `FAnimNode_KawaiiPhysics.bPhysicsSettingsInitialized`
- `FCollisionLimitBase.OffSetLocation`
- `FCapsuleLimit.SphereRadius`

Minimal runtime/editor behavior was also wired:

- `bUseRelativeMove` uses `MovementReferenceDisplacement` as the skeletal-component movement reference. Source-game Physics AnimLayer JSON shows this value is copied into each Kawaii node through `AnimBlueprintExtension_PropertyAccess` from `HTPlayerPhysicsAnimLayer`.
- `ForwardMoveOffset` is propagated into each `ModifyBone` physics setting and offsets the pose-pull base location.
- `CapsuleLimit.SphereRadius` is used as a capsule collision bone-sphere radius override when greater than zero.

This is a compatibility bridge, not a claim that every NTE private simulation tweak has been perfectly reverse engineered. The serialization schema is now good enough for the DataAsset writer and package tests below.

Verified commands:

```text
Build.bat PhyLabEditor Win64 Development -Project=F:/NTE/PhyLab/PhyLab.uproject -WaitMutex -NoHotReload

UnrealEditor-Cmd.exe F:/NTE/PhyLab/PhyLab.uproject -run=NteCharacterModSpec
  -Spec=F:/NTE/NTEBuildTool/.scratch/character-mod-workspace/004_lacrimosa_kawaii_preset_validation.spec.json
  -ApplyKawaii

UnrealEditor-Cmd.exe F:/NTE/PhyLab/PhyLab.uproject -run=NteCharacterModSpec
  -Spec=F:/NTE/NTEBuildTool/.scratch/character-mod-workspace/004_lacrimosa_kawaii_preset_validation.spec.json
  -ApplyKawaii -WritePackageJob -BuildPackage

UnrealEditor-Cmd.exe F:/NTE/PhyLab/PhyLab.uproject -run=NteAssetInspection
  -Assets=/Game/Characters/Player/004_lacrimosa/mod/Kawaii/Anim/ABP_NTE_hair_tail_Kawaii
  -Output=F:/NTE/NTEBuildTool/.scratch/kawaii-animgraph-inspection-report.json
```

Results:

- `SchemaProbe.NteCompatible=true`
- missing required fields: none
- wrote:
  - `/Game/Characters/Player/004_lacrimosa/mod/Kawaii/Data/DA_NTE_hair_tail_kawaii_KawaiiLimits`
  - `/Game/Characters/Player/004_lacrimosa/mod/Kawaii/Data/DA_NTE_hair_tail_kawaii_KawaiiConstraints`
  - `/Game/Characters/Player/004_lacrimosa/mod/Kawaii/Anim/ABP_NTE_hair_tail_Kawaii`
- the AnimBP writer action reports `rebuilt attached-mesh Kawaii AnimGraph with 1 Kawaii node(s)`
- `NteAssetInspection` reports `AnimGraphSummary.HasExpectedAttachedKawaiiChain=true`, with one CopyPose node, one Kawaii node, one Local/Component conversion pair, one root node, and `CopyPoseNodes[0].UseAttachedParent=true`
- `KawaiiPlan` reports placeholder missing bones and invalid placeholder tags through `MissingBones` / `KawaiiPhysicsTagValid=false`
- built `lacrimosa004_kawaii_preset_validation_P.pak/.ucas/.utoc` into the configured game Mods directory with `ErrorCount=0`.

Strict plugin build verification:

```text
RunUAT.bat BuildPlugin
  -Plugin=F:\NTE\NTEBuildTool\NTEBuildTool.uplugin
  -Package=F:\NTE\NTEBuildTool\.scratch\PluginBuild_KawaiiAnimGraph_Strict
  -StrictIncludes
```

Result: `BUILD SUCCESSFUL`. A second strict build after adding `AnimGraphSummary` inspection also succeeds for `.scratch/PluginBuild_KawaiiInspection_Strict`.

The Character Workspace `Apply/Open` editor bridge was verified with:

```text
RunUAT.bat BuildPlugin
  -Plugin=F:\NTE\NTEBuildTool\NTEBuildTool.uplugin
  -Package=F:\NTE\NTEBuildTool\.scratch\PluginBuild_KawaiiApplyOpen_Strict
  -StrictIncludes

Build.bat PhyLabEditor Win64 Development -Project=F:/NTE/PhyLab/PhyLab.uproject -WaitMutex -NoHotReload

UnrealEditor-Cmd.exe F:/NTE/PhyLab/PhyLab.uproject -run=NteCharacterModSpec
  -Spec=F:/NTE/NTEBuildTool/.scratch/character-mod-workspace/004_lacrimosa_kawaii_preset_validation.spec.json
  -ApplyKawaii
  -Output=F:/NTE/NTEBuildTool/.scratch/kawaii-apply-open-report.json

UnrealEditor-Cmd.exe F:/NTE/PhyLab/PhyLab.uproject -run=NteAssetInspection
  -Assets=/Game/Characters/Player/004_lacrimosa/mod/Kawaii/Anim/ABP_NTE_hair_tail_Kawaii
  -Output=F:/NTE/NTEBuildTool/.scratch/kawaii-apply-open-inspection-report.json

UnrealEditor-Cmd.exe F:/NTE/PhyLab/PhyLab.uproject -run=NteCharacterModSpec
  -Spec=F:/NTE/NTEBuildTool/.scratch/character-mod-workspace/004_lacrimosa_kawaii_preset_validation.spec.json
  -ApplyAppearance -ApplyKawaii -WritePackageJob -BuildPackage
  -Output=F:/NTE/NTEBuildTool/.scratch/kawaii-apply-open-package-report.json
```

Result: strict plugin build succeeds, PhyLab editor build succeeds, `ApplyKawaii` reports `ErrorCount=0`, the inspected AnimBP still reports `AnimGraphSummary.HasExpectedAttachedKawaiiChain=true`, and the full package command produces `lacrimosa004_kawaii_preset_validation_P.pak/.ucas/.utoc` with `ErrorCount=0`.

## Native Kawaii edit and sync loop

The intended Kawaii authoring experience now mirrors the native plugin workflow:

```text
import/manual/template preset
  -> CharacterModSpec.KawaiiPresets
  -> KawaiiPlan
  -> Apply generated AnimBP/DataAssets
  -> edit in native UE Kawaii Details/Persona/DataAsset editors
  -> Sync back to CharacterModSpec.KawaiiPresets
  -> reapply/package from the synced spec
```

Evidence from the mirror Kawaii plugin:

- `AnimGraphNode_KawaiiPhysics::CustomizeDetails` provides the native Details customization for the Kawaii node.
- The native details surface includes `Export Limits` and `Export BoneConstraints`, matching the idea that collision limits and bone constraints are DataAsset-backed editing products.
- `KawaiiPhysicsEditMode` is the Persona edit/debug mode for visual Kawaii collision/limit/constraint work.

The NTE tool therefore does not maintain a second permanent parameter editor for Kawaii. It generates the correct assets, opens the native editors, and syncs edited asset values back into the same preset model used by import and package generation.

Character Workspace row actions:

- `Apply`: resolve the row through `KawaiiPlan` and rewrite the generated Runtime AnimBP plus generated Limits/Constraints DataAssets from the spec.
- `AnimBP`: open the generated Runtime AnimBP without rewriting it.
- `Limits`: open the generated Kawaii Limits DataAsset without rewriting it.
- `Constraints`: open the generated Kawaii BoneConstraints DataAsset without rewriting it.
- `Sync`: read the generated Runtime AnimBP/DataAssets back into `CharacterModSpec.KawaiiPresets`.

Commandlet sync:

```text
UnrealEditor-Cmd.exe F:/NTE/PhyLab/PhyLab.uproject -run=NteCharacterModSpec
  -Spec=F:/NTE/NTEBuildTool/.scratch/character-mod-workspace/004_lacrimosa_kawaii_preset_validation.spec.json
  -SyncKawaiiFromAssets
  -KawaiiPresetId=hair_tail_kawaii
  -WriteUpdatedSpec=F:/NTE/NTEBuildTool/.scratch/kawaii-native-edit-sync.synced.spec.json
  -Output=F:/NTE/NTEBuildTool/.scratch/kawaii-native-edit-sync-report.json
```

Then reapply and package from the synced spec:

```text
UnrealEditor-Cmd.exe F:/NTE/PhyLab/PhyLab.uproject -run=NteCharacterModSpec
  -Spec=F:/NTE/NTEBuildTool/.scratch/kawaii-native-edit-sync.synced.spec.json
  -ApplyKawaii
  -Output=F:/NTE/NTEBuildTool/.scratch/kawaii-native-edit-sync-reapply-report.json

UnrealEditor-Cmd.exe F:/NTE/PhyLab/PhyLab.uproject -run=NteCharacterModSpec
  -Spec=F:/NTE/NTEBuildTool/.scratch/kawaii-native-edit-sync.synced.spec.json
  -ApplyAppearance -ApplyKawaii -WritePackageJob -BuildPackage
  -Output=F:/NTE/NTEBuildTool/.scratch/kawaii-native-edit-sync-package-report.json
```

Verified results:

- sync report: `Errors=0`, `KawaiiAssetSyncResults[0].Errors=[]`;
- synced spec contains generated Runtime AnimBP, generated Limits/Constraints DataAsset paths, collision limit data, physics settings, node settings, and force/collision settings;
- reapply report: `Errors=0`;
- package report: `Errors=0`, `BuildPackage=true`;
- generated game Mods files exist: `lacrimosa004_kawaii_preset_validation_P.pak`, `.ucas`, and `.utoc`;
- inspection report `.scratch/kawaii-native-edit-sync-inspection-report.json` confirms `HasExpectedAttachedKawaiiChain=true`, one CopyPose node, one Kawaii node, one Local/Component conversion pair, one root node, and `CopyPoseNodes[0].UseAttachedParent=true`.

`Apply` remains the spec-to-asset writer and can overwrite generated assets. `AnimBP` / `Limits` / `Constraints` are safe open-only actions. `Sync` is the required step that makes native UE edits durable in the spec before package generation.
