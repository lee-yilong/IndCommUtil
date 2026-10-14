# 0001. 协议是一等公民，客户端/服务器是协议内部模式

- **状态**：已接受
- **日期**：2026-10-12（引入 ADR 机制后于 2026-10-14 补记）

## 背景

工具要支持多种工业协议（TCP、串口、Modbus RTU、MQTT…）。最初的思路沿用了常见做法：
把「TCP 客户端」和「TCP 服务器」当成两个独立功能，各有自己的配置对话框、菜单项和配置文件。

这个思路在协议数量少的时候看不出问题，但它会导致：

- 同一个协议在菜单里出现两个入口（"TCP客户端" / "TCP服务器"）
- 协议配置要保存两份文件（一份客户端、一份服务器），切换模式时丢失对方配置
- UI 层出现大量 `if (是客户端) … else …` 分支
- 新增一个协议 = 新增两个类 + 两套 UI 注册

## 决策

**协议是一个整体。** `IProtocol` 的实例代表协议本身（`Id() = "tcp"`），
「客户端 / 服务器」是协议**内部的一种模式（Mode）**，由 `HasModes()` / `ModeNames()` /
`FieldsForMode(modeIndex)` 描述。

一个 `.conn` 文件保存**一份**协议配置（含 Mode 字段），不是两份。

模式差异被收敛到两个地方：

1. `IProtocol::CreateSession(modeIndex, fields)` —— 由协议自己决定造哪个 Transport
2. `ITransport` 的 `clientId` 参数 —— 统一客户端（恒为 `0`）与服务器（真实客户端号）的收发语义

UI 层通过 `Session::IsServerMode()` 适配显示差异，不再区分协议种类。

## 替代方案

| 方案 | 为什么没选 |
|---|---|
| `TcpClientProtocol` + `TcpServerProtocol` 两个类 | 菜单/配置/UI 都要复制一份，新增协议成本翻倍 |
| 一个类但 Mode 硬编码在 UI 里 | 破坏「新增协议不改 UI」，协议无法自带模式描述 |
| 用两个独立 `.conn` 文件 | 切换模式要保存两份，用户体验差且容易不一致 |

## 后果

**好处**

- 菜单里每个协议只有一个入口，由协议自行提供模式列表
- 一个 `.conn` 文件承载完整配置，模式可随时切换
- 新增协议只需实现一个类 + 注册，UI 零改动

**代价 / 限制**

- `ITransport` 必须同时覆盖客户端与服务器语义，接口比单一用途更宽；
  因此 Transport 层保留 `TcpClientTransport` / `TcpServerTransport` 两个实现，API 差异被挡在协议层内
- 协议作者必须正确实现 `FieldsForMode()`，否则 UI 生成的字段不对

**后续必须遵守的约束**

- 不允许出现 `XxxClientProtocol` / `XxxServerProtocol` 这类拆分
- 模式的字段描述必须来自 `FieldsForMode()`，不得在 UI 里硬编码

## 参考

- 代码：`src/core/IProtocol.h`、`src/protocols/tcp/TcpProtocol.h`、`src/core/ITransport.h`
- 文档：根 `README.md` 第 2.1 节
