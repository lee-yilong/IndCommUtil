# IndComm — 开发指南与 AI Skill

> **文档定位**：本文档既是项目开发文档，也是 AI 辅助开发的 Skill 定义。所有开发按本文档推进，AI 在协助编码时必须遵循本文档的架构、约定和任务清单。
>
> **本版重点**：阶段 12（MQTT 协议接入）已完成。**当前准备进入阶段 13（应用设置完善与发布准备）。**

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

**IndComm**（Industrial Communication Utility）是一个面向工业现场的多协议通讯测试/调试及自动化测试平台。目标整合 NetAssist、Modbus Poll、MQTT Explorer 等工具的调试能力，并提供基于 `.json` 测试流程的无头自动化执行，以及基于 SQLite 的数据可视化回放。

### 1.2 核心目标

| 目标 | 含义 |
|---|---|
| **协议可插拔** | 新增协议 = 新增一个类，不改 UI、不改菜单、不改对话框 |
| **UI 数据驱动** | 配置对话框、会话面板、菜单项全部从协议描述自动生成 |
| **统一底层会话** | 字节流、事务型和发布订阅型协议统一抽象为 `ISession`，UI 层不感知底层差异 |
| **状态与重连标准化** | 提供 `ReconnectConfig` 与 `SessionStateMachine`，所有协议开箱即用 |
| **自动化测试** | 提供通用的 `TestRunner`，支持通信变量提取、断言校验和批量生成 |
| **数据持久化与图表** | 内置 SQLite 记录运行数据，提供实时多变量折线图与历史回放 |
| **无头 CLI 支持** | 独立 `indcomm-cli.exe`，方便接入 CI/CD 流水线与自动化测试 |
| **分层解耦** | 应用层 / 框架层 / 协议层 / UI 层 / 服务层，依赖单向 |
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
| JSON | nlohmann/json（submodule） |
| 数据库 | SQLite3（submodule） |
| i18n | GNU gettext（.po / .mo） |
| 网络/串口 | wxSocket（内置） / 自实现（`SerialPort`） |
| Modbus | serhmarch/ModbusLib（submodule） |
| MQTT | Eclipse Paho MQTT C++（submodule） |
| 图表 | wxGraphicsContext 自绘 |
| CI | GitHub Actions |
| 覆盖率 | gcovr + GCC `--coverage` |

---

## 2. 核心设计原则

### 2.1 协议是一等公民

**协议是一个整体，不是「客户端」和「服务器」两个东西。**
「客户端 / 服务器」是 TCP 或 Modbus 协议**内部的一种模式（Mode）**。一个 `.json` 文件 = 一份协议配置（含 Mode 与测试步骤），**不是两份**。

### 2.2 UI 完全数据驱动且统一

- 配置对话框：通过 `FieldDescriptor` 列表自动生成 `GenericProtocolDialog`。
- 变量参数表单：通过 `IProtocol::VariableParamsFields()` 描述，自动生成 UI 控件。
- 会话面板：使用 `wxSplitterWindow` 左右分栏，左侧统一展示变量池，右侧根据协议类型自动分派操作区。
- **新增协议绝不需要改动任何 `ui/` 下的代码**。

### 2.3 状态机与重连标准化

- **底层差异收敛**：所有会话继承 `ISession`。字节流继承 `StreamSession`，事务型继承 `ITransactionSession`，发布订阅型继承 `ISession`（通过 Paho 内置重连）。
- **状态与重连抽象**：任何协议都组合 `SessionStateMachine` 管理生命周期状态，组合 `ReconnectConfig` 获得统一的重连配置与 UI 字段。

### 2.4 自动化测试引擎通用化

