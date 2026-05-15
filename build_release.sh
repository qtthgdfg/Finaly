#!/bin/bash

set -e

echo "=== Building Monero Miner Release APK ==="
echo ""

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")/android"

cd "$PROJECT_DIR"

# Check for keystore properties
if [ ! -f "app/monerominer.keystore" ]; then
    echo "❌ Keystore not found! Run create_keystore.sh first"
    exit 1
fi

# Get version info
VERSION_CODE=$(grep "versionCode" app/build.gradle | awk '{print $2}')
VERSION_NAME=$(grep "versionName" app/build.gradle | awk '{print $2}' | tr -d '"')

echo "Version: $VERSION_NAME ($VERSION_CODE)"
echo ""

# Clean build
echo "Cleaning..."
./gradlew clean

# Build release APK
echo "Building release APK..."
./gradlew assembleRelease

# Check if build succeeded
APK_PATH="app/build/outputs/apk/release"
if ls "$APK_PATH"/*.apk 1> /dev/null 2>&1; then
    echo ""
    echo "✅ Build successful!"
    echo ""
    
    # Display APK info
    for apk in "$APK_PATH"/*.apk; do
        echo "APK: $(basename "$apk")"
        echo "Size: $(du -sh "$apk" | cut -f1)"
        echo "Architecture: $(echo "$apk" | grep -o 'arm64\|armeabi')"
        
        # Check native libs
        echo "Native libs:"
        unzip -l "$apk" | grep "lib/.*\.so" | awk '{print "  " $NF}'
        echo ""
    done
    
    # Copy to dist folder
    DIST_DIR="$PROJECT_DIR/dist"
    mkdir -p "$DIST_DIR"
    cp "$APK_PATH"/*.apk "$DIST_DIR/"
    echo "📦 APKs copied to: $DIST_DIR"
    
else
    echo "❌ Build failed"
    exit 1
fi
