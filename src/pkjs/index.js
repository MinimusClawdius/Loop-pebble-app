/*
 * Loop CGM Watchface - PebbleKit JavaScript with Settings
 */

var API_BASE = 'http://127.0.0.1:8080';

var THEME_AURORA = 0;
var THEME_WHITE_CYAN = 1;
var THEME_DARK_WARM = 2;
var THEME_PASTEL = 3;
var THEME_OCEAN = 4;
var THEME_MATRIX_SUBTLE = 5;
var THEME_MATRIX_BRIGHT = 6;

var TREND_MAP = {
  'UP_UP_UP': 1,
  'UP_UP': 2,
  'UP': 3,
  'FLAT': 4,
  'DOWN': 5,
  'DOWN_DOWN': 6,
  'DOWN_DOWN_DOWN': 7
};

// Load saved theme
var savedTheme = parseInt(localStorage.getItem('loop_theme') || THEME_MATRIX_BRIGHT);

function fetchCGMData() {
  var xhr = new XMLHttpRequest();
  xhr.open('GET', API_BASE + '/api/all', true);
  xhr.timeout = 10000;
  
  xhr.onload = function() {
    if (xhr.status === 200) {
      try {
        var data = JSON.parse(xhr.responseText);
        sendToWatch(data);
      } catch (e) {
        console.log('Parse error: ' + e);
      }
    }
  };
  
  xhr.ontimeout = function() { console.log('Timeout'); };
  xhr.onerror = function() { console.log('Error'); };
  xhr.send();
}

function sendToWatch(data) {
  var message = { KEY_THEME: savedTheme };
  
  if (data.cgm) {
    if (data.cgm.glucose !== null) message.KEY_GLUCOSE = Math.round(data.cgm.glucose);
    if (data.cgm.trend) message.KEY_TREND = TREND_MAP[data.cgm.trend] || 0;
    if (data.cgm.date) {
      message.KEY_GLUCOSE_DATE = Math.floor(new Date(data.cgm.date).getTime() / 1000);
    }
  }
  
  if (data.loop) {
    if (data.loop.iob !== null) message.KEY_IOB = Math.round(data.loop.iob * 10);
    message.KEY_IS_CLOSED_LOOP = data.loop.isClosedLoop ? 1 : 0;
  }
  
  if (data.pump && data.pump.battery !== null) {
    message.KEY_BATTERY = Math.round(data.pump.battery);
  }
  
  Pebble.sendAppMessage(message,
    function() { console.log('Sent'); },
    function(e) { console.log('Send failed: ' + JSON.stringify(e)); }
  );
}

// Settings page
var Clay = require('pebble-clay');
var clayConfig = [
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
    "defaultValue": THEME_MATRIX_BRIGHT.toString(),
    "label": "Theme",
    "options": [
      { "label": "Aurora (Northern Lights)", "value": "0" },
      { "label": "Clean White + Cyan", "value": "1" },
      { "label": "Dark + Warm Orange", "value": "2" },
      { "label": "Soft Pastel Purple", "value": "3" },
      { "label": "Ocean Blue Waves", "value": "4" },
      { "label": "Matrix Rain (Subtle)", "value": "5" },
      { "label": "Matrix Bright", "value": "6" }
    ]
  }
];

var clay = new Clay(clayConfig);

// Handle messages from watch
Pebble.addEventListener('appmessage', function(e) {
  if (e.payload && e.payload.KEY_REQUEST_DATA) {
    fetchCGMData();
  }
});

// Handle settings
Pebble.addEventListener('webviewclosed', function(e) {
  if (e && e.response) {
    var settings = JSON.parse(e.response);
    if (settings.KEY_THEME !== undefined) {
      Pebble.sendAppMessage({
        KEY_THEME: parseInt(settings.KEY_THEME)
      });
    }
  }
});

Pebble.addEventListener('ready', function() {
  console.log('PebbleKit JS ready');
  fetchCGMData();
});

function fetchCGMData() {
  var xhr = new XMLHttpRequest();
  xhr.open('GET', API_BASE + '/api/all', true);
  xhr.timeout = 10000;
  
  xhr.onload = function() {
    if (xhr.status === 200) {
      try {
        sendToWatch(JSON.parse(xhr.responseText));
      } catch (e) {
        console.log('Parse error: ' + e);
      }
    }
  };
  
  xhr.onerror = function() {
    console.log('Connection error');
  };
  
  xhr.send();
}

var TREND_MAP = {
  'UP_UP_UP': 1, 'UP_UP': 2, 'UP': 3, 'FLAT': 4,
  'DOWN': 5, 'DOWN_DOWN': 6, 'DOWN_DOWN_DOWN': 7
};

function sendToWatch(data) {
  var msg = {};
  if (data.cgm && data.cgm.glucose !== null) msg.KEY_GLUCOSE = Math.round(data.cgm.glucose);
  if (data.cgm && data.cgm.trend) msg.KEY_TREND = TREND_MAP[data.cgm.trend] || 0;
  if (data.cgm && data.cgm.date) msg.KEY_GLUCOSE_DATE = Math.floor(new Date(data.cgm.date).getTime() / 1000);
  if (data.loop && data.loop.iob !== null) msg.KEY_IOB = Math.round(data.loop.iob * 10);
  if (data.loop) msg.KEY_IS_CLOSED_LOOP = data.loop.isClosedLoop ? 1 : 0;
  if (data.pump && data.pump.battery !== null) msg.KEY_BATTERY = Math.round(data.pump.battery);
  
  Pebble.sendAppMessage(msg);
}

setInterval(fetchCGMData, 5 * 60 * 1000);
