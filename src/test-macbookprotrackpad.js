//
// test-macbooktrackpad.js -- demonstrate receiving trackpad data
//                     also demonstrates filtering on usagePage & usage
//
// You may need to run this as 'root' user with 'sudo'.
// 
// For more details, see: https://www.pjrc.com/teensy/rawhid.html
// and https://github.com/node-hid/node-hid/issues/165
//
//
// Tod E. Kurt / github.com/todbot
//

var HID = require('../');

var devices = HID.devices();

// The trackpad on Macbooks are composite devices: One device with three interfaces.
// We must filter devices by vendorId, productId, usagePage, and usage.
var deviceInfo = devices.find( function(d) {
    // return d.vendorId===0x5AC && d.productId===0x0262 && d.usagePage===65280 && d.usage===1; // nothing?
    // return d.vendorId===0x5AC && d.productId===0x0262 && d.usagePage===0x0001 && d.usage===0x0006; // keyboard, as root, danger
    return d.vendorId===0x5AC && d.productId===0x0262 && d.usagePage===0x0001 && d.usage===0x0002; // trackpad, maybe as root
});
console.log("deviceInfo: ", deviceInfo);
if( !deviceInfo ) {
    console.log(devices);
    console.log("Could not find device in device list");
    process.exit(1);
}

console.log("got device. listening...");
var device = new HID.HID( deviceInfo.path );

device.on('data', function(data) {
    console.log("got data:",data);
});
device.on('error', function(err) {
    console.log("error:",err);
});