- **动作统一**：无论 TCP、串口、Modbus 还是 MQTT，测试步骤统一抽象为 `read`（读取/接收）、`write`（写入/发送）和 `delay`（延时）。
- 所有的测试步骤抽象为 `ITestStep`。由具体协议通过 `IProtocol::CreateTestStep` 工厂生成。
- **批量生成**：支持在 UI 上选定一个基准步骤，通过设置“重复次数”、“间隔”和“自增/自减”快速生成一系列测试步骤。
- `TestRunner` 纯粹负责流程调度，支持自动全跑与交互单步阻断。

### 2.5 分层清晰，依赖单向

- `app/` -> `ui/` -> `protocols/` -> `core/`。
- `core/` 只依赖 C++ STL 和 wxBase（不包含任何 GUI 元素）。
- **核心逻辑抽离为 `icu_core_lib` 静态库，同时服务 GUI (`IndComm.exe`) 与 CLI (`indcomm-cli.exe`)。**

### 2.6 单一数据源

- **协议配置**：唯一来源是 `.json` 文件（`config/` 管）
- **应用设置**：唯一来源是 `settings/` 模块
- **协议描述**：唯一来源是 `IProtocol::FieldsForMode()` 与 `VariableParamsFields()`
- **历史数据**：唯一来源是 SQLite（`test_history.db`）

### 2.7 UI 生命周期安全

**任何"用户操作触发 UI 自身销毁"的场景，都必须用 `CallAfter` 延迟。**
**任何"会被异步回调引用"的对象，销毁前必须先清空它持有的所有回调。**

---

## 3. 架构设计

### 3.1 底层会话架构

所有协议最终都通过 `SessionManager` 管理唯一的 `ISessionPtr`。

| 协议类型 | 会话实现 | 特点 |
|---|---|---|
| 字节流 (TCP, Serial) | `StreamSession` | 组合 `ITransport` 和 `ICodec`，底层做半包/粘包处理。 |
| 事务型 (Modbus) | `ModbusSession` | 组合 `ModbusLib`，后台起心跳线程管理连接状态与重连。 |
| 发布订阅型 (MQTT) | `MqttSession` | 封装 `paho.mqtt.cpp`，处理主题订阅与消息分发，内置重连。 |

### 3.2 统一的 UI 生成逻辑

`SessionPanel` 采用 `wxSplitterWindow` 左右分栏。左侧为全局变量池，右侧使用 `wxNotebook` 提供三种模式（手动、交互、自动、图表）。在手动模式下，根据 `ISession` 动态生成操作区。

### 3.3 自动化测试执行流

```
UI/CLI 加载 test_flow (JSON)
    -> 实例化 TestRunner (包含多个 ITestStep)
    -> 在后台线程按顺序运行：
        -> Execute() 从 variables 中读取 params，发送指令
        -> 等待响应
        -> Assert() 进行比对
        -> Extract() 提取变量存入全局池
        -> 触发 RecordData() 写入 SQLite 并推送到实时图表
    -> 抛出回调更新 UI 或 CLI 终端
```

### 3.4 会话销毁顺序

```
SessionManager::SetActive(new) 或 Close()
           │
           ▼
DetachAndClose()：
   1. session->DetachAllCallbacks()          ← 先清所有回调
           │
           ▼
   2. session->Close()                       ← transport 触发 NotifyState(Closed)
           │
           ▼
   3. m_active.reset()                       ← 此时才能真正销毁
```

---

## 4. 关键接口

### 4.1 `ISession`（统一基类）
```cpp
class ISession {
public:
    virtual bool Open() = 0;
    virtual void Close() = 0;
    virtual SessionState State() const = 0;
    virtual bool IsConnected() const = 0;
    virtual std::string Target() const = 0;
    virtual std::string TransportName() const = 0;
    virtual bool IsServerMode() const { return false; }

    using StateCallback = std::function<void(SessionState)>;
    virtual void OnStateChanged(StateCallback cb) = 0;
    virtual void DetachAllCallbacks() = 0;
};
```

