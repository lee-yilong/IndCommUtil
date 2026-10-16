# 快速上手

## 1. 这是什么

**Industrial Communication Utility**（工业通信实用工具）是一个面向工业现场的多协议通讯测试/调试工具，
用来连 PLC、单片机、网关、串口设备，手动收发报文、观察原始数据。

定位类似 NetAssist + Modbus Poll + MQTT Explorer 的整合版。目前实现了 **TCP/IP** 协议
（客户端 + 服务器），串口与 Modbus 在后续阶段加入。

---

## 2. 安装

当前没有安装包（计划在阶段 10）。直接用构建产物：

```bash
# 见 docs/developer/building.md
scripts/build.sh
./build/IndustrialCommunicationUtility.exe
```

运行时需要与 exe 同目录的 wxWidgets DLL 和 `resources/i18n/`（构建时已自动复制）。

---

## 3. 界面结构

启动后是欢迎页，此时**没有活动连接**。

菜单栏：

| 菜单 | 项 | 说明 |
|---|---|---|
| **文件** | 新建连接 ▸ | 每个已注册协议一个子项（当前只有 `TCP/IP`） |
| | 最近连接 ▸ | 最近打开的 `.json`，最多 5 个 |
| | 打开连接 `Ctrl+O` | 打开已保存的 `.json` |
| | 保存连接 `Ctrl+S` | 保存当前配置 |
| | 另存连接 `Ctrl+Shift+S` | 保存到新文件 |
| | 关闭连接 `Ctrl+W` | 关闭当前连接 |
| | 退出 `Alt+F4` | |
| **视图** | 显示日志 `F2` | 打开日志窗口（历史保留） |
| **设置** | 语言 ▸ | 简体中文 / English（切换后需重启） |
| **帮助** | 关于 `F1` | |

标题栏显示 `当前连接名 - Industrial Communication Utility`；配置有未保存修改时前面带 `*`。

---

## 4. 第一个连接（TCP 客户端）

**准备一个测试服务端**（MSYS2 里 `pacman -S netcat`）：

```bash
nc -l -p 5020
```

**在工具里：**

1. `文件 → 新建连接 → TCP/IP`
2. 弹出配置对话框，模式选 **Client**，填：
   - Remote Host：`127.0.0.1`
   - Remote Port：`5020`
   - Connect Timeout：默认 3000 ms 即可
3. 确定 → 状态依次显示「连接中…」→「已连接」
4. 在发送框输入 `hello`，点发送 → `nc` 那边显示 `hello`
5. 在 `nc` 里输入 `world` 回车 → 工具的接收区出现 `world`

**几个常用行为**

- 点「断开连接」再点「打开连接」是**直接重连**，不会重新弹配置对话框。
- 「配置连接…」只在**未连接**状态下可用，避免连上后改配置造成混乱。
- 勾了「自动重连」后，对端挂掉会自动重试，状态显示「重连中…」。

---

## 5. 做 TCP 服务器

1. `文件 → 新建连接 → TCP/IP`，模式选 **Server**
2. Listen Host `0.0.0.0`，Listen Port `5030`
3. 客户端连上来：

```bash
nc 127.0.0.1 5030
```

4. 面板上会显示客户端数量与编号（`#1`、`#2`…），收到的数据带客户端号前缀
5. 在工具里发送的内容会**广播**给所有已连接客户端

---

## 6. 保存与复用配置

- 保存后得到 `.json` 文件，可用文本编辑器查看：：

```json
{
  "connection": {
    "protocol": "tcp",
    "mode": "Client",
    "remoteHost": "127.0.0.1",
    "remotePort": 5020
  }
```

> 所有字段都在顶层 connection 对象内。protocol / mode 是稳定 key，
> 不随界面语言变化。字段类型按 FieldDescriptor::type 映射：
> 字符串/选项 → JSON string，整数 → number，布尔 → true/false

- 文件 → 打开连接 或从「最近连接」里选，即可恢复配置并连接。
- `.json` 是纯文本，方便版本管理、批量生成、脚本化。

---

## 7. 语言与日志

- `设置 → 语言 → English`，确认重启后界面变英文。
- `视图 → 显示日志`（`F2`）打开日志窗口，开发排查问题用。
- 日志文件位于可执行文件同目录的 `logs/app.log`。

---

## 8. 下一步

- [TCP/IP 协议详解](tcp.md)
- [故障排查](troubleshooting.md)
- 想自己加协议：[新增协议](../developer/adding-a-protocol.md)
