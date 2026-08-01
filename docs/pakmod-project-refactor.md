# Pakmod Project 重构设计

状态：package-first 主链路已实现；Generated Blueprint Composer 深化与旧 Adapter 最终移除留作后续独立阶段

日期：2026-08-01

关联决策：`docs/adr/0002-package-first-pakmod-project.md`

## 2026-08-01 实施结果

本轮已经完成 Phase 1、Phase 2、Phase 3 与 Phase 5 中可在不重写稳定 writer 的前提下完成的内容：

- 已实现 `NTE.PakmodProject v1`、JSON round-trip、版本拒绝、Origin/Intent 分离和 `CharacterModSpec` 单向迁移；
- 已实现 Manifest-only 的 Package Plan 与 Package Job 创建，Build 不再隐式调用 Apply、Sync 或 authoring preflight；
- Manifest 解析按用户声明的 Asset ID 顺序稳定去重，不用字母排序改写显式 package 顺序；
- 已实现持久 Nomad tab，统一 Project、Assets、Materials、Runtime、Physics、Kawaii、Package、Build 页面；
- 已实现 Material、Runtime、PhysicsAsset、Kawaii/Post Process Recipe 的显式 Apply，以及 Kawaii Sync 和通用 Detach；
- 已将编辑器主菜单收敛到 `Open Pakmod Project`，删除旧 modal Workspace 与 `ApplyCopyPoseOnly` 实验分支；
- 已将游戏字体与按钮资源移入 `StyleProfileId` 解析，项目只持久化必要 override；
- 已保留已跑通的 Post Process Kawaii writer，并把原游戏 AnimBP/Data/DataTable/SCS 修改留在手工与 Diagnostics 文档；
- 已新增项目模型、Manifest 和迁移自动化测试；严格无 Unity/无共享 PCH 的 Editor、Game Development、Game Shipping 构建均通过。

当前 `NtePakmodRecipeAdapter` 是有意保留的过渡 Adapter：它把 Recipe 投影到已经验证的 Character writers，以避免本轮同时重写 Kawaii、Runtime 和材质资产写入算法。它不属于 Package core，且 Build 永远不会调用它。Phase 4 的 Generated Blueprint Composer 和 Phase 6 的 writer 内部深化，必须在 marker ownership 与交错 Apply 自动化测试建立后再做，不能只为减少函数数量而提前拆分。

## 目标

NTEBuildTool 的核心目标是把用户已经配置好的 UE 资产打包为 NTE 可加载的 `.pak/.utoc/.ucas`。材质实例、运行时按键/UI、PhysicsAsset、Kawaii 和 Blueprint 自动化继续完整保留，但它们是服务于最终打包的可选 Authoring Recipe，不是打包的前置条件。

本轮重构要同时获得：

- 一个不会因执行一次动作就丢失上下文的统一 GUI；
- 一个只记录关键信息的轻量项目格式；
- Apply 与 Build 的严格分离；
- 游戏原始资源、用户导入资源、工具生成资源的明确区分；
- 可安全组合 Runtime EventGraph 与 Kawaii AnimGraph 的 Blueprint 写入方式；
- 可以继续添加动画、Morph Target 等功能，而不扩大打包核心 Interface；
- 删除旧 fallback、过度验证和失效分支的明确顺序。

这不是按文件尺寸拆分代码。重构以 Module 的 Depth 为目标：小 Interface 隐藏足够多的 Implementation，给调用者 Leverage，并让变更和故障保持 Locality。

## 当前摩擦的源码证据

### 打包知道太多 authoring 细节

`NteModPackagePlan.cpp` 的 `CollectEffectiveCharacterPackageSeeds` 直接创建 Material、Runtime Action 和 Kawaii plan；`BuildCharacterModSpecPackagePreflight` 又验证 Appearance、Material、Runtime、Kawaii 及具体 Post Process 图规则。

这使打包 Module 的 Interface 实际上等于整个角色制作流程。删除这些调用并不会让复杂性消失，只会让每个 authoring writer 自己声明输出；因此 Package Manifest 是更有 Depth 的 seam。

### 一个持久化结构聚合所有功能

`NteCharacterModSpec.h` 的 `FNteCharacterModSpec` 同时拥有 Appearance、主 Mesh、附加 Mesh、材质、Runtime UI、Kawaii 和 Package。每新增一种 authoring 功能都会扩大所有加载、保存、验证、GUI 和 commandlet 的 Interface。

