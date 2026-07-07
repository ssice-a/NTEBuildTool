# Kawaii Plugin Recovery Research

This note records how to recover enough of NTE's game-specific KawaiiPhysics variant for exact asset reconstruction.

## Core Finding

The KawaiiPhysics native plugin source is not recoverable from `.pak` / `.ucas` content archives.

For this game, the KawaiiPhysics runtime is compiled into:

```text
F:\Neverness To Everness\Client\WindowsNoEditor\HT\Binaries\Win64\HTGame.exe
```

There is no standalone `KawaiiPhysics.dll` or loose Kawaii plugin folder in the packaged game directory.

Local string scans confirm that `HTGame.exe` contains Kawaii reflection names and some build source paths:

```text
/Script/KawaiiPhysics
AnimNode_KawaiiPhysics
KawaiiPhysicsSettings
ForwardMoveOffset
bUseRelativeMove
MovementReferenceDisplacement
OffSetLocation
SphereRadius
E:\pcrt_publish_version_wai\Projects\HT\Plugins\KawaiiPhysics\Source\KawaiiPhysics\Public\KawaiiPhysicsCustomExternalForce.h
E:\pcrt_publish_version_wai\Projects\HT\Plugins\KawaiiPhysics\Source\KawaiiPhysics\Private\KawaiiPhysicsBoneConstraintsDataAsset.cpp
```

So the practical task is not "extract the plugin from pak". It is:

1. Reconstruct the reflected schema.
2. Reconstruct or infer the runtime behavior deltas from public KawaiiPhysics.
3. Patch/fork public KawaiiPhysics into a game-compatible `/Script/KawaiiPhysics` plugin.
4. Build NTEBuildTool importers on top of that compatible plugin.

## Existing Wheels

These are useful existing tools, but none of them will directly restore original C++ source:

- FModel / CUE4Parse: archive and cooked asset parsing, already used in this project.
- UAssetAPI / UAssetGUI: low-level `.uasset` parsing and JSON round-trip style inspection.
- UE4SS: runtime dumpers, including C++ headers, UHT-compatible headers, object dumps, and property offsets.
- Dumper-7: runtime SDK generator for Unreal Engine games.
- Ghidra / IDA / x64dbg / ReClass.NET: binary reverse engineering and memory layout inspection.

Dynamic dumpers are powerful, but this game includes AntiCheatExpert in the shipped directory. Do not use injection-style dumpers against a live online/anti-cheat session. Prefer static analysis first.

## Best Route For This Project

### Phase 1: Static Schema

Use the game `.usmap` as the authoritative serializable field schema:

```text
F:\F-model\NT\HT-5.6.1-0+UE5-0196ef29.usmap
```

The generated report is:

```text
F:\F-model\Output\Reports\kawaii_usmap_report.txt
```

This already confirms that the game Kawaii layout differs from public KawaiiPhysics 1.20.0.

### Phase 2: Public Source Diff

Use public KawaiiPhysics 5.6 / 1.20.0 as the base implementation, then make a field-by-field diff against the game schema.

Known differences:

- Add `FKawaiiPhysicsSettings::ForwardMoveOffset`.
- Add `FAnimNode_KawaiiPhysics::bUseRelativeMove`.
- Add `FAnimNode_KawaiiPhysics::MovementReferenceDisplacement`.
- Add `FCapsuleLimit::SphereRadius`.
- Preserve the serialized spelling `OffSetLocation`, not only public `OffsetLocation`.
- Remove or isolate public fields that are not in the game schema if final cooked compatibility requires exact layout.

### Phase 3: Runtime Delta Recovery

Fields that are only serialized are easy. Fields that affect simulation need behavior.

High-risk behavior fields:

- `ForwardMoveOffset`
- `bUseRelativeMove`
- `MovementReferenceDisplacement`
- `SphereRadius`

Recover these by comparing public Kawaii simulation functions with decompiled regions around Kawaii strings and reflected registration blocks inside `HTGame.exe`.

### Phase 4: Compatible UE Plugin

Create a project-local fork of KawaiiPhysics that keeps the same module/script identity:

```text
/Script/KawaiiPhysics
```

The UE-side plugin must own the exact reflected types before NTEBuildTool creates Kawaii animation-layer assets. Otherwise generated assets may look valid in the editor but be schema-incompatible with the game runtime.

### Phase 5: Importer

After the compatible Kawaii plugin compiles:

1. Load FModel JSON.
2. Analyze all `AnimGraphNode_KawaiiPhysics*` nodes.
3. Create or patch the corresponding AnimBlueprint / linked anim layer data.
4. Assign imported limits, constraints, curves, tags, alpha, collision settings, and root bone chains.

Do not implement final Kawaii import against stock public KawaiiPhysics.
