# Loop CGM Matrix - Pebble Watchface

A beautiful, animated CGM watchface for Loop with 8 selectable themes including the signature Matrix Rain effect.

## Themes

| # | Theme | Description |
|---|-------|-------------|
| 0 | **Aurora** | Northern lights waves with starfield |
| 1 | **Clean White + Cyan** | Light, medical-looking with cyan accents |
| 2 | **Dark + Warm Orange** | Cozy evening vibe with orange chart |
| 3 | **Soft Pastel Purple** | Gentle lavender with purple chart |
| 4 | **Ocean Blue Waves** | Fresh gradient blue background |
| 5 | **Matrix Rain (Subtle)** | Dark cascading characters |
| 6 | **Matrix Bright** | Intense Matrix effect (default) |

## Features

- 📊 Real-time blood glucose display with color coding
- 📈 Animated chart with last 2 hours of readings
- 💉 IOB (Insulin on Board) display
- 🔄 Loop status indicator
- 🔋 Battery level
- ⏱️ Time since last reading
- 🎨 8 beautiful themes
- 🎬 Animated backgrounds (Matrix rain, Aurora waves)
- ⚙️ Settings accessible via Pebble app

## Display

```
┌─────────────────────┐
│       12:30         │
│                     │
│       125  >        │ ← Large BG + trend
│                     │
│  ┌───────────────┐  │
│  │ ~~~chart~~~●  │  │ ← Animated chart
│  └───────────────┘  │
│                     │
│      IOB 2.5U       │
│  >>> LOOPING <<<    │
│     SYS: 2m AGO     │
│                     │
│                  85%│
└─────────────────────┘
```

## Color Coding

- 🟢 **Green**: In range (70-180 mg/dL)
- 🟠 **Orange**: High (>180 mg/dL)
- 🔴 **Red**: Low (<70 mg/dL)

## Settings

1. Open Pebble app on your phone
2. Find "Loop CGM Matrix" in your watchfaces
3. Tap the settings gear icon
4. Select your preferred theme
5. Save

## Installation

### Via CloudPebble

1. Import this GitHub repo into CloudPebble
2. Build
3. Install to your Pebble

### Via Rebble Appstore

Search for "Loop CGM Matrix" in the Pebble app.

## Requirements

- Pebble smartwatch (any model)
- [Rebble app](https://rebble.io/howto/) on iPhone
- [Loop iOS app](https://github.com/MinimusClawdius/LoopWorkspace) with PebbleService

## Companion App

Install [Loop Actions](https://github.com/MinimusClawdius/Loop-actions-app) for bolus and carb entry from your Pebble.

## Demo Mode

The watchface includes demo data so you can see how it looks without connecting to Loop. To connect to real data, ensure the Loop app with PebbleService is running on your iPhone.

## Technical Details

- Animation: 80ms frame rate (~12fps)
- Chart: Last 9 readings displayed
- Settings: Stored in persistent storage
- API: Connects to localhost:8080 on iPhone

## License

MIT
