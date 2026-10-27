# IndComm — 开发指南与 AI Skill

> **文档定位**：本文档既是项目开发文档，也是 AI 辅助开发的 Skill 定义。所有开发按本文档推进，AI 在协助编码时必须遵循本文档的架构、约定和任务清单。
>
> **本版重点**：阶段 21（工作台 UI 稳定性）已完成。自定义标签栏、会话内工具栏、对话框层级以及中英文翻译均已打磨完成。

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
| **多会话并发** | 支持通过多标签页（Tab）同时打开、管理和调试多个独立连接 |
| **高效键盘交互** | 支持通过快捷键（Ctrl+N/C/V/E、Del、↑/↓）快速管理变量与测试步骤 |
| **现代化工作台** | 提供工具栏、侧边快捷入口（仿 VS Code 欢迎页）以及完整的编辑菜单 |
| **统一底层会话** | 字节流、事务型和发布订阅型协议统一抽象为 `ISession` |
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
| 打包 | CPack + NSIS |
| 日志 | spdlog（header-only） |
| 测试 | doctest（header-only） |
| JSON | nlohmann/json（submodule） |
| 数据库 | SQLite3（submodule） |
| i18n | GNU gettext（.po / .mo） |
| 网络/串口 | wxSocket（内置） / 自实现（`SerialPort`） |
| Modbus | serhmarch/ModbusLib（submodule） |
| MQTT | Eclipse Paho MQTT C++（客户端） |
| 图表 | wxGraphicsContext 自绘 |
| CI | GitHub Actions |
| 覆盖率 | gcovr + GCC `--coverage` |

---

## 2. 核心设计原则

### 2.1 协议是一等公民

**协议是一个整体，不是「客户端」和「服务器」两个东西。**
「客户端 / 服务器」是 TCP/IP、Modbus、MQTT 等协议**内部的一种模式（Mode）或角色（Role）**。一个 `.json` 文件 = 一份协议配置，**不是两份**。

### 2.2 UI 完全数据驱动且统一

- 配置对话框：通过 `FieldDescriptor` 列表自动生成 `GenericProtocolDialog`。
- 变量参数表单：通过 `IProtocol::VariableParamsFields()` 描述，自动生成 UI 控件。
- 会话面板：使用 `wxSplitterWindow` 左右分栏，左侧统一展示变量池，右侧根据协议类型自动分派操作区。
- **新增协议绝不需要改动任何 `ui/` 下的代码**。

### 2.3 状态机与重连标准化

- **底层差异收敛**：所有会话继承 `ISession`。字节流继承 `StreamSession`，事务型继承 `ITransactionSession`，发布订阅型继承 `ISession`。
- **状态与重连抽象**：任何协议都组合 `SessionStateMachine` 管理生命周期状态，组合 `ReconnectConfig` 获得统一的重连配置与 UI 字段（UDP 等无连接协议可忽略重连）。

### 2.4 自动化测试引擎通用化

- **动作统一**：无论何种协议，测试步骤统一抽象为 `read`、`write` 和 `delay`。
- 所有的测试步骤抽象为 `ITestStep`。由具体协议通过 `IProtocol::CreateTestStep` 工厂生成。
- **批量生成**：支持在 UI 上选定一个基准步骤，通过设置“重复次数”、“间隔”和“自增/自减”快速生成一系列测试步骤。
- **失败可干预**：断言或执行失败时暂停流程，由用户选择重试当前步骤、跳过该步骤或结束流程，而不是直接终止。
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

### 2.7 多标签页隔离

- **全局管理**：`MainFrame` 持有自定义标签栏 `TabBook`（自绘标签条 + `wxSimplebook` 页面容器），不再使用 `wxAuiNotebook`；页面必须挂到 `wxSimplebook` 名下，才能正确布局并接收鼠标事件。
- **上下文独立**：每一个 Tab 对应一个独立的 `SessionPanel`、`ISessionPtr` 和 `SessionConfig`。它们之间互不干扰。
- **焦点联动**：切换 Tab 时，全局菜单项（保存、关闭、重新配置）必须自动重定向到当前激活的 Tab。

### 2.8 UI 生命周期安全

