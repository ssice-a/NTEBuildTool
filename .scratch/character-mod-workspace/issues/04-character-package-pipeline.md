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
- runtime action material references

`NteCharacterModSpec` commandlet now includes `PackageSeeds` and `PackagePlan` in its report.

## Next implementation

1. Add a `BuildPackagePlanFromCharacterModSpec` API instead of keeping this only inside commandlet/report code.
2. Add commandlet options to write a package job from spec.
3. Respect Package settings from `CharacterModSpec.Package`.
4. Keep runtime Blueprint packages versioned.
5. Ensure editor-only Material Proxies remain excluded by default.

## Tests

- Commandlet report for example spec.
- Package Plan candidate classification with missing future assets must not emit UE load warnings.
- Existing selected-asset package workflow must continue to build.
