# 构建指南

本文档描述如何从零构建本项目。所有命令都在 **MSYS2 UCRT64** 环境下执行。

---

## 1. 环境要求

| 组件 | 版本 | 说明 |
|---|---|---|
| MSYS2 | 最新 | 提供 UCRT64 工具链 |
| GCC | 16.1.0+ | `mingw-w64-ucrt-x86_64-gcc` |
| CMake | ≥ 3.20 | `mingw-w64-ucrt-x86_64-cmake` |
| Ninja | 任意 | 可选，`mingw-w64-ucrt-x86_64-ninja` |
| gettext | 任意 | 可选，`mingw-w64-ucrt-x86_64-gettext`；缺失时 i18n 步骤自动禁用 |
| gcovr | 任意 | 可选，仅覆盖率报告需要 |
| clang-format | 19.1.5 | 可选，仅格式检查需要 |

一次性安装：

```bash
pacman -S --needed \
    mingw-w64-ucrt-x86_64-gcc \
    mingw-w64-ucrt-x86_64-cmake \
    mingw-w64-ucrt-x86_64-ninja \
    mingw-w64-ucrt-x86_64-gettext
```

验证：

```bash
g++ --version
cmake --version
```

---

## 2. 第三方依赖

| 依赖 | 形式 | 固定版本 | 位置 |
|---|---|---|---|
| wxWidgets | **submodule** | `469921b9`（3.3.5 开发快照） | `third_party/wxWidgets/` |
| spdlog | **submodule** | `f544fce0`（v1.17.0 之后的快照） | `third_party/spdlog/` |
| doctest 2.4.11 | 单头文件（直接提交） | 2.4.11 | `third_party/doctest/doctest.h` |

> **wxWidgets 的版本注意**：上游**没有** `v3.3.5` 这个 tag（最新官方 tag 是 `v3.3.4`）。
> 本项目固定的是 `master` 上的开发快照 `469921b9`，它的 1447 个头文件与
> `v3.3.4` 中 1441 个逐字节相同，只多了 6 个文件的增量改动（含把版本号改成 3.3.5）。
> 已核对：该 commit 的 `include/wx/**` 与仓库原有头文件**完全一致**。

### 2.1 拉取 submodule（必需）

```bash
git submodule update --init --recursive
```

校验：

```bash
git submodule status
#  469921b9... third_party/wxWidgets (heads/master)
#  f544fce0... third_party/spdlog
```

`.gitmodules` 里两个 submodule 都标了 `shallow = true`，所以拉取是浅克隆，体积很小
（wxWidgets 约 20 MB），CI 上很快。

> ⚠️ **绝对不要用 `git submodule update --force`**。
> wxWidgets 自己的 `.gitignore` 忽略了 `lib/gcc*_dll`，正常 `submodule update` 会保留你编译好的库；
> 但 `--force` 会把它删掉，让你重新花几十分钟编译 wxWidgets。
> 同理也不要 `git checkout -f` 这个 submodule。

### 2.2 构建 wxWidgets（**必需**，编译产物不进仓库）

`third_party/wxWidgets/lib/` 里的库是**编译产物**，被 wxWidgets 自己的 `.gitignore` 忽略，
所以每次全新克隆都必须自己编译一次。submodule 里已有完整的 `configure` 和 `src/`。

```bash
cd third_party/wxWidgets
mkdir -p build-ci && cd build-ci

# 注意：wxWidgets 3.3 起 Unicode 是强制的，--enable-unicode 已不是合法选项
# （configure 会报 "unrecognized options: --enable-unicode" 直接退出）
../configure \
    --enable-shared \
    --enable-debug \
    --disable-tests \
    --disable-monolithic \
    --with-msw \
    CFLAGS="-O2" CXXFLAGS="-O2"

make -j$(nproc)
```

> `--enable-debug` 会同时产出 **Debug（`*ud_*`）与 Release（`*u_*`，无 `d`）** 两套库，
> 本项目 CMake 链接的是 Release 命名的那套（`libwx_mswu_core-3.3.dll.a` 等），
> 两种 `CMAKE_BUILD_TYPE` 都能用。

构建完成后应有：

