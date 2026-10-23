# 0016. 细化 MQTT 协议模式（发布者、订阅者、代理者）

- **状态**：已接受
- **日期**：2026-10-27
- **决策者**：项目所有者

## 背景

当前版本的 `IndComm` 已经支持了 MQTT 协议，但实现相对基础：`MqttProtocol` 只有单一的模式，`MqttSession` 在连接成功后默认订阅 `#` 通配符，同时 UI 面板既允许发送（Publish）又自动接收（Subscribe）。

随着测试场景的深入，测试工程师发现这种“混合”模式无法精准模拟真实的工业现场角色。在 MQTT 协议规范中，参与者有非常明确的划分：
1. **发布者 (Publisher)**：向 Broker 发送消息的客户端。
2. **订阅者 (Subscriber)**：从 Broker 接收消息的客户端。
3. **代理者/服务器 (Broker)**：接收所有客户端连接，并负责消息路由的服务端。

将三者混在一个模式里会导致：
- **行为不清晰**：作为“发布者”测试时，不需要（也不应该）自动订阅 `#` 导致接收到一大堆无关的现场噪音数据。
- **UI 割裂**：作为“订阅者”测试时，UI 上不应该出现发送按钮，避免误操作。
- **无法充当服务端**：当前完全依赖 Paho 的客户端接口，无法在本机直接拉起一个 MQTT Broker 供其他设备连接测试。

## 决策

**参照 Modbus 的模式设计，将 `MqttProtocol` 拆分为三个独立的模式（Mode）：**
1. **Publisher (发布者)**：作为客户端连接远端 Broker，UI 隐藏订阅相关配置，仅允许发送消息。
2. **Subscriber (订阅者)**：作为客户端连接远端 Broker，自动订阅指定主题（默认 `#`），UI 隐藏发送按钮，仅展示接收数据。
3. **Broker (代理者/服务器)**：工具自身作为服务端（Server），监听指定端口，接收其他 MQTT 客户端的连接与消息。

### 具体实施策略

1. **协议层重构**：`MqttProtocol::ModeNames()` 返回 `{"Publisher", "Subscriber", "Broker"}`。
2. **字段差异化**：
   - `Publisher` 与 `Subscriber` 模式：需要填写 `brokerHost`、`brokerPort`、`clientId`、`keepAlive`。
   - `Broker` 模式：仅需要填写 `listenPort`（默认 1883）。
3. **会话层解耦**：
   - 新增 `MqttBrokerSession`（针对 Broker 模式）。
   - 重构 `MqttSession`，通过内部标志位明确当前是 `Publisher` 还是 `Subscriber`。
4. **底层依赖策略**：由于当前项目使用的 `paho.mqtt.cpp` 仅支持客户端（Client），不支持内建 Broker。为了支持 `Broker` 模式，我们将在后续阶段引入轻量级 MQTT Broker 库（如 `mosquitto` 的 C 库）或者调用系统现有的 broker 进程。本次重构优先打通接口和客户端模式划分，对于 Broker 模式，在底层实现完成前，UI 上会明确提示“底层 Broker 支持正在构建中”。

## 替代方案

| 方案 | 为什么没选 |
|---|---|
| 保持现状，通过勾选框决定是否订阅或发布 | UI 过于杂乱，不符合“模式即角色”的直观认知，且无法扩展出服务端能力。 |
| 将 Publisher、Subscriber、Broker 拆分为三个独立的顶级协议 | 违反 ADR 0001（协议是一等公民）。它们同属于 MQTT 协议栈，只是角色不同，不应拆分。 |

## 后果

**好处**
- **职责单一**：测试工程师可以精确模拟现场设备（仅发布）或监控中心（仅订阅），互不干扰。
- **UI 数据驱动**：借助现有的 `FieldDescriptor` 和 `GenericProtocolDialog`，无需硬编码即可自动生成 Broker 模式专属的字段（仅显示监听端口）。
- **未来扩展**：为后续引入内置 Broker 打通了架构上的道路。

**代价 / 限制**
- 需要新增针对 Broker 的底层实现（引入新的第三方库）。
- 需要调整 UI 层的 `SessionPanel`，使其在特定模式下动态隐藏发送/操作面板。

**后续必须遵守的约束**
- 严禁在 `Publisher` 模式下自动订阅任何主题。
- `Broker` 模式不应实例化 Paho 客户端。
- 严禁在 UI 层写 `if (protocolId == "mqtt" && mode == "Broker")` 这样的逻辑，必须通过 `IProtocol` 暴露的数据和 `Session` 状态来驱动。