该结构已经通过删除测试：如果删除它，真正需要保留的是独立 Recipe 和 Package Manifest，而不是在每个调用点重新构造同样的大型角色规格。

### GUI 是动作分发器，不是项目工作区

`NteCharacterWorkspaceDialog.cpp` 每次打开都会调用 `MakeDraftCharacterSpec`。多个按钮通过 `CloseWithAction` 关闭 modal 窗口再执行操作。用户只想更新一个材质或 Kawaii 参数时，也容易失去其他未保存状态。

### 多个 writer 可能拥有同一 Blueprint

Runtime writer 生成 EventGraph、Widget 和 SaveGame；Kawaii writer 生成 AnimGraph。当前两者都是大型 Implementation，并可能面对同一 host AnimBP。执行顺序和“整图重建”不能成为调用者必须知道的 Interface。

### 验证侵入正常流程

骨架对比、精确节点拓扑、FModel 来源、容器优先级和角色特例对诊断很有价值，但它们不应该阻止用户打包已经存在且可 Cook 的 UE 资产。Nanally 的材质槽和链路检查属于验证基线或 Diagnostics，不属于通用 core。

### 入口和命令参数持续膨胀

`NTEBuildTool.cpp` 当前暴露多个独立菜单。`NteCharacterModSpecCommandlet.cpp` 通过大量 flag 组合 Apply/Sync/Package 行为。这些 Interface 把执行顺序推给用户和测试。

## 设计原则

1. Package Manifest 是打包核心唯一的领域输入。
2. Authoring Recipe 只在用户显式点击 `Apply Changes` 时执行。
3. `Build Package` 只读取磁盘上的 UE 资产，不隐式 Apply、Sync、迁移或覆盖。
4. Origin 描述资产从哪里来，Intent 描述是否以及如何进入包，两者正交。
5. Recipe 只持久化 source、target、用户 delta 和 outputs，不保存可重新推导的大块结果。
6. 生成资产有明确 ownership；用户可以 Sync 或 Detach。
7. 默认验证只阻止已知必坏包；深度验证进入 Diagnostics。
8. 不为单一 Implementation 预先制造抽象。只有真实存在两种 Adapter 的地方才建立 seam。
9. 原游戏资产的直接重建是高级研究能力，不进入主 GUI 的默认路径。

## 目标 Module

### Pakmod Project Model

Interface：

```text
Load(filename) -> Project
Save(project, filename)
MigrateLegacy(characterModSpec) -> Project + MigrationReport
```

Implementation 负责版本解析、轻量 schema、稳定 ID、枚举合法性和 one-way migration。它不扫描资产、不加载 Blueprint、不构建依赖树。

Depth 来自一个小型序列化 Interface 同时覆盖 GUI、commandlet、恢复和迁移。项目格式错误的处理具有 Locality。

### Recipe Module

每种 Recipe 类型提供同一生命周期语义，但不强制为所有 Recipe 建立一个庞大的 C++ 基类层次。第一阶段可以使用带类型分派的 registry；只有出现第二种真正不同的执行 Adapter 时，再抽取额外 seam。

Interface：

```text
PlanApply(project, recipeIds) -> ApplyPlan
Apply(applyPlan) -> ApplyResult
Sync(recipeId) -> RecipeDelta | ExplicitSnapshot
Detach(recipeId, outputAssetId) -> ProjectChange
```

`PlanApply` 是只读的。`Apply` 只更新计划中列出的 Recipe-owned outputs。`Sync` 优先保存用户 delta；无法可靠 diff 时保存明确标记的 snapshot，不能伪装成 delta。

首批 Recipe 类型：

- `MaterialInstance`
- `RuntimeActions`
- `RuntimeUi`
- `PhysicsAsset`
- `KawaiiPostProcess`
- `StageReplacement`

后续 `Animation` 和 `MorphActions` 以新 Recipe 类型接入，不修改 Package Manifest 的语义。

### Generated Blueprint Composer

Interface：

```text
Compose(targetBlueprint, declarations[]) -> ComposePlan
Apply(composePlan) -> GeneratedAssetResult
```

声明至少包含：

