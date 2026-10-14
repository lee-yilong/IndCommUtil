#!/usr/bin/env bash
# =========================================================================
# 代码格式化（clang-format）
#
# 用法：
#   scripts/format.sh          # 就地格式化 src/ 与 tests/
#   scripts/format.sh check    # 只检查，不修改（CI 用），有差异则退出码 1
#
# 版本要求：clang-format 19.1.5（与 CI 一致）。其他版本可能产生不同结果。
# =========================================================================
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_ROOT"

MODE="${1:-apply}"

CLANG_FORMAT="${CLANG_FORMAT:-clang-format}"
if ! command -v "$CLANG_FORMAT" >/dev/null 2>&1; then
    echo "!! 找不到 clang-format。安装方式见 docs/developer/building.md" >&2
    echo "   也可以用 CLANG_FORMAT=/path/to/clang-format 指定路径" >&2
    exit 1
fi

echo ">> clang-format $("$CLANG_FORMAT" --version)"

mapfile -t FILES < <(find src tests -type f \( -name '*.cpp' -o -name '*.h' \) | sort)

if [ "${#FILES[@]}" -eq 0 ]; then
    echo "!! 没有找到源文件" >&2
    exit 1
fi

# 一次传给 clang-format，避免逐个进程启动的开销
if [ "$MODE" = "check" ]; then
    if "$CLANG_FORMAT" --style=file --dry-run --Werror "${FILES[@]}"; then
        echo ">> 格式检查通过（${#FILES[@]} 个文件）"
    else
        echo "!! 存在格式问题，运行 scripts/format.sh 修正" >&2
        exit 1
    fi
else
    "$CLANG_FORMAT" -i --style=file "${FILES[@]}"
    echo ">> 已格式化 ${#FILES[@]} 个文件"
fi
