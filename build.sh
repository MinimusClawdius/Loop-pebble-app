#!/bin/bash
#
# Loop CGM Pebble App Build Script
# Builds and optionally deploys to Rebble Developer Portal
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

echo "================================"
echo "Loop CGM - Pebble Build Script"
echo "================================"
echo ""

# Check for Pebble SDK
if ! command -v pebble &> /dev/null; then
    echo "❌ Pebble SDK not found!"
    echo ""
    echo "Install options:"
    echo "  macOS:  brew install pebble-sdk"
    echo "  Linux:  pip install pebble-sdk"
    echo "  Docker: docker run -v \$PWD:/app rebble/pebble-sdk pebble build"
    echo ""
    exit 1
fi

echo "✅ Pebble SDK found: $(pebble --version 2>/dev/null || echo 'installed')"
echo ""

# Clean previous builds
echo "🧹 Cleaning previous builds..."
rm -rf build/
rm -f *.pbw

# Build the app
echo "🔨 Building Loop CGM..."
pebble build

if [ $? -eq 0 ]; then
    echo ""
    echo "✅ Build successful!"
    echo ""
    
    # Find the built .pbw file
    PBW_FILE=$(find build -name "*.pbw" 2>/dev/null | head -1)
    
    if [ -n "$PBW_FILE" ]; then
        # Copy to current directory with friendly name
        cp "$PBW_FILE" "./loop-cgm.pbw"
        echo "📦 Package: loop-cgm.pbw"
        echo "📏 Size: $(du -h loop-cgm.pbw | cut -f1)"
        echo ""
    fi
else
    echo ""
    echo "❌ Build failed!"
    exit 1
fi

# Deployment options
echo "================================"
echo "Deployment Options:"
echo "================================"
echo ""
echo "1️⃣  Install to local Pebble (via phone):"
echo "    pebble install --phone <phone-ip>"
echo ""
echo "2️⃣  Install via cloud (Rebble):"
echo "    pebble install --cloudpebble"
echo ""
echo "3️⃣  Deploy to Rebble Appstore:"
echo "    a. Go to https://dev-portal.rebble.io/"
echo "    b. Click 'Add a Watchapp'"
echo "    c. Upload loop-cgm.pbw"
echo "    d. Add screenshots and description"
echo "    e. Publish!"
echo ""
echo "4️⃣  Side-load directly:"
echo "    - Transfer loop-cgm.pbw to your phone"
echo "    - Open with Pebble app"
echo ""

# If --install flag is passed, try to install
if [ "$1" == "--install" ]; then
    echo "📱 Installing to Pebble..."
    if [ -n "$2" ]; then
        pebble install --phone "$2"
    else
        pebble install --cloudpebble
    fi
fi

# If --deploy flag is passed, show deployment instructions
if [ "$1" == "--deploy" ]; then
    echo "🚀 For Rebble Appstore deployment:"
    echo ""
    echo "1. Create a developer account at https://dev-portal.rebble.io/"
    echo "2. Click 'Add a Watchapp'"
    echo "3. Fill in:"
    echo "   - Title: Loop CGM Monitor"
    echo "   - Category: Health & Fitness"
    echo "   - Source Code: https://github.com/MinimusClawdius/LoopWorkspace"
    echo "4. Upload loop-cgm.pbw as a release"
    echo "5. Add screenshots (take from Pebble emulator or phone)"
    echo "6. Add description:"
    echo "   'Monitor your Loop insulin pump CGM data directly"
    echo "    on your Pebble watch. View glucose trends, IOB,"
    echo "    and request bolus/carb entries with iPhone confirmation.'"
    echo "7. Publish!"
fi

echo ""
echo "Done! 🎉"
