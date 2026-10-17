# Industrial Communication Utility — 开发指南与 AI Skill

> **文档定位**：本文档既是项目开发文档，也是 AI 辅助开发的 Skill 定义。所有开发按本文档推进，AI 在协助编码时必须遵循本文档的架构、约定和任务清单。
>
> **本版重点**：阶段 7 全部完成（Modbus 协议接入 + 统一底层会话架构 + 标准化重连与状态机抽象）。**下一阶段 8：更多协议接入（MQTT 等）**。

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
| **统一底层会话** | 字节流和事务型协议统一抽象为 `ISession`，UI 层不感知底层差异 |
| **状态与重连标准化** | 提供 `ReconnectConfig` 与 `SessionStateMachine`，所有协议开箱即用 |
| **分层解耦** | 应用层 / 框架层 / 协议层 / UI 层 / 服务层，依赖单向 |
| **测试可回归** | `core/` / `protocols/` / `settings/` / `config/` 有单测覆盖 |

### 1.3 技术栈

| 项 | 选择 |
|---|---|
| 语言 | C++17 |
| GUI | wxWidgets 3.3（MSYS2 UCRT64 GCC 16.1.0） |
| 构建 | CMake ≥ 3.20 + MinGW Makefiles |
| 日志 | spdlog（header-only） |
| 测试 | doctest（header-only） |
| JSON | nlohmann/json（submodule） |
| i18n | GNU gettext（.po / .mo） |
| 网络 | wxSocket（内置） |
| 串口 | 自实现（`SerialPort`，Windows 实现） |
| Modbus | serhmarch/ModbusLib（submodule） |
| 静态分析 | clang-tidy + clang-format |
| CI | GitHub Actions |

---

## 2. 核心设计原则

### 2.1 协议是一等公民

**协议是一个整体，不是「客户端」和「服务器」两个东西。**

- `TcpProtocol` 是一个协议类，`Id() = "tcp"`，`DisplayName() = "TCP/IP"`
- 「客户端 / 服务器」是 TCP 协议**内部的一种模式（Mode）**
- 一个 `.json` 文件 = 一份协议配置（含 Mode），**不是两份**

**反例（禁止）**：

```
❌ TcpClientProtocol + TcpServerProtocol
❌ 两个独立的 .json 文件
❌ UI 里硬编码两套字段
❌ 菜单里"TCP客户端"和"TCP服务器"两个入口
```

**正例（目标）**：

```
✅ TcpProtocol（一个类，内部根据 Mode 分发）
✅ 一个 .json 文件，Mode=Client 或 Server
✅ UI 从 IProtocol 的字段描述自动生成
✅ 菜单里只有"TCP/IP"一个入口
```

### 2.2 UI 完全数据驱动且统一

- 协议通过 `FieldDescriptor` 列表描述自己的字段，`GenericProtocolDialog` 据此自动生成**配置对话框**。
- 对于**会话面板（SessionPanel）**：
  - 字节流协议（TCP / 串口）使用统一的**发送框与接收日志**。
  - 事务型协议（如 Modbus）通过实现 `OperationFieldsForMode` 描述操作表单，`SessionPanel` 自动生成参数区。
  - **新增协议绝不需要改动任何 `ui/` 下的代码**。
- **字段分组**：`FieldDescriptor` 支持 `group` 属性，允许将某些字段（如“重连设置”）放在带边框的 GroupBox 中集中展示。

### 2.3 状态机与重连标准化

- **底层差异收敛**：所有会话必须继承 `ISession`。字节流继承 `StreamSession`，事务型继承 `ITransactionSession`。
- **状态与重连抽象**：
  - 任何协议都通过组合 `SessionStateMachine` 来管理生命周期状态（自动去重、自动触发重连计数、自动输出标准日志）。
  - 任何协议都通过组合 `ReconnectConfig` 来获得“自动重连、最大次数、延迟”的统一配置与 UI 生成逻辑。
  - 协议作者只需关心“发报文、收报文”，不需要自己写状态文案和重连倒计时。

### 2.4 分层清晰，依赖单向

