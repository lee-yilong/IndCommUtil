# 测试指南

本项目用 **doctest 2.4.11** + **ctest**。

---

## 1. 目录结构

```
tests/
├── CMakeLists.txt          # 测试构建入口（GLOB 自动拾取源文件）
├── main.cpp                # doctest 实现 + main（唯一展开实现的 TU）
├── unit/                   # 单元测试，镜像 src/ 结构
│   ├── core/
│   │   ├── test_FieldValue.cpp
│   │   ├── test_ActiveProfile.cpp
│   │   ├── test_ProtocolRegistry.cpp
│   │   ├── test_MockTransport.cpp
│   │   ├── test_Session.cpp
│   │   └── test_SessionManager.cpp
│   └── protocols/codec/
│       └── test_RawCodec.cpp
├── integration/
│   └── test_ConnectionProfile.cpp
├── mocks/                  # header-only 假对象
│   ├── MockTransport.h
│   └── MockCodec.h
└── helpers/
    └── TestEnv.h           # 临时目录 RAII
```

---

## 2. 运行

```bash
# 构建（测试开关默认 OFF，命令行要显式打开）
cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Debug -DICU_BUILD_TESTS=ON
cmake --build build -j$(nproc)

# 跑全部
ctest --test-dir build --output-on-failure

# 或用脚本
scripts/test.sh
scripts/test.sh --list        # 列出所有用例
```

直接跑可执行文件（可用 doctest 的过滤参数）：

```bash
./build/tests/icu_tests.exe
./build/tests/icu_tests.exe --test-case="Session:*"
./build/tests/icu_tests.exe --test-case-exclude="*重连*"
./build/tests/icu_tests.exe --success      # 打印通过的断言
```

---

## 3. 覆盖率

```bash
scripts/coverage.sh
```

等价于：

```bash
cmake -S . -B build-coverage -G "MinGW Makefiles" \
      -DCMAKE_BUILD_TYPE=Debug -DICU_BUILD_TESTS=ON -DICU_BUILD_COVERAGE=ON
cmake --build build-coverage -j$(nproc)
cmake --build build-coverage --target coverage
# 报告：build-coverage/coverage/index.html
```

**过滤规则**（见 `tests/CMakeLists.txt`）：

- `--filter ".*/src/.*"`：只统计 `src/`
- 排除 `src/ui/` 与 `src/app/`：UI 不做单测
- `--exclude-unreachable-branches`

所以覆盖率反映的是 `core/` + `protocols/` + `config/` + `settings/`。

---

## 4. 写一个新测试

1. 在 `tests/unit/<模块>/` 或 `tests/integration/` 下新建 `test_<类名>.cpp`
2. 写用例：

```cpp
#include "doctest.h"

#include "core/StreamSession.h"

#include <memory>
#include <vector>

#include "mocks/MockCodec.h"
#include "mocks/MockTransport.h"

using namespace icu;
using icu::test::MockCodec;
using icu::test::MockTransport;

TEST_CASE("StreamSession: 未 Open 时 Send 失败") {
    auto t = std::make_unique<MockTransport>();
    auto c = std::make_unique<MockCodec>();
    StreamSession s(std::move(t), std::move(c));

    CHECK(s.State() == SessionState::Idle);
    CHECK_FALSE(s.Send({0x01}));
}
```

3. 重新构建即可 —— `tests/CMakeLists.txt` 用
   `file(GLOB_RECURSE ... CONFIGURE_DEPENDS)` 自动拾取，**不需要改 CMakeLists**。

---

## 5. 测试用 Mock

### 5.1 `MockTransport`

内存假 Transport，覆盖 `ITransport` 全部行为，并提供仿真入口：

