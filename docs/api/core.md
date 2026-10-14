# 核心 API

> 本文档描述 `src/core/` 的公开接口，签名与头文件逐行核对过。
> 接口变更属于框架变更：先更新本文档与根 `README.md`，再改代码。

所有类型位于命名空间 `icu`，头文件按 `#include "core/Xxx.h"` 引用（`src/` 已在 include 路径中）。

---

## 1. `FieldValue`

`core/ProtocolTypes.h` —— 协议字段值的统一载体（变体类型）。

```cpp
class FieldValue {
public:
    enum class Type { String, Int, Bool, Choice };

    FieldValue() = default;                    // 默认 Type::String，空串
    FieldValue(const std::string& s);          // Type::String
    FieldValue(const char* s);                 // Type::String
    FieldValue(int v);                         // Type::Int
    FieldValue(bool v);                        // Type::Bool

    Type        GetType() const;
    std::string AsString() const;              // 原样返回 m_str
    int         AsInt() const;                 // 原样返回 m_int
    bool        AsBool() const;                // 原样返回 m_bool
    std::string AsChoice() const;              // 同 AsString

    std::string Serialize() const;                              // 转成可写入 .conn 的字符串
    static FieldValue Deserialize(const std::string& s, Type t); // 按类型解析

    bool operator==(const FieldValue& o) const;   // 类型感知
    bool operator!=(const FieldValue& o) const;
};
```

**约定**

- `operator==` **先比类型**：`FieldValue(0)` 与 `FieldValue(false)`、`FieldValue("0")` 互不相等。
- `Choice` 用 `m_str` 存值，`GetType()` 区分它和普通 `String`（`Choice` 表示取值来自固定选项集）。
- `AsInt()` / `AsBool()` 是**直接返回成员**，不做字符串转换；需要从字符串取值时用
  `Deserialize(s, Type::Int)`。写新代码时注意这个区别。
- `Deserialize` 传入的 `Type` 决定解析方式，`.conn` 的字段类型来自协议的 `FieldDescriptor`。

---

## 2. `FieldDescriptor`

`core/ProtocolTypes.h` —— 描述一个可配置字段，驱动 UI 自动生成。

```cpp
struct FieldDescriptor {
    std::string key;            // 存储 key，如 "remotePort"
    std::string label;          // 显示名，同时是翻译 key
    FieldValue::Type type = FieldValue::Type::String;
    FieldValue defaultValue;

    int minValue = 0;           // 仅 type == Int 使用
    int maxValue = 100;

    std::vector<std::pair<std::string, std::string>> choices;   // {value, label}，仅 Choice 使用
};
```

**约定**

- `label` 必须是翻译 key，且要出现在 `.po` 里（`xgettext` 会从 `src/` 提取）。
- `choices` 的 **label 部分**同样是翻译 key，value 部分是存进 `.conn` 的原始值。
- 字段顺序即 UI 中的显示顺序。

---

## 3. `ActiveProfile`

`core/ProtocolTypes.h` —— 一份完整配置的快照，UI 与 `.conn` 读写都以它为单位。

```cpp
struct ActiveProfile {
    std::string protocolId;                        // "tcp"
    int modeIndex = 0;                             // 协议内模式序号
    std::map<std::string, FieldValue> fields;

    void Clear();
    bool Empty() const;                            // protocolId 为空
    bool operator==(const ActiveProfile& o) const; // protocolId + modeIndex + fields
    bool operator!=(const ActiveProfile& o) const;
};
```

`operator==` 用于「是否有未保存修改」判断（`MainFrame::IsModified()`）。

---

## 4. `IProtocol`

`core/IProtocol.h` —— 协议接口。**新增协议 = 新增一个子类，不改 UI。**

