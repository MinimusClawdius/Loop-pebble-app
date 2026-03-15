# Rebble Appstore Upload Package

Everything you need to publish Loop CGM to the Rebble Appstore.

---

## Step 1: Build the .pbw File

You need to build the .pbw before uploading. Options:

### Option A: CloudPebble (Easiest - No Install)
1. Go to [cloudpebble.net](https://cloudpebble.net/)
2. Create new project: "Loop CGM"
3. Upload all files from the `pebble/` directory:
   - `appinfo.json`
   - `package.json`
   - `src/main.c`
   - `src/js/pebble-js-app.js`
   - `resources/images/*.png`
4. Click "Build"
5. Download the .pbw file

### Option B: Local SDK
```bash
cd pebble/
./build.sh
```
Output: `loop-cgm.pbw`

---

## Step 2: Log into Rebble Developer Portal

1. Go to [dev-portal.rebble.io](https://dev-portal.rebble.io/)
2. Sign in with your Rebble account

---

## Step 3: Create New Watchapp

Click **"Add a Watchapp"** and fill in:

| Field | Value |
|-------|-------|
| **Title** | `Loop CGM Monitor` |
| **Category** | `Health & Fitness` |
| **Source Code URL** | `https://github.com/MinimusClawdius/LoopWorkspace` |
| **Support Email** | Your email |
| **Large Icon** | Upload `icon-large.png` |
| **Small Icon** | Upload `icon-small.png` |

Click **"Create"**

---

## Step 4: Upload Release

1. On the listing page, click **"Add a release"**
2. Upload the `.pbw` file
3. Release notes (copy/paste):

```
v1.0.0 - Initial Release

Monitor your Loop insulin pump CGM data directly on your Pebble watch.

Features:
• Real-time blood glucose display with trend arrows
• Insulin on board (IOB) and carbs on board (COB)
• Loop status indicator (ON/OFF)
• Pump battery and reservoir levels
• Bolus requests (requires iPhone confirmation)
• Carb entry (requires iPhone confirmation)
• Low/high glucose alerts with vibration
• Off-grid operation (Bluetooth only, no internet required)

Requirements:
• Loop iOS app with PebbleService integration
• Pebble smartwatch (any model)
• Rebble app installed on iPhone
```

4. Click **"Save"**
5. Click **"Publish"** next to the release

---

## Step 5: Add Asset Collections

For each platform (Basalt, Chalk, Diorite, Emery), click **"Manage Asset Collections"** → **"Create"**:

### Description (copy/paste for all platforms):
```
Monitor your Loop insulin pump CGM data directly on your Pebble watch.

Features:
• Real-time blood glucose display with trend arrows
• Insulin on board (IOB) monitoring
• Carbs on board (COB) monitoring
• Loop status indicator
• Pump battery and reservoir levels
• Bolus and carb entry with iPhone confirmation
• Low/high glucose alerts
• Off-grid operation (Bluetooth only)

Requirements:
• Loop iOS app with PebbleService integration
• Rebble app on iPhone

Safety:
All commands (bolus/carbs) require explicit confirmation on your iPhone before execution. Commands expire after 5 minutes if not confirmed.

For setup instructions, visit:
https://github.com/MinimusClawdius/LoopWorkspace
```

### Screenshots:
Take screenshots from Pebble emulator or real watch showing:
1. Main CGM screen (glucose + trend)
2. Bolus entry screen
3. Carb entry screen
4. Command menu

### Marketing Banner (optional):
Create a 720x320 banner image with:
- "Loop CGM Monitor" title
- Glucose display preview
- Pebble watch mockup

---

## Step 6: Publish

1. Once all asset collections are complete
2. Click **"Publish"** (or "Publish Privately" for testing)
3. Get your app link!

---

## Files in This Package

| File | Purpose |
|------|---------|
| `icon-large.png` | Large app icon (144x144) |
| `icon-small.png` | Small app icon (48x48) |
| `icon_bolus.png` | Reference: bolus icon |
| `icon_carbs.png` | Reference: carbs icon |
| `UPLOAD-INSTRUCTIONS.md` | This file |

---

## After Publishing

Your app will be available at:
- Web: `https://apps.rebble.io/en_US/application/[app-id]`
- Deep link: `pebble://appstore/[app-id]`

Users can search "Loop CGM" in the Pebble app to find and install it.

---

## Questions?

- Rebble Discord: #app-dev channel
- Email: support@rebble.io