```
┌─────────────────────────────────────────────────────────┐
│  app/          应用入口（App / MainFrame）                │
└─────────────────────────┬───────────────────────────────┘
                          │
┌─────────────────────────▼───────────────────────────────┐
│  ui/           UI 层（对话框 / 统一的 SessionPanel 等）     │
└─────────────────────────┬───────────────────────────────┘
                          │
┌─────────────────────────▼───────────────────────────────┐
│  protocols/    协议层（TcpProtocol / ModbusProtocol 等）  │
└─────────────────────────┬───────────────────────────────┘
                          │
┌─────────────────────────▼───────────────────────────────┐
│  core/         框架层（ISession / IProtocol / 状态机等）   │
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
| `config/` | `core/`、`nlohmann/json` | `ui/ protocols/ settings/` |
| `log/` | wxBase、spdlog | 业务逻辑 |

### 2.5 单一数据源

- **协议配置**：唯一来源是 `.json` 文件（`config/` 管）
- **应用设置**：唯一来源是 `settings/` 模块
- **协议描述**：唯一来源是 `IProtocol::FieldsForMode()`
- **协议列表**：唯一来源是 `ProtocolRegistry::All()`
- **会话状态**：唯一来源是 `SessionStateMachine::State()`

### 2.6 UI 生命周期安全

**任何"用户操作触发 UI 自身销毁"的场景，都必须用 `CallAfter` 延迟。**

典型场景：
- 按钮点击 → 切页面 → 销毁当前面板
- 菜单点击 → 重建窗口 → 销毁当前窗口
- 托盘退出 → 销毁托盘自身

**原因**：wxWidgets 的事件派发是**同步**的。如果在回调栈里 `delete` 了事件派发者，返回后访问成员会崩溃。

### 2.7 对象销毁前先断开回调

**任何"会被异步回调引用"的对象，销毁前必须先清空它持有的所有回调。**

典型场景：
- `Session` 析构 → `transport` 触发 `NotifyState(Closed)` → 状态回调 → `CallAfter` → 事件在 Session 死后执行 → 访问悬垂指针。
- `SessionManager::SetActive(new)` / `Close()` 在**真正销毁旧对象之前**清空回调。

**原因**：`CallAfter` 投递的事件在事件循环的后续迭代中执行，此时投递者可能已死。**光靠 `CallAfter` 里的 `if (ptr)` 判断不够**，因为指针可能是悬垂的（已释放但地址还"有效"）。

**正解**：销毁前清空回调（如 `DetachAllCallbacks()`），让投递方在投递前就发现"没有回调"而不再投递。

---

## 3. 架构设计

### 3.1 底层会话架构

所有协议最终都通过 `SessionManager` 管理唯一的 `ISessionPtr`。

| 协议类型 | 会话实现 | 特点 |
|---|---|---|
| 字节流 (TCP, Serial) | `StreamSession` | 组合 `ITransport` 和 `ICodec`，底层做半包/粘包处理。 |
| 事务型 (Modbus) | `ModbusSession` (继承 `ITransactionSession`) | 组合 `ModbusLib`，后台起心跳线程管理连接状态与重连。 |

### 3.2 消息与执行生命周期

**字节流发送**：
`UI` -> `StreamSession::Send()` -> `ICodec::Encode` -> `ITransport::Send`。

**事务型执行**：
`UI` -> `SessionPanel::OnExecute` (搜集表单数据) -> `ModbusSession::Execute` (在后台线程) -> `ModbusClientPort` -> 回调结果（`CallAfter`）。

### 3.3 统一的 UI 生成逻辑

`SessionPanel` 构建时会根据 `ISession` 的具体类型进行动态分派：
```cpp
m_streamSession = dynamic_cast<StreamSession*>(m_session);
m_txSession = dynamic_cast<ITransactionSession*>(m_session);
```
- 如果是 `StreamSession`，渲染标准的日志与发送区。
- 如果是 `ITransactionSession`，拉取 `OperationFieldsForMode` 并自动渲染参数表单，同时在后台维护心跳。

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

### 4.3 `IProtocol`（协议接口）
```cpp
class IProtocol {
public:
    virtual std::string Id() const = 0;
    virtual std::string DisplayName() const = 0;
    virtual bool HasModes() const = 0;
    virtual std::vector<std::string> ModeNames() const = 0;
    virtual std::vector<FieldDescriptor> FieldsForMode(int modeIndex) const = 0;

    // 字节流或事务型的统一创建入口
    virtual ISessionPtr CreateSession(int modeIndex, const std::map<std::string, FieldValue>& fields) = 0;

