/*
 * Loop CGM Watchface - PebbleKit JavaScript
 * 
 * Fetches CGM data from iPhone's local HTTP server
 * Sends to watchface for display
 */

var API_BASE = 'http://127.0.0.1:8080';

// Trend arrow mapping for display
var TREND_ARROWS = {
  'UP_UP_UP': '↑↑↑',
  'UP_UP': '↑↑',
  'UP': '↑',
  'FLAT': '→',
  'DOWN': '↓',
  'DOWN_DOWN': '↓↓',
  'DOWN_DOWN_DOWN': '↓↓↓'
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
  
  // Trend arrow
  if (data.cgm && data.cgm.trend) {
    // Convert trend symbol to arrow
    var trend = data.cgm.trend;
    if (TREND_ARROWS[trend]) {
      message.KEY_TREND = TREND_ARROWS[trend];
    } else {
      message.KEY_TREND = trend;
    }
  }
  
  // Glucose date (for time ago calculation)
  if (data.cgm && data.cgm.date) {
    var date = new Date(data.cgm.date);
    message.KEY_GLUCOSE_DATE = Math.floor(date.getTime() / 1000);
  }
  
  // Calculate delta if not provided
  if (data.cgm && data.cgm.glucose !== null) {
    // Delta would come from server if available
    // For now, we'll let the server provide it
  }
  
  // IOB (insulin on board) - send as integer x10
  if (data.loop && data.loop.iob !== null) {
    message.KEY_IOB = Math.round(data.loop.iob * 10);
  }
  
  // COB (carbs on board)
  if (data.loop && data.loop.cob !== null) {
    message.KEY_COB = Math.round(data.loop.cob);
  }
  
  // Loop status (closed loop on/off)
  if (data.loop) {
    message.KEY_IS_CLOSED_LOOP = data.loop.isClosedLoop ? 1 : 0;
  }
  
  // Pump battery
  if (data.pump && data.pump.battery !== null) {
    message.KEY_BATTERY = Math.round(data.pump.battery);
  }
  
  console.log('Sending to watch: ' + JSON.stringify(message));
  
  Pebble.sendAppMessage(message,
    function() {
      console.log('Message sent successfully');
    },
    function(e) {
      console.log('Message send failed: ' + JSON.stringify(e));
    }
  );
}

// Handle watch requests
Pebble.addEventListener('appmessage', function(e) {
  console.log('Watch requested data');
  fetchCGMData();
});

// On ready, fetch initial data
Pebble.addEventListener('ready', function() {
  console.log('PebbleKit JS ready');
  fetchCGMData();
});

// Auto-refresh every 5 minutes
setInterval(function() {
  console.log('Auto-refreshing CGM data');
  fetchCGMData();
}, 5 * 60 * 1000);
