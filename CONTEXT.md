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
- **Runtime Anchor Mesh**: The `SkeletalMesh` that owns the Post Process Anim Blueprint used to tick pure-pak runtime logic. A `Skeleton` asset cannot be used as this anchor because it has no runtime component tick.
- **Target Mesh**: The mesh whose material slots should be toggled. It can be the same asset as the Runtime Anchor Mesh, or a separate mesh referenced by the runtime controller.
- **StaticMesh Visibility Adapter**: Runtime logic used when the Target Mesh is a `StaticMesh`. `SkinnedMeshComponent::ShowMaterialSection` does not apply, so the controller must use a separate adapter such as material swapping to a hidden/transparent material.
- **Standard PostProcess Template Runtime**: The current core toggle runtime mode. The plugin duplicates three user-supplied standard template assets, patches them from `NTE.ModToggleSetup`, and assigns the generated Post Process Anim Blueprint to the Runtime Anchor Mesh. This is the main path for "select a mesh, enter hotkeys, generate runtime assets"; the generated asset names default to `ABP_NTE_ModToggle_PostProcess`, `WBP_NTE_ModToggleMenu`, and `BP_NTE_ModToggleSaveGame`, so users do not have to name them manually.
- **Standard Runtime Template Contract**: The narrow interface a template must expose to the generator. Each setup group is a user-authored toggle item with a label, optional hotkey, default state, and one or more material slots. Visible-state variables are named `NTE_Toggle_XX_toggle_group_N_Visible`, input marker variables are named `NTE_Toggle_Input_N_...`, WBP buttons/text labels are named `NTE_Toggle_Button_XX_toggle_group_N` and `NTE_Toggle_Button_XX_toggle_group_N_Label`, drag widgets are named `NTE_Toggle_TitleBarButton` and `NTE_Toggle_WindowPanel`, the generated Widget Blueprint owns drag-to-move behavior in its own Tick graph, polls `NTE_Toggle_TitleBarButton.IsPressed()`, reads viewport mouse position from `Self` world context, the number of template groups must equal the setup group count, UI key placeholders use the standard `Slash` plus modifier-key pins, repeated `ShowMaterialSection` nodes for the same group are assigned to configured slots cyclically, and non-ASCII labels trigger an embedded Unicode font in the generated Widget Blueprint.
- **Thin Post Process Anim Blueprint**: A future runtime mode where the generated post-process animation blueprint passes the pose through and only starts or reconnects a toggle runtime controller. It should not own UI/input/material state.
- **Toggle Runtime Controller**: The future generated runtime logic that polls hotkeys, opens or closes UI, persists toggle state, and applies material-section visibility to a target mesh. It is not required by the current Standard PostProcess Template Runtime.
- **Toggle Setup**: A JSON description of one target mesh, runtime assets, UI hotkey, save slot, and material-slot toggle groups.
- **Material Recipe**: A JSON description used to create or update a `MaterialInstanceConstant`, usually from an FModel material JSON plus explicit texture overrides. The preferred recipe format is authored around `SourceTextureOverrides`: the key is a source texture package used by the source material, the value is the mod texture package that should replace it. The material module expands that one source-texture replacement to every material parameter in the matching Source Texture Usage group. `TextureOverrides` remains the advanced per-parameter override format and wins when both formats write the same parameter.
- **Material Proxy**: An editor-only stand-in for a missing cooked game parent material. It lets the Mirror Project create and inspect a mod `MaterialInstanceConstant` that intends to inherit a source game material path. A Material Proxy must not be treated as a Replaced Asset and should not be included in the pakmod package.
- **Source Texture Usage**: The grouped view of a source material's texture parameters, keyed by the source texture asset. It answers "which texture does this source material use, and which parameters use it?" so the user can replace one source texture once and have that new texture override every matching parameter.
- **Package Job**: A JSON description of a cook/package run. It lists the project, engine, game mount, output mods directory, mod name, package list, exclusions, and cook/pack options.
- **Package Plan**: An editable candidate package list derived from one or more selected assets, usually starting from a selected mesh. It is a convenience preview, not an automatic decision: the tool shows selected packages, hard dependencies, candidate kinds, and reasons, then lets the user remove packages before writing a Package Job. Source-game dependencies and editor-only Material Proxy candidates are visible but default to not packed.

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
- The material module should expose Source Texture Usage so users can see original texture groups and choose which source texture each new texture replaces.
- Material recipes should prefer source-texture-group replacement over isolated parameter replacement, because FModel materials commonly bind one texture to several parameters such as `PM_Diffuse` plus `ID_Tex`, `LightMap` plus a same-name mask parameter, or `PM_SpecularMasks` plus `NomralMap`.
- The package pipeline should build an editable Package Plan from selected assets before creating a Package Job; users remain in control of the final package list.
- The toggle-runtime module should move toward configuration-driven UI and logic generation. Users define toggle items, labels, hotkeys, and material slots; templates should provide style and runtime anchor shape, not hardcoded item counts.

Pure pak remains the default strategy. A native DLL is only a fallback if evidence proves a target scenario has no reliable pure-pak runtime anchor. StaticMesh targets should first be attempted with a loaded Runtime Anchor Mesh plus a StaticMesh Visibility Adapter; adding a Post Process Anim Blueprint to a Skeleton is not a valid path.
