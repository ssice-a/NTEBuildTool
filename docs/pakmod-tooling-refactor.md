# Pakmod Tooling Refactor Plan

## Purpose

This document records the refactor plan for bringing the experimental `PhyLab` copy of `NTEBuildTool` back into the standalone plugin without carrying over its coupling and UI fragility.

The target product is a modular editor plugin for fast NTE pakmod production:

- create or update mod `MaterialInstanceConstant` assets from FModel JSON and material recipes;
- generate hotkey and UI driven material-slot visibility toggles;
- cook and package selected assets into `.pak/.utoc/.ucas` with the configured game mount path;
- support the current five verification mod targets in `F:\NTE\PhyLab` through external recipes, toggle setups, and package jobs instead of plugin-source hardcoding;
- keep code modules deep enough that each feature can be tested and changed locally.

## Evidence From The PhyLab Plugin

The `PhyLab` project contains a much larger copy of the plugin at:

```text
F:\NTE\PhyLab\Plugins\NTEBuildTool
```

Only three source files differ from the standalone repository:

```text
NTEBuildTool.Build.cs
Public/NTEBuildTool.h
Private/NTEBuildTool.cpp
```

The analysis/import files are byte-identical:

```text
FModelJsonUtils.*
FModelPhysicsAssetAnalysis.*
FModelPhysicsAssetImporter.*
FModelKawaiiAnimLayerAnalysis.*
```

The main divergence is `NTEBuildTool.cpp`: the standalone file is about 5 KB, while the `PhyLab` copy is about 303 KB. The experimental copy folds material editing, toggle UI generation, post-process animation graph generation, cook/package UI, JSON job handling, and process launch code into one file.

## Current Problems

### 1. One Giant Editor Module

The `PhyLab` copy makes `NTEBuildTool.cpp` the implementation for nearly every feature. This creates poor locality:

- material recipe parsing lives beside Slate UI code;
- package dependency preview lives beside widget graph generation;
- Post Process Anim Blueprint graph creation owns hotkey polling, UI creation, save-game state, and material visibility;
- command-line automation hooks are mixed into editor startup.

The standalone plugin should not accept this shape. Each feature needs its own module with a small interface and contained implementation.

### 2. Runtime Logic Is Over-Coupled To Post Process Anim Blueprint Tick

The experimental toggle system places all runtime behaviour in a generated Post Process Anim Blueprint:

- `BlueprintInitializeAnimation` loads or creates save state and applies default visibility.
- `BlueprintUpdateAnimation` polls `PlayerController.IsInputKeyDown`.
- The same update graph opens the UI, handles drag, toggles groups, saves state, and calls `ShowMaterialSection`.

Observed issue: in some levels and character preview pages, hotkeys do not fire and the UI cannot be opened. The working hypothesis is:

> Some scene contexts do not tick the generated Post Process Anim Blueprint on the target `SkeletalMeshComponent`, or the active preview component is not the replaced mesh/component that owns the generated post-process class.

This hypothesis is not yet proven. The refactor should preserve it as a diagnosis target and provide instrumentation seams to verify:

- whether the replaced mesh is loaded;
- whether the mesh's `PostProcessAnimBlueprint` is the generated class;
- whether `BlueprintInitializeAnimation` ran;
- whether `BlueprintUpdateAnimation` continues to run;
- whether the input polling sees the Up key and the UI hotkey.

### 3. UI Can Trap The Input State

Observed issue: after the generated UI opens, it can fail to close and hotkeys can stop working.

The old graph opens the UI by calling `AddToViewport`, setting `NTE_Toggle_UIVisible=true`, showing the mouse cursor, and setting `SetInputMode_GameAndUIEx`. Closing depends on the same Post Process Anim Blueprint update loop seeing another hotkey edge and then calling `RemoveFromParent` plus `SetInputMode_GameOnly`.

This is fragile:

- the UI has no independent close fallback;
- input-mode changes can affect key polling;
- previous-key-down variables can get stuck;
- the widget reference and visible-state boolean can diverge;
- all of this is hidden inside generated K2 graph code, making diagnosis poor.

### 4. Packaging Depends On A Project Script

The `PhyLab` packaging entry launches:

```text
ProjectDir/Scripts/BuildNteMod.ps1
```

The standalone plugin does not contain that script. Packaging must become plugin-owned, either by executing a script bundled under plugin resources or by copying that script into `Saved/NTEBuildTool/Scripts` before execution.

### 5. Package Selection Is Useful But Too Entangled With UI

The experimental package UI already has useful ideas:

- package profile/job JSON;
- `NeverPackPackagePrefixes`;
- hard dependency preview;
- adding assets, folders, mod folders, and dependencies;
- cook and IoStore output generation.

These behaviours should be retained behind a package pipeline interface. Slate dialogs should only gather options and display validation results.

## Pure Pak Boundary

Pure pak can work when a replaced asset provides a runtime anchor. For character mods, the most reliable anchor is usually:

```text
replaced SkeletalMesh -> generated PostProcessAnimBlueprint -> toggle runtime controller
```

Pure pak cannot guarantee global hotkeys in every game scene. It cannot run logic unless the game loads an asset that references that logic. If a character preview page uses a different mesh, a non-ticking component, or a UI-only rendering path that bypasses the replaced mesh's post-process animation instance, the pure-pak anchor may not execute.

Native DLL work should remain a fallback, not the first design. It becomes necessary only if evidence proves a required scene has no reliable asset reference chain or tickable pure-pak anchor.

## Target Architecture

### Mesh Mod Workspace

The user-facing pakmod workflow should be a `Mesh Mod Workspace`, not three unrelated path-entry dialogs.

Common user model:

```text
select mesh
  -> inspect material slots
  -> create or assign slot materials
  -> configure Toggle Items for slots
  -> review Package Plan
  -> build Package Job
```

The workspace is an orchestration interface. It should call the material module, toggle-runtime module, and package pipeline module through their existing interfaces, while keeping each implementation local to its module.

The workspace should provide these panes or steps:

- **Mesh Overview**: selected mesh package path, current references, current Post Process Anim Blueprint, material slot table, and LOD section-to-slot summary. The workspace displays what is selected; it does not try to prove that the package path is the intended original game path.
- **Material Slots**: one row per material slot with slot index, slot name, current material, material class, package status, and actions. The common action is a Slot Material Operation: choose a target slot, choose a source slot or FModel material JSON, choose replacement textures with asset pickers, then apply and assign the generated MaterialInstanceConstant.
- **Toggle Items**: user-authored UI label, captured hotkey chord, default visible state, and checked material slots. The user should not type standard template asset paths in the common workflow. Default template assets should come from plugin/project settings, with an advanced override.
- **Package Workspace**: editable Package Plan grouped by selected mesh, generated runtime assets, material instances, replacement textures, Skeleton/PhysicsAsset candidates, source-game dependencies, editor-only Material Proxies, and excluded assets. The user can add assets from Content Browser, remove candidates, change inclusion state, and then build.

This does not remove the existing advanced adapters:

- recipe JSON remains the automation adapter for material creation;
- Toggle Setup JSON remains the automation adapter for runtime generation;
- Package Job JSON remains the automation adapter for cook/package.

But the default editor path must be mesh/slot/texture/hotkey driven. If a user has to copy a `/Game/...` path for a normal operation, the workspace interface is too shallow.

### Editor Menu Module

Files:

```text
NTEBuildTool.cpp
NTEBuildTool.h
```

Responsibilities:

- register `Tools > NTE Build Tool` menu entries;
- open file/folder pickers and dialogs;
- call feature modules;
- show notifications and errors.

It should not contain material parsing, K2 graph generation, or cook/package implementation.

