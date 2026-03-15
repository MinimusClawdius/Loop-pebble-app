/*
 * Loop CGM Monitor - Pebble JavaScript
 * 
 * Fetches CGM data from iPhone's local HTTP server
 * Sends bolus/carb commands with iOS confirmation flow
 */

var API_BASE = 'http://127.0.0.1:8080';

function fetchCGMData() {
    var xhr = new XMLHttpRequest();
    xhr.open('GET', API_BASE + '/api/all', true);
    xhr.timeout = 10000;
    
    xhr.onload = function() {
        if (xhr.status === 200) {
            try {
                var data = JSON.parse(xhr.responseText);
                sendDataToWatch(data);
            } catch (e) {
                console.log('JSON parse error: ' + e);
            }
        } else {
            console.log('HTTP error: ' + xhr.status);
        }
    };
    
    xhr.ontimeout = function() {
        console.log('Request timeout');
    };
    
    xhr.onerror = function() {
        console.log('Request error');
    };
    
    xhr.send();
}

function sendDataToWatch(data) {
    var message = {};
    
    if (data.cgm && data.cgm.glucose !== null) {
        message.KEY_GLUCOSE = Math.round(data.cgm.glucose);
    }
    if (data.cgm && data.cgm.trend) {
        message.KEY_TREND = data.cgm.trend;
    }
    if (data.loop && data.loop.iob !== null) {
        message.KEY_IOB = Math.round(data.loop.iob * 10);
    }
    if (data.loop) {
        message.KEY_IS_CLOSED_LOOP = data.loop.isClosedLoop ? 1 : 0;
    }
    if (data.loop && data.loop.cob !== null) {
        message.KEY_COB = Math.round(data.loop.cob);
    }
    if (data.pump && data.pump.battery !== null) {
        message.KEY_BATTERY = Math.round(data.pump.battery);
    }
    
    Pebble.sendAppMessage(message, 
        function() { console.log('Data sent to watch'); },
        function(e) { console.log('Error sending to watch: ' + JSON.stringify(e)); }
    );
}

// Send bolus request (requires iOS confirmation)
function requestBolus(units) {
    var xhr = new XMLHttpRequest();
    xhr.open('POST', API_BASE + '/api/bolus', true);
    xhr.setRequestHeader('Content-Type', 'application/json');
    xhr.timeout = 10000;
    
    xhr.onload = function() {
        if (xhr.status === 202) {
            try {
                var response = JSON.parse(xhr.responseText);
                // Send confirmation request to watch
                Pebble.sendAppMessage({
                    'KEY_COMMAND_STATUS': 1,  // pending
                    'KEY_COMMAND_MSG': response.message || 'Confirm on iPhone'
                });
            } catch (e) {
                console.log('Parse error: ' + e);
            }
        } else {
            Pebble.sendAppMessage({
                'KEY_COMMAND_STATUS': -1,  // error
                'KEY_COMMAND_MSG': 'Request failed'
            });
        }
    };
    
    xhr.onerror = function() {
        Pebble.sendAppMessage({
            'KEY_COMMAND_STATUS': -1,
            'KEY_COMMAND_MSG': 'Connection error'
        });
    };
    
    xhr.send(JSON.stringify({ units: units }));
}

// Send carb entry request (requires iOS confirmation)
function requestCarbEntry(grams, absorptionHours) {
    var xhr = new XMLHttpRequest();
    xhr.open('POST', API_BASE + '/api/carbs', true);
    xhr.setRequestHeader('Content-Type', 'application/json');
    xhr.timeout = 10000;
    
    xhr.onload = function() {
        if (xhr.status === 202) {
            try {
                var response = JSON.parse(xhr.responseText);
                Pebble.sendAppMessage({
                    'KEY_COMMAND_STATUS': 1,  // pending
                    'KEY_COMMAND_MSG': response.message || 'Confirm on iPhone'
                });
            } catch (e) {
                console.log('Parse error: ' + e);
            }
        } else {
            Pebble.sendAppMessage({
                'KEY_COMMAND_STATUS': -1,
                'KEY_COMMAND_MSG': 'Request failed'
            });
        }
    };
    
    xhr.onerror = function() {
        Pebble.sendAppMessage({
            'KEY_COMMAND_STATUS': -1,
            'KEY_COMMAND_MSG': 'Connection error'
        });
    };
    
    xhr.send(JSON.stringify({ 
        grams: grams, 
        absorptionHours: absorptionHours || 3 
    }));
}

// Handle messages from watch
Pebble.addEventListener('appmessage', function(e) {
    var payload = e.payload;
    
    if (payload.KEY_REQUEST_DATA) {
        fetchCGMData();
    } else if (payload.KEY_BOLUS_REQUEST) {
        // Bolus amount in 0.05U increments (stored as integer x20)
        var units = payload.KEY_BOLUS_REQUEST / 20.0;
        requestBolus(units);
    } else if (payload.KEY_CARB_REQUEST) {
        // Carb amount in grams
        var grams = payload.KEY_CARB_REQUEST;
        var absorption = payload.KEY_ABSORPTION_HOURS || 3;
        requestCarbEntry(grams, absorption);
    }
});

Pebble.addEventListener('ready', function() {
    console.log('PebbleKit JS ready');
    fetchCGMData();
});

// Auto-refresh every 5 minutes
setInterval(function() {
    console.log('Auto-refreshing data');
    fetchCGMData();
}, 5 * 60 * 1000);
