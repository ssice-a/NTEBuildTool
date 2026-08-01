# Character asset authoring chain

> Architecture note (2026-08-01): this document remains the detailed cross-asset evidence and authoring reference. The persistent package model is now `NTE.PakmodProject`, and its package-first/separation rules are defined in `docs/adr/0002-package-first-pakmod-project.md` and `docs/pakmod-project-refactor.md`. The Nanally and attached-mesh checkpoints below are historical evidence, not universal core validation rules.

本文记录完整角色替换工作流中“哪些 asset 要改、怎么改、原生 UE 没有对应类怎么办、NTE 魔改 Kawaii 怎么处理、最后 Cook 会产出什么”。它是面向实现和 UI 设计的执行文档，不是调查笔记。

核心原则：

- 用户编辑的是 Mirror Project 里的 UE 资产和 `CharacterModSpec`，不是直接手写 cooked `.uasset`。
- 能用 UE 原生编辑器调的内容，优先保留 UE 原生体验，例如材质实例、PhysicsAsset、AnimBP、Kawaii 可视化编辑模式。
- 游戏专有类型通过 schema stub 或兼容插件解决；stub/兼容插件是 Cook 工具链的一部分，不是要随 pak 额外注入游戏的 DLL。
- 新增资产必须被替换资产引用，或者显式进入 Package Plan；纯新增但没有引用链的资产不能指望游戏自动加载。
- `CharacterModSpec` 是单一事实来源。UI 修改 spec，模块从 spec 生成材质、外观装配、运行时蓝图、Kawaii preset 和 package job。
- Kawaii 不能分成“JSON 导入模式”和“手动创建模式”两套产物。导入源游戏 JSON、引用其他预设、用户从零创建骨骼链，都必须落到同一个 `NteKawaiiPreset` 模型，并经过同一个 Apply/Cook 路径。

## Nanally 两 FBX 修复基线

Nanally 当前不是“主 FBX 保留完整原服装、附加 FBX 只含全新附件”的普通拆分。实际作者结构是：

- `原版服装修改.fbx` 是主 SkeletalMesh，但若干部件已从这里拆出，因此它缺少原版 `player_010_nanally_skin.psk` 中的若干材质槽。
- `额外物理骨骼.fbx` 同时承载被拆出的部件和新增物理骨骼网格。额外骨骼由其他服装骨架合并而来；发生重名后，相关顶点权重已经重映射到带 `.001` 后缀的新骨骼。
- 主 FBX 没有出现的原版材质槽仍必须保留为占位槽。槽位必须按原版 PSK 的完整材质顺序和原索引补回，不能追加到材质数组末尾，否则 LOD Section 的 `MaterialIndex` 会错位。
- 除明确要求替换贴图的材质外，其余槽位应继续引用游戏原有材质资产。
- 顶点色曾存在导入通道风险，现已用原版 PSK 对 `RGBA`、`RBGA`、`BGRA`、`GBRA` 四种排列计分。主 FBX 的原始 RGBA 在 9273 个可匹配位置中有 9262 个颜色兼容，G/B 交换只有 2089 个；附加 FBX 分别是 2371/2373 与 160/2373。结论是不存在全局 G/B 交换，禁止批量换通道；只需继续定位主 FBX 的 11 个和附加 FBX 的 2 个局部不兼容位置。报告见 `.scratch/nanally-vertex-color-comparison.json`。

原版 PSK 的材质槽 10 是第二个 `MI_player_010_female_cloth_b`，包含 2090 个三角面；额外 FBX 的 `MI_player_010_female_cloth_b_glass` 也恰好包含 2090 个三角面。这个几何数量对照证明眼镜部件来自原槽 10。主 FBX 应在索引 10 补一个空的原材质槽，使后续 `MI_player_010_female_hair_b` 和 `MI_player_010_nanally_fashion1_cloth_a` 回到原索引 11、12。

2026-07-30 对当前两个 FBX 进行独立 Skeleton 导入后发现，主 FBX 的 Armature 对象名 `player_010_nanally_skin` 被 UE Legacy FBX importer 当成了一根额外根骨。正式主 SkeletalMesh 的 ReferenceSkeleton 因此是 247 根，层级开头为 `player_010_nanally_skin -> root -> Bip001`；附加 SkeletalMesh 是预期的 272 根，以 `root` 为唯一根骨。两者真正共享的 246 根骨骼名称、声明顺序和参考姿势完全一致，附加网格独有的 26 根是预期的裙子/飘带物理骨骼。主网格多出的伪根骨会使它与官方 `player_010_nanally_skin_Skeleton` / `Player010_Nanally_*AnimBP` 不兼容，并对应运行时 AnimClass 存在但 AnimInstance 无法创建的 T Pose。

因此导入/打包前必须检查：主替换网格的唯一根骨是 `root`，不能把 Armature 对象节点导成骨骼；附加网格允许拥有额外物理骨骼，但所有共享骨骼必须保持名称、顺序、父级和参考姿势一致。Blender 导出时应让 Armature 对象使用 UE importer 可剥离的 `Armature`/`armature` 名称，同时保留真正骨骼 `root`。两个当前 FBX 都还触发了 UE 的 invalid BindPose 重建警告；本次重建后共享参考姿势一致，但后续导出仍应修正 BindPose，避免依赖导入器自动修复。

1213/1214 已纠正此前把 UI 预览和大世界视为同一条“组件创建链”的判断。两者可以共用同一个主 SkeletalMesh，但附加组件来源不同：UI 预览由 `PlayerUIShow_*` 的 SCS 子组件创建，大世界由 `HTPlayerAppearance.ArrayFashionAttachedMeshData` 交给角色装配逻辑创建。1214 已证明纯 pak 的 UI SCS 路线可以加载附加 SkeletalMesh；大世界 Appearance 路线仍需看到使用 world AnimBP 的实际组件后才能判定成功。

诊断顺序：

1. 从源游戏 `MeshAsset_*`、attached SkeletalMesh、Skeleton、AnimBP 和 Kawaii 节点建立一组完整原生样本。
2. 对照 `ArrayFashionAttachedMeshData` 的 mesh、AnimInstance、MobileAnimInstance、SocketName、transform 和 tags。
3. 对照主/附加 Skeleton 的骨骼层级、根变换、挂点、物理链命名和权重目标。
4. 对照 attached AnimBP 的输入姿势来源、Kawaii RootBone/limits/constraints/PhysicsAsset 引用。
5. 每个诊断包只改变一个变量，并以实际游戏中附加 Mesh 是否显示作为最终反馈信号。

## Kawaii 单一作者模型

Kawaii 的设计目标是：用户从游戏 JSON 导入后修改几个参数，和用户从零创建物理链后引用同一个游戏预设，最终看到、编辑、打包的对象完全一致。

