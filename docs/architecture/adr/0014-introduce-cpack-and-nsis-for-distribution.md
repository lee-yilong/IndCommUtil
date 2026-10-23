# 0014. 引入 CPack 与 NSIS 构建安装程序

- **状态**：已接受
- **日期**：2026-10-24
- **决策者**：项目所有者

## 背景

随着 `IndComm` 完成了阶段 13，软件的核心功能、协议栈、UI 以及数据持久化均已达到稳定可用状态。但目前用户如果要在工业现场部署该软件，必须自己克隆代码、配置 MSYS2 环境、拉取 submodule 并手动编译。

我们需要一套标准化的分发机制，使得非开发人员也能便捷地安装和使用。分发的产物需要满足以下要求：
1. **免安装版（绿色版）**：以 ZIP 格式提供，解压即用，适合将工具拷贝到 U 盘带入无网络环境（如工厂内网）。
2. **安装程序（Installer）**：提供标准的 Windows 安装包（包含开始菜单快捷方式、桌面快捷方式、卸载程序）。
3. **自动化流水线**：在 GitHub Actions 打上 tag 时，自动编译、打包并将产物发布到 GitHub Releases 页面。

## 决策

**引入 CMake 原生打包工具 CPack，并使用 NSIS 作为 Windows 下的安装包生成器。**

具体决策如下：
1. **CPack ZIP 生成器**：通过 CMake 配置，生成包含所有 DLL、资源、i18n 文件和 EXE 的免安装 ZIP 压缩包。
2. **NSIS 集成**：在 MSYS2 中安装 `mingw-w64-ucrt-x86_64-nsis`，并让 CPack 调用 NSIS 生成 `IndComm-Setup.exe` 安装程序。
3. **依赖收集**：CPack 安装规则需要包含：
   - `IndComm.exe` 和 `indcomm-cli.exe`
   - 所有依赖的 wxWidgets DLLs、`libmodbus.dll`
   - `resources/` 目录（包含 i18n 和图标）
4. **CI 自动发布**：在 `.github/workflows/build.yml` 中增加 `package` 步骤。当提交推送到 tag（如 `v0.1.0`）时，自动创建 GitHub Release 并上传 ZIP 与安装包。

## 替代方案

| 方案 | 为什么没选 |
|---|---|
| **Inno Setup** | 需要额外下载并安装软件（无法通过 pacman 直接管理），且对 CI 环境的配置略繁琐。 |
| **WiX Toolset** | 基于 XML，学习曲线极陡峭，安装包体积较大，不适合轻量级工具。 |
| **自解压压缩包 (7z SFX)** | 无法在开始菜单和桌面创建快捷方式，显得不够专业，且容易被杀毒软件误报。 |
| **纯批处理脚本** | 无法生成标准的卸载程序。 |

## 后果

**好处**
- **开箱即用**：终端用户无需任何 C++ 环境即可运行。
- **生态兼容**：NSIS 是 Windows 下最经典的安装包工具，稳定可靠，安全软件误报率低。
- **全自动 CI/CD**：打一个 tag 即可完成编译到发布的全流程。

**代价 / 限制**
- **依赖 NSIS 工具链**：本地打包和 CI 都需要预装 NSIS。
- **DLL 依赖追踪**：如果未来增加了新的第三方动态库（如 OpenSSL），需要手动将其加入 CPack 的安装列表中，否则安装后的程序会报缺 DLL。

**后续必须遵守的约束**
- 每次新增 `.dll` 依赖时，必须在 `CMakeLists.txt` 的 CPack `install(FILES ...)` 列表中登记。
- 确保 CI 使用的 NSIS 版本与本地一致（通过 MSYS2 pacman 安装）。

## 参考

- 工具：[CPack](https://cmake.org/cmake/help/latest/module/CPack.html)
- 工具：[NSIS](https://nsis.sourceforge.io/)
