#!/usr/bin/env bash
# =========================================================================
# i18n 更新（提取 → 合并 → 编译）
#
# 用法：
#   scripts/update_i18n.sh
#
# 说明：
#   构建时 CMake 也会自动跑一遍 xgettext/msgmerge/msgfmt；
#   这个脚本用于脱离构建单独刷新翻译文件（例如只改了注释不想重编译）。
#
#   真正的提取逻辑在 resources/i18n/update_i18n.sh，这里只是从仓库根目录调用它。
# =========================================================================
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_ROOT"

INNER="resources/i18n/update_i18n.sh"
if [ ! -f "$INNER" ]; then
    echo "!! 找不到 $INNER" >&2
    exit 1
fi

for tool in xgettext msgmerge msgfmt; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        echo "!! 缺少 $tool，安装：pacman -S mingw-w64-ucrt-x86_64-gettext" >&2
        exit 1
    fi
done

echo ">> 调用 $INNER"
bash "$INNER"

echo ">> 检查未翻译条目"
msgattrib --untranslated resources/i18n/zh_CN/LC_MESSAGES/*.po || true
echo ">> 检查 fuzzy 条目"
msgattrib --only-fuzzy resources/i18n/zh_CN/LC_MESSAGES/*.po || true

echo ">> 完成"