**任何"用户操作触发 UI 自身销毁"的场景，都必须用 `CallAfter` 延迟。**
**任何"会被异步回调引用"的对象，销毁前必须先清空它持有的所有回调。**

---

## 3. 架构设计

### 3.1 底层会话架构

所有协议最终都通过 `SessionManager` 或独立实例管理。

| 协议类型 | 会话实现 | 特点 |
|---|---|---|
| 字节流 (TCP, Serial, UDP) | `StreamSession` | 组合 `ITransport` 和 `ICodec`，底层做半包/粘包处理。 |
| 事务型 (Modbus) | `ModbusSession` | 组合 `ModbusLib`，后台起心跳线程管理连接状态与重连。 |
| 发布订阅型 (MQTT) | `MqttClientSession` | 封装 `paho.mqtt.cpp`，分为 Publisher 和 Subscriber。 |
| 代理服务型 (MQTT) | `MqttBrokerSession` | 封装底层 Broker 服务，监听端口并路由消息。 |

### 3.2 工作台 UI 逻辑

- **`MainFrame`**：自绘标题栏（拖拽/最小化/最大化/关闭）、`File` / `编辑` 下拉菜单、右上角的日志/语言/设置/关于图标，以及底部的状态栏。
- **`TabBook`**：自绘标签条 + `wxSimplebook`。每个页面先 `Reparent` 到 simplebook，再添加标签；标签支持关闭和标题刷新，最后一个页面的关闭按钮会自动隐藏，欢迎页也可以正常关闭。
- **欢迎页（WelcomePanel）**：在没有活跃会话时展示，采用仿 VS Code 的分块布局，左侧提供开始快捷入口，右侧展示最近打开的连接。
- **会话工具栏**：`SessionPanel` 顶部内嵌一排原生 `wxBitmapButton`（连接/断开、配置、录制、历史、变量与步骤操作、运行/停止、批量和排序）。按钮的图标与可用状态由 `UpdateToolbarState()` 在会话状态、列表选中、页签切换等明确时机刷新，不使用 `wxEVT_UPDATE_UI` 轮询。
- **编辑菜单**：`MainFrame` 的 `编辑` 菜单在弹出时读取当前激活的 `SessionPanel`，把添加/复制/编辑/删除转发给对应 Tab。

### 3.3 自动化测试执行流

```
UI/CLI 加载 test_flow (JSON)
    -> 实例化 TestRunner (包含多个 ITestStep)
    -> 在后台线程按顺序运行：
        -> Execute() 从 variables 中读取 params，发送指令
        -> 等待响应
        -> Assert() 进行比对
        -> 失败时暂停，等待 UI 选择“重试 / 跳过 / 结束”
        -> Extract() 提取变量存入全局池
        -> 按 wait_ms 等待后再执行下一步
        -> 触发 RecordData() 写入 SQLite 并推送到实时图表
    -> 抛出回调更新 UI 或 CLI 终端
```

### 3.4 会话销毁顺序

```
Tab 关闭或 MainFrame 退出
           │
           ▼
SessionPanel 析构：
   1. TestRunner->Stop()
           │
           ▼
   2. Session->DetachAllCallbacks()          ← 先清所有回调
           │
           ▼
   3. Session->Close()                       ← transport 触发 NotifyState(Closed)
           │
           ▼
   4. SessionPtr.reset()                     ← 此时才能真正销毁
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

    // 支持 UI 分组，为空则平铺
    std::string group;
};
```

---

## 5. 目录结构

### 5.1 当前目录

