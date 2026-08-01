# Nanally merged-mesh release contract

## Current chain

Nanally now uses one merged SkeletalMesh. The extra skirt geometry and its additional physics bones
are part of the main mesh and Skeleton; no attached component is generated or packaged.

```text
/Game/Characters/Player/010_nanally/player_010_nanally_skin
  -> official gameplay/UI AnimBP selected by the game
  -> PostProcessAnimBlueprint ABP_NTE_main_PostProcess
     -> PostProcessInputPose
     -> 2 KawaiiPhysics nodes
     -> OutputPose
```

The same replacement SkeletalMesh is consumed by player, NPC, and UI presentation paths. Those
consumers remain official external assets. The mod does not replace `MeshAsset_Player010`,
`BP_NPC_011_WA008`, `PlayerUIShow_010`, or any official gameplay/UI AnimBP.

## FBX and materials

Source FBX: `G:/yh/娜娜莉/nnl_postbp.fbx`.

The accepted import has one mesh, one Armature, 272 bones, 20 material slots, 20 LOD0 sections, and
190 imported MorphTargets. Import does not create materials or textures. It preserves the FBX slot
order and then resolves each slot to a material reference by its authored name.

Slots 0-11 use the corresponding source-game material assets. Slots 13-16 resolve to the original
`MI_player_010_female_cloth_b` material. Slot 12 uses the generated body instance with the four body
texture overrides. Slots 17-19 use generated translucent instances based on the original Oneiroi
swimsuit material with the four clothes texture overrides.

The deleted unused FBX material slot must not be restored, appended, or used as an index offset.

## Runtime controls

The generated Post Process AnimBP also hosts the material visibility actions. Runtime polling is
gated on its owning main `SkeletalMeshComponent`, not on an attached parent.

- `/` opens or closes the clothing UI; UI visibility itself is not persisted.
- Clothing choices use SaveGame persistence.
- The actions remain tie, coat, glasses, skirt, shoes, clothes, underpants, and bra.
- Coat controls slots 0, 10, and 16. Glasses controls only slot 13.
- Coat and skirt default to hidden.

## Build and deploy

- Spec: `.scratch/character-mod-workspace/010_nanally_world-attachment-ui-1215.spec.json`
- Job: `F:/NTE/PhyLab/Saved/NTEBuildTool/Packages/zzzzzzz_nanally_merged_postbp_0731_P/zzzzzzz_nanally_merged_postbp_0731_P.job.json`
- Staging: `F:/NTE/.mod-staging/NanallyMergedPostBP0731`
- Deployment: `F:/Neverness To Everness/Client/WindowsNoEditor/HT/Content/Paks/Mods/Nanallyshuiyi`
- Cook: versioned, 27 explicit packages

The deployed container set is:

```text
zzzzzzz_nanally_merged_postbp_0731_P.pak
zzzzzzz_nanally_merged_postbp_0731_P.ucas
zzzzzzz_nanally_merged_postbp_0731_P.utoc
```

Run the lightweight release check after packaging:

```powershell
& .\Resources\Validation\ValidateNanallyPackage.ps1
```

The check validates the current single-mesh package selection and staging/deployment hashes. It does
not revive historical attached-mesh, NPC SCS, UI SCS, or FBX topology experiments as release gates.
