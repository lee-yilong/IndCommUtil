# Industrial Communication Utility — 开发指南与 AI Skill

> **文档定位**：本文档既是项目开发文档，也是 AI 辅助开发的 Skill 定义。所有开发按本文档推进，AI 在协助编码时必须遵循本文档的架构、约定和任务清单。
>
> **本版重点**：阶段 4.6 全部完成（CI + clang-tidy + 版本管理 + CHANGELOG/LICENSE + docs + scripts）。下一阶段 4.7 清理。

---

## 目录

1. [项目概述](#1-项目概述)
2. [核心设计原则](#2-核心设计原则)
3. [架构设计](#3-架构设计)
4. [关键接口](#4-关键接口)
5. [目录结构](#5-目录结构)
6. [应用设置模块](#6-应用设置模块)
7. [协议配置文件格式](#7-协议配置文件格式)
8. [协议清单](#8-协议清单)
9. [测试策略](#9-测试策略)
10. [编码约定](#10-编码约定)
11. [CI 与代码质量](#11-ci-与代码质量)
12. [开发路线图](#12-开发路线图)
13. [AI Skill 指南](#13-ai-skill-指南)
14. [关键决策记录](#14-关键决策记录)
15. [变更日志](#15-变更日志)
16. [附录](#附录)
    - [A 快速命令参考](#附录-a快速命令参考)
    - [B 协议实现检查清单](#附录-b协议实现检查清单)
    - [C 设置项实现检查清单](#附录-c设置项实现检查清单)
    - [D 测试编写检查清单](#附录-d测试编写检查清单)
    - [E 标准测试流程](#附录-e标准测试流程)

---

## 1. 项目概述

### 1.1 定位

**Industrial Communication Utility**（工业通信实用工具）是一个面向工业现场的多协议通讯测试/调试工具，目标类似 NetAssist、Modbus Poll、MQTT Explorer 等工具的整合版本。

### 1.2 核心目标

| 目标 | 含义 |
|---|---|
| **协议可插拔** | 新增协议 = 新增一个类，不改 UI、不改菜单、不改对话框 |
| **UI 数据驱动** | 配置对话框、会话面板、菜单项全部从协议描述自动生成 |
| **分层解耦** | 应用层 / 框架层 / 协议层 / UI 层 / 服务层，依赖单向 |
| **设置集中管理** | 语言、字体、主题、图标等应用级设置统一走 `settings/` |
| **测试可回归** | `core/` / `protocols/` / `settings/` / `config/` 有单测覆盖 |
| **工程化规范** | CI、静态分析、版本管理、文档、打包全流程 |
| **跨平台** | Windows 优先，保留 Linux / macOS 兼容性 |

### 1.3 技术栈

| 项 | 选择 |
|---|---|
| 语言 | C++17 |
| GUI | wxWidgets 3.3（MSYS2 UCRT64 GCC 16.1.0） |
| 构建 | CMake ≥ 3.20 + MinGW Makefiles / Ninja |
| 日志 | spdlog（header-only） |
| 测试 | doctest（header-only） |
| i18n | GNU gettext（.po / .mo） |
| 网络 | wxSocket（内置） |
| 静态分析 | clang-tidy + clang-format |
| CI | GitHub Actions |
| 覆盖率 | gcovr + GCC `--coverage` |
| 调试器 | LLDB-DAP（GDB 17.2 与 cpptools MI 握手失败） |
| 串口 | 待定（wxWidgets 无内置，需自实现或第三方库） |

---

## 2. 核心设计原则

### 2.1 协议是一等公民

**协议是一个整体，不是「客户端」和「服务器」两个东西。**

- `TcpProtocol` 是一个协议类，`Id() = "tcp"`，`DisplayName() = "TCP/IP"`
- 「客户端 / 服务器」是 TCP 协议**内部的一种模式（Mode）**
- 一个 `.conn` 文件 = 一份协议配置（含 Mode），**不是两份**

**反例（禁止）**：

```
❌ TcpClientProtocol + TcpServerProtocol
❌ 两个独立的 .conn 文件
❌ UI 里硬编码两套字段
❌ 菜单里"TCP客户端"和"TCP服务器"两个入口
```

**正例（目标）**：

```
✅ TcpProtocol（一个类，内部根据 Mode 分发）
✅ 一个 .conn 文件，Mode=Client 或 Server
✅ UI 从 IProtocol 的字段描述自动生成
✅ 菜单里只有"TCP/IP"一个入口
```

### 2.2 UI 完全数据驱动

协议通过 `FieldDescriptor` 列表描述自己的字段，`GenericProtocolDialog` 据此自动生成控件。**新增协议不得触碰 UI 代码。**

```
IProtocol::FieldsForMode(mode)  →  [ FieldDescriptor, ... ]
                                        ↓
                              GenericProtocolDialog 自动生成
                                        ↓
                              [wxTextCtrl / wxSpinCtrl / wxChoice / wxCheckBox]
```

### 2.3 分层清晰，依赖单向

```
┌─────────────────────────────────────────────────────────┐
│  app/          应用入口（App / MainFrame）                │
└─────────────────────────┬───────────────────────────────┘
                          │
┌─────────────────────────▼───────────────────────────────┐
│  ui/           UI 层（对话框 / 面板 / 窗口）               │
└─────────────────────────┬───────────────────────────────┘
                          │
┌─────────────────────────▼───────────────────────────────┐
│  protocols/    协议层（TcpProtocol / SerialProtocol / ...）│
└─────────────────────────┬───────────────────────────────┘
                          │
┌─────────────────────────▼───────────────────────────────┐
│  core/         框架层（接口 + 基础类型 + Session）         │
└─────────────────────────┬───────────────────────────────┘
                          │
┌─────────────────────────▼───────────────────────────────┐
│  settings/  config/  log/   基础服务层                    │
└─────────────────────────────────────────────────────────┘
```

**依赖规则**：

| 层 | 允许依赖 | 禁止依赖 |
|---|---|---|
| `app/` | `ui/ core/ settings/ config/ log/` | — |
| `ui/` | `core/ settings/` | `protocols/`（具体协议） |
| `protocols/` | `core/ log/` | `ui/ settings/` |
| `core/` | C++ STL、wxBase | `ui/ protocols/ settings/ config/` |
| `settings/` | `log/` | `core/ ui/ protocols/` |
| `config/` | `core/` | `ui/ protocols/ settings/` |
| `log/` | wxBase、spdlog | 业务逻辑 |

### 2.4 单一数据源

- **协议配置**：唯一来源是 `.conn` 文件（`config/` 管）
- **应用设置**：唯一来源是 `settings/` 模块
- **协议描述**：唯一来源是 `IProtocol::FieldsForMode()`
- **协议列表**：唯一来源是 `ProtocolRegistry::All()`
- **会话状态**：唯一来源是 `Session::State()`

**禁止跨模块直接操作底层存储**（如绕过 `Settings` 直接调 `wxConfigBase`）。

### 2.5 UI 生命周期安全

**任何"用户操作触发 UI 自身销毁"的场景，都必须用 `CallAfter` 延迟。**

典型场景：

- 按钮点击 → 切页面 → 销毁当前面板
- 菜单点击 → 重建窗口 → 销毁当前窗口
- 托盘退出 → 销毁托盘自身

**原因**：wxWidgets 的事件派发是**同步**的。如果在回调栈里 `delete` 了事件派发者，返回后访问成员会崩溃。

---

## 3. 架构设计

### 3.1 完整数据流

```
用户点击菜单"新建连接 → TCP/IP"
           │
           ▼
MainFrame 从 ProtocolRegistry 查 tcp 协议
           │
           ▼
创建 GenericProtocolDialog(protocol=tcp, initialMode=Client)
           │  从 IProtocol::FieldsForMode(Client) 生成字段控件
           ▼
用户填完点 OK
           │
           ▼
得到 { modeIndex, fields: map<key, FieldValue> }
           │
           ▼
IProtocol::CreateSession(modeIndex, fields)
           │  内部：
           │    if (mode == Client) → TcpClientTransport
           │    else                 → TcpServerTransport
           │  + RawCodec
           │  → Session
           ▼
SessionManager::SetActive(session)
           │
           ▼
UI 切换到 ProtocolSessionPanel
           │
           ▼
Session::Open() → Transport::Open() → 建立连接/监听
           │
           ▼
数据回调 → Session → UI 面板刷新
```

### 3.2 消息生命周期

**发送**：

```
UI 面板 Send(text)
    → Session::Send(vector<uint8_t>)
    → ICodec::Encode(message) → bytes
    → ITransport::Send(bytes)
    → wxSocket::Write
```

**接收**：

```
wxSocket Input 事件
    → ITransport::NotifyData(clientId, bytes)
    → Session::HandleTransportData
        ├─→ OnRawData 回调（原始字节视图）
        └─→ ICodec::Feed(bytes) → 一条或多条消息
            → Session::OnMessage(clientId, message)
            → UI 面板显示
```

---

## 4. 关键接口

### 4.1 `IProtocol`

```cpp
class IProtocol {
public:
    virtual ~IProtocol() = default;

    virtual std::string Id() const = 0;             // "tcp"
    virtual std::string DisplayName() const = 0;    // "&TCP/IP"

    virtual bool HasModes() const = 0;
    virtual std::vector<std::string> ModeNames() const = 0;

    virtual std::vector<FieldDescriptor> FieldsForMode(int modeIndex) const = 0;

    virtual SessionPtr CreateSession(
        int modeIndex,
        const std::map<std::string, FieldValue>& fields) = 0;

    virtual std::string MakeDefaultFileName(
        int modeIndex,
        const std::map<std::string, FieldValue>& fields) const = 0;
};
```

### 4.2 `FieldDescriptor`

```cpp
struct FieldDescriptor {
    std::string key;
    std::string label;          // 翻译 key
    FieldValue::Type type;
    FieldValue defaultValue;

    int minValue = 0;
    int maxValue = 100;

    std::vector<std::pair<std::string, std::string>> choices;
};
```

### 4.3 `FieldValue`

```cpp
class FieldValue {
public:
    enum class Type { String, Int, Bool, Choice };

    FieldValue();
    FieldValue(const std::string& s);
    FieldValue(int v);
    FieldValue(bool v);

    Type        GetType() const;
    std::string AsString() const;
    int         AsInt()    const;
    bool        AsBool()   const;

    std::string Serialize() const;
    static FieldValue Deserialize(const std::string& s, Type t);

    bool operator==(const FieldValue& o) const;
    bool operator!=(const FieldValue& o) const;
};
```

### 4.4 `ProtocolRegistry`

```cpp
class ProtocolRegistry {
public:
    static ProtocolRegistry& Instance();
    void Register(std::unique_ptr<IProtocol> proto);
    IProtocol* Find(const std::string& id) const;
    std::vector<IProtocol*> All() const;
};
void RegisterBuiltinProtocols();
```

### 4.5 `ITransport`

```cpp
class ITransport {
public:
    virtual bool           Open() = 0;
    virtual void           Close() = 0;
    virtual TransportState State() const = 0;
    virtual bool           IsOpen() const = 0;
    virtual bool           IsServerMode() const { return false; }
    virtual std::string    Target() const = 0;
    virtual size_t         Send(const uint8_t* data, size_t len) = 0;
    virtual int            ClientCount() const { return 0; }

    using StateCallback       = std::function<void(TransportState)>;
    using DataCallback        = std::function<void(int clientId, const uint8_t*, size_t)>;
    using ReconnectCallback   = std::function<void(int attempt)>;
    using ClientEventCallback = std::function<void(int clientId)>;

    void OnStateChanged(StateCallback cb);
    void OnDataReceived(DataCallback cb);
    void OnReconnectAttempt(ReconnectCallback cb);
    void OnClientConnected(ClientEventCallback cb);
    void OnClientDisconnected(ClientEventCallback cb);
};
```

### 4.6 `ICodec`

```cpp
class ICodec {
public:
    virtual bool Encode(const std::vector<uint8_t>& message,
                        std::vector<uint8_t>& out) = 0;

    using MessageCallback = std::function<void(const std::vector<uint8_t>&)>;
    virtual void Feed(const uint8_t* data, size_t len,
                      MessageCallback onMessage) = 0;

    virtual void Reset() = 0;
    virtual std::string Name() const = 0;
};
```

### 4.7 `Session`

```cpp
class Session {
public:
    Session(ITransportPtr transport, ICodecPtr codec);

    bool         Open();
    void         Close();
    SessionState State() const;
    bool         IsConnected() const;
    bool         IsServerMode() const;

    bool Send(const std::vector<uint8_t>& message);
    bool SendRaw(const uint8_t* data, size_t len);

    std::string Target() const;
    std::string Protocol() const;
    std::string TransportName() const;

    using StateCallback       = std::function<void(SessionState)>;
    using MessageCallback     = std::function<void(int clientId, const std::vector<uint8_t>&)>;
    using RawCallback         = std::function<void(int clientId, const uint8_t*, size_t)>;
    using ReconnectCallback   = std::function<void(int attempt)>;
    using ClientEventCallback = std::function<void(int clientId)>;

    void OnStateChanged(StateCallback cb);
    void OnMessage(MessageCallback cb);
    void OnRawData(RawCallback cb);
    void OnReconnectAttempt(ReconnectCallback cb);
    void OnClientConnected(ClientEventCallback cb);
    void OnClientDisconnected(ClientEventCallback cb);
};
```

---

## 5. 目录结构

### 5.1 当前目录（阶段 4.5 完成后）

```
IndustrialCommunicationUtility/
├── src/
│   ├── main.cpp
│   ├── app/
│   │   ├── App.h/.cpp
│   │   └── MainFrame.h/.cpp
│   ├── core/
│   │   ├── ICodec.h
│   │   ├── ITransport.h
│   │   ├── IProtocol.h
│   │   ├── ProtocolTypes.h/.cpp      ← 4.4 合并（FieldValue / FieldDescriptor / ActiveProfile）
│   │   ├── ProtocolRegistry.h/.cpp
│   │   ├── Session.h/.cpp
│   │   └── SessionManager.h/.cpp
│   ├── protocols/
│   │   ├── tcp/
│   │   │   ├── TcpProtocol.h/.cpp
│   │   │   └── TcpTransport.h/.cpp
│   │   └── codec/
│   │       ├── RawCodec.h/.cpp
│   │       └── ModbusRtuCodec.h/.cpp （.cpp 待阶段 6 实现）
│   ├── ui/
│   │   ├── GenericProtocolDialog.h/.cpp
│   │   ├── ProtocolSessionPanel.h/.cpp
│   │   ├── LogFrame.h/.cpp
│   │   └── AppTaskBarIcon.h/.cpp
│   ├── settings/
│   │   ├── SettingsKeys.h
│   │   ├── Settings.h/.cpp
│   │   ├── LanguageSettings.h/.cpp
│   │   ├── UISettings.h/.cpp
│   │   ├── FontSettings.h/.cpp
│   │   ├── ThemeSettings.h/.cpp
│   │   ├── IconSettings.h/.cpp
│   │   └── README.md
│   ├── config/
│   │   └── ConnectionProfile.h/.cpp
│   └── log/
│       └── Log.h/.cpp
├── tests/
│   ├── CMakeLists.txt
│   ├── main.cpp                       ← doctest 实现 + main
│   ├── unit/
│   │   ├── core/
│   │   │   ├── test_FieldValue.cpp
│   │   │   ├── test_ActiveProfile.cpp
│   │   │   ├── test_ProtocolRegistry.cpp
│   │   │   ├── test_MockTransport.cpp
│   │   │   ├── test_Session.cpp
│   │   │   └── test_SessionManager.cpp
│   │   └── protocols/
│   │       └── codec/
│   │           └── test_RawCodec.cpp
│   ├── integration/
│   │   └── test_ConnectionProfile.cpp
│   ├── mocks/
│   │   ├── MockTransport.h
│   │   └── MockCodec.h
│   ├── fixtures/
│   └── helpers/
│       └── TestEnv.h
├── resources/
│   ├── app.rc
│   └── i18n/
├── third_party/
│   ├── wxWidgets/
│   ├── spdlog/
│   └── doctest/
└── CMakeLists.txt
```

### 5.2 完整目标结构（含未来模块）

```
IndustrialCommunicationUtility/
├── src/                              （同 5.1）
├── tests/                            ✅ 4.5 已建
├── docs/                             ✅ 4.6 已建
│   ├── architecture/
│   │   ├── overview.md
│   │   ├── layers.md
│   │   └── adr/                      （README 索引 + template + 0001~0003）
│   ├── user/
│   │   ├── getting-started.md
│   │   ├── tcp.md
│   │   └── troubleshooting.md
│   ├── developer/
│   │   ├── building.md
│   │   ├── adding-a-protocol.md
│   │   ├── adding-a-setting.md
│   │   └── testing.md
│   └── api/
│       └── core.md
├── scripts/                          ✅ 4.6 已建
│   ├── build.sh
│   ├── test.sh
│   ├── format.sh
│   ├── coverage.sh
│   └── update_i18n.sh
├── cmake/                            ✅ 4.6 已建
│   └── version.h.in
├── packaging/                        ★ 待建（阶段 10）
├── resources/
│   ├── app.rc
│   ├── icons/
│   └── i18n/
├── third_party/
│   ├── wxWidgets/
│   ├── spdlog/
│   └── doctest/
├── .vscode/
├── .github/workflows/build.yml       ✅ 4.6.1
├── CMakeLists.txt
├── CMakePresets.json
├── .clang-format
├── .clang-tidy                       ✅ 4.6.2
├── .gitattributes                    ✅ 4.6（脚本用 LF，防 CRLF 破坏 shebang）
├── .gitignore
├── README.md
├── CHANGELOG.md                      ✅ 4.6.4
├── LICENSE                           ✅ 4.6.5（MIT）
└── CONTRIBUTING.md                   ✅ 4.6
```

### 5.3 各目录职责

| 目录 | 职责 | 不得包含 | 依赖 |
|---|---|---|---|
| `src/app/` | 启动、主窗口骨架、菜单 | 业务逻辑、协议细节 | `ui/ core/ settings/ config/ log/` |
| `src/core/` | 接口、基础类型、Session | wxWidgets UI、具体协议 | C++ STL、wxBase |
| `src/protocols/` | 具体协议实现、Codec | UI 代码 | `core/`、`log/` |
| `src/ui/` | 窗口、对话框、面板 | 协议逻辑、socket 操作 | `core/ settings/` |
| `src/settings/` | 应用级设置 | 协议配置、业务逻辑 | `log/` |
| `src/config/` | `.conn` 协议配置文件读写 | UI、传输 | `core/` |
| `src/log/` | 日志初始化、GUI sink | 业务逻辑 | wxBase、spdlog |
| `tests/` | 单元 / 集成测试 | 生产代码 | `core/`、`protocols/`、`settings/`、`config/` |
| `docs/` | 文档、ADR | 代码 | — |

### 5.4 分层规则

**新增功能时先问自己：改动属于哪一层？**

| 需求 | 归属 |
|---|---|
| 新协议 | `protocols/<name>/` |
| 新编解码 | `protocols/codec/` |
| 新对话框 | `ui/` |
| 新设置项 | `settings/` |
| 协议配置格式变化 | `config/` |
| 框架接口扩展 | `core/` |
| 菜单项调整 | `app/MainFrame` |
| 新增测试 | `tests/unit/` 或 `tests/integration/` |
| 新增决策 | `docs/adr/` |

---

## 6. 应用设置模块

### 6.1 设计目标

- **集中管理**：所有应用级设置在一个目录
- **类型安全**：每个设置项有明确类型
- **可扩展**：新增设置只改 `settings/` 目录内的 3 个文件
- **零耦合**：`core/` 和 `protocols/` 不知道 settings 存在

### 6.2 与 `config/` 的区别

| | `settings/` | `config/` |
|---|---|---|
| 内容 | 应用级偏好（语言、窗口、字体、主题） | 协议配置（.conn 文件） |
| 存储 | wxConfig（注册表 / ini） | 独立 `.conn` 文件 |
| 用户感知 | 隐式 | 显式 |
| 生命周期 | 应用级 | 会话级 |

### 6.3 模块结构

```
settings/
├── SettingsKeys.h             # 所有 key 的常量
├── Settings.h/.cpp            # 通用 key-value 读写（单例）
├── LanguageSettings.h/.cpp    # 语言
├── UISettings.h/.cpp          # 窗口几何 + UI 状态
├── FontSettings.h/.cpp        # 字体（预留）
├── ThemeSettings.h/.cpp       # 主题（预留）
├── IconSettings.h/.cpp        # 图标（预留）
└── README.md
```

### 6.4 关键约定

- **`Settings` 是唯一允许直接操作 `wxConfigBase` 的类**
- 所有 key 在 `SettingsKeys.h` 集中定义
- 新设置项 = 加 key + 加接口 + 加实现

### 6.5 设置项清单

| 类别 | 设置项 | 状态 |
|---|---|---|
| **语言** | 界面语言 | ✅ 已实现 |
| **窗口** | 主窗口几何 | ✅ 已实现 |
| **窗口** | 日志窗口几何 | 🟡 待迁移 |
| **窗口** | 关闭时最小化到托盘 | 🟡 待迁移 |
| **UI** | 字体（UI / 日志 / 数据） | ⬜ 预留 |
| **UI** | 主题（浅色 / 深色） | ⬜ 预留 |
| **UI** | 十六进制显示 | ⬜ 预留 |
| **UI** | 自动滚动 | ⬜ 预留 |
| **图标** | 协议图标 | ⬜ 预留 |
| **图标** | UI 图标 | ⬜ 预留 |
| **最近连接** | 最近打开的 `.conn` 列表 | ✅ 已实现 |

**图例**：✅ 已实现 / 🟡 待迁移 / ⬜ 预留

### 6.6 怎么加新设置

**场景**：加一个"是否在关闭时最小化到托盘"设置。

1. **加 key** —— `SettingsKeys.h`
2. **加接口** —— `UISettings.h`
3. **加实现** —— `UISettings.cpp`
4. **加 UI** —— `ui/SettingsDialog`
5. **加使用** —— 使用点调用

**全程只改 `settings/` + `SettingsDialog` + 使用点，不触碰 `core/` 和 `protocols/`。**

---

## 7. 协议配置文件格式

### 7.1 `.conn` 通用结构

INI 格式（`wxFileConfig`）：

```ini
[Connection]
Protocol=tcp
Mode=Client
<协议专属字段>=<值>
```

**必须字段**：
- `Protocol` — 协议 ID
- `Mode` — 模式名

### 7.2 TCP 客户端

```ini
[Connection]
Protocol=tcp
Mode=Client
RemoteHost=127.0.0.1
RemotePort=502
LocalHost=0.0.0.0
LocalPort=0
TimeoutMs=3000
AutoReconnect=1
MaxReconnectAttempts=3
ReconnectDelaySec=5
```

### 7.3 TCP 服务器

```ini
[Connection]
Protocol=tcp
Mode=Server
ListenHost=0.0.0.0
ListenPort=502
MaxClients=10
ListenBacklog=5
```

### 7.4 文件命名约定

```
<协议>_<模式>_<主标识>_<次标识>.conn

例：
  TCP_Client_127_0_0_1_502.conn
  TCP_Server_0_0_0_0_502.conn
  ModbusRTU_Master_COM3_9600.conn
```

由 `IProtocol::MakeDefaultFileName()` 生成。

---

## 8. 协议清单

| 协议 | ID | 模式 | 传输 | 编解码 | 状态 |
|---|---|---|---|---|---|
| **TCP/IP** | `tcp` | Client / Server | TcpClient / TcpServer | Raw | ✅ 完成 |
| 串口 | `serial` | — | SerialTransport | Raw | ⬜ 待实现 |
| Modbus RTU | `modbus-rtu` | Master / Slave | SerialTransport | ModbusRtu | ⬜ 待实现 |
| Modbus TCP | `modbus-tcp` | Master / Slave | TcpClient / TcpServer | ModbusTcp | ⬜ 待实现 |
| MQTT | `mqtt` | Pub / Sub | TcpClient | MqttCodec | ⬜ 待实现 |
| OPC UA | `opcua` | Client / Server | 专用 | 专用 | ⬜ 待实现 |
| Siemens TIA | `tia` | Client | TcpClient | S7Codec | ⬜ 待实现 |
| Allen-Bradley | `ab` | Client | TcpClient | CIPCodec | ⬜ 待实现 |
| 三菱 | `mitsubishi` | Client | TcpClient | MCProtocol | ⬜ 待实现 |

**图例**：✅ 完成 / 🟡 进行中 / ⬜ 未开始

---

## 9. 测试策略

### 9.1 测试框架

**doctest**（header-only，零构建配置，编译极快）。放在 `third_party/doctest/doctest.h`。

### 9.2 测试分层

| 层级 | 目标 | 覆盖范围 |
|---|---|---|
| **单元测试** | 每个类独立可测 | `core/` / `protocols/` / `settings/` / `config/` |
| **集成测试** | 模块间组合正确 | Session + Transport + Codec |
| **手动测试** | UI 交互 | `ui/` / `app/`（见附录 E） |

**UI 不做单测**。

### 9.3 目录

```
tests/
├── unit/         # 单测（镜像 src/ 结构）
├── integration/  # 集成
├── mocks/        # Mock 对象（header-only）
├── fixtures/     # 测试数据
└── helpers/      # 测试辅助
```

### 9.4 构建集成

```cmake
option(ICU_BUILD_TESTS "Build tests" ON)
option(ICU_BUILD_COVERAGE "Enable code coverage instrumentation" OFF)

if(ICU_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
```

`tests/CMakeLists.txt` 用 `file(GLOB_RECURSE CONFIGURE_DEPENDS)` 自动扫描 `unit/*.cpp` 和 `integration/*.cpp`，**新增测试文件无需改 CMakeLists**。

### 9.5 运行

```bash
# 构建 + 跑测试
cmake -S . -B build -G "MinGW Makefiles" \
      -DCMAKE_BUILD_TYPE=Debug -DICU_BUILD_TESTS=ON
cmake --build build -j16
ctest --test-dir build --output-on-failure

# 覆盖率（可选，会显著拖慢编译）
cmake -S . -B build -G "MinGW Makefiles" \
      -DCMAKE_BUILD_TYPE=Debug \
      -DICU_BUILD_TESTS=ON -DICU_BUILD_COVERAGE=ON
cmake --build build -j16
cmake --build build --target coverage
# 报告：build/coverage/index.html
```

### 9.6 覆盖要求

| 模块 | 覆盖率目标 | 当前（4.5 结束） |
|---|---|---|
| `src/core/` | ≥ 80% | ~85% ✅ |
| `src/protocols/` | ≥ 70% | ~40% ⚠️（TcpTransport 0%） |
| `src/settings/` | ≥ 60% | 0% ⚠️（阶段 8 补） |
| `src/config/` | ≥ 70% | 100% ✅ |
| `src/ui/` / `src/app/` | 手动测试 | — |
| **整体** | — | **50.0%** |

**已知缺口及处理计划**：

| 缺口 | 原因 | 补测时机 |
|---|---|---|
| `TcpTransport.cpp` 0% | 依赖真实 wxSocket 事件循环，单测成本高 | 阶段 5/6 引入串口/Modbus 时统一评估 |
| `settings/*.cpp` 0% | 依赖 `wxApp` 初始化 | 阶段 8 应用设置完善时补 |

### 9.7 什么必须测

- `FieldValue` 序列化往返
- `ActiveProfile` 的 `operator==` / `operator!=`
- `ConnectionProfile` 读写往返
- `Session` 状态机
- 每个 Codec 的半包 / 粘包 / 校验
- 每个 Protocol 的 `FieldsForMode()` 数量 + label 是翻译 key
- `ITransport` 的 `clientId` 语义
- `SessionManager` 的生命周期

### 9.8 测试命名约定

- 文件：`test_<类名>.cpp`
- 用例：`TEST_CASE("<类名>: <场景描述>")`

### 9.9 Mock 对象

放在 `tests/mocks/`（header-only）：

- `MockTransport` —— 内存假 Transport；`Open()` 只切到 `Opening`（对齐真实异步语义）；计数用 `std::shared_ptr<int>` 存储，跨对象生命周期可读
- `MockCodec` —— 直通 / 定长切帧两模式
- `MockProtocol` —— 简单协议（暂未使用）

### 9.10 覆盖率工具

- **gcovr 8.6+**（MSYS2 UCRT64：`pacman -S mingw-w64-ucrt-x86_64-python-gcovr`）
- filter 用 `.*/src/.*` 正则（绝对路径在 MinGW 下匹配失败）
- `--coverage` 插桩仅对 `icu_core_test` / `icu_tests`，**不影响主程序**
- **日常构建不开覆盖率**，`ICU_BUILD_COVERAGE=OFF` 是默认

---

## 10. 编码约定

### 10.1 命名空间

- 顶层：`icu`
- 子命名空间：`icu::core` / `icu::ui` / `icu::log` / `icu::settings`

### 10.2 日志规范

**所有日志都要翻译。**

| 场景 | 用法 |
|---|---|
| 无参数 | `SPDLOG_INFO(_("..."))` |
| 有参数 | `ICU_SPDLOG_INFO(_("... %s ..."), args)` |

**格式占位符**：
- `ICU_SPDLOG_*` **只认 `%s` / `%d` / `%llu`**
- **禁止**用 `{}`

**自检命令**：

```bash
grep -rn 'SPDLOG_\(INFO\|WARN\|ERROR\|DEBUG\)("'  src/ | grep -v '_('
grep -rn 'ICU_SPDLOG_.*".*{.*}'  src/
grep -rn 'SPDLOG_\(INFO\|WARN\|ERROR\|DEBUG\)(_.*,'  src/ | grep -v 'ICU_'
```

### 10.3 字符串

- 用户可见字符串**必须**用 `_()` 包裹
- 协议字段的 `label` 就是翻译 key
- `.po` 文件用 `msgmerge` 自动合并
- **`tests/` 下的字符串不翻译**（开发者向）

### 10.4 头文件

- 每个 `.h` 用 `#pragma once`
- **不要 include 自己**
- 前向声明放在使用它的命名空间内
- 头文件里用前向声明 + 指针/引用，`.cpp` 里才 include 完整定义
- **header-only 类里用 `_()` 必须 `#include <wx/translation.h>`**

### 10.5 生命周期

| 对象 | 注意事项 |
|---|---|
| `wxTaskBarIcon` | 不要 `delete`，只 `RemoveIcon()` |
| `wxSocketBase` | 销毁前先 `Notify(false)`，**没有** `RemoveEventHandler` |
| `wxEvtHandler` | 用 `Bind` 或 `SetEventHandler` |
| **UI 自毁场景** | **必须用 `CallAfter` 延迟**（见 2.5） |

### 10.6 线程安全

- 所有 UI 操作必须在主线程
- 从 socket / 定时器回调更新 UI 用 `CallAfter`
- 跨线程传递数据**必须复制**到堆上再 `CallAfter`

### 10.7 提交前自检

```bash
# 日志约定
grep -rn 'SPDLOG_\(INFO\|WARN\|ERROR\|DEBUG\)("'  src/ | grep -v '_('
grep -rn 'ICU_SPDLOG_.*".*{.*}'  src/

# 分层违规
grep -rn '#include.*ui/\|#include.*settings/' src/core/ src/protocols/

# 绕过 settings 直接调 wxConfigBase
grep -rn "wxConfigBase" src/ | grep -v src/settings/

# 循环包含
for f in $(find src -name "*.h"); do
    grep -q "#include \"$(basename $f)\"" $f && echo "SELF INCLUDE: $f"
done

# i18n
msgfmt --check-format -o /dev/null \
    resources/i18n/zh_CN/LC_MESSAGES/IndustrialCommunicationUtility.po
msgattrib --untranslated \
    resources/i18n/zh_CN/LC_MESSAGES/IndustrialCommunicationUtility.po
msgattrib --only-fuzzy \
    resources/i18n/zh_CN/LC_MESSAGES/IndustrialCommunicationUtility.po
```

---

## 11. CI 与代码质量

### 11.1 CI

GitHub Actions 每次 push / PR 跑四件事：**wxWidgets 构建缓存 → Debug 构建+测试 → Release 构建+测试 → clang-format 检查**。

**`.github/workflows/build.yml`**（实际结构）：

```yaml
jobs:
  wxwidgets:                    # 构建并缓存第三方 GUI 库
    runs-on: windows-latest
    steps:
      - uses: actions/checkout@v4
        with: { submodules: recursive }
      - uses: msys2/setup-msys2@v2     # UCRT64: gcc/cmake/ninja/gettext/make
      - uses: actions/cache@v4         # path: third_party/wxWidgets/lib
      - run: |                         # 仅缓存未命中时
          cd third_party/wxWidgets && mkdir -p build-ci && cd build-ci
          ../configure --enable-shared --enable-unicode --enable-debug \
                       --disable-tests --disable-monolithic --with-msw
          make -j$(nproc)

  build:                        # matrix: [Debug, Release]
    needs: wxwidgets
    steps:
      - uses: actions/cache/restore@v4  # fail-on-cache-miss: true
      - run: cmake -S . -B build-ci -G Ninja -DCMAKE_BUILD_TYPE=${{ matrix.build_type }} -DICU_BUILD_TESTS=ON
      - run: cmake --build build-ci -j$(nproc)
      - run: ctest --test-dir build-ci --output-on-failure

  format:                       # clang-format 19.1.5（pip 固定版本）
    steps:
      - run: pip install "clang-format==19.1.5"
      - run: clang-format --style=file --dry-run --Werror <src+r tests 下所有 .cpp/.h>
```

**几个必须知道的点**：

- `third_party/wxWidgets` 与 `third_party/spdlog` 都是 **submodule**，
  分别固定到 `469921b9`（wx 3.3.5 开发快照）与 `f544fce0`。
- **必须用 `submodules: recursive`**：wxWidgets 自身还带 12 个嵌套 submodule
  （zlib / png / expat / jpeg / tiff / pcre / scintilla / lexilla / libwebp /
  lunasvg / nanosvg / Catch），它的 `configure` + `make` 需要其中一部分。
- `third_party/wxWidgets/lib/`（编译产物）被 wxWidgets 自己的 `.gitignore` 忽略，
  **CI 上必须从源码构建 wxWidgets**，所以有了独立的 `wxwidgets` job 和缓存。
  本地开发同理，见 [docs/developer/building.md](docs/developer/building.md) 第 2.2 节。
- 缓存键用 `third_party/wxWidgets/include/wx/version.h` 的内容哈希推导，
  换了 wx 版本会自动失效。
- 格式检查用 **pip 装固定版本的 clang-format**，不用 MSYS2 的 clang
  （后者随滚动更新变化，会导致格式化结果漂移）。本地也要用 19.1.5，
  见 `scripts/format.sh`。
- 本地等价命令：`scripts/build.sh`、`scripts/test.sh`、`scripts/format.sh check`。

### 11.2 静态分析

**`.clang-tidy`**：

```yaml
Checks: >
  -*,
  bugprone-*,
  modernize-*,
  performance-*,
  readability-*,
  -modernize-use-trailing-return-type,
  -readability-magic-numbers
WarningsAsErrors: ''
HeaderFilterRegex: 'src/.*'
```

### 11.3 版本管理

**`cmake/version.h.in`**：

```cpp
#pragma once
#define ICU_VERSION_MAJOR @PROJECT_VERSION_MAJOR@
#define ICU_VERSION_MINOR @PROJECT_VERSION_MINOR@
#define ICU_VERSION_PATCH @PROJECT_VERSION_PATCH@
#define ICU_VERSION_STRING "@PROJECT_VERSION@"
#define ICU_BUILD_TYPE "@CMAKE_BUILD_TYPE@"
```

### 11.4 文档体系

```
docs/                                 ✅ 4.6 已建
├── architecture/
│   ├── overview.md                   分层结构、核心抽象
│   ├── layers.md                     依赖方向 + 自检命令
│   └── adr/                          架构决策记录
│       ├── README.md                 索引 + 何时写 ADR
│       ├── template.md
│       ├── 0001-protocol-as-first-class-with-modes.md
│       ├── 0002-ui-data-driven-by-field-descriptors.md
│       └── 0003-doctest-as-test-framework.md
├── user/
│   ├── getting-started.md
│   ├── tcp.md
│   └── troubleshooting.md
├── developer/
│   ├── building.md                   环境、wxWidgets 构建、常见问题
│   ├── adding-a-protocol.md
│   ├── adding-a-setting.md
│   └── testing.md
└── api/
    └── core.md                       逐行核对过头文件的接口文档
```

**约定**：架构级决策写进 `docs/architecture/adr/`（编号递增，不复用不删除）；
接口变更先改 `docs/api/`，再改代码。

### 11.5 `CHANGELOG.md`

遵循 [Keep a Changelog](https://keepachangelog.com/)。

---

## 12. 开发路线图

### 已完成

- [x] **阶段 1：协议抽象层**
- [x] **阶段 2：MainFrame 切换到协议层**
- [x] **阶段 3：会话面板通用化**
- [x] **阶段 4.1：抽出 `settings/` 模块**
- [x] **阶段 4.2：App 层切换到 Settings**
- [x] **阶段 4.3：目录重组**
- [x] **阶段 4.4：核心类型合并**
- [x] **阶段 4.5：测试模块**

### 阶段 4：目录结构与设置模块重构 🟡 **进行中（剩 4.7 清理）**

**4.4 核心类型合并** ✅

- [x] 4.4.1 建 `core/ProtocolTypes.h/.cpp`
- [x] 4.4.2 删 `core/FieldValue.h/.cpp` / `FieldDescriptor.h` / `ActiveProfile.h`
- [x] 4.4.3 全局替换 include
- [x] 4.4.4 编译 + 功能回归

**4.5 测试模块** ✅

- [x] 4.5.1 引入 doctest
- [x] 4.5.2 `tests/CMakeLists.txt` + `ICU_BUILD_TESTS`
- [x] 4.5.3 单测：`FieldValue` / `ActiveProfile` / `ProtocolRegistry`
- [x] 4.5.4 集成测试：`ConnectionProfile` roundtrip
- [x] 4.5.5 引入 `MockTransport` / `MockCodec`
- [x] 4.5.6 单测：`Session` 状态机 + `SessionManager` 生命周期
- [x] 4.5.7 单测：`RawCodec`
- [x] 4.5.8 覆盖率报告（gcovr 集成）

**4.6 工程化配套** ✅

- [x] 4.6.1 CI（`.github/workflows/build.yml`：wxWidgets 缓存 + Debug/Release 构建 + ctest + clang-format）
- [x] 4.6.2 `.clang-tidy`
- [x] 4.6.3 `cmake/version.h.in` + `project(... VERSION 0.1.0)`
- [x] 4.6.4 `CHANGELOG.md`
- [x] 4.6.5 `LICENSE`（MIT）
- [x] 4.6.6 `docs/`（architecture/adr + user + developer + api）
- [x] 4.6.7 `scripts/`（build/test/format/coverage/update_i18n）

**4.7 清理** 🟡 **当前阶段**

- [ ] 修正 `en_US.po` 未翻译条目
- [ ] 把 `DisplayName()` / `ModeNames()` 接到 `_()` 上（当前菜单与模式名不翻译）
- [ ] 清理 `tests/CMakeLists.txt` 里失效的 DLL 过滤代码（第 68~75 行会重复复制约 1.3 GB 的 DLL）
- [ ] 补 `tests/fixtures/` 或从文档中删除

**4.6 之后的补充（已完成）**

- [x] `third_party/wxWidgets` 改为**真正的 submodule**，固定到 `469921b9`
      （3.3.5 开发快照；上游无 `v3.3.5` tag，最新官方 tag 是 `v3.3.4`）
- [x] `third_party/spdlog` 补上 `.gitmodules` 映射（此前是 gitlink 但无映射，导致
      `git submodule status` 与 `actions/checkout` 都会失败），固定到 `f544fce0`

### 阶段 5：串口协议 ⬜ 未开始

### 阶段 6：Modbus RTU 协议 ⬜ 未开始

### 阶段 7：更多协议 ⬜ 未开始

### 阶段 8：应用设置完善 ⬜ 未开始

### 阶段 9：UI 打磨 ⬜ 未开始

### 阶段 10：发布准备 ⬜ 未开始

---

## 13. AI Skill 指南

### 13.1 上下文理解

**当前状态**：
- 阶段 1~3 完成
- 阶段 4.1~4.4 完成：`settings/` 就位，目录重组，核心类型合并到 `core/ProtocolTypes.h/.cpp`
- **4.5 全部完成**：
  - doctest 2.4.11 接入，`tests/` 就位（自动 GLOB 扫描，无需改 CMakeLists）
  - `MockTransport` / `MockCodec` 就位（`tests/mocks/`，header-only）
  - **91 个 test case / 300 assertions 全绿**（Debug 与 Release 均通过）
  - 覆盖率报告机制（gcovr + GCC `--coverage`，`scripts/coverage.sh`）
  - 最近一次测量：core/ ~85%、config/ 100%、protocols/ ~40%、settings/ 0%，整体 50.0%
- **4.6 全部完成**：
  - CI（wxWidgets 缓存 + Debug/Release 构建 + ctest + clang-format）
  - `.clang-tidy`、`cmake/version.h.in`（`project(VERSION 0.1.0)`）
  - `CHANGELOG.md`、`LICENSE`（MIT）、`CONTRIBUTING.md`
  - `docs/`：architecture（含 adr/）、user、developer、api
  - `scripts/`：build / test / format / coverage / update_i18n
  - 全量套用 `.clang-format`（clang-format 19.1.5）
- **4.7 清理** 为当前阶段；阶段 5~10 未开始

**工程命令**（仓库根）：

| 命令 | 作用 |
|---|---|
| `scripts/build.sh [debug\|release] [clean]` | 配置 + 编译 |
| `scripts/test.sh [debug\|release\|--list]` | ctest / 列出用例 |
| `scripts/format.sh [check]` | 格式化 / 只检查（CI 用 19.1.5） |
| `scripts/coverage.sh` | 生成 HTML 覆盖率报告 |
| `scripts/update_i18n.sh` | 提取 + 合并 + 编译翻译 |

**关键文件**：
- `src/core/IProtocol.h` — 协议接口
- `src/core/ITransport.h` / `src/core/ICodec.h` — 传输/编解码接口
- `src/protocols/tcp/TcpProtocol.cpp` — 协议范例
- `src/protocols/tcp/TcpTransport.cpp` — Transport 范例
- `src/ui/GenericProtocolDialog.cpp` — UI 数据驱动核心
- `src/core/ProtocolRegistry.cpp` — 协议注册
- `src/settings/Settings.h` — 设置入口
- `tests/CMakeLists.txt` — 测试构建入口（GLOB + 覆盖率开关）
- `tests/main.cpp` — doctest 实现 + main
- `tests/mocks/MockTransport.h` — 内存假 Transport 范例
- `tests/helpers/TestEnv.h` — 临时目录 RAII

### 13.2 回答用户问题的流程

| 用户意图 | AI 应做什么 |
|---|---|
| 询问设计/架构 | 引用本文档，不发明新架构 |
| 要求实现某协议 | 按 `TcpProtocol` 模板，只写 `protocols/<name>/` |
| 报告编译错误 | 定位根因，给最小修复 |
| 要求加 UI 功能 | 先问"能否通过 `FieldDescriptor` 描述" |
| 要求加 App 设置 | 走 `settings/` 模块 |
| 要求加测试 | 放 `tests/unit/` 或 `tests/integration/`，用 doctest；Mock 放 `tests/mocks/` |
| 讨论需求 | 更新本文档路线图 |

**检查清单**：
- [ ] 改动是否在正确的层？
- [ ] 是否引入硬编码？
- [ ] 日志是否符合约定？（`SPDLOG_*(_("..."))` 或 `ICU_SPDLOG_*(_("... %s"), x)`）
- [ ] 设置是否走 `settings/`？
- [ ] 是否需要同步加测试？
- [ ] header-only 类里用 `_()` 是否 include 了 `<wx/translation.h>`？

### 13.3 代码生成规范

**新增协议模板**：

```
src/protocols/<protocol_id>/
├── <Name>Protocol.h/.cpp
├── <Name>Config.h               （如需要）
├── <Name>Transport.h/.cpp       （如需要独立传输）
└── <Name>Codec.h/.cpp           （如需要独立编解码）

tests/unit/protocols/<protocol_id>/
├── test_<Name>Protocol.cpp
└── test_<Name>Codec.cpp         （如有独立 Codec）
```

**AI 严禁**：
- 为客户端 / 服务器分别创建协议类
- 在 `MainFrame` 里加 `OnNewXxxClient` / `OnNewXxxServer`
- 在 `GenericProtocolDialog` 里加 `if (protocol == "tcp")`
- 硬编码菜单项
- 绕过 `settings/` 直接调 `wxConfigBase`
- 在 `core/` 或 `protocols/` 里 include `ui/` 或 `settings/`

### 13.4 修复编译错误的准则

1. **先看第一条错误**
2. **不要批量改**
3. **不要顺手重构**
4. **不要发明 API**
5. **循环包含：`.h` 里不要 include 自己**
6. **CMake target 引用必须在 `add_library` / `add_executable` 之后**

### 13.5 日志和 i18n 准则

| 场景 | 正确 | 错误 |
|---|---|---|
| 无参数 | `SPDLOG_INFO(_("..."))` | `SPDLOG_INFO("...")` |
| 有参数 | `ICU_SPDLOG_INFO(_("... %s ..."), x)` | `ICU_SPDLOG_INFO(_("... {} ..."), x)` |
| 测试代码 | `SPDLOG_WARN("...")` （无 `_()`） | `SPDLOG_WARN(_("..."))` |
| header-only | `std::string(_("...").utf8_str())` + `#include <wx/translation.h>` | 忘 include |

### 13.6 完成任务后的输出

1. 变更清单
2. 验收步骤
3. 路线图更新
4. 遗留问题

### 13.7 禁止事项

| 禁止 | 理由 |
|---|---|
| 拆分 TCP 为 TCPClientProtocol / TCPServerProtocol | 协议是一个整体 |
| 在 UI 层加 `if (protocol == "xxx")` | 破坏数据驱动 |
| 直接 `delete m_trayIcon` | 触发断言 |
| `ICU_SPDLOG_*` 里用 `{}` | 格式不支持 |
| 头文件 include 自己 | 循环包含 |
| 绕过 `settings/` 直接调 `wxConfigBase` | 破坏单一数据源 |
| `core/` 或 `protocols/` 里 include `ui/` / `settings/` | 破坏分层 |
| **在按钮回调里同步销毁宿主面板** | **闪退**（见 2.5） |
| 新增协议不加测试 | 违背 9.7 |
| 在 `add_executable` 之前 `target_*_options` | CMake 报"target not built" |
| 在 `mgr.Close()` / `SetActive(new)` 之后访问旧对象裸指针 | use-after-free |
| `--coverage` 默认开启 | 显著拖慢日常开发 |

---

## 14. 关键决策记录

| 决策 | 理由 |
|---|---|
| **协议是一等公民** | 避免 Client / Server 被拆成两个"协议" |
| **一个 `.conn` 文件一份配置** | 切换模式不需保存两份 |
| **UI 数据驱动** | 新协议不改 UI |
| **`FieldDescriptor` 的 label 作为翻译 key** | 不引入额外 i18n 机制 |
| **`ITransport` 的 `clientId` 参数** | 统一客户端和服务器 |
| **Transport 层保留 Client / Server 两个实现** | wxWidgets API 差异大 |
| **UI 层不区分 Client / Server** | 通过 `IsServerMode()` 适配 |
| **日志全翻译** | 用户可见 |
| **测试代码不翻译** | 开发者向 |
| **`wxTaskBarIcon` 不直接 delete** | 内部窗口销毁时机问题 |
| **托盘退出用 `CallAfter` 包裹 `Close`** | 避免在托盘事件栈上销毁托盘 |
| **UI 自毁场景用 `CallAfter` 延迟** | 避免访问已释放内存 |
| **`app.rc` 引用 `wx.rc`** | 一次性解决 Manifest、光标、图标 |
| **`config/` 用 INI（wxFileConfig）** | 跨平台、易读 |
| **`settings/` 与 `config/` 分离** | App 级偏好 vs 协议级配置 |
| **`Settings` 是唯一 `wxConfigBase` 消费者** | 单一数据源 |
| **`settings/` 预留字体/主题/图标空壳** | 提前定型接口 |
| **用 doctest 做测试框架** | header-only，零配置，编译快 |
| **`tests/main.cpp` 提供 doctest 实现** | header-only 必须在一个 TU 展开 |
| **测试与 src 镜像** | 找测试文件路径直觉化 |
| **Mock 对象放 `tests/mocks/`（header-only）** | 不污染 `core/`，可被多测试复用 |
| **`tests/CMakeLists.txt` 用 GLOB + `CONFIGURE_DEPENDS`** | 新增测试无需改 CMakeLists |
| **`icu_core_test` 显式列源文件（不用 GLOB）** | 精确控制，避免误编 |
| **UI 不做单测** | 投入产出比低 |
| **目录重组分批次** | 每批可回滚 |
| **include 路径用 `src/` 前缀** | `target_include_directories` 已含 `src` |
| **主程序用 `file(GLOB_RECURSE CONFIGURE_DEPENDS)`** | 新增文件自动被拾取 |
| **"打开连接" 直接重连不弹框** | 用户已配置好，直接复用 |
| **"配置连接" 只在未连接状态可用** | 避免连上后改配置造成混乱 |
| **测试用 LLDB-DAP 调试** | GDB 17.2 与 cpptools MI 握手失败 |
| **覆盖率用 gcovr，`ICU_BUILD_COVERAGE=OFF` 默认** | 插桩拖慢编译，日常不开 |
| **gcovr filter 用 `.*/src/.*`** | MinGW 下绝对路径匹配失败 |
| **`MockTransport::Open()` 只切到 `Opening`** | 对齐真实异步 Transport 语义 |
| **Mock 计数用 `shared_ptr<int>` 存储** | 跨对象生命周期可读，避免 use-after-free |

---

## 15. 变更日志

| 日期 | 变更 |
|---|---|
| 2026-10-12 | 初版：确立协议层抽象 |
| 2026-10-12 | 阶段 1~3 完成 |
| 2026-10-12 | 确立目标目录结构；新增第 6 节设置模块 |
| 2026-10-12 | 新增 `FontSettings` / `ThemeSettings` / `IconSettings` 空壳 |
| 2026-10-12 | 新增第 9 节测试策略；新增第 11 节 CI |
| 2026-10-13 | 4.1~4.3 完成：settings 模块、App 层切换、目录重组 |
| 2026-10-13 | 新增附录 E：标准测试流程 |
| 2026-10-13 | 新增"UI 生命周期安全"原则（2.5 节）；"打开连接"改为直接重连；"配置连接"限制在未连接状态；协议面板加模式标签 |
| 2026-10-13 | 4.4 完成：核心类型合并到 `core/ProtocolTypes.h/.cpp` |
| 2026-10-13 | 4.5.1~4.5.3 完成：doctest 接入、`tests/` 就位、3 个 core 单测全绿 |
| 2026-10-13 | 调试器从 GDB 切换到 LLDB-DAP（GDB 17.2 与 cpptools MI 握手失败） |
| 2026-10-13 | 4.5.4 完成：`ConnectionProfile` roundtrip 集成测试 |
| 2026-10-13 | 4.5.5 完成：`MockTransport` / `MockCodec` 就位（header-only） |
| 2026-10-13 | 4.5.6 完成：`Session` 状态机 + `SessionManager` 生命周期单测；修正 `MockTransport::Open` 语义（只到 `Opening`）；Mock 计数改为 `shared_ptr<int>` |
| 2026-10-13 | 4.5.7 完成：`RawCodec` 单测（ModbusRtuCodec 待阶段 6） |
| 2026-10-13 | 4.5.8 完成：gcovr 集成、`ICU_BUILD_COVERAGE` 开关；覆盖率 core/ ~85%、config/ 100%、protocols/ ~40%、settings/ 0%，整体 50.0% |
| 2026-10-13 | 全项目日志翻译补齐：所有用户可见字符串 `_()`，无参数 `SPDLOG_*`、有参数 `ICU_SPDLOG_*`；header-only Codec 加 `#include <wx/translation.h>` |
| 2026-10-13 | `zh_CN.po` 补全所有空翻译 + 消除 fuzzy |
| 2026-10-14 | 4.6 全部完成：CI（wxWidgets 缓存 + Debug/Release + ctest + clang-format 19.1.5）、`.clang-tidy`、`cmake/version.h.in` + `project(VERSION 0.1.0)`、`CHANGELOG.md`、`LICENSE`（MIT）、`CONTRIBUTING.md`、`docs/`（architecture/adr + user + developer + api）、`scripts/`（build/test/format/coverage/update_i18n）、`.gitattributes` |
| 2026-10-14 | 全量套用 `.clang-format`（59 个源文件）；`wxBEGIN_EVENT_TABLE` 区域加 `// clang-format off/on` 保护（clang-format 会把宏后的缩进逐级累积） |
| 2026-10-14 | 修复 `resources/i18n/update_i18n.sh`：原来只扫描 `src/*.cpp src/*/*.cpp`，漏掉 `src/core/` 等更深层级，改为 `find src` 递归并带上 `.h` |
| 2026-10-14 | `tests/helpers/TestEnv.h` 支持无效 `TEMP`/`TMP`：`temp_directory_path()` 失败时逐个回退，修复受限环境下 6 个集成测试失败 |
| 2026-10-14 | 修正本文档与代码的漂移：测试用例数 92 → 91、CI 描述、目录结构状态、构建命令改为 `scripts/` |

---

## 附录 A：快速命令参考

### A.1 构建

推荐直接用脚本（仓库根执行）：

```bash
scripts/build.sh              # Debug + 测试
scripts/build.sh release      # Release（build-release/）
scripts/build.sh debug clean  # 先清理再构建
```

等价的手工命令：

```bash
# 在 MSYS2 UCRT64 shell 里
cmake -S . -B build -G "MinGW Makefiles" \
      -DCMAKE_BUILD_TYPE=Debug \
      -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
      -DICU_BUILD_TESTS=ON
cmake --build build -j16
```

> MSYS2 UCRT64 里 `cmake` 默认不在 PATH，用绝对路径
> `"/c/Program Files/CMake/bin/cmake.exe"` 或 `pacman -S mingw-w64-ucrt-x86_64-cmake`。
>
> 首次构建前必须先编译 wxWidgets，见 [docs/developer/building.md](docs/developer/building.md) 第 2.2 节。

### A.2 测试

```bash
scripts/test.sh               # ctest（Debug）
scripts/test.sh release       # ctest（Release）
scripts/test.sh --list        # 列出所有用例

# 等价手工命令
ctest --test-dir build --output-on-failure
# 或直接跑（可用 doctest 过滤参数）
build/tests/icu_tests.exe --test-case="Session:*"
```

### A.3 覆盖率

```bash
scripts/coverage.sh
# 报告：build-coverage/coverage/index.html
```

等价手工命令：

```bash
cmake -S . -B build-coverage -G "MinGW Makefiles" \
      -DCMAKE_BUILD_TYPE=Debug \
      -DICU_BUILD_TESTS=ON -DICU_BUILD_COVERAGE=ON
cmake --build build-coverage -j16
cmake --build build-coverage --target coverage
```

### A.4 i18n

```bash
scripts/update_i18n.sh
msgattrib --untranslated resources/i18n/zh_CN/LC_MESSAGES/*.po
msgattrib --only-fuzzy     resources/i18n/zh_CN/LC_MESSAGES/*.po
```

### A.5 格式化与静态分析

```bash
scripts/format.sh             # 就地格式化 src/ 与 tests/
scripts/format.sh check       # 只检查（CI 用），需要 clang-format 19.1.5

# 静态分析（需要 clang-tidy + compile_commands.json）
clang-tidy -p build src/core/Session.cpp
```

### A.6 自检

```bash
# 日志约定
grep -rn 'SPDLOG_\(INFO\|WARN\|ERROR\|DEBUG\)("'  src/ | grep -v '_('
grep -rn 'ICU_SPDLOG_.*".*{.*}'  src/

# 分层违规
grep -rn '#include.*ui/\|#include.*settings/' src/core/ src/protocols/

# 绕过 settings 直接调 wxConfigBase
grep -rn "wxConfigBase" src/ | grep -v src/settings/

# 循环包含
for f in $(find src -name "*.h"); do
    grep -q "#include \"$(basename $f)\"" $f && echo "SELF INCLUDE: $f"
done
```

### A.7 残留进程清理

```powershell
Get-Process IndustrialCommunicationUtility -ErrorAction SilentlyContinue | Stop-Process -Force
Get-Process icu_tests -ErrorAction SilentlyContinue | Stop-Process -Force
```

---

## 附录 B：协议实现检查清单

- [ ] 协议类实现 `IProtocol` 的所有纯虚函数
- [ ] `Id()` 唯一
- [ ] `DisplayName()` 是翻译 key
- [ ] `FieldsForMode()` 返回的 label 都是翻译 key
- [ ] `CreateSession()` 不依赖具体 UI
- [ ] `MakeDefaultFileName()` 生成的名称可跨平台
- [ ] 已注册到 `ProtocolRegistry`
- [ ] 新增的 label 已在 `.po` 里翻译
- [ ] 已在本 README 第 8 节协议清单表格里更新状态
- [ ] 已加 `tests/unit/protocols/<id>/` 单测
- [ ] 未修改任何非 `protocols/<id>/` 目录下的生产代码

---

## 附录 C：设置项实现检查清单

- [ ] Key 定义在 `SettingsKeys.h`
- [ ] 类型化接口加在对应的 `XxxSettings.h`
- [ ] 实现走 `Settings::Instance()`
- [ ] 在 `SettingsDialog` 里加 UI
- [ ] 在使用点调用
- [ ] 未在 `core/` 或 `protocols/` 里直接访问 settings
- [ ] 已在本 README 第 6.5 节设置项清单里更新状态

---

## 附录 D：测试编写检查清单

- [ ] 文件命名：`test_<类名>.cpp`
- [ ] 放在 `tests/unit/<模块>/` 或 `tests/integration/`
- [ ] 用例命名：`TEST_CASE("<类名>: <场景>")`
- [ ] 每个 `TEST_CASE` 独立
- [ ] 临时文件用 `TestEnv` 辅助
- [ ] 测试结束后清理临时资源
- [ ] Mock 放在 `tests/mocks/`（header-only）
- [ ] 若需在对象销毁后观察计数，用 `shared_ptr<int>` 而非裸指针
- [ ] 不要在 `Close` / `SetActive(new)` 之后访问旧对象裸指针
- [ ] 测试代码不翻译（不用 `_()`）
- [ ] 无需手动加到 `tests/CMakeLists.txt`（GLOB 自动拾取）
- [ ] `ctest` 全绿

---

## 附录 E：标准测试流程

### E.1 测试环境准备

```bash
pacman -S netcat            # MSYS2 UCRT64
nc -h                       # 验证
```

### E.2 通用启动检查

| 序 | 操作 | 期望 |
|---|---|---|
| 1 | 启动程序 | 主窗口出现，标题正确 |
| 2 | 查看标题栏 | 未加载配置时无 `*` |
| 3 | 菜单栏 | `文件 / 视图 / 设置 / 帮助` 完整 |

### E.3 TCP 客户端测试（nc 做服务器）

**准备**：另开终端 `nc -l -p 5020`。

| 序 | 操作 | 期望 |
|---|---|---|
| 1 | `文件 → 新建连接 → TCP/IP` | 弹配置对话框 |
| 2 | Client 模式，`127.0.0.1:5020`，OK | 状态"连接中…"→"已连接" |
| 3 | 程序发 `hello` | nc 显示 `hello` |
| 4 | nc 输入 `world` | 程序数据日志 `< world` |
| 5 | 点"断开连接" | 状态"已断开" |
| 6 | 点"打开连接" | **直接重连**（不弹框）→"已连接" |
| 7 | `Ctrl+C` 关闭 nc | 状态"已断开" |
| 8 | 重新 `nc -l -p 5020` | 若开自动重连，5 秒内连上 |

**边界测试**：

| 场景 | 操作 | 期望 |
|---|---|---|
| 连接失败 | 连 `127.0.0.1:9999` | 超时后"已断开" |
| 中途断线 | 连接中 `Ctrl+C` 关 nc | "已断开" |

### E.4 TCP 服务器测试（nc 做客户端）

**准备**：程序先启动服务器。

| 序 | 操作 | 期望 |
|---|---|---|
| 1 | `文件 → 新建连接 → TCP/IP` | 弹配置对话框 |
| 2 | Server 模式，`0.0.0.0:5030`，OK | 状态栏"正在监听"，状态"监听中" |
| 3 | 查看"统计"区 | `Clients: 0` |
| 4 | `nc 127.0.0.1 5030` | 程序显示 `Client #1 connected`，Clients=1 |
| 5 | 第二个 `nc` | `Client #2 connected`，Clients=2 |
| 6 | 客户端 1 发 `hello from #1` | 程序显示 `<#1 hello from #1` |
| 7 | 客户端 2 发 `hello from #2` | 程序显示 `<#2 hello from #2` |
| 8 | 程序发 `broadcast` | 两个 nc 都显示 `broadcast` |
| 9 | 关闭客户端 1 | `Client #1 disconnected`，Clients=1 |
| 10 | 关闭客户端 2 | Clients=0，状态"监听中" |
| 11 | 点"停止监听" | 状态"已断开" |

**多客户端并发测试**：

```bash
for i in 1 2 3; do
    (echo "hello from #$i"; sleep 5) | nc 127.0.0.1 5030 &
done
wait
```

### E.5 配置文件测试

| 序 | 操作 | 期望 |
|---|---|---|
| 1 | Client 模式 → 保存为 `TCP_Client_test.conn` | 标题显示 `- TCP_Client_test`，无 `*` |
| 2 | 查看文件内容 | 含 `Protocol=tcp` + `Mode=Client` |
| 3 | 点"配置连接..."改端口 | 标题出现 `*` |
| 4 | `文件 → 保存连接` | `*` 消失 |
| 5 | 关闭连接 | 无提示 |
| 6 | `文件 → 打开连接` 选该文件 | 恢复 |

### E.6 最近连接测试

| 序 | 操作 | 期望 |
|---|---|---|
| 1 | 保存 3 个 `.conn` | 最近连接列出 3 项 |
| 2 | 点击其中一项 | 加载并连接 |
| 3 | 重启 | 仍在 |
| 4 | 手动删除一个 → 重启 | 该项消失 |
| 5 | 添加第 6 个 | 只保留 5 个 |

### E.7 窗口与 UI 测试

| 序 | 操作 | 期望 |
|---|---|---|
| 1 | 拖窗口到右下角 | — |
| 2 | 关闭 → 重启 | 恢复位置 |
| 3 | 最大化 → 关闭 → 重启 | 保持最大化 |
| 4 | `视图 → 显示日志` | 日志窗口弹出 |
| 5 | 关闭日志窗口 | 隐藏，主窗口正常 |
| 6 | 再次打开日志 | 历史保留 |

### E.8 语言切换测试

| 序 | 操作 | 期望 |
|---|---|---|
| 1 | `设置 → 语言 → English` | 弹框询问重启 |
| 2 | 点"是" | 全英文界面 |
| 3 | `Settings → Language → 简体中文` | 中文界面 |

### E.9 关闭与托盘测试

| 序 | 操作 | 期望 |
|---|---|---|
| 1 | 有未保存配置时点 X | 弹框"是否保存？" |
| 2 | 点"取消" | 不关闭 |
| 3 | 点"是" | 保存后关闭 |
| 4 | 点"否" | 直接关闭 |
| 5 | 无未保存配置时点 X | 弹框"是否最小化到托盘？" |
| 6 | 点"是" | 窗口隐藏，托盘出现 |
| 7 | 双击托盘图标 | 窗口恢复 |
| 8 | 托盘右键 → 退出 | **无断言**，干净退出 |
| 9 | `Get-Process IndustrialCommunicationUtility` | 无输出 |

### E.10 异常场景

| 场景 | 操作 | 期望 |
|---|---|---|
| 打开损坏的 `.conn` | 手动改 `Type=XYZ` | 提示"加载失败" |
| 保存到只读路径 | `C:\Windows\System32\test.conn` | 提示"保存失败" |
| 配置不完整 | 手动删除 `RemoteHost` | 加载后使用默认值 |

### E.11 测试结果记录

```markdown
## 测试日期：2026-XX-XX
## 版本：v0.1.0
## 测试人：<姓名>

| 章节 | 结果 | 备注 |
|---|---|---|
| E.2 启动 | ✅ | — |
| E.3 TCP Client | ✅ | — |
| E.4 TCP Server | ✅ | — |
| E.5 配置文件 | ✅ | — |
| E.6 最近连接 | ✅ | — |
| E.7 窗口与 UI | ✅ | — |
| E.8 语言切换 | ✅ | — |
| E.9 关闭与托盘 | ✅ | — |
| E.10 异常场景 | ✅ | — |

**结论**：全部通过。
```

---

**文档结束。**

> 本文档随项目演进持续更新。任何架构级改动必须先更新本文档，再动代码。
