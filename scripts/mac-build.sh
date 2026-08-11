#!/bin/bash
set -e

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

BUILD_DIR="$ROOT/build"

RUN_TARGET="faxedit"

while [ $# -gt 0 ]; do
    case "$1" in
        --no-run)      RUN_TARGET="" ;;
        --run-cli)     RUN_TARGET="faxiscripts" ;;
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
echo "    $BUILD_DIR/faxedit/faxedit"
echo "    $BUILD_DIR/faxiscripts/faxiscripts"

if [ -n "$RUN_TARGET" ]; then
    echo ""
    echo "==> Running $RUN_TARGET..."
    # Run from the executable's directory so eoe_config.xml is picked up.
    cd "$BUILD_DIR/$RUN_TARGET"
    ./"$RUN_TARGET"
fi