### Material Module

Proposed files:

```text
NteMaterialInstanceTool.h/.cpp
NteMaterialInstanceDialog.h/.cpp
```

Responsibilities:

- load FModel material JSON;
- derive parent material paths from FModel export paths;
- create or update `MaterialInstanceConstant` assets;
- show Source Texture Usage: group source texture parameters by the texture asset they currently reference;
- let the user choose a replacement texture once per source texture group, then write that replacement to every parameter in the group;
- create or use editor-only Material Proxy assets when the cooked parent material cannot be loaded in the Mirror Project;
- reset instances for pak-only texture override workflows;
- write raw texture parameter overrides;
- optionally apply scalar/vector overrides;
- optionally bind the created material to a selected mesh slot;
- write material override reports under `Saved/NTEBuildTool/MaterialReports`.

Interface:

- `FNteMaterialInstanceOptions`
- `FNteMaterialInstanceCreateResult`
- `ApplyModMaterialConfigFromFile`
- `CreateOrUpdateModMaterialInstance`

Material-instance creation is about preserving intent, not replacing source game materials. A mod material instance can intend to inherit `/Game/.../M_source`, but the Mirror Project may need a Material Proxy to let the editor save and preview that instance. The package pipeline must not include the proxy parent unless the user explicitly marks it as a real Replaced Asset, which should be unusual.

Texture replacement should be presented by source texture usage rather than as isolated parameter rows. Source materials often bind the same texture to multiple parameters. The material UI should show the source texture once, list all parameters that use it, and allow one replacement texture to override all of those parameters together. Advanced users can still edit individual parameters, but the common workflow is "replace source texture A with mod texture B everywhere A is used."

The recipe format should mirror that workflow:

```json
{
  "SourceTextureOverrides": {
    "/Game/.../source_id": "/Game/.../body_id",
    "/Game/.../source_d": "/Game/.../body_bml",
    "/Game/.../source_m": "/Game/.../body_rmt",
    "/Game/.../source_n": "/Game/.../body_nm"
  },
  "TextureOverrides": {
    "OptionalSingleParameter": "/Game/.../special_case"
  }
}
```

`SourceTextureOverrides` is expanded through Source Texture Usage. If the source material uses `/Game/.../source_id` for both `PM_Diffuse` and `ID_Tex`, one entry writes the replacement texture into both parameters. `TextureOverrides` remains an advanced per-parameter layer and is applied after source-texture expansion.

Current implementation checkpoint:

- `Create Material Instance From FModel Material JSON` opens a source-texture usage dialog.
- The dialog displays each source texture once and lists the material parameters that use it.
- Users can choose one replacement texture per source texture group, and the material module expands it into parameter overrides.
- `Create Material Instance From Recipe JSON` and the `NteMaterialConfig` commandlet still consume the same core material module, so manual and automated recipes stay compatible.
- Raw FModel `MaterialInstanceConstant` export arrays are normalized into the same internal material-parameter object used by recipe JSON. This lets source texture usage work directly on normal FModel exports such as `MI_player_004_lacrimosa_fashion_01.json`.
- `CharacterModSpec.MaterialOperations` now has a plan/write path through `NteCharacterMaterialPlan` and `NteCharacterMaterialWriter`.
- `NteCharacterModSpec` always reports `MaterialPlan`; `-ApplyMaterials` creates or updates material instances through the material module.
- Material plan package seeds include generated material instance paths and replacement textures, including output paths derived from target mesh roots when `OutputMaterialPath` is empty.
- The Character Workspace material slot action now uses the `CharacterModSpec.MaterialOperations` plan/write path in memory. The standalone material menu remains as an advanced direct adapter, not the workspace's core architecture.

Next UX target:

- replace the default material dialog with the Material Slots pane in the Mesh Mod Workspace;
- let users pick the target slot directly from the selected mesh;
- let users pick the source slot directly from the same mesh when they want "slot N inherits slot M";
- keep FModel material JSON selection as an advanced source-material override;
- use asset pickers and "Use Selected Asset" for replacement textures instead of text-only package paths;
- preview grouped Source Texture Usage with thumbnails and parameter names;
- provide one `Apply & Assign` action that creates or updates the material instance, assigns it to the target slot, saves dirty assets, and writes the material report.

### Toggle Runtime Module

Proposed files:

```text
NteMeshToggleTypes.h
NteMeshToggleConfig.h/.cpp
NteMeshToggleBlueprintBuilder.h/.cpp
NteMeshToggleDialog.h/.cpp
```

Responsibilities:

- read/write `NTE.ModToggleSetup` JSON;
- gather and validate material-slot groups;
- generate or update runtime assets:
  - Standard PostProcess Template Runtime assets;
  - widget Blueprint;
  - save-game Blueprint;
- assign the generated Post Process Anim Blueprint to the target `SkeletalMesh` only when requested;
- inspect generated assets for expected graph/variable/widget structure.

The current core path is deliberately narrow: select one `SkeletalMesh`, enter a UI hotkey, define toggle items, and generate runtime assets from a standard template contract. A toggle item is user-authored data: UI label, optional hotkey, default visible state, and one or more material slots. The generator should not adapt arbitrary old project assets or infer behaviour from the current five mods.

Runtime Blueprint generation is not optional in the editor workflow: hotkey bindings, UI button labels, save defaults, and material slot mapping are all patched into generated Blueprint assets. The tool supplies default generated names (`ABP_NTE_ModToggle_PostProcess`, `WBP_NTE_ModToggleMenu`, `BP_NTE_ModToggleSaveGame`) and preserves existing setup names when regenerating.

The deeper target is configuration-driven generation: the user decides the toggle items, labels, hotkeys, and material slots, and the generator creates the required UI entries, variables, save state, and graph logic. Hardcoded group counts in templates are transitional. A standard template may currently define a maximum group capacity, but it should not force the user's setup to use every group. The generator disables unused template groups, and the next deeper target is generating group UI/state/graph entries from config without requiring pre-authored group widgets.

Current implementation checkpoint:

- `NteMeshToggleStandardTemplateModel` owns the Standard Runtime Template Contract naming rules.
- The model scans SaveGame visible variables, Post Process visible/input marker variables, and Widget Blueprint button/label widgets before the builder patches any graphs.
- Compatibility errors now point at the exact missing standard contract piece, such as SaveGame capacity, Post Process visible capacity, a missing Widget button/label, or a missing input marker for a configured hotkey.
- `NteMeshToggleBlueprintBuilder` consumes the model instead of rediscovering template capacity through ad hoc string parsing. This is the seam for the future config-generated UI/state/graph entries.
- `NtePakModAudit` and `NteAssetInspection` also report the same Standard Template Model, so generated assets can be checked through commandlets instead of only by manually opening Blueprint graphs.

Next UX target:

- expose Toggle Items in the Mesh Mod Workspace next to the material slot table;
- capture hotkeys from real key presses instead of requiring Unreal key names like `NumPadOne`;
- let users choose material slots through checkboxes instead of typing `1,13,15`;
- require UI labels at entry time and show those labels in a live button preview;
- source default template assets from plugin/project settings so users do not type three template paths for normal generation;
- keep template path overrides in an Advanced section;
- after generation, show the generated runtime asset paths and whether the Runtime Anchor Mesh now references the generated Post Process Anim Blueprint.

The future deeper runtime target is separating the runtime controller from the post-process animation instance. In that mode, the Post Process Anim Blueprint becomes a thin anchor that ensures a controller exists and has the target `SkinnedMeshComponent`. The controller owns:

- hotkey polling;
- UI show/hide state;
- widget creation and binding;
- save-game persistence;
- `ShowMaterialSection` calls;
- debug/instrumentation counters.

