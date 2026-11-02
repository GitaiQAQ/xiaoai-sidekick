# Agent Notes（中文版）

本目录里的设计文档只有一类。**Agent Note** 记录一个影响本仓库的决定——代码和普通文档承载不了的**为什么**和**放弃了什么**。本文件定义记录放在哪里、什么时候写、以及文件内格式。英文版见 [README.md](README.md)。

## 布局与命名

每条 Agent Note 有两个维度，都编码在路径里：`{lifecycle}/{class}/yyyy-mm-dd-topic-title.md`。

- **生命周期**是记录的状态，状态变化时文件随之移动：
  - `proposed/`——评审中的提案，尚未实现或只实现了一部分。
  - `implemented/`——决定已经落地。文件记录决定了什么、否决了什么，并**与已发布的代码保持同步**：代码后来移动了文件、改了包名或默认值，同一次改动里更新记录（只改事实——路径、名字、结构——不改决定本身）。细则见 [implemented/AGENTS.md](implemented/AGENTS.md)。
  - `rejected/`——考虑过并否决。只有当它还能阻止一个真实、诱人的错误时才保留，否则删除。
  - `archived/`——已实现、但不再指导当前工作的冻结历史记录。细则见 [archived/AGENTS.md](archived/AGENTS.md)。
- **分类**是决定的种类，取自下面封闭集合。

文件名里的日期是话题首次提出的日期。记录之间用相对 Markdown 链接互相引用，不用裸文字，移动后仍然可查。不要新增集中式 `INDEX.md`：目录本身就是清单。

## 受控区域

另外两个区域同样由路径约定并由门禁检查，用来放**不是决定**的材料：

- `reference/<topic>.md`——手册、协议、策略、部署范围、状态流水和阅读指南。头部为 `# Reference: <title>`；文件名用小写 kebab-case。参考文档是当前说明，可以每轮更新，不是冻结的决定。
- `reports/<topic>.md`——验证记录。头部为 `# Report: <title>`；文件名使用 ASCII 小写 kebab-case。报告按技术主题组织，必要时在标题和正文中保留轮次、时间与证据来源，不以轮次作为唯一导航方式。

参考资料与报告没有生命周期和分类，也不进入 Agent Note 树。知识索引（`reference/knowledge.md`）是人工维护的入口，不是自动生成的 INDEX。

原始材料与生成脚本在本目录之外：`rom/`（公开固件来源与校验资料）、`private/`（设备特定与个人数据）、`temp/`（可重建的派生产物）和 `tools/`（脚本，机密文件放在 `tools/local/`）。本仓库的目录名和文件名必须使用 ASCII 字符；正文可使用中文。

## 分类

每条记录只属于一个分类，分类即目录。封闭集合定义在 `scripts/verify-agent-notes.ps1`；新增分类必须同时更新脚本和这张表。

| 分类 | 覆盖内容 |
|---|---|
| `feature` | 面向用户或模型的新能力。 |
| `bug-fix` | 修复缺陷，或补上复盘暴露的缺口。 |
| `simplification` | 删除代码、行为或接口面，不增加能力。 |
| `architecture` | 关于已发布源码的结构决定——组件如何关联、运行时词汇是什么。 |
| `process` | 代码周边的工具、策略或流程——门禁、构建、部署——与运行时行为无关。 |
| `testing` | 测试基础设施与策略。 |

`architecture` 管的是我们发布的源码；`process` 管的是周边工具和流程。`refactor` 故意不设——它关心的“可观察行为是否改变”已经被 `simplification` 覆盖。

## 什么时候写

当决定在代码之外仍有长期价值时写：

- 光看实现看不出这个选择；
- 确实存在替代方案，而且放弃了一些；
- 它确立了否定性保证、所有权边界、线上/持久化语义、安全规则或重新引入的条件；
- 它记录了一个会诱使别人重犯的缺陷原因。

变更日志、任务进度、逐文件说明、对代码的复述，都不写。小改动不会因为“已经实现”就值得记录。

## 文件格式

每条活动记录都有下面的头部块，由门禁检查：

```markdown
# Agent Note: <title>

Status: proposed | implemented | rejected
```

归档记录保留 `Status: implemented`，并在下一行加 `Archived: YYYY-MM-DD`。

正文骨架随生命周期变化；必需的标题使用下面这些精确英文词，正文可以用中文：

- **所有记录**：`## Problem` 和 `## Alternatives considered`（最后一节）。
- **`proposed/`**：另需 `## Proposal`、`## Acceptance criteria`、`## Risks`。
- **`implemented/`**：另需 `## Decision`、`## Consequences`；提案期标题（`## Proposal`、`## Acceptance criteria`、`## Risks`）在这里会被拒绝。
- **`rejected/`**：另需 `## Proposal`、`## Why rejected`。
- **`archived/`**：与 implemented 相同。

专门的技术章节可以放在必需章节之间。文件名或版本历史已经承载的事实和日期，不必再写进正文。

`Alternatives considered` 是强制的：列出真正权衡过的选项以及它们为什么落选。不要为了填满这一节编造替代方案；当时没有记录就明确写出来。

## 归档与删除

- **删除**只描述小 UI 调整或纯机械改动的 implemented 记录；修复或删除入站链接。局部缺陷修复、新能力、实质性的行为决定，不会因为实现小就符合删除条件。
- **归档**已经完成、理由不太可能再指导未来工作、但仍有历史价值的 implemented 记录。
- **保持活动**：当它的替代方案、所有权边界、否定性保证、持久化语义、安全规则或重新引入条件仍会指导未来工作时。
- **proposed 永不归档**；不再值得做就改为 rejected。
- **rejected 记录**只在能阻止一个可信错误时保留。

归档路径是 `archived/{class}/yyyy-mm-dd-topic-title.md`——不写 `implemented`，因为只有 implemented 记录能进归档。归档时移动文件、保留 `Status: implemented`、插入 `Archived: YYYY-MM-DD`，并修复入站链接。之后记录即冻结：不要编辑、重排、翻译、更新或删除，也不要把它当作当前行为的权威。门禁会跳过归档来源。

## 门禁

约定必须可机械执行；没有门禁的约定会腐化。

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .agents/notes/scripts/verify-agent-notes.ps1
```

校验脚本检查：生命周期与分类的封闭集合、文件名模式、头部块与目录一致、各生命周期的章节骨架、implemented 中禁止的提案期标题、`## Alternatives considered` 是否存在、是否存在集中式 `INDEX.md`，以及两个受控区域（`reference/` 的文件名与 `# Reference:` 头部、`reports/` 的 `# Report:` 头部）。它只校验不生成，从不改写记录。

## 新增分类

在同一次改动里更新 `scripts/verify-agent-notes.ps1` 的封闭集合和上面的表格。
