# TCP/IP 协议

---

## 1. 两种模式

`TCP/IP` 是**一个**协议，内部有 Client / Server 两种**模式**。

新建连接时选模式（`Mode=Client` 或 `Mode=Server` 存进 `.json`），
菜单里只有一个 `TCP/IP` 入口 —— 这是刻意的设计，
见 [ADR 0001](../architecture/adr/0001-protocol-as-first-class-with-modes.md)。

---

## 2. Client 模式字段

| 字段 | key | 类型 | 默认 | 范围 | 说明 |
|---|---|---|---|---|---|
| Remote Host | `remoteHost` | 字符串 | `127.0.0.1` | — | 对端地址，可填域名 |
| Remote Port | `remotePort` | 整数 | `502` | 1–65535 | 对端端口 |
| Local Host (optional) | `localHost` | 字符串 | `0.0.0.0` | — | 绑定本地地址 |
| Local Port (0 = auto) | `localPort` | 整数 | `0` | 0–65535 | 0 表示由系统分配 |
| Connect Timeout (ms) | `timeoutMs` | 整数 | `3000` | 100–600000 | 连接超时 |
| Auto reconnect on disconnect | `autoReconnect` | 布尔 | 开 | — | 断开后自动重连 |
| Max reconnect attempts | `reconnectMaxAttempts` | 选项 | `0` | 0/1/3/5/10/30 | `0` = 不限次 |
| Reconnect delay | `reconnectDelaySec` | 选项 | `0` | 0/1/3/5/10/30 | `0` = 自动（退避） |

### 状态与动作

| Session 状态 | 界面显示 | 可用动作 |
|---|---|---|
| `Idle` | 已断开 | 打开连接、配置连接 |
| `Connecting` | 连接中… | 断开连接 |
| `Connected` | 已连接 | 发送、断开连接、配置连接不可用 |
| `Reconnecting` | 重连中…（附尝试次数） | 断开连接 |
| `Disconnected` | 已断开 | 打开连接 |

- **打开连接**：在 `Disconnected` / `Idle` 状态下按当前配置直接重连，**不弹配置对话框**。
  设计理由：配置已经确认过，重连时再弹框是多余操作。
- **配置连接…**：仅在未连接状态可用。

---

## 3. Server 模式字段

| 字段 | key | 类型 | 默认 | 范围 | 说明 |
|---|---|---|---|---|---|
| Listen Host | `listenHost` | 字符串 | `0.0.0.0` | — | 监听地址 |
| Listen Port | `listenPort` | 整数 | `502` | 1–65535 | 监听端口 |
| Max Clients | `maxClients` | 整数 | `10` | 1–1000 | 最大并发客户端 |
| Listen Backlog | `backlog` | 整数 | `5` | 1–1000 | 监听队列长度 |

### 行为

- 启动后状态为「监听中」，统计区显示 `Clients: 0`。
- 每个客户端分配一个递增编号（`#1`、`#2`…），连接/断开时在数据区提示。
- 收到的数据带客户端号前缀，便于分辨来源。
- **发送是广播**：内容发给所有已连接客户端。
- 「停止监听」关闭服务器并断开所有客户端（`Clients` 归零）。

多客户端并发测试：

```bash
for i in 1 2 3; do
    (echo "hello from #$i"; sleep 5) | nc 127.0.0.1 5030 &
done
wait
```

---

## 4. 编解码

TCP 模式目前使用 **`RawCodec`（透传）**：不做任何封帧处理，
发送框里的内容原样发出去，收到的字节原样显示在接收区。

`ICodec::Feed` 的框架支持半包/粘包切帧，等引入 Modbus RTU 等有帧结构的协议时启用
（见 `src/protocols/codec/ModbusRtuCodec.h`，实现计划在阶段 6）。

---

## 5. 通信日志格式

数据区区分方向：

| 前缀 | 含义 |
|---|---|
| `<` | 收到的数据 |
| `>` | 发出的数据 |
| `<#1` | 服务器模式下来自客户端 #1 的数据 |

---


## 6. `.json` 文件格式

```json
{
  "connection": {
    "protocol": "tcp",
    "mode": "Client",
    "remoteHost": "192.168.1.10",
    "remotePort": 502,
    "localHost": "0.0.0.0",
    "localPort": 0,
    "timeoutMs": 3000,
    "autoReconnect": true,
    "reconnectMaxAttempts": "3",
    "reconnectDelaySec": "5"
  }
}
```

- 全部字段都写在 `[Connection]` 段下（`wxFileConfig` 的 `/Connection/<key>` 路径）。
- `Protocol` 必须是已注册协议 id（`tcp`）。
- `Mode` 是模式名（`Client` / `Server`）；**名字不匹配时回退到 `modeIndex` 0**，
  所以手工改成无法识别的模式名不会导致加载失败。
- `Protocol` 指向不存在的协议 → 加载失败并提示。
- 字段缺失时使用协议的默认值。
- 键名**不区分大小写**：`RemoteHost` 与 `remoteHost` 等价（`wxFileConfig` 行为）。
  应用保存时写的是 `FieldDescriptor::key` 的原始大小写（`remoteHost`）。

---

## 7. 与 `nc` 配合的完整回归流程

见根 `README.md` 附录 E.3（客户端）与 E.4（服务器），含边界场景：

| 场景 | 操作 | 期望 |
|---|---|---|
| 连接失败 | 连 `127.0.0.1:9999` | 超时后「已断开」 |
| 中途断线 | 连接中 `Ctrl+C` 关掉 nc | 显示「已断开」（或开始重连） |
| 服务器满 | 超过 `maxClients` 后新连 | 拒绝或忽略新连接 |

---

## 8. 相关文档

- [快速上手](getting-started.md)
- [故障排查](troubleshooting.md)
- [核心 API](../api/core.md)
