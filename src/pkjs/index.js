/*
 * Loop CGM Watchface - PebbleKit JavaScript
 * 
 * Fetches CGM data from iPhone's local HTTP server
 */

var API_BASE = 'http://127.0.0.1:8080';

// Trend mapping: API trend strings to Pebble indices
var TREND_MAP = {
  'UP_UP_UP': 1,
  'UP_UP': 2,
  'UP': 3,
  'FLAT': 4,
  'DOWN': 5,
  'DOWN_DOWN': 6,
  'DOWN_DOWN_DOWN': 7
};

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
        console.log('JSON parse error: ' + e);
      }
    } else {
      console.log('HTTP error: ' + xhr.status);
    }
  };
  
  xhr.ontimeout = function() {
    console.log('Request timeout - is Loop running?');
  };
  
  xhr.onerror = function() {
    console.log('Request error - check connection');
  };
  
  xhr.send();
}

function sendToWatch(data) {
  var message = {};
  
  // CGM glucose value
  if (data.cgm && data.cgm.glucose !== null) {
    message.KEY_GLUCOSE = Math.round(data.cgm.glucose);
  }
  
  // Trend (convert to index)
  if (data.cgm && data.cgm.trend) {
    message.KEY_TREND = TREND_MAP[data.cgm.trend] || 0;
  }
  
  // Glucose date
  if (data.cgm && data.cgm.date) {
    var date = new Date(data.cgm.date);
    message.KEY_GLUCOSE_DATE = Math.floor(date.getTime() / 1000);
  }
  
  // IOB
  if (data.loop && data.loop.iob !== null) {
    message.KEY_IOB = Math.round(data.loop.iob * 10);
  }
  
  // COB
  if (data.loop && data.loop.cob !== null) {
    message.KEY_COB = Math.round(data.loop.cob);
  }
  
  // Loop status
  if (data.loop) {
    message.KEY_IS_CLOSED_LOOP = data.loop.isClosedLoop ? 1 : 0;
  }
  
  // Battery
  if (data.pump && data.pump.battery !== null) {
    message.KEY_BATTERY = Math.round(data.pump.battery);
  }
  
  console.log('Sending: ' + JSON.stringify(message));
  
  Pebble.sendAppMessage(message,
    function() { console.log('Message sent'); },
    function(e) { console.log('Send failed: ' + JSON.stringify(e)); }
  );
}

// Handle watch requests
Pebble.addEventListener('appmessage', function(e) {
  fetchCGMData();
});

Pebble.addEventListener('ready', function() {
  console.log('PebbleKit JS ready');
  fetchCGMData();
});

// Auto-refresh every 5 minutes
setInterval(function() {
  fetchCGMData();
}, 5 * 60 * 1000);
