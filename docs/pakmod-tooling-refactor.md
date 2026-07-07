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
  - thin Post Process Anim Blueprint;
  - runtime controller Blueprint;
  - widget Blueprint;
  - save-game Blueprint;
- assign the generated Post Process Anim Blueprint to the target `SkeletalMesh` only when requested;
- inspect generated assets for expected graph/variable/widget structure.

The key change is separating the runtime controller from the post-process animation instance. The Post Process Anim Blueprint should become a thin anchor that ensures a controller exists and has the target `SkinnedMeshComponent`. The controller owns:

- hotkey polling;
- UI show/hide state;
- widget creation and binding;
- save-game persistence;
- `ShowMaterialSection` calls;
- debug/instrumentation counters.

The UI must also include a close control and not depend solely on the hotkey path to recover.

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
- validate project, engine, game mount, package names, and output directories;
- preview hard dependencies and `NeverPack` conflicts;
- save dirty packages before build;
- launch cook/package as an external process;
- collect logs and outputs under `Saved/NTEBuildTool/Packages/<ModName>`;
- create `.pak/.utoc/.ucas` using `UnrealPak` and the correct `HT` mount path.

The package job remains the stable interface shared by UI, CLI, and future automation.

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

### Initial Pure-Pak Runtime Chain

```text
Target SkeletalMesh
  -> PostProcessAnimBlueprint = ABP_NTE_ModToggle_PostProcess
      -> pass-through AnimGraph
      -> Initialize/Update ensures BP_NTE_ModToggleController exists
          -> controller stores TargetComponent
          -> controller ticks independently
          -> controller owns hotkeys, UI, save state, visibility
```

This still needs the Post Process Anim Blueprint to run at least once. It reduces damage when the UI changes input mode or when the animation update graph is not a good place for UI state. If evidence later proves a target scene never runs the post-process anchor, the next fallback is to find another replaced asset anchor in that scene. DLL/hook work is the last resort.

### Input Rules

- Group hotkeys use edge-triggered polling.
- `player_075_oneir_rpg_level2` uses the keyboard Up arrow key, not numpad Up or numpad 8.
- UI hotkey remains configurable.
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
- slots 2 and 3 toggle together using the keyboard Up arrow key;
- slot 2 and 3 material setup follows the task notes: slot 2 and 3 use the material relationship derived from slot 0/1, with material 1 getting its own instance inheriting material 0 as required;
- body texture overrides are the existing `body_bml`, `body_id`, `body_nm`, `body_rmt`;
- pants textures from the sibling pants folder must be included in the package job.

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

## Git Strategy

Use sparse milestone commits:

1. Documentation and architecture skeleton.
2. Modular code implementation builds.
3. Five-mod asset/job regeneration and package verification.

Do not commit every small edit. Do not commit generated build intermediates or cooked outputs.
