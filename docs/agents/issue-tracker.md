# Issue Tracker: 本地 Markdown

本仓库的问题、PRD 和任务拆分使用本地 Markdown 文件管理，位置在 `.scratch/`。

## 约定

- 每个功能或主题一个目录：`.scratch/<feature-slug>/`
- PRD 文件：`.scratch/<feature-slug>/PRD.md`
- 实现任务：`.scratch/<feature-slug>/issues/<NN>-<slug>.md`，从 `01` 编号
- triage 状态写在 issue 文件顶部附近的 `Status:` 行
- 状态值见 `docs/agents/triage-labels.md`
- 评论和对话历史追加到文件底部的 `## Comments` 小节

## 当技能说“发布到 issue tracker”

在 `.scratch/<feature-slug>/` 下创建对应 Markdown 文件；目录不存在时先创建。

## 当技能说“读取相关 ticket”

读取用户提供的路径或 issue 编号对应的 Markdown 文件。若信息不明确，先在 `.scratch/` 中查找匹配任务。
