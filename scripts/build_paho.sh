#!/usr/bin/env bash
# =========================================================================
# 构建 Paho MQTT C 和 C++ 库（静态库，Release 模式）
# 用法：./scripts/build_paho.sh [clean]
# 产物放在 third_party/paho.mqtt.c/build/install 和 third_party/paho.mqtt.cpp/build/install
# =========================================================================
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_ROOT"

PAHO_C_SRC="third_party/paho.mqtt.c"
PAHO_CPP_SRC="third_party/paho.mqtt.cpp"
PAHO_C_INSTALL="$REPO_ROOT/$PAHO_C_SRC/build/install"
PAHO_CPP_INSTALL="$REPO_ROOT/$PAHO_CPP_SRC/build/install"

ACTION="${1:-}"

if [ "$ACTION" = "clean" ]; then
    echo ">> 清理 Paho 构建目录..."
    rm -rf "$PAHO_C_SRC/build"
    rm -rf "$PAHO_CPP_SRC/build"
fi

# 定义清理函数，确保无论编译成功还是失败，都能恢复 test 目录
restore_test() {
    if [ -f "$PAHO_C_SRC/test/CMakeLists.txt.bak" ]; then
        mv "$PAHO_C_SRC/test/CMakeLists.txt.bak" "$PAHO_C_SRC/test/CMakeLists.txt"
    fi
}
trap restore_test EXIT

echo ">> 临时屏蔽 Paho C 的测试目录..."
if [ -f "$PAHO_C_SRC/test/CMakeLists.txt" ]; then
    mv "$PAHO_C_SRC/test/CMakeLists.txt" "$PAHO_C_SRC/test/CMakeLists.txt.bak"
    touch "$PAHO_C_SRC/test/CMakeLists.txt"
fi

# 1. 编译 Paho C
echo ">> 编译 Paho C..."
cmake -S "$PAHO_C_SRC" -B "$PAHO_C_SRC/build" -G "MinGW Makefiles" \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_INSTALL_PREFIX="$PAHO_C_INSTALL" \
      -DPAHO_BUILD_STATIC=ON \
      -DPAHO_BUILD_SHARED=OFF \
      -DPAHO_WITH_SSL=OFF \
      -DPAHO_BUILD_DOCUMENTATION=OFF \
      -DPAHO_BUILD_SAMPLES=OFF \
      -DCMAKE_C_FLAGS="-Wno-error=incompatible-pointer-types"
cmake --build "$PAHO_C_SRC/build" -j"$(nproc)"
cmake --install "$PAHO_C_SRC/build"

# 2. 编译 Paho C++
echo ">> 编译 Paho C++..."
cmake -S "$PAHO_CPP_SRC" -B "$PAHO_CPP_SRC/build" -G "MinGW Makefiles" \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_INSTALL_PREFIX="$PAHO_CPP_INSTALL" \
      -DCMAKE_PREFIX_PATH="$PAHO_C_INSTALL" \
      -DPAHO_BUILD_STATIC=ON \
      -DPAHO_BUILD_SHARED=OFF \
      -DPAHO_BUILD_DOCUMENTATION=OFF \
      -DPAHO_BUILD_SAMPLES=OFF \
      -DPAHO_BUILD_TESTS=OFF
cmake --build "$PAHO_CPP_SRC/build" -j"$(nproc)"
cmake --install "$PAHO_CPP_SRC/build"

echo ">> Paho 编译完成！"
echo "  C 库路径: $PAHO_C_INSTALL"
echo "  C++ 库路径: $PAHO_CPP_INSTALL"
