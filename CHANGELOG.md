# 变更日志

本文件记录本项目所有值得注意的变更。

格式参考 [Keep a Changelog](https://keepachangelog.com/zh-CN/1.1.0/)，
版本号遵循 [语义化版本](https://semver.org/lang/zh-CN/)。

版本号在 `CMakeLists.txt` 的 `project(... VERSION x.y.z)` 里维护，
构建时由 `cmake/version.h.in` 生成 `ICU_VERSION_STRING` 等宏。

## [未发布]

### 计划中

- 阶段 5：串口协议
- 阶段 6：Modbus RTU 协议
- 阶段 7：更多协议（MQTT / Modbus TCP 等）
- 阶段 8：应用设置完善（字体 / 主题 / 图标，`ui/SettingsDialog`）
- 阶段 9：UI 打磨
- 阶段 10：发布准备（打包、安装程序）

### 已完成（阶段 4.7 清理）

- 补齐 `en_US.po` 全部翻译，并清除 fuzzy / untranslated 条目。
- 菜单、模式名与字段标签接入 gettext：`DisplayName()` 运行时翻译；`ModeNames()` 与
  `FieldDescriptor` 保持稳定 key，由 UI 通过 `wxGetTranslation()` 翻译，避免语言影响 `.conn` 持久化值。
- 修复 `tests/CMakeLists.txt` 中失效的 DLL 过滤：按 Debug / Release 只复制匹配的 wxWidgets DLL。
- README 目录树移除不存在的 `tests/fixtures/` 说明。

### 修复

- Windows 下非管理员启动时，`wxFileConfig` 在用户配置目录创建临时文件失败会导致
  can't open user configuration file. 弹窗。应用设置改用 `wxRegConfig`（HKCU 注册表）。

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
### 变更

- **第三方依赖统一为 submodule**：
  - `third_party/wxWidgets` 从「被提交的普通目录」改为真正的 submodule，
    固定到上游 `469921b9`（`master` 上的 3.3.5 开发快照）。
    经逐文件校验，该 commit 的 `include/wx/**` 1447 个文件与仓库原有头文件**完全一致**，
    因此升级过程对代码零影响。
    背景：上游**没有** `v3.3.5` tag（最新官方 tag 是 `v3.3.4`），
    原来的 `.gitmodules` 声明了 submodule 但索引里是普通目录，两者不一致。
  - `third_party/spdlog` 此前是 gitlink 却**没有 `.gitmodules` 映射**，
    导致 `git submodule status` 直接报 fatal、`actions/checkout` 也会失败；
    现补上映射并固定到 `f544fce0`（内容与原有工作区逐文件一致，同样零影响）。
  - 两个 submodule 都标记 `shallow = true`，CI 拉取体积很小（wx 约 20 MB）。
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
