# 串口协议

---

## 1. 概述

`Serial Port` 是**单模式**协议：串口是点对点通信，不存在 Client / Server 角色区分。
新建连接时直接配置串口参数即可。

`文件 → 新建连接 → Serial Port` 打开配置对话框。

---

## 2. 字段说明

| 字段 | key | 类型 | 默认 | 说明 |
|---|---|---|---|---|
| Port | `portName` | 下拉框 | 自动 | 列出系统已注册的串口（COM1 / COM3 / ...） |
| Baud Rate | `baudRate` | 下拉框 | `9600` | 1200 ~ 921600 |
| Data Bits | `dataBits` | 下拉框 | `8` | 5 / 6 / 7 / 8 |
| Parity | `parity` | 下拉框 | `None` | None / Even / Odd / Mark / Space |
| Stop Bits | `stopBits` | 下拉框 | `1` | 1 / 1.5 / 2 |
| Flow Control | `flowControl` | 下拉框 | `None` | None / RTS/CTS / XON/XOFF |
| Read Timeout (ms) | `readTimeoutMs` | 整数 | `100` | 单次读等待，范围 10~60000 |
| Write Timeout (ms) | `writeTimeoutMs` | 整数 | `1000` | 单次写等待，范围 10~60000 |

**端口枚举**：Windows 下从注册表 `HKEY_LOCAL_MACHINE\HARDWARE\DEVICEMAP\SERIALCOMM` 读取。
Linux 枚举待实现。

---

## 3. 使用流程

1. 用 com0com 或物理串口线，准备一对可通信的串口（如 `COM5` ↔ `COM6`）
2. 程序里 `文件 → 新建连接 → Serial Port`
3. 填端口、波特率、数据位、校验位、停止位、流控
4. 确定后，状态「已连接」
5. 发送框输入内容，点发送
6. 对端（如 XCOM）收到数据
7. 对端发送数据，程序接收区显示

**重连**：点「断开连接」后再点「打开连接」直接重连，不弹配置对话框。

**改配置**：「配置连接…」只在未连接状态可用。

---

## 4. 会话面板功能

Data Log 顶部工具栏：

| 控件 | 说明 |
|---|---|
| **HEX Display** | 接收数据以十六进制显示（默认关闭，用 UTF-8 显示） |
| **Timestamp** | 每条消息前加 `[YYYY-MM-DD HH:MM:SS]`（默认开启） |
| **Auto Scroll** | 新消息到达时自动滚到底部（默认开启） |
| **Clear** | 清空接收区 |

Send 行右侧：

| 控件 | 说明 |
|---|---|
| **Append newline** | 发送时自动追加 `\n`（默认开启） |

**这些功能对 TCP 也生效**，不区分协议。

---

## 5. `.json` 文件格式

```json
{
  "connection": {
    "protocol": "serial",
    "mode": "",
    "portName": "COM3",
    "baudRate": "9600",
    "dataBits": "8",
    "parity": "N",
    "stopBits": "1",
    "flowControl": "None",
    "readTimeoutMs": 100,
    "writeTimeoutMs": 1000
  }
}
