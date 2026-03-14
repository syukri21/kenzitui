#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
THIRD_PARTY_DIR="$ROOT_DIR/third_party/ncurses"
BUILD_DIR="$THIRD_PARTY_DIR/build"
PREFIX_DIR="$THIRD_PARTY_DIR/local"
VERSION="6.5"
TARBALL="ncurses-${VERSION}.tar.gz"
URL="https://invisible-mirror.net/archives/ncurses/${TARBALL}"
SRC_DIR="$BUILD_DIR/ncurses-${VERSION}"

mkdir -p "$BUILD_DIR"

if [ -f "$PREFIX_DIR/lib/libncursesw.a" ]; then
  echo "ncurses already bootstrapped at $PREFIX_DIR"
  exit 0
fi

if [ ! -f "$BUILD_DIR/$TARBALL" ]; then
  echo "Downloading $TARBALL..."
  curl -fL "$URL" -o "$BUILD_DIR/$TARBALL"
fi

if [ ! -d "$SRC_DIR" ]; then
  echo "Extracting $TARBALL..."
  tar -xzf "$BUILD_DIR/$TARBALL" -C "$BUILD_DIR"
fi

echo "Building ncurses (wide-char) into $PREFIX_DIR ..."
cd "$SRC_DIR"
mkdir -p "$PREFIX_DIR/share/terminfo"
export TERMINFO="$PREFIX_DIR/share/terminfo"
unset TERMINFO_DIRS
./configure \
  --prefix="$PREFIX_DIR" \
  --with-shared=no \
  --with-normal \
  --enable-widec \
  --disable-db-install \
  --without-tests \
  --without-manpages \
  --without-ada
make -j"$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)"
make install

echo "Done. Local ncurses installed in $PREFIX_DIR"
