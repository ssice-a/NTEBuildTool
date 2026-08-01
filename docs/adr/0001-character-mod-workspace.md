# ADR 0001: Character Mod Workspace as the primary workflow

Status: accepted (historical; package core direction superseded by ADR-0002)

ADR-0002 supersedes the decision that `CharacterModSpec` is the universal persistent source of truth and that Appearance authoring is part of the package core. The Character Workspace and its authoring features remain supported, but the persistent package-first model is now `NTE.PakmodProject`; see `docs/pakmod-project-refactor.md`.

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

Historical checkpoint, superseded by the 1213/1214 runtime correction below: real `PlayerUIShow` SCS synchronization was still unproven here because the Mirror Project did not yet contain a loadable source-style `PlayerUIShow_*` Blueprint.

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

## 2026-07-29 native attached-mesh schema correction

An in-game failure showed that a cooked `HTPlayerAppearance` can expose plausible values in CUE4Parse while the game rejects its attached-mesh fields. The generated versioned package used the stub struct tags `HTFashionMeshData` and `HTFashionAttachedMeshData`, but the game schema requires `CharacterMeshData` and `AttachedMeshData`. The game/UE loader reported both struct-type and array-inner-type mismatches.

Decision:

- `/Script/HTGame` reflected type names, property kinds, inheritance, and declaration order must match the game usmap exactly.
- `HTPlayerAppearance` inherits `HTAppearanceDataAsset` and declares all 19 game-owned properties in usmap order.
- `FashionMeshData` uses `CharacterMeshData`; `ArrayFashionAttachedMeshData` contains `AttachedMeshData`.
- Anim blueprint class references in those structs serialize as `ObjectProperty`, matching the usmap, rather than `ClassProperty`.
- `HTAttachedMeshAnimInstance` includes all 13 game-owned properties in usmap order.
- `ApplyAppearance` validates this schema through UE reflection before writing an asset and fails rather than emitting a known-incompatible package.
- Big-world attachment is registered by the replaced `MeshAsset_*` / `HTPlayerAppearance`. `PlayerUIShow_*` is a separate replacement required only for the character-preview UI.
- Appearance writing and package building run in separate UE processes until package planning no longer risks observing stale AssetRegistry dependencies from the writer process.

Verified with the Nanally probe:

- a fresh UE process reloads the rewritten `MeshAsset_Player010` without struct mismatch warnings;
- a second process cooks and packages the appearance, main mesh, attached mesh, skeleton, and CopyPose AnimBP;
- CUE4Parse selects `zz_nanally_attached_pelvis_socket_probe_999_P.utoc` at read order `100003` and reads one attached entry with the expected mesh, AnimBP, mobile AnimBP, pelvis socket, and component tag.

## 2026-07-29 Nanally attached-mesh diagnosis boundary (superseded by 1213/1214)

The Nanally authoring inputs require a narrower diagnosis model than the generic two-mesh workflow previously assumed.

Facts:

- `原版服装修改.fbx` is the main mesh, but several original garment parts were moved into `额外物理骨骼.fbx`.
- Missing main-mesh material slots must be restored at their original `player_010_nanally_skin.psk` indices. Appending them changes the section/material contract and is invalid.
- The attached FBX uses an extended skeleton. Bones copied from another outfit collided by name, and affected skin weights were remapped to the `.001` bone variants.
- The PSK/FBX channel-permutation comparison rules out a global G/B swap. RGBA matches 9262/9273 main-mesh positions and 2371/2373 attached-mesh positions, while RBGA matches only 2089/9273 and 160/2373. No bulk channel rewrite is allowed; the remaining 11 and 2 local mismatches are separate follow-up evidence.
- Material-face counts identify original PSK slot 10 (`MI_player_010_female_cloth_b`, 2090 triangles) as the part renamed to attached-FBX `MI_player_010_female_cloth_b_glass` (also 2090 triangles). The main mesh needs an empty slot at index 10 so original slots 11 and 12 do not shift.
- At this checkpoint, the replaced main mesh was visible while the mesh registered in `ArrayFashionAttachedMeshData` was not. The later 1213/1214 checkpoint separates UI SCS success from the still-unproven world Appearance path.

