//
// test-rawhid64.js -- demonstrate sending data to Teensy RawHID device
//                     also demonstrates filtering on usagePage & usage
//
// For more details, see: https://www.pjrc.com/teensy/rawhid.html
// and https://github.com/node-hid/node-hid/issues/165
//
//
// Tod E. Kurt / github.com/todbot
//

var os = require('os');

var HID = require('../');

var devices = HID.devices();

// We must filter devices by vendorId, productId, usagePage, and usage
// because Teensy RawHID sketch shows up as TWO devices to node-hid / hidapi
var deviceInfo = devices.find( function(d) {
    var isTeensy = d.vendorId===0x16C0 && d.productId===0x0486;
    return isTeensy && d.usagePage===0xFFAB && d.usage===0x200;
});
console.log("deviceInfo: ", deviceInfo);
if( !deviceInfo ) {
    console.log(devices);
    console.log("Could not find RawHID device in device list");
    process.exit(1);
}

var device = new HID.HID( deviceInfo.path );

device.on('data', function(data) {
    console.log("got data:",data);
});
device.on('error', function(err) {
    console.log("error:",err);
});

var messageA = [];
for(var i=0; i < 64; i++) {
    messageA[i] = 120 + i;
}
// for Windows, must prepend report number, even when there isn't one
if( os.platform() == 'win32' ) {
    messageA.unshift( 0x00 );
}

var numsentA = device.write(messageA);
console.log('message A: ', JSON.stringify(messageA))
console.log('sent len:', messageA.length, 'actual len:', numsentA);

var messageB = [];
for(var i=0; i < 64; i++) {
    messageB[i] = 0 + i;
}
// for Windows, must prepend report number, even when there isn't one
if( os.platform() == 'win32' ) {
    messageB.unshift( 0x00 );
}
var numsentB = device.write(messageB);
console.log('message B: ', JSON.stringify(messageB))
console.log('sent len:', messageB.length, 'actual len:', numsentB);

console.log("waiting 5 seconds for data from Teensy");
setTimeout( function() {
    device.close();
}, 5000);
