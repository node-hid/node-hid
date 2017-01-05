//
// test-macbooktrackpad.js -- demonstrate receiving trackpad data
//                     also demonstrates filtering on usagePage & usage
//
//
// For more details, see: https://www.pjrc.com/teensy/rawhid.html
// and https://github.com/node-hid/node-hid/issues/165
//
//
// Tod E. Kurt / github.com/todbot
//

var HID = require('../');
require('bleno');

var devices = HID.devices();
console.log( devices );

// The trackpad on Macbooks are composite devices: One device with three interfaces.
// We must filter devices by vendorId, productId, usagePage, usage or other properties
var deviceInfo = devices.find( function(d) {
    // return d.vendorId===0x5AC && d.productId===0x0262 && d.usagePage===0x0001 && d.usage===0x0002; // specific Mac trackpad
    return d.usagePage === 1 && d.usage === 2 && d.path.includes('IOHIDPointingDevice'); // general Mac trackpad
    // return d.usagePage === 1 && d.usage === 2 && d.path.includes('IOHIDUserDevice'); // Bluetooth mouse on Mac
    // return d.usagePage === 1 && d.usage === 2; // for just a pointing device (but doesn't always work on Macs)
});

console.log("deviceInfo: ", deviceInfo);
if( !deviceInfo ) {
    console.log("Could not find device in device list");
    process.exit(1);
}

console.log("got device. listening to:", deviceInfo.path);
var device = new HID.HID( deviceInfo.path );

setTimeout( function() {
    device.close();
}, 10 * 1000); // after 10 seconds quit

device.on('data', function(data) {
    console.log("got data:",data);
});
device.on('error', function(err) {
    console.log("error:",err);
});
