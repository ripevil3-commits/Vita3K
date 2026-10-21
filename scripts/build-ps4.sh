#!/usr/bin/env bash
# Experimental Vita3K PS4 build + local PKG packaging.
# Requires a prepared PS4 sysroot/toolchain, PS4-built dependencies, and RADV/VideoOut WSI.
set -Eeuo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SDK="${OO_PS4_TOOLCHAIN:-${OPENORBIS:-}}"
PS4_TOOLCHAIN_BIN="${PS4_TOOLCHAIN_BIN:-$HOME/xemups4/toolchain/bin}"
PS4_SYSROOT="${PS4_SYSROOT:-$HOME/xemups4-build/sysroot}"
WSI="${VITA3K_PS4_WSI_SOURCE:-}"
RUNTIME_INCLUDES="${VITA3K_PS4_RUNTIME_INCLUDE_DIRS:-}"
RUNTIME_LIBS="${VITA3K_PS4_RUNTIME_LIBRARIES:-}"
BUILD_DIR="${VITA3K_PS4_BUILD_DIR:-$ROOT/build-ps4}"
OUT_DIR="${VITA3K_PS4_OUT_DIR:-$ROOT/out}"
PAID="${VITA3K_PS4_PAID:-0x3800000000000035}"
VERSION="${VITA3K_PS4_VERSION:-00.01}"
TITLE_ID="${VITA3K_PS4_TITLE_ID:-VITA00003}"
CONTENT_LABEL="${VITA3K_PS4_CONTENT_LABEL:-VITAPS4EMU000000}"
TITLE="${VITA3K_PS4_TITLE:-Vita3K PS4 Prototype}"

die() {
    printf '[vita3k-ps4] ERROR: %s\n' "$*" >&2
    exit 1
}
need_file() {
    [[ -f "$1" ]] || die "Missing file: $1"
}
need_exec() {
    [[ -x "$1" ]] || die "Missing executable: $1"
}

[[ -n "$SDK" ]] || die "Set OO_PS4_TOOLCHAIN to an OpenOrbis SDK root for create-fself/create-gp4/PkgTool.Core."
[[ -d "$SDK" ]] || die "OpenOrbis SDK directory does not exist: $SDK"
SDK="$(cd "$SDK" && pwd)"
[[ -d "$PS4_SYSROOT" ]] || die "PS4 sysroot not found: $PS4_SYSROOT"
[[ -x "$PS4_TOOLCHAIN_BIN/ps4-cc" ]] || die "Missing PS4 compiler wrapper: $PS4_TOOLCHAIN_BIN/ps4-cc"
[[ -x "$PS4_TOOLCHAIN_BIN/ps4-c++" ]] || die "Missing PS4 compiler wrapper: $PS4_TOOLCHAIN_BIN/ps4-c++"
[[ -n "$WSI" ]] || die "Set VITA3K_PS4_WSI_SOURCE to a PS4 RADV/VideoOut WSI source file."
need_file "$WSI"
WSI="$(cd "$(dirname "$WSI")" && pwd)/$(basename "$WSI")"
[[ -n "$RUNTIME_LIBS" ]] || die "Set VITA3K_PS4_RUNTIME_LIBRARIES to the semicolon-separated PS4 RADV/Orbis link libraries."
need_exec "$SDK/bin/linux/create-fself"
need_exec "$SDK/bin/linux/create-gp4"
need_exec "$SDK/bin/linux/PkgTool.Core"
need_file "$SDK/samples/SDL2/sce_sys/icon0.png"
need_file "$SDK/samples/piglet/sce_sys/about/right.sprx"

[[ "$TITLE_ID" =~ ^[A-Z]{4}[0-9]{5}$ ]] || die "TITLE_ID must be four uppercase letters followed by five digits."
[[ "$VERSION" =~ ^[0-9]{2}\.[0-9]{2}$ ]] || die "VERSION must use format XX.YY."

export PS4_TOOLCHAIN_BIN PS4_SYSROOT OO_PS4_TOOLCHAIN="$SDK"
mkdir -p "$OUT_DIR" "$BUILD_DIR"
OUT_DIR="$(cd "$OUT_DIR" && pwd)"
BUILD_DIR="$(cd "$BUILD_DIR" && pwd)"

