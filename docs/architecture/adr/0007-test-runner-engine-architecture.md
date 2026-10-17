# 0007. 通用自动化测试引擎架构

- **状态**：提议
- **日期**：2026-10-18
- **决策者**：项目所有者

## 背景

当前工具仅支持手动收发报文，无法满足工业现场的自动化回归测试需求。我们需要一种机制，能够按照预设的步骤自动发送指令、接收响应、进行断言校验，并支持提取响应数据供后续步骤使用。

核心挑战在于：**必须对所有协议通用**（TCP 字节流、Modbus 事务型，以及未来的协议），不能只在某种协议里写死测试逻辑。

## 决策

采用 **接口抽象 + 工厂模式 + 全局变量池** 的架构，实现与具体协议解耦的测试引擎。

1. **`ITestStep` 接口（位于 `core/`）**
   所有的测试步骤都实现此接口，包含三个核心动作：
   - `Execute(ISession* session, Variables& vars)`：执行发送动作并获取响应。
   - `Assert(const TestResult& result, ...)`：断言校验。
   - `Extract(const TestResult& result, Variables& vars)`：提取变量供后续使用。

2. **协议工厂（扩展 `IProtocol`）**
   `IProtocol` 增加接口 `CreateTestStep(stepType, params)`。由具体协议（如 `TcpProtocol`、`ModbusProtocol`）负责解析 `nlohmann::json` 参数，并返回具体的 `ITestStep` 实例。

3. **`TestRunner` 调度器（位于 `core/`）**
   完全依赖 `IProtocol`、`ISession` 和 `ITestStep`，不感知底层通信细节。它负责按顺序执行步骤、维护变量池 `std::map<std::string, std::string>`，并抛出执行状态回调给 UI。

4. **变量提取与替换**
   支持类似 Postman 的 `{{variable}}` 语法，在发送前扫描并替换参数。

## 替代方案

| 方案 | 为什么没选 |
|---|---|
| 给每种协议单独写一套执行流程 | 代码大量重复，违背“协议可插拔”原则 |
| 引入 Lua 等脚本引擎驱动测试 | 学习成本高，增加第三方依赖，破坏项目现有的轻量级设计 |
| 在 UI 层实现循环与断言 | UI 层会变得极其臃肿，且无法复用于 CLI 工具 |

## 后果

**好处**
- 实现了真正的多协议自动化测试，新增协议只要实现 `CreateTestStep` 即可拥有测试能力。
- `TestRunner` 完全无 UI 依赖，为后续无头 CLI 提供了即插即用的核心引擎。
- 交互模式和自动模式可复用同一套底层逻辑。

**代价 / 限制**
- 步骤参数使用 `nlohmann::json` 传递，可能在使用点需要做轻微类型转换。
- 需确保 `TestRunner` 的断言逻辑足够健壮，能处理超时等异步错误。