- `OwnerRecipeId`
- `StableDeclarationId`
- `GraphKind`
- `Inputs`
- `Outputs`
- `GeneratedNodeMarker`
- `Payload`

Composer 负责确定依赖次序、查找 marked nodes、只更新对应 Recipe 拥有的节点、编译和保存。各 Recipe writer 只声明意图，不再分别重建整张图。

首批贡献关系：

| Recipe | Blueprint 声明 |
|---|---|
| Kawaii Post Process | AnimGraph 的输入姿势、稳定器、空间转换、Kawaii 节点和 OutputPose |
| Runtime Actions | AnimBP EventGraph 的初始化、输入轮询、状态应用和保存 |
| Runtime UI | Widget 树、按钮绑定、SaveGame 字段和 UI 显隐 |

这条 seam 给调用者高 Leverage：Runtime 和 Kawaii 不再了解彼此的 writer 细节。节点 ownership、重入和编译错误集中在 Composer，获得 Locality。

### Package Manifest Planner

Interface：

```text
BuildPlan(project.packageManifest, assetRegistry) -> PackagePlan
```

它只做：

- 读取 manifest-owned assets；
- 规范化 `/Game/...` package path；
- 排除 `ExternalReference`；
- 展示硬依赖候选和用户显式排除项；
- 生成可编辑的 Package Plan。

它不调用 Recipe plan，也不判断 Kawaii 图、材质槽、骨架或 FModel 来源。Recipe outputs 只有进入 Package Manifest 后才成为包输入。

### Package Builder

Interface：

```text
Build(packagePlan, buildSettings) -> BuildResult
```

Implementation 封装 Cook、response file、IoStore、输出复制和最低限度验收。调用者不需要了解命令拼接、临时目录和容器文件布局。

已知 Blueprint 包版本规则等真正会产生坏包的条件留在这里。角色专用断言不进入此 Module。

### Game Reference Library

这是 FModel/容器数据的只读索引 Module，支持材质、PhysicsAsset、Kawaii 参数和 UI style 搜索。

Interface：

```text
Search(kind, query, filters) -> SourceReferenceSummary[]
Resolve(sourceReferenceId) -> SourceReferenceDetails
```

扫描缓存位于 `Saved/NTEBuildTool`，不写入 Pakmod Project。源根缺失时返回 unavailable 状态；只禁用依赖该 source 的 Apply，不影响 Build。

FModel JSON 与未来直接容器读取形成两个真实 Adapter 后，才共享一个 source-library seam。目前可以先保留现有 FModel Implementation，避免假抽象。

### Diagnostics

Interface：

```text
Run(profileId, project | assets | builtContainers) -> DiagnosticReport
```

Diagnostics 是二级入口。它可以读取 Recipe、FModel、骨架、Blueprint 图和游戏容器，但报告默认不改变项目、不 Apply、不 Build。

Nanally 等角色基线放在外部 profile/data 中，不在通用 C++ 里硬编码。

### Pakmod Project Editor

一个 persistent、non-modal editor tab。它调用上述 Module，不拥有 writer Implementation。关闭和重开 tab 后从最近项目恢复，不创建新 draft 覆盖现有工作。

## `NTE.PakmodProject` version 1

### 顶层结构

```json
{
  "Format": "NTE.PakmodProject",
  "Version": 1,
  "Project": {
    "Id": "nanally_swimsuit",
    "DisplayName": "Nanally Swimsuit",
    "GameProfileId": "nte-default"
  },
  "Sources": [],
  "Assets": [],
  "Recipes": [],
  "PackageManifest": {}
}
```

机器相关的 Unreal Engine、游戏安装和默认 FModel 根路径由 editor settings/profile 解析。项目只在某个 source 必须固定到特殊位置时保存显式 locator。

### Source Reference

```json
{
  "Id": "game_material_cloth_b",
  "Kind": "FModelMaterial",
  "GamePackagePath": "/Game/Characters/Player/010_nanally/.../MI_player_010_female_cloth_b",
  "Locator": "Exports/HT/Content/Characters/Player/010_nanally/...json"
}
```

不保存解析后的全部参数。Recipe Apply 时解析；摘要和 fingerprint 存在 `Saved/NTEBuildTool`。

### Asset Reference