echo "[vita3k-ps4] configure (experimental; full Vita3K build is not yet validated)"
cmake -S "$ROOT" -B "$BUILD_DIR" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE="$ROOT/cmake/ps4-wrapper.cmake" \
    -DVITA3K_PS4=ON \
    -DUSE_DISCORD_RICH_PRESENCE=OFF \
    -DUSE_LTO=NEVER \
    -DBoost_ROOT="$PS4_SYSROOT" \
    -DCMAKE_PREFIX_PATH="$PS4_SYSROOT" \
    -DVITA3K_PS4_WSI_SOURCE="$WSI" \
    -DVITA3K_PS4_RUNTIME_INCLUDE_DIRS="$RUNTIME_INCLUDES" \
    -DVITA3K_PS4_RUNTIME_LIBRARIES="$RUNTIME_LIBS"

echo "[vita3k-ps4] compile"
cmake --build "$BUILD_DIR" --target vita3k --parallel "${JOBS:-2}"

ELF="$BUILD_DIR/bin/Vita3K"
need_file "$ELF"
STAGE="$BUILD_DIR/pkg-stage"
rm -rf "$STAGE"
mkdir -p "$STAGE/sce_sys/about" "$STAGE/sce_module"

echo "[vita3k-ps4] convert ELF to eboot.bin"
(
    cd "$STAGE"
    "$SDK/bin/linux/create-fself" "-in=$ELF" "-out=$STAGE/Vita3K.oelf" --eboot "eboot.bin" --paid "$PAID"
)
need_file "$STAGE/eboot.bin"

echo "[vita3k-ps4] stage immutable assets"
cp "$SDK/samples/SDL2/sce_sys/icon0.png" "$STAGE/sce_sys/icon0.png"
cp "$SDK/samples/piglet/sce_sys/about/right.sprx" "$STAGE/sce_sys/about/right.sprx"
shopt -s nullglob
modules=( "$SDK"/samples/piglet/sce_module/*.prx )
shopt -u nullglob
(("${#modules[@]}" > 0)) || die "No standard module files found under $SDK/samples/piglet/sce_module/"
cp "${modules[@]}" "$STAGE/sce_module/"
cp -R "$ROOT/data" "$STAGE/data"
cp -R "$ROOT/vita3k/shaders-builtin" "$STAGE/shaders-builtin"
cp -R "$ROOT/vita3k/icons" "$STAGE/icons"
cp "$ROOT/COPYING.txt" "$STAGE/COPYING.txt"

echo "[vita3k-ps4] create param.sfo"
CONTENT_ID="IV0000-${TITLE_ID}_00-${CONTENT_LABEL}"
SFO="$STAGE/sce_sys/param.sfo"
"$SDK/bin/linux/PkgTool.Core" sfo_new "$SFO"
set_entry() {
    "$SDK/bin/linux/PkgTool.Core" sfo_setentry "$SFO" "$1" --type "$2" --maxsize "$3" --value "$4" >/dev/null
}
set_entry APP_TYPE Integer 4 1
set_entry APP_VER Utf8 8 "$VERSION"
set_entry ATTRIBUTE Integer 4 0
set_entry CATEGORY Utf8 4 gd
set_entry CONTENT_ID Utf8 48 "$CONTENT_ID"
set_entry DOWNLOAD_DATA_SIZE Integer 4 0
set_entry SYSTEM_VER Integer 4 0
set_entry TITLE Utf8 128 "$TITLE"
set_entry TITLE_ID Utf8 12 "$TITLE_ID"
set_entry VERSION Utf8 8 "$VERSION"

echo "[vita3k-ps4] build local PKG"
(
    cd "$STAGE"
    "$SDK/bin/linux/create-gp4" -out pkg.gp4 --content-id="$CONTENT_ID" --path .
    "$SDK/bin/linux/PkgTool.Core" pkg_build pkg.gp4 "$OUT_DIR"
)
PKG="$OUT_DIR/$CONTENT_ID.pkg"
need_file "$PKG"
cp "$PKG" "$OUT_DIR/Vita3K-PS4-Prototype-v$VERSION.pkg"

echo "[vita3k-ps4] package created: $OUT_DIR/Vita3K-PS4-Prototype-v$VERSION.pkg"
echo "[vita3k-ps4] This build was not tested on a real PS4. Keep the package private; it includes modules from your local SDK."
