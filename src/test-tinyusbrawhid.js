//
// test-tinuusbrawhid.js -- demonstrate sending data to TinyUSB RawHID device
//
// For more details, see: https://www.pjrc.com/TinyUSB/rawhid.html
// and https://github.com/node-hid/node-hid/issues/165
//
//
// Tod E. Kurt / github.com/todbot
//

var os = require('os');

var HID = require('../');

var devices = HID.devices();

console.log("HID devices:",devices);

var isTinyUSB = function(d) { return d.vendorId===0x239A && d.productId===0x801E;}

var device;  // to be filled out below
//var deviceInfo
//var deviceList = devices.filter(function(d) { return isTinyUSB(d) });

var deviceInfo = devices.find( function(d) {return isTinyUSB(d) });

if( deviceInfo ) {
    device = new HID.HID( deviceInfo.path );
}

console.log("deviceInfo: ", deviceInfo);
if( !device ) {
    console.log(devices);
    console.log("Could not find RawHID device in device list");
    process.exit(1);
}

console.log("Attaching receive 'data' handler");
device.on('data', function(data) {
    console.log("Got data:", data.toString('hex') );
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

console.log("Sending messages to TinyUSB, watch TinyUSB Serial Montor for data");

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

console.log("Waiting 10 seconds for data from TinyUSB");
setTimeout( function() {
    console.log("Done");
    device.close();
}, 10000);
