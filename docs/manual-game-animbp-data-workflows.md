# 原游戏 AnimBP、Appearance 与 Data 资产手工工作流

状态：研究/Diagnostics 文档，不属于主 GUI 自动化路径

日期：2026-08-01

## 目的

源游戏的附加 Mesh、UI 预览、大世界角色和 NPC 可能通过不同的注册链创建组件。FModel 能告诉我们资产字段和引用关系，UE4SS 能告诉我们运行时实际创建了什么，但“能从 JSON 读到一个字段”不等于“当前角色装配器消费了这个字段”。

本文保存这条研究链路，供专家在确实需要修改原游戏装配数据时手工执行。NTEBuildTool 主 GUI 不会隐式执行这些步骤，也不会因为用户没有研究某个 DataTable 就拒绝打包一个已经存在的 replacement Mesh。

## 三条独立的运行时入口

### 合并主 Mesh + Post Process

```text
原 AnimBP（ExternalReference）
  -> replacement SkeletalMesh 的基础姿势
  -> replacement SkeletalMesh 的 Post Process AnimBP
  -> Kawaii / Runtime declarations
  -> OutputPose
```

这是当前推荐主路径。UI、大世界和 NPC 只要最终引用同一个 replacement Mesh，就共享这个 Post Process。它不要求修改 `HTPlayerAppearance`、PlayerUIShow SCS 或源 AnimBP。

### UI PlayerUIShow SCS

```text
DT_AppearanceData.UIActorClass
  -> PlayerUIShow_* Blueprint
  -> native Mesh component
  -> SCS HTSkeletalMeshComponentBudgeted child
  -> attached Mesh / UI AnimBP
```

UI 和大世界可以共享主 Mesh，但它们不是同一个组件创建链。UI SCS 成功不能证明世界 Appearance 成功，反之亦然。

### 大世界 Appearance / NPC Blueprint

常见的两种原生形态：

```text
MeshAsset_* / HTPlayerAppearance
  -> FashionMeshData
  -> ArrayFashionAttachedMeshData
  -> world appearance assembler
  -> attached component
```

或：

```text
NPC/WA Blueprint
  -> native CharacterMesh0
  -> SCS HTSkeletalMeshComponentBudgeted child
```

具体角色必须用 UE4SS 确认 active actor 和实际 owner。不能因为某个 `MeshAsset_*` 有 attached array，就假定当前 NPC 使用它。

## FModel 静态研究步骤

### 1. 选择官方样本

从同一角色或相近角色收集：

- 主 SkeletalMesh；
- attached SkeletalMesh；
- attached Skeleton；
- PhysicsAsset；
- attached AnimBP/AnimLayer；
- `MeshAsset_*` 或 `HTPlayerAppearance`；
- UI `PlayerUIShow_*`；
- 相关 Appearance DataTable/DataAsset；
- 目标世界/NPC Blueprint。

扫描必须从完整 Game Container Scan Root 开始：

```text
F:\Neverness To Everness\Client\WindowsNoEditor
```

只扫 `HT\Content\Paks` 不能证明 PatchPaks、TagPatchPaks 或 Mods 中没有更高优先级引用。

### 2. 建立引用矩阵

每个样本记录：

| 对象 | 官方路径 | 关键字段 |
|---|---|---|
| Main Mesh | `/Game/...` | Skeleton、AnimClass、PostProcessAnimBlueprint、材质槽 |
| Attached Mesh | `/Game/...` | Skeleton、AnimBP、MobileAnimBP、PhysicsAsset |
| MeshAsset | `/Game/...` | `FashionMeshData`、`ArrayFashionAttachedMeshData` |
| PlayerUIShow | `/Game/...` | native actor class、Mesh、SCS child、parent、socket |
| World/NPC BP | `/Game/...` | native main component、SCS child、AnimClass |
| Appearance Data | `/Game/...` | active character/outfit selection、UIActorClass |

JSON 报告只保存 source reference 和摘要；完整 exports 放在外部研究目录，不进入 Pakmod Project。

### 3. 确认字段 schema

对于 `/Script/HTGame.HTPlayerAppearance`，Mirror Project 的 stub 必须按游戏 usmap 对齐：