    // 事务型操作的表单描述，供 SessionPanel 自动生成 UI
    virtual std::vector<FieldDescriptor> OperationFieldsForMode(int modeIndex) const { return {}; }

    virtual std::string MakeDefaultFileName(
        int modeIndex, const std::map<std::string, FieldValue>& fields) const = 0;
};
```

### 4.4 `FieldDescriptor`（扩展分组）
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

### 4.5 `SessionManager`
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
IndustrialCommunicationUtility/
├── src/
│   ├── main.cpp
│   ├── app/
│   │   ├── App.h/.cpp
│   │   └── MainFrame.h/.cpp             ← 主窗口骨架，剥离了会话具体类型判断
│   ├── core/
│   │   ├── ICodec.h
│   │   ├── ITransport.h
│   │   ├── IProtocol.h
│   │   ├── ISession.h                   ← ★ 所有会话的统一基类
│   │   ├── ITransactionSession.h        ← ★ 事务型会话扩展接口
│   │   ├── ReconnectConfig.h            ← ★ 统一重连配置与 UI 字段
│   │   ├── SessionStateMachine.h        ← ★ 统一状态流转与重连计数
│   │   ├── ProtocolRegistry.h/.cpp
│   │   ├── ProtocolTypes.h/.cpp
│   │   ├── StreamSession.h/.cpp         ← ★ 字节流会话实现
│   │   └── SessionManager.h/.cpp        ← 统一管理 ISessionPtr
│   ├── protocols/
│   │   ├── tcp/                         ← 字节流协议范例
│   │   ├── serial/                      ← 字节流协议范例
│   │   ├── modbus/                      ← ★ 事务型协议范例
│   │   │   ├── ModbusProtocol.h/.cpp
│   │   │   └── ModbusSession.h/.cpp
│   │   └── codec/
│   │       └── RawCodec.h
│   ├── ui/
│   │   ├── GenericProtocolDialog.h/.cpp
│   │   ├── SessionPanel.h/.cpp          ← ★ 唯一的动态生成会话面板（已废弃 ModbusPanel）
│   │   ├── LogFrame.h/.cpp
│   │   └── AppTaskBarIcon.h/.cpp
│   ├── settings/                        （保持不变）
│   ├── config/                          （保持不变）
│   └── log/                             （保持不变）
├── tests/
│   ├── CMakeLists.txt
│   ├── main.cpp
│   ├── unit/ ... (包含 Protocol / Modbus 等单测)
│   ├── integration/ ...
│   ├── mocks/ ...
│   └── helpers/ ...
├── resources/
│   ├── app.rc
│   └── i18n/
├── third_party/
│   ├── wxWidgets/
│   ├── spdlog/
│   ├── doctest/
│   ├── json/
│   └── ModbusLib/                       ★ 阶段 7 引入
└── CMakeLists.txt
```

### 5.2 各目录职责

| 目录 | 职责 | 不得包含 | 依赖 |
|---|---|---|---|
| `src/app/` | 启动、主窗口骨架、菜单 | 业务逻辑、协议细节 | `ui/ core/ settings/ config/ log/` |
| `src/core/` | 接口、基础类型、Session | wxWidgets UI、具体协议 | C++ STL、wxBase |
| `src/protocols/` | 具体协议实现、Transport、Codec | UI 代码 | `core/`、`log/` |
| `src/ui/` | 窗口、对话框、面板 | 协议逻辑、socket 操作 | `core/ settings/` |
| `src/settings/` | 应用级设置 | 协议配置、业务逻辑 | `log/` |
| `src/config/` | `.json` 协议配置文件读写 | UI、传输 | `core/`、`nlohmann/json` |
| `src/log/` | 日志初始化、GUI sink | 业务逻辑 | wxBase、spdlog |

---

## 6. 应用设置模块

（保持原样，详见 `settings/README.md`。注意：当前版本 `Settings` 已迁移为 JSON 存储 `settings.json`）

---

## 7. 协议配置文件格式

### 7.1 `.json` 通用结构

由于引入了 `ReconnectConfig`，**所有协议（TCP、Modbus、未来的 MQTT 等）都会共享三个重连字段**，其 key 完全标准化。

