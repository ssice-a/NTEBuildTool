# NTE Build Tool

NTE Build Tool is an Unreal Engine 5.6 editor plugin for building pak-only character mods from editable mirror-project assets.

The plugin is organized around four modules:

- PhysicsAsset import from FModel JSON.
- MaterialInstance creation from FModel material JSON or recipe JSON.
- Hotkey/UI material-slot toggle runtime generation.
- Cook and IoStore package job creation.

The common editor entry point is:

```text
Tools > NTE Build Tool > Open Mesh Mod Workspace
```

Select a `SkeletalMesh` in the Content Browser first. The workspace shows the selected mesh, its material slots, project settings, and shortcuts into the material, toggle, and package workflows.

## Settings

Project defaults live in:

```text
Project Settings > Plugins > NTEBuildTool
```

Important settings:

- `Game Mount Name`: defaults to `HT`.
- `Default Mods Output Directory`: used by package jobs when the user does not choose another output folder.
- `FModel Export Root`: optional analysis-side reference root.
- Default toggle runtime template assets: Post Process Anim Blueprint, Widget Blueprint, and SaveGame Blueprint.

## Workflows

### Material Instances

Use `Create Material Instance From FModel Material JSON` for the guided material workflow, or `Create Material Instance From Recipe JSON` for automation.

The guided dialog groups texture parameters by source texture usage, so one replacement texture can update every material parameter that used the same original source texture. Replacement textures can be filled from the currently selected Content Browser asset.

### Toggle Runtime

Use `Manage Mesh Toggle Setup` after selecting one `SkeletalMesh`.

The setup is user-authored data:

- UI hotkey.
- Toggle item label.
- Optional toggle hotkey.
- Default visible state.
- One or more material slots.

Generated runtime asset names have defaults, so users normally only configure labels, hotkeys, slots, and template defaults.

### Packaging

Use `Build Mod Package` to create an editable Package Plan from selected Content Browser assets or folders. The Package Plan is a convenience preview, not an automatic decision: the user chooses the final package list before the Package Job JSON is written and built.

`Build Mod Package From Job JSON` remains the repeatable automation path.

## Requirements

- Unreal Engine 5.6.x. Verified with Unreal Engine 5.6.1.
- A C++ Unreal project or an installed C++ toolchain that can compile editor plugins.
- FModel JSON exports for analysis/import workflows that need source metadata.

## Notes

This repository contains tooling code only. It does not include game assets, extracted assets, cooked outputs, or FModel output.

The plugin should not hardcode one-off mod targets. Project-specific material recipes, toggle setups, and package jobs belong in JSON files or user-edited assets.
