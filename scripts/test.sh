#!/usr/bin/env bash
# =========================================================================
# 测试脚本（MSYS2 UCRT64）
#
# 用法：
#   scripts/test.sh            # 跑 ctest（Debug）
#   scripts/test.sh release    # 跑 Release 的 ctest
#   scripts/test.sh --list     # 列出所有用例，不执行
# =========================================================================
set -euo pipefail

if [ "${1:-}" = "--list" ]; then
    EXE="build-debug/tests/icu_tests.exe"
    if [ ! -f "$EXE" ]; then
        echo "!! $EXE 不存在，先运行 scripts/build.sh" >&2
        exit 1
    fi
    "$EXE" --list-test-cases
    exit 0
fi

# 这里把默认的 build 改成了 build-debug，与 build.sh 保持一致
case "${1:-debug}" in
    debug)   BUILD_DIR="build-debug" ;;
    release) BUILD_DIR="build-release" ;;
    *)
        echo "用法: $0 [debug|release|--list]" >&2
        exit 2
        ;;
esac

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_ROOT"

if [ ! -d "$BUILD_DIR" ]; then
    echo "!! $BUILD_DIR 不存在，先运行 scripts/build.sh ${1:-debug}" >&2
    exit 1
fi

echo ">> ctest（$BUILD_DIR）"
ctest --test-dir "$BUILD_DIR" --output-on-failure
