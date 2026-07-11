Status: ready-for-agent

# Character Mod Workspace UI

## Goal

Replace the current shell UI with a concise, precise, attractive editor workspace for the full character-mod workflow.

## UI structure

- Appearance summary and target row.
- Main mesh panel.
- Attached mesh tree/list.
- Material slot operation panel.
- Runtime action panel for hotkeys/UI.
- Kawaii preset panel.
- Package panel.

## Rules

- UI edits `CharacterModSpec`; it should not own generation logic.
- Common flow should use asset pickers and selected assets, not manual `/Game/...` typing.
- Hotkeys and UI buttons come from the same runtime action model.
- Attachment definitions are entered once and synced to MeshAsset and PlayerUIShow by modules.
- Advanced JSON adapters remain available for automation/debugging, but are not the default user experience.

## Tests

- Open workspace with no mesh selected.
- Open workspace with one SkeletalMesh selected.
- Material slot rows still launch material workflow with correct defaults.
- Package entry uses spec/package seeds once package API is promoted from commandlet/report.

## Current state

Implemented:

- Workspace can load/save `CharacterModSpec` JSON.
- Core spec fields are editable in the workspace: workspace name, main mesh, appearance asset, UI preview actor, mod name, and mods output directory.
- Existing `MaterialOperations` are shown from the active spec.
- Material slot rows can add material operations or first-slice `MaterialSlotVisibility` runtime actions.
- The Runtime Action button writes `CharacterModSpec.RuntimeActions` instead of calling the legacy mesh-only toggle generator.

Verified:

- `PhyLabEditor Win64 Development` builds after syncing the plugin mirror.
- `RunUAT BuildPlugin -StrictIncludes` passes for `.scratch/PluginBuild_CharacterWorkspaceRuntimeActionUi`.

Next:

- Convert material/runtime rows from summaries into editable rows with remove/reorder/reapply controls.
- Add attached mesh and Kawaii panels.
- Replace raw path text boxes with asset pickers for the common flow.