```json
{
  "Id": "main_mesh",
  "PackagePath": "/Game/Characters/Player/010_nanally/mesh/player_010_nanally_skin",
  "Class": "SkeletalMesh",
  "Origin": "UserImported",
  "Intent": "ReplacementAsset",
  "OwnerRecipeId": null
}
```

合法组合示例：

| 资产 | Origin | Intent |
|---|---|---|
| 游戏原始父材质 | `GameReference` | `ExternalReference` |
| 用户导入并替换原路径的主 Mesh | `UserImported` | `ReplacementAsset` |
| 工具生成的 Post Process AnimBP | `ToolGenerated` | `AddedAsset` |
| 工具重建且有意覆盖原路径的资产 | `ToolGenerated` | `ReplacementAsset` |
| 用户自己创建的附加贴图 | `UserImported` | `AddedAsset` |

### Authoring Recipe

所有 Recipe 共用最小 envelope：

```json
{
  "Id": "kawaii_main_qun",
  "Type": "KawaiiPostProcess",
  "Enabled": true,
  "Sources": ["game_kawaii_qun"],
  "Targets": ["main_mesh"],
  "Deltas": {},
  "Outputs": ["main_post_process_abp", "qun_limits", "qun_constraints"]
}
```

Recipe-specific payload 只保存用户选择和 delta。例如：

- Material：源材质、目标 Mesh/slot、输出 MI、源贴图到用户贴图的替换；
- Runtime：动作类型、目标 Mesh/slot、标签、显示名、按键、默认状态；
- Runtime UI：style profile ID、显隐按键、输出 Widget/SaveGame、显式 override；
- PhysicsAsset：源 PhysicsAsset、目标 Mesh、选择的骨骼链和骨名映射；
- Kawaii：源 preset、目标 Mesh、root/骨名映射、参数 delta、输出 AnimBP/DataAssets；
- StageReplacement：源资产、目标游戏 package path、备份策略。

不要保存 Blueprint 节点坐标、扫描结果、展开后的游戏参数、依赖树、日志或 cook 中间文件。

### Package Manifest

The implementation now ships `NtePakmodProjectEditorModel` and a persistent Nomad tab. The tab is deliberately thin: it edits the project model, delegates authoring to Recipe Adapters, and delegates packaging to `BuildPackagePlanFromManifest` / `CreateModPackageJobFromManifest`. Its pages are Project, Assets, Materials, Runtime, Physics, Kawaii, Package, and Build.

`NtePakmodRecipeAdapter` is the transitional writer seam. It projects selected recipes into the proven Character writers for MaterialInstance, RuntimeActions, PhysicsAsset, and Kawaii/Post Process assets. This is intentionally one-way at first; generated outputs remain owned by their recipe and can be detached without deleting the UE asset. The package core never calls this adapter.

```json
{
  "ModName": "Nanallyshuiyi",
  "OutputProfileId": "nte-client-mods",
  "AssetIds": [
    "main_mesh",
    "main_post_process_abp",
    "body_mi",
    "body_diffuse"
  ],
  "ExplicitExclusions": [],
  "Cook": {
    "Unversioned": false
  }
}
```

Manifest 引用 Asset ID，不复制 Recipe 内容。`ExternalReference` 即使误加入 `AssetIds` 也显示为错误并从 plan 排除。

## 持久化与恢复

### 项目 JSON 保存

- project identity；
- source references；
- asset references；
- lightweight Recipes；
- Package Manifest。

### `EditorPerProjectUserSettings` 保存

- 最近打开的 Pakmod Project 列表；
- 上次活动项目；
- tab、分栏宽度、表格排序和筛选；
- 最近使用的 profile；
- 未影响输出的 GUI 偏好。

### `Saved/NTEBuildTool` 保存

- source/output fingerprint；
- 最后一次 Apply/Build 结果；
- 扫描缓存；
- Diagnostics 报告；
- 日志和临时 Package Plan。

这些内容可全部删除并重新生成，不能成为项目可移植性的必要条件。

### Recipe 状态

GUI 中显示四种核心状态：

- `unconfigured`：缺少 Recipe 必要输入；
- `pending Apply`：配置完整但 output 尚未生成；
- `generated`：output 与最近 fingerprint 一致；
- `source changed`：source、target 或 delta 变化，output 尚未重新 Apply。

状态由项目配置、资产存在性和 `Saved` fingerprint 推导，不作为另一份真相写回项目 JSON。

