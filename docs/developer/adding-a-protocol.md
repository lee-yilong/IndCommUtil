# 新增协议

> 目标：**新增协议 = 新增一个类 + 注册一行。不碰 UI、不碰菜单、不碰对话框。**
> 如果实现过程中需要改 `src/ui/` 或 `src/app/`，说明设计走偏了，停下来先看
> [ADR 0002](../architecture/adr/0002-ui-data-driven-by-field-descriptors.md)。

参考实现：`src/protocols/tcp/`（`TcpProtocol.h/.cpp`、`TcpTransport.h/.cpp`）。

---

## 1. 目录与文件

假设新协议 id 是 `foo`：

```
src/protocols/foo/
├── FooProtocol.h
├── FooProtocol.cpp
├── FooTransport.h        # 需要自定义传输时
└── FooTransport.cpp
```

编码/切帧逻辑放 `src/protocols/codec/`（可复用既有 Codec）。

---

## 2. 实现 `IProtocol`

```cpp
// FooProtocol.h
#pragma once

#include <map>
#include <string>
#include <vector>

#include "core/IProtocol.h"

namespace icu {

class FooProtocol : public IProtocol {
public:
    std::string Id() const override { return "foo"; }
    std::string DisplayName() const override { return _("&Foo Protocol").utf8_string(); }

    bool HasModes() const override { return true; }
    std::vector<std::string> ModeNames() const override { return {"Client", "Server"}; }

    std::vector<FieldDescriptor> FieldsForMode(int modeIndex) const override;
    SessionPtr CreateSession(int modeIndex,
                             const std::map<std::string, FieldValue>& fields) override;
    std::string MakeDefaultFileName(
        int modeIndex, const std::map<std::string, FieldValue>& fields) const override;
};

}  // namespace icu
```

**要点**

| 项 | 要求 |
|---|---|
| `Id()` | 全局唯一，小写，作为 `.conn` 里的 `Protocol=` 值 |
| `DisplayName()` | 翻译 key；`&` 是菜单助记符 |
| `HasModes()` | 单模式协议返回 `false`，`ModeNames()` 可返回空 |
| `ModeNames()` | 翻译 key 列表，顺序即 `modeIndex` |
| `FieldsForMode(i)` | 返回该模式的字段描述列表，顺序即 UI 顺序 |
| `MakeDefaultFileName()` | 跨平台合法（不含 `\/:*?"<>|`），参考 TCP 的 `sanitize` |

> `IProtocol` 的字符串接口都是 `std::string`（UTF-8）。`DisplayName()` 若要返回
> 翻译后的结果，用 `_("...").utf8_string()`；`FieldsForMode()` 里的 `label` 保持
> **翻译 key 原文**（`FieldDescriptor::label` 是 `std::string`），由 UI 层翻译。

---

## 3. 描述字段

```cpp
std::vector<FieldDescriptor> FooProtocol::ClientFields() const {
    return {
        {"host", "Remote Host:", FieldValue::Type::String,
         FieldValue(std::string("127.0.0.1"))},

        {"port", "Remote Port:", FieldValue::Type::Int, FieldValue(502), 1, 65535},

        {"mode", "Mode:", FieldValue::Type::Choice, FieldValue(std::string("rtu")),
         0, 0, {{"rtu", "RTU"}, {"ascii", "ASCII"}}},
    };
}
```

- `Int` 用 `minValue` / `maxValue` 约束（生成 `wxSpinCtrl`）。
- `Choice` 用 `choices`，元素是 `{value, label}`：`value` 存进 `.conn`，`label` 是翻译 key。
- `Bool` 生成复选框。
- 每个 `label` 都要能在 `.po` 里找到，否则英文界面会显示中文原文。

---

## 4. 创建会话

```cpp
SessionPtr FooProtocol::CreateSession(int modeIndex,
                                      const std::map<std::string, FieldValue>& fields) {
    FooConfig cfg;
    cfg.host = GetStr(fields, "host", "127.0.0.1");
    cfg.port = GetInt(fields, "port", 502);

    auto transport = std::make_unique<FooTransport>(cfg);
    auto codec = std::make_unique<ModbusRtuCodec>();   // 或 RawCodec
    return std::make_unique<Session>(std::move(transport), std::move(codec));
}
```

建议在 `.cpp` 的匿名命名空间里放 `GetStr` / `GetInt` / `GetBool` 小工具（照抄
`TcpProtocol.cpp` 第 9~27 行）。

**`CreateSession()` 不得依赖任何 UI。**

---

## 5. 实现 Transport（如需要）

继承 `ITransport`，只需要实现纯虚函数，回调通过基类的 `Notify*` 触发：

```cpp
class FooTransport : public ITransport {
public:
    bool Open() override;                       // 返回「已发起」，真实结果走 NotifyState
    void Close() override;
    TransportState State() const override;
    bool IsOpen() const override;
    std::string Target() const override;        // 用于界面显示，如 "COM3 @ 9600"
    size_t Send(const uint8_t* data, size_t len) override;
    bool IsServerMode() const override;         // 服务器型协议才需要覆盖
    int ClientCount() const override;           // 同上
};
```

**要点**

