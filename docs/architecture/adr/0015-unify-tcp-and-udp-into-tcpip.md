# 0015. 统一 TCP 与 UDP 为 TCP/IP 协议并参照 Modbus 模式设计

- **状态**：已接受
- **日期**：2026-10-23
- **决策者**：项目所有者

## 背景

`IndComm` 原有的协议类为 `TcpProtocol`，其内部 ID 为 `tcp`，但在 UI 上的显示名称为 `TCP/IP`。它仅包含 Client / Server 两种模式，底层基于 wxWidgets 的 `wxSocketClient` 和 `wxSocketServer` 实现。

随着测试场景的深入，用户明确提出了需求：需要支持 **UDP** 协议的测试。

在评估如何加入 UDP 时，我们最初考虑将其拆分为两个独立的协议菜单项。但这一方案被否决，原因是：**TCP 和 UDP 在协议栈分类上都属于 TCP/IP 协议族**。拆分菜单会导致分类混乱，且不符合工业测试软件的常规习惯。同时，UI 数据驱动架构要求我们尽量减少菜单层级，依靠数据字段来驱动。

现有的 `Modbus` 协议已经提供了一个优秀的范式：它有 `TCP`、`RTU`、`ASCII` 三个模式，而 Client / Server 的角色则是通过字段列表中的 `role` 下拉框来选择的。

## 决策

**参照 Modbus 的形式，将 TCP 和 UDP 统一到 `TCP/IP` 协议下，通过 Mode 区分传输层协议，通过 Field 区分角色。**

具体决策如下：
1. **统一协议**：将原 `TcpProtocol` 重命名为 `TcpIpProtocol`。ID 保持 `"tcp"` 以兼容旧配置，`DisplayName()` 保持 `"&TCP/IP"`。
2. **模式划分**：`ModeNames()` 返回 `{"TCP", "UDP"}`。
3. **角色划分**：在 `FieldsForMode` 中，针对 TCP 和 UDP 模式，均返回一个 `role` 字段（`Client` / `Server`），并合并原本的 `remoteHost` 和 `listenHost` 为统一的 `host` 字段，`remotePort` 和 `listenPort` 合并为 `port` 字段。界面由 `GenericProtocolDialog` 自动生成。
4. **底层解耦**：`TcpIpProtocol::CreateSession()` 根据 `modeIndex` 和 `role` 字段，决定实例化 `TcpClientTransport`、`TcpServerTransport` 还是 `UdpTransport`。
5. **向后兼容**：旧的 `.json` 配置文件在加载时，如果没有 `role` 字段，`CreateSession` 会通过探测 `remoteHost`（说明是 Client）或 `listenHost`（说明是 Server）自动推断角色，保证所有历史脚本无需修改即可继续运行。

## 替代方案

| 方案 | 为什么没选 |
|---|---|
| 拆分为独立的 TCP 和 UDP 顶级菜单 | TCP 和 UDP 同属 TCP/IP 协议栈，拆分导致菜单分类割裂，不符合用户认知。 |
| 在现有 `TcpProtocol` 内部硬编码分支 | 会让该协议类变得臃肿，且 UI 无法通过数据驱动的方式区分两种协议的专属字段。 |

## 后果

**好处**
- **结构极简**：菜单栏只有唯一的 `TCP/IP` 入口。
- **高度复用**：完全复用现有的 `GenericProtocolDialog` 和 `FieldDescriptor` 数据驱动生成 UI 的机制。
- **向后兼容**：原有 `.json` 配置文件完美兼容。
- **符合直觉**：工程师习惯了 Modbus 的操作方式，在 TCP/IP 里面选 TCP/UDP 和 Client/Server 非常自然。

**后续必须遵守的约束**
- 严禁在 `ui/` 目录下硬编码 `if (protocol == "tcp" && mode == "UDP")` 这样的逻辑。
- `TcpIpProtocol` 必须保持无状态，只负责根据字段描述和解析规则构建正确的 Session。
