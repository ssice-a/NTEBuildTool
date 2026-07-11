# Character Mod Workspace implementation plan

Status: in-progress

## Goal

Build the full `Character Mod Workspace` as the default NTE character replacement workflow.

The workspace must be driven by one `CharacterModSpec` and keep code/UI concepts aligned:

- target appearance and UI preview;
- main mesh replacement;
- multiple attached meshes;
- material-slot operations and material instance creation;
- hotkey/UI runtime actions;
- UE-side Kawaii preset editing/application;
- package plan, package job, cook, and practical game-package verification.

Old mesh-only and hardcoded experimental paths may be removed once their responsibilities are covered by the new modules.

## Non-negotiable architecture

`CharacterModSpec` is the single source of truth. Slate UI edits it; commandlets validate it; feature modules transform it. No feature module should require users to maintain duplicate hand-written data.

```text
CharacterModSpec
  -> Appearance Assembly
       -> HTPlayerAppearance / MeshAsset
       -> PlayerUIShow preview SCS components
  -> Material Operations
       -> material recipes / source texture usage / slot assignment
  -> Runtime Actions
       -> hotkeys and UI buttons from the same action model
       -> generated AnimBP / Widget / SaveGame assets as needed
  -> Kawaii Presets
       -> UE-edited parameters seeded from game JSON when possible
  -> Package Pipeline
       -> editable Package Plan
       -> Package Job
       -> cook + IoStore outputs
```

The preferred attached-mesh route follows source-game appearance assets:

```text
HTPlayerAppearance
  FashionMeshData
    CharacterMesh
    AnimInstance
  ArrayFashionAttachedMeshData[]
    CharacterMesh
    AnimInstance
    MobileAnimInstance
    SocketName
    MeshComponentOwnedTags
    RelativeLocation
    RelativeRotation
    RelativeScale3D

PlayerUIShow_*
  SCS child HTSkeletalMeshComponentBudgeted components
```

The 071 Chaos mask chain remains evidence for native gameplay/state toggles, not the mod architecture:

```text
Buff_Chaos_KeepMask
  -> GameplayCue.Display.Chaos.KeepMask
  -> GC_Chaos_KeepMask
  -> player_071_Chaos.PlayFaceMaskFadeIn / PlayFaceMaskFadeOut
```

## Module slices

### Slice 1: schema and evidence lock

Deliverables:

- `/Script/HTGame` stub module in the plugin for mirror-project cook/build only.
- Minimal reflected classes/structs required by current evidence:
  - `UHTPlayerAppearance`;
  - `FHTFashionMeshData`;
  - `FHTFashionAttachedMeshData`;
  - `UHTSkeletalMeshComponentBudgeted`.
- Field names and broad types aligned to FModel JSON/usmap evidence.
- Schema/evidence report kept in `.scratch/character-mod-workspace/`.

Verification:

- `RunUAT BuildPlugin -StrictIncludes`.
- `PhyLabEditor` build after plugin sync.
- Commandlet report still validates the example spec.

### Slice 2: appearance assembly writer

Deliverables:

- `FNteAppearanceAssemblyWriter` consumes `FNteAppearanceAssemblyPlan`.
- Writes or updates `MeshAsset_PlayerXXX`:
  - main `FashionMeshData`;
  - `ArrayFashionAttachedMeshData`.
- Writes or updates `PlayerUIShow_XXX` preview components from the same attachment list.
- Commandlet flag:
  - plan-only by default;
  - `-ApplyAppearance` writes assets.

Verification:

- Plan-only report unchanged.
- Apply mode creates loadable assets in `F:\NTE\PhyLab`.
- Reopened assets preserve the expected fields.

### Slice 3: workspace UI shell cleanup

Deliverables:

- Rename and simplify the default editor entry to `Character Mod Workspace`.
- UI sections mirror the spec:
  - Appearance;
  - Meshes and Attachments;
  - Materials;
  - Runtime Actions;
  - Kawaii;
  - Package.
- Avoid raw path entry as the normal route; keep advanced JSON adapters.

Verification:

- UI opens with selected/current spec.
- The UI summary and commandlet report agree.

### Slice 4: material operations integration

Deliverables:

- Material slot operations inside `CharacterModSpec` call the existing material module.
- Source texture usage remains the main UX model.
- Material proxies remain editor-only unless explicitly promoted.
- Package candidates include generated MIs and replacement textures.

Verification:

- Existing material recipe commandlet remains working.
- Character spec report lists material outputs and textures as package seeds.