```text
Source game AnimBP / AnimLayer JSON
Source game attached-mesh AnimBP JSON
Source game merged-main physics AnimLayer JSON
User-authored blank chain
User references another preset/template
  -> Kawaii Import/Authoring Adapter
    -> NteKawaiiPreset
      -> Preview/Editable UE assets
        -> AnimBP Kawaii node(s)
        -> LimitsDataAsset
        -> BoneConstraintsDataAsset
        -> optional CurveFloat assets
      -> Package Plan
      -> Cook
```

因此实现时禁止出现两套等价逻辑：

- 不要有一套“导入 JSON 直接生成 AnimBP”的快捷路径，再有一套“手动 UI 生成 AnimBP”的路径。
- 不要让导入的 Kawaii 参数只停留在报告里，而手动创建的参数才进入最终打包。
- 不要让用户从零建链时使用一套字段，而导入游戏原生链时使用另一套字段。

正确做法：

- JSON 导入只负责创建 `NteKawaiiPreset` 的候选节点和源引用信息。
- 用户手动创建链也是创建同一个 `NteKawaiiPreset` 节点。
- “引用其他预设”是把另一个 preset 或源 JSON 节点作为 seed/template 复制到当前 preset，不在打包时保留动态依赖。
- 用户修改参数、重映射骨骼、启用/禁用链、调碰撞体之后，保存回同一个 preset。
- Apply 阶段只读取 resolved `NteKawaiiPreset`，统一生成或更新 AnimBP、LimitsDataAsset、BoneConstraintsDataAsset、曲线资产和 package seeds。

当前已落地的第一步是 commandlet 级导入/预览：

```text
NteCharacterModSpec
  -ImportKawaiiJson=<FModel AnimBP/AnimLayer JSON>
  -KawaiiTargetMeshId=<main or attached mesh id>
  -KawaiiPresetPrefix=<stable id prefix>
  -WriteUpdatedSpec=<updated CharacterModSpec>
```

这一步会把每个 `AnimGraphNode_KawaiiPhysics*` 转成 `CharacterModSpec.KawaiiPresets[]`，并在报告里输出 `KawaiiImportResult` 和 `KawaiiPresetPlan`。它不会绕过 preset 直接生成 AnimBP；后续 UE 可视化编辑器和 AnimBP/DataAsset writer 也必须继续消费同一个 preset 模型。

导入之后会进入 `KawaiiPlan`：

```text
CharacterModSpec.KawaiiPresets
  -> KawaiiPlan
     -> TargetMeshPath
     -> RuntimeAnimBlueprintPath
     -> OutputLimitsDataAssetPath
     -> OutputBoneConstraintsDataAssetPath
     -> Output CurveFloat paths
     -> PackageSeeds
     -> Schema warnings / blocking errors
```

`KawaiiPlan` 负责派生缺省输出路径，不把“路径是自动派生的”当成 warning；报告里用 `DerivedRuntimeAnimBlueprintPath`、`DerivedOutputLimitsDataAssetPath`、`DerivedOutputCurvePath` 等布尔字段标记。真正应该提醒用户的是 schema 兼容、目标 mesh 缺失、无法派生 `/Game` 路径、骨骼链未设置等问题。

UI 上应该把 Kawaii preset 看成一组“可应用到某个目标 mesh 的物理节点”：

```text
KawaiiPreset
  TargetMeshId = main / attached_x
  Nodes[]
    Label
    Source
      ImportedFromJsonPath
      ImportedNodeName
      ReferencedPresetId
      TemplateKind
    Chains
    Settings
    Curves
    Limits
    BoneConstraints
    NteExtensions
    ResolvedOutput
      RuntimeAnimBlueprintPath
      LimitsDataAssetPath
      BoneConstraintsDataAssetPath
      CurveAssetPaths
```

一个源 JSON 可能代表“分件附加 mesh 的 AnimBP”，也可能代表“主 mesh 上合并了多个部位的 Physics AnimLayer”。导入器必须把两者都解析成同一种 node candidate：

- 分件 AnimBP JSON：通常一个文件对应一个附加 mesh 的一组 Kawaii 节点，默认 `TargetMeshId` 可以指向当前选中的 attached mesh。
- 合并主 mesh / Physics AnimLayer JSON：一个文件可能有很多 `AnimGraphNode_KawaiiPhysics*`，导入后应显示为可勾选的节点列表，用户决定哪些节点应用到 main，哪些复制/重映射到 attached mesh。
- 若源骨骼名和目标 Skeleton 不一致，导入仍然成功；UI 标记 unresolved bones，用户用骨架树重映射或批量改名前缀。

打包前用户应该能看到“将要生成/更新什么”：

```text
Kawaii package preview
  TargetMesh: attached_tail
  RuntimeAnimBP: /Game/.../mod/Anim/ABP_NTE_attached_tail_Runtime
  KawaiiNodes:
    tail_chain
      RootBone: bone_tail_00
      AdditionalRootBones: ...
      Limits: DA_NTE_tail_KawaiiLimits
      Constraints: DA_NTE_tail_KawaiiConstraints
      Schema: compatible / warning / blocked
  Package seeds:
    ABP_NTE_attached_tail_Runtime
    DA_NTE_tail_KawaiiLimits
    DA_NTE_tail_KawaiiConstraints
    optional CurveFloat assets
```

这和材质模块的原则一致：材质既可以从源游戏材质 JSON 生成，也可以用户手动改参数，但最终都是同一个 Material Operation 和同一个 MI writer；Kawaii 也必须是同一模型、同一 writer、同一 package preview。

## 全链路资产矩阵

