#!/bin/bash
set -e

CATALOG=IndustrialCommunicationUtility
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
I18N="$ROOT/resources/i18n"
LANGS=(zh_CN en_US)

cd "$ROOT"

# 递归收集 src/ 下的 .cpp 与 .h
#   - 必须递归：src/core/、src/ui/ 等，以及 header-only 的 Codec 里的 _()
#   - 传入相对路径，避免 .pot 里出现绝对路径
mapfile -t SOURCES < <(find src -type f \( -name '*.cpp' -o -name '*.h' \) | sort)

if [ "${#SOURCES[@]}" -eq 0 ]; then
    echo "错误：src/ 下没有找到源文件" >&2
    exit 1
fi

echo "[1/3] Extracting strings from ${#SOURCES[@]} files -> $CATALOG.pot"
xgettext \
    --language=C++ \
    --keyword=_ \
    --keyword=wxTRANSLATE \
    --from-code=UTF-8 \
    --output="$I18N/$CATALOG.pot" \
    "${SOURCES[@]}"

for lang in "${LANGS[@]}"; do
    po="$I18N/$lang/LC_MESSAGES/$CATALOG.po"
    mo="$I18N/$lang/LC_MESSAGES/$CATALOG.mo"

    if [ ! -f "$po" ]; then
        echo "[init] Creating $po"
        msginit --input="$I18N/$CATALOG.pot" --locale="$lang" --output="$po" --no-translator
    else
        echo "[2/3] Merging $po"
        msgmerge --update --backup=none --quiet "$po" "$I18N/$CATALOG.pot"
    fi

    echo "[3/3] Compiling $mo"
    msgfmt --check-format --output-file="$mo" "$po"
done

echo "Done. Edit .po files, then rerun this script."