### Slice 5: runtime action generation

Deliverables:

- One runtime action model drives both hotkeys and UI buttons.
- Supported first actions:
  - material slot visibility;
  - whole attached mesh visibility;
  - material swap;
  - scalar/vector parameter hooks as data model.
- Attached mesh AnimBP is the preferred owner when a custom attached mesh needs runtime logic.
- Old PostProcess template runtime remains only as a legacy adapter until superseded.

Verification:

- Duplicate hotkeys, blank labels, missing targets, and missing slots fail validation.
- Generated UI and hotkeys are derived from the same spec entries.

### Slice 6: Kawaii preset editor/application

Deliverables:

- Kawaii preset data model stores editable root bones, excluded bones, limits, collision references, and source-game seed JSON path.
- UE-side editing is the primary workflow.
- Source-game AnimBP JSON can seed parameters when schema-compatible.
- The NTE usmap remains the schema authority.

Verification:

- Import preserves known editable fields.
- Schema mismatch produces warnings/errors before cook.

### Slice 7: package pipeline integration

Deliverables:

- `BuildPackagePlanFromCharacterModSpec`.
- Package job creation from the spec.
- Runtime Blueprint packages keep versioned cook behavior.
- Package UI groups candidates by role and reason.

Verification:

- BuildPlugin.
- Commandlet package plan report.
- Cook/package one real attached-mesh example.

### Slice 8: practical example verification

Deliverables:

- A real example based on 071/004 evidence:
  - main mesh replacement;
  - at least one attached mesh;
  - material operation;
  - package job.
- Output copied to game Mods tree.
- Practical test notes recorded.

Verification:

- Theoretical report: 0 errors.
- Cook/package: 0 errors.
- In-game observations recorded separately because editor cannot prove every live-scene tick path.

## Clean-code rules while implementing

- Domain structs and validation stay independent from Slate.
- UI modules collect/edit data only; they do not own asset-writing policy.
- Hardcoded target characters, one-off test paths, and migration hacks stay out of source.
- When replacing an old path, delete obsolete branches rather than leaving permanent compatibility mazes.
- Commit only meaningful milestones:
  1. docs/spec/schema skeleton;
  2. compilable HTGame stubs;
  3. first appearance writer;
  4. first package-integrated example.

## Current checkpoint: 2026-07-11

Completed:

- Slice 1 schema/stub foundation:
  - plugin runtime module `HTGame`;
  - `/Script/HTGame.HTPlayerAppearance`;
  - `/Script/HTGame.HTSkeletalMeshComponentBudgeted`;
  - minimal fashion mesh/attached mesh reflected structs.
- First part of Slice 2:
  - `FNteAppearanceAssemblyWriter`;
  - `NteCharacterModSpec -ApplyAppearance`;
  - MeshAsset write/update for `FashionMeshData` and `ArrayFashionAttachedMeshData`;
  - SCS sync code path for already-loadable `PlayerUIShow` Blueprints;
  - `NteAssetInspection` detailed reports for generated `HTPlayerAppearance` assets.
- First part of Slice 7:
  - `BuildPackagePlanFromCharacterModSpec`;
  - `CreateModPackageJobFromCharacterModSpec`;
  - Workspace Package action now uses the `CharacterModSpec` package path instead of selected Content Browser assets;
  - `RequiresHTGameStub` package-job preflight protects Character packages from Mirror Project `TargetAllowList=["Editor"]` cook failures;
  - `Config/FilterPlugin.ini` excludes `.scratch`, build outputs, binaries, intermediate files, and cache folders from `RunUAT BuildPlugin`.
- First part of Slice 4:
  - `NteCharacterMaterialPlan`;
  - `NteCharacterMaterialWriter`;
  - `NteCharacterModSpec -ApplyMaterials`;
  - material plan package seeds for generated MIs and replacement textures;
  - raw FModel `MaterialInstanceConstant` JSON array normalization in the material module.

Verified:

- `RunUAT BuildPlugin -StrictIncludes`.
- `PhyLabEditor` build after plugin sync.
- `NteCharacterModSpec` plan-only report for the 071 example.
- 004 Lacrimosa smoke `-ApplyAppearance` generated a loadable `HTPlayerAppearance` asset.
- 004 Lacrimosa smoke attached mesh wrote one `ArrayFashionAttachedMeshData` entry:
  - socket `Bip001-Head`;
  - attached mesh anim class `/Game/Characters/Player/004_lacrimosa/mod/Runtime/ABP_NTE_ModToggle_PostProcess`;
  - component tags `NTE.ToggleTarget` and `NTE.Attached.smoke_attach`.
