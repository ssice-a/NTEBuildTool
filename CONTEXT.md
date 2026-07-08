# NTEBuildTool Context

This repository contains an Unreal Engine editor plugin for rebuilding and packaging NTE pak-only character mods from FModel exports and editable mirror-project assets.

## Domain Terms

- **Mirror Project**: The UE 5.6 project used to rebuild editable assets that match game content paths before cooking. In current verification work this is `F:\NTE\PhyLab`.
- **FModel Export Root**: The extracted source data under `F:\F-model\Output\Exports\HT\Content`. The plugin treats these paths as the source of truth for original game content layout and FModel JSON.
- **Game Mount**: The cooked output mount name used by the target game. For NTE this is `HT`, producing pak paths like `../../../HT/Content/...`.
- **Pakmod**: A mod distributed as cooked assets in `.pak/.utoc/.ucas` form. A pakmod can replace assets at identical game paths or add assets that are reachable through a replaced asset reference chain.
- **Replaced Asset**: A cooked asset whose `/Game/...` package path matches the original game asset path. Replaced assets are the reliable pure-pak entry point.
- **Added Asset**: A new asset under a mod subfolder such as `/Game/.../mod/Runtime/...`. Added assets must be referenced by a replaced asset, otherwise cook or the game may not load them.
- **Runtime Anchor**: A loaded asset or runtime object that gives pure-pak logic an execution opportunity. The preferred current anchor is the replaced `SkeletalMesh` referencing a generated Post Process Anim Blueprint.
- **Thin Post Process Anim Blueprint**: A generated post-process animation blueprint whose AnimGraph passes the pose through and whose EventGraph only starts or reconnects a toggle runtime controller. It should not own UI/input/material state.
- **Toggle Runtime Controller**: The generated runtime logic that polls hotkeys, opens or closes UI, persists toggle state, and applies material-section visibility to a target `SkinnedMeshComponent`.
- **Toggle Setup**: A JSON description of one target mesh, runtime assets, UI hotkey, save slot, and material-slot toggle groups.
- **Material Recipe**: A JSON description used to create or update a `MaterialInstanceConstant`, usually from an FModel material JSON plus explicit texture overrides.
- **Package Job**: A JSON description of a cook/package run. It lists the project, engine, game mount, output mods directory, mod name, package list, exclusions, and cook/pack options.

## Current Mod Targets

The current verification set contains five pakmods:

- `Content\Characters\Player\004_lacrimosa`
- `Content\Maps_4N\Characters\Player\075_oneir_rpg\player_075_oneir_rpg_level0`
- `Content\Maps_4N\Characters\Player\075_oneir_rpg\player_075_oneir_rpg_level1`
- `Content\Maps_4N\Characters\Player\075_oneir_rpg\player_075_oneir_rpg_level2`
- `Content\Maps_4N\Characters\Player\075_oneir_rpg\player_075_oneir_rpg_level3`

`player_075_oneir_rpg_level2` is newly imported and still needs material-instance and runtime-anchor validation. Slots 0 and 1 are always visible. Slot 2 is toggled with numpad Up (`NumPadEight`) and slot 3 is toggled with numpad Down (`NumPadTwo`); shortcut keys are user-authored data, not inferred by the tool. Slot 2 and 3 should use the material relationship described in the task notes, and the pants texture package must be included in its package job.

## Architectural Direction

The plugin should stay modular:

- PhysicsAsset rebuild remains a narrow importer module.
- Material instance creation is a separate material module.
- Hotkey/UI/material visibility generation is a separate toggle-runtime module.
- Cook and IoStore packaging is a separate package pipeline module.
- The editor menu module wires these modules together and owns no large implementation.

Pure pak remains the default strategy. A native DLL is only a fallback if evidence proves a target scenario has no reliable pure-pak runtime anchor.
