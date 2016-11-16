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

var HID = require('../');

var devices = HID.devices();

// We must filter devices by vendorId, productId, usagePage, and usage
// because Teensy RawHID sketch shows up as TWO devices to node-hid / hidapi
var deviceInfo = devices.find( function(d) {
    return d.vendorId===0x16C0 && d.productId===0x0486 && d.usagePage===0xFFAB && d.usage===0x200;
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

var message = [];
for(var i=0; i < 64; i++) {
    message[i] = 120;
}
message[0] = 121;
message[1] = 122;

console.log('message : ', JSON.stringify(message));
device.write(message);

device.close();
