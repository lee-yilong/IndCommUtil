# 新增设置项

> `settings/` 是**应用级偏好**的唯一入口：语言、窗口几何、字体、主题、图标。
> 协议配置（`.conn` 文件）走 `config/`，不要混进这里。
>
> **加一个设置项 = 改 3 个文件（其中 2 个是 `settings/` 里的）。**

---

## 1. 先判断该不该放这里

| 这个设置是… | 放哪里 |
|---|---|
| 应用级偏好（语言、窗口位置、字号、主题） | `settings/` ✅ |
| 单个连接的配置（主机、端口、波特率） | 协议的 `FieldDescriptor`，存 `.conn` ✅ |
| 一次会话的运行态（是否已连接、收发字节数） | 运行时对象，不入配置 ❌ |
| 协议清单 / 注册信息 | `ProtocolRegistry` ❌ |

详见根 `README.md` 第 6.2 节「`settings/` 与 `config/` 的区别」。

---

## 2. 三步走

### 第 1 步：加 key —— `src/settings/SettingsKeys.h`

```cpp
namespace icu::settings {

// ...

// =========================================================================
// 日志窗口
// =========================================================================
constexpr const char* kLogWindowShowTimestamp = "/Window/Log/ShowTimestamp";

}  // namespace icu::settings
```

**约定**

- 一律 `constexpr const char*`，`k` 前缀 + 驼峰。
- 路径按功能分组，用 `/Group/Sub/Name` 形式（wxConfig 的层级路径）。
- 已在 `SettingsKeys.h` 里定义过的 key 不要重复定义。

### 第 2 步：加类型化接口 —— 对应的 `XxxSettings.h`

选择归属的类（没有合适的就是该新建一个，但先考虑现有 6 个是否够用）：

| 类 | 管什么 |
|---|---|
| `LanguageSettings` | 界面语言 |
| `UISettings` | 窗口几何、最近连接、UI 开关（十六进制显示、自动滚动、关闭最小化到托盘） |
| `FontSettings` | UI / 日志 / 数据字体（接口已定，实现待阶段 8） |
| `ThemeSettings` | 浅色 / 深色（预留） |
| `IconSettings` | 协议图标、UI 图标（预留） |
| `Settings` | 底层 key-value 读写，**唯一直接访问 `wxConfigBase` 的类** |

以 `UISettings` 为例：

```cpp
// UISettings.h
class UISettings {
public:
    static UISettings& Instance();

    // ... 已有接口 ...

    bool GetLogShowTimestamp() const;
    void SetLogShowTimestamp(bool on);
};
```

**命名约定**：读 `GetXxx()`，写 `SetXxx()`。

### 第 3 步：加实现 —— 对应的 `XxxSettings.cpp`

```cpp
// UISettings.cpp
bool UISettings::GetLogShowTimestamp() const {
    return Settings::Instance().GetBool(kLogWindowShowTimestamp, true);
}

void UISettings::SetLogShowTimestamp(bool on) {
    Settings::Instance().SetBool(kLogWindowShowTimestamp, on);
}
```

**约定**

- 必须提供**默认值**（第二个参数），首次运行的机器上没有这个 key。
- **不要在 `settings/` 以外的地方调用 `wxConfigBase`**：
  ```bash
  grep -rn "wxConfigBase" src/ | grep -v src/settings/   # 应该没有输出
  ```
- 需要立即落盘时调 `Settings::Instance().Flush()`（窗口几何就是这么做的）；
  非关键设置可以只在退出时统一 `Shutdown()` 落盘。

---

## 3. 在使用点调用

```cpp
#include "settings/UISettings.h"

// ...
const bool showTs = settings::UISettings::Instance().GetLogShowTimestamp();
```

`core/` 与 `protocols/` **不得** include `settings/`（分层规则，见
[layers.md](../architecture/layers.md)）。

---

## 4. 加 UI（当前状态的说明）

根 `README.md` 第 6.6 节提到第 4 步是「在 `ui/SettingsDialog` 里加控件」。

> ⚠️ **`ui/SettingsDialog` 目前还不存在**（计划在阶段 8 实现）。
> 在此之前，新增设置项的落地方式是：**先提供接口，由使用点直接读取**，
> 或者临时放进现有的菜单项里（例如「视图」菜单）。
> 阶段 8 引入 `SettingsDialog` 后，再把这些接口接到界面上。

如果你确实要让用户能改这个设置，现阶段可选择：

- 加到现有菜单（`MainFrame::BuildMenuBar()`）里的开关项
- 等阶段 8 的 `SettingsDialog`

---

## 5. 测试

`settings/` 目前没有单测覆盖：`Settings` 依赖 `wxConfigBase`，需要一个运行中的 `wxApp`/`wxInitializer`。

- 纯逻辑（比如把一个 packed 字符串解析成列表）应当抽成不依赖 `Settings` 的静态函数，
  放到可以单测的位置。
- 依赖真实配置读写的部分，用根 `README.md` 附录 E.7「窗口与 UI 测试」做手工回归。

---

## 6. 检查清单

- [ ] Key 定义在 `SettingsKeys.h`（`constexpr const char*`，`k` 前缀）
- [ ] 类型化接口加在对应的 `XxxSettings.h`
- [ ] 实现通过 `Settings::Instance()`，且提供了默认值
- [ ] 在使用点调用
- [ ] **未在 `settings/` 之外访问 `wxConfigBase`**
- [ ] **未在 `core/` 或 `protocols/` 里 include `settings/`**
- [ ] 已在根 `README.md` 第 6.5 节设置项清单里更新状态
- [ ] `ctest` 全绿、`scripts/format.sh` 无 diff

---

## 7. 相关文档

- [分层规则](../architecture/layers.md)
- [测试指南](testing.md)
- 根 `README.md` 第 6 节「应用设置模块」、附录 C