- 继承关系；
- property 名称；
- property 类型；
- declaration order；
- `FashionMeshData` 的 `CharacterMeshData` struct；
- `ArrayFashionAttachedMeshData` 的 `AttachedMeshData` inner type；
- Anim blueprint 引用的 ObjectProperty/ClassProperty 形态。

Schema 不一致时，UE 可能能保存一个看似正确的 asset，游戏加载却报告 struct/array inner type mismatch。此类修复属于 manual/Diagnostics，不能藏在普通 Build 中。

## 手工修改原游戏 AnimBP

### 何时才需要

只有以下情况才考虑替换源 AnimBP：

- Post Process 不能满足目标行为；
- 目标组件只接受某个特定 AnimClass，且无法使用自身生成的 attached AnimBP；
- 需要修改基础姿势而不是在基础姿势之后加 Kawaii；
- 通过运行时证据证明一个字段只能由该 AnimBP 消费。

### 操作原则

1. 从官方 JSON 和 class path 建立 reference-only baseline。
2. 只复制必要的 graph/event declarations。
3. 保留官方 Skeleton、preview mesh、parent class 和输入姿势策略。
4. 不为了加入 Kawaii 重建整张源图。
5. 生成的 AnimBP 使用明确的 `ToolGenerated + ReplacementAsset/AddedAsset` Intent。
6. 在 UE 中打开并编译，随后用静态 inspection 检查输入、输出和 node count。
7. 用 UE4SS 检查运行时实际 `AnimInstance` class，而不是只读 cooked JSON。

### 典型 T Pose 根因

- imported Mesh 多出一个 FBX Armature pseudo-root；
- replacement Skeleton 与官方 AnimBP 不兼容；
- 重建了错误的 world/UI AnimBP；
- CopyPose 的 source/attached parent 设置错误；
- Post Process 被误当成基础 AnimBP；
- graph output 没有连接到 `OutputPose`。

遇到 T Pose 时，优先确认 Mesh、Skeleton、基础 AnimClass 和 Post Process class 的运行时组合，不要先复制更多 DataTable。

## 手工修改 HTPlayerAppearance / MeshAsset

### 适用场景

这条链适用于原生 world appearance assembler 确实会读取 `ArrayFashionAttachedMeshData` 的角色。它不是合并主 Mesh + Post Process 的必需步骤。

### 关键数据

每个 attached entry 至少核对：

- attached SkeletalMesh；
- desktop AnimInstance；
- mobile AnimInstance；
- Skeleton；
- `SocketName`；
- relative transform；
- component-owned tags；
- visibility/enable fields（若 schema 存在）。

`SocketName=None` 不是“没有挂点”的安全默认值。应使用目标主 Mesh 确实存在的 mount，例如 `root` 或官方样本使用的 pelvis socket，并保留 UE `FName` 语义。

### 手工链路

```text
官方 MeshAsset JSON/usmap
  -> Mirror Project HTGame schema stub
  -> 复制官方字段顺序和类型
  -> 只修改 main/attached tuple
  -> UE 新进程重新加载
  -> 静态 inspection
  -> separate process cook/package
  -> UE4SS 检查 active appearance actor
```

必须使用独立 UE 进程验证 writer 与 package。AssetRegistry 在同一进程内可能仍持有旧依赖，导致看似成功的 package 缺少新引用。

### 运行时判定

UE4SS 观察顺序：

1. active actor 是否真的加载了目标 MeshAsset/Appearance；
2. 是否拥有预期 attached component；
3. component 的 Mesh、Skeleton、AnimInstance 是否正确；
4. socket/relative transform 是否正确；
5. `IsVisible`、`bHiddenInGame`、render flags、`WasRecentlyRendered`；
6. 最后才检查几何位置、权重和 Kawaii pose。

没有 component 是注册链问题；有 component 但隐藏是 visibility/tuple 问题；有可渲染 component 但几何异常才进入 Mesh/Skeleton/AnimBP 诊断。

## 手工修改 PlayerUIShow SCS

### 原生形态要求

UI Blueprint 必须保持游戏的 native actor class。用 `ACharacter` 或通用 Actor 替代会改变 native component/export 形状，可能导致 cooked Blueprint 不兼容。

### 操作步骤

