# ADR 0006：`.conn` 从 INI 迁移到 JSON

**新文件路径**：`docs/architecture/adr/0006-conn-format-ini-to-json.md`

```markdown
# 0006. `.conn` 配置文件从 INI 迁移到 JSON

- **状态**：已接受
- **日期**：2026-10-16
- **决策者**：项目所有者

## 背景

`.conn` 文件当前用 INI 格式（`wxFileConfig`），保存一个协议的连接配置。
现有结构是**平铺的 key-value**：

```ini
[Connection]
Protocol=tcp
Mode=Client
RemoteHost=127.0.0.1
RemotePort=5020
```

随着工具演进，需要承载的内容越来越多：

1. **测试步骤**（阶段 6 后续）：用户预先定义"发什么、等多久、期待什么"，
   用于交互模式和自动模式。这是一个**数组**结构，而不是平铺字段。

   INI 里表达数组只能靠命名约定：

   ```ini
   [Test/Step1]
   Action=Send
   Payload=hello

   [Test/Step2]
   Action=Wait
   TimeoutMs=1000
   ```

   步骤数量一变就要重排编号，手工编辑极易出错。

2. **类型系统缺失**：INI 里所有值都是字符串。`"5020"` 和 `5020` 无区别，
   `"true"` 和 `true` 无区别。当前解析靠 `FieldDescriptor::type` 反推，
   未来字段类型增多时容易出错。

3. **未来扩展**（数据库、报告导出、CLI 参数化）：JSON 是这些场景的事实标准。
   `.conn` 作为"测试配置"应当与外部生态兼容。

INI 已经撑不住，现在改比半年后改便宜很多——**项目还在 0.1.0，用户少，
文件格式还没成为"契约"**。

## 决策

**`.conn` 从 INI 切换为 JSON，文件扩展名改为 `.json`，不兼容旧 INI 文件。**

### 具体决策

1. **格式**：JSON
2. **库**：`nlohmann/json`（header-only）
3. **依赖管理**：作为 git submodule 引入到 `third_party/json/`
4. **扩展名**：`.conn` → `.json`
5. **向后兼容**：**不兼容**。旧 INI `.conn` 文件加载失败，用户需手工转换
6. **接口签名**：`ConnectionProfile::Load` / `Save` / `MakeDefaultFileName`
   的签名**保持不变**，只改内部实现
7. **不做测试步骤**：本次只做格式迁移，测试步骤（交互/自动模式）作为后续阶段

### 新文件示例

```json
{
  "connection": {
    "protocol": "tcp",
    "mode": "Client",
    "remoteHost": "127.0.0.1",
    "remotePort": 5020,
    "timeoutMs": 3000,
    "autoReconnect": true
  }
}
```

顶层 `connection` 对象，字段名沿用 `FieldDescriptor::key`，
类型按 `FieldDescriptor::type` 映射：

| FieldValue::Type | JSON 类型 |
|---|---|
| String | string |
| Int | number (integer) |
| Bool | bool |
| Choice | string |

## 替代方案

| 方案 | 为什么没选 |
|---|---|
| **保持 INI** | 数组表达差、类型缺失、生态不通用。测试步骤加进来会越来越别扭 |
| **XML（wxXmlDocument）** | wxWidgets 内置不用引库，但语法冗余，工业界事实标准已是 JSON |
| **rapidjson** | 性能好，但 API 啰嗦，DOM 手写繁琐，开发效率低 |
| **自写 JSON 解析** | 零依赖，但易踩坑（转义、Unicode、数字精度），工作量大 |
| **兼容读 INI** | 30 行兼容代码，理论上代价小。但项目在开发阶段、用户少，**趁早硬切比长期带着兼容负担更干净** |
| **保留 `.conn` 扩展名** | 内容变了扩展名不变容易混淆。IDE 也不会自动高亮 JSON |

## 后果

**好处**

- **数组和嵌套**原生支持，为测试步骤、断言、变量等扩展铺路
- **类型系统**：数字就是数字，布尔就是布尔，不再靠 `FieldDescriptor` 反推
- **生态**：所有主流语言/工具都能读写；用户可以用 VSCode 直接编辑
- **submodule 统一管理**：与 wxWidgets / spdlog 一致，团队和 CI 拉取版本一致
- **未来 CLI**：nlohmann/json 在 CLI 场景下直接可用，不用额外引入

**代价 / 限制**

- **破坏向后兼容**：旧 `.conn` 文件加载失败，用户必须手工转换
- **多一个第三方依赖**：`third_party/json` submodule，clone 时需要 `--recursive`
- **文件体积略大**：JSON 有括号和引号开销，但配置文件本身很小，无所谓
- **手写需谨慎**：JSON 对尾逗号、引号敏感，比 INI 严格

**后续必须遵守的约束**

- 新增协议字段的类型映射必须遵守上表（`FieldValue::Type` → JSON 类型）
- `.conn` 的 JSON schema 变更属于**文件格式变更**，需要写 ADR
- 任何**"用户可见的字符串"**（如 `protocol` / `mode` 值）不翻译，
  与现有 `.conn` 一致（`Protocol=tcp` / `Mode=Client` 是稳定 key）

## 实施路径（阶段 6）

1. 添加 `third_party/json` submodule（nlohmann/json）
2. `CMakeLists.txt`：`add_subdirectory` + `target_link_libraries`
3. `ConnectionProfile.cpp`：重写 `Load` / `Save`
4. `ConnectionProfile.h`：接口签名不变
5. `tests/integration/test_ConnectionProfile.cpp`：测试断言基本不变
6. `docs/user/*.md`：`.conn` 例子全改 JSON
7. `README.md` 第 7 节：重写
8. `.gitignore`：把 `*.json` 加入 ignore（避免用户测试文件被提交）
9. 手工回归：TCP + 串口的保存/加载/最近连接

## 影响的文件清单

| 文件 | 改动 |
|---|---|
| `third_party/json/` | 新增 submodule |
| `.gitmodules` | 新增映射 |
| `CMakeLists.txt` | 引入 nlohmann/json |
| `src/config/ConnectionProfile.cpp` | 重写读写逻辑 |
| `tests/integration/test_ConnectionProfile.cpp` | 断言不变，但文件名从 `.conn` 改 `.json` |
| `docs/user/getting-started.md` | 例子改 JSON |
| `docs/user/tcp.md` | 同上 |
| `docs/user/serial.md` | 同上 |
| `docs/user/troubleshooting.md` | 涉及 `.conn` 的段落改 JSON |
| `README.md` 第 7 节 | 重写文件格式说明 |
| `README.md` 第 12 节 | 新增阶段 6（本任务），原阶段 6 顺延为阶段 7 |
| `resources/i18n/*.po` | 涉及 `.conn` 的提示文案若提及格式需更新 |

## 参考

- 库：[nlohmann/json](https://github.com/nlohmann/json)
- 代码：`src/config/ConnectionProfile.cpp`
- 代码：`src/core/ProtocolTypes.h`（`FieldValue::Type`）
- 文档：[ADR 0004 事务型协议与字节流协议分离](0004-transaction-protocol-separation.md)
- 文档：根 `README.md` 第 7 节
```

---

## 配套改动

### 1. `docs/architecture/adr/README.md` 索引

在表格最后加一行：

```markdown
| [0006](0006-conn-format-ini-to-json.md) | `.conn` 配置文件从 INI 迁移到 JSON | 已接受 |
```

### 2. `.gitignore` 追加

在文件末尾加：

```gitignore
# 用户测试用的 .json 配置文件（避免误提交）
*.json
!package.json
```

> 注意：`third_party/json` 里的源码也是 `.json`（比如 `package.json`），
> 上面那个 `!package.json` 用来放行。但更好的做法是**只忽略项目根的 `.json`**：
>
> ```gitignore
> /test_*.json
> /*.json
> !CMakePresets.json
> ```
>
> 需要你确认一下项目根目录下有没有必须提交的 `.json` 文件（如 `CMakePresets.json`）。
> 从之前收到的资料看，`CMakePresets.json` 是要提交的，要放行。

---

## 提交文档

```bash
git add docs/architecture/adr/0006-conn-format-ini-to-json.md \
        docs/architecture/adr/README.md \
        .gitignore
git commit -m "docs: 新增 ADR 0006，.conn 从 INI 迁移到 JSON

- 记录格式迁移决策：JSON + nlohmann/json + submodule 管理
- 扩展名 .conn → .json，不兼容旧 INI 文件
- 接口签名不变，只改 ConnectionProfile 内部实现
- 阶段 6 优先做格式迁移，原阶段 6（Modbus）顺延为阶段 7"
git push origin main
```

---
