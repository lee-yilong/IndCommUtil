# 架构决策记录（ADR）

本目录记录**架构级决策**：为什么这样设计、当时考虑了哪些替代方案、后果是什么。

代码回答「怎么做」，ADR 回答「为什么这么做」。当有人问「为什么 TCP 客户端和服务器是同一个协议类」
时，答案应该在这里，而不是在聊天记录里。

---

## 什么时候写 ADR

- 引入 / 移除一个抽象（接口、基类、层）
- 改变依赖方向或分层规则
- 选定或替换第三方依赖（测试框架、日志库、GUI 库）
- 改变文件格式 / 持久化策略（`.json`、设置存储）
- 任何**后来者会问「为什么不那样做」**的决定

不需要 ADR 的情况：修 bug、加一个字段、调整 UI 布局。

---

## 命名与格式

```
docs/architecture/adr/
├── README.md              ← 本文件（索引 + 模板说明）
├── template.md            ← 复制这个开始写
├── 0001-protocol-as-first-class-with-modes.md
├── 0002-ui-data-driven-by-field-descriptors.md
├── 0003-doctest-as-test-framework.md
├── 0004-transaction-protocol-separation.md
├── 0005-serial-protocol-and-common-panel-enhancement.md
├── 0006-conn-format-ini-to-json.md
├── 0007-test-runner-engine-architecture.md
├── 0008-standalone-headless-cli-tool.md
├── 0009-sqlite-data-logging-and-charting.md
├── 0010-project-file-and-test-flow-redesign.md
└── 0011-session-panel-ux-refactoring.md
```

文件名：`NNNN-短横线小写标题.md`，编号四位递增，**不复用、不删除**。

格式（Michael Nygard 风格）：

| 小节 | 内容 |
|---|---|
| 状态 | 提议 / 已接受 / 已废弃 / 被 NNNN 取代 |
| 背景 | 面临什么问题，有哪些约束 |
| 决策 | 我们决定做什么（一句话能说清） |
| 替代方案 | 考虑过什么，为什么没选 |
| 后果 | 好处、代价、后续必须遵守的约束 |

---

## 索引

| 编号 | 标题 | 状态 |
|---|---|---|
| [0001](0001-protocol-as-first-class-with-modes.md) | 协议是一等公民，客户端/服务器是内部模式 | 已接受 |
| [0002](0002-ui-data-driven-by-field-descriptors.md) | UI 由 FieldDescriptor 数据驱动 | 已接受 |
| [0003](0003-doctest-as-test-framework.md) | 用 doctest 作为测试框架 | 已接受 |
| [0004](0004-transaction-protocol-separation.md) | 事务型协议与字节流协议分离 | 已接受 |
| [0005](0005-serial-protocol-and-common-panel-enhancement.md) | 串口协议与通用面板增强 | 已接受 |
| [0006](0006-conn-format-ini-to-json.md) | `.conn` 配置文件从 INI 迁移到 JSON | 已接受 |
| [0007](0007-test-runner-engine-architecture.md) | 通用自动化测试引擎架构 | 已接受 |
| [0008](0008-standalone-headless-cli-tool.md) | 独立无头 CLI 工具 (indcomm-cli) | 已接受 |
| [0009](0009-sqlite-data-logging-and-charting.md) | 基于 SQLite 的数据记录与实时图表可视化 | 提议 |
| [0010](0010-project-file-and-test-flow-redesign.md) | 项目文件重构与测试流重设计 | 已接受 |
| [0011](0011-session-panel-ux-refactoring.md) | 会话面板 UX 交互优化与重构 | 已接受 |

> 更早的决策（在引入 ADR 机制之前做的）汇总在根 `README.md` 第 14 节「关键决策记录」。
> 后续新增决策一律写在 `docs/architecture/adr/`。

---

## 相关文档

- [架构总览](../overview.md)
- [分层规则](../layers.md)