```json
{
  "connection": {
    "protocol": "modbus",
    "mode": "TCP",
    "slaveId": 1,
    "timeoutMs": 1000,

    "autoReconnect": true,
    "reconnectMaxAttempts": "0",
    "reconnectDelaySec": "3",

    "host": "127.0.0.1",
    "port": 502
  }
}
```

### 7.2 文件命名约定

```
<协议>_<模式>_<主标识>_<次标识>.json

例：
  TCP_Client_127_0_0_1_502.json
  ModbusTCP_Client_127_0_0_1_502.json
  Serial_COM3_9600.json
```

由 `IProtocol::MakeDefaultFileName()` 生成。

---

## 8. 协议清单

| 协议 | ID | 模式 | 传输 | 编解码 | 状态 |
|---|---|---|---|---|---|
| **TCP/IP** | `tcp` | Client / Server | TcpClient / TcpServer | Raw | ✅ 完成 |
| **串口** | `serial` | — | SerialTransport | Raw | ✅ 完成 |
| **Modbus** | `modbus` | TCP / RTU / ASCII | 内置 (ModbusLib) | 事务型 | ✅ 完成（阶段 7） |
| MQTT | `mqtt` | Pub / Sub | TcpClient | MqttCodec | ⬜ 待实现（阶段 8） |

---

## 9. 测试策略

（保持原样，详见 `docs/developer/testing.md`）

---

## 10. 编码约定

### 10.1 日志规范

**所有日志都要翻译。**

| 场景 | 用法 |
|---|---|
| 无参数 | `SPDLOG_INFO(_("..."))` |
| 有参数 | `ICU_SPDLOG_INFO(_("... %s ..."), args)` |

### 10.2 状态机与重连约定

- **禁止在协议层手写 `Connected` / `Reconnecting` 等状态的判定与日志。**
- 必须通过 `m_stateMachine.TransitionTo(...)` 抛出状态，`SessionStateMachine` 会自动处理状态去重、日志打印和重连计数重置。
- 所有协议必须提供标准重连配置，通过 `ReconnectConfig::GetFieldDescriptors()` 插入 UI 表单，并通过 `ReconnectConfig::FromFields()` 解析参数。

### 10.3 线程安全

- **事务型协议（如 Modbus）必须在后台线程执行阻塞操作**，以防止 UI 卡死。
- 后台线程的 UI 回调必须使用 `wxTheApp->CallAfter()`。
- 销毁协议对象前，必须调用 `Close()` 终止后台线程并 `join`。

### 10.4 提交前自检

```bash
# 日志约定
grep -rn 'SPDLOG_\(INFO\|WARN\|ERROR\|DEBUG\)("'  src/ | grep -v '_('
grep -rn 'ICU_SPDLOG_.*".*{.*}'  src/

# 分层违规
grep -rn '#include.*ui/\|#include.*settings/' src/core/ src/protocols/

# 循环包含
for f in $(find src -name "*.h"); do
    grep -q "#include \"$(basename $f)\"" $f && echo "SELF INCLUDE: $f"
done
```

---

## 11. CI 与代码质量

（保持原样）

---

## 12. 开发路线图

### 已完成

- [x] **阶段 1：协议抽象层**
- [x] **阶段 2：MainFrame 切换到协议层**
- [x] **阶段 3：会话面板通用化**
- [x] **阶段 4：目录结构与设置模块重构**
- [x] **阶段 5：串口协议 + 会话面板通用增强**
- [x] **阶段 6：`.conn` 从 INI 迁移到 JSON**
- [x] **阶段 7：Modbus 协议（TCP/RTU/ASCII） + 统一底层与重连架构**

### 阶段 7：Modbus 协议与架构重构 ✅ 已完成

- [x] 7.1 引入 `ModbusLib` submodule
- [x] 7.2 新增 `ISession` / `ITransactionSession`，统一底层
- [x] 7.3 抽象 `ReconnectConfig` 和 `SessionStateMachine`
- [x] 7.4 `ProtocolRegistry` 支持 `ProtocolKind` 分类
- [x] 7.5 `ModbusProtocol` 实现 01~16 全功能码支持
- [x] 7.6 `SessionPanel` 支持动态表单生成与后台心跳
- [x] 7.7 `.json` 读写及 `FieldDescriptor` 分组属性支持
- [x] 7.8 后台心跳断线检测与自动重连

