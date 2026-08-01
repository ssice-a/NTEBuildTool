# 合并主 Mesh 的 Post Process Kawaii 工作流

状态：已验证的主路径

日期：2026-08-01

## 适用范围

本文定义 NTEBuildTool 对“主服装 Mesh 与额外物理网格已合并为一个 SkeletalMesh”的默认 Kawaii 制作和打包流程。目标 Mesh 使用扩展 Skeleton，既包含原角色骨骼，也包含用户新增的物理骨骼链。

这条路径已经解决了此前附加 Mesh 注册链、UI/大世界组件创建差异和 CopyPose 对齐带来的额外复杂性。它不要求替换源游戏主/UI/NPC AnimBP，也不要求通过 `HTPlayerAppearance` 或 Blueprint SCS 另外挂载一个 Mesh。

附加 Mesh + CopyPose + 独立 AnimBP 仍可作为高级路线，但不是本文主路径。

## 为什么使用 Post Process AnimBP

同一个 replacement SkeletalMesh 可以被 UI、大世界玩家或 NPC 等不同对象引用。每个对象仍使用游戏原本选择的 AnimBP 产生基础动作。SkeletalMesh 上配置的 Post Process AnimBP 在基础动画之后执行，因此：

- 不需要知道每个消费者具体使用哪个源 AnimBP；
- 不会把 UIAnimBP、Player AnimBP 或 NPC AnimBP 替换成工具重建版本；
- 同一 replacement Mesh 的所有消费者获得同一套额外 Kawaii；
- Runtime EventGraph 和 Kawaii AnimGraph 可以存在于同一个工具生成 Post Process AnimBP；
- 纯 pak 只需要 replacement Mesh 引用 added Post Process AnimBP。

Post Process AnimBP 不是基础动画的替代品。它必须从 `LinkedInputPose` 接收源游戏已经求出的姿势。

## 资产角色

| 资产 | Origin | Intent | 作用 |
|---|---|---|---|
| 游戏原始 Mesh/AnimBP/Kawaii JSON | `GameReference` | `ExternalReference` | 提供路径、材质和物理参数参考 |
| 用户合并后的 SkeletalMesh | `UserImported` | `ReplacementAsset` | 替换游戏原 Mesh，并携带扩展 Skeleton 和新权重 |
| 用户导入 Skeleton | `UserImported` | `ReplacementAsset` 或 `AddedAsset` | 取决于 Mesh 最终引用路径 |
| 生成的 Post Process AnimBP | `ToolGenerated` | `AddedAsset` | 接收基础姿势并运行 Kawaii/Runtime |
| 生成的 Limits/Constraints/Curve | `ToolGenerated` | `AddedAsset` | Kawaii 节点使用的参数资产 |
| 游戏原始父材质和未修改贴图 | `GameReference` | `ExternalReference` | 由 replacement Mesh/MI 引用，不打入 mod |
| 用户贴图和生成 MI | `UserImported`/`ToolGenerated` | `AddedAsset` | 只打包实际修改或新建的内容 |

## 必须保持的骨架条件

### replacement Mesh 的 Skeleton

合并后的 Mesh 必须使用一个能同时表示以下内容的 Skeleton：

- 源角色原有骨骼；
- 新增物理链；
- 蒙皮权重引用的确切骨名；
- Kawaii Recipe 引用的 root、additional root、exclude、limit driving 和 constraint bones。

FBX 骨名与 UE 导入后的 Skeleton 骨名必须对应。若 UE 导入时规范化名称，Recipe 的 bone remap 必须以 UE 中实际骨名为 target。不要在 Kawaii writer 内用模糊后缀猜测骨名。

### 原骨与新增骨的姿势来源不同

游戏原 AnimBP 认识原骨并为其输出正常姿势，但它通常不知道新增骨链。`LinkedInputPose` 对新增骨的 component-space transform 可能未被可靠初始化。Kawaii 会以 incoming transform 作为模拟初态和边界条件；无效初态会导致链条爆炸。

因此不能直接使用：

```text
LinkedInputPose
-> LocalToComponentSpace
-> Kawaii
-> ComponentToLocalSpace
-> OutputPose
```

需要在进入 Kawaii 前，仅为新增链的安全 anchor 恢复 reference pose。

## 正式 AnimGraph

```text
                           LocalRefPose
                               |
LinkedInputPose -> LayeredBoneBlend
                         |
                 LocalToComponentSpace
                         |
                 Kawaii node 1
                         |
                 Kawaii node 2 ...
                         |
                 ComponentToLocalSpace
                         |
                     OutputPose
```

规则：

1. `LinkedInputPose` 是 base pose，保留所有游戏原骨动画。
2. `LocalRefPose` 只通过 `LayeredBoneBlend` 覆盖 topology-safe custom-chain anchors。
3. 每个 branch filter 使用 `BlendDepth=0`。
4. 稳定完成后才转换到 component space。
5. Kawaii 节点按 Recipe 的明确顺序串联。
6. 最后转回 local space 并连接 `OutputPose`。
7. 不使用 `CopyPoseFromMesh`，因为当前就是主 Mesh 自身的 Post Process。
8. 不把整个 Skeleton 恢复成 reference pose。