The UI must also include a close control and not depend solely on the hotkey path to recover.

Legacy runtime migration is not part of the core module. If old generated assets need migration, it should be a separate commandlet with its own contract and tests, not a compatibility layer inside `NteMeshToggleBlueprintBuilder`.

### Package Pipeline Module

Proposed files:

```text
NteModPackageTypes.h
NteModPackageJob.h/.cpp
NteModPackagePipeline.h/.cpp
NteModPackageDialog.h/.cpp
Resources/Scripts/BuildNteMod.ps1
```

Responsibilities:

- read/write package profiles and package jobs;
- build an editable Package Plan from selected assets, starting from common shortcuts such as "selected mesh and its dependencies";
- show why each candidate package is present and whether it looks like a mod asset, source game dependency, runtime asset, Skeleton/PhysicsAsset candidate, texture override, or editor-only proxy;
- validate project, engine, game mount, package names, and output directories;
- preview hard dependencies and `NeverPack` conflicts;
- save dirty packages before build;
- launch cook/package as an external process;
- collect logs and outputs under `Saved/NTEBuildTool/Packages/<ModName>`;
- create `.pak/.utoc/.ucas` using `UnrealPak` and the configured game mount path.

The Package Plan is a convenience, not an authority. Users can manually add or remove packages before the Package Job is written. Skeleton and PhysicsAsset packages are not globally included or excluded by policy; they are candidates whose inclusion depends on the mod. The default should favor mod-authored assets and generated runtime assets while making source game dependencies visible but easy to exclude when they are already provided by the base game.

The package job remains the stable interface shared by UI, CLI, and future automation.

Current implementation checkpoint:

- `Build Mod Package` first creates a Package Plan from the Content Browser selection.
- Selected assets, `/mod/` assets, and generated runtime assets are checked by default.
- Source-game hard dependencies and likely editor-only Material Proxy assets are shown but unchecked by default; Skeleton and PhysicsAsset candidates stay visible so the user can include them when the mod requires it.
- The user confirms the plan before the package job JSON is written.
- The commandlet path still consumes a stable Package Job directly, so manual JSON packaging remains supported.

Next UX target:

- replace the flat package candidate list with the Package Workspace pane;
- group candidates by role: selected mesh, generated runtime assets, material instances, replacement textures, Skeleton/PhysicsAsset candidates, mod-authored assets, source-game dependencies, editor-only Material Proxies, and excluded assets;
- show package path, class, size when available, reason, and default inclusion state;
- provide buttons for `Add Selected Assets`, `Add Folder`, `Include Runtime`, `Include Material Slots`, `Include Skeleton/Physics`, `Exclude Source Dependencies`, and `Exclude Proxies`;
- warn before build when generated runtime assets referenced by the selected mesh are not included, or when editor-only Material Proxies are included as if they were real replaced assets;
- show output directory, ModName, GameMount, mode, and final artifact names in the same confirmation window before launching cook/package;
- write and display the Package Job path after build so automation can repeat the same package later.

### Shared Editor Utilities

Proposed files:

```text
NteEditorAssetUtils.h/.cpp
NteJsonFileUtils.h/.cpp
NteNotificationUtils.h/.cpp
```

Responsibilities:

- Content Browser selection helpers;
- asset path normalization;
- asset load/create helpers;
- JSON object load/save;
- common notifications and message dialogs.

## Toggle Runtime Design

### Current Pure-Pak Runtime Chain

```text
Runtime Anchor SkeletalMesh
  -> PostProcessAnimBlueprint = ABP_NTE_ModToggle_PostProcess
      -> standard template EventGraph polls UI and group hotkeys
      -> widget/save-game template classes are patched to generated copies
      -> ShowMaterialSection applies configured material-slot visibility
```

This still needs the Post Process Anim Blueprint to run. If evidence later proves a target scene never runs the post-process anchor, the next fallback is to find another replaced asset anchor in that scene. DLL/hook work is the last resort.

### Standard Runtime Template Contract

The core generator duplicates three user-supplied template assets and patches only this contract:

- `TemplatePostProcessAnimBlueprint` is an `AnimBlueprint` with standard input polling and `ShowMaterialSection` nodes.
- `TemplateWidgetBlueprint` is a `UserWidget` template used by the standard post-process runtime.
- `TemplateSaveGameBlueprint` stores one visible-state variable per toggle group.
- Visible-state variables use `NTE_Toggle_XX_toggle_group_N_Visible`, where `N` is the 1-based setup group ordinal.
- Group input marker variables use `NTE_Toggle_Input_N_...`, where `N` is the 1-based setup group ordinal. Empty user hotkeys are patched to `None`, so the UI button remains usable without accidentally inheriting a template placeholder key.
- UI button widgets use `NTE_Toggle_Button_XX_toggle_group_N`, and their label `TextBlock` widgets use `NTE_Toggle_Button_XX_toggle_group_N_Label`. The label must be inside the corresponding button so it is both visible and clickable. The generator patches the label text from the setup JSON; unlabeled old-style button-only widgets are not valid standard templates.
- If any configured UI label contains non-ASCII text, the generator embeds a Unicode fallback font into the generated Widget Blueprint package and applies it to the template text blocks. This keeps Chinese labels readable without requiring an extra font package in the mod job.
- The template must expose `NTE_Toggle_TitleBarButton` and `NTE_Toggle_WindowPanel` for the standard drag-to-move UI logic. The generated Widget Blueprint owns this drag behavior in its own Tick graph: it polls `NTE_Toggle_TitleBarButton.IsPressed()`, reads viewport mouse position using `Self` as the world context, stores the last mouse position when dragging starts, accumulates mouse delta into a drag offset, and calls `SetRenderTranslation` on `NTE_Toggle_WindowPanel`.
- Title-bar descendant text should be hit-test invisible, generated toggle button labels should be hit-test invisible, and generated buttons should be non-focusable so text and focus state do not block button clicks, hotkeys, or title-bar dragging.
- UI button click handlers in the template must toggle the same visible-state variable used by the group hotkey and save-game state.
- A toggle item may bind multiple material slots. The Post Process template must contain enough `ShowMaterialSection` nodes linked to that group's visible-state variable to cover all configured slots. The generator assigns slot indices to those nodes in graph traversal order, cycling through the configured slots when the same group logic appears more than once, and warns if a configured slot has no node to control it.
- The template group capacity must be greater than or equal to `Groups.Num()` in the setup JSON. Extra template groups are patched to `None` hotkeys, false save defaults, and hidden/disabled Widget Blueprint buttons so a single larger standard template can serve smaller setups.
- Capacity is evaluated by `NteMeshToggleStandardTemplateModel`, which treats contiguous groups from 1 as the safe generated runtime capacity. Non-contiguous or mismatched template groups fail early with a targeted validation message instead of producing half-patched Blueprint assets.
- UI hotkey placeholder nodes use `Slash` and modifier-key pins; those pins are patched from `UIInputChord`.
- The generated runtime asset names default to `ABP_NTE_ModToggle_PostProcess`, `WBP_NTE_ModToggleMenu`, and `BP_NTE_ModToggleSaveGame`; users normally configure templates and toggle items, not output asset names.

Anything outside this contract is a different runtime mode, not something the core generator should guess.

### Future Thin-Controller Runtime

The next deepening target is:

```text
Runtime Anchor SkeletalMesh
  -> thin PostProcessAnimBlueprint
      -> pass-through AnimGraph
      -> Initialize/Update ensures BP_NTE_ModToggleController exists
          -> controller stores TargetComponent
          -> controller ticks independently
          -> controller owns hotkeys, UI, save state, visibility
```