| 资产/文件 | 是否需要修改/生成 | 修改方式 | 生成/打包结果 | 备注 |
|---|---:|---|---|---|
| `CharacterModSpec` JSON | 必须 | Character Workspace UI 编辑；命令行可读写 | 不一定入 pak，作为工具侧源数据 | 记录目标外观、主 mesh、附加 mesh、材质操作、运行时动作、Kawaii preset、打包设置 |
| 主 `SkeletalMesh` | 常见 | Blender/FBX 导入 UE；可替换原路径或新增到 `/mod/...` 后由 MeshAsset 引用 | cooked `.uasset/.ubulk/.uexp` | 物理骨骼链在这个 mesh 的 Skeleton 中，不是另一个“物理骨架” |
| 附加 `SkeletalMesh` | 常见 | Blender/FBX 导入 UE；在 MeshAsset attached mesh 数组注册 | cooked `.uasset/.ubulk/.uexp` | 多个附加 mesh 各自拥有自己的 Skeleton/AnimBP/物理链更清晰 |
| `Skeleton` | 常见 | UE 随 SkeletalMesh 导入或复用；可在 Skeleton Tree 中检查骨骼链 | cooked `.uasset` | Skeleton 本身不 tick，不能作为运行时逻辑锚点 |
| `PhysicsAsset` | 可选但推荐 | UE PhAT 编辑；或由重建模块从源 JSON/导入数据生成 | cooked `.uasset` | 常规碰撞和预览用；Kawaii 的阻尼/刚性/曲线不在 PhysicsAsset 里 |
| 材质实例 `MaterialInstanceConstant` | 常见 | 从 FModel 材质 JSON 解析参数；UI 选择源贴图替换组；生成/更新 MI | cooked `.uasset` | 与现有材质模块一致：源材质 JSON -> 参数归一化 -> 源贴图使用组 -> MI |
| 贴图 | 常见 | UE 导入或复用已有游戏贴图路径 | cooked `.uasset/.ubulk` | 必须被 MI 参数引用或显式打包 |
| 主/附加 `AnimBP` | 常见 | NTEBuildTool 生成或用户打开 UE 调整 | cooked `.uasset` | 源游戏附加 mesh 通常使用自己的 SequencePlayer/BlendSpace 姿势源，再经过 Kawaii 和 OutputPose；CopyPose 只是一种待验证策略，不是原生默认结论 |
| Kawaii `LimitsDataAsset` | 可选但推荐 | 通过 Kawaii 节点导出，或插件 UI 生成；在 Kawaii 可视化编辑模式中调碰撞体 | cooked `.uasset` | 比把所有碰撞体内联在 AnimBP 节点上更利于复用和 UI 管理 |
| Kawaii `BoneConstraintsDataAsset` | 可选 | 通过 Kawaii 节点导出，或插件 UI 生成；可手动骨骼对/正则批量生成 | cooked `.uasset` | 裙摆、多链约束、保持骨骼间距离时使用 |
| 曲线 `CurveFloat` | 可选 | UE 曲线编辑器或内联 `RuntimeFloatCurve` | cooked `.uasset` 或内联到 AnimBP | 用于按骨骼链长度比例缩放 Damping/Stiffness/Radius/LimitAngle |
| `HTPlayerAppearance` / `MeshAsset_*` | 角色装配必须 | Appearance writer 写 `FashionMeshData` 与 attached mesh 数组 | cooked `.uasset` | 原生角色装配入口；新增附加 mesh 应在这里注册 |
| `PlayerUIShow_*` Blueprint | UI 预览需要 | UIShow writer 同步 SCS 子组件 | cooked `.uasset` | 如果只替换原主 mesh，源 UI 预览可能自然显示替换 mesh；新增附加组件通常仍要同步 |
| Runtime SaveGame Blueprint | 有运行时 UI/热键时生成 | Runtime Action Writer 生成 | cooked `.uasset` | 保存每个 action 的状态 |
| Runtime Widget Blueprint | 启用 UI 时生成 | Runtime Action Writer 生成按钮树与点击事件 | cooked `.uasset` | 必须使用 versioned runtime Blueprint cook，避免 UMG cooked 数据被游戏误读 |
| Runtime host AnimBP EventGraph | 有热键/状态应用时生成 | Runtime Action Writer 注入热键、UI、SaveGame、目标组件查找、显隐/材质动作 | cooked `.uasset` | 主 host 可通过 component tags 控制附加 mesh |
| Package Job JSON | 必须 | Package Workspace/commandlet 生成 | 工具侧文件 | 写入 package list、mods 输出目录、cook/pack 设置 |
| `.pak/.ucas/.utoc` | 最终产物 | UE Cook + IoStore/UnrealPak 打包 | 放入游戏 Mods 目录 | 包内路径必须使用游戏 mount，例如 `../../../HT/Content/...` |

## Mesh、骨骼和 Kawaii 的关系

物理骨骼不是单独一套运行时骨架。它就是目标 SkeletalMesh 的 Skeleton 中的一组骨骼链。

主 mesh 路线：

```text
Main SkeletalMesh
  -> Main Skeleton contains hair/skirt/cloth physics bones
  -> Main or generated physics AnimBP
     -> Kawaii node(s)
```

附加 mesh 路线：

```text
Attached SkeletalMesh
  -> Attached Skeleton contains tail/hair/accessory physics bones
  -> Attached Runtime AnimBP
     -> source-game sequence/blendspace, or another explicitly verified pose source
     -> Kawaii node(s)
     -> OutputPose
```

原游戏中 `004_lacrimosa_fashion4` 的 tail/eardrop/ribbon 资源证明了后一种路线：附加 mesh 拥有自己的 attached AnimBP，AnimBP 父类为 `HTAttachedMeshAnimInstance`，AnimGraph 中包含 `AnimGraphNode_KawaiiPhysics`。已检查的 `oneiroi075_swimsuit_ribbon_AB`、`lacrimosa_fashion4_tail_AB`、`nanally_fashion3_hair_AB`、`player_004_lacrimosa_nighty_hair_AB` 和 `shinku076_swimsuit_ribbon_AB` 使用自身 SequencePlayer/BlendSpace 姿势源，而不是 CopyPose。`lacrimosa_fashion4_tail_skin_new_Skeleton` 里的 `bone_tail_00` 到 `bone_tail_12` 是典型物理骨骼链。

## Kawaii 参数到底复制什么

从源游戏复制的不是 mesh，也不是 PhysicsAsset，而是 Kawaii 节点配置和相关 DataAsset：

```text
RootBone
ExcludeBones
AdditionalRootBones
DummyBoneLength
BoneForwardAxis
PhysicsSettings
  Damping
  Stiffness
  WorldDampingLocation
  WorldDampingRotation
  Radius
  LimitAngle
  ForwardMoveOffset          # NTE-specific
Curves
  DampingCurveData
  StiffnessCurveData
  WorldDampingLocationCurveData
  WorldDampingRotationCurveData
  RadiusCurveData
  LimitAngleCurveData
Limits
  SphericalLimits
  CapsuleLimits
  BoxLimits
  PlanarLimits
  LimitsDataAsset
  PhysicsAssetForLimits
BoneConstraints
  BoneConstraintGlobalComplianceType
  BoneConstraintIterationCountBeforeCollision
  BoneConstraintIterationCountAfterCollision
  bAutoAddChildDummyBoneConstraint
  BoneConstraints
  BoneConstraintsDataAsset
Forces / collision / ignore
  Gravity
  bEnableWind
  WindScale
  bAllowWorldCollision
  bOverrideCollisionParams
  bIgnoreSelfComponent
  IgnoreBones
  IgnoreBoneNamePrefix
NTE-specific
  bUseRelativeMove
  MovementReferenceDisplacement
  CapsuleLimit.SphereRadius
  CollisionLimitBase.OffSetLocation
  KawaiiPhysicsTag
```

不要把运行时缓存字段当成源参数复制：

