# 架构总览

> 本文档描述项目的整体结构与设计意图。任何架构级改动都必须先更新本文档，再改代码（见根 `README.md` 第 13 节）。

---

## 1. 目标

**Industrial Communication Utility** 是一个面向工业现场的多协议通讯测试/调试工具，
定位类似 NetAssist + Modbus Poll + MQTT Explorer 的整合版。

| 目标 | 含义 |
|---|---|
| 协议可插拔 | 新增协议 = 新增一个类，不碰 UI / 菜单 / 对话框 |
| UI 数据驱动 | 配置对话框、会话面板、菜单项从协议描述自动生成 |
| 分层解耦 | 应用层 / UI 层 / 协议层 / 框架层 / 服务层，依赖单向 |
| 设置集中 | 语言、字体、主题、图标统一走 `settings/` |
| 测试可回归 | `core/` / `protocols/` / `settings/` / `config/` 有单测覆盖 |
| 跨平台 | Windows 优先，保留 Linux / macOS 兼容性 |

---

## 2. 分层结构

```
┌─────────────────────────────────────────────────────────────┐
│  app/          应用入口（App / MainFrame）                    │
└───────────────────────────┬─────────────────────────────────┘
                            │
┌───────────────────────────▼─────────────────────────────────┐
│  ui/           UI 层（对话框 / 面板 / 窗口 / 托盘）             │
└───────────────────────────┬─────────────────────────────────┘
                            │
┌───────────────────────────▼─────────────────────────────────┐
│  protocols/    协议层（具体协议 + Transport + Codec）          │
└───────────────────────────┬─────────────────────────────────┘
                            │
┌───────────────────────────▼─────────────────────────────────┐
│  core/         框架层（IProtocol / ITransport / ICodec /      │
│                        Session / ProtocolRegistry）           │
└───────┬───────────────────┬─────────────────┬───────────────┘
        │                   │                 │
┌───────▼──────┐  ┌─────────▼──────┐  ┌───────▼────────┐
│  config/     │  │  settings/     │  │  log/          │
│  .conn 文件   │  │  应用级偏好     │  │  日志          │
└──────────────┘  └────────────────┘  └────────────────┘
```

**依赖规则：只允许自上而下。** 反向依赖（`core/` include `ui/`、`protocols/` include `settings/`）一律禁止。

---

## 3. 各层职责

| 目录 | 职责 | 不得包含 | 允许依赖 |
|---|---|---|---|
| `src/app/` | 启动、主窗口骨架、菜单 | 业务逻辑、协议细节 | `ui/ core/ settings/ config/ log/` |
| `src/core/` | 接口、基础类型、Session | wxWidgets UI、具体协议 | C++ STL、wxBase |
| `src/protocols/` | 具体协议实现、Codec | UI 代码 | `core/`、`log/` |
| `src/ui/` | 窗口、对话框、面板 | 协议逻辑、socket 操作 | `core/ settings/` |
| `src/settings/` | 应用级设置 | 协议配置、业务逻辑 | `log/` |
| `src/config/` | `.conn` 协议配置文件读写 | UI、传输 | `core/` |
| `src/log/` | 日志初始化、GUI sink | 业务逻辑 | wxBase、spdlog |
| `tests/` | 单元 / 集成测试 | 生产代码 | `core/ protocols/ settings/ config/` |
| `docs/` | 文档、ADR | 代码 | — |

---

## 4. 核心抽象

### 4.1 `IProtocol` —— 协议是一等公民

**协议是一个整体，不是「客户端」和「服务器」两个东西。**
「客户端 / 服务器」是 TCP 协议**内部的一种模式（Mode）**。

```cpp
class IProtocol {
public:
    virtual std::string Id() const = 0;                  // "tcp"
    virtual wxString DisplayName() const = 0;            // 翻译 key
    virtual bool HasModes() const = 0;
    virtual std::vector<wxString> ModeNames() const = 0; // {"Client", "Server"}
    virtual std::vector<FieldDescriptor> FieldsForMode(int modeIndex) const = 0;
    virtual SessionPtr CreateSession(int modeIndex,
                                     const std::map<std::string, FieldValue>& fields) = 0;
    virtual wxString MakeDefaultFileName(int modeIndex,
                                         const std::map<std::string, FieldValue>& fields) const = 0;
};
```

一个 `.conn` 文件 = 一份协议配置（含 Mode），**不是两份**。

### 4.2 `FieldDescriptor` —— UI 数据驱动的关键

协议用 `FieldDescriptor` 列表描述自己的字段，`GenericProtocolDialog` 据此自动生成控件：

```
IProtocol::FieldsForMode(mode)  →  [ FieldDescriptor, ... ]
                                        ↓
                              GenericProtocolDialog 自动生成
                                        ↓
                              [wxTextCtrl / wxSpinCtrl / wxChoice / wxCheckBox]
```

`FieldDescriptor` 的 label 同时是翻译 key，不引入额外 i18n 机制。

### 4.3 `ITransport` / `ICodec` / `Session`

- `ITransport`：连接生命周期。`clientId` 参数统一了客户端（`0`）与服务器（多客户端）语义。
- `ICodec`：`Encode`（上行编码）+ `Feed`（下行切帧，处理半包/粘包）+ `Reset`。
- `Session`：组合一个 Transport 与一个 Codec，对外暴露统一的状态机与回调；
  UI 只跟 `Session` 打交道，不知道底下是 TCP 还是串口。
- `SessionManager`：持有唯一活动会话，管理 `SetActive` / `Close` 生命周期。

---

## 5. 关键设计决策

完整列表见 [ADR 目录](adr/README.md)，以及根 `README.md` 第 14 节。

摘要：

| 决策 | 理由 |
|---|---|
| Transport 层保留 Client / Server 两个实现 | wxWidgets API 差异大 |
| UI 层不区分 Client / Server | 通过 `IsServerMode()` 适配 |
| `config/` 用 INI（`wxFileConfig`） | 跨平台、易读 |
| `settings/` 与 `config/` 分离 | App 级偏好 vs 协议级配置 |
| `Settings` 是唯一 `wxConfigBase` 消费者 | 单一数据源 |
| 用 doctest 做测试框架 | header-only，零配置，编译快 |
| UI 不做单测 | 投入产出比低 |
| UI 自毁场景用 `CallAfter` 延迟 | 避免访问已释放内存 |

---

## 6. 相关文档

- [分层规则与依赖方向](layers.md)
- [架构决策记录（ADR）](adr/README.md)
- [核心 API](../api/core.md)
- [协议配置文件格式](../../README.md#7-协议配置文件格式)
