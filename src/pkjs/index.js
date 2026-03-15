// Loop CGM Matrix - PebbleKit JS with Clay Settings

var Clay = require('pebble-clay');
var clayConfig = require('./config');
var clay = new Clay(clayConfig);

// API configuration
var API_BASE = 'http://127.0.0.1:8080';
var TREND_MAP = {
  'UP_UP_UP': 1, 'UP_UP': 2, 'UP': 3, 'FLAT': 4,
  'DOWN': 5, 'DOWN_DOWN': 6, 'DOWN_DOWN_DOWN': 7
};

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

function sendToWatch(data) {
  var msg = {};
  
  if (data.cgm && data.cgm.glucose !== null) {
    msg.KEY_GLUCOSE = Math.round(data.cgm.glucose);
  }
  if (data.cgm && data.cgm.trend) {
    msg.KEY_TREND = TREND_MAP[data.cgm.trend] || 0;
  }
  if (data.cgm && data.cgm.date) {
    msg.KEY_GLUCOSE_DATE = Math.floor(new Date(data.cgm.date).getTime() / 1000);
  }
  if (data.loop && data.loop.iob !== null) {
    msg.KEY_IOB = Math.round(data.loop.iob * 10);
  }
  if (data.loop) {
    msg.KEY_IS_CLOSED_LOOP = data.loop.isClosedLoop ? 1 : 0;
  }
  if (data.pump && data.pump.battery !== null) {
    msg.KEY_BATTERY = Math.round(data.pump.battery);
  }
  
  Pebble.sendAppMessage(msg,
    function() { console.log('Sent'); },
    function(e) { console.log('Send failed: ' + JSON.stringify(e)); }
  );
}

// Handle watch requests
Pebble.addEventListener('appmessage', function(e) {
  if (e.payload && e.payload.KEY_REQUEST_DATA) {
    fetchCGMData();
  }
});

// Handle Clay settings
Pebble.addEventListener('webviewclosed', function(e) {
  if (e && e.response) {
    var settings = clay.getSettings(e.response);
    // Send theme setting to watch
    if (settings.KEY_THEME) {
      Pebble.sendAppMessage({ KEY_THEME: parseInt(settings.KEY_THEME.value) });
    }
  }
});

// Ready
Pebble.addEventListener('ready', function() {
  console.log('Loop CGM ready');
  fetchCGMData();
});

// Auto-refresh every 5 minutes
setInterval(fetchCGMData, 5 * 60 * 1000);