- `Open()` 是**异步语义**：先 `NotifyState(TransportState::Opening)`，连接结果稍后通过
  `NotifyState(Open/Closed)` 通知。`MockTransport::Open()` 就是只切到 `Opening`。
- `clientId`：客户端恒为 `0`；服务器型协议填真实客户端号。
- 收到数据用 `NotifyData(clientId, ptr, len)`；不要在 Transport 里做切帧（那是 Codec 的事）。
- 销毁前先 `Notify(false)`；`wxSocketBase` **没有** `RemoveEventHandler`。
- 回调每类只存一个，重复注册会覆盖。

---

## 6. 实现 Codec（如需要）

继承 `ICodec`：

```cpp
class FooCodec : public ICodec {
public:
    bool Encode(const std::vector<uint8_t>& message, std::vector<uint8_t>& out) override;
    void Feed(const uint8_t* data, size_t len, MessageCallback onMessage) override;
    void Reset() override;
    std::string Name() const override { return "Foo"; }
};
```

- `Feed` 自己维护接收缓冲区，处理**半包 / 粘包**，一次可以产出多条消息。
- 重连时框架会调 `Reset()`。
- header-only 的 Codec 里若用 `_()`，必须 `#include <wx/translation.h>`。
- 无状态的透传协议直接用 `RawCodec`。

---

## 7. 注册

`src/core/ProtocolRegistry.cpp`：

```cpp
#include "protocols/foo/FooProtocol.h"

void RegisterBuiltinProtocols() {
    auto& reg = ProtocolRegistry::Instance();
    reg.Register(std::make_unique<TcpProtocol>());
    reg.Register(std::make_unique<FooProtocol>());   // ← 加这一行
}
```

> `All()` 遍历 `std::map`，所以**菜单顺序按协议 id 字母序**，与注册顺序无关。

不需要改任何 UI / 菜单代码：`MainFrame` 在构造时遍历 `ProtocolRegistry::All()` 生成菜单项。

---

## 8. 加测试

在 `tests/unit/protocols/foo/` 下新建 `test_FooProtocol.cpp`：

```cpp
#include "doctest.h"

#include "core/ProtocolRegistry.h"
#include "core/Session.h"
#include "protocols/foo/FooProtocol.h"

using namespace icu;

TEST_CASE("FooProtocol: 元数据") {
    FooProtocol p;
    CHECK(p.Id() == "foo");
    CHECK_FALSE(p.DisplayName().empty());
    CHECK(p.HasModes());
    CHECK(p.ModeNames().size() == 2);
}

TEST_CASE("FooProtocol: 字段描述非空且 label 是翻译 key") {
    FooProtocol p;
    for (int mode = 0; mode < static_cast<int>(p.ModeNames().size()); ++mode) {
        const auto fields = p.FieldsForMode(mode);
        CHECK_FALSE(fields.empty());
        for (const auto& f : fields) {
            CHECK_FALSE(f.key.empty());
            CHECK_FALSE(f.label.empty());
        }
    }
}

TEST_CASE("FooProtocol: CreateSession 返回可用会话") {
    FooProtocol p;
    auto session = p.CreateSession(0, {{"host", FieldValue(std::string("127.0.0.1"))},
                                      {"port", FieldValue(502)}});
    REQUIRE(session != nullptr);
    CHECK(session->State() == SessionState::Idle);
}
```

参考 `tests/unit/core/test_ProtocolRegistry.cpp` 里对 `TcpProtocol` 的既有用例
（包括「label 是非空翻译 key」的检查）。需要模拟传输行为时用
`tests/mocks/MockTransport.h`。

---

## 9. 补齐翻译

```bash
scripts/update_i18n.sh
```

然后在 `resources/i18n/zh_CN/LC_MESSAGES/IndustrialCommunicationUtility.po` 里补上
新 label 的中文，`en_US` 同理。

检查是否漏翻：

```bash
msgattrib --untranslated resources/i18n/zh_CN/LC_MESSAGES/*.po
msgattrib --only-fuzzy   resources/i18n/zh_CN/LC_MESSAGES/*.po
```

---

## 10. 检查清单

- [ ] 实现了 `IProtocol` 的全部纯虚函数
- [ ] `Id()` 唯一
- [ ] `DisplayName()` 是翻译 key
- [ ] `FieldsForMode()` 返回的 label 都是翻译 key
- [ ] `CreateSession()` 不依赖任何 UI
- [ ] `MakeDefaultFileName()` 生成的名称跨平台合法
- [ ] 已注册到 `RegisterBuiltinProtocols()`
- [ ] 新增 label 已在 `.po` 里翻译
- [ ] 已在根 `README.md` 第 8 节协议清单里更新状态
- [ ] 已加 `tests/unit/protocols/<id>/` 单测
- [ ] **未修改任何非 `protocols/<id>/` 目录下的生产代码**
- [ ] `ctest` 全绿
- [ ] `scripts/format.sh` 无 diff

---

## 11. 相关文档

- [核心 API](../api/core.md)
- [架构总览](../architecture/overview.md)
- [测试指南](testing.md)
- [ADR 0001：协议是一等公民](../architecture/adr/0001-protocol-as-first-class-with-modes.md)
