# Character Mod Workspace PRD

Status: ready-for-agent

## Problem Statement

当前 `NTEBuildTool` 已经能完成若干独立能力：材质实例创建、材料槽切换运行时生成、Package Plan/Package Job 打包。但这些能力仍偏向“单点工具”和“旧 PostProcess 模板运行时”，用户在做真正的人物模型替换时会遇到几个核心问题：

- 工作流不是以“一个角色外观”为中心，而是以零散对话框和路径输入为中心。
- 材质、附加 mesh、按键/UI 切换、Kawaii 物理、打包清单之间缺少统一的源数据。
- 当前运行时切换逻辑历史上依赖生成 PostProcess AnimBP 模板，和游戏原生 `HTPlayerAppearance` 的附加 mesh 机制没有统一。
- UI 层需要让用户通过选择 mesh、槽位、材质、贴图、骨骼链、热键、附件来工作，而不是手写大量 `/Game/...` 路径。
- 代码层需要模块化和可测试，减少硬编码、一次性实验逻辑和为了兼容旧方案而堆出的复杂分支。

用户的最终目标不是只复刻 `071_chaos` 的口罩按键，而是构建一个稳定的人物替换 mod 工具链，获得：

1. 材质自由：可以解析游戏 FModel 材质 JSON，创建材质实例，并让自定义 mesh 使用游戏原有材质体系。
2. 切换自由：可以通过按键和 UI 控制 mesh、材质槽、材质参数、后续 morph/动画状态。
3. 物理自由：可以为自定义主 mesh 与多个附加 mesh 配置 KawaiiPhysics；参数应主要在 UE 中编辑，可复制游戏原生参数，骨骼链/权重由 Blender/DCC 提供。
4. 打包自由：可以自动推导替换资产、新增资产和引用链，减少用户手动猜包。
5. 后续扩展自由：未来支持动画、形态键、更多运行时动作。

## Solution

将现有 `Mesh Mod Workspace` 升级为 `Character Mod Workspace`。

核心设计是使用一个统一的 `CharacterModSpec` 作为“单一事实源”。用户在 UI 中选择目标外观、主 mesh、多个附加 mesh、材质槽操作、热键/UI 切换项、Kawaii 物理配置、打包设置；工具内部从 `CharacterModSpec` 生成或修改所需资产：

- 主 mesh 与材质实例。
- `HTPlayerAppearance` / `MeshAsset_PlayerXXX` 中的主 mesh 和附加 mesh 数据。
- `PlayerUIShow_XXX` 中的 UI 预览附加组件。
- 每个需要运行时逻辑的附加 mesh AnimBP，例如 `ABP_<AttachName>_Runtime`。
- 按键/UI 切换所需 Widget、保存状态和运行时动作数据。
- Kawaii preset/data 与可编辑 AnimBP 节点配置。
- Package Plan 和 Package Job。

新的主线运行时不再以旧的“所有逻辑塞进 PostProcess AnimBP 模板”为中心。对附加 mesh，优先使用游戏原生 appearance 机制挂载，并让附加 mesh 自己的 AnimBP 成为运行时宿主：

```text
HTPlayerAppearance / MeshAsset_PlayerXXX
  -> FashionMeshData: 主 mesh + 主 AnimBP
  -> ArrayFashionAttachedMeshData: 多个附加 mesh + socket + transform + attached AnimBP

Attached Mesh Component
  -> ABP_<AttachName>_Runtime
     -> AnimGraph: CopyPose / Kawaii / Output Pose
     -> EventGraph: hotkey/UI/save/apply runtime actions
```

UI 预览资产 `PlayerUIShow_XXX` 与运行时 `MeshAsset_PlayerXXX` 都由同一份 `CharacterModSpec` 生成/同步，避免让用户手工维护两份重复数据。

旧 PostProcess 模板运行时只作为历史实现和必要时的迁移参考，不作为新架构的核心。对于阻碍新架构的旧兼容分支、硬编码测试逻辑、一次性实验代码，应在对应模块替代能力完成后删除。