### 4.2 `ITransactionSession`（事务型扩展）
```cpp
class ITransactionSession : public ISession {
public:
    virtual TransactionResponse Execute(const TransactionRequest& request) = 0;
    virtual TransactionResponse Execute(const std::map<std::string, FieldValue>& fields) = 0;

    using LogCallback = std::function<void(const std::string&)>;
    virtual void OnLog(LogCallback cb) = 0;
};
```

### 4.3 `ITestStep`（自动化测试步骤）
```cpp
class ITestStep {
public:
    virtual ~ITestStep() = default;
    virtual std::string Name() const = 0;
    virtual TestResult Execute(ISession* session, std::map<std::string, CommVariable>& vars) = 0;
    virtual bool Assert(const TestResult& result, std::string& outErrorMsg) = 0;
    virtual std::vector<std::string> Extract(const TestResult& result, std::map<std::string, CommVariable>& vars) = 0;
};
```

### 4.4 `IProtocol`（协议接口）
```cpp
class IProtocol {
public:
    virtual std::string Id() const = 0;
    virtual std::string DisplayName() const = 0;
    virtual bool HasModes() const = 0;
    virtual std::vector<std::string> ModeNames() const = 0;
    virtual std::vector<FieldDescriptor> FieldsForMode(int modeIndex) const = 0;

    // 返回该协议通讯变量的专属参数描述（驱动 UI 自动生成操作表单）
    virtual std::vector<FieldDescriptor> VariableParamsFields() const { return {}; }

    virtual ISessionPtr CreateSession(int modeIndex, const std::map<std::string, FieldValue>& fields) = 0;
    virtual std::vector<FieldDescriptor> OperationFieldsForMode(int modeIndex) const { return {}; }
    virtual ITestStepPtr CreateTestStep(const std::string& stepType, const nlohmann::json& params) = 0;
    virtual std::string MakeDefaultFileName(int modeIndex, const std::map<std::string, FieldValue>& fields) const = 0;
};
```

### 4.5 `FieldDescriptor`（扩展分组）
```cpp
struct FieldDescriptor {
    std::string key;
    std::string label;
    FieldValue::Type type = FieldValue::Type::String;
    FieldValue defaultValue;
    int minValue = 0;
    int maxValue = 100;
    std::vector<std::pair<std::string, std::string>> choices;
    std::string group;
};
```

### 4.6 `SessionManager`
```cpp
class SessionManager {
public:
    void SetActive(ISessionPtr s);
    void Close();
    bool IsConnected() const;
    ISession* Active() const;
    SessionState ActiveState() const;
private:
    void DetachAndClose();
    ISessionPtr m_active;
};
```

---

## 5. 目录结构

```text
IndComm/
├── src/
│   ├── main.cpp
│   ├── app/
│   ├── cli/
│   │   └── main.cpp
│   ├── core/
│   │   ├── ICodec.h
│   │   ├── ITransport.h
│   │   ├── IProtocol.h
│   │   ├── ISession.h
│   │   ├── ITransactionSession.h
│   │   ├── ReconnectConfig.h
│   │   ├── SessionStateMachine.h
│   │   ├── ITestStep.h
│   │   ├── TestRunner.h/.cpp
│   │   ├── ProtocolRegistry.h/.cpp
│   │   ├── ProtocolTypes.h/.cpp
│   │   ├── StreamSession.h/.cpp
│   │   ├── SessionManager.h/.cpp
│   │   ├── Database.h/.cpp
│   │   ├── BytesParser.h
│   │   └── DelayStep.h
│   ├── protocols/
│   │   ├── tcp/
│   │   ├── serial/
│   │   ├── modbus/
│   │   ├── mqtt/                        ← 新增
│   │   │   ├── MqttProtocol.h/.cpp
│   │   │   ├── MqttSession.h/.cpp
│   │   │   ├── MqttPublishStep.h/.cpp
│   │   │   └── MqttSubscribeStep.h/.cpp
│   │   └── codec/
│   ├── ui/
│   │   ├── GenericProtocolDialog.h/.cpp
│   │   ├── SessionPanel.h/.cpp
│   │   ├── TestStepEditDialog.h/.cpp
│   │   ├── VariableEditDialog.h/.cpp
│   │   ├── HistoryDialog.h/.cpp
│   │   ├── ChartPanel.h/.cpp
│   │   ├── LogFrame.h/.cpp
│   │   ├── LogSink.h/.cpp
│   │   └── AppTaskBarIcon.h/.cpp
│   ├── settings/
│   ├── config/
│   └── log/
├── tests/
├── resources/
├── third_party/
│   ├── wxWidgets/
│   ├── spdlog/
│   ├── json/
│   ├── sqlite/
│   ├── ModbusLib/
│   ├── paho.mqtt.c/                     ← 新增
│   └── paho.mqtt.cpp/                   ← 新增
└── CMakeLists.txt
```

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
| 内容 | 应用级偏好（语言、窗口、字体、主题） | 协议配置（`.json` 文件） |
| 存储 | JSON 文件（`settings.json`） | 独立 `.json` 文件 |
| 用户感知 | 隐式 | 显式 |
| 生命周期 | 应用级 | 会话级 |