| 成员 | 作用 |
|---|---|
| `Open()` | 只切到 `Opening`（对齐真实异步语义），返回 `false` 可模拟失败 |
| `SimulateState(TransportState::Open)` | 切到 `Open` 并通知状态回调 |
| `SimulateState(TransportState::Closed)` | 切到 `Closed` 并通知状态回调 |
| `InjectData(clientId, bytes)` | 注入收到的数据 |
| `SimulateReconnectAttempt(n)` | 触发重连回调 |
| `SimulateClientConnected(id)` / `SimulateClientDisconnected(id)` | 服务器模式客户端事件 |
| `SentBytes()` / `SentString()` / `ClearSent()` | 检查发出去的内容 |
| `openCount` / `closeCount` | `shared_ptr<int>` 计数 |

### 5.2 `MockCodec`

- `m_frameSize == 0`：透传（一次 `Feed` 产出一条消息）
- `m_frameSize > 0`：定长切帧，用于测试半包 / 粘包
- `m_failEncode`：令 `Encode` 返回 `false`

### 5.3 `TestEnv`

```cpp
#include "helpers/TestEnv.h"

using icu::test::TestEnv;

TEST_CASE("...") {
    TestEnv env;                                  // 构造：建临时目录
    const std::string path = env.Path("a.json");  // 目录内的文件路径
    // ... 析构：递归删除
}
```

`TestEnv` 优先在当前工作目录（`ctest` 运行时是 `build/tests/`，一定可写）创建临时目录，
如果失败，再回退到系统 `%TEMP%` 目录。因此 CI 与受限环境下都能跑。

---

## 6. 检查清单（提交前）

- [ ] 文件名 `test_<类名>.cpp`
- [ ] 放在 `tests/unit/<模块>/` 或 `tests/integration/`
- [ ] 用例名格式 `TEST_CASE("<类名>: <场景>")`
- [ ] 每个 `TEST_CASE` 独立，不依赖执行顺序
- [ ] 临时文件用 `TestEnv`
- [ ] 测试结束后清理临时资源（`TestEnv` 析构已处理）
- [ ] Mock 放 `tests/mocks/`（header-only）
- [ ] 需要在对象销毁后观察计数时用 `shared_ptr<int>`，不要用裸指针
- [ ] 不在 `Close()` / `SetActive(new)` 之后访问旧对象裸指针
- [ ] 测试代码**不翻译**（不用 `_()`）
- [ ] `ctest` 全绿

---

## 7. 当前覆盖情况

| 模块 | 覆盖 | 说明 |
|---|---|---|
| `core/` | 高 | `FieldValue` / `ActiveProfile` / `ProtocolRegistry` / `StreamSession` / `SessionManager` |
| `config/` | 高 | `ConnectionProfile` roundtrip（集成测试） |
| `protocols/codec/` | 中 | `RawCodec` 完整；`ModbusRtuCodec` 待完善 |
| `protocols/tcp/` | 低 | `TcpTransport` 未单测（依赖真实 socket） |
| `settings/` | 无 | 依赖 `wxApp` 与文件系统，计划阶段 13 补充 |
| `ui/` `app/` | 无 | 按设计不做单测 |

**未覆盖是已知且有意为之的**，不要为了让数字好看而写无意义的测试。

---

## 8. 手工回归测试

单元测试覆盖不到 UI、托盘、真实网络交互。完整的手工测试流程见根 `README.md` 附录 E
「标准测试流程」，包含：

- E.2 启动检查
- E.3 TCP 客户端（用 `nc` 做服务器）
- E.4 TCP 服务器（多客户端并发）
- E.5 串口测试
- E.6 配置文件
- E.7 最近连接
- E.8 窗口与 UI
- E.9 语言切换
- E.10 关闭与托盘
- E.11 异常场景

需要 `nc`：`pacman -S netcat`。

---

## 9. 相关文档

- [构建指南](building.md)
- [新增协议](adding-a-protocol.md)
- [架构决策 0003：用 doctest 作为测试框架](../architecture/adr/0003-doctest-as-test-framework.md)

