#!/bin/bash
set -e

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

APP_NAME="Echoes of Eolis"
APP_BUNDLE="EchoesOfEolis.app"
DMG_NAME="EchoesOfEolis.dmg"

echo "==> Checking dependencies..."
if ! command -v dylibbundler &> /dev/null; then
    echo "Installing dylibbundler..."
    brew install dylibbundler
fi
if ! command -v create-dmg &> /dev/null; then
    echo "Installing create-dmg..."
    brew install create-dmg
fi

echo "==> Building..."
mkdir -p build
cd build
cmake "$ROOT"
make
cp "$ROOT/faxedit/eoe_config.xml" .
cd "$ROOT"

echo "==> Creating icon..."
ICON_SRC="$ROOT/scripts/assets/icon.png"
ICONSET="$ROOT/scripts/assets/icon.iconset"
rm -rf "$ICONSET"
mkdir -p "$ICONSET"
sips -z 16 16     "$ICON_SRC" --out "$ICONSET/icon_16x16.png"
sips -z 32 32     "$ICON_SRC" --out "$ICONSET/icon_16x16@2x.png"
sips -z 32 32     "$ICON_SRC" --out "$ICONSET/icon_32x32.png"
sips -z 64 64     "$ICON_SRC" --out "$ICONSET/icon_32x32@2x.png"
sips -z 128 128   "$ICON_SRC" --out "$ICONSET/icon_128x128.png"
sips -z 256 256   "$ICON_SRC" --out "$ICONSET/icon_128x128@2x.png"
sips -z 256 256   "$ICON_SRC" --out "$ICONSET/icon_256x256.png"
sips -z 512 512   "$ICON_SRC" --out "$ICONSET/icon_256x256@2x.png"
sips -z 512 512   "$ICON_SRC" --out "$ICONSET/icon_512x512.png"
sips -z 1024 1024 "$ICON_SRC" --out "$ICONSET/icon_512x512@2x.png"
iconutil -c icns "$ICONSET" -o "$ROOT/scripts/assets/icon.icns"
rm -rf "$ICONSET"

echo "==> Creating app bundle..."
rm -rf "$APP_BUNDLE"
mkdir -p "$APP_BUNDLE/Contents/MacOS"
mkdir -p "$APP_BUNDLE/Contents/Frameworks"
mkdir -p "$APP_BUNDLE/Contents/Resources"

cp build/faxedit "$APP_BUNDLE/Contents/MacOS/"
cp build/eoe_config.xml "$APP_BUNDLE/Contents/MacOS/"
cp "$ROOT/scripts/assets/icon.icns" "$APP_BUNDLE/Contents/Resources/"

cat > "$APP_BUNDLE/Contents/Info.plist" << 'EOF'
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleExecutable</key>
    <string>faxedit</string>
    <key>CFBundleIdentifier</key>
    <string>com.kaimitai.faxedit</string>
    <key>CFBundleName</key>
    <string>Echoes of Eolis</string>
    <key>CFBundleVersion</key>
    <string>1.0</string>
    <key>CFBundlePackageType</key>
    <string>APPL</string>
    <key>CFBundleIconFile</key>
    <string>icon</string>
</dict>
</plist>
EOF

echo "==> Bundling SDL3 library..."
dylibbundler -od -b \
    -x "$APP_BUNDLE/Contents/MacOS/faxedit" \
    -d "$APP_BUNDLE/Contents/Frameworks/" \
    -p @executable_path/../Frameworks/

echo "==> Creating DMG..."
rm -f "$DMG_NAME"
create-dmg \
    --volname "$APP_NAME" \
    --window-size 500 300 \
    --icon-size 128 \
    --icon "$APP_BUNDLE" 150 150 \
    --app-drop-link 350 150 \
    "$DMG_NAME" \
    "$APP_BUNDLE"

echo ""
echo "==> Done! Created $DMG_NAME"
echo "    Note: recipients will need to right-click -> Open the first time"
echo "    due to macOS Gatekeeper (no code signing)."