## User Stories

1. As a character mod author, I want to select one target character appearance, so that the tool can understand which game appearance asset and UI preview asset I am replacing.
2. As a character mod author, I want the workspace to show the current `DT_AppearanceData` mapping, so that I can verify the active `MeshAsset_PlayerXXX` and `PlayerUIShow_XXX`.
3. As a character mod author, I want to import or assign a replacement main SkeletalMesh, so that the game loads my character model at the original path.
4. As a character mod author, I want to add multiple attached meshes, so that hair, skirt parts, accessories, tails, props, and modular clothing can be handled separately.
5. As a character mod author, I want each attached mesh to choose a socket/bone and relative transform, so that it mounts like the source game’s attached meshes.
6. As a character mod author, I want attached meshes to inherit the main mesh pose through a generated AnimBP, so that they follow player animation correctly.
7. As a character mod author, I want attached meshes to have their own KawaiiPhysics nodes, so that hair, cloth, ribbon, tail, and accessories can have independent secondary motion.
8. As a character mod author, I want to copy Kawaii settings from source-game AnimBP JSON, so that I can start from proven game parameters instead of guessing.
9. As a character mod author, I want to edit Kawaii root bones, excluded bones, curves, limits, and collision references in UE, so that physics tuning is visual and precise.
10. As a character mod author, I want the tool to warn when the mirror project’s Kawaii plugin schema differs from NTE’s usmap, so that I do not cook incompatible assets unknowingly.
11. As a character mod author, I want to inspect material slots from the selected mesh, so that I can assign materials without manually counting section indices.
12. As a character mod author, I want to choose a source game material JSON, so that the tool can reconstruct its parent path and parameter intent.
13. As a character mod author, I want to replace a source texture group once, so that all matching material parameters are updated together.
14. As a character mod author, I want to use any compatible game material instance or parent material, so that my custom mesh can borrow existing shader behavior.
15. As a character mod author, I want material proxies to stay editor-only by default, so that fake parent assets are not accidentally packaged as real game replacements.
16. As a character mod author, I want a clean material-slot operation UI, so that I can choose target slot, parent/source material, texture replacements, and output MI in one place.
17. As a character mod author, I want to define toggle items by label, hotkey, default state, and affected targets, so that runtime switching matches my mod design.
18. As a character mod author, I want toggle targets to include material slots, whole attached meshes, material swaps, scalar/vector parameters, and future morph targets, so that one runtime model covers many actions.
19. As a character mod author, I want a generated UI panel with clear labels and buttons, so that users can switch mod states without remembering every hotkey.
20. As a character mod author, I want the UI to remain visually simple and precise, so that it feels like a tool panel rather than a debug dump.
21. As a character mod author, I want the UI to be generated from the same toggle spec as hotkeys, so that UI and keyboard behavior cannot drift.
22. As a character mod author, I want toggle state to persist, so that players do not need to reapply preferences every load.
23. As a character mod author, I want package planning to include generated runtime assets automatically, so that added Widget/AnimBP/SaveGame/DataAsset packages are not missed.
24. As a character mod author, I want the Package Plan to explain why each asset is included, so that I can catch accidental source-game dependency or editor-only proxy packaging.
25. As a character mod author, I want one Build action to create pak/utoc/ucas outputs, so that I can test in game without hand-authoring package jobs.
26. As a character mod author, I want the tool to support sparse milestone commits, so that meaningful architecture checkpoints can be saved without noisy micro-commits.
27. As a plugin maintainer, I want `CharacterModSpec` to be testable without Slate UI, so that core logic can be validated quickly.
28. As a plugin maintainer, I want appearance assembly, material creation, runtime generation, Kawaii presets, and packaging to be separate modules, so that each can change without breaking the others.
29. As a plugin maintainer, I want old PostProcess-only hardcoding removed after replacement, so that the codebase stays understandable.
30. As a plugin maintainer, I want source-game asset investigation to be evidence-backed, so that claims about `071_chaos`, `010_nanally`, or `004_lacrimosa` remain traceable to FModel/AssetProbe data.
31. As a plugin maintainer, I want commandlets and UI to consume the same core modules, so that automated tests and manual editor workflows stay aligned.
32. As a plugin maintainer, I want validation errors to point at the exact missing schema/class/asset/reference, so that users can fix problems without spelunking cooked assets.
33. As a plugin maintainer, I want generated assets to use stable naming conventions but not hardcoded target characters, so that the system scales beyond current examples.
34. As a plugin maintainer, I want package jobs containing runtime Blueprints to keep versioned cook behavior, so that known unversioned Widget serialization crashes do not regress.
35. As a future animation author, I want morph targets and animation state switches to plug into the same runtime action model, so that later features do not require another parallel toggle system.

