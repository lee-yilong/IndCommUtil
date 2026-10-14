# 0002. UI 由 FieldDescriptor 数据驱动

- **状态**：已接受
- **日期**：2026-10-12（引入 ADR 机制后于 2026-10-14 补记）

## 背景

每个协议都有自己的配置项：TCP 有主机、端口、超时、自动重连；串口有波特率、数据位、校验位。
如果每个协议的配置对话框都手写一个 `wxDialog` 子类，那么：

- 新增协议必须改 UI 代码，违反「协议可插拔」
- 字段校验、默认值、类型转换逻辑在每个对话框里重复一遍
- 翻译工作分散在各处，容易漏

## 决策

**协议用 `FieldDescriptor` 列表描述自己的字段，UI 完全由数据生成。**

```
IProtocol::FieldsForMode(mode)  →  [FieldDescriptor, ...]
                                        ↓
                            GenericProtocolDialog 自动生成
                                        ↓
                        [wxTextCtrl / wxSpinCtrl / wxChoice / wxCheckBox]
```

`FieldDescriptor` 描述：字段 key、显示 label（同时是翻译 key）、控件类型、默认值、取值范围/选项。
字段值统一用 `FieldValue`（变体类型，支持 String / Int / Bool / Choice）承载，
序列化、比较、类型转换都由它负责。

**新增协议不得触碰 UI 代码。**

## 替代方案

| 方案 | 为什么没选 |
|---|---|
| 每个协议一个手写 Dialog | 新增协议要改 UI，重复代码多 |
| 用 .xrc 资源文件描述界面 | 仍需为每个协议写资源，且与协议实现分离 |
| 让协议直接返回 wxWindow* | UI 生命周期与协议耦合，协议层被迫依赖 wx GUI |
| 引入独立的 schema 文件（JSON 描述字段） | 增加一层间接，编译期无法检查 |

## 后果

**好处**

- 新增协议只需实现 `FieldsForMode()`，UI 自动适配
- 字段类型转换、默认值、校验集中在一处（`FieldValue` + `GenericProtocolDialog`）
- label 即翻译 key，i18n 无需额外机制

**代价 / 限制**

- 控件类型受限于已支持的几种；需要特殊交互的字段要扩展 `FieldDescriptor`
- 复杂条件显示（字段 A 影响字段 B 可见性）目前不支持
- UI 只能表达「平面字段列表」，嵌套结构（如客户端列表）需要协议自行处理

**后续必须遵守的约束**

- 新协议禁止新增 `ui/` 下的代码
- label 必须是翻译 key，且要同步到 `.po`（见 [新增协议](../../developer/adding-a-protocol.md)）
- 扩展 `FieldDescriptor` 属于框架变更，需先更新根 `README.md`

## 参考

- 代码：`src/core/ProtocolTypes.h`（`FieldDescriptor` / `FieldValue`）、`src/ui/GenericProtocolDialog.cpp`
- 文档：根 `README.md` 第 2.2 节