```text
ModifyBones
DeltaTime
PreSkelCompTransform
bPhysicsSettingsInitialized
MergedBoneConstraints
ActualAlpha
```

用户体验应该是“复制源游戏参数作为种子，然后在 UE 中重映射骨骼链和可视化微调碰撞体/约束”，而不是要求用户手写所有字段。这个 seed 过程不应改变最终路径：导入得到的 preset 和手动创建的 preset 都必须通过同一个 Kawaii Apply writer 生成最终 AnimBP/DataAsset。

## Kawaii 可视化编辑体验

公开 KawaiiPhysics 的原生设计是：

- 在 AnimBP 中添加/选中 `Kawaii Physics` 节点。
- 节点进入 `AnimGraph.SkeletalControl.KawaiiPhysics` 编辑模式。
- Persona/AnimBP 预览视口显示：
  - ModifyBones 点线；
  - 每根骨骼的 Radius；
  - LimitAngle 圆锥；
  - Sphere/Capsule/Box/Plane collision limits；
  - BoneConstraint 约束线；
  - ExternalForce 箭头；
  - Bone Length Rate 调试信息。
- 用户可点击碰撞体，用 UE gizmo 平移、旋转、缩放。

因此插件 UI 不应该替代这个视口。插件 UI 应该做：

- 从源游戏 AnimBP/AnimLayer JSON 导入 Kawaii node candidates，并把勾选的节点保存为 `NteKawaiiPreset`。
- 允许从零创建 node，并允许该 node 引用另一个 preset、源游戏 JSON 节点或模板作为初始参数。
- 显示目标 mesh 和骨架树，辅助选择 `RootBone`、`AdditionalRootBones`、`ExcludeBones`。
- 对不存在的骨骼显示 unresolved warning，但不阻断保存/打包。
- 提供模板：头发、裙摆、袖子、挂件、尾巴。
- 提供“打开/生成 Runtime AnimBP 并进入 Kawaii 编辑”的按钮。
- 提供 schema compatibility report，说明哪些字段 common、NTE-only、public-only、unsupported。

## 原生 UE 没有游戏类型怎么办

Mirror Project 不能直接保存未知 `/Script/...` 类型。解决方式按类型分层：

| 缺失类型 | 解决方式 | 是否打进 pak |
|---|---|---:|
| `/Script/HTGame.HTPlayerAppearance` 等游戏装配类 | 在本仓库提供 `HTGame` stub module，字段名/类型对齐 usmap/导出 JSON | 不打 DLL；只 Cook 由 stub 保存的资产 |
| `/Script/KawaiiPhysics.AnimNode_KawaiiPhysics` | 安装并 patch/fork NTE 兼容 KawaiiPhysics 插件，保持同名 module/class | 不打 DLL；Cook asset 引用游戏已有真实模块 |
| 源游戏 parent material 在 Mirror Project 不存在 | 生成 editor-only Material Proxy 或选择替代 parent；Proxy 不进入 pak | Proxy 不打包 |
| 源游戏 Blueprint/AnimInstance 父类缺失 | 优先用可 Cook 的 stub 或改用生成的自有 AnimBP 父类；必须保证游戏运行时可解析引用 | 只打 cooked BP，不打工具 DLL |
| UE 无法表达的 cooked 私有字段 | 保留在 neutral spec/report；final writer 必须等兼容 schema 或低层 patch 工具支持 | 未解决前不生成“声称最终兼容”的资产 |

`HTGame` stub 是 schema bridge，适合字段数据型资产，例如 `HTPlayerAppearance`。Kawaii 不适合只做空 stub，因为它是运行时模拟节点：最终 AnimBP 需要真实 Kawaii 行为。Mirror Project 里的 Kawaii 插件必须既能序列化字段，也能在预览/调参时执行近似真实模拟。

## NTE 魔改 Kawaii 怎么办

NTE 仍使用：

```text
/Script/KawaiiPhysics.AnimNode_KawaiiPhysics
```

但字段不等于公开 KawaiiPhysics。当前确认的差异包括：

```text
KawaiiPhysicsSettings.ForwardMoveOffset
AnimNode_KawaiiPhysics.bUseRelativeMove
AnimNode_KawaiiPhysics.MovementReferenceDisplacement
CapsuleLimit.SphereRadius
CollisionLimitBase.OffSetLocation
```

处理规则：

1. `F:\F-model\NT\HT-5.6.1-0+UE5-0196ef29.usmap` 是最终序列化 schema 权威。
2. 公开 KawaiiPhysics 只能作为算法和编辑器 UX 基础。
3. Mirror Project 的 `Plugins/KawaiiPhysics` 必须 patch/fork 成 NTE 兼容版：
   - 保持 module name `KawaiiPhysics`；
   - 保持 class path `/Script/KawaiiPhysics.AnimNode_KawaiiPhysics`；
   - 添加 NTE 字段；
   - 保留 `OffSetLocation` 这种游戏序列化拼写；
   - 隔离或移除 public-only 字段，避免最终 cooked 布局不匹配；
   - 为影响模拟的字段补行为，不只补 UPROPERTY。
4. `NTEBuildTool` 先把源 Kawaii 节点导入 neutral `NteKawaiiPreset`；用户从零创建的链也必须进入同一个模型。
5. 只有 compatibility report 通过后，才把 resolved preset 应用到真实 AnimBP Kawaii 节点和 DataAsset。

## Cook 和 Package

推荐路线是 UE 正规 Cook，再从 cooked 输出打包，不手写二进制 cooked `.uasset`。

```text
CharacterModSpec
  -> ApplyMaterials
  -> ApplyAppearance
  -> ApplyRuntimeActions
  -> ApplyKawaii / GenerateAttachedAnimBP
  -> BuildPackagePlan
  -> PackageJob
  -> UE Cook
  -> pak/ucas/utoc
```

Cook 前要求：

- Mirror Project 能加载 `NTEBuildTool` runtime side for Game target。
- 需要 `HTGame` stub 的 package job 标记 `RequiresHTGameStub=true`。
- 需要 Kawaii 的 package job 必须确认 NTE-compatible Kawaii plugin 已启用。
- Runtime Blueprint / Widget package 使用 versioned cook；不要对这些包强行 unversioned。
- Editor-only Material Proxy 不进入 package candidates。
- 新增资产要么被替换资产引用，要么显式进入 Package Plan；推荐两者都有。

Cook 后包内通常包含：

```text
Content/Characters/Player/<target>/...
  MeshAsset_PlayerXXX.uasset                         # replaced or generated appearance data
  player_xxx_skin.uasset/.ubulk                      # replaced main mesh if same path
  mod/
    Meshes/*.uasset/.ubulk                           # added main/attached meshes
    Materials/*.uasset                               # generated MIs
    Textures/*.uasset/.ubulk                         # replacement textures
    Anim/*.uasset                                    # generated runtime AnimBPs
    Physics/*.uasset                                 # PhysicsAsset / Kawaii DataAssets / curves
    Runtime/*.uasset                                 # SaveGame / Widget / host AnimBPs
```

