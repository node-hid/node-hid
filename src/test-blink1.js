/**
 * Simple demostration of sendFeatureReport
 * for a blink(1) USB LED by ThingM http://blink1.thingm.com/
 */

'use strict';

var HID = require('../');

var VENDOR_ID = 0x27B8;
var PRODUCT_ID = 0x01ED;
var REPORT_ID = 1;
var REPORT2_ID = 2;
var REPORT_LENGTH = 8;
var REPORT2_LENGTH = 60;

var blink1Version = 0; // to be filled in later

var serial_number = process.argv[2];

var JSONstringifyHex = function(arr) {
  var str='[';
  for (var i = 0; i < arr.length-1; i++) {
      str += '0x' + arr[i].toString(16)
      if(i<arr.length-2) { str+=','; }
  }
  str += ']';
  return str;
};

var devices_found = HID.devices( VENDOR_ID, PRODUCT_ID );

if( devices_found.length === 0 ) {
    console.log("no blink(1) devices found");
    process.exit(0);
}
console.log("blink(1) devices found:", devices_found,'\n');

var hidDevice;
try {
    if( serial_number ) {
        console.log("opening serial number "+serial_number);
        hidDevice = new HID.HID( VENDOR_ID, PRODUCT_ID, serial_number );
    } else {
        console.log("opening first device");
        hidDevice = new HID.HID( VENDOR_ID, PRODUCT_ID );
    }
} catch(err) {
    console.log(err);
    process.exit(1);
}

var deviceInfo = hidDevice.getDeviceInfo();
console.log("deviceInfo.manufacturer:", deviceInfo.manufacturer);
console.log("deviceInfo.product:", deviceInfo.product);
console.log("deviceInfo.serialNumber:", deviceInfo.serialNumber);
console.log("\n");

// shamelessly stolen from node-blink1
var blink1_sendCommand = function( /* command [, args ...]*/ ) {
    var featureReport = new Array(REPORT_LENGTH + 1).fill(0);  // + 1 for reportId
    featureReport[0] = REPORT_ID;
    featureReport[1] = arguments[0].charCodeAt(0); // cmd
    for (var i = 1; i < arguments.length; i++) {
        featureReport[i + 1] = arguments[i];
    }
    console.log("report sent:", JSONstringifyHex(featureReport));
    var rc = hidDevice.sendFeatureReport(featureReport);
    console.log("sent size:",featureReport.length," actual size:", rc);
};

var blink1_sendCommand2 = function(/*command, args...*/) {
  var featureReport = new Array(REPORT2_LENGTH + 1).fill(0);
  featureReport[0] = REPORT2_ID;
  featureReport[1] = arguments[0].charCodeAt(0);
  for (var i = 1; i < arguments.length; i++) {
      featureReport[i + 1] = arguments[i];
  }
  console.log("report2 sent:"+ JSONstringifyHex(featureReport));
  var rc = hidDevice.sendFeatureReport(featureReport);
  console.log("sent size:",featureReport.length," actual size:", rc);
};

var blink1_readResponse = function(callback) {
    callback.apply(null, [hidDevice.getFeatureReport(REPORT_ID, REPORT_LENGTH+1)]);
};

var blink1_readResponse2 = function(callback) {
    callback.apply(null, [hidDevice.getFeatureReport(REPORT2_ID, REPORT2_LENGTH+1)]);
};

var blink1_getVersion = function(callback) {
    blink1_sendCommand('v');
    blink1_readResponse(function(response) {
        console.log('getVersion response: ',JSONstringifyHex(response))
        // var version = String.fromCharCode(response[3])  + '.' + String.fromCharCode(response[4]);
        var version = Number.parseInt(String.fromCharCode(response[3]))*100 + Number.parseInt(String.fromCharCode(response[4]))
        callback(version);
  });
};

// var blink1_getStartupParams = function(callback) {
//   blink1_sendCommand('b');
//   blink1_readResponse(function(response) {})
// });

var blink1_getChipId = function(callback) {
    blink1_sendCommand2('U');
    blink1_readResponse2(function(response) {
        console.log('getChipId response: ',JSONstringifyHex(response))
        // var version = String.fromCharCode(response[3])  + '.' + String.fromCharCode(response[4]);
        // NOTE: as of node-hid@1.0, response contains reportId as first element of array
        // var version = Number.parseInt(String.fromCharCode(response[4]))*100 + Number.parseInt(String.fromCharCode(response[5]))
        var chipId = response;
        callback(chipId);
  });
};

var blink1_fadeToColor = function( fadeMillis, r,g,b, ledn ) {
    var dms = fadeMillis / 10;
    blink1_sendCommand( 'c', r,g,b, dms>>8, dms%0xff, ledn );
};

blink1_getVersion( function(version) {
    blink1Version = version;
    console.log("\nblink(1) version: ", version);
});

setTimeout( function() {
    console.log("\nSetting blink(1) to #00FF33 over 300 millisecs");
    blink1_fadeToColor( 300, 0x00,0xFF,0x33, 0);
}, 100);

setTimeout( function() {
    console.log("\nSetting blink(1) to #FF6633 over 700 millisecs");
    blink1_fadeToColor( 700, 0xff,0x66,0x00, 0);
}, 1000);

setTimeout( function() {
  if( blink1Version > 300 ) {
    console.log("\nGetting chipId (mk3 only)");
    blink1_getChipId(function(response) {
    // do something here
    });
  }
}, 2000);

setTimeout( function() {
    console.log("\nSetting blink(1) to #000000 over 1000 millisecs, and closing");
    blink1_fadeToColor( 1000, 0x00,0x00,0x00, 0);
    hidDevice.close();
}, 3000 );
