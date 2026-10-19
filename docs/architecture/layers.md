# 分层规则

> 本文档把「依赖单向」这条原则写成可执行的规则，并给出自检命令。

---

## 1. 允许的依赖方向

```
app/  ──▶  ui/  ──▶  protocols/  ──▶  core/  ──▶  (STL / wxBase)
  │         │            │              │
  │         │            │              ├──▶  config/
  │         │            │              ├──▶  settings/   ⚠ core/ 不得直接依赖 settings/
  │         │            │              └──▶  log/
  │         │            │
  │         └──▶ settings/  core/
  └──▶ config/ settings/ log/ ui/ core/
```

**只允许向下，禁止向上，禁止跨层反向。**

---

## 2. 硬性禁止项

| 禁止 | 原因 |
|---|---|
| `core/` 或 `protocols/` include `ui/` | 框架层不该知道 UI |
| `core/` 或 `protocols/` include `settings/` | 框架层不该知道应用偏好 |
| 绕过 `settings/` 直接操作底层存储（如直接读写本地 JSON） | 破坏单一数据源（唯一例外是 `settings/Settings.cpp`） |
| `ui/` 里直接操作 socket / 串口 | 网络细节属于 `protocols/`，必须通过 `ISession` 接口 |
| 头文件 include 自己 | 循环包含 |
| 新增协议时改 `ui/` / `app/` | 说明 UI 数据驱动被破坏了（尤其是 `if (protocolId == "xxx")`） |
| 在 `ui/` 里硬编码 Modbus 专属参数（如 slaveId） | 必须通过 `IProtocol::VariableParamsFields()` 动态生成 |

---

## 3. 需求 → 归属对照表

**新增功能时先问自己：改动属于哪一层？**

| 需求 | 应改哪里 |
|---|---|
| 新协议 | `protocols/<name>/` |
| 新编解码 | `protocols/codec/` |
| 新对话框 | `ui/` |
| 新设置项 | `settings/` |
| 协议配置格式变化 | `config/` |
| 框架接口扩展 | `core/` |
| 菜单项调整 | `app/MainFrame` |
| 新增测试 | `tests/unit/` 或 `tests/integration/` |
| 新增决策 | `docs/architecture/adr/` |

---

## 4. 自检命令

提交前在仓库根目录运行（MSYS2 shell）：

```bash
# 1. 分层违规：core/ 与 protocols/ 不得 include ui/ 或 settings/
grep -rn '#include.*ui/\|#include.*settings/' src/core/ src/protocols/

# 2. 绕过 settings 直接操作底层存储（比如自己造个文件读写）
grep -rn "std::ofstream.*settings" src/ | grep -v src/settings/

# 3. 循环包含（头文件 include 自己）
for f in $(find src -name "*.h"); do
    grep -q "#include \"$(basename $f)\"" $f && echo "SELF INCLUDE: $f"
done

# 4. 日志约定（用户可见字符串必须翻译）
grep -rn 'SPDLOG_\(INFO\|WARN\|ERROR\|DEBUG\)("' src/ | grep -v '_('
grep -rn 'ICU_SPDLOG_.*".*{.*}' src/

# 5. UI 层不得硬编码协议 ID 进行特判
grep -rn 'protocolId == "' src/ui/ src/app/
```

五条命令都应当**没有输出**（除了一些明确的框架级判断，但正常情况下应为空）。

---

## 5. 违反时怎么办

1. 先判断是**代码放错层**还是**接口设计缺陷**。
2. 代码放错层 → 移到正确目录，不改接口。
3. 接口缺陷 → 先在根 `README.md` 更新架构说明，再改代码（README 是 AI Skill 契约）。
4. 确实需要新方向时，写一条 [ADR](adr/README.md)，说明为什么必须破坏分层。

---

## 6. 相关文档

- [架构总览](overview.md)
- [新增协议](../developer/adding-a-protocol.md)
- [新增设置](../developer/adding-a-setting.md)