```bash
ls lib/gcc_dll/libwx_mswu_core-3.3.dll.a
ls lib/gcc_dll/libwx_baseu-3.3.dll.a
ls lib/gcc_dll/mswu/wx/setup.h
```

> CMake 里 `WX_SETUP_DIR = mswu`，头文件搜索路径依赖 `lib/gcc_dll/mswu/wx/setup.h` 存在。

### 2.3 维护 submodule

```bash
# 查看当前固定的 commit
git submodule status

# 升级 wxWidgets（改完要重新编译 wx 并跑一次完整测试）
cd third_party/wxWidgets && git fetch origin master && git checkout <new-sha> && cd ../..
git add third_party/wxWidgets
```

升级 wxWidgets 属于依赖变更，按 [CONTRIBUTING.md](../../CONTRIBUTING.md) 的要求，
应当同时更新 `CHANGELOG.md` 并在 PR 描述里说明原因。

---

## 3. 构建本项目

### 3.1 命令行

```bash
# Debug + 测试
cmake -S . -B build -G "MinGW Makefiles" \
      -DCMAKE_BUILD_TYPE=Debug \
      -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
      -DICU_BUILD_TESTS=ON

cmake --build build -j$(nproc)
```

产物：

| 路径 | 内容 |
|---|---|
| `build/IndustrialCommunicationUtility.exe` | 主程序 |
| `build/tests/icu_tests.exe` | 测试可执行文件 |
| `build/generated/version.h` | 由 `cmake/version.h.in` 生成 |
| `build/compile_commands.json` | clang-tidy / IDE 用 |

构建后会自动：复制 wxWidgets DLL 到输出目录、重新生成并编译 `.po` → `.mo`、复制 i18n 资源。

### 3.2 CMake Presets

```bash
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
```

`release` preset 输出到 `build-release/`（默认 `ICU_BUILD_TESTS=OFF`）。

### 3.3 常用开关

| 选项 | 默认 | 说明 |
|---|---|---|
| `ICU_BUILD_TESTS` | `OFF` | 构建 `tests/`（命令行请显式开 `ON`，preset 已开） |
| `ICU_BUILD_COVERAGE` | `OFF` | GCC `--coverage` 插桩 + 启用 `coverage` 目标 |
| `CMAKE_BUILD_TYPE` | `Debug` | `Debug` / `Release` |

---

## 4. 运行

```bash
./build/IndustrialCommunicationUtility.exe
```

运行时需要同目录下的 wxWidgets DLL（构建后已自动复制）与 `resources/i18n/`。

日志位置：`logs/app.log`（相对可执行文件所在目录）。

---

## 5. 常见问题

### 5.1 `fatal error: wx/wx.h: No such file or directory`

`third_party/wxWidgets/lib/` 不存在或未构建 → 回到 [2.2](#22-构建-wxwidgets必需编译产物不进仓库)。

### 5.2 `cannot find -lwx_mswu_core-3.3`

wxWidgets 用了 release 命名但没产出对应导入库。确认 configure 参数里
`--enable-shared --enable-unicode` 都在，并检查 `lib/gcc_dll/`。

### 5.3 `gettext tools not found; i18n disabled`

未装 gettext。程序仍可构建，但界面不会有翻译。安装：
`pacman -S mingw-w64-ucrt-x86_64-gettext`。

### 5.4 CMake 报 `target not built`

`add_executable` 之前不能调用 `target_*_options`。见 README 第 10 节禁忌列表。

### 5.5 编码问题

MinGW 下 CMake 已强制 `-finput-charset=UTF-8 -fexec-charset=UTF-8`。
若出现乱码，确认源文件本身是 UTF-8，且编辑器没有写 BOM。

### 5.6 测试报 `temp_directory_path: No such file or directory`

`tests/helpers/TestEnv.h` 依赖有效的 `%TEMP%`。检查环境变量 `TEMP` / `TMP` 是否指向存在的目录。

---

## 6. 相关文档

- [测试指南](testing.md)
- [新增协议](adding-a-protocol.md)
- [新增设置](adding-a-setting.md)
- 仓库根目录 `README.md` 附录 A「快速命令参考」
