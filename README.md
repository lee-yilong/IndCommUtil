# IndComm — 开发指南与 AI Skill

> **文档定位**：本文档既是项目开发文档，也是 AI 辅助开发的 Skill 定义。所有开发按本文档推进，AI 在协助编码时必须遵循本文档的架构、约定和任务清单。
>
> **本版重点**：阶段 10（UX 重构）已完成。**当前处于阶段 11（SQLite 数据可视化）。**

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

**IndComm**（Industrial Communication Utility）是一个面向工业现场的多协议通讯测试/调试及自动化测试平台。目标整合 NetAssist、Modbus Poll 等工具的调试能力，并提供基于 `.json` 测试流程的无头自动化执行。

### 1.2 核心目标

| 目标 | 含义 |
|---|---|
| **协议可插拔** | 新增协议 = 新增一个类，不改 UI、不改菜单、不改对话框 |
| **UI 数据驱动** | 配置对话框、会话面板、菜单项全部从协议描述自动生成 |
| **统一底层会话** | 字节流和事务型协议统一抽象为 `ISession`，UI 层不感知底层差异 |
| **状态与重连标准化** | 提供 `ReconnectConfig` 与 `SessionStateMachine`，所有协议开箱即用 |
| **自动化测试** | 提供通用的 `TestRunner`，支持通信变量提取、断言校验和正则匹配 |
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
| i18n | GNU gettext（.po / .mo） |
| 网络/串口 | wxSocket（内置） / 自实现（`SerialPort`） |
| Modbus | serhmarch/ModbusLib（submodule） |
| 数据库/图表 | SQLite3（阶段 11） / wxGraphicsContext 自绘（阶段 11） |
| CI | GitHub Actions |
| 覆盖率 | gcovr + GCC `--coverage` |
| 调试器 | LLDB-DAP（GDB 17.2 与 cpptools MI 握手失败）；CLI 用 gdb |

---

## 2. 核心设计原则

### 2.1 协议是一等公民

**协议是一个整体，不是「客户端」和「服务器」两个东西。**
「客户端 / 服务器」是 TCP 或 Modbus 协议**内部的一种模式（Mode）**。一个 `.json` 文件 = 一份协议配置（含 Mode 与测试步骤），**不是两份**。

### 2.2 UI 完全数据驱动且统一

- 配置对话框：通过 `FieldDescriptor` 列表自动生成 `GenericProtocolDialog`。
- 会话面板：使用 `wxNotebook` 提供三种模式（手动、交互、自动）。左侧固定为全局变量池，右侧通过 `VariableParamsFields` 自动渲染参数表单和唯一的操作值。
- **新增协议绝不需要改动任何 `ui/` 下的代码**。

### 2.3 状态机与重连标准化

- **底层差异收敛**：所有会话继承 `ISession`。字节流继承 `StreamSession`，事务型继承 `ITransactionSession`。
- **状态与重连抽象**：任何协议都组合 `SessionStateMachine` 管理生命周期状态，组合 `ReconnectConfig` 获得统一的重连配置与 UI 字段。

### 2.4 自动化测试引擎通用化（阶段 8）

- 所有的测试步骤抽象为 `ITestStep`。由具体协议通过 `IProtocol::CreateTestStep` 工厂生成。
- 引入**通信变量（CommVariable）**：支持定义类型、读写权限（RO/RW）和协议专属参数。测试步骤通过变量名直接引用参数，实现业务逻辑与协议细节解耦。
- `TestRunner` 纯粹负责流程调度，支持自动全跑与交互单步阻断，并实时向 UI 抛出底层日志。

### 2.5 分层清晰，依赖单向

- `app/` -> `ui/` -> `protocols/` -> `core/`。
- `core/` 只依赖 C++ STL 和 wxBase（不包含任何 GUI 元素）。
- `ui/` 不得直接操作 socket 或串口，必须通过 `ISession` 接口。
- **核心逻辑抽离为 `icu_core_lib` 静态库，同时服务 GUI (`IndComm.exe`) 与 CLI (`indcomm-cli.exe`)。**

### 2.6 单一数据源

- **协议配置**：唯一来源是 `.json` 文件（`config/` 管）
- **应用设置**：唯一来源是 `settings/` 模块
- **协议描述**：唯一来源是 `IProtocol::FieldsForMode()` 与 `VariableParamsFields()`
- **协议列表**：唯一来源是 `ProtocolRegistry::All()`
- **会话状态**：唯一来源是 `SessionStateMachine::State()`

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
| 事务型 (Modbus) | `ModbusSession` (继承 `ITransactionSession`) | 组合 `ModbusLib`，后台起心跳线程管理连接状态与重连。 |

### 3.2 统一的 UI 生成逻辑

`SessionPanel` 采用 `wxSplitterWindow` 分栏布局。左侧为全局变量池，右侧使用 `wxNotebook` 提供三种模式。在手动模式下，构建时根据 `ISession` 动态分派：
```cpp
m_streamSession = dynamic_cast<StreamSession*>(m_session);
m_txSession = dynamic_cast<ITransactionSession*>(m_session);
```

### 3.3 自动化测试执行流

```
UI/CLI 加载 test_flow (JSON)
    -> 实例化 TestRunner (包含多个 ITestStep)
    -> 在后台线程按顺序运行：
        -> Execute() 从 variables 中读取 params，发送指令
        -> 等待响应
        -> Assert() 进行比对
        -> Extract() 提取变量存入全局池
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
                                              ← 但已无回调，不会投递事件
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

### 4.3 `ITestStep`（自动化测试步骤，阶段 8）
```cpp
class ITestStep {
public:
    virtual ~ITestStep() = default;
    virtual std::string Name() const = 0;
    virtual TestResult Execute(ISession* session, std::map<std::string, CommVariable>& vars) = 0;
    virtual bool Assert(const TestResult& result, std::string& outErrorMsg) = 0;
    virtual void Extract(const TestResult& result, std::map<std::string, CommVariable>& vars) = 0;
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

