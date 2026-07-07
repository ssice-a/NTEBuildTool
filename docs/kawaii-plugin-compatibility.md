# Kawaii Plugin Compatibility Findings

NTE does not appear to use the stock KawaiiPhysics 5.6 / 1.20.0 layout as-is.

The current source of truth is the game mapping file:

```text
F:\F-model\NT\HT-5.6.1-0+UE5-0196ef29.usmap
```

The local generated report is:

```text
F:\F-model\Output\Reports\kawaii_usmap_report.txt
```

## Confirmed Game Class

The exported animation-layer JSON references:

```text
/Script/KawaiiPhysics.AnimNode_KawaiiPhysics
```

So the game kept the public module/class identity, but the reflected serializable properties differ from the public plugin currently installed in the test project.

## Confirmed Differences

Fields present in the game usmap / FModel JSON that are missing or incompatible in the public plugin:

- `KawaiiPhysicsSettings.ForwardMoveOffset`
- `AnimNode_KawaiiPhysics.bUseRelativeMove`
- `AnimNode_KawaiiPhysics.MovementReferenceDisplacement`
- `CapsuleLimit.SphereRadius`
- `CollisionLimitBase.OffSetLocation`

The spelling of `OffSetLocation` is especially important. The public plugin uses `OffsetLocation`, while the game serializes `OffSetLocation`.

The public plugin also contains newer/different editable fields, such as `SimulationSpace`, `SimulationBaseBone`, `SkelCompMoveScale`, and `bUpdatePhysicsSettingsInGame`, which are not present in the game usmap's `AnimNode_KawaiiPhysics` property layout.

## Consequence

The NTEBuildTool should not create final Kawaii assets against stock KawaiiPhysics classes. Doing so may import visually in the editor, but the cooked asset will not be a schema-perfect match for the game class layout.

Exact replacement work should treat the game usmap as the authoritative serialized schema, then patch or fork KawaiiPhysics so UE has a game-compatible `/Script/KawaiiPhysics` module before generating animation-layer assets.

## Required Next Step

Before implementing Kawaii import:

1. Build a field-by-field compatibility report from usmap vs the installed Kawaii source.
2. Patch/fork the Kawaii plugin to match the game's reflected property names, order, and types.
3. Implement runtime behavior for the game-specific fields, not only serialization stubs.
4. Compile and validate the patched plugin in UE.
5. Only then add NTEBuildTool Kawaii asset creation/import on top of that exact plugin.
