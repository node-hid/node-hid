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

console.log("HID devices:",devices);

// We must filter devices by vendorId, productId, usagePage, and usage
// because Teensy RawHID sketch shows up as TWO devices to node-hid
// Note this only works on Mac & Windows though as the underlying
// hidapi C library doesn't support usagePage on libusb or hidraw

var isTeensy = function(d) { return d.vendorId===0x16C0 && d.productId===0x0486;}
var isRawHidUsage = function(d) { 
    return ((d.usagePage===0xFFAB && d.usage===0x2000) || (d.usagePage===0xFFAB && d.usage===0x0200));
}

var device;  // to be filled out below
var deviceInfo
if( os.platform() == 'linux' ) {
    var deviceList = devices.filter(function(d) { return isTeensy(d) });
    if( deviceList.length == 2 ) { 
        deviceInfo = deviceList[0]
	    device = new HID.HID( deviceInfo.path ); // normally first device
    }
}
else { // Mac & Windows
    var deviceInfo = devices.find( function(d) {
	    return isTeensy(d) && isRawHidUsage(d);
    });
    if( deviceInfo ) { 
        device = new HID.HID( deviceInfo.path );
    }
}

console.log("deviceInfo: ", deviceInfo);
if( !device ) {
    console.log(devices);
    console.log("Could not find RawHID device in device list");
    process.exit(1);
}

console.log("Attaching receive 'data' handler");
device.on('data', function(data) {
    console.log("got data:", data.toString('hex') );
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

console.log("Sending messages to Teensy, watch Teensy Serial Montor for data");

console.log('Sending message A: ', JSON.stringify(messageA))
var numsentA = device.write(messageA);
console.log('messageA len:', messageA.length, 'actual sent len:', numsentA);

var messageB = [];
for(var i=0; i < 64; i++) {
    messageB[i] = 1 + i;
}
// for Windows, must prepend report number, even when there isn't one
if( os.platform() == 'win32' ) {
    messageB.unshift( 0x00 );
}
console.log('Sending message B: ', JSON.stringify(messageB))
var numsentB = device.write(messageB);
console.log('messageB len:', messageB.length, 'actual sent len:', numsentB);

console.log("Waiting 10 seconds for data from Teensy");
setTimeout( function() {
    console.log("Done");
    device.close();
}, 10000);

