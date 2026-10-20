# 0012. 引入 MQTT 协议与 Paho C++ 客户端

- **状态**：已接受
- **日期**：2026-10-22
- **决策者**：项目所有者

## 背景

`IndComm` 定位为工业现场的多协议通讯测试工具。目前已经支持了 TCP（原始字节流）、串口（原始字节流）以及 Modbus（事务型请求-响应）。随着物联网（IoT）和工业物联网（IIoT）的普及，MQTT 协议因其轻量级、发布/订阅模型和对不稳定网络的适应能力，已成为工业数据采集和云端对接的事实标准。

为了满足测试工程师和自动化流水线对 MQTT 协议调试的需求，我们需要在框架中接入 MQTT 协议。面临的约束如下：
1. **不破坏现有架构**：必须严格遵守 `IProtocol` / `ISession` / `ITransport` 的抽象，不能为 MQTT 在 UI 层写专属的 `if (protocol == "mqtt")` 代码。
2. **功能完整性**：需要支持基础的连接（Broker URL、Client ID、Keep Alive）、发布（Publish）和订阅（Subscribe）。
3. **跨平台与工程化**：底层库必须支持 Windows (MinGW) 和 Linux，且能通过 CMake 以 submodule 的形式无缝集成，不引入庞大的外部构建系统（如强依赖 Boost）。

## 决策

**引入 Eclipse Paho MQTT C++ 库，并新增 `MqttProtocol` 实现。**

具体决策如下：
1. **依赖管理**：通过 git submodule 引入 `third_party/paho.mqtt.c` 和 `third_party/paho.mqtt.cpp`。在 CMake 中开启静态编译（`PAHO_BUILD_STATIC=ON`），暂不开启 TLS（`PAHO_WITH_SSL=OFF`）以降低初次集成的复杂度。
2. **协议抽象**：新增 `MqttProtocol` 类，实现 `IProtocol` 接口。
   - 单模式（`HasModes() = false`）。
   - 字段包括：Broker 地址、端口、Client ID、Keep Alive 等。
3. **会话实现**：新增 `MqttSession` 类，继承 `ISession`。
   - 封装 Paho 的异步客户端（`mqtt::async_client`）。
   - 内部维护订阅状态和消息接收队列。
   - 通过 `OnStateChanged` 和 `OnMessage` 回调将 Paho 的网络事件和消息传递到 UI 线程。
4. **测试步骤**：新增 `MqttPublishStep` 和 `MqttSubscribeStep`，分别对应 Paho 的 Publish 和 Subscribe 操作。
5. **UI 数据驱动**：通过 `VariableParamsFields()` 自动在 UI 上生成 Topic 和 QoS 的输入框，无需写死 MQTT 专属面板。

## 替代方案

| 方案 | 为什么没选 |
|---|---|
| **mqtt_cpp (redboltz)** | Header-only 集成极轻量，但强依赖 Boost.Asio。引入 Boost 会显著增加项目的编译时间、第三方库体积以及依赖复杂度，不符合项目当前“轻量、控制边界”的诉求。 |
| **自研 MQTT 协议实现** | 完整支持 MQTT v3.1.1/v5.0 的报文组装、QoS 1/2 的重传机制、心跳和断线重连工作量极大，且极易在高并发和弱网环境下踩坑，不利于自动化测试的稳定性。 |
| **直接使用 Paho C 库** | Paho C++ 是 C 库的完美封装，提供了现代 C++ 的接口（如 `std::string`、回调、自动管理生命周期），比直接操作 C 指针和手动释放内存安全得多。 |

## 后果

**好处**
- **工业级稳定性**：Paho 内置了完整的自动重连、离线消息缓冲和心跳保活机制，符合现场测试的严苛要求。
- **架构零污染**：`MqttProtocol` 和 `MqttSession` 完全遵循现有的分层规则，UI 层（`SessionPanel`）和自动化引擎（`TestRunner`）不需要为 MQTT 改动一行代码。
- **跨平台**：Paho 官方同时支持 Windows 和 Linux，静态编译后无需额外分发 DLL。

**代价 / 限制**
- **依赖增加**：引入了两个新的 submodule（C 库和 C++ 库），增加了 CI 和初次构建的时间。
- **学习曲线**：Paho 的异步 API 基于回调，需要确保回调在正确的线程（UI 主线程）中执行，这需要严格执行 `CallAfter` 机制。
- **TLS 暂缓**：当前配置关闭了 SSL，如果后续需要测试 `mqtts://`，需重新编译 Paho 并引入 OpenSSL。

**后续必须遵守的约束**
- `MqttProtocol` 必须注册到 `RegisterBuiltinProtocols()` 中。
- MQTT 的测试动作（如 publish/subscribe）必须遵循 `ITestStep` 的接口定义，且底层发送与接收要在 `MqttSession` 中统一处理。
- 如果引入了新的 Paho 版本，必须通过修改 submodule 的 commit 指针来固定版本。

## 参考

- 库：[Eclipse Paho MQTT C++](https://github.com/eclipse/paho.mqtt.cpp)
- 文档：[ADR 0001 协议是一等公民](0001-protocol-as-first-class-with-modes.md)
- 文档：[ADR 0004 事务型协议与字节流协议分离](0004-transaction-protocol-separation.md)
