# NTE Build Tool

NTE Build Tool is an Unreal Engine 5.6 editor plugin for building pak-only character mods from editable mirror-project assets.

The plugin is organized around one persistent Pakmod Project workflow with optional authoring recipes:

- Classify game references, user-imported assets, and tool-generated assets independently from package intent.
- Configure optional Material, Runtime, PhysicsAsset, and Kawaii/Post Process recipes.
- Explicitly apply, synchronize, or detach recipe-owned outputs without coupling those operations to packaging.
- Build Cook and IoStore jobs exclusively from the Package Manifest and the current on-disk UE assets.

The common editor entry point is:

```text
Tools > NTE Build Tool > Open Pakmod Project
```

Create or open a `.pakmod.json` project. The editor persists project identity, source and asset references, lightweight recipes, and the Package Manifest. It does not persist scans, diagnostics, cook intermediates, or Blueprint node layout.

## Settings

Project defaults live in:

```text
Project Settings > Plugins > NTEBuildTool
```

Important settings:

- `Game Mount Name`: defaults to `HT`.
- `Default Mods Output Directory`: used by package jobs when the user does not choose another output folder.
- `FModel Export Root`: required by the Player/NPC material and PhysicsAsset libraries. Set it to `.../Exports/HT/Content` (or an ancestor containing that folder).

## Workflows

### Material Instances

Use `Create Material Instance From Game Material Library` for the guided material workflow, or `Create Material Instance From Recipe JSON` for automation.

The game-material library scans the configured FModel export root for Player and NPC `MI_*.json` exports. Search for a game material instance, choose it as the template, then replace only the texture groups you need. The generated asset is a new mod MaterialInstance; it never overwrites the game material.

The guided dialog groups texture parameters by source texture usage, so one replacement texture can update every material parameter that used the same original source texture. Replacement textures can be filled from the currently selected Content Browser asset.

### PhysicsAsset

Use `Import PhysicsAsset From Game Library` after selecting the target `SkeletalMesh`. The library scans Player and NPC PhysicsAsset exports, then offers either the full source setup or a chain rooted at a matching bone.

Choose a chain only for an independent attached mesh such as a tail, ribbon, hair piece, or accessory. A chain import creates a new partial PhysicsAsset containing the matching body and constraint chain. Choose the full setup for a main character mesh.

### Kawaii Physics Chains

For copied game physics bones, use the Character Mod Workspace in this order:

1. Add the separate skirt/accessory SkeletalMesh under `Attached Skeletal Meshes`.
2. Choose `Add Game Kawaii Preset` and select that attached mesh as the target.
3. Choose the original Player/NPC clothing PSK from which the bones were copied, then search/select the exact chain such as `qun`.
4. Apply the preset and tune its generated Kawaii AnimBP, Limits, and BoneConstraints assets with Unreal's native Kawaii editors. Use `Sync` before packaging to store those adjustments back in the CharacterModSpec.

For example, the `004_lacrimosa_nighty` skirt PSK contains `Bn_*_qun*` bones; select its matching `Physics_AnimLayer_female051_BP_UI` Kawaii node rather than importing unrelated hair or accessory chains. The generated attached-mesh graph uses `CopyPoseFromMesh -> Kawaii`, so it does not replace the game's main character AnimBP.

### Runtime Actions

Use the Character Mod Workspace to add runtime actions after selecting one `SkeletalMesh`.

The setup is user-authored data:

- UI hotkey.
- Toggle item label.
- Optional toggle hotkey.
- Default visible state.
- One or more material slots.

Generated runtime asset names have defaults, so users normally only configure labels, hotkeys, and slots.

### Packaging

Use `Build Mod Package` to create an editable Package Plan from selected Content Browser assets or folders. The Package Plan is a convenience preview, not an automatic decision: the user chooses the final package list before the Package Job JSON is written and built.

`Build Mod Package From Job JSON` remains the repeatable automation path.

For a distributable plugin build, use:

```powershell
& .\Resources\Scripts\BuildNtePlugin.ps1 -StrictIncludes
```

The wrapper always writes outside the plugin repository. Do not point Unreal Automation Tool's `BuildPlugin -Package` argument at this repository or one of its subdirectories: UAT creates a temporary HostProject by copying the plugin, so an output directory inside the plugin can recursively copy `.scratch`, `.git`, and earlier HostProjects.

## Requirements

- Unreal Engine 5.6.x. Verified with Unreal Engine 5.6.1.
- A C++ Unreal project or an installed C++ toolchain that can compile editor plugins.
- FModel JSON exports for analysis/import workflows that need source metadata.

## Notes

This repository contains tooling code only. It does not include game assets, extracted assets, cooked outputs, or FModel output.

The plugin should not hardcode one-off mod targets. Project-specific material recipes, toggle setups, and package jobs belong in JSON files or user-edited assets.