Decision:

- Historical decision at this checkpoint: do not split UI-preview and big-world mesh bugs. Runtime attribution in 1213 later disproved this because the two views share mesh assets but create attached components independently.
- Treat FModel exports and the game usmap as evidence sources. Compare complete native attachment tuples: appearance entry, skeletal mesh, skeleton hierarchy/root transform, socket/relative transform, attached AnimBP pose source, and Kawaii configuration.
- Do not treat the generated CopyPose probe as proof of the game's native method. Inspected native attached AnimBPs inherit `HTAttachedMeshAnimInstance` but commonly use their own SequencePlayer/BlendSpace before LocalToComponent, Kawaii, ComponentToLocal, and Output.
- Keep probes single-variable and require an in-game visibility result. CUE4Parse proving that the cooked reference exists is necessary but not sufficient.
- Native-sample scan currently covers 54 `ArrayFashionAttachedMeshData` entries. Every native entry uses a real bone/socket name; none uses `NAME_None`. A full-body-space attached FBX therefore needs an explicit compatible mount such as the main mesh `root`, not the string `None` serialized as `FName(None)`.

Implementation and probe:

- Appearance planning now trims `SocketName` and rejects both an empty value and UE `NAME_None`.
- A negative commandlet regression using the old `SocketName=None` spec returns code 4 with one appearance error and writes no asset; the equivalent `root` spec has no appearance-plan error.
- Probe 1002 changes only the Reference Pose attachment socket from `None` to `root`. Apply and package run in separate UE processes with zero errors.
- CUE4Parse selects `zzzzzz_nanally_attached_root_refpose_schema_probe_1002_P.utoc` at read order `100303` for the main mesh, appearance, attached mesh, and attached skeleton. The cooked entry reads `SocketName=root` and null desktop/mobile AnimInstances. In-game visibility remains the required result before accepting hypothesis 1.

## 2026-07-29 Nanally 1213/1214 runtime correction

Runtime evidence supersedes the earlier assumption that sharing one main SkeletalMesh means UI and
world create attached components through one path.

Decision:

- Treat `PlayerUIShow_*` SCS and `HTPlayerAppearance.ArrayFashionAttachedMeshData` as independent
  component-creation paths. They should be generated from the same spec, but each requires its own
  runtime acceptance signal.
- Attribute a runtime component by its assigned AnimBP and owning actor, not merely by mesh path.
  For Nanally, `CopyPoseProbe` identifies the UI SCS component and the generated Kawaii AnimBP
  identifies the intended world Appearance component.
- Mirror `HTUIShowSimpleCharacter` as its native Actor schema. An `ACharacter` substitute changes
  native component names and exports and is not serialization-compatible.
- Treat existing source-game AnimBPs as external references by default. Package generated mod
  AnimBPs explicitly, but do not package reconstructed copies of shared original AnimBPs unless the
  mod intentionally replaces them.

Evidence:

- Final 1213 cooks `PlayerUIShow_010` as the official seven-export shape plus one
  `HTSkeletalMeshComponentBudgeted` template and one SCS node. The node parents to native `Mesh`,
  uses `Bip001-Pelvis`, and assigns the extra mesh plus `CopyPoseProbe`.
- UE4SS runtime enumeration found that exact visible component under the UI character actor. UE4SS
  did not create it. Loading and inspecting `MeshAsset_Player010` in the same probe proved only that
  the modified Appearance asset was readable.
- 1214 differs from 1213 by exactly two excluded packages: `Player010_Nanally_AnimBP` and
  `Player010_Nanally_UIAnimBP`. The official packages win those paths again.
