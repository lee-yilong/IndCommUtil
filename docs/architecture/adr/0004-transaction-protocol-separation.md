```markdown
# 0004. 事务型协议与字节流协议分离

- **状态**：已接受
- **日期**：2026-10-14
- **决策者**：项目所有者

## 背景

阶段 6 要接入 Modbus 协议，需要同时支持 **TCP / RTU / ASCII** 三种模式，以及
**Client (Master) / Server (Slave)** 两种角色。经过评估，选用开源库
[serhmarch/ModbusLib](https://github.com/serhmarch/ModbusLib)。

### ModbusLib 的 API 形态

ModbusLib 的 API 是**事务型**的：

```cpp
// 创建客户端端口
Modbus::NetSettings settings;
settings.host = "127.0.0.1";
settings.port = 502;
settings.timeout = 3000;
ModbusClientPort* port = Modbus::createClientPort(Modbus::TCP, &settings, true);

// 一次调用完成：发请求 + 等响应 + 校验 CRC + 校验功能码 + 超时 + 重试
uint16_t values[16];
Modbus::StatusCode status = port->readHoldingRegisters(1, 0, 16, values);
if (Modbus::StatusIsGood(status)) {
    // values 里是结构化数据
}
```

**它不暴露裸字节流。** 用户拿到的不是"发了一批字节、收了一批字节"，而是
"成功/失败 + 结构化数据"。

### 现有架构的形态

现有 `ITransport` / `ICodec` 是**字节流**设计：

```cpp
class ITransport {
    virtual size_t Send(const uint8_t* data, size_t len) = 0;  // fire-and-forget
    using DataCallback = std::function<void(int clientId, const uint8_t*, size_t)>;
    void OnDataReceived(DataCallback cb);                       // 异步字节到达
};

class ICodec {
    virtual bool Encode(const std::vector<uint8_t>& message,
                        std::vector<uint8_t>& out) = 0;         // 消息 → 字节
    virtual void Feed(const uint8_t* data, size_t len,
                      MessageCallback onMessage) = 0;           // 字节 → 消息
};
```

- `Send()` 是 fire-and-forget，不等待响应
- `Encode` / `Feed` 是字节流双向转换
- 超时 / 重试由 Transport 层在**连接级**做（重连），不是**请求级**

### 冲突点

| 现有抽象 | ModbusLib 需要的 |
|---|---|
| `Send(bytes)` fire-and-forget | 请求-响应必须**配对** |
| `OnDataReceived` 异步 | 响应要**同步返回**（或 future） |
| 超时/重试在连接级 | 超时/重试在**请求级** |
| `ICodec::Encode` 输入是 `vector<uint8_t>` | 输入是 `{slaveId, funcCode, addr, count}` |
| `ICodec::Feed` 输出是 `vector<uint8_t>` | 输出是 `{funcCode, data[], exceptionCode}` |

**5 个冲突点，没有一个能靠"小修 ITransport/ICodec"解决。**

### 如果强行塞进现有抽象会怎样

需要给 `ITransport` 加：

```cpp
virtual bool Transact(const uint8_t* req, size_t reqLen,
                      std::vector<uint8_t>& resp, int timeoutMs) { return false; }
```

但 `Transact()` 的入参是字节流，ModbusLib 需要的是结构化参数。所以还必须写一个
`ModbusRequestCodec`，把字节流解析成结构体，再把响应编码回字节流。**等于在字节流
协议和事务协议之间加了一层多余的编解码，类型安全还崩了。**

## 决策

**事务型协议与字节流协议分离，各自使用独立抽象。**

- 字节流协议（TCP / 串口 / MQTT / ...）继续走
  `IProtocol + ITransport + ICodec + Session`
- 事务型协议（Modbus / OPC UA / ...）走新的
  `ITransactionProtocol + ITransactionSession`
- 两者都由 `ProtocolRegistry` 注册，注册表记录协议类型
  （`ProtocolKind::ByteStream` / `ProtocolKind::Transaction`）
- UI 层按协议类型分派面板：字节流用 `ProtocolSessionPanel`，
  事务型用专属面板（Modbus 用 `ModbusPanel`）
- **Modbus 是一个协议三个模式**（TCP / RTU / ASCII），Client / Server 通过
  字段 `role` 区分，由 `FieldsForMode(modeIndex)` 描述

### 抽象接口草案

```cpp
// src/core/ITransactionProtocol.h
class ITransactionProtocol {
public:
    virtual ~ITransactionProtocol() = default;

    virtual std::string Id() const = 0;             // "modbus"
    virtual std::string DisplayName() const = 0;    // "&Modbus"

    virtual bool HasModes() const = 0;
    virtual std::vector<std::string> ModeNames() const = 0;  // {"TCP", "RTU", "ASCII"}

    virtual std::vector<FieldDescriptor> FieldsForMode(int modeIndex) const = 0;

    virtual std::unique_ptr<ITransactionSession> CreateTransactionSession(
        int modeIndex,
        const std::map<std::string, FieldValue>& fields) = 0;

    virtual std::string MakeDefaultFileName(
        int modeIndex,
        const std::map<std::string, FieldValue>& fields) const = 0;
};

// src/core/ITransactionSession.h
enum class TransactionState { Idle, Connecting, Connected, Disconnected };

struct TransactionRequest {
    int slaveId = 1;
    int functionCode = 0;   // 01/02/03/04/05/06/0F/10...
    int startAddress = 0;
    int count = 0;
    std::vector<uint16_t> writeValues;
};

