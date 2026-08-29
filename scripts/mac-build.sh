#!/bin/bash
set -e

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

BUILD_DIR="$ROOT/build"

RUN_DIRECTORY="faxedit"
RUN_BINARY="eoe"

while [ $# -gt 0 ]; do
    case "$1" in
        --no-run)      RUN_BINARY="" ;;
        --run-cli)     RUN_DIRECTORY="faxiscripts"; RUN_BINARY="eoe-cli" ;;
        *) echo "Usage: $0 [--no-run | --run-cli]" >&2; exit 1 ;;
    esac
    shift
done

if [ -e "$BUILD_DIR/faxedit" ] && [ ! -d "$BUILD_DIR/faxedit" ]; then
    echo "==> Discarding stale build directory (pre-consolidation layout)..."
    rm -rf "$BUILD_DIR"
fi

echo "==> Building..."
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"
cmake "$ROOT"
make -j"$(sysctl -n hw.ncpu)"

echo ""
echo "==> Built:"
echo "    $BUILD_DIR/faxedit/eoe"
echo "    $BUILD_DIR/faxiscripts/eoe-cli"

if [ -n "$RUN_BINARY" ]; then
    echo ""
    echo "==> Running $RUN_BINARY..."
    "$BUILD_DIR/$RUN_DIRECTORY/$RUN_BINARY"
fi
