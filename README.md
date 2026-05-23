# NTE Build Tool

NTE Build Tool is an Unreal Engine editor plugin for rebuilding NTE character assets from FModel JSON exports.

Current features:

- Import a FModel `PhysicsAsset` Save Properties JSON file.
- Rebuild capsule bodies and physics constraints.
- Restore disabled body collision pairs.
- Assign the rebuilt `PhysicsAsset` to the selected `SkeletalMesh`.

## Requirements

- Unreal Engine 5.6.x. Verified with Unreal Engine 5.6.1.
- A C++ Unreal project or an installed C++ toolchain that can compile editor plugins
- FModel JSON exported with `Save Properties (.json)`

## Installation

Copy this folder into your project:

```text
YourProject/Plugins/NTEBuildTool
```

Then restart Unreal Editor and enable the plugin if prompted.

## Usage

1. Select exactly one `SkeletalMesh` in the Content Browser.
2. Open `Tools > NTE Build Tool > Import FModel PhysicsAsset JSON`.
3. Choose the FModel-exported `*_PhysicsAsset.json`.
4. Check the generated PhysicsAsset, then save the dirty assets.

The importer creates a new asset named like:

```text
SelectedMesh_FModel_PhysicsAsset
```

It does not overwrite an existing PhysicsAsset.

## Notes

This repository contains only tooling code. It does not include game assets, extracted assets, or FModel output.

KawaiiPhysics AnimLayer JSON import is planned, but not implemented yet.

## License

License is not selected yet.
