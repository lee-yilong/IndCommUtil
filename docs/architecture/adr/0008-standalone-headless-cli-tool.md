# 0008. 独立无头 CLI 工具 (icu-cli)

- **状态**：提议
- **日期**：2026-10-18
- **决策者**：项目所有者

## 背景

工具的核心价值在于工业现场的自动化测试。如果只能在图形界面下手动点击运行，就无法无缝接入持续集成（CI/CD）流水线（如 GitHub Actions、Jenkins），也无法在无桌面环境的服务器或边缘网关上运行。

我们需要一个无头（Headless）的命令行工具，读取 `.json` 配置文件，自动执行测试并返回结果。

## 决策

**单独编译一个名为 `icu-cli` 的可执行文件**，不包含任何 GUI 代码。

1. **静态库抽离**
   重构 CMake 架构，将 `src/core/`、`src/protocols/`、`src/config/`、`src/log/` 编译为静态库 `icu_core_lib`。
   - `IndustrialCommunicationUtility.exe`：链接 `icu_core_lib` + `src/ui/`。
   - `icu-cli.exe`：只链接 `icu_core_lib`（仅依赖 wxBase 用于字符串处理，不依赖 wxGUI）。

2. **接口设计**
   - 命令格式：`icu-cli.exe --config <path_to_json> [--report <report_file_path>]`
   - CLI 内部实例化 `TestRunner`，执行测试流程。

3. **退出码与输出**
   - 成功执行所有断言：返回 `0`。
   - 出现断言失败或通信异常：返回 `1`。
   - 参数错误：返回 `2`。
   - 标准输出（stdout）实时打印测试步骤与结果，方便 CI 日志记录。

## 替代方案

| 方案 | 为什么没选 |
|---|---|
| 给现有 GUI 程序加 `--headless` 参数 | 每次启动 CLI 都要加载庞大的 wxWidgets GUI 动态库，内存占用高，启动慢；入口代码逻辑耦合。 |
| 用 Python/Shell 重写 CLI 逻辑 | 违背了核心代码只用一套 C++ 实现的原则，增加了跨语言维护成本。 |

## 后果

**好处**
- 满足自动化测试流水线的需求，退出码标准符合 Unix 规范。
- 强制分离核心逻辑与 UI 逻辑，有利于架构的长期健康。
- 命令行工具体积小巧，跨平台部署方便。

**代价 / 限制**
- 必须重构 `CMakeLists.txt` 形成合理的 `add_library(icu_core_lib ...)` 依赖树。
- 涉及日志的初始化需要区分：GUI 模式下输出到文件+窗口，CLI 模式下输出到 stdout。