- A direct `HTGame.exe` launch without UE4SS still loads the extra mesh, while base 010 and night
  animations recover. This proves the UI attachment is pure-pak and the T Pose was caused by the two
  reconstructed shared AnimBP replacements.

Remaining boundary: the confirmed component uses the UI `CopyPoseProbe` class. The world Appearance
path is not accepted until the actual world actor exposes a component using
`ABP_NTE_extra_physics_Kawaii_C`. Alignment is also separate from component creation and animation
recovery.

## 2026-07-29 Nanally world SCS and indexed-material decision

Runtime evidence from 1215 confirms that the target big-world actor is
`/Game/Blueprints/Character/NPC/NPC_011/WA/BP_NPC_011_WA008`. Its native main component is
`CharacterMesh0`. The accepted pure-pak world path adds one SCS
`HTSkeletalMeshComponentBudgeted` child named `NTE_Attach_extra_physics`, parented to
`CharacterMesh0`, with identity relative transform and the generated Kawaii AnimBP. UE4SS observed
this component after a full process restart; it was not present in the pre-1215 process.

Material identity is an indexed mesh contract. Retain the official 13-entry material table even
though the edited main FBX renders only 11 sections. Keep the LOD0 mapping
`[1,2,3,4,5,6,7,8,9,11,12]`. Official index 5 is the eyelash material. If an official material is
absent from the Mirror Project, create an editor-only same-path proxy and exclude it from packaging
so the cooked mesh imports the original game object rather than shipping the proxy.

1216 verifies this decision: cooked slot 5 imports `MI_player_010_eyelash`, the section map is
unchanged, the proxy is absent from the response, all 31 targets resolve to 1216, and shared source
AnimBPs continue to resolve to official containers.

## 2026-07-29 config-driven presentation and package-intent decision

The accepted 1215/1216 probes exposed two missing domain concepts in the plugin: a character can have
multiple Blueprint presentation targets, and an asset path can be a reference without being package-owned.

Decision:

- Replace the PlayerUIShow-only SCS writer with one presentation-target writer.
- Store target Blueprint path, exact parent mesh component name, optional main AnimBP, and main-mesh update
  policy in `CharacterModSpec.Appearance.PresentationTargets`.
- Fail when the named parent component does not exist. Component-name guessing is not a supported fallback.
- Generate UI and world attached components from the same `AppearanceAssemblyPlan`.
- Keep `UIActorClassPath` only as a legacy JSON migration input to `ui/Mesh`.
- Record explicit `ExternalReference`, `GeneratedAsset`, and `ReplacementAsset` entries in
  `CharacterModSpec.Package.Assets`.
- Treat source-game main, UI, desktop, and mobile AnimBP fields as reference-only unless an explicit package
  intent says the mod owns their path. Kawaii-generated Runtime AnimBPs remain generated assets.
- Run asset-level Kawaii preflight before a Package Job: generated assets must exist, CopyPose must use the
  attached parent, Kawaii node count must match the preset group, referenced bones must exist, and the
  Appearance plan must assign that Runtime AnimBP to its attached mesh.

The indexed material-slot correction remains specific to the current Nanally FBX. It must not become a generic
plugin rule or hardcoded 13-slot layout.

## 2026-07-30 Nanally native-world attachment correction

The world and UI presentation targets remain distinct, but an attached mesh does not have to be emitted to
every target. Nanally runtime evidence and the final static differential showed a duplicate world
registration: the native `HTPlayerAppearance.ArrayFashionAttachedMeshData` entry coexisted with a generated
`BP_NPC_011_WA008` SCS child for the same mesh and AnimBP.

Decision:

- `MeshAsset_Player010` is the only big-world attachment owner for Nanally.
- `PlayerUIShow_010` retains one generated SCS child because UI preview assembles components independently.
- `BP_NPC_011_WA008` is an external original-game Blueprint and must not be packaged. Same-path main-mesh
  replacement already propagates through its original references; reconstructing the Blueprint only to
  restate `CharacterMesh0`/AnimBP is a destructive write.
