#!/usr/bin/env bash
# =========================================================================
# 覆盖率报告（gcovr + GCC --coverage）
#
# 用法：
#   scripts/coverage.sh
#
# 产物：
#   build-coverage/coverage/index.html   逐文件 HTML 报告
#   build-coverage/coverage/summary.txt  文本汇总
#
# 注意：插桩会显著拖慢编译，所以日常构建不建议开 ICU_BUILD_COVERAGE。
# =========================================================================
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_ROOT"

BUILD_DIR="build-coverage"

if ! command -v gcovr >/dev/null 2>&1; then
    echo "!! 找不到 gcovr，安装：pacman -S mingw-w64-ucrt-x86_64-gcovr" >&2
    exit 1
fi

if [ ! -f third_party/wxWidgets/lib/gcc_dll/libwx_mswu_core-3.3.dll.a ]; then
    echo "!! wxWidgets 尚未构建，见 docs/developer/building.md 第 2.2 节" >&2
    exit 1
fi

echo ">> 配置（插桩开启）"
cmake -S . -B "$BUILD_DIR" -G "MinGW Makefiles" \
      -DCMAKE_BUILD_TYPE=Debug \
      -DICU_BUILD_TESTS=ON \
      -DICU_BUILD_COVERAGE=ON

echo ">> 编译"
cmake --build "$BUILD_DIR" -j"$(nproc)"

echo ">> 生成报告"
cmake --build "$BUILD_DIR" --target coverage

echo ">> 完成"
echo "   HTML：$BUILD_DIR/coverage/index.html"
echo "   汇总：$BUILD_DIR/coverage/summary.txt"