### 6.3 模块结构

```
settings/
├── SettingsKeys.h
├── Settings.h/.cpp
├── LanguageSettings.h/.cpp
├── UISettings.h/.cpp
├── FontSettings.h/.cpp
├── ThemeSettings.h/.cpp
├── IconSettings.h/.cpp
└── README.md
```

### 6.4 关键约定

- **`Settings` 是唯一允许直接操作底层存储的类**
- 存储位置按优先级探测：`%APPDATA%` → `%LOCALAPPDATA%` → `%USERPROFILE%`。
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
| **UI** | 十六进制显示 | 🟡 会话面板内存（阶段 13 持久化） |
| **UI** | 自动滚动 | 🟡 会话面板内存（阶段 13 持久化） |
| **UI** | 时间戳显示 | 🟡 会话面板内存（阶段 13 持久化） |
| **图标** | 协议图标 | ⬜ 预留 |
| **图标** | UI 图标 | ⬜ 预留 |
| **最近连接** | 最近打开的 `.json` 列表 | ✅ 已实现 |

**图例**：✅ 已实现 / 🟡 部分实现或待迁移 / ⬜ 预留

### 6.6 怎么加新设置

1. **加 key** —— `SettingsKeys.h`
2. **加接口** —— `UISettings.h`
3. **加实现** —— `UISettings.cpp`
4. **加 UI** —— `ui/SettingsDialog`（阶段 13 待建）
5. **加使用** —— 使用点调用

---

## 7. 协议配置文件格式

### 7.1 `.json` 通用结构

配置文件顶层分为 `connection` 和 `session`。`session` 中包含**通信变量**与**各模式的测试流**。

```json
{
  "connection": {
    "protocol": "mqtt",
    "mode": "Client",
    "brokerHost": "127.0.0.1",
    "brokerPort": 1883,
    "clientId": "IndComm_Test"
  },
  "session": {
    "active_mode": "auto",
    "variables": {
      "my_topic": {
        "type": "string",
        "access": "RW",
        "enabled": true,
        "params": {
          "topic": "test/topic",
          "qos": 0
        }
      }
    },
    "auto_mode": {
      "test_flow": [
        {
          "name": "Publish Hello",
          "action": "write",
          "variable": "my_topic",
          "value": "Hello MQTT",
          "wait_ms": 100
        }
      ]
    }
  }
}
```

### 7.2 字段详细说明

#### `session.variables` 节点（通信变量）
| 字段名 | 类型 | 说明 |
|---|---|---|
| `type` | string | 数据类型（int8/uint16/int32/float/string/hex 等） |
| `access` | string | `RO`（只读）、`RW`（读写）、`WO`（只写） |
| `enabled` | bool | 是否启用该变量 |
| `params` | object | 协议专属参数（如 MQTT 的 topic, qos） |

