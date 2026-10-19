# 故障排查

按现象查。每条都给出「原因」和「怎么做」。

---

## 1. 启动与构建

### 程序启动后立刻退出，没有任何窗口

1. 看日志：可执行文件同目录的 `logs/app.log`。
2. 常见原因是缺少 wxWidgets DLL。清理后重新构建（构建会自动复制 DLL）：
   ```bash
   scripts/build.sh debug clean
   ```
3. 确认 exe 同目录下存在 `wxbase*_gcc*.dll`、`wxmsw*_gcc*.dll`。

### 提示缺少某个 DLL（如 `libgcc_s_seh-1.dll`）

MinGW 运行时没静态链接进去。本项目 CMake 已加 `-static-libgcc -static-libstdc++ -static`；
如果改了链接选项导致回归，从 `C:\msys64\ucrt64\bin` 复制对应 DLL 到 exe 同目录即可临时解决。

### `fatal error: wx/wx.h: No such file or directory`

`third_party/wxWidgets/lib/` 被 `.gitignore` 忽略，全新克隆后必须先编译 wxWidgets。
见 [构建指南](../developer/building.md) 第 2.2 节。

### `cannot find -lwx_mswu_core-3.3`

同样的原因：wxWidgets 没构建完。确认 `third_party/wxWidgets/lib/gcc_dll/` 下有
`libwx_mswu_core-3.3.dll.a`（或者标准布局的 `lib/` 目录）。

### `gettext tools not found; i18n disabled`

没装 gettext。程序能构建，但没有翻译。安装：
```bash
pacman -S mingw-w64-ucrt-x86_64-gettext
```

### `bash: ./scripts/build.sh: /bin/bash^M: bad interpreter`

脚本被 Git 转成 CRLF。仓库根有 `.gitattributes` 声明 `*.sh text eol=lf`，
如果文件已经坏了，重新检出：
```bash
git rm --cached -r . >/dev/null && git reset --hard
```
或者临时转换：`sed -i 's/\r$//' scripts/*.sh`

---

## 2. 连接相关

### 状态一直是「连接中…」，最后变「已断开」

- 检查目标地址/端口是否可达：`nc -vz 127.0.0.1 5020`
- 检查防火墙是否拦了出站连接。
- 提高 `Connect Timeout`（默认 3000 ms），跨网段时 3000 ms 可能不够。

### 状态显示「重连中…」反复循环

这是**自动重连**在工作。想关掉：`配置连接…` 里取消 `Auto reconnect on disconnect`。
`Max reconnect attempts` 设成 `0` 表示不限次数。

### 「配置连接…」是灰的 / 点不动

设计如此：**未连接状态才能改配置**，避免连上后改配置造成混乱。
先「断开连接」，再改。

### 点「打开连接」没有弹出配置对话框

设计如此：配置已经确认过，重连直接复用当前配置。
要改配置用「配置连接…」。

### 服务器模式起来了，但客户端连不上

- `Listen Host` 填 `0.0.0.0` 才能接受任意网卡的连接；填 `127.0.0.1` 只接受本机。
- 端口被占用会监听失败，换一个端口或先关掉占用程序：
  ```powershell
  Get-NetTCPConnection -LocalPort 5030 | Select-Object OwningProcess
  ```
- 超过 `Max Clients` 后新连接会被拒绝，调大该值。

---

## 3. 数据收发

### 找不到底部的通用发送框了？

在阶段 10 的 UX 重构后，底部的公共发送框已删除。所有的发送操作现在均通过**通讯变量**驱动。
1. 在左侧列表选中一个变量。
2. 在右侧 `Operation Request` 区域的 `Value:` 输入框中输入内容。
3. 点击 `Send` 按钮即可发送。

### 收到的数据被拆成好几段 / 几段粘在一起

TCP 是**字节流**，没有消息边界。当前 TCP 用 `RawCodec` **透传**，
不做切帧，所以按 `recv` 的实际结果分段显示是正常现象。

需要按帧解析（定长 / 分隔符 / 协议头）时要实现 `ICodec::Feed`，
框架已支持。

### 发送了但对端收不到