That mode should be implemented as `RuntimeMode=ThinAnchorController` with its own generator and inspector. It should not be faked by requiring `BP_NTE_ModToggleController` in Standard PostProcess Template setups.

### Input Rules

- Group hotkeys use edge-triggered polling.
- Shortcut keys are user-authored data. The tool must validate and preserve them, not infer defaults from slot numbers.
- `player_075_oneir_rpg_level2` currently uses `NumPadEight` for slot 2 and `NumPadTwo` for slot 3.
- UI hotkey remains configurable.
- Toggle item labels are required. Blank labels are rejected before generation because generated UI buttons must remain identifiable.
- Toggle item hotkeys are optional, but configured hotkey chords must be unique and must not collide with the UI show/hide chord.
- UI close button must always exist.
- If the UI is open, both the UI close button and the configured UI hotkey should be able to close it.

### Visibility Rules

Visibility is applied through `USkinnedMeshComponent::ShowMaterialSection` against material slots and resolved render sections for each LOD. The setup must persist slot bindings in JSON for auditability.

## Package Pipeline Design

### Package Job Format

Required fields:

```json
{
  "Format": "NTE.ModPackageJob",
  "Version": 1,
  "ProjectRoot": "F:/NTE/PhyLab",
  "ProjectFile": "F:/NTE/PhyLab/PhyLab.uproject",
  "ProjectName": "PhyLab",
  "EngineRoot": "F:/ue5.6.1/UE_5.6",
  "GameMountName": "HT",
  "ModsDir": "F:/Neverness To Everness/Client/WindowsNoEditor/HT/Content/Paks/Mods",
  "ModName": "example_mod_P",
  "Mode": "CookAndPack",
  "Packages": [],
  "NeverPackPackagePrefixes": []
}
```

Modes:

- `CookAndPack`
- `CookOnly`
- `PackOnly`

The pipeline writes all generated files under:

```text
Saved/NTEBuildTool/Packages/<ModName>
```

Expected artifacts:

```text
<ModName>.job.json
cook.log
pack.log
<ModName>.response.txt
<ModName>.iostore.txt
<ModName>.pak
<ModName>.utoc
<ModName>.ucas
manifest.json
```

## Current Five-Mod Model

### 004 Lacrimosa

Mirror project root:

```text
F:\NTE\PhyLab\Content\Characters\Player\004_lacrimosa
```

Original FModel root:

```text
F:\F-model\Output\Exports\HT\Content\Characters\Player\004_lacrimosa
```

Existing toggle setup:

```text
Content\Characters\Player\004_lacrimosa\mod\Runtime\NTE_ModToggleSetup.json
```

Known runtime assets:

```text
/Game/Characters/Player/004_lacrimosa/mod/Runtime/ABP_NTE_ModToggle_PostProcess
/Game/Characters/Player/004_lacrimosa/mod/Runtime/WBP_NTE_ModToggleMenu
/Game/Characters/Player/004_lacrimosa/mod/Runtime/BP_NTE_ModToggleSaveGame
/Game/Characters/Player/004_lacrimosa/mod/Materials/MI_mod_body
```

After refactor, generated assets must be inspected and updated to the new thin-anchor/controller model.

### Oneir 075 Level 0

FModel root:

```text
F:\F-model\Output\Exports\HT\Content\Maps_4N\Characters\Player\075_oneir_rpg\player_075_oneir_rpg_level0
```

Existing package job:

```text
F:\NTE\PhyLab\NTE_Oneir075Level0ClothModPackage.json
```

The existing job includes the cloth skin, skeleton, `MI_mod_body`, body textures, and cloth/pants diffuse replacements. The refactor must preserve equivalent package coverage.

### Oneir 075 Level 1

Existing toggle setup:

```text
Content\Maps_4N\Characters\Player\075_oneir_rpg\player_075_oneir_rpg_level1\player_075_oneir_rpg_level1_cloth\mod\Runtime\NTE_ModToggleSetup.json
```

Current groups use Up and Down keys. Refactor must inspect and regenerate this setup under the new runtime model.

### Oneir 075 Level 2

FModel root:

```text
F:\F-model\Output\Exports\HT\Content\Maps_4N\Characters\Player\075_oneir_rpg\player_075_oneir_rpg_level2
```

Mirror project root:

```text
F:\NTE\PhyLab\Content\Maps_4N\Characters\Player\075_oneir_rpg\player_075_oneir_rpg_level2
```

Intended setup:

- target cloth mesh under `player_075_oneir_rpg_level2_cloth`;
- slots 0 and 1 are always visible;
- slot 2 toggles with numpad Up (`NumPadEight`);
- slot 3 toggles with numpad Down (`NumPadTwo`);
- slot 2 and 3 material setup follows the task notes: slot 2 and 3 use the material relationship derived from slot 0/1, with material 1 getting its own instance inheriting material 0 as required;
- body texture overrides are the existing `body_bml`, `body_id`, `body_nm`, `body_rmt`;
- pants textures from the sibling pants folder must be included in the package job.
- current inspection shows the `cloth` target is a `StaticMesh`. A Post Process Anim Blueprint cannot be attached to the target itself, and it also cannot be attached to a `Skeleton` asset because a `Skeleton` has no runtime component tick.
- the pure-pak route for this case is to choose a loaded `SkeletalMesh` as `RuntimeAnchorMesh`, keep the level2 `cloth` as `TargetMesh`, and have the runtime controller apply a `StaticMeshVisibilityAdapter` such as material swapping to a hidden/transparent material for slots 2 and 3.

## Final Package Output Layout

The verified package outputs must be copied into the live Mods tree:

```text
F:\Neverness To Everness\Client\WindowsNoEditor\HT\Content\Paks\Mods\安魂曲\lacrimosa_mod_P.*
F:\Neverness To Everness\Client\WindowsNoEditor\HT\Content\Paks\Mods\yly-level0\oneir075_level0_cloth_mod_P.*
F:\Neverness To Everness\Client\WindowsNoEditor\HT\Content\Paks\Mods\yly-level1\nte_mod_P.*
F:\Neverness To Everness\Client\WindowsNoEditor\HT\Content\Paks\Mods\yly-level2\oneir075_level2_mod_P.*
F:\Neverness To Everness\Client\WindowsNoEditor\HT\Content\Paks\Mods\yly-level3\oneir075_level3_mod_P.*
```

The source package lists come from the old `PhyLab` package jobs under `Saved\NTEBuildTool\Packages`. The refactor should normalize their `ModsDir` fields without changing the package coverage unless inspection finds a missing required asset.

### Oneir 075 Level 3

Existing toggle setup:

```text
Content\Maps_4N\Characters\Player\075_oneir_rpg\player_075_oneir_rpg_level3\player_075_oneir_rpg_level3_cloth\mod\Runtime\NTE_ModToggleSetup.json
```

Existing generated assets and material instance must be inspected and regenerated under the new runtime model.

## Verification Strategy

### Code-Level Verification

- Build the plugin in the standalone repository.
- Copy or install the plugin into `F:\NTE\PhyLab\Plugins\NTEBuildTool`.
- Build `PhyLabEditor` with:

```powershell
& 'F:\ue5.6.1\UE_5.6\Engine\Build\BatchFiles\Build.bat' PhyLabEditor Win64 Development -Project='F:\NTE\PhyLab\PhyLab.uproject' -WaitMutex -NoHotReloadFromIDE
```

### Asset-Level Verification

For each of the five mods:

- load or create the material recipe;
- create/update material instances;
- create/update toggle setup;
- inspect generated runtime assets for expected classes, variables, widget tree, and package references;
- write or update package jobs;
- preview package dependencies and `NeverPack` matches;
- cook and package;
- confirm `.pak/.utoc/.ucas` output.

### Runtime Verification

The editor plugin can verify graph structure and asset references. In-game verification still requires manual observation for:

- whether the runtime anchor ticks in the target scene;
- whether character preview pages use the same mesh/component;
- whether hotkeys are visible to `PlayerController`;
- whether the UI close button recovers from input focus changes.

If preview pages still do not run the anchor, record evidence before considering DLL work.

### Package UI Verification

The editor `Build Mod Package` menu entry should:

- collect selected Content Browser assets and recursively selected `/Game` folders;
- save a normalized `NTE.ModPackageJob` JSON outside plugin code;
- launch the same cook/package pipeline as `Build Mod Package From Job JSON`.

## 2026-07-08 Verification Result

This refactor checkpoint has been built and exercised through the `PhyLab` mirror project.

- `PhyLabEditor` builds successfully after syncing the standalone plugin into `F:\NTE\PhyLab\Plugins\NTEBuildTool`.
- `NteMaterialConfig` recreated/updated `player_075_oneir_rpg_level2` slot 2 and slot 3 bindings to `/Game/.../player_075_oneir_rpg_level2_cloth/mod/Materials/MI_mod_body`.
- `NteAssetInspection` confirms level2 `cloth` is still a `StaticMesh`, slots 2 and 3 both use `MI_mod_body`, and the level2 pants texture package loads.
- Existing 004/level1/level3 toggle setups validate and reassign their Post Process Anim Blueprint. They are legacy generated runtime assets and should not define the new core generator contract.
- Level2 toggle setup fails intentionally with a StaticMesh Runtime Anchor error. This is the current blocker for pure-pak hotkey/UI toggles on level2.
- Five package jobs complete through `NteModPackage` and copy verified `.pak/.utoc/.ucas` outputs into the final Mods subfolders.
- `BuildNteMod.ps1` reads package jobs as UTF-8 and shared JSON writes now include a UTF-8 BOM, preventing Windows PowerShell from corrupting non-ASCII paths such as `Mods\安魂曲`.

The generated outputs are:

```text
F:\Neverness To Everness\Client\WindowsNoEditor\HT\Content\Paks\Mods\安魂曲\lacrimosa_mod_P.pak/.utoc/.ucas
F:\Neverness To Everness\Client\WindowsNoEditor\HT\Content\Paks\Mods\yly-level0\oneir075_level0_cloth_mod_P.pak/.utoc/.ucas
F:\Neverness To Everness\Client\WindowsNoEditor\HT\Content\Paks\Mods\yly-level1\nte_mod_P.pak/.utoc/.ucas
F:\Neverness To Everness\Client\WindowsNoEditor\HT\Content\Paks\Mods\yly-level2\oneir075_level2_mod_P.pak/.utoc/.ucas
F:\Neverness To Everness\Client\WindowsNoEditor\HT\Content\Paks\Mods\yly-level3\oneir075_level3_mod_P.pak/.utoc/.ucas
```

## 2026-07-08 Runtime Anchor And Package UI Update

This checkpoint deepens two modules without coupling the current five mod targets into plugin code.

- `NTE.ModToggleSetup` now distinguishes `TargetMesh` from `RuntimeAnchorMesh`. Existing `Mesh` JSON still loads as both fields for backward compatibility.
- `RuntimeAnchorMesh` must be a `SkeletalMesh`; a `Skeleton` cannot host a Post Process Anim Blueprint and cannot tick runtime logic.
- `TargetMesh` can be a `StaticMesh`, but the setup is only runtime-ready when the runtime controller implements the configured `StaticMeshVisibilityAdapter`. The current documented adapter is `MaterialSwap`, which requires a `HiddenMaterial` package for hidden states.
- `ValidateOnly` was added for `NTE.ModToggleSetup` so StaticMesh target/RuntimeAnchorMesh combinations can be checked without saving setup JSON, assigning a Post Process Anim Blueprint, or requiring generated runtime assets to exist.
- The editor menu entry `Build Mod Package` now creates a package job from selected Content Browser assets or folders, asks for the Mods output directory and job JSON path, saves the normalized job, and launches the existing cook/package pipeline.
- `F:\NTE\NTEBuildTool\.scratch\level2_static_target_anchor_validate.json` validates the level2 StaticMesh target with a sample SkeletalMesh runtime anchor and exits with 0 errors. It still warns that `MaterialSwap` needs a `HiddenMaterial` and controller support before the setup is runtime-complete.
- The remaining runtime work is to create standard template assets for normal generation, then later implement `RuntimeMode=ThinAnchorController` as a separate generator/inspector. Existing 004/level1/level3 assets remain legacy generated runtimes until an explicit migration task handles them.

## 2026-07-08 Core Toggle Refactor Constraint

The core toggle workflow is now intentionally scoped to a standard interface:

```text
select SkeletalMesh
  -> enter UI hotkey
  -> add toggle items with label, optional hotkey, default state, and material slots
  -> choose standard PostProcess, Widget, and SaveGame templates
  -> generate setup JSON and runtime assets with default generated names
```

`StandardPostProcessTemplate` is the current default `RuntimeMode`. The generator no longer grows compatibility logic for arbitrary old blueprint names such as `ui_only` or `cycle_a`. A template that does not expose the Standard Runtime Template Contract should fail with a clear message.

Hardcoded package targets, the five current mod paths, and one-off asset-repair rules must stay out of plugin source. They belong in setup JSON, material recipes, package jobs, audit configs, or a future migration commandlet.

## 2026-07-08 Widget-Owned Drag Verification

The standard generator now patches generated Widget Blueprints so UI dragging is not owned by the Post Process Anim Blueprint update loop. This removes one failure path where the UI exists but the ABP tick path stops controlling the panel. Dragging is driven by the title-bar button's `IsPressed()` state rather than by `PlayerController.IsInputKeyDown`, because UI hit testing can consume mouse input before the controller-level query sees it.

Verification run:

- `PhyLabEditor Win64 Development` builds with the mirrored plugin.
- `NteMeshToggle` regenerates `/Game/Characters/Player/004_lacrimosa/mod/Runtime` from `NTE_ModToggleSetup.json`.
- `NteAssetInspection` reports `WBP_NTE_ModToggleMenu` with 31 graph nodes and `InvalidNodeGuid=0`.
- The generated 004 widget has title-bar drag widgets, hit-test invisible title text, non-focusable toggle buttons, and Chinese labels `外套`, `裙子`, `鞋子`, `上衣`.
- `NteModPackage` rebuilds `lacrimosa_mod_P` into `F:\Neverness To Everness\Client\WindowsNoEditor\HT\Content\Paks\Mods\安魂曲` with 0 errors and 0 warnings.

## 2026-07-08 Standard Template Model Checkpoint

The toggle generator now has a dedicated `NteMeshToggleStandardTemplateModel` module. The model centralizes standard group parsing for SaveGame variables, Post Process variables/input markers, and Widget Blueprint button/label widgets. This removes another chunk of naming-rule knowledge from `NteMeshToggleBlueprintBuilder` and makes the next step explicit: generate missing UI/state/graph entries from config behind the model instead of adding asset-specific compatibility code.

`NtePakModAudit` now loads the generated Post Process, Widget, and SaveGame assets for a toggle setup and emits the same model in the audit JSON. It raises targeted findings for missing SaveGame capacity, Post Process visible capacity, Widget buttons/labels, or input markers required by configured hotkeys. `NteAssetInspection` reports the standard groups exposed by individual Blueprint assets for lower-level debugging.