最终输出：

```text
<ModName>_P.pak
<ModName>_P.ucas
<ModName>_P.utoc
```

## UI 设计要求

每类 asset 都应该有“源参数导入 + UE 可视化编辑 + spec 回写 + package preview”的体验：

| 模块 | UI 应提供 |
|---|---|
| 材质 | material slot 表；源材质 JSON 导入；Source Texture Usage 分组；一键生成/更新 MI；贴图选择器；slot 应用预览 |
| 外观装配 | main mesh、attached mesh 列表；socket/transform/tag/AnimBP 字段；从源 MeshAsset 导入；同步 UIShow 预览 |
| Runtime actions | action 列表；热键冲突检查；UI 按钮标题/默认状态；目标 mesh/tags；生成 Widget/SaveGame/AnimBP |
| Kawaii | 从源 AnimBP/AnimLayer JSON 导入节点；从零创建同一类节点；引用其他 preset/template 作为 seed；骨架树选链；参数模板；兼容报告；在 Character Workspace 列表中显示目标 skeleton、缺失骨骼和 tag 状态；打开 AnimBP 可视化调碰撞体/约束；回写同一个 preset |
| Package | 按来源分组的 Package Plan；替换资产/新增资产/依赖/Proxy 分组；用户确认后写 Package Job |

目标是让用户不需要知道每个 cooked 字段，但高级用户仍能打开 UE 原生编辑器或高级字段面板精调。

## Current Kawaii writer rule

`CharacterModSpec.KawaiiPresets -> KawaiiPlan -> ApplyKawaii` is the only Kawaii asset generation path. JSON import, manual creation, template/preset seeding, and UE-edited asset sync must all converge before writing assets.

The current writer deliberately separates safe editor preparation from final game-compatible Kawaii output:

- `CurveFloat` assets can be generated from resolved curve plan items.
- Kawaii `LimitsDataAsset`, `BoneConstraintsDataAsset`, and attached-mesh AnimBP node writing require an NTE-compatible `/Script/KawaiiPhysics` schema.
- Attached-mesh Runtime AnimBP generation is supported. The generated AnimGraph is `CopyPoseFromMesh(bUseAttachedParent=true) -> LocalToComponentSpace -> one or more KawaiiPhysics nodes -> ComponentToLocalSpace -> OutputPose`.
- Main-mesh Kawaii AnimGraph generation is still intentionally blocked until there is an explicit source-pose strategy that preserves the game's original character animation.
- `KawaiiPlan` performs non-blocking diagnostics against the target mesh skeleton and GameplayTags registry. It reports `TargetMeshLoaded`, `TargetSkeletonPath`, `ReferencedBones`, `MissingBones`, `KawaiiPhysicsTagChecked`, and `KawaiiPhysicsTagValid`, and mirrors missing bones / invalid tags into warnings for UI display. These diagnostics do not prevent save, apply, cook, or package.
- Character Workspace consumes the same `KawaiiPlan` diagnostics in its Kawaii preset list, showing target mesh kind, target skeleton, missing-bone count, and tag status with a tooltip containing the full target/skeleton/missing-bone summary plus the resolved Runtime AnimBP path.
- Attached-mesh Kawaii rows expose separate `Apply`, `AnimBP`, `Limits`, `Constraints`, and `Sync` actions. `Apply` resolves the selected preset through `KawaiiPlan` and rewrites its generated Runtime AnimBP/DataAssets through `NteCharacterKawaiiWriter`. `AnimBP`, `Limits`, and `Constraints` open the generated UE assets without overwriting them. `Sync` reads the generated Runtime AnimBP/DataAssets back into `CharacterModSpec.KawaiiPresets`. Main-mesh Kawaii rows remain disabled until source-pose preservation is designed.
- The schema probe checks for the game fields `ForwardMoveOffset`, `bUseRelativeMove`, `MovementReferenceDisplacement`, `SphereRadius`, and `OffSetLocation`.
- Missing required game/NTE fields block DataAsset/AnimBP writing. Public-Kawaii-only fields are reported as warnings until the mirror plugin surface is fully reconciled with the game usmap.
- A package job/build with Kawaii presets requires successful `-ApplyKawaii` in the commandlet run, so package seeds and generated assets cannot drift apart.

## Current Kawaii implementation checkpoint

The PhyLab mirror plugin `F:/NTE/PhyLab/Plugins/KawaiiPhysics` has been patched with the first required NTE schema bridge:

```text
KawaiiPhysicsSettings.ForwardMoveOffset
AnimNode_KawaiiPhysics.TargetFrameRate
AnimNode_KawaiiPhysics.bUseRelativeMove
AnimNode_KawaiiPhysics.MovementReferenceDisplacement
AnimNode_KawaiiPhysics.bPhysicsSettingsInitialized
CapsuleLimit.SphereRadius
CollisionLimitBase.OffSetLocation
```

The current behavior bridge is intentionally minimal:

- `bUseRelativeMove` consumes `MovementReferenceDisplacement`, matching source-game PropertyAccess evidence from `HTPlayerPhysicsAnimLayer`.
- `ForwardMoveOffset` is propagated to per-bone settings and affects pose-pull base location.
- `SphereRadius` affects capsule collision when non-zero.

Verified package chain:

```text
CharacterModSpec.KawaiiPresets
  -> KawaiiPlan
  -> ApplyKawaii
  -> generated Kawaii Limits/Constraints DataAssets
  -> generated attached-mesh Runtime AnimBP
  -> PackagePlan seeds
  -> PackageJob
  -> BuildPackage
  -> pak/ucas/utoc
```

Validation sample:

```text
.scratch/character-mod-workspace/004_lacrimosa_kawaii_preset_validation.spec.json
```

Results:

- `ApplyKawaii` reports `SchemaProbe.NteCompatible=true`.
- Generated:
  - `/Game/Characters/Player/004_lacrimosa/mod/Kawaii/Data/DA_NTE_hair_tail_kawaii_KawaiiLimits`
  - `/Game/Characters/Player/004_lacrimosa/mod/Kawaii/Data/DA_NTE_hair_tail_kawaii_KawaiiConstraints`
  - `/Game/Characters/Player/004_lacrimosa/mod/Kawaii/Anim/ABP_NTE_hair_tail_Kawaii`
