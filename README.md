# Loop CGM - Pebble Watchapp

A Pebble smartwatch app for monitoring Loop insulin pump CGM data.

## Features

- 📊 Real-time blood glucose display with trend arrows
- 💉 Insulin on board (IOB) monitoring
- 🍽️ Carbs on board (COB) monitoring
- 🔄 Loop status indicator (ON/OFF)
- 🔋 Pump battery and reservoir levels
- 💊 Bolus requests (requires iPhone confirmation)
- 🥗 Carb entry (requires iPhone confirmation)
- ⚠️ Low/high glucose alerts with vibration
- 📡 Off-grid operation (Bluetooth only)

## CloudPebble Setup

This repo is designed to work with [CloudPebble](https://cloudpebble.net/):

1. Go to [cloudpebble.net](https://cloudpebble.net/)
2. Create new project
3. Select **"Import from GitHub"**
4. Enter this repository URL
5. CloudPeble will automatically pull all source files
6. Click **"Build"** to create the .pbw file

## Project Structure

```
loop-pebble-app/
├── appinfo.json          # App configuration
├── package.json          # PebbleKit message keys
├── build.sh              # Local build script
├── src/
│   ├── main.c            # Main watchapp code
│   └── js/
│       └── pebble-js-app.js  # JavaScript for API calls
├── resources/
│   └── images/
│       ├── icon.png          # Main app icon
│       ├── icon_bolus.png    # Bolus menu icon
│       ├── icon_carbs.png    # Carbs menu icon
│       ├── icon_alert.png    # Alert icon
│       ├── icon_check.png    # Checkmark icon
│       └── icon_reject.png   # Reject icon
└── publish-package/      # Assets for Rebble Appstore upload
```

## Local Build

If you have the Pebble SDK installed:

```bash
./build.sh
```

Or manually:

```bash
pebble build
```

## Installation

### Via CloudPebble
1. Build in CloudPebble
2. Click "Run" to launch in emulator
3. Or click "Install on Phone" to send to your watch

### Via Rebble Appstore
1. Build the .pbw file
2. Go to [dev-portal.rebble.io](https://dev-portal.rebble.io/)
3. Upload the .pbw and publish

### Via Direct Install
```bash
pebble install --phone <phone-ip>
```

## Requirements

- Pebble smartwatch (any model)
- [Rebble app](https://rebble.io/howto/) installed on iPhone
- [Loop iOS app](https://github.com/MinimusClawdius/LoopWorkspace) with PebbleService integration

## Safety

All commands (bolus/carbs) require explicit confirmation on your iPhone before execution. Commands expire after 5 minutes if not confirmed.

## Related

- [LoopWorkspace](https://github.com/MinimusClawdius/LoopWorkspace) - Main Loop app with PebbleService
- [Rebble](https://rebble.io/) - Keeping Pebble alive
- [Pebble Developer Docs](https://developer.rebble.io/)

## License

MIT
