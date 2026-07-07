# Pak-only 角色资产替换流程调研

本文目标是定义一套不依赖 UE4SS 运行时框架、只依赖 cooked Pak 替换/新增资产的 NTE 角色 Mod 生产流程。

结论先行：这不是“把游戏 cooked uasset 还原成源码工程”，而是建立一个足够像游戏工程的 Mirror UE Project，把 FModel/UE4SS/usmap 能导出的数据转换成原版 UE 可显示、可编辑、可 Cook 的资产，再用同路径替换或引用链新增的方式让游戏加载。

## 依据

- UE 官方说明 cooked asset 在 Editor 中可用但限制很大：资产是只读的，不能打开 cooked Material/Mesh 的编辑器；为了保留引用，需要维持原 cooked 内容的目录结构，cooked 资产也不能随意移动或重命名。见 Epic 文档：`https://dev.epicgames.com/documentation/unreal-engine/working-with-cooked-content-in-the-unreal-engine`
- UE 官方说明打包由 Build、Cook、Stage、Package 等阶段组成；Cook 会把 geometry、materials、textures、Blueprints 等资产转换成目标平台可运行格式。见 Epic 文档：`https://dev.epicgames.com/documentation/unreal-engine/packaging-your-project`
- UE 官方说明 Cook commandlet 用于生成平台特定内容；基本命令形态是 `UnrealEditor.exe <uproject> -run=cook -targetplatform=<Platform>`。见 Epic 文档：`https://dev.epicgames.com/documentation/unreal-engine/cooking-content-in-unreal-engine`
- UE4SS UHT Dumper 能生成 UHT-compatible C++ headers，用于创建游戏 mirror `.uproject`；`.usmap` Dumper 用于 unversioned properties mapping。见 UE4SS 文档：`https://docs.ue4ss.com/dev/feature-overview/dumpers.html`
- UAssetGUI/UAssetAPI 是二进制资产低层编辑路线，可按 engine version 和 mappings 读取/修改 `.uasset/.uexp`，但它不是 UE Editor 可视化资产重建路线。见 `https://github.com/atenfyr/UAssetGUI` 与 `https://atenfyr.github.io/UAssetAPI/guide/basic.html`

## 本地事实

当前重点调研对象：

```text
F:\F-model\Output\Exports\HT\Content\Characters\Player\051_female
F:\F-model\Output\Reports
F:\F-model\NT\HT-5.6.1-0+UE5-0196ef29.usmap
F:\NTE\NTEBuildTool
```

`051_female` 中已看到这些关键资产：

```text
player_051_female_skin.psk
player_051_female_skin.uasset
player_051_female_skin.ubulk
player_051_female_skin_Skeleton.uasset
player_051_female_skin_PhysicsAsset.uasset
player_051_female_skin_PhysicsAsset.json
Player051_Female_AnimBP.uasset/json
Player051_Female_UIAnimBP.uasset/json
MeshAsset_Player051.uasset
FBIKPlayer_051_IKRig.uasset
DT_Player051Montage.uasset
```

本地 PhysicsAsset 报告显示：

```text
SkeletalBodySetups: 19
LoadedBodies: 19
ConstraintSetup: 18
LoadedConstraints: 18
CollisionDisableTableRows: 171
```

其中 body 覆盖骨盆、腿、脊椎、手臂、头、头发、裙摆等骨骼，例如：

```text
Bip001-Pelvis
Bip001-Spine
Bip001-Head
Bn_l_hairB_001
Bn_r_hairB_002
Bn_r_qunD_001
Bn_m_qunTie_ALL
```

本地 Kawaii usmap 报告显示游戏保留 `/Script/KawaiiPhysics.AnimNode_KawaiiPhysics`，但字段与公开 KawaiiPhysics 不完全一致。关键差异包括：

```text
KawaiiPhysicsSettings.ForwardMoveOffset
AnimNode_KawaiiPhysics.bUseRelativeMove
AnimNode_KawaiiPhysics.MovementReferenceDisplacement
CapsuleLimit.SphereRadius
CollisionLimitBase.OffSetLocation
```

所以 NTE 的 KawaiiPhysics 不能直接假设等于公开插件版本。

## UE 角色资产运行原理

一个角色外观在 UE 里不是单个模型文件，而是多资产协同：

