# 0003. 用 doctest 作为测试框架

- **状态**：已接受
- **日期**：2026-10-13（引入 ADR 机制后于 2026-10-14 补记）

## 背景

阶段 4.5 要为 `core/` / `protocols/` / `config/` 建立单元与集成测试。项目已经用了
wxWidgets（GUI 库，自带 `wxTest`？实际上没有成熟的测试框架）与 CMake（`enable_testing()` + `ctest`）。

需要选一个 C++ 测试框架，约束是：

- 构建时间要短（开发者要频繁跑）
- 集成成本低（不要引入复杂依赖或额外构建步骤）
- 能在 Windows + MinGW + MSYS2 下工作
- 断言失败信息要能定位到文件行号
- 支持 `ctest` 集成与按用例过滤

## 决策

用 **doctest 2.4.11**（header-only，单头文件提交在 `third_party/doctest/doctest.h`）。

- 实现只在 `tests/main.cpp` 里用一个 TU 展开（`#define DOCTEST_CONFIG_IMPLEMENT`）
- `main()` 里先 `wxInitializer` 初始化 wxBase，再 `wxTranslations::Set(new wxTranslations())`
  提供空翻译系统，保证 `_()` 宏能安全调用（无 catalog 时返回原文）
- 测试文件用 GLOB 自动拾取，新增测试无需改 CMakeLists

## 替代方案

| 方案 | 为什么没选 |
|---|---|
| GoogleTest | 需要单独构建库或引入 FetchContent，构建时间明显更长 |
| Catch2 | header-only 但单头文件极大，编译明显慢于 doctest；功能大多用不上 |
| QtTest | 项目不用 Qt |
| 自研断言宏 + 简单 runner | 功能太少（无 SUBCASE、无过滤、无良好失败输出），维护成本反而高 |
| Boost.Test | 依赖 Boost，重 |

## 后果

**好处**

- 编译快：测试套件（91 个用例）在本机约 24 MB 二进制、增量编译只需数秒
- 零配置：无第三方构建步骤，仓库自带 `doctest.h`
- 支持 `SUBCASE`（用于 `MakeDefaultFileName` 这类多分支用例）与 `--test-case=` 过滤
- `ctest` 直接集成（`add_test(NAME icu_tests COMMAND icu_tests)`）

**代价 / 限制**

- header-only 必须恰好在一个 TU 展开实现，新增可执行文件时要注意
- 需要手动搭好 wxBase 初始化（否则 `_()` 相关的代码路径不可测）
- 不提供 mock 框架，Mock 对象要自己写（`tests/mocks/`，header-only）

**后续必须遵守的约束**

- `tests/main.cpp` 是唯一的 doctest 实现 TU，不要在其他文件定义 `DOCTEST_CONFIG_IMPLEMENT`
- 测试代码不翻译（不用 `_()`），见根 `README.md` 附录 D
- Mock 放 `tests/mocks/`，header-only

## 参考

- 代码：`tests/main.cpp`、`tests/CMakeLists.txt`、`third_party/doctest/doctest.h`
- 文档：根 `README.md` 第 9 节、[测试指南](../../developer/testing.md)