```cpp
class IProtocol {
public:
    virtual ~IProtocol() = default;

    virtual std::string Id() const = 0;                   // "tcp"，全局唯一
    virtual std::string DisplayName() const = 0;          // "&TCP/IP"，翻译 key

    virtual bool HasModes() const = 0;
    virtual std::vector<std::string> ModeNames() const = 0;   // 翻译 key 列表

    virtual std::vector<FieldDescriptor> FieldsForMode(int modeIndex) const = 0;

    virtual SessionPtr CreateSession(int modeIndex,
                                     const std::map<std::string, FieldValue>& fields) = 0;

    virtual std::string MakeDefaultFileName(
        int modeIndex, const std::map<std::string, FieldValue>& fields) const = 0;
};
```

**约定**

- 注意 `DisplayName()` / `ModeNames()` / `MakeDefaultFileName()` 返回 **`std::string`**（UTF-8），
  不是 `wxString`；需要 `wxString` 时在调用点用 `wxString::FromUTF8()`。
- `DisplayName()` 里的 `&` 是菜单助记符，UI 显示时由 wxWidgets 处理。
- `CreateSession()` **不得依赖任何 UI**。
- `MakeDefaultFileName()` 生成的名称必须跨平台合法（不含 `\/:*?"<>|`）。

---

## 5. `ITransport`

`core/ITransport.h` —— 传输层。回调注册是**基类里的非虚函数**，子类只负责 `Notify*`。

```cpp
enum class TransportState { Idle, Opening, Reconnecting, Open, Closed };

class ITransport {
public:
    virtual ~ITransport() = default;

    virtual bool Open() = 0;
    virtual void Close() = 0;
    virtual TransportState State() const = 0;
    virtual bool IsOpen() const = 0;
    virtual std::string Target() const = 0;

    virtual size_t Send(const uint8_t* data, size_t len) = 0;   // 返回实际发送字节数
    virtual int ClientCount() const { return 0; }               // 服务器模式有效
    virtual bool IsServerMode() const { return false; }

    // ---------- 回调类型 ----------
    using StateCallback       = std::function<void(TransportState)>;
    using DataCallback        = std::function<void(int clientId, const uint8_t*, size_t)>;
    using ReconnectCallback   = std::function<void(int attempt)>;
    using ClientEventCallback = std::function<void(int clientId)>;

    // ---------- 回调注册（非虚，基类实现）----------
    void OnStateChanged(StateCallback cb);
    void OnDataReceived(DataCallback cb);
    void OnReconnectAttempt(ReconnectCallback cb);
    void OnClientConnected(ClientEventCallback cb);
    void OnClientDisconnected(ClientEventCallback cb);

protected:
    // 供子类触发
    void NotifyState(TransportState st);
    void NotifyData(int clientId, const uint8_t* d, size_t n);
    void NotifyReconnectAttempt(int attempt);
    void NotifyClientConnected(int id);
    void NotifyClientDisconnected(int id);
};
```

**约定**

- `clientId` 是统一客户端/服务器的关键：客户端恒为 `0`，服务器为真实客户端号。
- `Open()` 表示「**已发起**」而不是「已连接」；真实结果通过 `OnStateChanged` 异步通知。
  因此 `MockTransport::Open()` 只把状态切到 `Opening`。
- 每个回调只保存**一个** `std::function`，重复注册会覆盖。
- `ITransport` 的所有权通过 `ITransportPtr`（`unique_ptr`）转移给 `Session`。
- 销毁 `wxSocketBase` 之前先 `Notify(false)`；wxWidgets **没有** `RemoveEventHandler`。

---

## 6. `ICodec`

`core/ICodec.h` —— 只管「消息 ↔ 字节流」的转换。

```cpp
class ICodec {
public:
    virtual ~ICodec() = default;

    virtual bool Encode(const std::vector<uint8_t>& message, std::vector<uint8_t>& out) = 0;

    using MessageCallback = std::function<void(const std::vector<uint8_t>&)>;
    virtual void Feed(const uint8_t* data, size_t len, MessageCallback onMessage) = 0;

    virtual void Reset() = 0;
    virtual std::string Name() const = 0;      // 用于 UI 显示
};
```