Verification run:

- `RunUAT BuildPlugin -Plugin='F:\NTE\NTEBuildTool\NTEBuildTool.uplugin' -Package='F:\NTE\NTEBuildTool\.scratch\PluginBuild_TemplateModel' -TargetPlatforms=Win64 -StrictIncludes`
- Result: `BUILD SUCCESSFUL`.
- `RunUAT BuildPlugin -Plugin='F:\NTE\NTEBuildTool\NTEBuildTool.uplugin' -Package='F:\NTE\NTEBuildTool\.scratch\PluginBuild_TemplateAudit' -TargetPlatforms=Win64 -StrictIncludes`
- Result: `BUILD SUCCESSFUL`.

## 2026-07-09 Runtime Class Reference Patch

Duplicating a standard runtime template into another folder creates several Blueprint classes with the same short names, such as `WBP_NTE_ModToggleMenu_C` and `BP_NTE_ModToggleSaveGame_C`. The generator must patch references by object identity and generated class path, not by the user-visible class display name.

The Post Process builder now patches four layers after duplicating runtime assets:

- member variable types for `NTE_Toggle_SaveObject` and `NTE_Toggle_Widget`;
- class pins such as `SaveGameClass` and `WidgetType`;
- dynamic cast targets for the generated SaveGame and Widget classes;
- external variable nodes that read or write fields owned by the duplicated SaveGame or Widget classes.

After node reconstruction, the builder repairs orphan pins that UE keeps for data recovery. Linked orphan pins must be reconnected to the matching non-orphan replacement pin before compile; otherwise the compiler reports "pin no longer exists" or "same-named object reference is incompatible" errors.

Verification run:

- `PhyLabEditor Win64 Development` builds with the mirrored plugin.
- `NteMaterialConfig` generated `/Game/Characters/Npc/NPC_Sub/NPC_Sub_073_fm/mod/Materials/MI_mod_npc_sub_073_body_slot1` with `body`, `body_rmt`, and `body_nm` replacing the source texture usage groups.
- `NteMeshToggle` generated `/Game/Characters/Npc/NPC_Sub/NPC_Sub_073_fm/mod/Runtime` with `DefaultVisible=false` for slot 2 and `NumPadOne` as the toggle hotkey. The second regeneration pass finished with `0 error(s)`; the remaining warning is an unused template `Up` input node.
- `NteModPackage` built `npc_sub_073_fm_mod_P` into `F:\Neverness To Everness\Client\WindowsNoEditor\HT\Content\Paks\Mods\NPC_Sub_073_fm`.
- The package response includes only the selected mesh, slot 1 material instance, three body textures, and the three generated runtime Blueprints. It excludes the physics asset, skeleton, and editor-only material proxy parent.

## 2026-07-09 Runtime Blueprint Cook Versioning

The NPC runtime package reproduced a target-game load crash in `AsyncLoading2`:

```text
ObjectSerializationError:
/Game/Characters/Npc/NPC_Sub/NPC_Sub_073_fm/mod/Runtime/WBP_NTE_ModToggleMenu
WidgetTree.NTE_Toggle_ButtonScrollBox: Bad export index .../32
```

The failing job was manually authored with `Unversioned=true`, while the known-good 004 lacrimosa package used `Unversioned=false`. Runtime Widget Blueprints are therefore treated as incompatible with unversioned package serialization for this target game until proven otherwise.

Packaging now normalizes cook options when a job contains generated runtime Blueprint packages:

- C++ job creation/launch disables `bUnversioned` before saving the normalized work-root job.
- `BuildNteMod.ps1` also disables `-Unversioned` when run directly with generated runtime Blueprint packages.
- Ordinary mesh/material/texture packages are unaffected.

## 2026-07-10 Settings And Workspace Shell

This checkpoint starts moving common usage away from disconnected path-entry dialogs without embedding current mod targets in plugin code.

- `UNteBuildToolSettings` centralizes project defaults for `GameMountName`, default Mods output directory, FModel export root, and standard toggle runtime templates.
- Package job creation and the package menu now read defaults from settings instead of assuming a hardcoded mount/output location.
- The toggle setup dialog pre-fills standard template paths from settings while keeping the JSON adapter available for automation.
- `Open Mesh Mod Workspace` now provides the first mesh-centered orchestration surface: selected mesh summary, material slot list, settings summary, and entry points into material, toggle, and package workflows.
- The material source-texture dialog can pre-fill the selected mesh path and fill replacement texture paths from the currently selected Content Browser asset, reducing manual `/Game/...` path entry.
- Package Plan classification now exposes Skeleton and PhysicsAsset dependencies as explicit user-controlled candidate kinds.
- Documentation now treats Source Asset inspection as analysis-side tooling, not a prerequisite in the main creation chain.
- Package Plan wording has been corrected: Skeleton and PhysicsAsset packages are user-controlled candidates, not globally excluded or included by policy.
- Source-game reference scans must start from the full installed game root `F:\Neverness To Everness\Client\WindowsNoEditor`, not only `...\HT\Content\Paks`. Patch, tag-patch, and mod container folders can contain live assets, so a negative result from the base `Paks` folder alone is not enough to conclude that an asset or reference chain is absent.

Verification run:

```powershell
& 'F:\ue5.6.1\UE_5.6\Engine\Build\BatchFiles\RunUAT.bat' BuildPlugin -Plugin='F:\NTE\NTEBuildTool\NTEBuildTool.uplugin' -Package='F:\NTE\NTEBuildTool\.scratch\PluginBuild_WorkspaceSettings' -TargetPlatforms=Win64 -StrictIncludes
```

Result: `BUILD SUCCESSFUL`.

## Git Strategy

Use sparse milestone commits:

1. Documentation and architecture skeleton.
2. Modular code implementation builds.
3. Five-mod asset/job regeneration and package verification.

Do not commit every small edit. Do not commit generated build intermediates or cooked outputs.

## 2026-07-11 Character Mod Workspace Direction

The tooling direction has moved from a mesh-only workflow to a full `Character Mod Workspace`.

The user's target is full NTE character model replacement with:

- material freedom through FModel material JSON parsing, source texture usage, and generated material instances;
- runtime switching through hotkeys and UI buttons for material slots, attached mesh visibility, material swaps/parameters, and future morph or animation actions;
- physics freedom through editable UE-side KawaiiPhysics presets for custom main and attached meshes;
- automated package planning and cook/IoStore packaging;
- simple, precise, attractive editor UI backed by modular, testable code and minimal hardcoding.

The new design center is `CharacterModSpec`, a single source of truth for one character mod workspace. The UI edits the spec; deep modules validate and transform it. The spec should drive:

- target appearance resolution from `DT_AppearanceData`, `MeshAsset_PlayerXXX`, and `PlayerUIShow_XXX`;
- main mesh replacement;
- multiple attached mesh definitions, including socket, transform, runtime AnimBP, and UI preview sync;
- material-slot operations and material instance generation;
- runtime action groups for hotkey/UI switching;
- Kawaii preset import/edit/apply;
- Package Plan and Package Job generation.

The preferred attached-mesh path now follows source-game evidence instead of the old PostProcess-only template route:

```text
HTPlayerAppearance / MeshAsset_PlayerXXX
  -> FashionMeshData: main mesh + main AnimBP
  -> ArrayFashionAttachedMeshData: attached mesh + socket + transform + attached AnimBP

PlayerUIShow_XXX
  -> SCS child HTSkeletalMeshComponentBudgeted components for preview

ABP_<AttachName>_Runtime
  -> AnimGraph: CopyPose / Kawaii / Output Pose
  -> EventGraph: hotkey/UI/save/apply runtime actions when needed
```

