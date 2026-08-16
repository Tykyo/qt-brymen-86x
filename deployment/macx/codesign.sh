#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

APP="$1"
SKIP_BUNDLE_CONTENTS="$2"
ENTITLEMENTS="$3"

# Arguments codesign supplémentaires passés individuellement après un séparateur --
shift 3 2>/dev/null || true
EXTRA_ARGS=("$@")

if [ -z "$ENTITLEMENTS" ]; then
    ENTITLEMENTS="$SCRIPT_DIR/USB.entitlements"
fi

_FILTER="Apple Development"

IDENTITY=$(security find-identity -v -p codesigning \
    | grep "$_FILTER" \
    | head -n 1 \
    | awk '{print $2}')

if [ -z "$IDENTITY" ]; then
    echo "Codesign - No valid codesign identity found for $_FILTER"
    IDENTITY='-'
fi

echo "Codesign - Using identity: $IDENTITY"

FRAMEWORKS="${APP}/Contents/Frameworks"
if [ -d "$FRAMEWORKS" ] && { [ -z "$SKIP_BUNDLE_CONTENTS" ] || [ "$SKIP_BUNDLE_CONTENTS" = "0" ]; }; then
    for d in "$FRAMEWORKS"/*; do
        if [ -e "$d" ]; then
            codesign --force -s "$IDENTITY" "$d"
        fi
    done
fi

PLUGINS="${APP}/Contents/PlugIns"
if [ -d "$PLUGINS" ] && { [ -z "$SKIP_BUNDLE_CONTENTS" ] || [ "$SKIP_BUNDLE_CONTENTS" = "0" ]; }; then
    find "$PLUGINS" -type f \( -name "*.dylib" -o -perm +111 \) -print0 |
    while IFS= read -r -d '' lib; do
        codesign --force --deep -s "$IDENTITY" "$lib"
    done
fi

codesign --force \
    --entitlements "$ENTITLEMENTS" \
    -s "$IDENTITY" \
    "${EXTRA_ARGS[@]}" \
    "$APP"

codesign --verify --deep --strict --verbose=2 "$APP"
