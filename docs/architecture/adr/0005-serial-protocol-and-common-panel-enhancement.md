# 0005. 串口协议实现与通用会话面板增强

- **状态**：已接受
- **日期**：2026-10-14
- **决策者**：项目所有者

## 背景

阶段 5 要实现串口通信。约束：

1. wxWidgets **没有内置串口**，必须自己封装
2. 不引入第三方串口库（目标是把边界完全掌握在自己手里）
3. 参考市面上的串口工具（UartAssist / NetAssist / XCOM），
   它们在通信面板上有一组**通用能力**，与协议无关：
   HEX 显示、时间戳、自动滚动、清空接收、发送新行开关
4. 现有 `ProtocolSessionPanel` 只支持 ASCII 显示、无时间戳开关、
   无自动滚动、无条件追加 `\n`

另外，串口有物理控制线（RTS/DTR/CTS...），**评估后决定不做**：
- 工业调试用 RTS/DTR 场景少
- 引入可选接口（`ISerialControl`）会污染 Transport 抽象
- 真要控制流控，通过 `flowControl` 字段配置即可（RTS/CTS 硬件流控）

## 决策

**串口协议本体 + 通用会话面板增强，两部分并行。**

### 串口协议

- `SerialProtocol` 实现 `IProtocol`，**单模式**（`HasModes() = false`）
  - 理由：串口是点对点，不存在 Client/Server 角色区分
- `SerialTransport` 实现 `ITransport`，**后台线程 + 同步 I/O**
  - Windows：`CreateFile` + `SetCommState` + `ReadFile` / `WriteFile`
  - 读线程阻塞在 `ReadFile`，读到数据后 `CallAfter` 抛回主线程
  - `Close()` 用 `Interrupt()` 打断阻塞读，再 `join` 线程
  - Linux 实现留 `#ifdef` 骨架，阶段 5 不实现
- `SerialPort` 是**平台无关接口 + 平台相关实现**，不依赖 wxWidgets
- 跨平台差异通过 `#ifdef _WIN32` 收敛在 `SerialPort.cpp`

### 通用会话面板增强（TCP 同步受益）

`ProtocolSessionPanel` 新增以下能力，**与协议无关**：

- HEX 接收显示切换（接收区不再自动判断，由用户显式选择）
- 时间戳开关（每条消息前加 `[HH:MM:SS.mmm]`）
- 自动滚动开关
- 清空接收按钮
- 发送新行开关（默认勾选，保持现有行为）

这些能力放在 `ProtocolSessionPanel` 而非协议层，因为它们是**显示层行为**，
不影响 `ITransport` / `ICodec` / `Session` 的数据语义。

`Session` / `ITransport` / `ICodec` **完全不动**。

## 替代方案

| 方案 | 为什么没选 |
|---|---|
| 引入第三方串口库（Boost.Asio / serial / libserial） | 违反"边界自己掌握"的约束；Boost 太重；serial 引入额外依赖 |
| 用 `wxTimer` 轮询串口 | 延迟取决于轮询间隔，空闲时占 CPU，不如阻塞读线程干净 |
| Windows 重叠 I/O | 跨平台代码差异太大；Linux 要重写 |
| 串口拆 Master/Slave 两模式 | 裸串口没有 Master/Slave 语义，空洞 |
| 在 `ITransport` 上加 RTS/DTR | 污染所有 Transport 实现；TCP 无意义 |
| 用 `ISerialControl` 可选接口 | 收益低（RTS/DTR 场景少），抽象成本高 |
| HEX 显示/时间戳放在协议层 | 它们是显示层行为，不应污染数据传输语义 |

## 后果

**好处**

- 串口实现完全自掌握，不依赖第三方库
- `ProtocolSessionPanel` 的通用能力对 TCP 和串口共享，**避免重复实现**
- `Session` / `ITransport` / `ICodec` 零改动，现有测试不受影响
- 后台线程模型与 `TcpTransport` 的事件模型语义一致（都是异步通知）

**代价 / 限制**

- 串口读线程的启动/停止时序要小心（`Interrupt` + `join` 顺序）
- Windows 和 Linux 的 `SerialPort` 实现需要各自维护
- 阶段 5 只做 Windows，Linux 用户暂时用不了

**后续必须遵守的约束**

- 串口**不得**拆分为 Master/Slave 两个协议
- `ProtocolSessionPanel` 的增强项必须对**所有协议**生效，不得写 `if (protocolId == "serial")`
- `SerialPort` 不得依赖 wxWidgets（方便单测和跨平台）
- RTS/DTR 如果未来要做，必须新写 ADR，不得直接加到 `ITransport`

## 参考

- 代码：`src/protocols/tcp/TcpProtocol.h`、`src/protocols/tcp/TcpTransport.h`
- 代码：`src/ui/ProtocolSessionPanel.h/.cpp`
- 文档：[ADR 0001 协议是一等公民](0001-protocol-as-first-class-with-modes.md)
- 参考软件：UartAssist / NetAssist / XCOM（通用面板设计）