**约定**

- `Feed` 的实现**自己维护接收缓冲区**，负责半包 / 粘包；一次调用可能产出 0 个、1 个或多个消息。
- 重连时调用 `Reset()` 清空缓冲。
- header-only 的 Codec 里若用 `_()`，必须 `#include <wx/translation.h>`。

---

## 7. `Session`

`core/Session.h` —— 组合一个 `ITransport` 和一个 `ICodec`，对 UI 暴露统一接口。

```cpp
enum class SessionState { Idle, Connecting, Reconnecting, Connected, Disconnected };

class Session {
public:
    Session(ITransportPtr transport, ICodecPtr codec);   // 接管两者所有权

    bool Open();
    void Close();
    SessionState State() const;
    bool IsConnected() const;

    bool Send(const std::vector<uint8_t>& message);      // 经 Codec::Encode
    bool SendRaw(const uint8_t* data, size_t len);       // 不经 Codec

    std::string Target() const;
    std::string Protocol() const;                        // 协议 Id
    std::string TransportName() const;

    using StateCallback     = std::function<void(SessionState)>;
    using MessageCallback   = std::function<void(int clientId, const std::vector<uint8_t>&)>;
    using RawCallback       = std::function<void(int clientId, const uint8_t*, size_t)>;
    using ReconnectCallback = std::function<void(int attempt)>;
    using ClientEventCallback = std::function<void(int clientId)>;

    void OnStateChanged(StateCallback cb);
    void OnMessage(MessageCallback cb);
    void OnRawData(RawCallback cb);
    void OnReconnectAttempt(ReconnectCallback cb);
    void OnClientConnected(ClientEventCallback cb);
    void OnClientDisconnected(ClientEventCallback cb);

    ITransport* Transport() const;
    bool IsServerMode() const;
};
```

**约定**

- `Open()` 幂等：已连接 / 连接中时不重复发起，也不重复通知。
- 状态只在**真正改变**时通知一次。
- `OnMessage` 收到的是 `Feed` 切帧后的完整消息；`OnRawData` 是未切帧的原始字节，两者互相独立。
- 与 `ITransport` 一样，每类回调只保存一个 `std::function`。
- 所有 UI 更新必须经 `CallAfter` 回主线程（根 `README.md` 第 10.6 节）。

---

## 8. `SessionManager`

`core/SessionManager.h` —— 持有唯一活动会话。不可拷贝。

```cpp
class SessionManager {
public:
    void SetActive(SessionPtr s);      // 会先关闭旧会话
    void Close();
    bool IsConnected() const;
    Session* Active() const;
    SessionState ActiveState() const;
};
```

**约定**

- `SetActive(new)` / `Close()` 之后**不得再访问旧对象的裸指针**（use-after-free）。
  需要销毁后仍可观察的计数，用 `shared_ptr<int>` 持有。

---

## 9. `ProtocolRegistry`

`core/ProtocolRegistry.h` —— 协议注册中心（单例）。

```cpp
class ProtocolRegistry {
public:
    static ProtocolRegistry& Instance();

    void Register(IProtocolPtr proto);              // 接管所有权
    IProtocol* Find(const std::string& id) const;   // 未找到返回 nullptr
    std::vector<IProtocol*> All() const;
};

void RegisterBuiltinProtocols();                    // 在 App::OnInit 里调用一次
```

**约定**

- 新增协议必须在 `RegisterBuiltinProtocols()` 里注册，否则菜单里不会出现。
  `All()` 的顺序决定菜单项顺序。
- `Find()` 返回的是**借用指针**，所有权仍在注册表。

---

## 10. 相关文档

- [架构总览](../architecture/overview.md)
- [新增协议](../developer/adding-a-protocol.md)
- [测试指南](../developer/testing.md)
- 根 `README.md` 第 4 节「关键接口」