```text
角色 Actor / Pawn
└─ SkeletalMeshComponent
   ├─ SkeletalMesh
   │  ├─ Skeleton
   │  ├─ PhysicsAsset
   │  ├─ Material Slots
   │  ├─ Morph Targets
   │  ├─ LOD / Sections
   │  └─ Post Process Anim Blueprint
   ├─ AnimClass / 主 AnimBP
   └─ 运行时材质实例 / 动态材质实例
```

主 AnimBP 负责基础动作、状态机、Montage、BlendSpace 等。Post Process AnimBP 在主动画之后执行，适合追加 KawaiiPhysics、AnimDynamics、IK 修正、材质/部件控制入口等逻辑。

Pak-only 模式没有 UE4SS 动态修引用能力，因此必须从游戏本来会加载的资产开始建立引用链：

```text
游戏加载原路径 SkeletalMesh
    -> 替换 Pak 提供同路径 SkeletalMesh
        -> SkeletalMesh 引用 PostProcessAnimBP
            -> PostProcessAnimBP 引用/Spawn 控制 Actor
                -> 控制 Actor 引用材质实例、Widget、数据资产
```

未被任何资产引用的新增蓝图/材质/UI，大概率不会被 Cook 进去，也不会被游戏自动加载。

## 模型网格

PSK/FBX/GLTF 负责把网格带回 UE，但 PSK 通常只能可靠承载：

- 顶点
- 面
- 骨骼权重
- 骨架层级/参考姿态的一部分
- 材质槽名或材质 ID

PSK 不等于完整 UE `SkeletalMesh`。它不会还原：

- 游戏材质实例参数
- 材质节点图
- PhysicsAsset
- KawaiiPhysics 节点
- 后期动画蓝图
- 游戏自定义 DataAsset
- 完整 cooked 资产引用链

因此 DCC 软件负责的是“几何和绑定正确”：

```text
网格形状
骨骼层级
权重
Morph Target / Shape Key
UV
材质 ID / Section 划分
```

NTEBuildTool 负责把游戏 cooked 数据中 UE 空项目无法直接恢复的部分，转换成 UE 可编辑资产。

## 骨骼与权重

新增物理骨骼首先是 DCC 工作：

```text
新增 bone
调整 parent
刷权重
导出 FBX/PSK
导入 UE
```

但“骨骼能动”不等于“物理正确”。骨骼链要成为游戏里可控的二次运动，至少还需要：

```text
Skeleton/SkeletalMesh 中存在骨骼和权重
PhysicsAsset 中存在 bodies/constraints
PostProcessAnimBP 中存在 Kawaii/AnimDynamics 节点
节点参数引用正确 root bone / limits / collision
```

所以新增物理骨骼涉及：

```text
SkeletalMesh.uasset
Skeleton.uasset
PhysicsAsset.uasset
PostProcessAnimBP.uasset
```

不是只改一个文件。

## PhysicsAsset

PhysicsAsset 是可在 UE 中编辑的资产，负责碰撞体、约束和碰撞禁用表。NTEBuildTool 当前已实现方向正确：读取 FModel `PhysicsAsset` Save Properties JSON，重建 bodies、constraints、disabled collision pairs，并绑定到选中 SkeletalMesh。

下一步最小补全：

- 支持 sphere/box/convex/tapered capsule，而不只 capsule
- 完整恢复 `BodySetup` 的物理材质、质量、collision profile、trace flag
- 完整恢复 `ConstraintInstance` 的 linear/angular limit、drive、projection、breakable、disable collision
- 记录无法映射的字段到报告，避免静默丢失
- 增加“以骨骼链生成 body/constraint 模板”的功能，用于新增物理骨骼

PhysicsAsset 只提供碰撞和约束，不负责 Kawaii 的弹性、阻尼、曲线、外力、风等模拟逻辑。

## KawaiiPhysics / AnimDynamics

NTE 角色物理更关键的是 PostProcess AnimBP 中的动画物理节点。

本地报告说明 `AnimGraphNode_KawaiiPhysics` 的可编辑配置藏在动画蓝图 CDO 上，例如：

```text
Default__*_C.Properties.AnimGraphNode_KawaiiPhysics*
```

应保留的源配置字段包括：