## Apply、Sync、Detach 与 Build

The editor's `Apply enabled` action only invokes recipes explicitly selected on the current page. It records no derived dependency tree in the project file. `Sync` currently supports Kawaii recipes and writes only the synced preset delta; `Detach` clears ownership and disables the recipe while preserving generated assets.

```text
编辑 Recipe
  -> pending Apply
  -> Apply Changes
  -> Tool Generated Asset
  -> 可选 UE 原生编辑
      -> Sync delta/snapshot 回 Recipe
      或 Detach 解除 Recipe ownership
  -> 将目标资产加入 Package Manifest
  -> Build Package（只读磁盘资产）
```

规则：

- Apply 前展示会写入或覆盖的精确资产列表。
- Apply 不自动 Build。
- Build 不自动 Apply，也不因 `source changed` 阻止；GUI 只提示包会使用当前磁盘版本。
- Sync 后不自动 Apply，因为 UE 资产已经是用户刚编辑的版本。
- Detach 后 Recipe 不再覆盖该 output；资产 Origin 可保留 `ToolGenerated` 作为来源事实，但 `OwnerRecipeId` 清空。
- Stage as Replacement 在执行前展示 exact target、现有 mirror asset 和 backup 位置。

## 统一 GUI

### 全局框架

顶部固定显示：活动项目、项目文件状态、Mirror Project profile、Game profile、`Save`、`Apply Changes`、`Build Package`。

主区域使用 tabs：

1. `Project`
2. `Assets`
3. `Materials`
4. `Runtime`
5. `Physics`
6. `Kawaii`
7. `Package`
8. `Build`

`Diagnostics` 放在次级菜单或独立 tab，不与正常 Build 混在同一个阻塞结果中。

### Project

- 项目名称和 profile；
- source availability 摘要；
- legacy spec migration；
- project save/save as；
- 最近 Apply/Build 摘要。

### Assets

使用一个可筛选表格展示：ID、资产、Class、Origin、Intent、Owner Recipe、状态、是否进入 Manifest。

常用操作：

- 从 Content Browser 添加；
- 从 FModel/Game Reference Library 添加引用；
- `Stage as Replacement`；
- 添加到/移出 Package Manifest；
- 打开资产；
- Detach generated output。

普通流程不要求手填路径。资产 picker 负责 UE 资产，Game Reference Library 负责源游戏路径。

### Materials

- 选择目标 SkeletalMesh；
- 按真实 slot index/name 显示材质表；
- 每行显示当前材质、源游戏材质候选、输出 MI 和 Recipe 状态；
- 通过 searchable game material library 选择父 MI；
- 显示 Source Texture Usage 和贴图缩略图；
- 用户以“源贴图 -> 新贴图”选择替换，高级模式才显示逐参数 override；
- Apply 后可直接打开生成 MI。

不在 core 中硬编码某个角色的槽位顺序。角色专用修复由用户资产、Recipe 或 Diagnostics profile 表达。

### Runtime

动作表字段：显示名、动作类型、目标资产、slot/parameter/morph、按键、默认状态、是否进入 Widget。

第一阶段保留已经跑通的：

- material section visibility；
- attached/component visibility；
- `/` 打开关闭 Runtime UI；
- Widget 按钮；
- SaveGame 持久化。

UI style 使用 `StyleProfileId`，例如 `NTE.Common.DarkButton`，并允许 font/button/brush 的显式 overrides。删除 `FNteCharacterRuntimeUiSpec` 中把某个游戏字体或按钮路径当成通用默认值的硬编码。

未来 Morph Target 和动画切换进入新的 typed action payload，由 Runtime Recipe 声明 EventGraph 行为；它们不会进入 Package Builder。

### Physics

- 目标 Mesh asset picker；
- Player/NPC PhysicsAsset library 搜索；
- 骨架树与链选择；
- source bone -> target bone 映射；
- copy whole asset 或 selected chains；
- Apply、Open PhysicsAsset。

PhysicsAsset 刚体/胶囊体与 Kawaii 参数分开呈现，避免把两种物理模型混成一个“数据表”。

### Kawaii

- 目标 Mesh；
- game/source preset 搜索；
- 骨架树选择 root/additional root；
- source -> target bone remap；
- 节点顺序；
- 参数 delta；
- Limits/Constraints/Curve outputs；
- `Apply`、`Open AnimBP`、`Open Limits`、`Open Constraints`、`Sync`、`Detach`。

