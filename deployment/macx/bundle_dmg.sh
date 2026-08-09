#!/bin/bash
set -e

APP="$1"
INSTALL_DIR="$2"
ENTITLEMENTS="$3"

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

if [ -z "$APP" ]; then
    echo "Bundle_dmg - Missing app file name"
	exit 1
fi

TARGET="${APP##*/}"
TARGET="${TARGET%.app}"

if [ -z "$INSTALL_DIR" ]; then
    INSTALL_DIR="${APP%/*}/../install/${TARGET}"
else
    INSTALL_DIR="${INSTALL_DIR}/${TARGET}"
fi

if [ -z "$ENTITLEMENTS" ]; then
    ENTITLEMENTS="$SCRIPT_DIR/USB.entitlements"
fi

_FILTER="Apple Development"

IDENTITY=$(security find-identity -v -p codesigning \
    | grep "$_FILTER" \
    | head -n 1 \
    | awk '{print $2}')

if [ -z "$IDENTITY" ]; then
    echo "Bundle_dmg - No valid codesign identity found for $_FILTER"
    IDENTITY='-'
fi

echo "Bundle_dmg - Using identity: $IDENTITY"

# Create install directory
mkdir -p "${INSTALL_DIR}"

# Copy file to install directory
rsync -a "${APP}" "${INSTALL_DIR}/"

# Deploy Qt
macdeployqt6 "${INSTALL_DIR}/${TARGET}.app"

# Normalize bundled third-party libraries.
#
# macdeployqt deploys Qt frameworks and copies non-Qt dependencies,
# but some third-party dylibs (e.g. Homebrew packages) may keep their
# original LC_ID_DYLIB or internal references. Rewrite every bundled
# library to use @rpath so the application bundle is fully relocatable
# before code signing.
FRAMEWORKS="${INSTALL_DIR}/${TARGET}.app/Contents/Frameworks"
find "$FRAMEWORKS" -name "*.dylib" | while read lib
do
    name=$(basename "$lib")

    # Fix dylib identity
    install_name_tool \
        -id "@rpath/$name" \
        "$lib" 2>/dev/null || true

    # Fix references to bundled libraries
    otool -L "$lib" | awk '{print $1}' | while read dep
    do
        depname=$(basename "$dep")

        if [ -e "$FRAMEWORKS/$depname" ] && [ "$dep" != "@rpath/$depname" ]; then
            echo "Bundle_dmg - $lib: $dep -> @rpath/$depname"

            install_name_tool \
                -change "$dep" \
                "@rpath/$depname" \
                "$lib"
        fi
    done
done

# Create a symlink to "/Applications"
ln -s "/Applications" "${INSTALL_DIR}/Applications"

# Sign the app file
"${SCRIPT_DIR}/codesign.sh" "${INSTALL_DIR}/${TARGET}.app" "0" "${ENTITLEMENTS}" "--options runtime"

# Generate a .dmg file
OUT_DMG="${INSTALL_DIR}/../${TARGET}.dmg"
[ -f "$OUT_DMG" ] && rm "$OUT_DMG"
hdiutil create \
    -volname "${TARGET}" \
    -srcfolder "${INSTALL_DIR}" \
    -fs HFS+ \
    -format UDBZ \
    "$OUT_DMG"

# Sign the dmg file
codesign --force \
    -s "$IDENTITY" \
    "${INSTALL_DIR}/../${TARGET}.dmg"

# Remove temp folder
rm -rf "${INSTALL_DIR}"

exit 0
