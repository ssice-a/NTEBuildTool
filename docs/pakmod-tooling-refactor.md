# Pakmod Tooling Refactor Plan

## Purpose

This document records the refactor plan for bringing the experimental `PhyLab` copy of `NTEBuildTool` back into the standalone plugin without carrying over its coupling and UI fragility.

The target product is a modular editor plugin for fast NTE pakmod production:

- create or update mod `MaterialInstanceConstant` assets from FModel JSON and material recipes;
- generate hotkey and UI driven material-slot visibility toggles;
- cook and package selected assets into `.pak/.utoc/.ucas` with the correct `HT` mount paths;
- support the current five mod targets in `F:\NTE\PhyLab`;
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
- show why each candidate package is present and whether it looks like a mod asset, source game dependency, runtime asset, texture override, or editor-only proxy;
- validate project, engine, game mount, package names, and output directories;
- preview hard dependencies and `NeverPack` conflicts;
- save dirty packages before build;
- launch cook/package as an external process;
- collect logs and outputs under `Saved/NTEBuildTool/Packages/<ModName>`;
- create `.pak/.utoc/.ucas` using `UnrealPak` and the correct `HT` mount path.

The Package Plan is a convenience, not an authority. Users can manually add or remove packages before the Package Job is written. The default should favor mod-authored assets and generated runtime assets while making source game dependencies visible but easy to exclude when they are already provided by the base game.

The package job remains the stable interface shared by UI, CLI, and future automation.

Current implementation checkpoint:

- `Build Mod Package` first creates a Package Plan from the Content Browser selection.
- Selected assets, `/mod/` assets, and generated runtime assets are checked by default.
- Source-game hard dependencies and likely editor-only Material Proxy assets are shown but unchecked by default.
- The user confirms the plan before the package job JSON is written.
- The commandlet path still consumes a stable Package Job directly, so manual JSON packaging remains supported.

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

## Git Strategy

Use sparse milestone commits:

1. Documentation and architecture skeleton.
2. Modular code implementation builds.
3. Five-mod asset/job regeneration and package verification.

Do not commit every small edit. Do not commit generated build intermediates or cooked outputs.