- The generated attached AnimBP reports `rebuilt attached-mesh Kawaii AnimGraph with 1 Kawaii node(s)`.
- `NteAssetInspection` reports `AnimGraphSummary.HasExpectedAttachedKawaiiChain=true` for the generated attached AnimBP.
- `KawaiiPlan` reports missing placeholder bones and the invalid placeholder tag before the Kawaii AnimBP compiler emits its own warnings.
- `-ApplyKawaii -WritePackageJob -BuildPackage` produces `lacrimosa004_kawaii_preset_validation_P.pak/.ucas/.utoc` under the configured game Mods directory with `ErrorCount=0`.
- `RunUAT BuildPlugin -StrictIncludes` succeeds for `.scratch/PluginBuild_KawaiiAnimGraph_Strict`.
- Character Workspace `Apply/Open` was added after this chain and verified with `RunUAT BuildPlugin -StrictIncludes` at `.scratch/PluginBuild_KawaiiApplyOpen_Strict`, `PhyLabEditor Win64 Development`, `NteCharacterModSpec -ApplyKawaii` to `.scratch/kawaii-apply-open-report.json`, `NteAssetInspection` to `.scratch/kawaii-apply-open-inspection-report.json`, and full `-ApplyAppearance -ApplyKawaii -WritePackageJob -BuildPackage` to `.scratch/kawaii-apply-open-package-report.json`.
- Character Workspace native Kawaii edit/sync was then verified with strict plugin build `.scratch/PluginBuild_KawaiiNativeEditSync2_Strict`, `NteCharacterModSpec -SyncKawaiiFromAssets -KawaiiPresetId=hair_tail_kawaii -WriteUpdatedSpec=.scratch/kawaii-native-edit-sync.synced.spec.json`, reapply to `.scratch/kawaii-native-edit-sync-reapply-report.json`, full package to `.scratch/kawaii-native-edit-sync-package-report.json`, and inspection to `.scratch/kawaii-native-edit-sync-inspection-report.json`.

## Current Kawaii native edit/sync chain

Native Kawaii editing is treated as an asset-editing bridge, not a second source of truth:

```text
CharacterModSpec.KawaiiPresets
  -> KawaiiPlan
  -> Apply generated Runtime AnimBP + generated Limits/Constraints DataAssets
  -> native UE Kawaii node/DataAsset editing
  -> Sync generated assets back into CharacterModSpec.KawaiiPresets
  -> ApplyKawaii / Package from the synced spec
```

The workflow deliberately matches the stock Kawaii plugin editing surface:

- Kawaii node parameters are edited in the AnimBP node Details panel.
- Limits and constraints live in DataAssets and can be opened independently.
- Persona/Kawaii edit mode remains the visual surface for checking collision, limits, and constraints.
- The NTE tool owns the asset graph, paths, sync, diagnostics, and package seeds; the native Kawaii editor owns detailed visual parameter editing.

The asset responsibilities are:

| Asset | Written by Apply | Opened by UI | Read by Sync | Packaged |
|---|---:|---:|---:|---:|
| generated attached Runtime AnimBP | yes | `AnimBP` | yes | yes |
| generated Kawaii Limits DataAsset | yes | `Limits` | yes | yes |
| generated Kawaii BoneConstraints DataAsset | yes | `Constraints` | yes | yes |
| `CharacterModSpec.KawaiiPresets` | no | Workspace form | yes | source model |

`Apply` can overwrite generated Kawaii assets because it replays the spec. Opening the generated assets is non-destructive. `Sync` must be run after native editing if those edits should survive the next apply/package run.

## Nanally 1213/1214 attached-component checkpoint

The test package contains two distinct native attachment descriptions:

```text
UI preview:
DT_AppearanceData[Fashion_1010_0].UIActorClass
  -> PlayerUIShow_010_C
  -> SCS child HTSkeletalMeshComponentBudgeted
  -> native parent Mesh, socket Bip001-Pelvis
  -> SM_NTE_Nanally_ExtraPhysics
  -> ABP_NTE_extra_physics_CopyPoseProbe_C

World candidate:
MeshAsset_Player010.ArrayFashionAttachedMeshData[0]
  -> SM_NTE_Nanally_ExtraPhysics
  -> socket Bip001-Pelvis
  -> ABP_NTE_extra_physics_Kawaii_C (desktop and mobile)
  -> CopyPoseFromMesh(bUseAttachedParent=true)
  -> Kawaii x2
```

Final 1213 is the first package that created a confirmed runtime attached component. The runtime
component used `ABP_NTE_extra_physics_CopyPoseProbe_C`, so it is specifically attributed to the UI
SCS path. The default source `PlayerUIShow_010` contains only the native main `Mesh`; Appearance-only
probes could never create an extra component in that UI actor. Final 1213 added one
`HTSkeletalMeshComponentBudgeted` template and one SCS node, parented to the native `Mesh` property.

The mirror parent schema is part of the serialization contract. Modeling
`BP_UIShowSimpleCharacterBase` as `ACharacter` caused UE to serialize `CharacterMesh0`,
`CollisionCylinder`, and `CharMoveComp`. The accepted 1213 form models the native
`HTUIShowSimpleCharacter` Actor schema, produces the official seven exports plus exactly one
component template and one SCS node, and uses `ParentComponentOrVariableName=Mesh` with
`bIsParentComponentNative=true`.

UE4SS only enumerated this existing component. A direct 1214 launch without UE4SS still loads the
extra mesh, proving that the component is created by the pure-pak UI Blueprint path. Conversely,
calling `LoadAsset` on `MeshAsset_Player010` and reading its attached array does not prove the world
assembler consumed it. World success requires observing a world component that uses
`ABP_NTE_extra_physics_Kawaii_C`; the UI component uses `CopyPoseProbe` and cannot serve as that
signal.

1214 differs from 1213 by excluding exactly these two shared source-game packages:

```text
/Game/Characters/Player/010_nanally/animation/Player010_Nanally_AnimBP
/Game/Characters/Player/010_nanally/animation/Player010_Nanally_UIAnimBP
```

Their reconstructed 1213 copies caused base 010 and the shared night presentation to T Pose. With
the packages absent in 1214, the official containers win again and animation is restored while the
attached UI component remains. Source-game AnimBPs are therefore external references by default;
only generated mod AnimBPs or intentionally edited source AnimBPs belong in the Package Job.

For a main FBX that moved the original index-10 glass geometry to the attached FBX, preserve the original main material table as 13 slots. Insert the duplicate `MI_player_010_female_cloth_b` at index 10 and store LOD0's effective material map as `[1,2,3,4,5,6,7,8,9,11,12]`. This preserves the no-geometry slots at indices 0 and 10 without shifting rendered hair/fashion sections.

The current 1214 static evidence is
`.scratch/character-mod-workspace/010_nanally-full-chain-anim-filter-1214.container.json` plus the
cooked exports beside it. `ContainerWinnerReporter` verifies 31 selected assets resolve to 1214,
while both shared original AnimBPs resolve to official containers. The historical
`.scratch/validate_nanally_game_test_final.ps1` gate predates the UI/world split and must not be used
as the sole 1214 acceptance test.

