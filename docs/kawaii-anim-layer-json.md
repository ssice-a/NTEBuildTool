# Kawaii AnimLayer JSON Parsing

FModel exports animation-layer blueprints as a JSON array of Unreal exports. KawaiiPhysics node settings are not top-level objects. They live on the class default object:

```text
Default__*_C.Properties.AnimGraphNode_KawaiiPhysics*
```

The analysis step should read every property whose name is either:

```text
AnimGraphNode_KawaiiPhysics
AnimGraphNode_KawaiiPhysics_<number>
```

## Important Fields

These fields describe the editable KawaiiPhysics behavior and should be preserved for import:

- `RootBone`
- `ExcludeBones`
- `AdditionalRootBones`
- `DummyBoneLength`
- `BoneForwardAxis`
- `PhysicsSettings`
- `DampingCurveData`
- `StiffnessCurveData`
- `WorldDampingLocationCurveData`
- `WorldDampingRotationCurveData`
- `RadiusCurveData`
- `LimitAngleCurveData`
- `SphericalLimits`
- `CapsuleLimits`
- `BoxLimits`
- `PlanarLimits`
- `LimitsDataAsset`
- `PhysicsAssetForLimits`
- `BoneConstraints`
- `BoneConstraintsDataAsset`
- `Gravity`
- `bEnableWind`
- `WindScale`
- `bAllowWorldCollision`
- `bOverrideCollisionParams`
- `CollisionChannelSettings`
- `bIgnoreSelfComponent`
- `IgnoreBones`
- `IgnoreBoneNamePrefix`
- `KawaiiPhysicsTag`

## Runtime Cache Fields

These fields are generated or cached at runtime and should not be treated as source configuration:

- `ModifyBones`
- `DeltaTime`
- `PreSkelCompTransform`
- `bPhysicsSettingsInitialized`
- `ComponentPose`
- `ActualAlpha`

Alpha fields such as `Alpha`, `AlphaInputType`, and `AlphaScaleBias` still matter because they are part of the anim node's editable blend behavior.

## Version Differences

Some exported NTE KawaiiPhysics nodes contain fields that are not present in the public KawaiiPhysics source currently used by the test project:

- `PhysicsSettings.ForwardMoveOffset`
- `bUseRelativeMove`
- `MovementReferenceDisplacement`
- `CapsuleLimits[].SphereRadius`
- `CapsuleLimits[].OffSetLocation`

The analyzer preserves these fields in neutral structs where possible, but a future importer cannot apply them to stock KawaiiPhysics classes unless the matching plugin version or game-specific changes are available.
