# 变更日志

本文件记录本项目所有值得注意的变更。

格式参考 [Keep a Changelog](https://keepachangelog.com/zh-CN/1.1.0/)，
版本号遵循 [语义化版本](https://semver.org/lang/zh-CN/)。

版本号在 `CMakeLists.txt` 的 `project(... VERSION x.y.z)` 里维护，
构建时由 `cmake/version.h.in` 生成 `ICU_VERSION_STRING` 等宏。

## [未发布]

### 计划中

- 阶段 12：更多协议（MQTT 等）
- 阶段 13：应用设置完善（字体 / 主题 / 图标，`ui/SettingsDialog`）
- 阶段 14：发布准备（打包、安装程序）

### 已完成（阶段 10 及阶段 11）

- **SQLite 数据持久化与可视化**（阶段 11）
  - 引入 SQLite3（通过 submodule 引入），新增 `core/Database.h/.cpp` 封装。
  - 建立 `test_runs` 和 `run_data` 数据表，自动记录每次测试运行与变量数据。
  - 新增 `ui/ChartPanel.h/.cpp`，基于 `wxGraphicsContext` 自绘多变量实时折线图，支持滚轮缩放与拖拽平移。
  - 新增 `ui/HistoryDialog.h/.cpp`，实现独立的历史查询窗口，支持按时间段回放。
- **自动化测试增强**（阶段 11）
  - 动作彻底统一：所有协议（TCP/串口/Modbus）的测试步骤动作全部统一为 `read` / `write` / `delay`。
  - 断言符号化与完善：测试步骤的断言从 `eq/gt/lt` 更新为 `==/!=/>/</regex`，并在底层真正生效。
  - 步骤编辑联动：`TestStepEditDialog` 根据选择的 Action 自动显示/隐藏无关的输入控件。
  - 支持步骤的“批量生成”：选定一个基准步骤后，可设置重复次数、间隔时间和自增/自减步长，一键生成连续测试步骤。
  - 变量池支持多选与批量操作：左侧变量列表支持通过复选框启用/禁用，测试列表支持 Ctrl/Shift 多选删除。
- **通讯变量与类型统一**（阶段 11）
  - 新增 `core/BytesParser.h`，统一处理各种类型（int8/16/32、uint8/16/32、float、double、hex 等）的二进制解析。
  - 新增 `core/DelayStep.h`，提供原生的延时测试步骤。
- **UI/UX 深度重构**（阶段 10）
  - 引入 `wxSplitterWindow`，左侧固定展示全局通讯变量池，右侧为多模式操作区与统一日志区。
  - 移除底部的通用发送框，所有收发均围绕唯一的 `CommVariable` 的 `Value` 展开，通过 `RO/RW/WO` 严格限制权限。
  - 执行步骤时，底层交互报文自动同步到统一日志窗口。
  - 新建 `TestStepEditDialog` 和 `VariableEditDialog`，彻底废除 JSON 文本弹窗编辑。

### 修复

- 修复 `TcpSendStep` 在等待接收数据时因局部变量销毁导致的 Use-After-Free 崩溃。
- 修复 `ModbusSession` 在 CLI 模式下（无 `wxApp`）因 `wxTheApp->CallAfter` 导致的潜在崩溃。
- 修复 `build.sh` 和 `coverage.sh` 中过时的 wxWidgets 路径检查逻辑，兼容标准布局与老式布局。
- 修正脚本输出中的可执行文件名提示（由旧名 `IndustrialCommunicationUtility.exe` 改为 `IndComm.exe`）。
- 修复测试环境的 DLL 缺失（`0xc0000135`），确保 `ctest` 全部通过。
- **i18n 全量清理**：修复 `zh_CN.po` 和 `en_US.po` 中所有的 fuzzy 标记与错误翻译，补充所有新增 UI 控件的翻译，确保界面无英文残留。

## [0.1.0] - 2026-10-14

首个可运行版本：协议可插拔框架 + TCP 协议 + 数据驱动 UI + 测试与工程化配套。

### 新增

- **协议抽象层（阶段 1）**：`core/IProtocol.h`、`core/ITransport.h`、`core/ICodec.h`
  - 协议是一等公民：客户端/服务器是协议内部的 Mode，不是两个协议
- **协议注册表**：`core/ProtocolRegistry.h/.cpp`，`RegisterBuiltinProtocols()` 注册 TCP
- **会话层**：`core/Session.h/.cpp`（Transport + Codec 组合、状态机、重连）、
  `core/SessionManager.h/.cpp`（单活动会话生命周期）
- **TCP 协议**（阶段 2）：`protocols/tcp/TcpProtocol.h/.cpp`
  - Client / Server 两种 Mode，字段由 `FieldsForMode()` 描述
  - `protocols/tcp/TcpTransport.h/.cpp`：`TcpClientTransport`（连接超时 + 自动重连退避）、
    `TcpServerTransport`（多客户端、广播、`SendTo`）
- **编解码**：`protocols/codec/RawCodec.h`（透传，header-only）
- **会话面板通用化（阶段 3）**：`ui/ProtocolSessionPanel.h/.cpp`
- **数据驱动 UI**：`ui/GenericProtocolDialog.h/.cpp`，从 `FieldDescriptor` 列表自动生成控件；
  新增协议不需要改动任何 UI 代码
- **配置文件**：`config/ConnectionProfile.h/.cpp`（`.conn` INI 格式，协议无关，含 Mode 名回退）
- **设置模块（阶段 4.1~4.2）**：`settings/`
  - `Settings` 是唯一直接访问 `wxConfigBase` 的类
  - `LanguageSettings`（中文 / 英文，GNU gettext）、`UISettings`（窗口几何、最近连接、UI 开关）
  - `FontSettings` / `ThemeSettings` / `IconSettings` 预留接口
- **日志**：`log/Log.h/.cpp`，spdlog + GUI sink；`ICU_SPDLOG_*` / `SPDLOG_*` 宏，日志文本全部可翻译
- **主窗口**：`app/MainFrame.h/.cpp`，菜单、`wxSimplebook` 页面切换、`.conn` 打开/保存/另存/关闭、
  最近连接（上限 5）、修改标记、关闭最小化到托盘（`ui/AppTaskBarIcon`）
- **i18n**：`resources/i18n/`（zh_CN / en_US），构建时自动 `xgettext` → `msgmerge` → `msgfmt`
- **测试（阶段 4.5）**：doctest 2.4.11
  - `tests/unit/`：`FieldValue`、`ActiveProfile`、`ProtocolRegistry`、`Session`、`SessionManager`、
    `MockTransport`/`MockCodec`、`RawCodec`
  - `tests/integration/`：`ConnectionProfile` roundtrip
  - `tests/mocks/`：`MockTransport`、`MockCodec`（header-only）
  - `tests/helpers/TestEnv.h`：临时目录 RAII
  - 共 91 个 test case / 300 个 assertion
- **工程化（阶段 4.6）**：
  - GitHub Actions CI：wxWidgets 构建缓存 + Debug/Release 构建 + ctest + clang-format 检查
  - `.clang-tidy` 配置
  - `cmake/version.h.in` + `project(VERSION)` 版本宏
  - `docs/`（架构、ADR、用户手册、开发者文档、API）
  - `scripts/`（构建、测试、覆盖率、格式化、i18n）
  - `LICENSE`（MIT）、`CONTRIBUTING.md`
- **阶段 7 完成：Modbus 协议（TCP / RTU / ASCII）**
  - 引入 `third_party/ModbusLib`，实现标准的 Modbus TCP 通信。
  - 完整支持 01/02/03/04/05/06/0F/10 功能码的读写操作。
  - 新增 `core/ReconnectConfig.h`：标准化重连配置（次数、延迟），所有协议共享。
  - 新增 `core/SessionStateMachine.h`：统一会话状态机，集中处理状态流转与重连计数。
  - 新增 `core/ISession.h` 和 `core/ITransactionSession.h`：统一字节流与事务型协议的底层接口。
  - 新增 `ui/SessionPanel.h/.cpp`：统一的会话面板，通过 `OperationFieldsForMode` 自动生成事务型表单。
  - 后台心跳线程：Modbus 连接后自动发起探测请求，实现掉线秒级感知与自动重连。
- **自动化测试引擎（阶段 8）**
  - 引入 `ITestStep` 与 `TestRunner`，实现自动化测试步骤调度。
  - `SessionConfig` 支持三种独立模式：手动（Manual）、交互（Interactive）、自动（Auto）。
  - 引入“通信变量（CommVariable）”概念，支持定义类型、读写权限（RO/RW）和协议专属参数。
  - UI 彻底重构为 `wxNotebook` 选项卡布局，支持变量的增删改查与动态参数面板。
  - 测试步骤可视化编辑，支持动态选择变量，支持变量池的 `{{var}}` 引用与提取。
  - 交互模式支持单步阻断执行，自动模式支持一键跑通。
- **独立 CLI 工具（阶段 9）**
  - 重构 CMake 架构，抽离核心静态库 `icu_core_lib`，实现 GUI 与 CLI 双端复用。
  - 新增 `src/cli/main.cpp`，编译生成 `indcomm-cli.exe`。
  - CLI 支持 `-c` 加载配置，`-r` 自动运行，`-i` 交互单步运行，`--read` / `--write` 直接读写变量。
  - 分离 GUI 日志 Sink 到 `ui/LogSink`，确保 CLI 端编译不依赖 GUI 库。

### 变更

- **第三方依赖统一为 submodule**：
  - `third_party/wxWidgets` 从「被提交的普通目录」改为真正的 submodule，
    固定到上游 `469921b9`（`master` 上的 3.3.5 开发快照）。
  - `third_party/spdlog` 补上 `.gitmodules` 映射并固定到 `f544fce0`。
  - 新增 `third_party/json`（`nlohmann/json`）。
  - 新增 `third_party/ModbusLib`。
  - `third_party/sqlite`（阶段 11 新增）。
  - 所有 submodule 都标记 `shallow = true`。
- **CI（4.6.1）修正**：wxWidgets 自带 12 个嵌套 submodule，构建需要其中一部分，
  因此 CI 使用 `submodules: recursive`，并补充了 `src/zlib` 非空的校验。
- **目录重组（阶段 4.3）**：源码按 `app/` `core/` `protocols/` `ui/` `settings/` `config/` `log/` 分层
- **核心类型合并（阶段 4.4）**：`FieldValue` / `FieldDescriptor` / `ActiveProfile`
  合并进 `core/ProtocolTypes.h/.cpp`，删除三个旧头文件
- **代码格式化**：全量套用 `.clang-format`（clang-format 19.1.5）；
  `wxBEGIN_EVENT_TABLE` 宏区域用 `// clang-format off` 保护，避免缩进逐级累积
- **架构重构：彻底统一 UI 与底层会话**
  - 移除 `ui/ProtocolSessionPanel` 与 `ui/ModbusPanel`，合并为通用的 `SessionPanel`。
  - 移除 `TransactionSessionManager`，统一由 `SessionManager` 管理 `ISessionPtr`。
  - `FieldDescriptor` 新增 `group` 属性，支持将相关字段（如“重连设置”）收纳到底部的分割框中。
  - `TcpProtocol` 与 `ModbusProtocol` 全面接入 `ReconnectConfig` 标准化字段。
- **项目更名**：可执行文件由 `IndustrialCommunicationUtility.exe` 精简为 `IndComm.exe`。
- **禁止生成 ModbusLib 示例**：CMake 添加 `EXCLUDE_FROM_ALL`，避免生成多余的 `mbclient.exe` / `mbserver.exe`。
- **配置结构重构**：`.json` 文件严格划分为 `connection` 和 `session` 两大节点，旧的 INI 格式已彻底废弃。

### 修复

- 修正 `MockTransport::Open()` 语义：只切到 `Opening`，对齐真实异步 Transport
- Mock 计数器改用 `shared_ptr<int>`，避免对象销毁后 use-after-free
- `zh_CN.po` 补全空翻译并消除 fuzzy 条目
- Windows 非管理员权限下设置无法初始化：`wxRegConfig` 写 `HKCU` 返回 `0x5`
  拒绝访问，导致启动和退出各弹一次错误框。`Settings` 改用 `wxFileConfig`，
  按 `%APPDATA%` → `%LOCALAPPDATA%` → exe 目录 → 当前目录的顺序探测可写位置，
  探测阶段用 `wxLogNull` 屏蔽 wx 内部弹框，并用写探针验证真实可写。
- 修复保存连接对话框在部分环境（Windows Shell 缩略图缓存或 COM 组件异常）下
  卡在「正在处理…」：保存对话框过滤器从 `*.conn|*.*` 收窄为仅 `*.conn`，
  避免 Shell 枚举目录下所有文件并尝试加载缩略图。同时补 `path` 为空检查和
  `.conn` 扩展名兜底。
- `Session::Send` 的 Codec 编码失败日志补翻译（`SPDLOG_WARN` → `ICU_SPDLOG_WARN`）。

### 已知问题

- 覆盖率报告需要手动 `-DICU_BUILD_COVERAGE=ON` 生成，仓库内不保留产物
- 本地执行 `git submodule update --init --recursive` 可能较慢（会拉 wxWidgets 的 12 个嵌套
  submodule）；只更新本项目需要的两个可用
  `git submodule update --init third_party/wxWidgets third_party/spdlog`

[未发布]: https://github.com/lee-yilong/IndCommUtil/compare/v0.1.0...HEAD
[0.1.0]: https://github.com/lee-yilong/IndCommUtil/releases/tag/v0.1.0
