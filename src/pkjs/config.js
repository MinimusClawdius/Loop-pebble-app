// Clay configuration for Loop CGM Settings

module.exports = [
  {
    "type": "heading",
    "defaultValue": "Loop CGM Settings"
  },
  {
    "type": "text",
    "defaultValue": "Choose your watchface theme"
  },
  {
    "type": "select",
    "messageKey": "KEY_THEME",
    "defaultValue": 6,
    "label": "Theme",
    "options": [
      { "label": "Aurora Northern Lights", "value": 0 },
      { "label": "Clean White + Cyan", "value": 1 },
      { "label": "Dark + Warm Orange", "value": 2 },
      { "label": "Soft Pastel Purple", "value": 3 },
      { "label": "Ocean Blue", "value": 4 },
      { "label": "Matrix Rain (Subtle)", "value": 5 },
      { "label": "Matrix Bright", "value": 6 }
    ]
  },
  {
    "type": "slider",
    "messageKey": "KEY_LOW_THRESHOLD",
    "defaultValue": 70,
    "label": "Low Alert (mg/dL)",
    "min": 50,
    "max": 90,
    "step": 5
  },
  {
    "type": "slider",
    "messageKey": "KEY_HIGH_THRESHOLD",
    "defaultValue": 180,
    "label": "High Alert (mg/dL)",
    "min": 150,
    "max": 250,
    "step": 5
  }
];