#### `test_flow` 节点（测试步骤）
| 字段名 | 类型 | 说明 |
|---|---|---|
| `name` | string | 步骤名称 |
| `action` | string | 动作类型：`read`, `write`, `delay` |
| `variable` | string | 引用的通信变量名 |
| `wait_ms` | int | 发送后等待响应的时间（毫秒） |
| `value` | string | （可选）写入操作时的目标值 |
| `assert` | object | 断言规则 |

---

## 8. 协议清单

| 协议 | ID | 模式 | 传输 | 编解码 | 状态 |
|---|---|---|---|---|---|
| **TCP/IP** | `tcp` | Client / Server | TcpClient / TcpServer | Raw | ✅ 完成 |
| **串口** | `serial` | — | SerialTransport | Raw | ✅ 完成 |
| **Modbus** | `modbus` | TCP / RTU / ASCII | 内置 (ModbusLib) | 事务型 | ✅ 完成 |
| **MQTT** | `mqtt` | Client | Paho MQTT C++ | 内置 | ✅ 完成 |

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

### 9.3 运行

```bash
scripts/build.sh
scripts/test.sh
```

---

## 10. 编码约定

### 10.1 命名空间

- 顶层：`icu`
- 子命名空间：`icu::core` / `icu::ui` / `icu::log` / `icu::settings`

### 10.2 日志规范

| 场景 | 用法 |
|---|---|
| 无参数 | `SPDLOG_INFO(_("..."))` |
| 有参数 | `ICU_SPDLOG_INFO(_("... %s ..."), args)` |

### 10.3 字符串

- 用户可见字符串**必须**用 `_()` 包裹
- 协议字段的 `label` 就是翻译 key

### 10.4 生命周期

| 对象 | 注意事项 |
|---|---|
| **UI 自毁场景** | **必须用 `CallAfter` 延迟** |
| **异步回调持有者** | **销毁前必须清空回调** |

---

## 11. CI 与代码质量

