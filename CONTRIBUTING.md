# 贡献指南

感谢参与。本项目有一份「文档即契约」的约定：根 `README.md` 既是开发文档，也是 AI 辅助开发的
Skill 定义，**架构级改动必须先更新文档，再改代码**。

---

## 1. 开始之前

1. 读根 [`README.md`](README.md) —— 尤其是第 2 节（核心设计原则）、第 5 节（目录结构与分层）、
   第 10 节（编码约定）、第 14 节（关键决策记录）。
2. 读 [`docs/architecture/overview.md`](docs/architecture/overview.md) 与
   [`docs/architecture/layers.md`](docs/architecture/layers.md)。
3. 拉取第三方依赖（两个 submodule）：
   ```bash
   git submodule update --init third_party/wxWidgets third_party/spdlog
   ```
   > 不要用 `--recursive`：wxWidgets 自身带 12 个嵌套 submodule，本项目两个都不需要，
   > 全拉会慢很多。**也绝对不要加 `--force`**，它会删掉你编译好的
   > `third_party/wxWidgets/lib/`。
4. 按 [`docs/developer/building.md`](docs/developer/building.md) 把环境搭起来并构建成功
   （**必须先编译 wxWidgets**，它的编译产物不进仓库）。

---

## 2. 铁律

| 规则 | 原因 |
|---|---|
| **协议是一等公民**：客户端/服务器是协议内部的 Mode，不是两个协议 | 见 [ADR 0001](docs/architecture/adr/0001-protocol-as-first-class-with-modes.md) |
| **新增协议不改 UI** | UI 由 `FieldDescriptor` 数据驱动，见 [ADR 0002](docs/architecture/adr/0002-ui-data-driven-by-field-descriptors.md) |
| **依赖单向**：`app → ui → protocols → core`，`core/` 与 `protocols/` 不得 include `ui/` 或 `settings/` | 分层解耦 |
| **`Settings` 是唯一访问 `wxConfigBase` 的类** | 单一数据源 |
| **用户可见字符串必须走 `_()`**；日志用 `SPDLOG_*`（无参数）/ `ICU_SPDLOG_*`（有参数） | i18n |
| **测试代码不翻译**（不用 `_()`） | 开发者向 |
| **UI 自毁场景用 `CallAfter` 延迟** | 避免访问已释放内存导致闪退 |
| **改动分层要正确**：新协议放 `protocols/<id>/`，新设置放 `settings/` | 见 [layers.md](docs/architecture/layers.md) |

完整禁忌列表见根 `README.md` 第 10.8 节。

---

## 3. 开发流程

```bash
# 1. 构建（含测试）
scripts/build.sh

# 2. 写代码 / 写测试

# 3. 跑测试
scripts/test.sh

# 4. 格式化（必须，CI 会检查）
scripts/format.sh

# 5. 静态检查（可选，需要 clang-tidy）
clang-tidy -p build src/core/Session.cpp
```

### 提交前自检

```bash
# 分层违规（应无输出）
grep -rn '#include.*ui/\|#include.*settings/' src/core/ src/protocols/

# 绕过 settings 访问 wxConfigBase（应无输出）
grep -rn "wxConfigBase" src/ | grep -v src/settings/

# 用户可见字符串漏翻译（应无输出）
grep -rn 'SPDLOG_\(INFO\|WARN\|ERROR\|DEBUG\)("' src/ | grep -v '_('

# 格式
scripts/format.sh check

# 测试
scripts/test.sh
```

---

## 4. 代码风格

- **格式化**：`.clang-format`（Google 基础，4 空格缩进，列宽 100）。
  **用 clang-format 19.1.5**（与 CI 一致），其他版本可能产生不同结果。
  ```bash
  scripts/format.sh         # 就地格式化
  scripts/format.sh check   # 只检查
  ```
- **`wxBEGIN_EVENT_TABLE` 区域**必须用 `// clang-format off` / `// clang-format on` 包住：
  宏展开后是一条语句，clang-format 会把后续行的缩进逐级累积。
  现有的 4 个事件表（`MainFrame` / `LogFrame` / `GenericProtocolDialog` / `AppTaskBarIcon`）
  已经处理过，新增时照做。
- **头文件**：`#pragma once`；不 include 自己；能用前向声明就用前向声明。
- **命名**：成员变量 `m_` 前缀；常量 `k` 前缀；接口类 `I` 前缀（`IProtocol`、`ITransport`）。
- **字符串字面量**：源文件 UTF-8，无 BOM。CMake 已强制
  `-finput-charset=UTF-8 -fexec-charset=UTF-8`。

---

## 5. 提交规范

提交信息用**祈使句、中文或英文都可**，第一行不超过 72 字符，说明「做了什么」而不是「改了个啥」：

```
修复 Session 重连后未清空 Codec 缓冲区

Transport 触发 Reconnecting 时没有调用 ICodec::Reset()，
导致上一轮残留的半包与新数据拼接。

- Session::HandleTransportState 在 Reconnecting 分支调用 m_codec->Reset()
- 补 test_Session.cpp 用例覆盖该路径
```

一个提交只做一件事。格式化产生的巨大 diff 建议**单独提交**，便于 review。

---

## 6. 新增功能要带什么

| 改动类型 | 必须附带 |
|---|---|
| 新协议 | `tests/unit/protocols/<id>/` 单测、`.po` 翻译、更新根 README 第 8 节协议清单 |
| 新设置项 | 更新根 README 第 6.5 节设置项清单、[新增设置](docs/developer/adding-a-setting.md) 检查清单 |
| 新框架接口 | **先**更新 `docs/api/core.md` 与根 README 第 4 节 |
| 架构级决策 | 新增一条 ADR（`docs/architecture/adr/`），见 [ADR 索引](docs/architecture/adr/README.md) |
| 修 bug | 尽量补一个能复现该 bug 的测试 |

---

## 7. CI

每次 push / PR 会跑（`.github/workflows/build.yml`）：

| Job | 内容 |
|---|---|
| `wxWidgets 3.3.5` | 从源码构建 wxWidgets 并缓存（`third_party/wxWidgets/lib` 被 gitignore，CI 必须自己编） |
| `build (Debug)` | 配置 + 编译 + `ctest` |
| `build (Release)` | 同上 |
| `clang-format` | 格式检查，有 diff 就失败 |

本地没跑过的格式问题会被 CI 拦下来，提交前先 `scripts/format.sh check`。

---

## 8. 报告问题

提供这些信息：

1. 复现步骤
2. 期望行为 vs 实际行为
3. `logs/app.log`
4. 相关的 `.conn` 文件
5. `g++ --version`、`cmake --version`

见 [故障排查](docs/user/troubleshooting.md)。

---

## 9. 许可证

本项目采用 [MIT 许可证](LICENSE)。提交代码即表示你同意以该许可证发布你的贡献。
