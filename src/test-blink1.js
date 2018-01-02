/**
 * Simple demostration of sendFeatureReport
 * for a blink(1) USB LED by ThingM http://blink1.thingm.com/
 */

var HID = require('../');

var VENDOR_ID = 0x27B8;
var PRODUCT_ID = 0x01ED;
var REPORT_ID = 1;
var REPORT_LENGTH = 9;

var devices_found = HID.devices( VENDOR_ID, PRODUCT_ID );

if( devices_found.length === 0 ) {
    console.log("no blink(1) devices found");
    process.exit(0);
}
console.log("blink(1) devices found:", devices_found,'\n');

var hidDevice = new HID.HID( VENDOR_ID, PRODUCT_ID );

var deviceInfo = hidDevice.getDeviceInfo();
console.log("deviceInfo.manufacturer:", deviceInfo.manufacturer);
console.log("deviceInfo.product:", deviceInfo.product);
console.log("deviceInfo.serialNumber:", deviceInfo.serialNumber);

// shamelessly stolen from node-blink1
var blink1_sendCommand = function( /* command [, args ...]*/ ) {
    var featureReport = [REPORT_ID, 0, 0, 0, 0, 0, 0, 0, 0];

    featureReport[1] = arguments[0].charCodeAt(0);

    for (var i = 1; i < arguments.length; i++) {
        featureReport[i + 1] = arguments[i];
    }
    var rc = hidDevice.sendFeatureReport(featureReport);
    console.log("report sent:", featureReport);
    console.log("sent size:",featureReport.length," actual size:", rc);
};
var blink1_readResponse = function(callback) {
    callback.apply(null, [hidDevice.getFeatureReport(REPORT_ID, REPORT_LENGTH)]);
};

var blink1_getVersion = function(callback) {
    blink1_sendCommand('v');
    blink1_readResponse(function(response) {
        var version = String.fromCharCode(response[3]) + '.' + String.fromCharCode(response[4]);
        callback(version);
  });
};

var blink1_fadeToColor = function( fadeMillis, r,g,b, ledn ) {
    var dms = fadeMillis / 10;
    blink1_sendCommand( 'c', r,g,b, dms>>8, dms%0xff, ledn );
};

blink1_getVersion( function(version) {
    console.log("blink(1) version: ", version);
});

setTimeout( function() {
    console.log("Setting blink(1) to #00FF33 over 300 millisecs");
    blink1_fadeToColor( 300, 0x00,0xFF,0x33, 0);
}, 100);

setTimeout( function() {
    console.log("Setting blink(1) to #FF6633 over 700 millisecs");
    blink1_fadeToColor( 700, 0xff,0x66,0x00, 0);
}, 1000);

setTimeout( function() {
    console.log("Setting blink(1) to #000000 over 1000 millisecs, and closing");
    blink1_fadeToColor( 1000, 0x00,0x00,0x00, 0);
    hidDevice.close();
}, 2000 );