## Implementation Decisions

- Build a `Character Mod Workspace` as the default user-facing flow. The older mesh-centered dialogs remain only as advanced adapters until their functionality is fully absorbed.
- Introduce `CharacterModSpec` as the single source of truth for a mod workspace. It should describe target appearance, main mesh, attached meshes, material operations, toggle groups/actions, Kawaii presets, generated runtime assets, and packaging preferences.
- Keep UI state separate from domain logic. Slate dialogs should collect/edit `CharacterModSpec`; core modules should validate and transform it without depending on Slate.
- Keep UI simple: one appearance summary, one mesh/attachment tree, one material-slot table, one toggle/action panel, one physics panel, one package panel. Avoid exposing raw JSON paths as the normal route.
- Use `DT_AppearanceData` and container scans to identify `UIActorClass` and `PlayerAppearanceAsset`, but do not rely on exported DataTable JSON alone when PatchPaks are involved.
- Use `HTPlayerAppearance` as the main appearance assembly target. Runtime attached meshes go into `ArrayFashionAttachedMeshData` or the proven equivalent field for the target asset.
- Sync UI preview through `PlayerUIShow_XXX` SCS child components generated from the same attached mesh definitions. Users should not manually duplicate attachment data in two places.
- Provide `/Script/HTGame` stubs/schema support before attempting final cooked `HTPlayerAppearance` and `HTSkeletalMeshComponentBudgeted` asset generation.
- For attached mesh runtime logic, generate attached mesh AnimBPs with a narrow contract: CopyPose from parent where applicable, apply Kawaii in AnimGraph, own hotkey/UI/save/apply logic in EventGraph when that mesh needs runtime switching.
- Do not make a separate Controller Blueprint the default owner if one generated AnimBP can keep the runtime simple and reliable. A controller can be introduced only when evidence shows AnimBP ownership is insufficient for a specific action.
- Do not make PostProcess AnimBP the new primary route. It remains a legacy runtime path and possible migration reference.
- Preserve 071 evidence accurately: native 071 mask toggle is a gameplay/state/cue chain (`Buff_Chaos_KeepMask` -> GameplayCue -> `GC_Chaos_KeepMask` -> `player_071_Chaos.PlayFaceMaskFadeIn/Out`), not proof that mesh-local AnimBP alone implements the mask key.
- Use 004/010 evidence as the stronger attached mesh model: `MeshAsset_Player004_lacrimosa_fashion4` and `MeshAsset_Player010_fashion3` show attached mesh data with socket names and attached AnimBPs; their `PlayerUIShow` assets duplicate child preview components.
- Preserve and extend the existing Material Recipe model. `SourceTextureOverrides` remains the preferred user-level texture replacement interface.
- Treat Material Proxy assets as editor-only unless explicitly promoted. Package Plan should flag them as excluded by default.
- Add a Kawaii preset/config module that can read FModel AnimBP JSON node properties, preserve editable fields, and apply them to generated AnimBPs once the mirror Kawaii schema is compatible.
- Treat the current NTE usmap as the serialized schema source of truth for Kawaii fields. Do not claim stock KawaiiPhysics cooked assets are safe until field compatibility is verified.
- Package Plan generation must start from the `CharacterModSpec` and include all replaced assets plus added assets reachable through references. The user can still review and override before build.
- Runtime Blueprint packages should continue to cook versioned by default because unversioned Widget Blueprint packages have produced bad export-index runtime serialization errors.
- Remove old compatibility branches and hardcoded test logic when a new module supersedes them. Compatibility should be explicit migration work, not permanent complexity inside core modules.
- Use sparse git commits at meaningful milestones: documentation/spec, compilable module skeleton, first working appearance assembly, first runtime toggle slice, first Kawaii-compatible slice, first successful full package example.

