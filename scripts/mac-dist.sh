#!/bin/bash
set -e

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

APP_NAME="Echoes of Eolis"
APP_BUNDLE="EchoesOfEolis.app"
DMG_NAME="EchoesOfEolis.dmg"
BUILD_DIR="$ROOT/build"
# Staging directory holding everything that ends up in the DMG.
STAGE_DIR="$ROOT/build/dmg-stage"
# faxiscripts is a terminal application, so it ships beside the app bundle
# rather than inside it, together with its own copy of eoe_config.xml.
CLI_DIR="$STAGE_DIR/faxiscripts"

echo "==> Checking dependencies..."
if ! command -v dylibbundler &> /dev/null; then
    echo "Installing dylibbundler..."
    brew install dylibbundler
fi
if ! command -v create-dmg &> /dev/null; then
    echo "Installing create-dmg..."
    brew install create-dmg
fi

"$ROOT/scripts/mac-build.sh" --no-run

# CMake copies eoe_config.xml next to each executable.
FAXEDIT_BIN="$BUILD_DIR/faxedit/faxedit"
FAXISCRIPTS_BIN="$BUILD_DIR/faxiscripts/faxiscripts"
CONFIG_XML="$BUILD_DIR/faxedit/eoe_config.xml"
for f in "$FAXEDIT_BIN" "$FAXISCRIPTS_BIN" "$CONFIG_XML"; do
    if [ ! -f "$f" ]; then
        echo "Expected build output missing: $f" >&2
        exit 1
    fi
done

echo "==> Creating icon..."
ICON_SRC="$ROOT/faxedit/assets/eoe_icon_256x256.png"
ICONSET="$ROOT/faxedit/assets/eoe_icon_256x256.iconset"
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
iconutil -c icns "$ICONSET" -o "$ROOT/scripts/icon.icns"
rm -rf "$ICONSET"

echo "==> Creating app bundle (faxedit)..."
rm -rf "$APP_BUNDLE"
mkdir -p "$APP_BUNDLE/Contents/MacOS"
mkdir -p "$APP_BUNDLE/Contents/Frameworks"
mkdir -p "$APP_BUNDLE/Contents/Resources"

cp "$FAXEDIT_BIN" "$APP_BUNDLE/Contents/MacOS/faxedit_bin"
cp "$CONFIG_XML" "$APP_BUNDLE/Contents/Resources/"
cp "$ROOT/scripts/icon.icns" "$APP_BUNDLE/Contents/Resources/"

cat > "$APP_BUNDLE/Contents/MacOS/faxedit" << 'LAUNCHER'
#!/bin/bash
cd "$(dirname "$0")/../Resources"
exec "$(dirname "$0")/faxedit_bin" "$@"
LAUNCHER
chmod +x "$APP_BUNDLE/Contents/MacOS/faxedit"

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
    -x "$APP_BUNDLE/Contents/MacOS/faxedit_bin" \
    -d "$APP_BUNDLE/Contents/Frameworks/" \
    -p @executable_path/../Frameworks/

echo "==> Staging DMG contents..."
rm -rf "$STAGE_DIR"
mkdir -p "$CLI_DIR"
cp -R "$APP_BUNDLE" "$STAGE_DIR/"

# faxiscripts links only against system libraries, so it needs no dylib bundling.
cp "$FAXISCRIPTS_BIN" "$CLI_DIR/faxiscripts"
cp "$CONFIG_XML" "$CLI_DIR/eoe_config.xml"
cp "$ROOT/util/eoe_config_override-advanced.xml" "$CLI_DIR/"

cat > "$CLI_DIR/README.txt" << 'EOF'
FaxIScripts - command-line assembler and disassembler for Faxanadu scripts,
music and other data that does not lend itself to GUI editing.

This is a terminal application. Copy this folder somewhere convenient, open
Terminal in it and run:

    ./faxiscripts

to see the available commands.

eoe_config.xml is part of the application and should never be modified.
To enable advanced modding features, rename eoe_config_override-advanced.xml
to eoe_config_override.xml and edit it as needed. Documentation:
https://github.com/kaimitai/FaxIScripts/blob/master/docs/advanced_doc.md

macOS Gatekeeper: the binary is not code signed. The first time you run it you
may need to allow it under System Settings -> Privacy & Security.
EOF

echo "==> Creating DMG..."
rm -f "$DMG_NAME"
create-dmg \
    --volname "$APP_NAME" \
    --window-size 620 420 \
    --icon-size 100 \
    --icon "$APP_BUNDLE" 150 150 \
    --app-drop-link 470 150 \
    --icon "faxiscripts" 310 310 \
    "$DMG_NAME" \
    "$STAGE_DIR"

rm -rf "$STAGE_DIR"

echo ""
echo "==> Done! Created $DMG_NAME"
echo "    Contents: $APP_BUNDLE (GUI editor) and faxiscripts/ (command-line tool)"
echo "    Note: recipients will need to right-click -> Open the first time"
echo "    due to macOS Gatekeeper (no code signing)."