The 071 Chaos mask investigation remains useful evidence, but it should not be overgeneralized. The native mask chain is a gameplay/state path:

```text
Buff_Chaos_KeepMask
  -> GameplayCue.Display.Chaos.KeepMask
  -> GC_Chaos_KeepMask
  -> player_071_Chaos.PlayFaceMaskFadeIn / PlayFaceMaskFadeOut
```

That means it demonstrates a source-game state-driven toggle, not a reusable mesh-local blueprint pattern for mods.

The stronger attached-mesh evidence comes from source-game appearance assets:

- `MeshAsset_Player004_lacrimosa_fashion4` contains attached eardrop, ribbon, and tail entries with sockets and attached AnimBPs.
- `MeshAsset_Player010_fashion3` contains attached hair data with an attached AnimBP and mobile/UI variants.
- `PlayerUIShow_004_fashion4` and `PlayerUIShow_010_fashion3` duplicate preview child components attached under the main `Mesh`.

Implementation should therefore add an Appearance Assembly module that can write both runtime appearance data and UI preview data from one spec. The tool should not require users to manually duplicate attachment definitions.

PostProcess template runtime is now considered legacy/advanced. It can remain until the new modules cover its use cases, but new work should not deepen that dependency. Once a Character Mod Workspace slice supersedes old PostProcess-only logic or hardcoded test code, it should be removed rather than kept as permanent compatibility.

KawaiiPhysics remains schema-sensitive. The mirror project must provide an NTE-compatible `/Script/KawaiiPhysics` layout before final Kawaii assets are trusted. The current game usmap is the serialization authority, and Kawaii parameters should be editable in UE while optionally seeded from source-game AnimBP JSON.

The PRD for this work is tracked at:

```text
.scratch/character-mod-workspace/PRD.md
```

The executable implementation plan is tracked at:

```text
.scratch/character-mod-workspace/implementation-plan.md
```

The accepted architecture decision is tracked at:

```text
docs/adr/0001-character-mod-workspace.md
```

The next engineering slice is the `/Script/HTGame` schema/stub and appearance assembly writer. It should use source-game field evidence from `MeshAsset_Player004_lacrimosa_fashion4` and related UIShow assets, not guessed class layouts.

## 2026-07-11 CharacterModSpec package checkpoint

The first Character Mod Workspace vertical slice is now compiling and smoke-tested:

- The plugin contains a runtime `HTGame` module so Mirror Project assets can save with `/Script/HTGame` class paths.
- Minimal stubs exist for `HTPlayerAppearance`, fashion mesh data, fashion attached mesh data, and `HTSkeletalMeshComponentBudgeted`.
- `NteCharacterModSpec -ApplyAppearance` can write `HTPlayerAppearance` assets from `CharacterModSpec`.
- `NteAssetInspection` reports `HTPlayerAppearance` fields, including `FashionMeshData`, attached mesh arrays, `MeshComponentOwnedTags`, and Blueprint SCS nodes.
- A 004 Lacrimosa attached-mesh smoke spec generated `/Game/Characters/Player/004_lacrimosa/mod/Generated/MeshAsset_Player004_NTE_AttachedSmoke` and inspection confirmed it loads as `HTPlayerAppearance`.
- The generated asset contains one attached mesh entry on socket `Bip001-Head` with tags `NTE.ToggleTarget` and `NTE.Attached.smoke_attach`.
- The appearance writer avoids loading future packages before creation, preventing the `LoadPackage: SkipPackage` warning that previously appeared when probing not-yet-generated package paths.
- `BuildPackagePlanFromCharacterModSpec` now builds package candidates directly from `CharacterModSpec`; seed candidates report the reason `from CharacterModSpec`.
- `CreateModPackageJobFromCharacterModSpec` now writes `NTE.ModPackageJob` JSON from `CharacterModSpec.Package`; Character package jobs set `RequiresHTGameStub=true` and keep `Unversioned=false`.
- The Character Workspace Package button now follows the `CharacterModSpec` package path instead of selected Content Browser assets.
- `Config/FilterPlugin.ini` excludes `.scratch`, build outputs, binaries, intermediate files, saved data, and DDC from plugin package builds.
- `NteCharacterModSpec -ApplyAppearance -BuildPackage` produced pak/utoc/ucas for `lacrimosa004_character_attached_smoke_P` in the game Mods directory.
- `NteCharacterRuntimeActionPlan` now exposes the next runtime slice from the same `CharacterModSpec.RuntimeActions` model:
  - action host grouping by main/attached mesh and AnimBP path;
  - tag-based target lookup data from `TargetComponentTags` plus `MeshComponentOwnedTags`;
  - shared Widget/SaveGame Blueprint output paths under `/mod/Runtime`;
  - first-slice Blueprint support flags for `AttachedMeshVisibility` and `MaterialSlotVisibility`.

Mirror Project requirement:

- Character packages cook generated `HTPlayerAppearance` assets that import `/Script/HTGame`. Therefore the Mirror Project must let the `NTEBuildTool` plugin load for the Game target. If the `.uproject` plugin entry uses `TargetAllowList`, it must include `Game` or be removed.

Remaining work:

- validate `PlayerUIShow` SCS sync against a real existing UIShow Blueprint;
- implement generated runtime action assets from `NteCharacterRuntimeActionPlan`, starting with `AttachedMeshVisibility` and `MaterialSlotVisibility`;
- build the Kawaii preset editor/data model;
- replace remaining mesh-only UI fragments with spec-first Character Workspace panels.

## 2026-07-11 CharacterModSpec material-operation checkpoint

The material slice is now on the CharacterModSpec main path.

Implemented:

- `NteCharacterMaterialPlan` resolves each `MaterialOperations` entry to main/attached target mesh data, slot index, source material JSON, parent material path, output material path, and normalized source texture overrides.
- Empty `ParentMaterialPath` is derived from the FModel material export path.
- Empty `OutputMaterialPath` is derived under the target character root, for example `/Game/Characters/Player/004_lacrimosa/mod/Materials/...`.
- `NteCharacterMaterialWriter` consumes the plan and calls the existing material module, so standalone recipe JSON, the material dialog, and CharacterModSpec all share the same material-instance code path.
- `NteCharacterModSpec -ApplyMaterials` writes material instances and reports per-operation output path, report filename, applied count, texture override count, source texture usage, missing textures, unmatched source texture overrides, and editor-only proxy creation.
- Raw FModel material export arrays are now accepted by the material module and normalized into `Textures`, `Scalars`, `Colors`, and `Switches` parameter sections.
- `BuildPackagePlanFromCharacterModSpec` merges material-plan package seeds, so generated MIs and replacement textures enter package candidates even when the output MI path is derived.
- Character Workspace material actions apply through the same plan/writer path as `NteCharacterModSpec -ApplyMaterials`.
- Character Workspace now has a spec-first persistence path: load/save `CharacterModSpec` JSON, edit core spec fields, show existing `MaterialOperations`, and upsert material slot actions back into the saved spec file.

Verified:

- `PhyLabEditor Win64 Development` builds after syncing the plugin mirror, including the Workspace spec persistence/upsert UI.
- `RunUAT BuildPlugin -StrictIncludes` succeeds for `.scratch/PluginBuild_CharacterWorkspaceSpecPersistence`.
- `NteCharacterModSpec` plan-only reports still pass for:
  - `.scratch/character-mod-workspace/examples/071_chaos_character_mod_spec.example.json`;
  - `.scratch/character-mod-workspace/004_lacrimosa_runtime_actions_validation.spec.json`.