## Testing Decisions

- Tests should validate external behavior and generated outputs, not internal helper function trivia.
- `CharacterModSpec` validation should be testable without UE editor UI: invalid paths, duplicate hotkeys, missing labels, missing attachment sockets, conflicting generated names, and missing package roots should produce targeted errors.
- Appearance assembly tests should verify that a spec with multiple attached meshes produces the expected runtime appearance data and UI preview component model.
- Material module tests should verify source-texture usage expansion, unmatched source texture reporting, material proxy exclusion intent, and slot assignment output.
- Runtime toggle tests should verify that toggle groups/actions generate a consistent hotkey/UI/save/action model from one spec.
- Kawaii module tests should verify JSON import preserves known editable fields and rejects or warns on schema-incompatible fields.
- Package pipeline tests should verify package candidates are grouped and included/excluded according to role: replaced assets, added runtime assets, material instances, textures, skeleton/physics candidates, source dependencies, editor-only proxies.
- Build verification should use `RunUAT BuildPlugin` as the first gate.
- Practical verification should include at least one real character example with attached mesh assembly and one packaged mod copied to the game Mods directory.
- In-game behavior that cannot be proven in editor should be recorded as practical test evidence, not silently assumed.

## Out of Scope

- Rebuilding the entire original game project from cooked assets.
- Guaranteeing every original gameplay ability/character blueprint can be edited or recompiled.
- Implementing a native DLL runtime unless pure-pak evidence proves a necessary scenario cannot be anchored through assets.
- Full migration of all old generated PostProcess runtime assets in the first slice.
- Stock public KawaiiPhysics compatibility claims without usmap/schema verification.
- Automatic Blender-side authoring of Kawaii node parameters. Blender/DCC is responsible for geometry, skeleton, bone names, weights, morphs, UVs, and material IDs; UE owns Kawaii node editing.
- Full animation replacement/state-machine authoring in the first implementation phase.

## Further Notes

- The architecture should be “一次性做对主线”，但 implementation 仍应按可验证纵切推进：每个纵切都必须让架构更接近最终形态，而不是堆补丁。
- 旧逻辑不需要无限兼容。只要新模块覆盖其目标并通过构建/验证，就应删除旧硬编码和测试残留。
- 文档、代码和 UI 术语要保持一致：`Character Mod Workspace`、`CharacterModSpec`、`Appearance Assembly`、`Attached Mesh Runtime`、`Kawaii Preset`、`Package Plan`。
- 当前最佳实施顺序：
  1. 固化文档与领域语言。
  2. 建立 `CharacterModSpec` 数据结构、验证器和 workspace 骨架。
  3. 建立 `HTGame` schema/stub 与 appearance assembly 接口。
  4. 整合材质槽操作到 Character Workspace。
  5. 实现 MeshAsset/UIShow 的多附加 mesh 同步生成。
  6. 实现 attached mesh runtime AnimBP 生成。
  7. 实现 Kawaii preset 导入/编辑/应用。
  8. 实现从 spec 到 Package Plan/Package Job 的自动打包。
  9. 用真实角色例子完成 cook/package/in-game 验证。