```text
IndComm/
├── src/
│   ├── main.cpp
│   ├── app/
│   ├── cli/
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
│   │   ├── tcpip/
│   │   ├── serial/
│   │   ├── modbus/
│   │   ├── mqtt/
│   │   └── codec/
│   ├── ui/
│   ├── settings/
│   ├── config/
│   └── log/
├── tests/
├── resources/
├── third_party/
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
| **窗口** | 日志窗口几何 | ✅ 已实现 |
| **窗口** | 关闭时最小化到托盘 | ✅ 已实现 |
| **UI** | 字体（UI / 日志 / 数据） | ✅ 已实现 |
| **UI** | 主题（浅色 / 深色 / 系统） | ✅ 已实现 |
| **UI** | 十六进制显示 | ✅ 已实现 |
| **UI** | 自动滚动 | ✅ 已实现 |
| **UI** | 时间戳显示 | ✅ 已实现 |
| **图标** | 协议图标 | ⬜ 预留 |
| **图标** | UI 图标 | ⬜ 预留 |
| **最近连接** | 最近打开的 `.json` 列表 | ✅ 已实现 |

**图例**：✅ 已实现 / 🟡 部分实现或待迁移 / ⬜ 预留

### 6.6 怎么加新设置

1. **加 key** —— `SettingsKeys.h`
2. **加接口** —— `UISettings.h`
3. **加实现** —— `UISettings.cpp`
4. **加 UI** —— `ui/SettingsDialog` 中添加控件并处理应用逻辑
5. **加使用** —— 使用点调用

---

## 7. 协议配置文件格式

### 7.1 `.json` 通用结构

配置文件顶层分为 `connection` 和 `session`。`session` 中包含**通信变量**与**各模式的测试流**。

```json
{
  "connection": {
    "protocol": "mqtt",
    "mode": "Subscriber",
    "brokerHost": "127.0.0.1",
    "brokerPort": 1883,
    "clientId": "IndComm_Sub_01",
    "topic": "test/topic",
    "qos": 0
  },
  "session": {
    "active_mode": "auto",
    "variables": {
      "my_data": {
        "type": "string",
        "access": "RW",
        "enabled": true,
        "params": {}
      }
    },
    "auto_mode": {
      "test_flow": [
        {
          "name": "Receive Hello",
          "action": "read",
          "variable": "my_data",
          "wait_ms": 1000
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
| **TCP/IP** | `tcp` | TCP / UDP | TcpClient / TcpServer / UdpTransport | Raw | ✅ 完成 |
| **串口** | `serial` | — | SerialTransport | Raw | ✅ 完成 |
| **Modbus** | `modbus` | TCP / RTU / ASCII | 内置 (ModbusLib) | 事务型 | ✅ 完成 |
| **MQTT** | `mqtt` | Publisher / Subscriber / Broker | Paho C++ / 内建 Broker | 内置 | ✅ 完成 |

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
- 发布时通过 GitHub Actions 自动运行 `cpack` 并上传安装包与免安装压缩包至 Releases。

---

## 12. 开发路线图

### 已完成

- [x] **阶段 1~14：基础框架、各协议接入、自动化测试、发布准备等**
- [x] **阶段 15：TCP/IP 协议栈统一**（支持 TCP 和 UDP）
- [x] **阶段 16：MQTT 模式细化**（拆分 Publisher、Subscriber、Broker）
- [x] **阶段 17：多会话并发架构**
  - [x] 将主窗口的单会话模式升级为多标签页（Multi-Tab）架构。
  - [x] 支持同时打开、管理和调试多个独立连接。
- [x] **阶段 18：列表操作快捷键与 UX 优化**
  - [x] 支持通过快捷键管理变量池和测试步骤（Ctrl+N/C/V/E, Del, ↑/↓）。
  - [x] 减少对鼠标的依赖，提升现场调试效率。
- [x] **阶段 19：日志文件加载与历史查询**
  - [x] 在日志窗口新增“加载文件”按钮，支持选择查看历史 `.log` 文件。
- [x] **阶段 20：工作台 UI 重构**
  - [x] 引入自绘标题栏、`File` / `编辑` 下拉菜单与右上角快捷图标。
  - [x] 用自定义 `TabBook`（标签条 + `wxSimplebook`）替代 `wxAuiNotebook`，支持标签关闭与标题刷新。
  - [x] 欢迎页重绘为仿 VS Code 风格（左开始、右最近），并支持直接打开向导或最近配置。
  - [x] 会话面板顶部改为原生 `wxBitmapButton` 工具栏，去掉 `wxEVT_UPDATE_UI` 的高频刷新。
- [x] **阶段 21：工作台 UI 稳定性与体验打磨**
  - [x] 修复页面未挂到 `wxSimplebook` 名下导致欢迎页链接、会话工具栏整页失灵的鼠标事件问题。
  - [x] 修复标签标题刷新时重建子控件导致关闭按钮失效的问题；欢迎页可关闭，最后一页隐藏关闭按钮。
  - [x] 统一标签文字/关闭按钮的背景色，消除标签名称后的白块。
  - [x] 所有自绘对话框补齐系统边框与阴影，与日志窗口保持一致的层级感。
  - [x] 重新提取并补齐中英文翻译（`.pot` / `.po` / `.mo`）。
- [x] **阶段 22：测试执行与操作增强**
  - [x] 手动操作值按变量类型校验，非法输入显示错误并禁用发送/写入。
  - [x] 修复 Modbus / MQTT 步骤 `wait_ms` 不生效的问题；统一步骤后等待语义。
  - [x] 断言/执行失败时暂停，支持重试、跳过或结束流程；步骤状态实时显示运行中/通过/失败/已跳过。
  - [x] 变量与步骤列表支持双击编辑、右键菜单、拖动排序。
  - [x] 变量顺序持久化到 `session.variable_order`。
- [x] **阶段 23：自动测试写入、历史查询与批量对话框修复**
  - [x] TCP/MQTT 写步骤使用步骤目标值，不再误用变量当前值；写步骤不提前消费后续读响应。
  - [x] TCP 读步骤超时明确失败，按变量类型解码后再断言，支持数值比较。
  - [x] 历史查询统一使用 `Untitled` 作为未保存会话的项目键，修复查不到数据；数据库增加 busy timeout。
  - [x] 批量生成步骤对话框改为统一的自绘样式。
- [x] **阶段 24：图表、日志与测试执行可视化**
  - [x] 实时图表和历史查询增加快捷时间段下拉框（1s ~ 24h）。
  - [x] 自动/交互模式增加进度条、当前步骤高亮与自动滚动。
  - [x] 日志时间戳统一为 `YYYY-MM-DD HH:MM:SS`。
  - [x] 趋势图日期单显、X 轴仅 `HH:MM:SS`、刻度自适应，窗口信息移到右上角。
  - [x] 接收/发送统计单独一行，避免与目标地址挤在一起。
- [x] **阶段 25：时间范围、统一对话框与内联失败处理**
  - [x] 实时图表和历史查询改为数字 + 单位（秒/分钟/小时/天）的时间范围输入。
  - [x] 统一所有提示/确认/多选对话框为自绘主题样式。
  - [x] 步骤失败不再弹窗，改为进度区内联“重试 / 停止”并等待。
  - [x] 主题化进度条，百分比与 `当前/总数` 显示在进度条内部。
  - [x] 步骤表格使用低饱和度主题配色，当前步骤单选高亮并自动滚动。
- [x] **阶段 26：进度细节与通信日志重设计**
  - [x] 进度条内部显示两位小数百分比，外部显示 `当前/总数`，固定宽度并预留按钮位置。
  - [x] 新增“预计/执行”时间，预计时间按全部步骤等待时间加额外等待计算，执行时间实时刷新。
  - [x] 通信日志改为“时间 / 方向 / 内容”三列表格，发送/接收/错误使用彩色徽标。
  - [x] HEX 显示、时间戳、自动滚动开关实时作用于日志表格。

---

## 13. AI Skill 指南

### 13.1 上下文理解

**当前状态**：
- 阶段 1~26 已全部完成（全协议栈接入 + 自动化引擎 + 数据持久化 + 自动打包 + MQTT 模式细化 + 多会话并发 + 快捷键 + 日志加载 + 工作台 UI、稳定性打磨、测试执行增强）。

### 13.2 代码生成规范

**AI 严禁**：
- 为客户端 / 服务器分别创建协议类
- 在 `GenericProtocolDialog` 或 `SessionPanel` 里加 `if (protocol == "tcp")` 或 `if (protocol == "udp")` 或 `if (protocol == "mqtt" && mode == 0)`
- 在 `core/` 或 `protocols/` 里 include `ui/` 或 `settings/`
- 将不同模式的逻辑硬塞进同一个 Session 或 Transport 类中
- **在 `MainFrame` 中使用全局唯一的 `ISession` 或 `SessionManager` 来管理所有连接**

---

## 14. 关键决策记录

| 决策 | 理由 |
|---|---|
| **协议是一等公民** | 避免 Client / Server 被拆成两个"协议" |
| **UI 数据驱动** | 新协议不改 UI |
| **通讯变量单一值抽象** | 所有收发数据围绕唯一的 `Value` 运作，避免参数割裂 |
| **测试动作统一** | 全部抽象为 read/write/delay，降低用户心智负担 |
| **SQLite 数据记录与图表** | 以 `.json` 路径为关联键，实现历史与实时数据可视化 |
| **使用 CPack + NSIS (ADR 0014)** | 自动化生成标准 Windows 安装包与免安装包，接入 CI 流水线 |
| **统一 TCP/IP 协议 (ADR 0015)** | TCP 和 UDP 同属 TCP/IP 协议族，参照 Modbus 形式合并为一个协议 |
| **细化 MQTT 模式 (ADR 0016)** | 将 MQTT 细分 Publisher、Subscriber、Broker，精确模拟现场角色 |
| **多标签页架构 (ADR 0017)** | 支持同时打开多个连接，提升现场多设备调试的效率与体验 |
| **列表操作快捷键 (ADR 0018)** | 支持键盘直接管理列表，减少鼠标依赖，提升效率 |
| **工作台 UI (ADR 0019)** | 自绘标签栏、会话内工具栏、编辑菜单和仿 VS Code 欢迎页，提升整体操作体验。 |

---

## 15. 变更日志

| 日期 | 变更 |
|---|---|
| 2026-10-27 | 阶段 26 完成：进度条百分比/步骤比/预计与执行时间细化，通信日志改为三列表格与彩色方向徽标。 |
| 2026-10-27 | 阶段 25 完成：时间范围改为数字+单位输入、统一自绘对话框、失败内联重试/停止与主题化进度条。 |
| 2026-10-27 | 阶段 24 完成：快捷时间段、执行进度条/当前步骤高亮、日志完整时间戳、趋势图坐标轴重排。 |
| 2026-10-27 | 阶段 23 完成：修复自动写步骤忽略目标值、读步骤复用旧值与历史查询无数据，批量生成对话框统一为自绘样式。 |
| 2026-11-01 | 阶段 22 完成：输入类型校验、步骤等待与失败重试/跳过/结束、列表双击/右键/拖动排序、变量顺序持久化。 |
| 2026-10-31 | 阶段 21 完成：修复页面挂载/标签关闭/鼠标事件问题，统一对话框边框与阴影，补齐中英文翻译。 |
| 2026-10-31 | 阶段 20 修订：以自定义 `TabBook` 和会话内 `wxBitmapButton` 工具栏落地工作台 UI，移除 `wxEVT_UPDATE_UI` 轮询。 |
| 2026-10-30 | 阶段 20 完成：工作台 UI 重构，新增工具栏、编辑菜单及仿 VS Code 欢迎页。 |
| 2026-10-29 | 阶段 19 完成：日志窗口支持加载外部历史 `.log` 文件进行离线查看。 |
| 2026-10-29 | 阶段 18 完成：支持变量池与测试步骤列表的键盘快捷键操作。 |

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

### A.3 打包

```bash
cd build-release
cpack -G ZIP    # 生成免安装 ZIP 压缩包
cpack -G NSIS   # 生成 Windows 安装程序
```

### A.4 覆盖率

```bash
scripts/coverage.sh
```

### A.5 i18n

```bash
# 重新提取 .pot 并合并到 .po（可选；日常构建会自动把 .po 编译成 .mo）
scripts/update_i18n.sh
msgattrib --untranslated resources/i18n/zh_CN/LC_MESSAGES/*.po
msgattrib --only-fuzzy     resources/i18n/zh_CN/LC_MESSAGES/*.po
```

> 构建时 CMake 会调用 `msgfmt` 把 `resources/i18n/<lang>/LC_MESSAGES/*.po` 编译到构建目录，并复制到输出目录；`.pot` / `.mo` 属于生成物，不进入版本库。

### A.6 格式化与静态分析

```bash
scripts/format.sh             # 就地格式化
scripts/format.sh check       # 只检查（CI 用 19.1.5）
```

### A.7 残留进程清理

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
- [ ] `ctest` 全绿

---

## 附录 E：标准测试流程

### E.1 测试环境准备

> **界面说明（阶段 20 起）**：窗口顶部是自绘标题栏 + `File` / `编辑` 两个下拉菜单，右上角依次是日志、语言、设置、关于四个图标。下文中的“新建连接”统一指 `File → New`（打开新建连接向导，在向导里选择协议）；“显示日志 / 语言 / 设置 / 关于”指右上角对应图标。

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
| 2 | 欢迎页布局 | 左侧有“开始”按钮，右侧有“最近打开”列表 |
| 3 | 右上角图标 | 依次为日志、语言、设置、关于 |
| 4 | 菜单 | 顶部左侧存在 `File` / `编辑` 两个下拉菜单 |

### E.3 TCP 模式测试（nc 做服务器）

**准备**：另开终端 `nc -l -p 5020`。

| 序 | 操作 | 期望 |
|---|---|---|
| 1 | `File → New → TCP/IP` | 弹配置对话框 |
| 2 | 模式选 `TCP`，角色选 `Client`，填写 `127.0.0.1:5020`，OK | 状态"连接中…"→"已连接" |
| 3 | 程序发 `hello` | nc 显示 `hello` |
| 4 | nc 输入 `world` | 程序数据日志 `< world` |
| 5 | 点"断开连接" | 状态"已断开" |
| 6 | 点"打开连接" | **直接重连**（不弹框）→"已连接" |

### E.4 TCP 模式测试（nc 做客户端）

| 序 | 操作 | 期望 |
|---|---|---|
| 1 | 模式选 `TCP`，角色选 `Server`，`0.0.0.0:5030`，OK | 状态栏"正在监听"，状态"监听中" |
| 2 | `nc 127.0.0.1 5030` | 程序显示 `Client #1 connected`，Clients=1 |
| 3 | 第二个 `nc` | `Client #2 connected`，Clients=2 |
| 4 | 客户端 1 发 `hello from #1` | 程序显示 `<#1 hello from #1` |
| 5 | 程序发 `broadcast` | 两个 nc 都显示 `broadcast` |
| 6 | 关闭客户端 1 | `Client #1 disconnected`，Clients=1 |
| 7 | 点"停止监听" | 状态"已断开" |

### E.5 UDP 模式测试（nc 做接收端）

**准备**：另开终端 `nc -u -l -p 5040`。

| 序 | 操作 | 期望 |
|---|---|---|
| 1 | `File → New → TCP/IP` | 弹配置对话框 |
| 2 | 模式选 `UDP`，角色选 `Client`，目标 `127.0.0.1:5040`，本地端口 `0`，OK | 状态"已打开"（UDP 无连接，状态可直接就绪） |
| 3 | 程序发 `hello udp` | nc 终端显示 `hello udp` |
| 4 | 另开终端 `echo "world udp" | nc -u 127.0.0.1 <程序本地端口>` | 程序数据日志显示 `< world udp` |

### E.6 串口测试

| 序 | 操作 | 期望 |
|---|---|---|
| 1 | Port 下拉框列出系统串口 | 选 `COM5`，9600 8N1，None 流控，OK |
| 2 | 本程序发 `hello` | 对端（XCOM）收到 `hello` |
| 3 | 对端发 `world` | 本程序接收区 `< world` |
| 4 | 取消 Append newline，再发 `hello` | 对端收到 `hello`（无换行） |
| 5 | 点"断开连接"再"打开连接" | 重连成功（不弹配置对话框） |

### E.7 MQTT 协议测试（使用 Mosquitto 做 Broker）

**准备**：另开终端启动 `mosquitto -v`（默认监听 1883）。

**测试 Publisher（发布者）模式**：
| 序 | 操作 | 期望 |
|---|---|---|
| 1 | `File → New → MQTT` | 模式选 `Publisher`，填 `127.0.0.1:1883`，OK |
| 2 | 状态显示 | "连接中…"→"已连接" |
| 3 | 添加变量，Topic 填 `test/topic` | 添加成功 |
| 4 | 在操作区输入 `hello mqtt`，点发送 | 成功，日志显示 `(MQTT Publish)` |
| 5 | 使用另一个 MQTT 客户端（如 MQTTX）订阅 `test/topic` | 收到 `hello mqtt` |

**测试 Subscriber（订阅者）模式**：
| 序 | 操作 | 期望 |
|---|---|---|
| 1 | `File → New → MQTT` | 模式选 `Subscriber`，填 `127.0.0.1:1883`，填 Topic `test/topic`，OK |
| 2 | 状态显示 | "连接中…"→"已连接" |
| 3 | 使用另一个 MQTT 客户端向 `test/topic` 发送 `world` | 程序接收区显示 `< world`，且界面无发送按钮 |

### E.8 多标签页并发测试

| 序 | 操作 | 期望 |
|---|---|---|
| 1 | 依次建立 TCP Client、TCP Server、串口三个连接 | 界面上出现三个 Tab 标签 |
| 2 | 在 Tab 1 发数据 | Tab 2 和 Tab 3 日志不受影响 |
| 3 | 切换到 Tab 2，修改其配置 | Tab 1 的配置不随之改变 |
| 4 | 关闭 Tab 2 | Tab 1 和 Tab 3 依然保持连接 |

### E.9 会话工具栏与编辑菜单测试

| 序 | 操作 | 期望 |
|---|---|---|
| 1 | 新建一个 TCP 连接 | 会话页顶部出现一排图标按钮 |
| 2 | 未选中变量时观察复制/编辑/删除/批量/上移/下移 | 对应图标置灰 |
| 3 | 选中一个变量 | 复制/编辑/删除、录制、历史变为可用 |
| 4 | 点击“运行流程” | 图标切换为停止；流程结束后恢复为运行 |
| 5 | 切到“自动模式”页签 | 添加/复制/编辑/删除、批量、上移/下移联动到步骤列表 |
| 6 | 点击顶部 `编辑 → 添加变量` | 当前 Tab 弹出新建变量窗口 |
| 7 | 关闭欢迎页标签 | 欢迎页消失；只剩一个页面时关闭按钮自动隐藏 |

### E.10 快捷键操作测试

| 序 | 操作 | 期望 |
|---|---|---|
| 1 | 在变量列表点击空白处，按 `Ctrl+N` | 弹出新建变量对话框 |
| 2 | 选中一个变量，按 `Ctrl+C`，再按 `Ctrl+V` | 列表中出现一个副本 |
| 3 | 选中一个变量，按 `Ctrl+E` | 弹出编辑变量对话框 |
| 4 | 选中一个变量，按 `Del` | 该变量被删除 |
| 5 | 在测试步骤列表按 `↑` / `↓` | 选中项上下切换 |
| 6 | 在测试步骤列表按 `Ctrl+N` | 弹出新建步骤对话框 |

### E.11 日志加载测试

| 序 | 操作 | 期望 |
|---|---|---|
| 1 | 点击右上角“日志”图标 | 弹出日志窗口 |
| 2 | 点击 `Load File...` | 弹出文件选择框，默认定位到 exe 目录下 |
| 3 | 选择一个之前的 `.log` 文件，点击打开 | 日志窗口清空并显示该文件的内容 |
| 4 | 勾选 `Auto Scroll` 再次加载 | 界面应停留在文件顶部 |

### E.12 全局设置测试

| 序 | 操作 | 期望 |
|---|---|---|
| 1 | 点击右上角“设置”图标 | 打开设置对话框 |
| 2 | 勾选“HEX 显示”，点“应用” | 所有已打开的会话面板立刻变为 HEX 显示状态 |
| 3 | 取消勾选“自动滚动”，点“确定” | 日志滚动停止 |
| 4 | 重启程序 | 上述开关状态保持之前设置的值 |
| 5 | 勾选“关闭时最小化到托盘”，点“确定” | 点击关闭按钮时直接最小化，不再弹询问框 |

### E.13 安装包测试

| 序 | 操作 | 期望 |
|---|---|---|
| 1 | 运行 `IndComm-Setup.exe` | 弹出安装向导，可选择安装路径 |
| 2 | 安装完成后双击桌面快捷方式 | 程序正常启动，图标正确 |
| 3 | 在开始菜单中寻找 | 存在 IndComm 文件夹及快捷方式 |
| 4 | 运行卸载程序 | 所有文件被清除，快捷方式消失 |

### E.14 异常场景

| 场景 | 操作 | 期望 |
|---|---|---|
| 打开损坏的 `.json` | 手动写非法 JSON | 提示"加载失败" |
| 保存到只读路径 | `C:\Windows\System32\test.json` | 提示"保存失败" |
| 串口被占用 | 别的软件已打开 | 提示"打开失败" |
| 串口拔掉 | 运行中拔 USB 转串口 | 状态"已断开" |
| MQTT Broker 不可达 | 连 `127.0.0.1:9999` | 提示超时，并尝试重连 |

### E.15 退出路径测试

| 序 | 操作 | 期望 |
|---|---|---|
| 1 | 开两个会话，`File → Quit` | 干净退出 |
| 2 | 开会话，X → 否 → 否 | 干净退出 |
| 3 | 无会话直接退出 | 干净退出 |
| 4 | 开会话 → 打开 → 断开 → 打开 → 退出 | 干净退出 |

每次退出后：

```bash
tasklist | grep IndComm
```

应**无输出**。

### E.16 列表快捷操作与失败重试

| 序 | 操作 | 期望 |
|---|---|---|
| 1 | 双击变量行 | 打开编辑变量对话框 |
| 2 | 右键变量行 | 出现添加/复制/编辑/删除/上移/下移菜单 |
| 3 | 把变量行拖到另一位置 | 顺序改变；保存并重新打开后顺序保持 |
| 4 | 双击/右键步骤行 | 打开编辑步骤对话框或步骤操作菜单 |
| 5 | 把步骤行拖到另一位置 | 步骤顺序改变 |
| 6 | 自动模式下让某一步断言失败 | 弹窗询问：重试 / 跳过 / 结束 |
| 7 | 选择“重试”并修正条件 | 当前步骤重新执行，成功后继续后续步骤 |
| 8 | 选择“跳过” | 该步骤显示“已跳过”，后续步骤继续执行 |
| 9 | 给 int32 变量输入“哈哈”并点发送 | 显示红色类型错误，发送按钮置灰，不发出数据 |

### E.17 自动写入与历史查询

| 序 | 操作 | 期望 |
|---|---|---|
| 1 | 变量当前值 123，写步骤目标值填 1，运行流程 | 实际发送 1，而不是 123 |
| 2 | 读步骤等待超时 | 步骤失败并提示未收到响应，不再复用旧值 |
| 3 | 开启录制一段时间后打开“历史查询”并查询 | 图表显示已录制的历史曲线 |
| 4 | 选中步骤后点击“批量生成步骤” | 弹出与编辑步骤一致的自绘样式对话框 |

### E.18 时间段、进度与图表

| 序 | 操作 | 期望 |
|---|---|---|
| 1 | 图表页选择 `1s / 5s / 10s / 1min` | 图表时间窗口立即切换 |
| 2 | 历史查询选择快捷时间段 | 起止时间自动填充，点击查询后显示对应范围 |
| 3 | 自动模式运行多步流程 | 进度条前进，当前步骤高亮并自动滚动到可见位置 |
| 4 | 查看日志时间 | 显示 `YYYY-MM-DD HH:MM:SS`，不再带毫秒 |
| 5 | 查看趋势图 | 顶部显示日期，X 轴只显示 `HH:MM:SS`，刻度不重叠 |

### E.19 时间输入、内联失败与统一对话框

| 序 | 操作 | 期望 |
|---|---|---|
| 1 | 在图表页输入 `30` 并选择“分钟” | 图表显示最近 30 分钟 |
| 2 | 在历史查询输入 `2` 并选择“小时” | 起止时间自动更新为最近 2 小时 |
| 3 | 让一个步骤执行失败 | 不弹窗，进度条右侧出现“重试 / 停止” |
| 4 | 点击“重试” | 当前步骤重新执行，进度条继续前进 |
| 5 | 点击“停止” | 流程结束，进度条显示已停止 |
| 6 | 打开任意提示/确认/录制变量选择 | 使用统一的自绘标题栏和扁平按钮 |

---

**文档结束。**

> 本文档随项目演进持续更新。任何架构级改动必须先更新本文档，再动代码。