- Nanally's only presentation target is `ui`. `AttachedMeshes[].PresentationTargetIds` remains the
  authoritative per-target filter for specs that actually need multiple modified presentation assets.
- Package validation must assert Appearance count 1 and UI SCS count 1, then prove
  `BP_NPC_011_WA008` resolves from the official game container.

This supersedes both the 2026-07-29 1215 world-SCS configuration and the intermediate 2026-07-30
zero-node world-Blueprint replacement. It does not remove generic world Blueprint presentation support from
the plugin; it requires specs to include such a target only when the Mod has an unavoidable Blueprint-level
delta.

The same checkpoint adds an authoring precondition: an FBX without Armature/skin/weights cannot be accepted as
a SkeletalMesh reimport merely because its filename is newer. A non-destructive repaired derivative may be
used only when its source hash, resulting skeleton, weights, topology, and output hash are recorded. Mesh
build settings are also part of the asset contract; reimport must preserve normal/tangent settings instead of
silently accepting a changed cooked vertex layout.

## 2026-07-31 Nanally merged-mesh Post Process Kawaii stabilization

The merged Nanally mesh uses the game's main-mesh Post Process AnimBP for Kawaii simulation. The
`LinkedInputPose` output is not a reliable initial pose for bones that exist only in the custom `_001`
physics chains: the cooked character AnimBP was authored for the original skeleton and does not initialize
those added bones explicitly. Kawaii initializes its simulation state from the incoming component-space
transforms, so those chains can start from stale or invalid transforms and explode even when the Kawaii
roots, limits, constraints, and runtime flags are correct.

Decision:

- Keep the game's animation pose for all original bones.
- In the generated main-mesh Post Process AnimGraph, resolve a topology-safe reference-pose branch for
  each Kawaii root. Starting at `RootBone` or `AdditionalRootBones`, walk toward the skeleton root while
  the parent has exactly one direct child, and stop before a shared branch such as `Bip001-Pelvis`.
  Blend `LocalRefPose` at those resolved branch roots with `BlendDepth=0`.
- Convert that stabilized local pose to component space, then run the existing Kawaii nodes and convert
  back to local space.
- Do not include original-game physics chains such as `Bn_m_tail_002`; only preset-declared custom roots
  are eligible.
- Attached-mesh `CopyPoseFromMesh` graphs remain unchanged.

Static proof:

- `HasExpectedPostProcessKawaiiChain=true` in `.scratch/kawaii-refpose-stabilized-inspection.json`;
- the graph contains one `LocalRefPose`, one `LayeredBoneBlend`, two Kawaii nodes, and the expected
  `LinkedInputPose -> LayeredBoneBlend <- LocalRefPose -> LocalToComponentSpace` links;
- package preflight rejects a missing stabilizer, a broken link, a non-zero branch depth, or a branch-root
  list that differs from the topology-safe anchors resolved from the target mesh;
- the generated package was rebuilt with zero commandlet errors and copied to the Nanallyshuiyi test mod.

Runtime correction:

- UE4SS confirmed that UI and world both instantiate `ABP_NTE_main_PostProcess_C`, both Kawaii nodes
  contain the intended `_002_001` roots and assets, and all eight roots resolve to valid bone indices.
- The faulty graph reset the simulated `_002_001` roots and descendants but skipped six fixed
  `_001_001` parent anchors. Those anchors are correctly parented to `Bip001-Pelvis` in the skeleton,
  but the source AnimBP leaves their runtime transforms invalid because they are custom bones.
- Kawaii starts simulation at `_002_001`; `_001_001` is not simulated, but its component-space transform
  is the boundary condition for the chain. The reference-pose stabilizer must therefore include that
  fixed anchor without resetting the shared pelvis or any original-game branch.