- Safe material apply using real FModel JSON generated `/Game/Characters/Player/004_lacrimosa/mod/Materials/MI_mod_MI_player_004_lacrimosa_fashion_01` without assigning it to the mesh slot.
- Source texture override apply expanded `/Game/Characters/Player/004_lacrimosa_fashion4/ter/cloth/T_player_004_lacrimosa_fashion1_1_d` to the `BaseColor` parameter and wrote `/Game/Characters/Player/004_lacrimosa/T_player_004_lacrimosa_02_d`.
- The material report confirms `TextureOverrides=1`, `SourceTextureOverrideGroups=1`, and `AssetLoads=true`.

Representative reports:

- `.scratch/character-mod-workspace/004_lacrimosa_apply_materials_safe.report.json`
- `.scratch/character-mod-workspace/004_lacrimosa_apply_materials_texture_override.report.json`

## 2026-07-12 Character Workspace runtime-action UI checkpoint

The Character Workspace no longer routes its Runtime Action button through the legacy mesh-only PostProcess toggle generator.

Implemented:

- Added a first-slice Runtime Action dialog for `CharacterModSpec.RuntimeActions`.
- Material-slot rows now expose `Add Toggle`, prefilled as a `MaterialSlotVisibility` action for that slot.
- The generic Runtime Action button can add/update a `MaterialSlotVisibility` action by manually entering material slots.
- Runtime actions are upserted by `Id`, validated through `ValidateCharacterModSpec`, and saved back to the active spec JSON when available.
- Character Workspace Package action saves confirmed package settings back into the active `CharacterModSpec` before creating or launching a package job.
- The legacy `Manage Mesh Toggle Setup` menu remains available as a standalone advanced adapter, but it is no longer the Character Workspace runtime path.

Verified:

- `PhyLabEditor Win64 Development` builds after syncing the plugin mirror.
- `RunUAT BuildPlugin -StrictIncludes` succeeds for `.scratch/PluginBuild_CharacterWorkspaceRuntimeActionUi`.
- `RunUAT BuildPlugin -StrictIncludes` succeeds for `.scratch/PluginBuild_CharacterWorkspacePackageSpecSave`.

## 2026-07-12 CharacterModSpec runtime-action writer checkpoint

The runtime-action module now has a first asset-writing slice. This is intentionally not the final execution graph yet; it proves that `CharacterModSpec.RuntimeActions` can generate stable, package-reachable runtime Blueprint assets without returning to the legacy PostProcess template generator.

Implemented:

- Added `NteCharacterRuntimeActionWriter`.
- `NteCharacterModSpec -ApplyRuntimeActions` consumes `NteCharacterRuntimeActionPlan`.
- The writer creates or updates:
  - a generated SaveGame Blueprint under the planned runtime root;
  - a generated Widget Blueprint under the planned runtime root;
  - each host AnimBP referenced by the runtime-action plan.
- Generated assets store auditable action data variables, including the condensed action plan JSON, action count, action ids, labels, types, target mesh ids, material slots, and default enabled states.
- Runtime action package seeds now include the generated SaveGame, Widget, and host AnimBP packages, so `BuildPackagePlanFromCharacterModSpec` and the commandlet report can see them before packaging.
- First-create load noise is avoided by checking in-memory packages and `FPackageName::DoesPackageExist` before loading future Blueprint package paths.

Verified:

- `PhyLabEditor Win64 Development` builds after syncing the plugin mirror.
- `NteCharacterModSpec -ApplyRuntimeActions` on `.scratch/character-mod-workspace/004_lacrimosa_runtime_actions_validation.spec.json` updates the runtime assets with 0 writer errors and 0 writer warnings.
- A temporary `RuntimeCreateProbe` spec created new SaveGame, Widget, and AnimBP Blueprint assets with no missing-package load warnings; the temporary uassets were removed after validation.
- `NteAssetInspection` using `.scratch/character-mod-workspace/004_lacrimosa_runtime_actions_assets.txt` confirms the formal runtime SaveGame, Widget, and AnimBP assets all load.
- `RunUAT BuildPlugin -StrictIncludes` succeeds for `.scratch/PluginBuild_CharacterRuntimeActionWriter_Strict3`.

Still open:

- Build the actual hotkey/UI/save/apply execution graph.
- Connect `MaterialSlotVisibility` to `ShowMaterialSection` for skinned mesh targets.
- Connect `AttachedMeshVisibility` to component visibility using `MeshComponentOwnedTags` / action target tags.
- Generate Widget buttons and click bindings from the same action data.

## 2026-07-12 CharacterModSpec runtime-action execution-graph checkpoint

The runtime-action module now has a first executable EventGraph slice. It still does not claim final UI/save/controller coverage, but it proves that `CharacterModSpec.RuntimeActions` can produce compiled host AnimBPs with real hotkey-driven visibility logic.

Implemented:

- `NteCharacterRuntimeActionPlan` now carries each host mesh path so generated host AnimBPs can derive their Skeleton from the target `USkeletalMesh`.
- `NteCharacterRuntimeActionWriter` creates new host AnimBPs through `UAnimBlueprintFactory` with `TargetSkeleton` and preview mesh set from the host mesh. Existing generated host AnimBPs are repaired if their Skeleton or preview mesh is missing.
- Host AnimBP EventGraphs now generate hotkey polling for first-slice owning-component actions:
  - `MaterialSlotVisibility` uses `APlayerController::WasInputKeyJustPressed`, modifier-key checks, toggles the per-action enabled variable, and calls `USkinnedMeshComponent::ShowMaterialSection` for configured slots on LOD 0.
  - `AttachedMeshVisibility` toggles the per-action enabled variable and calls `USceneComponent::SetVisibility` on the owning component with child propagation enabled.
- Host AnimBPs now also generate `BlueprintInitializeAnimation` apply branches, so `DefaultEnabled=false` actions are applied before the first hotkey press.
- Shared host AnimBP paths are detected and skip execution graph generation with one deduplicated warning, because `GetOwningComponent` would otherwise be ambiguous across multiple components using the same AnimBP.

Verified:

- `PhyLabEditor Win64 Development` builds after syncing the plugin mirror.
- Fresh first-create graph probe deletes and regenerates `/Game/Characters/Player/004_lacrimosa/mod/RuntimeGraphProbe` with no AnimBP missing-Skeleton compile errors.
- `NteCharacterModSpec -ApplyRuntimeActions` on `.scratch/character-mod-workspace/004_lacrimosa_runtime_actions_graph_probe.spec.json` generates hotkey execution graphs for:
  - `toggle_main_slot0`: `Ctrl+M` -> `ShowMaterialSection`;
  - `toggle_smoke_attach`: `H` -> `SetVisibility`.
- `NteAssetInspection` using `.scratch/character-mod-workspace/004_lacrimosa_runtime_actions_graph_probe_assets.txt` loads the generated SaveGame, Widget, main host AnimBP, and attached host AnimBP with 0 errors / 0 warnings and confirms the generated initialize/update graph calls.
- Shared-ABP validation on `.scratch/character-mod-workspace/004_lacrimosa_runtime_actions_validation.spec.json` succeeds with the expected shared-host skip warning.
- `RunUAT BuildPlugin -StrictIncludes` succeeds for `.scratch/PluginBuild_CharacterRuntimeActionGraph_Strict` and `.scratch/PluginBuild_RuntimeActionInitialApply_Strict`.

Still open:

- Generate Widget buttons/click handlers from the same action data.
- Generate SaveGame load/save and initial apply state.
- Generate OwnerComponentByTags lookup for actions where the host mesh and target mesh differ.
- Generate CopyPose/Kawaii AnimGraph content for attached meshes instead of only testing EventGraph execution.
