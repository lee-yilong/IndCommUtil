# settings/ — 应用设置模块

## 职责

集中管理**应用级**设置：语言、窗口几何、UI 状态、字体、主题、图标等。

## 与 config/ 的区别

| | settings/ | config/ |
|---|---|---|
| 内容 | 应用偏好（语言、窗口、字体） | 协议配置（.conn 文件） |
| 存储 | wxConfig（注册表 / ini） | 独立 `.conn` 文件 |
| 用户感知 | 隐式（应用记住上次状态） | 显式（用户主动保存/打开） |

**不要把协议配置塞进 settings/**。

## 模块结构

| 文件 | 职责 |
|---|---|
| `SettingsKeys.h` | 所有 key 常量，集中定义 |
| `Settings.h/.cpp` | 通用 key-value 层，**唯一** `wxConfigBase` 消费者 |
| `LanguageSettings.h/.cpp` | 语言：读取 / 保存 / 应用 |
| `UISettings.h/.cpp` | 窗口几何、UI 状态、最近连接 |
| `FontSettings.h/.cpp` | 字体（预留） |
| `ThemeSettings.h/.cpp` | 主题（预留） |
| `IconSettings.h/.cpp` | 图标（预留） |

## 使用方式

```cpp
#include "settings/LanguageSettings.h"
#include "settings/UISettings.h"

// 读取
auto geo = settings::UISettings::Instance().LoadMainWindowGeometry();
if (geo.valid) SetSize(geo.rect);

auto lang = settings::LanguageSettings::Instance().GetSavedLanguage();

// 写入
settings::UISettings::Instance().SaveMainWindowGeometry(GetScreenRect(), IsMaximized());