struct TransactionResponse {
    bool ok = false;
    std::string errorMessage;
    std::vector<uint16_t> registers;
    std::vector<bool> coils;
};

class ITransactionSession {
public:
    virtual ~ITransactionSession() = default;

    virtual bool Open() = 0;
    virtual void Close() = 0;
    virtual TransactionState State() const = 0;
    virtual bool IsConnected() const = 0;

    virtual TransactionResponse Execute(const TransactionRequest& request) = 0;

    virtual std::string Target() const = 0;

    using StateCallback = std::function<void(TransactionState)>;
    virtual void OnStateChanged(StateCallback cb) = 0;
};
```

### UI 分派

`ProtocolRegistry` 增加类型查询：

```cpp
enum class ProtocolKind { ByteStream, Transaction };

class ProtocolRegistry {
    // ...
    ProtocolKind KindOf(const std::string& id) const;
};
```

`MainFrame::OnMenuOpenProtocol` 按 `KindOf()` 决定：

- `ByteStream` → `GenericProtocolDialog` + `ProtocolSessionPanel`
- `Transaction` → `GenericProtocolDialog` + `ModbusPanel`（或其他事务面板）

**不在 UI 里硬编码 `if (protocolId == "modbus")`。**

## 替代方案

| 方案 | 为什么没选 |
|---|---|
| 路径 B：给 `ITransport` 加 `Transact()` | 接口语义变糊；ModbusLib 的结构化请求需要额外编解码层；现有所有 Transport 实现和测试都要改；未来 OPC UA 也会继续污染接口 |
| 路径 B 变种：把 ModbusLib 包成 `ModbusCodec` | 需要 `std::any` 传递结构体，类型安全丧失；同样污染 `ICodec` |
| 路径 A 变种：Modbus 拆成 6 个独立协议（TCP-Client / TCP-Server / RTU-Master / ...） | 违反 ADR 0001 的「协议是一等公民」；菜单膨胀；配置文件分裂；模式组合爆炸 |
| 自研 Modbus 实现，不依赖第三方库 | 工作量大；CRC / 超时 / 重试 / 异常处理容易出错；缺少跨平台串口支持；阶段 6 周期不可控 |
| 用其他 Modbus 库（如 libmodbus） | libmodbus 不支持 ASCII；LGPL 与静态链接策略冲突 |

## 后果

**好处**

- 语义匹配：ModbusLib 的 API 直接映射到 `ITransactionSession::Execute()`
- 隔离性：字节流与事务型协议各有专属抽象，互不干扰
- 现有代码零改动：`ITransport` / `ICodec` / `Session` 完全不动，现有测试不受影响
- 未来可扩展：OPC UA、S7 等事务型协议直接复用 `ITransactionProtocol`
- 符合 ADR 0001：Modbus 是一个协议，内部有 Mode
- 符合 ADR 0002：`FieldsForMode()` 依然驱动配置对话框，UI 数据驱动

**代价 / 限制**

- `ProtocolRegistry` 需要记录协议类型，`MainFrame` 需要按类型分派面板
- `Session` 与 `ITransactionSession` 有部分重复（状态枚举、回调机制），
  暂不做统一，避免过度抽象
- 新增协议类型需要同时理解两种抽象
- `ITransactionSession::Execute()` 是同步阻塞的，UI 层需要处理（放到后台线程或
  用 `CallAfter` 避免卡界面）

**后续必须遵守的约束**

- 事务型协议**不得**修改 `ITransport` / `ICodec` / `Session`
- 新增事务型协议必须实现 `ITransactionProtocol`，注册到 `ProtocolRegistry`
  并声明 `ProtocolKind::Transaction`
- UI 层不得出现 `if (protocolId == "modbus")` 之类的硬编码，必须通过
  `ProtocolKind` 分派
- Modbus 的三种模式（TCP / RTU / ASCII）必须在同一个协议类内，不得拆分
- `ITransactionSession` 的 `Execute()` 在后台线程执行时，结果必须经 `CallAfter`
  抛回主线程再更新 UI

## 实施路径（阶段 6）

1. 添加 `third_party/ModbusLib` submodule
2. 新增 `src/core/ITransactionProtocol.h` / `src/core/ITransactionSession.h`
3. `ProtocolRegistry` 加 `RegisterTransaction()` 和 `KindOf()`
4. `MainFrame` 按 `KindOf()` 分派会话面板
5. 新建 `src/protocols/modbus/`：`ModbusProtocol` / `ModbusTransactionSession`
6. 新建 `src/ui/ModbusPanel.h/.cpp`：事务面板（从站地址 / 功能码 / 地址 / 数量）
7. 单测：`test_ModbusProtocol.cpp`（元数据 / 字段描述 / `.conn` roundtrip）
8. 集成：与 Modbus Slave 模拟器对接

## 参考

- 代码：`src/core/IProtocol.h`、`src/core/ITransport.h`、`src/core/ICodec.h`
- 代码：`src/protocols/tcp/TcpProtocol.h`（字节流协议范例）
- 文档：[ADR 0001 协议是一等公民](0001-protocol-as-first-class-with-modes.md)
- 文档：[ADR 0002 UI 由 FieldDescriptor 数据驱动](0002-ui-data-driven-by-field-descriptors.md)
- 文档：[ADR 0005 串口协议与通用面板增强](0005-serial-protocol-and-common-panel-enhancement.md)
- 库：[serhmarch/ModbusLib](https://github.com/serhmarch/ModbusLib)
```