```text
RootBone
ExcludeBones
AdditionalRootBones
DummyBoneLength
BoneForwardAxis
PhysicsSettings
DampingCurveData
StiffnessCurveData
WorldDampingLocationCurveData
WorldDampingRotationCurveData
RadiusCurveData
LimitAngleCurveData
SphericalLimits
CapsuleLimits
BoxLimits
PlanarLimits
LimitsDataAsset
PhysicsAssetForLimits
BoneConstraints
BoneConstraintsDataAsset
Gravity
bEnableWind
WindScale
bAllowWorldCollision
bOverrideCollisionParams
CollisionChannelSettings
bIgnoreSelfComponent
IgnoreBones
IgnoreBoneNamePrefix
KawaiiPhysicsTag
```

运行时缓存字段不应作为源配置导入：

```text
ModifyBones
DeltaTime
PreSkelCompTransform
bPhysicsSettingsInitialized
ComponentPose
ActualAlpha
```

因为 NTE 的 `/Script/KawaiiPhysics` 与公开插件不完全一致，最小可行路线是：

1. 用 usmap 作为序列化 schema 真相源。
2. fork/patch KawaiiPhysics 插件，使 UE 工程中存在兼容的 `/Script/KawaiiPhysics` 类型。
3. 先只实现字段、序列化和编辑器显示，再逐步补运行时行为。
4. NTEBuildTool 从 FModel AnimBP JSON 读取 Kawaii 节点配置，生成可编辑 DataAsset 或直接应用到兼容节点。

如果不做兼容插件，只用公开 KawaiiPhysics，编辑器里可能能显示，但 Cook 后字段布局/名称不一定匹配游戏。

## 动画

动画资产可分三层：

- AnimSequence/Montage：具体动作数据。
- 主 AnimBP：状态机、Montage、Blend、角色运动逻辑。
- PostProcess AnimBP：在主动画之后追加物理/IK/控制逻辑。

Pak-only 角色替换不建议一开始替换主 AnimBP，因为主 AnimBP 经常依赖游戏逻辑、DataTable、状态机变量、角色类。更稳的是：

```text
保留游戏原主动画逻辑
替换 SkeletalMesh
用兼容 Skeleton/骨骼名承接原动画
在 SkeletalMesh 上挂自己的 PostProcessAnimBP
```

新增按键、UI、材质切换等逻辑也建议通过 PostProcessAnimBP 建立入口，再把复杂逻辑交给普通 Actor Blueprint。

## Morph Target / 形态键

Morph Target 是 SkeletalMesh 资产的一部分，通常由 DCC 导出并导入 UE。要在游戏中沿用原角色脸部/表情逻辑，需要保证：

- Morph Target 名称与游戏调用名称一致。
- 新 mesh 顶点/形态键导入成功。
- 如果游戏通过曲线或 AnimBP 驱动 morph，名称必须对齐。

NTEBuildTool 最小功能应先做 Morph 名称报告：

```text
导入后的 SkeletalMesh morph list
FModel/报告中发现的 morph/curve list
缺失项/新增项/重名项
```

是否能“增加”新 morph，取决于有没有运行时逻辑驱动它。Pak-only 模式下，新 morph 必须被你的 PostProcessAnimBP/控制 Actor/材质或动画资产引用，否则只是静态存在。

## 材质与材质实例

材质系统分三层：

```text
Material：节点图和 shader 逻辑
MaterialInstance：Parent + 参数表
Texture：贴图资源
```

Cooked Material 通常无法还原完整节点图。材质实例能还原/编辑的是参数，不是节点连线：

```text
Parent
TextureParameterValues
ScalarParameterValues
VectorParameterValues
StaticSwitchParameters
```

角色 A 的材质实例给角色 B 使用是可行的，前提是：

- B 的 SkeletalMesh material slot 引用到 A 的 MI 或其重建版本。
- A 的 MI parent 在游戏运行时存在，或你的 Pak 提供了兼容 parent material。
- B 的 UV、mask、vertex color、section 语义能匹配 A 的材质逻辑。

两个材质实例可以 parent 相同但贴图不同：

```text
MI_Body_A -> Parent M_CharacterSkin -> BaseColor T_A
MI_Body_B -> Parent M_CharacterSkin -> BaseColor T_B
```

新增材质实例可以，但必须被引用。删除材质实例可以，但不要留下缺失引用。

NTEBuildTool 的材质相关最小功能：

- 从 FModel/UAsset JSON 读取 MaterialInstance 参数。
- 在 UE 中创建可编辑 MaterialInstanceConstant。
- 自动设置 texture/scalar/vector/switch 参数。
- 如果 parent material 不存在，生成“同路径参数壳材质”或提示用户选择替代 parent。
- 为 SkeletalMesh 的 material slot 批量应用 MI。

