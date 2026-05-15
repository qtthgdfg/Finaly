#!/bin/bash

echo "=== Setting up Android Build Environment ==="

# Check Java
if ! command -v java &> /dev/null; then
    echo "❌ Java not found. Installing..."
    sudo apt-get update
    sudo apt-get install -y openjdk-17-jdk
fi

# Check Android SDK
if [ -z "$ANDROID_HOME" ] && [ -z "$ANDROID_SDK_ROOT" ]; then
    echo "⚠️ Android SDK not found"
    echo "Please install Android Studio or set ANDROID_HOME"
    echo ""
    echo "Manual installation:"
    echo "1. Download Android Studio: https://developer.android.com/studio"
    echo "2. Or use command line:"
    echo "   mkdir -p ~/Android/Sdk"
    echo "   cd ~/Android/Sdk"
    echo "   wget https://dl.google.com/android/repository/commandlinetools-linux-*.zip"
    echo ""
    read -p "Do you want to install command line tools? (y/n) " -n 1 -r
    echo
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        # Create SDK directory
        ANDROID_SDK_ROOT="$HOME/Android/Sdk"
        mkdir -p "$ANDROID_SDK_ROOT/cmdline-tools"
        
        # Download command line tools
        cd /tmp
        wget https://dl.google.com/android/repository/commandlinetools-linux-11076708_latest.zip
        unzip commandlinetools-linux-*.zip
        mv cmdline-tools "$ANDROID_SDK_ROOT/cmdline-tools/latest"
        
        # Set environment variables
        echo "export ANDROID_SDK_ROOT=$ANDROID_SDK_ROOT" >> ~/.bashrc
        echo "export ANDROID_HOME=$ANDROID_SDK_ROOT" >> ~/.bashrc
        echo 'export PATH=$PATH:$ANDROID_SDK_ROOT/cmdline-tools/latest/bin' >> ~/.bashrc
        echo 'export PATH=$PATH:$ANDROID_SDK_ROOT/platform-tools' >> ~/.bashrc
        
        source ~/.bashrc
        
        # Install required SDK components
        yes | "$ANDROID_SDK_ROOT/cmdline-tools/latest/bin/sdkmanager" --licenses
        "$ANDROID_SDK_ROOT/cmdline-tools/latest/bin/sdkmanager" \
            "platform-tools" \
            "platforms;android-34" \
            "build-tools;34.0.0" \
            "ndk;26.1.10909125" \
            "cmake;3.22.1"
    fi
fi

# Clone RandomX if not present
if [ ! -d "../../libs/randomx" ]; then
    echo "Cloning RandomX..."
    cd ../..
    git clone https://github.com/tevador/RandomX.git libs/randomx
    cd android
fi

# Download nlohmann/json
if [ ! -f "../../libs/nlohmann/json.hpp" ]; then
    echo "Downloading JSON library..."
    mkdir -p ../../libs/nlohmann
    wget -O ../../libs/nlohmann/json.hpp \
        https://github.com/nlohmann/json/releases/latest/download/json.hpp
fi

# Setup Gradle wrapper
if [ ! -f "gradlew" ]; then
    echo "Setting up Gradle wrapper..."
    gradle wrapper --gradle-version 8.5
fi

echo ""
echo "✅ Setup complete!"
echo ""
echo "Next steps:"
echo "1. Set wallet address in config or app"
echo "2. Build: cd android && ./gradlew assembleDebug"
echo "3. Install: adb install app/build/outputs/apk/debug/app-debug.apk"