    // 阶段 10 新增：返回该协议通讯变量的专属参数描述（驱动 UI 自动生成操作表单）
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

### 4.6 `SessionManager`
```cpp
class SessionManager {
public:
    void SetActive(ISessionPtr s);   // 先 DetachAndClose 旧会话
    void Close();                    // 走 DetachAndClose
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

### 5.1 当前目录

```text
IndComm/
├── src/
│   ├── main.cpp
│   ├── app/
│   ├── cli/                             ← 独立 CLI 入口
│   │   └── main.cpp
│   ├── core/
│   │   ├── ICodec.h
│   │   ├── ITransport.h
│   │   ├── IProtocol.h
│   │   ├── ISession.h                   ← 所有会话的统一基类
│   │   ├── ITransactionSession.h        ← 事务型会话扩展接口
│   │   ├── ReconnectConfig.h            ← 统一重连配置与 UI 字段
│   │   ├── SessionStateMachine.h        ← 统一状态流转与重连计数
│   │   ├── ITestStep.h                  ← 测试步骤接口
│   │   ├── TestRunner.h/.cpp            ← 测试调度器
│   │   ├── ProtocolRegistry.h/.cpp
│   │   ├── ProtocolTypes.h/.cpp
│   │   ├── StreamSession.h/.cpp         ← 字节流会话实现
│   │   └── SessionManager.h/.cpp
│   ├── protocols/
│   │   ├── tcp/
│   │   ├── serial/
│   │   ├── modbus/
│   │   └── codec/
│   ├── ui/
│   │   ├── GenericProtocolDialog.h/.cpp
│   │   ├── SessionPanel.h/.cpp          ← 唯一的动态生成会话面板
│   │   ├── TestStepEditDialog.h/.cpp    ← 步骤可视化编辑器
│   │   ├── VariableEditDialog.h/.cpp    ← 变量可视化编辑器
│   │   ├── LogFrame.h/.cpp
│   │   ├── LogSink.h/.cpp
│   │   └── AppTaskBarIcon.h/.cpp
│   ├── settings/
│   ├── config/
│   └── log/
├── tests/
├── resources/
├── third_party/
└── CMakeLists.txt
```

### 5.2 各目录职责

| 目录 | 职责 | 不得包含 | 依赖 |
|---|---|---|---|
| `src/app/` | 启动、主窗口骨架、菜单 | 业务逻辑、协议细节 | `ui/ core/ settings/ config/ log/` |
| `src/cli/` | CLI 入口与参数解析 | GUI 代码 | `core/ config/` |
| `src/core/` | 接口、基础类型、Session | wxWidgets UI、具体协议 | C++ STL、wxBase |
| `src/protocols/` | 具体协议实现、Transport、Codec | UI 代码 | `core/`、`log/` |
| `src/ui/` | 窗口、对话框、面板 | 协议逻辑、socket 操作 | `core/ settings/` |
| `src/settings/` | 应用级设置 | 协议配置、业务逻辑 | `log/` |
| `src/config/` | `.json` 协议配置文件读写 | UI、传输 | `core/`、`nlohmann/json` |
| `src/log/` | 日志初始化、基础配置 | 业务逻辑 | wxBase、spdlog |

### 5.3 分层规则

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
| 新增决策 | `docs/architecture/adr/` |

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

- **`Settings` 是唯一允许直接操作底层存储的类**
- 存储位置按优先级探测：`%APPDATA%` → `%LOCALAPPDATA%` → exe 目录 → 当前目录。
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

**场景**：加一个"是否在关闭时最小化到托盘"设置。

1. **加 key** —— `SettingsKeys.h`
2. **加接口** —— `UISettings.h`
3. **加实现** —— `UISettings.cpp`
4. **加 UI** —— `ui/SettingsDialog`（阶段 12 待建）
5. **加使用** —— 使用点调用

**全程只改 `settings/` + `SettingsDialog` + 使用点，不触碰 `core/` 和 `protocols/`。**

---

## 7. 协议配置文件格式

### 7.1 `.json` 通用结构

配置文件顶层分为 `connection` 和 `session`。`session` 中包含**通信变量**与**各模式的测试流**。

```json
{
  "connection": {
    "protocol": "modbus",
    "mode": "TCP",
    "host": "127.0.0.1",
    "port": 502,
    "reconnect": {
      "autoReconnect": true,
      "maxAttempts": "0",
      "delaySec": "3"
    }
  },
  "session": {
    "active_mode": "auto",
    "variables": {
      "my_temp": {
        "type": "int",
        "access": "RW",
        "params": {
          "slaveId": 1,
          "functionCode": 3,
          "startAddress": 0,
          "count": 1
        }
      }
    },
    "auto_mode": {
      "test_flow": [
        {
          "name": "Step 1 Read",
          "action": "read",
          "variable": "my_temp",
          "wait_ms": 100
        }
      ]
    },
    "interactive_mode": {
      "test_flow": []
    }
  }
}
```

### 7.2 字段详细说明

#### `session.variables` 节点（通信变量）
| 字段名 | 类型 | 说明 |
|---|---|---|
| `type` | string | 数据类型（int / string / bool / float） |
| `access` | string | `RO`（只读）、`RW`（读写）、`WO`（只写） |
| `params` | object | 协议专属参数（如 Modbus 的 slaveId, functionCode 等） |

#### `test_flow` 节点（测试步骤）
| 字段名 | 类型 | 说明 |
|---|---|---|
| `name` | string | 步骤名称 |
| `action` | string | 动作类型：`read`, `write`, `send`, `delay` 等 |
| `variable` | string | 引用的通信变量名 |
| `wait_ms` | int | 发送后等待响应的时间（毫秒） |
| `value` | string | （可选）写入操作时的目标值，不填则用变量当前值 |

---

## 8. 协议清单

| 协议 | ID | 模式 | 传输 | 编解码 | 状态 |
|---|---|---|---|---|---|
| **TCP/IP** | `tcp` | Client / Server | TcpClient / TcpServer | Raw | ✅ 完成 |
| **串口** | `serial` | — | SerialTransport | Raw | ✅ 完成 |
| **Modbus** | `modbus` | TCP / RTU / ASCII | 内置 (ModbusLib) | 事务型 | ✅ 完成（阶段 7/8） |
| MQTT | `mqtt` | Pub / Sub | TcpClient | MqttCodec | ⬜ 待实现 |

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
└── helpers/      # 测试辅助
```

### 9.4 构建集成

```cmake
option(ICU_BUILD_TESTS "Build unit/integration tests" OFF)
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
scripts/build.sh
scripts/test.sh

# 或手工
cmake -S . -B build -G "MinGW Makefiles" \
      -DCMAKE_BUILD_TYPE=Debug -DICU_BUILD_TESTS=ON
cmake --build build -j16
ctest --test-dir build --output-on-failure
```

### 9.6 覆盖要求

| 模块 | 覆盖率目标 | 当前 |
|---|---|---|
| `src/core/` | ≥ 80% | ~85% ✅ |
| `src/protocols/` | ≥ 70% | ~50% ⚠️ |
| `src/settings/` | ≥ 60% | 0% ⚠️ |
| `src/config/` | ≥ 70% | 100% ✅ |
| `src/ui/` / `src/app/` | 手动测试 | — |

**已知缺口及处理计划**：

| 缺口 | 原因 | 补测时机 |
|---|---|---|
| `TcpTransport.cpp` 0% | 依赖真实 wxSocket 事件循环 | 阶段 7 统一评估 |
| `SerialPort.cpp` / `SerialTransport.cpp` | 依赖真实串口硬件 | 手工回归（com0com） |
| `settings/*.cpp` 0% | 依赖 `wxApp` 初始化 | 阶段 12 |

### 9.7 什么必须测

- `FieldValue` 序列化往返
- `ActiveProfile` 的 `operator==` / `operator!=`
- `ConnectionProfile` 读写往返（JSON）
- `Session` 状态机
- `SessionManager` 销毁前清空回调
- 每个 Codec 的半包 / 粘包 / 校验
- 每个 Protocol 的 `FieldsForMode()` 数量 + label 是翻译 key
- `ITransport` 的 `clientId` 语义

### 9.8 测试命名约定

- 文件：`test_<类名>.cpp`
- 用例：`TEST_CASE("<类名>: <场景描述>")`

### 9.9 Mock 对象

放在 `tests/mocks/`（header-only）：
- `MockTransport` —— 内存假 Transport；`Open()` 只切到 `Opening`
- `MockCodec` —— 直通 / 定长切帧两模式

### 9.10 覆盖率工具

- **gcovr 8.6+**（MSYS2 UCRT64）
- filter 用 `.*/src/.*` 正则
- 日常构建不开覆盖率，`ICU_BUILD_COVERAGE=OFF` 是默认

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
| `wxSocketBase` | 销毁前先 `Notify(false)` |
| `wxEvtHandler` | 用 `Bind` 或 `SetEventHandler` |
| **UI 自毁场景** | **必须用 `CallAfter` 延迟** |
| **异步回调持有者** | **销毁前必须清空回调** |
| **后台读线程** | `Close()` 时先 `Interrupt()` 打断阻塞读，再 `join()` |

### 10.6 线程安全

- 所有 UI 操作必须在主线程
- 从 socket / 定时器 / 读线程回调更新 UI 用 `CallAfter`
- 跨线程传递数据**必须复制**到堆上再 `CallAfter`
- **事务型协议和 `TestRunner` 必须在后台线程执行阻塞操作**，防止 UI 卡死。

### 10.7 提交前自检

```bash
# 日志约定
grep -rn 'SPDLOG_\(INFO\|WARN\|ERROR\|DEBUG\)("'  src/ | grep -v '_('
grep -rn 'ICU_SPDLOG_.*".*{.*}'  src/

# 分层违规
grep -rn '#include.*ui/\|#include.*settings/' src/core/ src/protocols/

# 绕过 settings 直接调底层存储
grep -rn "wxConfigBase" src/ | grep -v src/settings/

# 循环包含
for f in $(find src -name "*.h"); do
    grep -q "#include \"$(basename $f)\"" $f && echo "SELF INCLUDE: $f"
done
```

---

## 11. CI 与代码质量

### 11.1 CI

GitHub Actions 每次 push / PR 跑四件事：**wxWidgets 构建缓存 → Debug 构建+测试 → Release 构建+测试 → clang-format 检查**。

**几个必须知道的点**：
- `third_party/wxWidgets`、`third_party/spdlog`、`third_party/json` 都是 **submodule**
- **必须用 `submodules: recursive`**：wxWidgets 自身还带 12 个嵌套 submodule
- wxWidgets 的 configure 参数 **不带 `--enable-debug`**，产出 release 库名
- CI 上 wx 采用"标准布局"，本地是"老式布局"，**CMakeLists.txt 自动探测两种布局**
- 强制 `wxDEBUG_LEVEL=0`（release wx 库没有 debug 符号）
- 格式检查用 **pip 装固定版本的 clang-format 19.1.5**

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
docs/
├── architecture/
│   ├── overview.md
│   ├── layers.md
│   └── adr/
│       ├── README.md                 索引 + 何时写 ADR
│       ├── template.md
│       ├── 0001-protocol-as-first-class-with-modes.md
│       ├── 0002-ui-data-driven-by-field-descriptors.md
│       ├── 0003-doctest-as-test-framework.md
│       ├── 0004-transaction-protocol-separation.md
│       ├── 0005-serial-protocol-and-common-panel-enhancement.md
│       ├── 0006-conn-format-ini-to-json.md
│       ├── 0007-test-runner-engine-architecture.md
│       ├── 0008-standalone-headless-cli-tool.md
│       ├── 0009-sqlite-data-logging-and-charting.md
│       ├── 0010-project-file-and-test-flow-redesign.md
│       └── 0011-session-panel-ux-refactoring.md
├── user/
│   ├── getting-started.md
│   ├── tcp.md
│   ├── serial.md
│   └── troubleshooting.md
├── developer/
│   ├── building.md
│   ├── adding-a-protocol.md
│   ├── adding-a-setting.md
│   └── testing.md
└── api/
    └── core.md
```

**约定**：架构级决策写进 `docs/architecture/adr/`（编号递增，不复用不删除）；接口变更先改 `docs/api/`，再改代码。

### 11.5 `CHANGELOG.md`

遵循 [Keep a Changelog](https://keepachangelog.com/)。

---

## 12. 开发路线图

### 已完成

- [x] **阶段 1~7：基础框架、TCP、串口、Modbus 接入**
- [x] **阶段 8：通用自动化测试引擎**
  - [x] 引入 `ITestStep` 与 `TestRunner`。
  - [x] 引入通信变量概念，实现 UI 动态管理。
  - [x] 交互模式单步阻断与自动模式一键运行。
- [x] **阶段 9：独立 CLI 工具 (indcomm-cli)**
  - [x] CMake 抽离 `icu_core_lib`。
  - [x] 编译产出 `IndComm.exe` 和 `indcomm-cli.exe`。
  - [x] CLI 支持 `-r` 自动运行、`-i` 交互、`--read` / `--write`。
- [x] **阶段 10：UI/UX 深度重构**
  - [x] 10.1 新建 `TestStepEditDialog`，用下拉框和输入框替代手写 JSON。
  - [x] 10.2 新建 `VariableEditDialog`，通过表单编辑变量属性与协议参数。
  - [x] 10.3 `SessionPanel` 布局重构，引入 `wxSplitterWindow` 分离配置与日志。
  - [x] 10.4 `TestRunner` 执行步骤联动，实时高亮并输出底层报文日志。

### 阶段 11：SQLite 数据持久化与实时曲线 ⬜ 未开始

- [ ] 11.1 引入 SQLite3 库。
- [ ] 11.2 建立 `test_runs` 和 `run_data` 数据库表。
- [ ] 11.3 `TestRunner` 增加 `OnDataRecorded` 回调。
- [ ] 11.4 执行测试时将变量数据写入 SQLite。
- [ ] 11.5 实现基于 `wxGraphicsContext` 的实时折线图。
- [ ] 11.6 历史数据查询与图表回放。

### 阶段 12：更多协议（MQTT 等） ⬜ 未开始
### 阶段 13：应用设置完善 ⬜ 未开始
### 阶段 14：发布准备 ⬜ 未开始

---

## 13. AI Skill 指南

### 13.1 上下文理解

**当前状态**：
- 阶段 1~10 已完成（统一底层 `ISession` + Modbus 接入 + 自动化引擎 + CLI 工具 + UX 重构）。
- **核心新增文件**：`ui/TestStepEditDialog`、`ui/VariableEditDialog`。

**遗留问题**：

| 问题 | 影响 |
|---|---|
| 托盘图标在部分环境不显示（`Shell_NotifyIcon` 权限拒绝） | 系统层问题，不影响功能 |

**工程命令**（仓库根）：

| 命令 | 作用 |
|---|---|
| `scripts/build.sh [debug\|release] [clean]` | 配置 + 编译 |
| `scripts/test.sh [debug\|release\|--list]` | ctest / 列出用例 |
| `scripts/format.sh [check]` | 格式化 / 只检查（CI 用 19.1.5） |
| `scripts/coverage.sh` | 生成 HTML 覆盖率报告 |
| `scripts/update_i18n.sh` | 提取 + 合并 + 编译翻译 |

**关键文件**：
- `src/core/ISession.h` — 会话基类
- `src/core/ReconnectConfig.h` — 标准化重连配置
- `src/core/SessionStateMachine.h` — 标准化状态机
- `src/core/ITestStep.h` — 自动化测试步骤接口
- `src/core/IProtocol.h` — 协议接口
- `src/protocols/modbus/ModbusProtocol.cpp` — 事务型协议范例
- `src/ui/SessionPanel.cpp` — 统一的动态生成 UI 面板
- `src/cli/main.cpp` — 独立 CLI 入口

### 13.2 回答用户问题的流程

| 用户意图 | AI 应做什么 |
|---|---|
| 询问设计/架构 | 引用本文档，不发明新架构 |
| 要求实现某协议 | 按 `ModbusProtocol` 或 `TcpProtocol` 模板，只写 `protocols/<name>/`，组合 `ReconnectConfig` 即可获得重连能力 |
| 报告编译错误 | 定位根因，给最小修复 |
| 报告崩溃 | 要求先用 gdb 抓栈（见附录 A.8），按栈定位 |
| 要求加 UI 功能 | 先问"能否通过 `FieldDescriptor` 或 `VariableParamsFields` 描述" |
| 要求加 App 设置 | 走 `settings/` 模块 |
| 要求加测试 | 放 `tests/unit/` 或 `tests/integration/`，用 doctest |

### 13.3 代码生成规范

**新增协议模板**：

```
src/protocols/<protocol_id>/
├── <Name>Protocol.h/.cpp
├── <Name>Config.h
├── <Name>Transport.h/.cpp
└── <Name>Codec.h/.cpp

tests/unit/protocols/<protocol_id>/
├── test_<Name>Protocol.cpp
└── test_<Name>Codec.cpp
```

**AI 严禁**：
- 为客户端 / 服务器分别创建协议类
- 在 `MainFrame` 里加 `OnNewXxxClient` / `OnNewXxxServer`
- 在 `GenericProtocolDialog` 或 `SessionPanel` 里加 `if (protocol == "tcp")`
- 硬编码菜单项
- 绕过 `settings/` 直接调底层存储
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
| 测试代码 | `SPDLOG_WARN("...")` | `SPDLOG_WARN(_("..."))` |
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
| 拆分串口为 Master / Slave 两个协议 | 裸串口无角色语义 |
| 在 UI 层加 `if (protocol == "xxx")` | 破坏数据驱动 |
| 直接 `delete m_trayIcon` | 触发断言 / 崩溃 |
| `ICU_SPDLOG_*` 里用 `{}` | 格式不支持 |
| 头文件 include 自己 | 循环包含 |
| 绕过 `settings/` 直接调 `wxConfigBase` | 破坏单一数据源 |
| `core/` 或 `protocols/` 里 include `ui/` / `settings/` | 破坏分层 |
| **在按钮回调里同步销毁宿主面板** | **闪退** |
| **销毁异步回调持有者前不清空回调** | **CallAfter 悬垂指针崩溃** |
| 新增协议不加测试 | 违背 9.7 |
| 新增协议不追加到 `icu_core_test` 源文件列表 | 链接失败（vtable undefined） |
| 在 `add_executable` 之前 `target_*_options` | CMake 报"target not built" |
| 在 `mgr.Close()` / `SetActive(new)` 之后访问旧对象裸指针 | use-after-free |
| `--coverage` 默认开启 | 显著拖慢日常开发 |
| 串口读线程 `Close()` 时不 `Interrupt` 直接 `join` | 死锁 |
| `SerialPort` 依赖 wxWidgets | 破坏可移植性和可测性 |

---

## 14. 关键决策记录

| 决策 | 理由 |
|---|---|
| **协议是一等公民** | 避免 Client / Server 被拆成两个"协议" |
| **一个 `.json` 文件一份配置** | 切换模式不需保存两份 |
| **UI 数据驱动** | 新协议不改 UI |
| **`FieldDescriptor` 的 label 作为翻译 key** | 不引入额外 i18n 机制 |
| **`ITransport` 的 `clientId` 参数** | 统一客户端和服务器 |
| **Transport 层保留 Client / Server 两个实现** | wxWidgets API 差异大 |
| **UI 层不区分 Client / Server** | 通过 `IsServerMode()` 适配 |
| **`ITransport::Name()` 虚函数** | 显示名由具体 Transport 提供 |
| **日志全翻译** | 用户可见 |
| **测试代码不翻译** | 开发者向 |
| **`wxTaskBarIcon` 不直接 delete** | 内部窗口销毁时机问题 |
| **托盘退出用 `CallAfter` 包裹 `Close`** | 避免在托盘事件栈上销毁托盘 |
| **UI 自毁场景用 `CallAfter` 延迟** | 避免访问已释放内存 |
| **销毁异步回调持有者前清空回调** | CallAfter 投递的事件在后续迭代执行，此时投递者可能已死 |
| **`SessionManager::DetachAndClose()`** | 先清 6 个回调 → Close → reset，防悬垂指针 |
| **`app.rc` 引用 `wx.rc`** | 一次性解决 Manifest、光标、图标 |
| **`.json` 格式（阶段 6）** | INI 表达数组能力弱、类型系统缺失、生态不通用 |
| **`nlohmann/json`（阶段 6）** | header-only，API 简洁，与现有 header-only 风格一致 |
| **`.json` 不兼容 INI（阶段 6）** | 项目在开发阶段，用户少，趁早硬切比长期带兼容负担干净 |
| **`settings/` 与 `config/` 分离** | App 级偏好 vs 协议级配置 |
| **`Settings` 是唯一底层存储消费者** | 单一数据源 |
| **`ProtocolRegistry` 用 `vector<pair>` 保序** | 菜单按注册顺序显示，不受 id 字母序影响 |
| **用 doctest 做测试框架** | header-only，零配置，编译快 |
| **`tests/main.cpp` 提供 doctest 实现** | header-only 必须在一个 TU 展开 |
| **Mock 对象放 `tests/mocks/`（header-only）** | 不污染 `core/`，可被多测试复用 |
| **`icu_core_test` 显式列源文件** | 精确控制，避免未来 GLOB 误拾取重依赖协议 |
| **UI 不做单测** | 投入产出比低 |
| **include 路径用 `src/` 前缀** | `target_include_directories` 已含 `src` |
| **"打开连接" 直接重连不弹框** | 用户已配置好，直接复用 |
| **"配置连接" 只在未连接状态可用** | 避免连上后改配置造成混乱 |
| **测试用 LLDB-DAP 调试** | GDB 17.2 与 cpptools MI 握手失败 |
| **覆盖率用 gcovr，`ICU_BUILD_COVERAGE=OFF` 默认** | 插桩拖慢编译，日常不开 |
| **`MockTransport::Open()` 只切到 `Opening`** | 对齐真实异步 Transport 语义 |
| **Mock 计数用 `shared_ptr<int>` 存储** | 跨对象生命周期可读 |
| **保存对话框只过滤 `*.json`** | Shell 枚举所有文件会触发部分环境缩略图缓存异常 |
| **测试临时目录优先用 cwd** | 系统 `%TEMP%` 在 MSYS2 等环境权限受限 |
| **CI wxWidgets 不启用 `--enable-debug`** | 只产 release 库名，与 CMakeLists 链接一致 |
| **CMakeLists 自动探测 wx 布局** | 兼容本地 in-tree build 与 CI 标准布局 |
| **强制 `wxDEBUG_LEVEL=0`** | Debug 构建不引用 release wx 库中缺失的 `wxOnAssert` |
| **串口单模式** | 裸串口无 Client/Server 语义 |
| **串口后台线程 + 同步 I/O** | 延迟低，跨平台清晰，关闭可控 |
| **不引入第三方串口库** | 边界自己掌握 |
| **不做 RTS/DTR 控制** | 场景少，会污染 Transport 抽象 |
| **事务型协议与字节流协议分离** | ModbusLib 是事务型 API，与 ITransport 字节流抽象不兼容 |
| **统一 `ISession`** | 消除 `StreamSession` 和 `ModbusSession` 在 UI 层的类型割裂 |
| **废弃专用 `ModbusPanel`** | 用 `OperationFieldsForMode` 数据驱动生成通用 UI，实现零代码 UI 适配 |
| **抽取 `ReconnectConfig`** | 统一所有协议的掉线重连配置和 UI 字段，零重复代码 |
| **抽取 `SessionStateMachine`** | 统一状态流转、日志文案和重连计数，防止协议层各自为战 |
| **自动化测试引擎通用化 (ADR 0007)** | 通过 `ITestStep` 和变量池，实现协议无关的自动化步骤调度 |
| **独立 CLI 无头工具 (ADR 0008)** | 方便接入 CI/CD 流水线，避免主程序臃肿 |
| **SQLite 数据记录与图表 (ADR 0009)** | 以 `.json` 路径为关联键，实现历史与实时数据可视化 |
| **项目文件重构 (ADR 0010)** | 剥离 `connection` 与 `session`，引入 Action 枚举与变量双向绑定 |
| **项目更名 `IndComm`** | 缩短可执行文件名，提升命令行输入体验 |
| **UX 深度重构 (ADR 0011)** | 废除 JSON 弹窗，引入可视化编辑器与分栏布局 |

**完整 ADR 列表**：[docs/architecture/adr/](docs/architecture/adr/README.md)

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
| 2026-10-13 | 新增"UI 生命周期安全"原则（2.5 节） |
| 2026-10-13 | 4.4 完成：核心类型合并 |
| 2026-10-13 | 4.5 完成：测试模块 |
| 2026-10-13 | 调试器从 GDB 切换到 LLDB-DAP |
| 2026-10-13 | 4.6 完成：CI / clang-tidy / version.h / CHANGELOG / LICENSE / CONTRIBUTING / docs / scripts |
| 2026-10-14 | 修复设置存储在受限权限下无法初始化：`Settings` 改用 `wxFileConfig`，多候选位置探测 |
| 2026-10-14 | 修复保存连接对话框在 Shell 异常环境下卡死：过滤器收窄为 `*.conn` |
| 2026-10-14 | 修复测试临时目录权限问题：`TestEnv` 优先用 cwd |
| 2026-10-14 | 修复 CI wxWidgets 构建参数：去掉 `--enable-debug`，缓存 key 加 `-release` |
| 2026-10-14 | 新增 ADR 0004：事务型协议与字节流协议分离 |
| 2026-10-14 | 新增 ADR 0005：串口协议实现与通用会话面板增强 |
| 2026-10-14 | 路线图更新：阶段 5 改为串口 + 面板增强，Modbus 移到阶段 6 |
| 2026-10-15 | CI 修复：wxWidgets 改 in-tree 构建，CMakeLists 自动探测 wx 布局，`wxDEBUG_LEVEL=0` |
| 2026-10-15 | 阶段 5 完成：`SerialPort` / `SerialTransport` / `SerialProtocol` + 单测 + 面板增强 |
| 2026-10-15 | 菜单顺序按注册顺序，ProtocolRegistry 用 `vector<pair>` |
| 2026-10-15 | Port 下拉框自动枚举系统串口，`ITransport::Name()` 泛化 |
| 2026-10-16 | 修复退出时 Session 悬垂指针崩溃（`SessionManager::DetachAndClose()`） |
| 2026-10-16 | 新增原则 2.6「对象销毁前先断开回调」 |
| 2026-10-16 | 命令从 PowerShell 换为 UCRT64；新增附录 A.8 GDB 调试 |
| 2026-10-16 | 新增 ADR 0006：`.conn` 从 INI 迁移到 JSON |
| 2026-10-16 | 路线图更新：新阶段 6 = 格式迁移，Modbus 顺延为阶段 7 |
| 2026-10-18 | 阶段 7 完成：Modbus 接入、`ISession` 统一、重连抽象 |
| 2026-10-18 | 确立阶段 8/9/10 架构，新增 ADR 0007/0008/0009 |
| 2026-10-19 | 阶段 8 & 9 完成：通信变量、自动化引擎、CLI 工具，项目更名 IndComm |
| 2026-10-20 | 新增 ADR 0011，启动阶段 10 UX 重构 |
| 2026-10-20 | 阶段 10 完成：UI 彻底分栏，变量池全局可见，动态参数生成 |
| 2026-10-20 | 修复技术债：跨平台脚本兼容、`ModbusSession` 跨线程安全、i18n 全量更新 |

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

# 或直接跑
build/tests/icu_tests.exe --test-case="Session:*"
```

### A.3 覆盖率

```bash
scripts/coverage.sh
# 报告：build-coverage/coverage/index.html
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

clang-tidy -p build src/core/Session.cpp
```

### A.6 自检

```bash
# 日志约定
grep -rn 'SPDLOG_\(INFO\|WARN\|ERROR\|DEBUG\)("'  src/ | grep -v '_('
grep -rn 'ICU_SPDLOG_.*".*{.*}'  src/

# 分层违规
grep -rn '#include.*ui/\|#include.*settings/' src/core/ src/protocols/

# 绕过 settings 直接调底层存储
grep -rn "wxConfigBase" src/ | grep -v src/settings/

# 循环包含
for f in $(find src -name "*.h"); do
    grep -q "#include \"$(basename $f)\"" $f && echo "SELF INCLUDE: $f"
done
```

### A.7 残留进程清理

```bash
# 查进程
tasklist | grep -E "IndComm|indcomm-cli|icu_tests"

# 杀进程（双斜杠防止 MSYS2 路径转换）
taskkill //F //IM IndComm.exe 2>/dev/null || true
taskkill //F //IM indcomm-cli.exe 2>/dev/null || true
taskkill //F //IM icu_tests.exe 2>/dev/null || true
```

### A.8 GDB 调试

**用 MSYS2 UCRT64 的 gdb 抓崩溃栈。**

#### 前置：确认 gdb 可用

```bash
gdb --version
# 若未安装：pacman -S mingw-w64-ucrt-x86_64-gdb
```

#### 场景 1：交互式调试（推荐排查时用）

```bash
cd build
gdb ./IndComm.exe
```

进入 gdb 后：

```
(gdb) run
```

在 GUI 里复现崩溃。程序 SIGSEGV 时 gdb 自动中断：

```
(gdb) bt            # 简栈
(gdb) bt full       # 完整栈（含局部变量）
(gdb) info registers
(gdb) quit
```

#### 场景 2：批处理（一条命令抓栈）

```bash
cd build
gdb -batch -ex "run" -ex "bt" -ex "bt full" ./IndComm.exe
```

复现崩溃后，gdb 打印栈并退出，适合快速抓一次。

#### 场景 3：程序正常退出（没有崩溃）

gdb 输出 `exited normally` 和 `No stack`。说明本次运行没触发崩溃，可能是偶发问题，多跑几次或换复现路径。

#### 读栈要点

- **栈顶 `#0`** 是崩溃点（出错的函数 + 行号）
- **往下的 `#1`、`#2`** 是调用链，找到**第一个属于 `icu::` 的帧**，那是本次 bug 的直接触发位置
- `bt full` 里的局部变量值能帮助判断是不是悬垂指针（例如 `this=0x7479800` 后访问 `m_transport` 崩了，说明对象已释放）

#### 典型例子：退出时 Session 悬垂指针

```
#0  icu::Session::TransportName() at Session.cpp:67  ← m_transport 已释放
#1  ProtocolSessionPanel::UpdateModeLabel at ...:402
#2  ProtocolSessionPanel::RefreshFromSession at ...:141
#3  MainFrame::AttachSessionCallbacks lambda at ...:682
#4  wxAsyncMethodCallEventFunctor::Execute
#5  wxEvtHandler::ProcessPendingEvents
```

**解读**：`Session` 析构时 transport 触发了状态变化 → `CallAfter` 投递 lambda →
事件在 `Session` 死后执行 → 访问已释放的 `m_transport` → SIGSEGV。

**修法**：在 `SessionManager` 销毁 `Session` 前清空所有回调（见 `DetachAndClose()`）。

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
- [ ] （阶段 8）已实现 `CreateTestStep`，返回对应的测试步骤实例
- [ ] （阶段 10）已实现 `VariableParamsFields()`，返回变量专属参数（如无专属参数则返回空）

---

## 附录 C：设置项实现检查清单

- [ ] Key 定义在 `SettingsKeys.h`
- [ ] 类型化接口加在对应的 `XxxSettings.h`
- [ ] 实现走 `Settings::Instance()`
- [ ] 在 `SettingsDialog` 里加 UI（阶段 12 之后）
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
| 4 | `文件 → 新建连接` | 子菜单顺序：`TCP/IP` → `Serial Port` → `Modbus` |

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

### E.5 串口测试

**准备**：
- com0com 创建虚拟串口对（`COM5` ↔ `COM6`）
- 或两台机器物理串口对接

**基础流程**：

| 序 | 操作 | 期望 |
|---|---|---|
| 1 | `文件 → 新建连接 → Serial Port` | 弹配置对话框，Port 下拉框列出系统串口 |
| 2 | 选 `COM5`，9600 8N1，None 流控，OK | 状态"已连接"，Target 显示 `COM5 @ 9600` |
| 3 | 另一软件（XCOM / UartAssist）打开 `COM6` | — |
| 4 | 本程序发 `hello` | 对端收到 `hello`（带换行） |
| 5 | 对端发 `world` | 本程序接收区 `< world` |
| 6 | 取消 Append newline，再发 `hello` | 对端收到 `hello`（无换行） |
| 7 | 点"断开连接" | 状态"已断开" |
| 8 | 点"打开连接" | 重连成功（不弹配置对话框） |

**面板增强验证**：

| 场景 | 操作 | 期望 |
|---|---|---|
| HEX 显示 | 勾选"HEX Display" | 接收区字节以十六进制显示 |
| 时间戳 | 取消勾选 | 消息前无时间戳 |
| 自动滚动 | 取消勾选 | 新消息到达时视图不滚动 |
| 清空接收 | 点"Clear" | 接收区清空 |
| 发送新行 | 取消勾选"Append newline" | 发送不加 `\n` |

### E.6 配置文件测试

| 序 | 操作 | 期望 |
|---|---|---|
| 1 | Client 模式 → 保存为 `TCP_Client_test.json` | 标题显示 `- TCP_Client_test`，无 `*` |
| 2 | 查看文件内容 | 含 `"protocol": "tcp"` + `"mode": "Client"` |
| 3 | 点"配置连接..."改端口 | 标题出现 `*` |
| 4 | `文件 → 保存连接` | `*` 消失 |
| 5 | 关闭连接 | 无提示 |
| 6 | `文件 → 打开连接` 选该文件 | 恢复 |

### E.7 最近连接测试

| 序 | 操作 | 期望 |
|---|---|---|
| 1 | 保存 3 个 `.json` | 最近连接列出 3 项 |
| 2 | 点击其中一项 | 加载并连接 |
| 3 | 重启 | 仍在 |
| 4 | 手动删除一个 → 重启 | 该项消失 |
| 5 | 添加第 6 个 | 只保留 5 个 |

### E.8 窗口与 UI 测试

| 序 | 操作 | 期望 |
|---|---|---|
| 1 | 拖窗口到右下角 | — |
| 2 | 关闭 → 重启 | 恢复位置 |
| 3 | 最大化 → 关闭 → 重启 | 保持最大化 |
| 4 | `视图 → 显示日志` | 日志窗口弹出 |
| 5 | 关闭日志窗口 | 隐藏，主窗口正常 |
| 6 | 再次打开日志 | 历史保留 |

### E.9 语言切换测试

| 序 | 操作 | 期望 |
|---|---|---|
| 1 | `设置 → 语言 → English` | 弹框询问重启 |
| 2 | 点"是" | 全英文界面 |
| 3 | `Settings → Language → 简体中文` | 中文界面 |

### E.10 关闭与托盘测试

| 序 | 操作 | 期望 |
|---|---|---|
| 1 | 有未保存配置时点 X | 弹框"是否保存？" |
| 2 | 点"取消" | 不关闭 |
| 3 | 点"是" | 保存后关闭 |
| 4 | 点"否" | 直接关闭 |
| 5 | 无未保存配置时点 X | 弹框"是否最小化到托盘？" |
| 6 | 点"是" | 窗口隐藏，托盘出现 |
| 7 | 双击托盘图标 | 窗口恢复 |
| 8 | 托盘右键 → 退出 | 干净退出 |
| 9 | `tasklist \| grep IndComm` | 无输出 |

### E.11 异常场景

| 场景 | 操作 | 期望 |
|---|---|---|
| 打开损坏的 `.json` | 手动写非法 JSON | 提示"加载失败" |
| 打开旧 `.conn` 文件 | 选 `.conn` | 提示"加载失败"（不兼容，见 ADR 0006） |
| 保存到只读路径 | `C:\Windows\System32\test.json` | 提示"保存失败" |
| 配置不完整 | 手动删除 `remoteHost` | 加载后使用默认值 |
| 串口被占用 | 别的软件已打开 | 提示"打开失败" |
| 串口拔掉 | 运行中拔 USB 转串口 | 状态"已断开" |
| 串口不存在 | Port 填 `COM99` | 打开失败，日志有错误 |

### E.12 退出路径测试

**准备**：每次先开一个会话（TCP 或串口），再测退出。

| 序 | 操作 | 期望 |
|---|---|---|
| 1 | 开 TCP 会话，`文件 → 退出` | 干净退出 |
| 2 | 开 TCP 会话，X → 否 → 否 | 干净退出 |
| 3 | 开串口会话，`文件 → 退出` | 干净退出 |
| 4 | 开串口会话，X → 否 → 否 | 干净退出 |
| 5 | 无会话直接退出 | 干净退出 |
| 6 | 开会话 → 断开连接 → 退出 | 干净退出 |
| 7 | 开会话 → 打开 → 断开 → 打开 → 退出 | 干净退出 |

每次退出后：

```bash
tasklist | grep IndComm
```

应**无输出**。

### E.13 测试结果记录


## 测试日期：2026-XX-XX
## 版本：v0.1.0
## 测试人：<姓名>

| 章节 | 结果 | 备注 |
|---|---|---|
| E.2 启动 | ✅ | — |
| E.3 TCP Client | ✅ | — |
| E.4 TCP Server | ✅ | — |
| E.5 串口 | ✅ | — |
| E.6 配置文件 | ✅ | — |
| E.7 最近连接 | ✅ | — |
| E.8 窗口与 UI | ✅ | — |
| E.9 语言切换 | ✅ | — |
| E.10 关闭与托盘 | ✅ | — |
| E.11 异常场景 | ✅ | — |
| E.12 退出路径 | ✅ | — |

**结论**：全部通过。


---

**文档结束。**

> 本文档随项目演进持续更新。任何架构级改动必须先更新本文档，再动代码。