## topology-safe anchor 算法

只对 Kawaii Recipe 声明的自定义 root 和 additional root 计算 anchor。

对每个模拟 root：

1. 从 root 向父级移动。
2. 当当前父骨只有一个 direct child 时，可以继续向上，把固定父 anchor 一并纳入 reference-pose branch。
3. 在即将进入共享分支前停止，例如 `Bip001-Pelvis` 这类同时拥有多个角色骨骼分支的骨骼。
4. 选择停止点以下的最上层自定义 anchor 作为 branch root。
5. 合并重复 anchor，并保持确定性排序。

伪代码：

```text
resolveAnchor(simulatedRoot):
  anchor = simulatedRoot
  while parent(anchor) exists:
    p = parent(anchor)
    if directChildCount(p) != 1:
      break
    if p is an original/shared animation branch:
      break
    anchor = p
  return anchor
```

Nanally 的运行时证据表明，只重置 `_002_001` 模拟 roots 不够；它们未模拟的固定 `_001_001` 父 anchor 也需要有效 transform。算法必须根据 topology 找到这些 anchor，而不是把某个具体后缀硬编码进 core。

## Kawaii 求解起点

Kawaii 节点从 Recipe 中声明的 `RootBone` 和 `AdditionalRootBones` 开始建立模拟链。root 的父骨不会自动成为模拟骨，但它的 component-space transform 是整条链的边界条件。

因此需要区分：

- simulated roots：Kawaii 节点实际积分和约束的骨骼；
- fixed anchors：不参与模拟，但必须在 Kawaii 前有正确 transform 的父骨；
- shared original ancestors：pelvis 等由游戏基础动画控制，不能被 reference-pose branch 覆盖。

物理爆炸并不必然表示 damping/stiffness 数值错误。若同一参数在附加 Mesh 路线正确、在 Post Process 路线爆炸，应首先比较 incoming pose、fixed anchor 和 graph order。

## Kawaii Recipe 数据

一个 `KawaiiPostProcess` Recipe 只持久化关键输入和用户 delta：

```json
{
  "Id": "kawaii_main_qun",
  "Type": "KawaiiPostProcess",
  "Enabled": true,
  "Sources": ["lacrimosa_nighty_qun_preset"],
  "Targets": ["main_mesh"],
  "Deltas": {
    "BoneRemap": [
      {"Source": "Bn_l_qunB_002", "Target": "Bn_l_qunB_002_001"}
    ],
    "NodeOrder": ["qun_back", "qun_front"],
    "Parameters": {}
  },
  "Outputs": [
    "main_post_process_abp",
    "qun_limits",
    "qun_constraints"
  ]
}
```

Recipe 不保存：

- 展开后的整份源游戏节点 JSON；
- Blueprint 节点坐标；
- 自动计算的 anchor 列表；
- skeleton 扫描报告；
- diagnostics；
- cook/package 结果。

这些内容由 Apply plan 或 Diagnostics 推导。

## 从游戏 preset 复制参数

Game Reference Library 应允许用户选择 Player/NPC 的某个 AnimBP/AnimLayer 和其中一个或多个 Kawaii 节点。导入流程是：

```text
FModel Kawaii JSON
-> neutral source preset
-> 用户选择 target Mesh
-> bone remap
-> 用户 parameter deltas
-> KawaiiPostProcess Recipe
```

源参数可能包括：

- root/exclude/additional roots；
- damping、stiffness、world damping、gravity、radius 等 settings；
- limits/collision；
- bone constraints；
- curves；
- NTE 魔改字段；
- node execution order。

原游戏参数是 seed，不代表目标模型一定适配。用户仍可以在 UE 原生 Kawaii Details、Persona、Limits 和 Constraints 编辑器中调整。

## Apply

`Apply Changes` 对本 Recipe 执行：

1. 检查 target Mesh 和 Skeleton 可加载。
2. 解析 source preset；source 不可用则本 Recipe Apply disabled。
3. 应用 bone remap 和参数 delta。
4. 计算 topology-safe anchors。
5. 生成 Limits/Constraints/Curve declarations。
6. 向 Generated Blueprint Composer 提交 Post Process AnimGraph declarations。
7. Composer 更新本 Recipe marked nodes，不删除 Runtime 或用户节点。
8. 编译并保存 outputs。
9. 在 replacement Mesh 上设置生成的 Post Process AnimBP。
10. 更新 `Saved/NTEBuildTool` fingerprint 和 Apply result。

Apply 不自动加入 Package Manifest，也不自动 Build。GUI 可以提供一次性命令“Apply 后将 outputs 加入 Manifest”，但它必须展示将加入的资产，不能成为隐藏行为。

## Runtime Recipe 与同一 Post Process AnimBP