1. 读取官方 `PlayerUIShow_*` 的 native root 和 Mesh component 名称。
2. 找到 exact parent skeletal-mesh component；不猜名字。
3. 添加一个 `HTSkeletalMeshComponentBudgeted` SCS child。
4. 设置 attached Mesh、Skeleton-compatible AnimClass、socket/relative transform、tags。
5. UI 需要独立 pose 时使用官方 UI attached AnimBP；不要把世界 AnimBP 塞进 UI。
6. 编译、保存、单独 cook。
7. UE4SS 检查 UI character actor 下的具体 component owner、Mesh、AnimInstance 和 visibility。

UI SCS 是独立 presentation target。它与 world `HTPlayerAppearance` 可以由同一 Pakmod Project Recipe 生成，但不能以“UI 已出现”作为 world 成功信号。

## 手工修改世界/NPC Blueprint SCS

只有 UE4SS 已证明 active world actor 不消费 Appearance attached array，且该 actor 确实需要 Blueprint-level child 时，才使用该路径。

步骤：

1. 用 UE4SS 找到 active actor 的 Blueprint class 和 native main component；
2. 读取官方 Blueprint SCS，确认 exact parent（例如 `CharacterMesh0`）；
3. 添加一个 child `HTSkeletalMeshComponentBudgeted`；
4. 设置 Mesh、AnimBP、socket/relative transform、tags；
5. 不覆盖官方主 AnimBP，除非运行时证据要求；
6. 只把这个 Blueprint 标成 `ReplacementAsset`，不要同时注册同一个 attached Mesh 到 Appearance array，避免重复组件；
7. 重启游戏进程，确认 active actor owner 只拥有预期的一个 child。

Nanally 的历史样本证明，Appearance array 和 world Blueprint SCS 同时注册同一 mesh 会产生 duplicate world registration；当前合并主 Mesh + Post Process 路径不需要两者。

## 手工修改 Appearance DataTable/DataAsset

### 研究目的

Appearance DataTable/DataAsset 只用于确定：

- 哪个 character/outfit row 被加载；
- main MeshAsset 的 exact path；
- UI `UIActorClass`；
- 是否存在 NPC/world appearance override；
- native assembler 是否会读取 attached arrays。

### 不要默认修改

若 replacement Mesh 在原有 package path 被加载，优先只替换该 Mesh，并让它引用 added Post Process assets。不要为了“确保游戏看到新 Mesh”去复制整张 DataTable 或全角色 Appearance asset。

Data asset 修改的风险包括：

- struct schema/type mismatch；
- row handle 或 soft object path 不一致；
- UI 和 world 选到不同 row；
- 包含大量无关依赖，造成内存和容器优先级问题；
- 把本应 ExternalReference 的官方 AnimBP 变成错误的 replacement。

只有运行时证据明确指向 row/asset 选择错误时，才制作最小 replacement，并将改动写入 Diagnostics 报告。

## FModel + UE4SS 全链路报告

一次完整的手工研究应输出：

```text
SourceReferenceMatrix.json
  - official package paths
  - selected container/read order
  - schema/property summaries
  - source AnimBP/Kawaii summaries

RuntimePresentationReport.json
  - active actor path
  - main component owner/name
  - attached component owner/name
  - mesh/skeleton/AnimInstance
  - socket/transform
  - visibility/render flags
  - WasRecentlyRendered

PackageManifest.json
  - explicit replacement/added assets
  - external references excluded
  - output hashes
```

静态报告回答“包里有什么”；UE4SS 报告回答“运行时谁真正创建并渲染了什么”。两者必须分开保存。

## 与主 GUI 的关系

主 GUI 只提供：

- Game Reference Library 搜索；
- source reference 摘要；
- 可选 Diagnostics profile；
- 将手工制作好的 UE asset 添加到 Package Manifest。

主 GUI 不提供默认的：

- 源 AnimBP 整图复制/替换；
- `HTPlayerAppearance` 自动改写；
- PlayerUIShow/NPC SCS 自动猜测 parent；
- DataTable 全量复制；
- UE4SS 进程控制。

这样保持了 Package Builder 的 Depth，也避免用户为了打包手工资产被游戏 schema、角色特例和运行时探针绑住。
