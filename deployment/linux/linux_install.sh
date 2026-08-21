#!/usr/bin/env bash
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

QT_INSTALL_PLUGINS="${1}"
QT_LIB_DIR="${2}"
INSTALL_DIR="${3}"
TARGET_NAME="${4}"

QT_PLUGIN_LIST="${SCRIPT_DIR}/qt_plugins_linux.txt"

# Check patchelf
if ! command -v patchelf >/dev/null 2>&1; then
    echo "
patchelf was not found on your system.
The installed binary's RUNPATH won't be patched, and it might not run outside the build directory.
Please install it using: 'sudo apt install patchelf' and re-Deploy
"
	exit 1
fi

if [ ! -f "${QT_PLUGIN_LIST}" ]; then
	echo "Qt plugin list not found: ${QT_PLUGIN_LIST}"
	exit 1
fi

if [ ! -d "${INSTALL_DIR}" ]; then
	echo "Missing install directory: ${INSTALL_DIR}"
	exit 1
fi

while IFS= read -r plugin
do
	[[ -z "${plugin}" || "${plugin}" =~ ^# ]] && continue

	if [ -f "${QT_INSTALL_PLUGINS}/${plugin}" ]; then		
		ldd  "${QT_INSTALL_PLUGINS}/$plugin" \
		| grep "${QT_INSTALL_PLUGINS}" \
		| awk '{print $3}' \
		| xargs -r -I{} rsync -a --copy-links "{}" "${INSTALL_DIR}/lib/"
		
		DIR="$(dirname "${plugin}")"
		mkdir -p "${INSTALL_DIR}/plugins/${DIR}"
		rsync -a "${QT_INSTALL_PLUGINS}/${plugin}" "${INSTALL_DIR}/plugins/${plugin}"
	fi
done < "${QT_PLUGIN_LIST}"

ldd "${INSTALL_DIR}/${TARGET_NAME}" \
| grep "${QT_LIB_DIR}" \
| awk '{print $3}' \
| xargs -r -I{} rsync -a --copy-links "{}" "${INSTALL_DIR}/lib/"

find "${INSTALL_DIR}/lib" -name "*.so" -print0 |
while IFS= read -r -d '' lib_file; do
    ldd "$lib_file" \
    | grep "${QT_LIB_DIR}" \
    | awk '{print $3}'
done \
| sort -u \
| xargs -r -I{} rsync -a --copy-links "{}" "${INSTALL_DIR}/lib/"

# Patch library
find "${INSTALL_DIR}/lib" -name "*.so" -print0 |
while IFS= read -r -d '' plugin; do
	patchelf --set-rpath "\$ORIGIN" "${plugin}"
done

# Patch executable
# Set ($ORIGIN/lib) to the existing RUNPATH.
# This allows the executable to find bundled libraries located next to it.
if ! patchelf --print-rpath "${INSTALL_DIR}/${TARGET_NAME}" | grep -q '\$ORIGIN/lib'; then
	echo "Executable : ${INSTALL_DIR}/${TARGET_NAME} - Setting \$ORIGIN/lib to RUNPATH"
	patchelf --set-rpath "\$ORIGIN/lib" "${INSTALL_DIR}/${TARGET_NAME}"
else
	echo "Executable : ${INSTALL_DIR}/${TARGET_NAME} - RUNPATH already contains \$ORIGIN/lib"
fi

cat <<EOF > "${INSTALL_DIR}/qt.conf"
[Paths]
Plugins=plugins

EOF