## Appearance schema bridge defaults

`HTGame` mirror classes are serialization bridges, not authoritative gameplay CDOs.
For target-specific Appearance values such as capsule size and mesh-relative location,
`CharacterModSpec.Appearance` should carry explicit source-game values. Mirror CDO
defaults must remain neutral so UE cooking does not elide a value that the target game
would restore from a different native CDO default.

Every generated mod AnimBP path explicitly stored in `CharacterModSpec` must also be an explicit
package seed. This includes generated main, main UI, attached desktop/mobile/UI, and attached runtime
AnimBP paths. Package planning runs before writers update asset dependencies, so dependency discovery
alone cannot safely include a newly generated Blueprint. Existing source-game AnimBPs are different:
they remain references to official containers and must be excluded unless the mod explicitly intends
to replace them.

## Nanally 1215/1216 world attachment and material checkpoint

The world runtime path is now proved independently of the UI path. Probe 1215 replaces
`BP_NPC_011_WA008` and adds `NTE_Attach_extra_physics` as an SCS child of native
`CharacterMesh0`. A post-restart UE4SS sample at `2026-07-29 20:37:39` observed both the main
component and the new attached component on the big-world actor. The attached component used
`SM_NTE_Nanally_ExtraPhysics`, `ABP_NTE_extra_physics_Kawaii_C`, identity relative transform, and
was visible. The generated Kawaii AnimBP distinguishes this world component from the UI component.

The official main mesh has 13 material slots and official index 5 is
`MI_player_010_eyelash`. The edited main FBX has no geometry for original indices 0 and 10, so its
11 LOD0 sections correctly map to `[1,2,3,4,5,6,7,8,9,11,12]`. Probe 1215 incorrectly referenced
`MI_player_010_female_cloth_b` at index 5, which rendered the eyelash faces with a clothing
material.

The Mirror Project did not contain `MI_player_010_eyelash`, so 1216 creates an editor-only
official-path Material Proxy, assigns it to main-mesh slot 5, and excludes that proxy package from
the IoStore response. At runtime the cooked import resolves to the game's original material.
CUE4Parse verifies that 1216 keeps all 13 entries, preserves the 11-section map, imports the
official eyelash at index 5, excludes the proxy, and wins all 31 selected packages at read order
`121703`. Both shared source AnimBPs still resolve to official containers.

For edited meshes with removed source sections, validation must compare both the full indexed
material-object table and each LOD section's effective material index against the original asset.

The 2026-07-30 latest Nanally refresh also proved that material-array restoration and section
remapping are separate writes. UE 5.6 Python can read LOD section slots but cannot write them. After
reimporting the main FBX and restoring its 13-entry material table, run
`NteSkeletalMeshSectionMaterial` in a fresh UE commandlet process with slots
`1,2,3,4,5,6,7,8,9,11,12`. Do not report the reimport complete from the Python process alone.
Comparing only imported slot names is insufficient.

## Config-driven presentation targets and package intent

The attachment writer no longer has a PlayerUIShow-only branch. `CharacterModSpec.Appearance.PresentationTargets`
declares every Blueprint that must receive generated attached components. Each target supplies an explicit
`BlueprintClassPath` and `ParentMeshComponentName`; the writer fails if that exact skeletal mesh component does
not exist. It does not guess `Mesh`, `CharacterMesh0`, or the first skeletal mesh component.

Nanally configures only the UI presentation target. Big-world attachment remains native Appearance data:

```text
world main -> original BP_NPC_011_WA008 from the game
world attachment -> MeshAsset_Player010.ArrayFashionAttachedMeshData
ui main/attachment -> PlayerUIShow_010 -> Mesh / NTE_Attach_extra_physics
```

The targets can reference the same SkeletalMesh package without sharing a component instance or the same
initialization path. `ConfigureMainMesh=false` only tells the writer not to author the target's main
`SkeletalMesh`/`AnimClass`; it does not clear values written by an earlier apply. The Nanally 0730
override-free UI probe was diagnostic only and runtime evidence rejected it: the preview selected another
character and entered T Pose. The release asset must mirror the official `PlayerUIShow_010` main `Mesh`
bindings (`player_010_nanally_skin` plus `Player010_Nanally_UIAnimBP_C`) and preserve the official
`DefaultCharacterID=1010` and character-info montage while retaining the one generated attached SCS node.
Changing a target back from `ConfigureMainMesh=false` to `true` is not complete until `ApplyAppearance`
runs again. Editing only the spec or patching the generated-class CDO can look correct in the editor while
the next cook still omits the child Blueprint's main-mesh overrides.

`AttachedMeshes[].PresentationTargetIds` can restrict a mesh to selected targets. An empty list means all
configured targets, with legacy `SyncToUIShow=false` still excluding the migrated `ui` target. Legacy
`UIActorClassPath` JSON is accepted only as a migration source for `ui/Mesh`.

`CharacterModSpec.Package.Assets` records one of three package intents:

```text
ExternalReference  # load from official game containers; never seed the mod package
GeneratedAsset     # produced by a plugin writer and included in the package
ReplacementAsset   # intentionally shadows an existing /Game package
```

Source-game main/UI AnimBPs are reference-only by default. Generated Kawaii Runtime AnimBPs and their
Limits/Constraints/Curve assets remain generated package seeds. Package preflight checks that the generated
AnimBP exists, has exactly one `CopyPoseFromMesh` with `bUseAttachedParent=true`, has the expected number of
Kawaii nodes, and is the AnimInstance selected by its attached-mesh plan.

The Nanally 13-slot/11-section repair is an FBX-specific indexed-material contract. It is documented as
diagnostic evidence and is deliberately not represented as a generic CharacterModSpec material-slot rewrite.

## Nanally durable package baseline

The current Nanally attachment release contract is recorded in `docs/nanally-package-baseline.md`
and `Resources/Validation/NanallyPackageBaseline.json`. The default validator deliberately checks
only the active attachment chain, external ownership, container selection, and deployment hashes.
Historical FBX hashes, topology, material experiments, UI defaults, and runtime-action defaults are
diagnostic evidence; they are not universal release gates.

## 2026-07-31 big-world runtime ownership correction

The first 0731 package added an attached array to
`DA_Appearance_011_RideAlong`, but a UE4SS sample disproved the assumption that the active
`BP_NPC_011_WA008_C` actor consumes it. The runtime actor is a `BP_NPC_011_C` child with a direct
`CharacterMesh0`; it is not an `HTPlayerNPCCharacter`, exposes no
`HTPlayerNPCAppearanceComponent`, and owned no extra mesh component.