合并主 Mesh 默认创建 Post Process Kawaii Recipe。附加 Mesh CopyPose 路线继续作为可选高级 Recipe，但不再要求修改 Appearance/UI/NPC 数据才能使用打包器。

### Package

- 明确的 Package Manifest 资产表；
- Intent、引用原因和 hard dependency candidates；
- Missing/External/Owned 状态；
- 可编辑 include/exclude；
- 输出 profile 和 cook 设置；
- 生成 Package Plan 预览。

### Build

- 当前磁盘资产将被打包的明确提示；
- 不自动 Apply 的状态提示；
- Cook/IoStore 分阶段进度；
- 输出文件、时间、大小和返回码；
- 打开输出目录和日志；
- 可选运行 Diagnostics，但不是 Build 的隐藏步骤。

## 默认验证与 Diagnostics

### 默认阻塞

- Pakmod Project schema/path 合法；
- Manifest 中 owned asset 存在；
- replacement path 已经在 Mirror Project 中真实存在；
- ExternalReference 没有被当成 packaged asset；
- 已知会破坏 cooked Blueprint 的版本配置；
- Cook 返回码；
- IoStore 返回码；
- 本轮输出是 fresh、非空且三件套完整。

### 默认提示但不阻塞

- Recipe 为 `pending Apply` 或 `source changed`；
- AddedAsset 尚未发现被 ReplacementAsset 引用；
- source reference unavailable；
- hard dependency 未加入 Manifest；
- generated output 已 Detach。

### 可选 Diagnostics

- skeleton/bone hierarchy 和 skin weight 对比；
- material slot、section map、vertex color、morph target 审计；
- Kawaii root、limits、constraints、curve 和 exact AnimGraph topology；
- FModel provenance 与 source fingerprint；
- game container precedence；
- `HTPlayerAppearance`、PlayerUIShow SCS、原 AnimBP/DataAsset schema；
- Nanally package baseline 等角色 profile。

## 迁移与删除顺序

### Phase 0：文档和回归样本

- 接受 ADR 0002；
- 固化本文件、Post Process Kawaii 文档和手工游戏资产文档；
- 将当前 Nanally 成功包作为外部 Diagnostics baseline，而非 core 条件；
- 为现有 package job/build 记录最小 smoke fixture。

退出条件：团队可以仅凭文档解释 Apply、Build、ownership 和 Kawaii 主路径。

### Phase 1：先抽出 package-only core

- 新建 Pakmod Project model 和 Package Manifest；
- Package Planner 改为只接受 manifest；
- Package Builder 保留现有稳定 Cook/IoStore Implementation；
- 新增从现有 Package Job/CharacterModSpec 到 manifest 的临时 Adapter；
- 添加 serialization、plan 和 build acceptance tests。

暂不改 writer，不删旧 GUI。

退出条件：一个完全不含 CharacterModSpec 的手工资产项目可以成功打包。

### Phase 2：持久 tab 与 Assets/Package/Build

- 注册一个 Nomad tab；
- 实现 ProjectStore、MRU 和自动恢复；
- 先迁移 Assets、Package、Build 三页；
- 旧菜单暂时跳转到新 tab 的对应页。

退出条件：关闭/重开 tab 不丢项目，Build 不触发 Apply。

### Phase 3：Recipe envelope 与现有 writer Adapter

- Material、Physics、Runtime、Kawaii 配置迁移成 lightweight Recipes；
- 现有 writer 先通过 Adapter 消费 Recipe plan，避免一次性重写内部算法；
- output 自动登记为 ToolGenerated，并由用户明确加入 Manifest；
- 引入 Apply status/fingerprint、Sync 和 Detach。

退出条件：每种 Recipe 可独立 Apply，未配置 Recipe 不参与验证。

### Phase 4：Generated Blueprint Composer

- 提取稳定 declaration model；
- 先迁移 Kawaii Post Process AnimGraph；
- 再迁移 Runtime EventGraph；
- 最后迁移 Widget/SaveGame；
- 通过 marker ownership 测试保证重复 Apply 和交错 Apply 不删除其他 Recipe 节点。

退出条件：Kawaii 与 Runtime 可以共享目标资产，任意 Apply 次序结果一致。

