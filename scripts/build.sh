#!/bin/bash

# Exit on error
set -e

# Directory setup
mkdir -p build dist exports

# Build the application
make clean
make

# Before creating the app bundle, create the icon
mkdir -p build/icon.iconset
for size in 16 32 64 128 256 512; do
    convert assets/icon.png -resize ${size}x${size} build/icon.iconset/icon_${size}x${size}.png
    convert assets/icon.png -resize $((size*2))x$((size*2)) build/icon.iconset/icon_${size}x${size}@2x.png
done

# Create icns file
iconutil -c icns -o build/icon.icns build/icon.iconset

# Create app bundle structure
APP_NAME="TasteWarp"
APP_BUNDLE="dist/$APP_NAME.app"
mkdir -p "$APP_BUNDLE/Contents/"{MacOS,Resources}

# Copy executable and resources
cp tastewarp "$APP_BUNDLE/Contents/MacOS/"
cp wav.wav "$APP_BUNDLE/Contents/Resources/"
cp build/icon.icns "$APP_BUNDLE/Contents/Resources/"

# Create Info.plist
cat > "$APP_BUNDLE/Contents/Info.plist" << EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleExecutable</key>
    <string>tastewarp</string>
    <key>CFBundleIconFile</key>
    <string>icon</string>
    <key>CFBundleIdentifier</key>
    <string>com.un1crom.tastewarp</string>
    <key>CFBundleName</key>
    <string>TasteWarp</string>
    <key>CFBundlePackageType</key>
    <string>APPL</string>
    <key>CFBundleShortVersionString</key>
    <string>1.0</string>
    <key>LSMinimumSystemVersion</key>
    <string>10.10</string>
    <key>NSHighResolutionCapable</key>
    <true/>
    <key>LSApplicationCategoryType</key>
    <string>public.app-category.music</string>
</dict>
</plist>
EOF

# Create PkgInfo
echo "APPL????" > "$APP_BUNDLE/Contents/PkgInfo"

# Create entitlements file
cat > build/entitlements.plist << EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>com.apple.security.cs.allow-jit</key>
    <true/>
    <key>com.apple.security.cs.allow-unsigned-executable-memory</key>
    <true/>
    <key>com.apple.security.cs.disable-library-validation</key>
    <true/>
    <key>com.apple.security.inherit</key>
    <true/>
</dict>
</plist>
EOF

# Sign the application
if [ -n "$APPLE_DEVELOPER_ID" ]; then
    echo "Signing with Developer ID: $APPLE_DEVELOPER_ID"
    codesign --force --options runtime \
        --entitlements build/entitlements.plist \
        --sign "$APPLE_DEVELOPER_ID" \
        --timestamp \
        --deep \
        --strict \
        --verbose \
        "$APP_BUNDLE" || echo "Warning: Signing failed"
else
    echo "No Developer ID provided - skipping signing"
fi

# Create DMG
create-dmg \
    --volname "$APP_NAME" \
    --window-pos 200 120 \
    --window-size 800 400 \
    --icon-size 100 \
    --icon "$APP_NAME.app" 200 190 \
    --hide-extension "$APP_NAME.app" \
    --app-drop-link 600 185 \
    --no-internet-enable \
    "$APP_NAME.dmg" \
    "dist/" || exit 1

# Sign the DMG if we have a developer ID
if [ -n "$APPLE_DEVELOPER_ID" ]; then
    echo "Signing DMG with Developer ID: $APPLE_DEVELOPER_ID"
    codesign --force --sign "$APPLE_DEVELOPER_ID" \
        --options runtime "$APP_NAME.dmg" || echo "Warning: DMG signing failed"
fi 