## 蓝图逻辑与按键切换

Pak-only 下不使用 UE4SS 动态脚本，因此蓝图逻辑必须从资产引用链启动。

推荐结构：

```text
替换后的 SkeletalMesh
└─ PostProcessAnimBP
   ├─ AnimGraph：Kawaii / AnimDynamics / IK
   └─ EventGraph：
      ├─ Initialize Animation -> Try Get Pawn Owner
      ├─ SpawnActor BP_ModController
      └─ 保存 Controller 引用

BP_ModController
├─ 保存 SkeletalMeshComponent 引用
├─ 监听按键
├─ SetMaterial 切换材质
├─ SetVisibility / SetHiddenInGame 切换部件
├─ CreateWidget / AddToViewport
└─ 保存当前状态
```

按键切换材质的稳妥方式：

- 预制多个 MI。
- Blueprint 中对指定 slot 调 `SetMaterial(SlotIndex, MI_X)`。
- 所有 MI 通过变量或数组被 BP 引用，确保 Cook。

动态材质实例路线也可行，但更依赖运行时参数名和蓝图函数可用性。

## 替换与新增的边界

同路径替换：

```text
游戏原本加载 /Game/Characters/Player/051_female/player_051_female_skin
你的 Pak 提供同路径 SkeletalMesh
游戏自然加载你的资产
```

适合：

- 替换 mesh
- 替换 skeleton
- 替换 physics asset
- 替换材质实例
- 替换贴图
- 替换后期动画蓝图引用

新增资产：

```text
/Game/Mods/MyCharacter/BP_ModController
/Game/Mods/MyCharacter/WBP_ModPanel
/Game/Mods/MyCharacter/MI_Skirt_Red
```

必须被某个已加载资产引用，或在蓝图运行时显式加载/Spawn。最稳入口是替换 SkeletalMesh 引用 PostProcessAnimBP，再由它引用新增资产。

删除资产：

Pak 不能真正删除游戏原资产。只能让替换后的资产不再引用它，或用空/透明/占位资源覆盖同路径。

## UE 能打包什么

UE 可以 Cook/Package 项目中的可编辑资产：

- SkeletalMesh
- Skeleton
- PhysicsAsset
- Material / MaterialInstance
- Texture
- Blueprint / Widget Blueprint / Anim Blueprint
- DataAsset / DataTable
- AnimSequence / Montage

但前提是：

- 资产在当前 UE 工程中可加载。
- 资产引用的 class/module/plugin 存在。
- 资产不是 editor-only。
- 依赖能被 Cook 发现，或被显式列入 AlwaysCook/PrimaryAsset/地图引用/蓝图引用链。

如果蓝图引用 `/Script/HTGame` 或 `/Script/KawaiiPhysics` 中游戏有、空项目没有的类型，则必须在 Mirror Project 中提供 stub/兼容插件，否则无法编译或 Cook。

## 简便打包方法

推荐先走 UE 正规 Cook，再提取 cooked 输出做 Pak，而不是手写二进制 uasset。

最低可执行路线：

1. 建立 UE 5.6.1 Mirror Project。
2. Content 路径按游戏虚拟路径摆放：

   ```text
   Content/Characters/Player/051_female/...
   ```

3. 放入/生成替换资产。
4. 用 UE Editor 或 commandlet Cook Windows 内容。
5. 从 `Saved/Cooked/Windows/.../Content/...` 取出 `.uasset/.uexp/.ubulk`。
6. 用 UnrealPak 或现有 MOD 打包器生成 Pak。
7. Pak 内路径对应：

   ```text
   HT/Content/Characters/Player/051_female/...
   ```

若游戏使用 IoStore（`.ucas/.utoc`），仍可观察它是否加载外部 `.pak` patch；如果现有 Mod 打包器已能生效，应复用其路径和 mount 规则，不重复造轮子。

## 可复用轮子

- FModel/CUE4Parse：读取 cooked 包、导出 JSON/PSK/贴图、理解 usmap。
- UE4SS UHT Dumper：生成 mirror project 所需 UHT headers。
- UE4SS usmap Dumper：生成 unversioned property mappings。
- UAssetGUI/UAssetAPI：低层修改 cooked uasset，适合作为补丁路线或调查工具，不是主编辑路线。
- UnrealPak / 现有 MOD 打包器：打包 cooked 输出。
- 公开 KawaiiPhysics：算法参考，但 NTE 需要兼容字段 patch/fork。

