#!/bin/bash
set -e

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

mkdir -p build
cd build
cmake "$ROOT"
make
cp "$ROOT/faxedit/eoe_config.xml" .
./faxedit