- 格式检查用 **pip 装固定版本的 clang-format 19.1.5**
- 文档体系见 `docs/` 目录
- `CHANGELOG.md` 遵循 [Keep a Changelog](https://keepachangelog.com/)。

---

## 12. 开发路线图

### 已完成

- [x] **阶段 1~7：基础框架、TCP、串口、Modbus 接入**
- [x] **阶段 8：通用自动化测试引擎**
- [x] **阶段 9：独立 CLI 工具 (indcomm-cli)**
- [x] **阶段 10：UI/UX 深度重构**
- [x] **阶段 11：SQLite 数据持久化与实时曲线**
- [x] **阶段 12：MQTT 协议接入**
  - [x] 引入 Paho MQTT C++ submodule。
  - [x] 新增 `MqttProtocol` 与 `MqttSession`。
  - [x] 支持 publish / subscribe 动作与 QoS 设置。
  - [x] 自动重连与订阅恢复。

### 阶段 13：应用设置完善与发布准备 ⬜ 进行中
- [ ] 实现 `ui/SettingsDialog`。
- [ ] 完善字体、主题、托盘持久化。
- [ ] 编译打包脚本。

### 阶段 14：发布准备 ⬜ 未开始

---

## 13. AI Skill 指南

### 13.1 上下文理解

**当前状态**：
- 阶段 1~12 已完成（统一底层 `ISession` + Modbus 接入 + 自动化引擎 + CLI 工具 + UX 重构 + 数据持久化及图表 + MQTT 接入）。

### 13.2 代码生成规范

**新增协议模板**：
```
src/protocols/<protocol_id>/
├── <Name>Protocol.h/.cpp
├── <Name>Config.h
├── <Name>Transport.h/.cpp
└── <Name>Codec.h/.cpp
```

**AI 严禁**：
- 为客户端 / 服务器分别创建协议类
- 在 `GenericProtocolDialog` 或 `SessionPanel` 里加 `if (protocol == "tcp")`
- 在 `core/` 或 `protocols/` 里 include `ui/` 或 `settings/`

---

## 14. 关键决策记录

| 决策 | 理由 |
|---|---|
| **协议是一等公民** | 避免 Client / Server 被拆成两个"协议" |
| **UI 数据驱动** | 新协议不改 UI |
| **通讯变量单一值抽象** | 所有收发数据围绕唯一的 `Value` 运作，避免参数割裂 |
| **测试动作统一** | 全部抽象为 read/write/delay，降低用户心智负担 |
| **SQLite 数据记录与图表** | 以 `.json` 路径为关联键，实现历史与实时数据可视化 |
| **引入 Paho MQTT (ADR 0012)** | 工业级稳定性，CMake 原生支持，不引入 Boost 依赖 |

---

## 15. 变更日志

| 日期 | 变更 |
|---|---|
| 2026-10-22 | 阶段 12 完成：MQTT 协议接入，发布订阅与自动化测试支持 |

---

## 附录 A：快速命令参考

### A.1 构建

```bash
scripts/build.sh              # Debug + 测试
scripts/build.sh release      # Release（build-release/）
scripts/build.sh debug clean  # 先清理再构建
```

### A.2 测试

```bash
scripts/test.sh               # ctest（Debug）
scripts/test.sh release       # ctest（Release）
scripts/test.sh --list        # 列出所有用例
```

### A.3 覆盖率

```bash
scripts/coverage.sh
```

### A.4 i18n

```bash
scripts/update_i18n.sh
msgattrib --untranslated resources/i18n/zh_CN/LC_MESSAGES/*.po
msgattrib --only-fuzzy     resources/i18n/zh_CN/LC_MESSAGES/*.po
```

### A.5 格式化与静态分析

```bash
scripts/format.sh             # 就地格式化
scripts/format.sh check       # 只检查（CI 用 19.1.5）
```

### A.6 残留进程清理

```bash
taskkill //F //IM IndComm.exe 2>/dev/null || true
taskkill //F //IM indcomm-cli.exe 2>/dev/null || true
taskkill //F //IM icu_tests.exe 2>/dev/null || true
```

---

## 附录 B：协议实现检查清单

- [ ] 协议类实现 `IProtocol` 的所有纯虚函数
- [ ] `Id()` 唯一
- [ ] `DisplayName()` 是翻译 key
- [ ] `FieldsForMode()` 返回的 label 都是翻译 key
- [ ] `CreateSession()` 不依赖具体 UI
- [ ] `MakeDefaultFileName()` 生成的名称可跨平台（后缀 `.json`）
- [ ] 已注册到 `ProtocolRegistry`（`RegisterBuiltinProtocols()`）
- [ ] 已追加到 `tests/CMakeLists.txt` 的 `icu_core_test` 源文件列表
- [ ] 新增的 label 已在 `.po` 里翻译
- [ ] 已在本 README 第 8 节协议清单表格里更新状态
- [ ] 已加 `tests/unit/protocols/<id>/` 单测
- [ ] 未修改任何非 `protocols/<id>/` 目录下的生产代码
- [ ] 已实现 `CreateTestStep`，返回对应的测试步骤实例
- [ ] 已实现 `VariableParamsFields()`，返回变量专属参数（如无专属参数则返回空）

---

## 附录 C：设置项实现检查清单

- [ ] Key 定义在 `SettingsKeys.h`
- [ ] 类型化接口加在对应的 `XxxSettings.h`
- [ ] 实现走 `Settings::Instance()`
- [ ] 在 `SettingsDialog` 里加 UI（阶段 13 之后）
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
- [ ] `ctest` 全绿

---

## 附录 E：标准测试流程

### E.1 测试环境准备

```bash
pacman -S netcat            # MSYS2 UCRT64
nc -h                       # 验证
```

com0com 虚拟串口：

```
# 在 com0com Setup Command Prompt（管理员）里
install PortName=COM5 PortName=COM6
list
```

### E.2 通用启动检查

| 序 | 操作 | 期望 |
|---|---|---|
| 1 | 启动程序 | 主窗口出现，标题正确 |
| 2 | 查看标题栏 | 未加载配置时无 `*` |
| 3 | 菜单栏 | `文件 / 视图 / 设置 / 帮助` 完整 |
| 4 | `文件 → 新建连接` | 子菜单顺序：`TCP/IP` → `Serial Port` → `Modbus` → `MQTT` |

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

### E.4 TCP 服务器测试（nc 做客户端）

| 序 | 操作 | 期望 |
|---|---|---|
| 1 | Server 模式，`0.0.0.0:5030`，OK | 状态栏"正在监听"，状态"监听中" |
| 2 | `nc 127.0.0.1 5030` | 程序显示 `Client #1 connected`，Clients=1 |
| 3 | 第二个 `nc` | `Client #2 connected`，Clients=2 |
| 4 | 客户端 1 发 `hello from #1` | 程序显示 `<#1 hello from #1` |
| 5 | 程序发 `broadcast` | 两个 nc 都显示 `broadcast` |
| 6 | 关闭客户端 1 | `Client #1 disconnected`，Clients=1 |
| 7 | 点"停止监听" | 状态"已断开" |

### E.5 串口测试

| 序 | 操作 | 期望 |
|---|---|---|
| 1 | Port 下拉框列出系统串口 | 选 `COM5`，9600 8N1，None 流控，OK |
| 2 | 本程序发 `hello` | 对端（XCOM）收到 `hello` |
| 3 | 对端发 `world` | 本程序接收区 `< world` |
| 4 | 取消 Append newline，再发 `hello` | 对端收到 `hello`（无换行） |
| 5 | 点"断开连接"再"打开连接" | 重连成功（不弹配置对话框） |

### E.6 MQTT 协议测试（使用 Mosquitto 做 Broker）

**准备**：另开终端启动 `mosquitto -v`（默认监听 1883）。

| 序 | 操作 | 期望 |
|---|---|---|
| 1 | `文件 → 新建连接 → MQTT` | 弹配置对话框，填 `127.0.0.1:1883`，OK |
| 2 | 状态显示 | "连接中…"→"已连接" |
| 3 | 添加变量，Topic 填 `test/topic`，QoS 填 `0` | 添加成功，参数在操作区可见 |
| 4 | 在操作区输入 `hello mqtt`，点发送 | 成功，日志显示 `(MQTT Publish)` |
| 5 | 使用另一个 MQTT 客户端订阅 `test/topic` | 收到 `hello mqtt` |
| 6 | 使用另一个客户端向 `test/topic` 发送 `world` | 程序接收区显示 `< world` |
| 7 | 关闭 Broker | 状态变为"重连中…"（自动重连生效） |

### E.7 异常场景

| 场景 | 操作 | 期望 |
|---|---|---|
| 打开损坏的 `.json` | 手动写非法 JSON | 提示"加载失败" |
| 保存到只读路径 | `C:\Windows\System32\test.json` | 提示"保存失败" |
| 串口被占用 | 别的软件已打开 | 提示"打开失败" |
| 串口拔掉 | 运行中拔 USB 转串口 | 状态"已断开" |
| MQTT Broker 不可达 | 连 `127.0.0.1:9999` | 提示超时，并尝试重连 |

### E.8 退出路径测试

| 序 | 操作 | 期望 |
|---|---|---|
| 1 | 开会话，`文件 → 退出` | 干净退出 |
| 2 | 开会话，X → 否 → 否 | 干净退出 |
| 3 | 无会话直接退出 | 干净退出 |
| 4 | 开会话 → 打开 → 断开 → 打开 → 退出 | 干净退出 |

每次退出后：

```bash
tasklist | grep IndComm
```

应**无输出**。

---

**文档结束。**

> 本文档随项目演进持续更新。任何架构级改动必须先更新本文档，再动代码。