### 阶段 8：更多协议（MQTT / OPC UA 等） ⬜ 未开始

### 阶段 9：应用设置完善 ⬜ 未开始

### 阶段 10：UI 打磨 ⬜ 未开始

### 阶段 11：发布准备 ⬜ 未开始

---

## 13. AI Skill 指南

### 13.1 上下文理解

**当前状态**：
- 阶段 1~7 全部完成
- **阶段 8 计划**：接入更多协议（MQTT 等），直接复用 `IProtocol` / `ISession` / `ReconnectConfig` / `SessionStateMachine` 标准化组件。

**工程命令**（仓库根）：

| 命令 | 作用 |
|---|---|
| `scripts/build.sh [debug\|release] [clean]` | 配置 + 编译 |
| `scripts/test.sh [debug\|release\|--list]` | ctest / 列出用例 |
| `scripts/format.sh [check]` | 格式化 / 只检查（CI 用 19.1.5） |
| `scripts/update_i18n.sh` | 提取 + 合并 + 编译翻译 |

**关键文件**：
- `src/core/ISession.h` — 会话基类
- `src/core/ReconnectConfig.h` — 标准化重连配置
- `src/core/SessionStateMachine.h` — 标准化状态机
- `src/core/IProtocol.h` — 协议接口
- `src/protocols/modbus/ModbusProtocol.cpp` — 事务型协议范例
- `src/ui/SessionPanel.cpp` — 统一的动态生成 UI 面板

### 13.2 回答用户问题的流程

| 用户意图 | AI 应做什么 |
|---|---|
| 询问设计/架构 | 引用本文档，不发明新架构 |
| 要求实现某协议 | 按 `ModbusProtocol` 或 `TcpProtocol` 模板，只写 `protocols/<name>/`，组合 `ReconnectConfig` 即可获得重连能力 |
| 报告编译错误 | 定位根因，给最小修复 |
| 要求加 UI 功能 | 先问"能否通过 `FieldDescriptor` 描述" |

---

## 14. 关键决策记录

| 决策 | 理由 |
|---|---|
| **统一 `ISession`** | 消除 `StreamSession` 和 `ModbusSession` 在 UI 层的类型割裂 |
| **废弃专用 `ModbusPanel`** | 用 `OperationFieldsForMode` 数据驱动生成通用 UI，实现零代码 UI 适配 |
| **抽取 `ReconnectConfig`** | 统一所有协议的掉线重连配置和 UI 字段，零重复代码 |
| **抽取 `SessionStateMachine`** | 统一状态流转、日志文案和重连计数，防止协议层各自为战 |
| **事务型库黑盒适配** | `ModbusLib` 内部封装驱动与编解码，`ModbusSession` 仅负责请求映射与状态机调度 |

---

## 15. 变更日志

| 日期 | 变更 |
|---|---|
| 2026-10-12 | 初版：确立协议层抽象 |
| ... (省略中间记录) |
| 2026-10-18 | 阶段 7 完成：Modbus 接入、`ISession` 统一、`ReconnectConfig` / `SessionStateMachine` 抽取、`SessionPanel` 动态生成 |

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
```

---

## 附录 B：协议实现检查清单

- [ ] 协议类实现 `IProtocol` 的所有纯虚函数
- [ ] `Id()` 唯一
- [ ] `DisplayName()` 是翻译 key
- [ ] `FieldsForMode()` 返回的 label 都是翻译 key
- [ ] `CreateSession()` 返回 `StreamSession` 或 `ITransactionSession`
- [ ] 如果是事务型，实现 `OperationFieldsForMode`
- [ ] 如果使用重连，将 `ReconnectConfig::GetFieldDescriptors()` 追加到配置字段列表
- [ ] 协议内部组合 `SessionStateMachine` 管理状态
- [ ] `MakeDefaultFileName()` 生成的名称可跨平台（后缀 `.json`）
- [ ] 已注册到 `ProtocolRegistry`
- [ ] 已追加到 `tests/CMakeLists.txt` 的 `icu_core_test` 源文件列表
- [ ] 新增的 label 已在 `.po` 里翻译
- [ ] 已加 `tests/unit/protocols/<id>/` 单测

---

**文档结束。**

> 本文档随项目演进持续更新。任何架构级改动必须先更新本文档，再动代码。