- 确认状态是「已连接」，不是「连接中」。
- 服务器模式下确认 `Clients` 大于 0。
- 用 `nc -l -p 5020` 做接收端验证一下链路。

### 中文发送后显示乱码

项目内部统一 UTF-8（`-finput-charset=UTF-8 -fexec-charset=UTF-8`）。
对端如果是 GBK 设备，需要在协议层做转换 —— 目前没有做，属于已知限制。

---

## 4. 配置文件

### 打开 `.json` 提示加载失败

三个常见原因：

| 原因 | 检查 |
|---|---|
| 文件不存在 / 路径不可读 | 文件真的在吗 |
| JSON 语法错误 | 用编辑器检查括号、逗号、引号；或贴进 `python -m json.tool` 校验 |
| `"protocol"` 指向未注册的协议 | 必须是 `tcp` / `serial` / `modbus`（见根 `README.md` 第 8 节）|

`"mode"` 写错**不会**导致失败，会回退到 `modeIndex` 0（即第一个模式）。

### 手工改了 `.json`，但加载后字段还是旧值

- 键名拼错 → 该字段被忽略，用默认值。
- **键名大小写敏感**（JSON 规范），必须是 `FieldDescriptor::key` 的原始大小写
  （如 `remoteHost`，不是 `RemoteHost`）。
- 所有连接字段必须在顶层 `connection` 对象内。
- 类型错误（如把 `remotePort` 写成字符串 `"502"` 而不是数字 `502`）会导致加载失败。

### 保存失败

- 目标目录只读，或文件被占用。
- 保存到 `C:\Program Files\...` 之类需要管理员权限的位置会失败，换用户目录。

---

## 5. 界面与语言

### 切换语言后界面没变

语言切换需要重启。`设置 → 语言` 选完后会弹框询问是否立刻重启：
- 点「是」→ 程序自动重启并生效
- 点「否」→ 下次启动生效

### 界面出现英文原文而不是中文

对应字符串没有在 `.po` 里翻译，`_()` 会回退显示原文（msgid）。
补翻译后重新构建：

```bash
scripts/update_i18n.sh
# 编辑 resources/i18n/zh_CN/LC_MESSAGES/IndustrialCommunicationUtility.po
scripts/build.sh
```

### 窗口位置/大小没有被记住

窗口几何存在用户配置目录的 `settings.json` 里（底层由 `settings/` 模块托管）。
如果配置损坏，删掉 `%APPDATA%/IndustrialCommunicationUtility/settings.json` 即可重置。

---

## 6. 托盘

### 点关闭按钮后程序不见了，但进程还在

这是「最小化到托盘」。双击托盘图标可以恢复窗口，
托盘图标右键有「显示窗口」和「退出」。

### 想彻底退出

托盘图标右键 → 退出（这条路径用 `CallAfter` 包裹 `Close`，
避免在托盘事件栈上销毁托盘，是刻意处理的）。

### 托盘图标不见了，但进程还在

Windows 有时会隐藏「不活跃」的托盘图标，展开隐藏区域找一下。

---

## 7. 测试相关

### 集成测试报 `temp_directory_path: No such file or directory`

`TEMP` / `TMP` 环境变量指向了不存在的目录。
`tests/helpers/TestEnv.h` 会自动回退（`TEMP` → `TMP` → 当前目录），
如果仍然失败，检查这两个变量是否都无效。

### `ctest` 找不到测试

测试开关默认是 `OFF`。配置时必须显式打开：

```bash
cmake -S . -B build -G "MinGW Makefiles" -DICU_BUILD_TESTS=ON
```

用 `scripts/build.sh` 或 `cmake --preset debug` 会自动打开。

### `ctest` 报 `Cannot open file for write: .../LastTest.log`

`build/` 目录没有写权限（只读挂载 / 权限不足）。检查目录权限。

---

## 8. 还是解决不了

收集这些信息再排查：

1. `logs/app.log` 的内容
2. 触发问题的 `.json` 文件内容
3. `g++ --version`、`cmake --version` 输出
4. 完整的错误输出 / 崩溃位置

---

## 9. 相关文档

- [快速上手](getting-started.md)
- [TCP/IP 协议](tcp.md)
- [构建指南](../developer/building.md)
- [测试指南](../developer/testing.md)

