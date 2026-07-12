Status: ready-for-agent

# CharacterModSpec package pipeline

## Goal

Build Package Plan and Package Job directly from `CharacterModSpec`.

## Current state

`CollectCharacterModSpecPackageSeeds` now collects:

- PlayerAppearanceAsset / MeshAsset
- PlayerUIShow
- main mesh and main AnimBP
- attached meshes and attached runtime AnimBPs
- generated material instances
- replacement textures
- material-plan-derived output MI paths when `MaterialOperations.OutputMaterialPath` is empty
- runtime action material references
- runtime action generated SaveGame / Widget / host AnimBP Blueprint paths

`NteCharacterModSpec` commandlet now includes `PackageSeeds` and `PackagePlan` in its report, using the formal `BuildPackagePlanFromCharacterModSpec` API.

## Next implementation

1. Add spec-first Package UI details beyond the current workspace action button.
2. Expose package candidate role/reason groups cleanly in the Character Workspace.
3. Keep runtime Blueprint packages versioned.
4. Ensure editor-only Material Proxies remain excluded by default.
5. Keep Character package preflight checks close to the package job path.

## Tests

- Commandlet report for example spec.
- Package Plan candidate classification with missing future assets must not emit UE load warnings.
- Existing selected-asset package workflow must continue to build.

## 2026-07-11 checkpoint

Implemented:

- `BuildPackagePlanFromCharacterModSpec`
- `NteCharacterModSpec` now uses this API instead of manually passing package seeds into the generic package-plan function.
- Package-plan seed reasons now report `from CharacterModSpec`, while selected Content Browser workflows still report `selected by user`.
- `CreateModPackageJobFromCharacterModSpec`
- `NteCharacterModSpec -WritePackageJob`
- `NteCharacterModSpec -BuildPackage` and `CharacterModSpec.Package.BuildAfterCreate` launch the existing package build path after writing the job.
- Character Workspace Package action uses `CharacterModSpec` package plan/job creation.
- Character Workspace Package action saves confirmed `ModName`, `ModsDir`, `JobFilename`, and `BuildAfterCreate` settings back into the active spec before creating or launching the package job.
- Character package jobs set `RequiresHTGameStub=true`.
- `LaunchModPackageBuildJob` fails early if the Mirror Project explicitly prevents `NTEBuildTool` from loading in the Game target.
- `Config/FilterPlugin.ini` prevents `RunUAT BuildPlugin` from copying `.scratch`, previous plugin builds, binaries, intermediate outputs, saved data, and DDC into plugin packages.

Verified:

- `RunUAT BuildPlugin -StrictIncludes`.
- `RunUAT BuildPlugin -StrictIncludes` for `.scratch/PluginBuild_CharacterWorkspacePackageSpecSave`.
- `PhyLabEditor` build after plugin sync.
- `.scratch/character-mod-workspace/004_lacrimosa_character_package_plan_api.report.json` shows the generated MeshAsset as a default-included `Mod` candidate with reason `from CharacterModSpec`.
- `.scratch/character-mod-workspace/004_lacrimosa_write_package_job.report.json` writes `F:/NTE/PhyLab/Saved/NTEBuildTool/Packages/lacrimosa004_character_writer_noloadspam_P/lacrimosa004_character_writer_noloadspam_P.job.json`.
- The generated job contains 6 packages, `GameMountName=HT`, and `Unversioned=false`.
- `NteCharacterModSpec -ApplyAppearance -BuildPackage` packages `.scratch/character-mod-workspace/004_lacrimosa_apply_attached_mesh.spec.json` and writes:
  - `F:/Neverness To Everness/Client/WindowsNoEditor/HT/Content/Paks/Mods/lacrimosa004_character_attached_smoke_P.pak`
  - `F:/Neverness To Everness/Client/WindowsNoEditor/HT/Content/Paks/Mods/lacrimosa004_character_attached_smoke_P.utoc`
  - `F:/Neverness To Everness/Client/WindowsNoEditor/HT/Content/Paks/Mods/lacrimosa004_character_attached_smoke_P.ucas`
- `NtePakModAudit` on the generated job reports 0 errors / 0 warnings:
  - `.scratch/character-mod-workspace/004_lacrimosa_attached_mesh_package_audit_preflight.json`

Remaining:

- Expose package candidate grouping/editing in the Character Mod Workspace UI.
- Re-run cook/IoStore after runtime actions and Kawaii presets enter the spec.

## 2026-07-11 material seed update

`BuildPackagePlanFromCharacterModSpec` now merges `CollectCharacterMaterialPlanPackageSeeds`.

This matters because material output paths can be derived by `NteCharacterMaterialPlan`; package planning must include the effective generated MI path even when the raw spec left `OutputMaterialPath` empty.

## 2026-07-12 runtime action seed update

`BuildPackagePlanFromCharacterModSpec` now merges `CollectCharacterRuntimeActionPlanPackageSeeds`.

This matters because runtime SaveGame, Widget, and host AnimBP Blueprint assets can be generated from `CharacterModSpec.RuntimeActions`; package planning must include those assets even before the execution graph is fully generated.

Verified:

- `.scratch/character-mod-workspace/004_lacrimosa_apply_runtime_actions.report.json` includes runtime Blueprint seeds.
- `RunUAT BuildPlugin -StrictIncludes` succeeds for `.scratch/PluginBuild_CharacterRuntimeActionWriter_Strict3`.