### Phase 5：迁移和清理

- 提供 `CharacterModSpec -> NTE.PakmodProject v1` one-way migration；
- 迁移已有样本并对比 manifest 和 Recipe outputs；
- 删除 CharacterModSpec runtime fallback；
- 删除 `UIActorClassPath` 运行时兼容，只保留迁移读取；
- 删除 `ApplyCopyPoseOnly` 等实验 flag 或移入 Diagnostics；
- 删除 modal `CloseWithAction` 工作流；
- 收敛主菜单为一个 Pakmod Project 入口，独立工具作为 tab deep-link；
- 删除 package preflight 对 Material/Runtime/Appearance/Kawaii plan 的直接调用；
- 删除 Nanally/角色专用 core 判断，保留外部 Diagnostics profile。

退出条件：新路径完整覆盖已验证功能，旧路径没有生产调用者。

### Phase 6：大文件内部深化

只在外部 Interface 稳定后整理大 writer：

- Runtime writer 按 EventGraph declaration、Widget declaration、SaveGame declaration 的内部职责提高 Locality；
- Kawaii writer 按 graph declaration、DataAsset writing、topology-safe anchor resolution 整理；
- Workspace modal 文件随新 tab 完成后删除；
- 共享反射/节点工具仅在两个真实调用者存在时提取。

文件行数不是验收指标。删除一个 helper 后如果复杂性只搬到多个调用点，该 helper 有 Depth；如果复杂性直接消失，它才是应删除的浅 Module。

## 明确不进入主 GUI 的内容

- 重建或替换源游戏主/UI/NPC AnimBP；
- 自动修改 `HTPlayerAppearance`；
- 自动修改 PlayerUIShow/NPC Blueprint SCS；
- 自动修改游戏 DataTable/DataAsset 注册链；
- 自动推断角色特有材质槽修复；
- 自动运行 UE4SS 或修改游戏进程。

这些流程保留在 `docs/manual-game-animbp-data-workflows.md` 和 Diagnostics。用户手工完成后，生成的 UE 资产仍可加入 Package Manifest 并正常打包。

## 测试策略

当前缺少正式 Tests 目录。重构必须先围绕新 Interface 建立测试，而不是继续只验证大型 commandlet 组合。

### Project model

- v1 round-trip golden JSON；
- unknown enum/version 错误；
- 不持久化 cache/result/diagnostics；
- CharacterModSpec migration golden tests。

### Recipe lifecycle

- plan 不写资产；
- Apply 只触碰声明 outputs；
- source unavailable 只禁用相关 Apply；
- Sync delta 与 snapshot 标记；
- Detach 后 reapply 不覆盖资产。

### Blueprint Composer

- 重复 Apply 幂等；
- Runtime/Kawaii 任意顺序结果一致；
- 删除一个 Recipe 只删除其 marked nodes；
- 未标记用户节点保留；
- compile failure 不更新 success fingerprint。

### Package core

- manual-only assets 不需要任何 Recipe；
- ExternalReference 永不进入 response；
- Replacement/Added intent 保留；
- Build 不调用 Apply；
- Cook/IoStore 非零返回码失败；
- 旧输出不能冒充本轮 fresh output。

### Diagnostics

- Diagnostics failure 默认不阻止 Build；
- 选定 strict profile 时只阻止 profile 明确声明的条件；
- 角色 profile 数据不出现在 core tests。

## 首个实施切片

文档评审通过后的第一刀只做 Phase 1：

1. 添加 `FNtePakmodProject`、`FNteAssetReference`、`FNteAuthoringRecipe`、`FNtePackageManifest` 的 version-1 model 与 JSON round-trip。
2. 添加 `BuildPackagePlanFromManifest`，复用现有 package dependency 和 candidate Implementation。
3. 将现有 `BuildPackagePlanFromCharacterModSpec` 改成临时 migration Adapter，内部先生成 manifest，再调用新 Interface。
4. 将 `BuildCharacterModSpecPackagePreflight` 的 authoring 深检留在旧 Adapter，不进入 manifest core。
5. 为 manual-only manifest 添加自动化 smoke test。

这个切片先建立真正的 package seam，同时最大限度保留当前已经跑通的 writer 和打包行为。完成后再进入 GUI 与 Recipe 迁移，避免一次大改同时破坏所有功能。
