#!/usr/bin/env bash
# =========================================================================
# 构建脚本（MSYS2 UCRT64）
#
# 用法：
#   scripts/build.sh              # Debug + 测试（默认）
#   scripts/build.sh release      # Release（build-release/）
#   scripts/build.sh debug clean  # 先清理再构建
# =========================================================================
set -euo pipefail

BUILD_TYPE="${1:-debug}"
ACTION="${2:-}"

case "$BUILD_TYPE" in
    debug)
        BUILD_DIR="build-debug"
        CMAKE_BUILD_TYPE="Debug"
        ICU_BUILD_TESTS="ON"
        ;;
    release)
        BUILD_DIR="build-release"
        CMAKE_BUILD_TYPE="Release"
        ICU_BUILD_TESTS="ON"
        ;;
    *)
        echo "用法: $0 [debug|release] [clean]" >&2
        exit 2
        ;;
esac

# 切到仓库根目录（脚本可能从任何位置调用）
REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_ROOT"

if [ "$ACTION" = "clean" ]; then
    echo ">> 清理 $BUILD_DIR"
    rm -rf "$BUILD_DIR"
fi

if [ ! -f third_party/wxWidgets/lib/gcc_dll/libwx_mswu_core-3.3.dll.a ]; then
    echo "!! wxWidgets 尚未构建：third_party/wxWidgets/lib/gcc_dll 下缺少导入库" >&2
    echo "   见 docs/developer/building.md 第 2.2 节" >&2
    exit 1
fi

echo ">> 配置 $CMAKE_BUILD_TYPE → $BUILD_DIR"
cmake -S . -B "$BUILD_DIR" -G "MinGW Makefiles" \
      -DCMAKE_BUILD_TYPE="$CMAKE_BUILD_TYPE" \
      -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
      -DICU_BUILD_TESTS="$ICU_BUILD_TESTS"

echo ">> 编译（$(nproc) 并行）"
cmake --build "$BUILD_DIR" -j"$(nproc)"

echo ">> 完成：$BUILD_DIR/IndustrialCommunicationUtility.exe"