- `BuildPackagePlanFromCharacterModSpec` produces package candidates with `from CharacterModSpec` reasons.
- `CreateModPackageJobFromCharacterModSpec` and `NteCharacterModSpec -WritePackageJob` produce a package job from spec package settings.
- Full `NteCharacterModSpec -ApplyAppearance -BuildPackage` smoke test produced pak/utoc/ucas in the game Mods directory for `lacrimosa004_character_attached_smoke_P`.
- `NtePakModAudit` on the generated package job reports 0 errors / 0 warnings.
- `NteCharacterModSpec` plan-only reports include `MaterialPlan` for 071 and 004 runtime-action specs.
- `NteCharacterModSpec -ApplyMaterials` safely generated a material instance from real FModel JSON without assigning it to the mesh slot.
- `NteCharacterModSpec -ApplyMaterials` with `SourceTextureOverrides` expanded a source texture group to the `BaseColor` parameter and wrote the replacement texture override.

Latest verification reports:

- `.scratch/character-mod-workspace/004_lacrimosa_attached_mesh_preflight.report.json`
- `.scratch/character-mod-workspace/004_lacrimosa_attached_mesh_build_package_preflight.report.json`
- `.scratch/character-mod-workspace/004_lacrimosa_attached_mesh_preflight_inspection.json`
- `.scratch/character-mod-workspace/004_lacrimosa_attached_mesh_package_audit_preflight.json`
- `.scratch/character-mod-workspace/004_lacrimosa_apply_materials_safe.report.json`
- `.scratch/character-mod-workspace/004_lacrimosa_apply_materials_texture_override.report.json`

Important environment requirement:

- The Mirror Project must expose this plugin to the Game target while cooking Character packages. The plugin's runtime `HTGame` module supplies schema stubs for generated `HTPlayerAppearance` assets. In `PhyLab.uproject`, `NTEBuildTool` must not be restricted to `TargetAllowList=["Editor"]`; either remove the allow-list or include `Game`.

Next action:

Continue with the remaining vertical slices:

1. validate `PlayerUIShow` SCS sync against a real existing UIShow Blueprint once the Mirror Project contains one;
2. implement the runtime action generator that uses `MeshComponentOwnedTags` to find attached mesh/material-slot targets;
3. expose `MaterialOperations` in the Character Workspace UI as slot-centered rows rather than raw JSON/path entry;
4. design and implement the UE-side Kawaii preset editor/data model;
5. replace the remaining mesh-only UI fragments with spec-first Character Workspace panels;
6. keep using the 004 smoke spec as the minimal package regression test.

Runtime action checkpoint:

- `CharacterModSpec.RuntimeActions` now has a typed target/action schema for material slot visibility, attached mesh visibility, material swap, scalar/vector parameters, and morph targets.
- Runtime validation now requires tag-based lookup data for attached mesh visibility, so later Blueprint generation can stay data-driven instead of guessing component names.
- `NteCharacterRuntimeActionPlan` now converts `RuntimeActions` into a commandlet-visible plan with host AnimBP grouping, target lookup mode, target component tags, generated Widget/SaveGame paths, and first-slice Blueprint support flags.
- Runtime asset roots now prefer explicit `RuntimeAnimBlueprintPath` directories and otherwise fall back to `/mod/Runtime`, so generated Widget/SaveGame/runtime assets do not drift into `/mod/Generated`.
- `.scratch/character-mod-workspace/004_lacrimosa_runtime_actions_validation.spec.json` is the focused spec for this data-layer regression.
- `.scratch/character-mod-workspace/004_lacrimosa_runtime_actions_plan.report.json` and `.scratch/character-mod-workspace/071_chaos_runtime_actions_plan.report.json` are the focused reports for the runtime-action planning regression.

Kawaii preset checkpoint:

- `CharacterModSpec.KawaiiPresets` now has structured fields for UE-edited root bones, additional root bones, physics settings, collision limits, limit/physics/bone-constraint assets, gravity/wind, ignore bones, source node names, and schema status.
- Package seeds include Kawaii asset references, but no AnimBP Kawaii node generation is claimed yet.
- `.scratch/character-mod-workspace/004_lacrimosa_kawaii_preset_validation.spec.json` is the focused spec for this data-layer regression.
