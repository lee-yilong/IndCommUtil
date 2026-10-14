#!/bin/bash
set -e

CATALOG=IndustrialCommunicationUtility
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
I18N="$ROOT/resources/i18n"
LANGS=(zh_CN en_US)

cd "$ROOT"

echo "[1/3] Extracting strings from source -> $CATALOG.pot"
xgettext \
    --language=C++ \
    --keyword=_ \
    --keyword=wxTRANSLATE \
    --from-code=UTF-8 \
    --output="$I18N/$CATALOG.pot" \
    src/*.cpp src/*/*.cpp

for lang in "${LANGS[@]}"; do
    po="$I18N/$lang/LC_MESSAGES/$CATALOG.po"
    mo="$I18N/$lang/LC_MESSAGES/$CATALOG.mo"

    if [ ! -f "$po" ]; then
        echo "[init] Creating $po"
        msginit --input="$I18N/$CATALOG.pot" --locale="$lang" --output="$po" --no-translator
    else
        echo "[2/3] Merging $po"
        msgmerge --update --backup=none "$po" "$I18N/$CATALOG.pot"
    fi

    echo "[3/3] Compiling $mo"
    msgfmt --output-file="$mo" "$po"
done

echo "Done. Edit .po files, then rerun this script."