## NTEBuildTool 最小改动

第一阶段：已有 PhysicsAsset 重建补全

- 完整 body shape 支持。
- 完整 constraint 参数支持。
- collision disable table 支持保持。
- “新增骨骼链生成 physics body/constraint 模板”。

第二阶段：Kawaii schema 调研落地

- 读取 usmap 中 `/Script/KawaiiPhysics` 类型。
- 生成字段差异报告。
- 生成 `NTEKawaiiConfig` DataAsset。
- 从 AnimBP JSON 导入 Kawaii 节点配置到 DataAsset。
- 等兼容 Kawaii 插件存在后，再应用到真实 AnimGraph 节点。

第三阶段：MaterialInstance 重建

- 读取 MI 参数 JSON。
- 创建 MI。
- 处理 parent 不存在时的同路径壳材质。
- 批量应用到 SkeletalMesh material slots。

第四阶段：PostProcessAnimBP 模板

- 创建标准 PP AnimBP 模板。
- 生成 `BP_ModController` 模板。
- 生成材质切换数组、slot index、按键配置。
- 确保新增资产被强引用。

第五阶段：Mirror Project/stub 辅助

- 接收 UE4SS UHT dump。
- 生成/整理 `/Script/HTGame` stub module。
- 生成缺失类型报告。
- 记录哪些类型只需要编译引用，哪些需要真实运行时实现。

## 一版可执行流程

1. FModel 导出目标角色：

   ```text
   SkeletalMesh PSK
   Skeleton uasset/json if available
   PhysicsAsset JSON
   AnimBP/PostProcess/AnimLayer JSON
   MaterialInstance/Texture JSON or exports
   usmap
   ```

2. DCC 制作：

   ```text
   修改模型
   增删骨骼
   刷权重
   制作 Morph Target
   分配材质 ID/section
   导出 FBX/PSK
   ```

3. UE Mirror Project：

   ```text
   UE 5.6.1
   安装 NTEBuildTool
   安装/生成 HTGame stubs
   安装/patch KawaiiPhysics_NTE
   ```

4. 导入模型：

   ```text
   导入 SkeletalMesh/Skeleton/Morph
   检查材质 slots
   保存到游戏同路径 Content 目录
   ```

5. 重建资产：

   ```text
   NTEBuildTool 导入 PhysicsAsset JSON
   NTEBuildTool 导入/创建 MI
   NTEBuildTool 导入 Kawaii 配置 DataAsset
   创建或应用 PostProcessAnimBP
   ```

6. 蓝图入口：

   ```text
   SkeletalMesh.PostProcessAnimBlueprint = PP_XXX_AnimBP
   PP_XXX_AnimBP 引用 BP_ModController
   BP_ModController 引用所有 MI / Widget / DataAsset
   ```

7. Cook：

   ```text
   UnrealEditor.exe <Project>.uproject -run=cook -targetplatform=Windows
   ```

8. Pak：

   ```text
   使用现有 MOD 打包器或 UnrealPak
   保持 HT/Content/... 路径
   设置 patch pak 优先级高于原 pak
   ```

9. 测试：

   ```text
   先测试 mesh 替换
   再测试材质
   再测试 PhysicsAsset
   再测试 PostProcessAnimBP
   最后测试 UI/按键/多材质切换
   ```

## 风险点

- KawaiiPhysics 字段不兼容是最高风险。
- 材质 parent 不存在会导致 UE 预览/编译困难。
- 主 AnimBP 替换风险高，先避免。
- 新增资产未引用会被 Cook 裁掉或游戏不加载。
- Morph 名称不一致会导致表情/脸部逻辑失效。
- 同路径替换必须注意 patch pak 优先级。
- 游戏如果有资源完整性检查，外部 Pak 生效策略需另行验证。

## 当前推荐下一步

先做一条最小可验证链路：

```text
051_female 的一条头发或裙摆骨骼链
    -> DCC 生成同名/新增骨骼和权重
    -> NTEBuildTool 重建 PhysicsAsset body/constraint
    -> 手动或 DataAsset 保存 Kawaii 参数
    -> PostProcessAnimBP 挂载
    -> Cook/Pak 测试
```

这条链路跑通后，再扩大到整角色、材质切换、UI 面板和多服装状态。