如果运行时按键和 UI 逻辑也由 Post Process AnimBP 承载：

- Kawaii Recipe 只提交 AnimGraph declarations；
- Runtime Recipe 只提交 EventGraph declarations；
- Runtime UI Recipe 提交 Widget/SaveGame declarations；
- Generated Blueprint Composer 统一保存和编译目标资产。

任意 Apply 次序都必须得到相同结果。删除 Runtime Recipe 只能删除 Runtime marked declarations；Kawaii graph 仍存在。反之亦然。

## Native Edit、Sync 与 Detach

### Native Edit

用户可以打开：

- Post Process AnimBP 的 Kawaii nodes；
- Limits DataAsset；
- BoneConstraints DataAsset；
- Curve assets。

Persona/Kawaii edit mode 是碰撞、限制和约束的主要可视化调参界面。Project GUI 不应复制整个原生编辑器。

### Sync

Sync 读取 marked Kawaii nodes 和 Recipe-owned DataAssets：

- 能可靠与 source preset 比较时，只保存用户 delta；
- 无法可靠 diff 时，保存 `Mode: Snapshot` 的明确快照；
- 自动计算的 topology anchor 不作为用户 delta；
- sync 成功后更新 Recipe，不自动重新 Apply。

### Detach

用户要完全手工维护生成资产时可以 Detach：

- output 的 `OwnerRecipeId` 清空；
- 后续 Apply 不覆盖它；
- 它仍可留在 Package Manifest；
- Recipe 若失去关键 output，状态显示为 unconfigured 或 detached。

## Build

Build 只读取 Package Manifest 和磁盘资产。对于本流程，Manifest 通常至少包含：

- replacement SkeletalMesh；
- replacement/added Skeleton（如果 cooked Mesh 需要）；
- generated Post Process AnimBP；
- generated Kawaii DataAssets/curves；
- 用户新增贴图和生成 MI；
- 其他真正被 replacement 链引用的 added assets。

游戏原 AnimBP、原材质、未修改贴图和其他 ExternalReference 不进入包。

即使 Recipe 状态是 `source changed`，Build 也使用当前磁盘版本。GUI 必须显示这一事实，但不能偷偷 Apply。

## 默认检查

Apply 的最低检查：

- target/output path 合法；
- target Mesh/Skeleton 存在；
- Recipe 声明的 target bones 在 Skeleton 中存在；
- Kawaii schema 足以保存所需字段；
- Composer 能编译并保存目标 Blueprint。

Build 的最低检查由 package core 负责，不重复检查 graph topology。

## 可选 Diagnostics

- Skeleton hierarchy、root/anchor 可视化；
- Recipe bone refs 与实际 UE bone index；
- exact graph topology 与 node order；
- limits/constraints 引用；
- source preset provenance；
- replacement Mesh 是否引用预期 Post Process class；
- cooked package 中的 class path 和容器优先级；
- UE4SS 运行时实例、root bone index 和 UI/world Post Process class。

这些 Diagnostics 适用于“物理爆炸”“只有某个消费者不运行”“打包后引用错误”的问题，不属于每次 Apply/Build 的默认成本。

## 常见失败与解释

### 角色 T Pose

通常先检查是否错误替换了源 Player/UI/NPC AnimBP，或 replacement Mesh/Skeleton 无法实例化源 AnimBP。Post Process 本身不应替代基础 pose。

### 所有 Kawaii 链爆炸

检查 `LinkedInputPose` 后新增 bones 是否有有效 transform、fixed anchors 是否纳入 reference-pose stabilizer、空间转换顺序是否正确。不要先假定所有物理参数同时坏了。

### 只有某条链异常

检查该 Recipe 的 root、additional roots、bone remap、权重、父子层级、limits/constraints 与 node order。

### UI 和大世界同时相同异常

若两者使用同一 replacement Mesh 和 Post Process class，这通常说明共享的 Post Process graph、Skeleton 或 Kawaii 数据有问题，而不是 UI 单独注册问题。

### UI 正常、大世界异常或相反

通过 Diagnostics 确认两个消费者实际使用的 Mesh、Skeleton、基础 AnimBP 和 Post Process class。不要仅凭资产配置推断运行时实例。

### Recipe source 丢失

禁用重新 Apply/Sync source delta；已经存在的 UE outputs 和 Package Manifest 仍可以 Build。

## 不应再引入的 fallback

- 因 Post Process Kawaii 失败而自动重建所有源 AnimBP；
- 自动把主 Mesh 拆回 attached Mesh；
- 用骨名后缀猜测 remap；
- 自动把 pelvis 或整个 Skeleton 恢复 reference pose；
- 打包时临时生成或修复 Kawaii graph；
- 在 Build preflight 中运行 UE4SS/FModel/全骨架深检；
- 将特定角色、特定骨名或特定节点数硬编码到 core。

失败应留在对应 Module：Recipe Apply 失败就报告 Recipe，Composer 编译失败就保留旧资产和失败结果，Build 只报告当前磁盘资产的打包结果。
