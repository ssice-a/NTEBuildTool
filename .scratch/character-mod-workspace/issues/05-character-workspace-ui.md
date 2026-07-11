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
