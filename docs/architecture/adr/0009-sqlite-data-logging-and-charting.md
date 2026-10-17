# 0009. 基于 SQLite 的数据记录与实时图表可视化

- **状态**：提议
- **日期**：2026-10-18
- **决策者**：项目所有者

## 背景

在执行自动化测试时，会产生海量的运行数据（如随时间变化的温度、电流数值）。单纯的文本日志不仅难以直观发现趋势（如数值波动、缓慢漂移），也无法做长周期的回归对比。

此外，测试配置是以 `.json` 文件为单位的，如何将数据与特定的 `.json` 配置文件关联起来也是一个必须解决的问题。

## 决策

**引入 SQLite 数据库进行数据持久化，并在 UI 中集成图表可视化。**

1. **数据关联设计**
   使用 `.json` 配置文件的**绝对路径**作为唯一关联键。在数据库中建立 `test_runs` 表，记录每次运行的 `id`、`project_path`、`start_time` 等。

2. **数据库结构 (SQLite)**
   ```sql
   CREATE TABLE test_runs (
       id INTEGER PRIMARY KEY AUTOINCREMENT,
       project_path TEXT,
       start_time DATETIME,
       status TEXT
   );
   CREATE TABLE run_data (
       id INTEGER PRIMARY KEY AUTOINCREMENT,
       run_id INTEGER,
       timestamp DATETIME,
       variable TEXT,
       value REAL,
       FOREIGN KEY(run_id) REFERENCES test_runs(id)
   );
   ```

3. **数据提取与记录**
   `TestRunner` 增加 `OnDataExtracted(variableName, value)` 回调。UI 层接收到回调后，将数据插入数据库，并实时推送到图表组件。

4. **UI 图表展示**
   在 `SessionPanel` 的下方增加 `wxNotebook`。新增“数据图表”页签。
   - **实时图表**：使用 `wxGraphicsContext` 手绘实时折线图。
   - **历史回放**：根据当前 `.json` 文件的路径查询数据库，展示历史运行的数据对比。

## 替代方案

| 方案 | 为什么没选 |
|---|---|
| 用 CSV 文件记录数据 | 查询和过滤能力极弱，难以处理“按项目路径查找历史”的需求。 |
| 用重型图表库（如 QCustomPlot） | 破坏 wxWidgets 的生态依赖，增加编译和打包难度。 |
| 仅生成 HTML 报告 | 无法实现“实时曲线”的交互体验，用户必须等测试完全跑完才能看结果。 |

## 后果

**好处**
- 将工具从单纯的“日志打印”升级为“数据可视化仪表盘”。
- 测试数据资产化，方便追溯历史问题。
- 实时曲线让工业现场的数据趋势一目了然。

**代价 / 限制**
- 增加对 SQLite3 的依赖。
- 用 wxGraphicsContext 自绘支持缩放、平移的折线图有一定的开发量。
- 数据库写入需要放在后台线程或非阻塞队列，避免拖慢 `TestRunner` 的执行。