This also explains the historical differential. The 1215 package that visibly created the world
component contained a generated SCS child on `BP_NPC_011_WA008`; removing that presentation target
removed the component. The corrected ownership chain is:

```text
BP_NPC_011_WA008_C
  -> native CharacterMesh0
  -> SCS child NTE_Attach_extra_physics
```

The writer configures the world main component with the modified Nanally mesh and the official
`NPC_011_AnimBP_C`, then adds exactly one child at `root` using the extra mesh and generated Kawaii
AnimBP. The early `Player010_Nanally_AnimBP_C` world assignment was wrong and caused T Pose.
`DA_Appearance_011_RideAlong` is not packaged for this mod.

## 2026-07-31 runtime attachment boundary

Static package validation is necessary but cannot prove that the world assembler rendered an attached
mesh. The UE4SS probe is the acceptance boundary for this distinction. In the 2026-07-30 samples,
`MeshAsset_Player010` loaded as `UHTPlayerAppearance`, reported one attached entry, and the transient
world `ChildActor...BPCharacterActor_C` owned an `HTSkeletalMeshComponentBudgeted` named
`NTE_Attach_extra_physics`. That component used the mod SkeletalMesh and
`ABP_NTE_extra_physics_Kawaii_C`, was parented to the world main `Mesh`, and had an active Kawaii
AnimInstance. The old probe did not record visibility, render-pass, socket, or relative-transform state,
so those samples prove component creation, not visible pixels.

The next runtime sample must classify the attached component by owner path and record
`IsVisible`, `bHiddenInGame`, `IsVisibleInScene`, `WasRecentlyRendered`, render-pass flags, socket, and
relative transform. Interpret the result in this order:

1. No `NTE_Attach_extra_physics` under the world `ChildActor`: the active appearance source/assembler is
   not consuming the replacement asset; inspect the NPC appearance selection data before changing any
   Blueprint.
2. Component exists but is hidden or not rendered: repair only component visibility/render state or the
   owning appearance tuple; do not replace the world Blueprint.
3. Component is visible/rendered but geometry is absent or displaced: inspect the attached mesh reference
   skeleton, root/socket transform, and AnimBP pose output; do not change the registration path.

The probe intentionally runs before repackaging. A new pak is justified only after one of these runtime
branches identifies a single field-level change and the static validator still passes.

## Nanally 0730 replacement-versus-attachment differential

The main replacement and the attached physics mesh have different animation entry points. The main
mesh uses the official Nanally AnimBP directly. The attached mesh is a separate component with its
own extended Skeleton; its generated AnimBP copies matching parent-pose bones and then evaluates the
additional Kawaii chains. The two Skeleton assets do not need identical bone counts. The attached
Skeleton may add physics bones, provided all shared bones keep compatible names, parents, declaration
order, and reference transforms.

The latest main FBX was compared with the official `player_010_nanally_skin.psk`: both contain 246
bones, with no missing/extra names, parent mismatch, or declaration-order mismatch. The main mesh not
containing the attached skirt's extra physics bones is therefore not a T-pose cause.

The 2026-07-30 formal-asset check found a separate import error: the main FBX Armature object was
named `player_010_nanally_skin`, so the legacy UE importer retained that object node as bone 0 and
produced `player_010_nanally_skin -> root -> Bip001` (247 bones). The attached FBX Armature object was
named `armature`, so its real root remained `root` (272 bones including the 26 intended physics
bones). The 247-bone main mesh could not instantiate the official Nanally AnimBP and caused the T
Pose. The formal main mesh was repaired in place by removing only the pseudo-root; the source FBXs,
attached mesh, geometry, morphs, materials, section map, and real-bone poses were preserved. The
release contract is now 246 main bones with `root` at index 0. The external main Skeleton and official
AnimBPs remain excluded from the mod package.

Independent FBX imports showed identical transforms for all 246 shared bones. The already-imported
formal UE assets contain a small pre-existing local-pose delta on a few shared face/foot bones (maximum
`0.004260314` on `Bon_uplip_M`). The root repair preserved that exact delta; it did not introduce or
attempt to normalize it. Future exports should name the Blender Armature object `Armature` or
`armature` and include a valid bind pose, avoiding both the pseudo-root and importer bind-pose rebuild.

The accepted 1215/1216 exports and the 0730 exports have byte-identical Appearance data, generated
Kawaii AnimBP JSON, and attached Skeleton JSON. The 31-package set is also unchanged. The meaningful
presentation differences were that the 0730 SCS nodes lost the cooked `AttachToName=root` tag, and the
world target stopped explicitly writing its main mesh and official world AnimBP. `SocketName` configures
the native Appearance entry; `PresentationSocketName` independently configures generated SCS nodes. The
Nanally spec sets both to `root`, but a fresh UE 5.6 cook was observed stripping the SCS tag even though
editor inspection still reported `root`; this tag is therefore not accepted as a T-pose explanation by
itself. Both UI and world main AnimInstances must be inspected at runtime independently.

The final 2026-07-31 runtime sample rejected the intermediate Appearance-only release. The world
presentation target and `BP_NPC_011_WA008` ReplacementAsset intent are required for this actor. The
replacement must remain narrow: configure only `CharacterMesh0` with the official NPC AnimBP and add
one generated child. UI independently retains one generated child. Container validation must prove
both Blueprints contain their attachment nodes while the official player AnimBPs remain external.

Reimport is also a validated source transition, not merely a filename operation. A skeletal FBX must contain
an Armature, a skinned mesh, non-empty bone weights, the intended root, and the expected bone count before it
can replace an existing SkeletalMesh. The Nanally D814 source export failed those checks, so the release uses
the audited D596 scene-derived copy without modifying the user's source file. Package validation pins both
main and attached source hashes, cooked topology, reference-skeleton counts, material order, and the
CopyPose/Kawaii node contract.

## 2026-07-31 Nanally merged-mesh Post Process transition

The current Nanally authoring path supersedes the two-component attachment path above. The main clothing
geometry and additional physics-bone geometry are authored in one FBX and imported as one SkeletalMesh with
one extended Skeleton. Kawaii runs in the main mesh `PostProcessAnimBlueprint`, using
`PostProcessInputPose`; it does not use `CopyPoseFromMesh` or an attached-parent lookup.

This removes the need to register an extra component independently in player, NPC, and UI presentation
assets. Official gameplay/UI/NPC AnimBPs remain external and continue to produce the base pose. The generated
Post Process AnimBP is bound once on the replacement SkeletalMesh and therefore follows every consumer of
that mesh.

Material import is name-driven for this asset. The FBX currently has 20 effective sections and slots. UE
normalizes `MI_player_010_female_cloth_b_body.001` to
`MI_player_010_female_cloth_b_body_001`; the material plan binds that name to the generated body instance.
No missing or deleted slot is restored, appended, or used to shift later material indices.
