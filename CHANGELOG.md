# 变更日志

本文件记录本项目所有值得注意的变更。

格式参考 [Keep a Changelog](https://keepachangelog.com/zh-CN/1.1.0/)，
版本号遵循 [语义化版本](https://semver.org/lang/zh-CN/)。

版本号在 `CMakeLists.txt` 的 `project(... VERSION x.y.z)` 里维护，
构建时由 `cmake/version.h.in` 生成 `ICU_VERSION_STRING` 等宏。

## [未发布]

### 已完成（阶段 28 运行控制、步骤详情与弹出框修复）

- **进度行整合**：预计/执行/剩余时间与进度条放在同一行，新增剩余时间 `预计 = 全部步骤等待 + 额外等待`，执行时间每 500ms 刷新。
- **步骤详情列**：步骤表格新增“详情”列，显示动作、变量、目标值、等待时间和断言摘要。
- **右键菜单位置修复**：上下文菜单改用鼠标所在位置的客户区坐标，修复菜单整体偏移到窗口外的问题。
- **运行控制**：运行中右键步骤可暂停/继续、停止、上一步、下一步、跳转至指定步骤；`TestRunner` 增加暂停、恢复和步骤跳转能力。

### 已完成（阶段 27 布局瘦身、日志滚动与步骤定位）

- **左侧布局瘦身**：主分割条固定在约 300px，左侧变量表由 4 列精简为“名称 / 类型 / 值”3 列，释放右侧操作与日志空间。
- **移除重复开关**：HEX 显示、时间戳、自动滚动不再出现在会话页，统一由“软件设置”控制。
- **日志自动滚动修复**：日志表格在追加、从隐藏变为显示、以及重新开启自动滚动时都会滚到底部。
- **步骤自动定位**：开始运行自动切换到对应的自动/交互页面，当前步骤获得焦点并滚动到可见位置，重试同一步也会重新定位。

### 已完成（阶段 26 进度细节与通信日志重设计）

- **进度条细化**：百分比改为两位小数并绘制在进度条内部；`当前/总数` 移到进度条外；进度条固定宽度，保留留白和重试/停止按钮位置。
- **时间统计**：新增“预计/执行”时间，预计时间按全部步骤 `wait_ms` 之和加上额外等待时间计算，执行时间实时刷新，显示为 `mm分ss秒`。
- **通信日志表格**：日志改为“时间 / 方向 / 内容”三列表格；发送使用蓝色徽标、接收使用绿色徽标、错误使用红色徽标，行背景交替；HEX 以次要文字显示在内容右侧。
- **日志开关**：HEX 显示、时间戳、自动滚动三个开关现在会立即作用于日志表格。
- **消息去噪**：测试步骤的 `>>> / => / <<< / !!!` 日志分别映射为发送/接收/错误方向，不再把方向符号直接显示在文本里。

### 已完成（阶段 25 时间范围、统一对话框与内联失败处理）

- **可编辑时间范围**：实时图表和历史查询改为“数字输入框 + 单位下拉框（秒/分钟/小时/天）”，可直接输入数字，也可用上下箭头调整。
- **统一对话框**：新增主题化的消息框、确认框和多选列表对话框，替换全部 `wxMessageBox` / `wxMultiChoiceDialog`；录制变量选择也改为统一的自绘样式。
- **失败内联处理**：步骤失败时不再弹出模态框，而是在进度区显示“重试 / 停止”按钮并等待操作；点击后继续或结束流程。
- **测试进度**：自动/交互模式标题精简为“测试进度”，使用主题化圆角进度条，百分比和 `当前/总数` 直接绘制在进度条内部。
- **步骤表格**：当前步骤只保留单选高亮并自动滚动；待执行、运行、通过、失败、跳过使用与主题一致的低饱和度配色。

### 已完成（阶段 24 图表、日志与测试执行可视化）

- **快捷时间段**：实时图表和历史查询都增加 `1s / 5s / 10s / 30s / 1min / 5min / 10min / 1h / 24h` 下拉选择；历史查询选择后自动填充起止时间。
- **执行进度**：自动/交互模式顶部增加进度条与“正在执行 / 已完成 / 失败 / 已跳过”计数；当前步骤自动高亮、选中并滚动到可见位置。
- **日志时间戳**：统一为 `[YYYY-MM-DD HH:MM:SS]`，不再显示毫秒。
- **趋势图**：日期只在图表顶部显示一次，X 轴刻度只显示 `HH:MM:SS`；刻度数量按宽度自适应，避免重叠；当前窗口长度显示在图表右上角。
- **状态栏**：接收/发送/客户端统计独立成一行右对齐，避免和目标地址挤在一起被裁剪。

### 已完成（阶段 23 自动测试写入、历史查询与批量对话框修复）

- **自动写步骤**：TCP 写步骤现在优先使用步骤里配置的目标值，不再误用变量当前值；写步骤不再提前消费后续读步骤需要的响应，等待时间由 Runner 统一处理。MQTT 发布步骤同样改为使用步骤目标值。
- **读步骤判断**：TCP 读步骤超时不再静默复用旧值，而是明确失败；响应按变量类型解码（int/uint/float/string/hex 等）后再断言，数值断言支持 `==`、`!=`、`>`、`<` 与容差比较。Modbus 读断言也同步支持这几种比较方式。
- **状态同步**：自动步骤更新变量后，手动操作区的值和提示会同步刷新，不再残留“请输入一个值”的旧提示。
- **历史查询**：未保存的会话统一使用 `Untitled` 作为项目键，修复历史查询把空路径当作项目路径导致“没有数据”的问题；数据库增加 busy timeout，并在首次记录时补建 run，避免多会话下记录丢失；查询增加起止时间校验。
- **批量生成对话框**：改为与其它对话框一致的自绘标题栏 + 卡片 + 扁平按钮样式。
- **测试**：新增 TCP 写/读步骤测试、数据库历史查询测试；全部测试通过。

### 已完成（CI 修复）

- **wxWidgets 构建**：CI 补充 `make` / `mingw32-make` / `pkgconf`，改为 in-source 构建并启用 `--disable-dependency-tracking`，修复 bundled libtiff 在 `config.status` 阶段的依赖跟踪失败；缓存键加入 wx 版本号。
- **ModbusLib 编译**：为 `modbus` 目标强制包含 `<algorithm>`，修复干净检出的 CI 上 `std::find` 编译失败。
- **缓存与矩阵**：wx / Paho 缓存按构建类型区分，编译完成后立即保存；矩阵改为 `fail-fast: false`，避免一个配置失败取消另一个。
- **格式检查**：改用 `actions/setup-python` + `pip install clang-format==19.1.5`，在 Git Bash 中执行 `scripts/format.sh check`。

### 已完成（阶段 22 测试执行与操作增强）

- **输入校验**：手动操作区的值按变量类型校验（int/uint/float/bool/hex/string），非法输入显示红色错误并禁用发送/写入；测试步骤编辑和实际下发也会再次校验，避免把非法数据发到设备。
- **等待修复**：Modbus / MQTT 步骤的 `wait_ms` 现在会在步骤执行后真正等待；TCP 继续使用 `wait_ms` 作为响应等待时间，不再重复等待。
- **失败处理**：断言失败或执行失败时，自动模式与交互模式都会暂停并询问“重试 / 跳过 / 结束”，不再直接结束整个流程；跳过的步骤会标记为“已跳过”并继续后续步骤。
- **执行状态**：步骤列表实时显示“运行中 / 通过 / 失败 / 已跳过”，并输出重试与跳过日志。
- **列表便捷操作**：变量和测试步骤支持双击编辑、右键菜单（添加/复制/编辑/删除/批量/上移/下移）以及鼠标拖动排序。
- **变量顺序持久化**：变量显示顺序写入配置文件 `session.variable_order`，重新打开后保持。
- **测试**：新增 `ValueValidator`、`TestRunner` 单元测试；补充变量顺序持久化测试；全部测试通过。

### 已完成（阶段 21 工作台 UI 稳定性与体验打磨）

- **页面挂载修复**：`TabBook` 现在会把页面 `Reparent` 到 `wxSimplebook` 名下，修复了欢迎页链接、会话页工具栏整页收不到鼠标事件、页面压住标签栏的问题。
- **标签关闭修复**：标签标题刷新改用 `TabItem::SetTitle()`，不再销毁并重建子控件；关闭按钮的点击绑定不会再丢失。
- **欢迎页可关闭**：欢迎页标签可以正常关闭；只剩一个页面时自动隐藏关闭按钮。
- **标签背景修复**：标签文字和关闭按钮的背景色跟随标签的激活/未激活状态，消除文字后的白块。
- **对话框层级**：所有自绘对话框补齐 `wxRESIZE_BORDER`，获得系统边框与投影，与日志窗口保持一致的分层效果。
- **崩溃修复**：语言选择对话框补上 `m_radio` 赋值，修复点击“确定”时的空指针崩溃。
- **翻译更新**：重新提取 `.pot`，补齐中英文 `.po`；构建时由 CMake 调用 `msgfmt` 自动生成 `.mo` 并复制到输出目录，不再依赖手工维护的二进制文件。

### 已完成（阶段 20 工作台 UI 重构）

- **自绘工作台**：主窗口改为自绘标题栏 + `File` / `编辑` 下拉菜单 + 右上角快捷图标 + 底部状态栏。
- **自定义标签栏 `TabBook`**：使用自绘标签条 + `wxSimplebook` 替代 `wxAuiNotebook`，支持多标签、关闭、标题刷新。
- **会话内工具栏**：`SessionPanel` 顶部改为原生 `wxBitmapButton` 工具栏，按会话状态和选中上下文显式刷新图标与可用状态，移除 `wxEVT_UPDATE_UI` 轮询。
- **欢迎页**：仿 VS Code 的“开始 / 最近打开”布局，可直接新建连接、打开配置或选择最近连接。
- **编辑菜单**：新增 `编辑` 菜单，变量与测试步骤的添加/复制/编辑/删除自动转发给当前标签页。

### 已完成（阶段 19 日志加载与离线查看）

- **日志加载功能**：在 `LogFrame` 工具栏新增“Load File...”按钮。
- **文件选择**：点击后弹出对话框，默认定位到可执行文件同级的 `logs` 目录，允许用户选择 `.log` 或 `.txt` 文件。
- **离线阅读**：加载历史日志文件时，自动关闭“自动滚动”，防止实时日志覆盖阅读内容。

### 已完成（阶段 18 快捷键与 UX 优化）

- **列表操作快捷键**：在变量池和测试步骤列表上，支持原生的键盘操作。
  - `Ctrl+N`：新建。
  - `Ctrl+C` / `Ctrl+V`：复制 / 粘贴（当前等同于复制插入）。
  - `Ctrl+E`：编辑。
  - `Del`：删除。
  - `↑` / `↓`：列表项上下切换。
- **零业务逻辑重复**：快捷键直接模拟按钮点击事件，完全复用现有的增删改查逻辑。

### 已完成（阶段 17 多会话并发架构）

- **多标签页架构**：将主窗口升级为 `wxAuiNotebook` 多标签页模式，支持同时打开、管理和调试多个独立连接。
- **标签页关闭**：标签页自带 `×` 按钮，并支持拖拽排序。
- **上下文隔离**：每个 Tab 包含独立的 `SessionPanel`、`ISessionPtr` 和 `SessionConfig`，互不干扰。
- **焦点联动**：全局菜单和标题栏自动跟随当前激活的 Tab 切换。
- **动态重配置**：在标签页内修改配置时，会自动销毁旧会话并重建新会话。

### 已完成（阶段 16 MQTT 模式细化）

- **MQTT 模式拆分**：将原有单一 MQTT 模式细化为 `Publisher`（发布者）、`Subscriber`（订阅者）和 `Broker`（代理者）三种模式。
- **字段与 UI 隔离**：
  - Publisher 模式：仅提供连接与发布所需字段。
  - Subscriber 模式：提供连接、订阅主题与 QoS 字段，界面隐藏发送按钮。
  - Broker 模式：提供监听端口，UI 自动调整为服务端形态。
- **底层会话解耦**：新增 `MqttBrokerSession` 占位；重构 `MqttSession`，根据模式标志位严格控制发布和订阅行为。
- **翻译更新**：补充新增模式及相关 UI 控件的 `.po` 翻译。

### 已完成（阶段 15 TCP/IP 协议统一）

- **重构 TCP 协议**：将原有的 `TcpProtocol` 升级为 `TcpIpProtocol`，内部 ID 保持 `tcp`，显示名称保持 `TCP/IP`。
- **支持 UDP 协议**：在 `TCP/IP` 协议内部增加 `UDP` 模式，底层新增 `UdpTransport`（基于 `wxDatagramSocket`）支持无连接的数据报收发。
- **模式与角色统一**：参照 Modbus 的设计，采用 `Mode` 区分 `TCP` 和 `UDP`，采用 `role` 字段（Client / Server）区分角色。
- **UI 自动适配**：移除多余的独立菜单项，统一通过 `TCP/IP` 入口进入，界面字段根据 Mode 动态生成。
- **向后兼容**：旧的 `.json` 配置文件在加载时会自动推断角色，无需手动修改。

### 已完成（阶段 14 发布准备）

- **CPack 与 NSIS 打包**：引入 CPack，支持一键生成免安装 `.zip` 包和 Windows 标准安装程序 `.exe`。
- **打包路径与图标优化**：修复了由于工程路径含 `C++` 导致 `windres` 解析资源失败的问题；通过强制生成 `manifest` 和 `app.rc` 到构建目录并指向相对路径，确保了程序图标、快捷方式图标和系统托盘图标的一致且不弹窗。
- **安装路径优化**：NSIS 默认将程序安装到 `%LOCALAPPDATA%\Programs\IndComm`，避免了 `C:\Program Files` 因权限不足导致无法写入日志（`logs`）的崩溃。
- **GitHub Actions**：配置 CI 流水线，在打标签时自动构建、打包并发布到 GitHub Releases。

### 已完成（阶段 13 应用设置完善）

- **全局设置中心**：新增 `ui/SettingsDialog` 窗口，提供“通用”与“界面与日志”分类选项卡。
- **偏好持久化**：
  - 将界面设置（字体大小、默认 HEX 显示、自动滚动、显示时间戳）提取到 `settings::UISettings`。
  - 将“关闭窗口时最小化到系统托盘”行为纳入配置持久化。
- **即改即生效**：引入全局设置变更回调，用户在 `SettingsDialog` 中点击“应用”后，已打开的 `SessionPanel` 会实时同步新的显示偏好。
- **应用图标统一**：引入多尺寸 `.ico` 文件合并方案，配置 Windows 资源脚本（`app.rc`），使 `IndComm.exe` 文件图标、任务栏图标与托盘图标完全统一且高清。

### 已完成（阶段 12 MQTT 协议接入）

- **MQTT 协议支持**：引入 Eclipse Paho MQTT C++ 库（通过独立脚本编译为静态库）。
  - 新增 `MqttProtocol`，注册到 `ProtocolRegistry`。
  - 新增 `MqttSession`，封装 Paho 异步客户端，支持自动重连和 QoS。
  - 新增 `MqttPublishStep`，支持通过测试步骤发送 MQTT 消息。
  - 支持通过 `VariableParamsFields()` 动态配置 Topic 与 QoS。
  - 接收回调通过全局日志窗口实时展示。

### 已完成（阶段 11 SQLite 持久化与可视化及自动化增强）

- **SQLite 数据持久化与可视化**（阶段 11）
  - 引入 SQLite3（通过 submodule 引入），新增 `core/Database.h/.cpp` 封装。
  - 建立 `test_runs` 和 `run_data` 数据表，自动记录每次测试运行与变量数据。
  - 新增 `ui/ChartPanel.h/.cpp`，基于 `wxGraphicsContext` 自绘多变量实时折线图，支持滚轮缩放与拖拽平移。
  - 新增 `ui/HistoryDialog.h/.cpp`，实现独立的历史查询窗口，支持按时间段回放。
- **自动化测试增强**（阶段 11）
  - 动作彻底统一：所有协议（TCP/串口/Modbus/MQTT）的测试步骤动作全部统一为 `read` / `write` / `delay`。
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
- **配置文件**：`config/ConnectionProfile.h/.cpp`（`.json` INI 格式，协议无关，含 Mode 名回退）
- **设置模块（阶段 4.1~4.2）**：`settings/`
  - `Settings` 是唯一直接访问 `wxConfigBase` 的类
  - `LanguageSettings`（中文 / 英文，GNU gettext）、`UISettings`（窗口几何、最近连接、UI 开关）
  - `FontSettings` / `ThemeSettings` / `IconSettings` 预留接口
- **日志**：`log/Log.h/.cpp`，spdlog + GUI sink；`ICU_SPDLOG_*` / `SPDLOG_*` 宏，日志文本全部可翻译
- **主窗口**：`app/MainFrame.h/.cpp`，菜单、`wxSimplebook` 页面切换、`.json` 打开/保存/另存/关闭、
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
  - 新增 `third_party/sqlite`。
  - 新增 `third_party/paho.mqtt.c`。
  - 新增 `third_party/paho.mqtt.cpp`。
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
