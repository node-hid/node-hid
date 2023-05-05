//
// Press Playstation button to start DS3 controller transmitting
// After receiving data, press dpad left & right to trigger rumble
// Press action buttons (triagle, square, etc) to change LEDs
//

/*jslint node: true */
"use strict";

var HID = require('..');

var hid = new HID.HID(1356, 616);

try {
    // note: this fails for bluetooth connections, so we catch
    console.log('features', hid.getFeatureReport(0xf2, 17));
} catch(err) {
    console.log("couldn't send 0xf2 getFeature, on bluetooth?");
}

// from: https://github.com/ros-drivers/joystick_drivers/blob/indigo-devel/ps3joy/scripts/ps3joy_node.py
function setRumbleLed(hidDevice, rumbleL, rumbleR, led_cmd )
{
    // write output report with report id 0x01
    hidDevice.write([
        0x01,     // <- feature report id
        0x00, 0xfe, rumbleL, 0xfe, rumbleR,
        0x00, 0x00, 0x00, 0x00, led_cmd,
        0xff, 0x27, 0x10, 0x00, 0x32,
        0xff, 0x27, 0x10, 0x00, 0x32,
        0xff, 0x27, 0x10, 0x00, 0x32,
        0xff, 0x27, 0x10, 0x00, 0x32,
        0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    ]);
}

hid.gotData = function (err, data) {
    console.log('got ps3 data', data);
    // map left & right d-pad to rumble, and right action buttons to LEDs
    setRumbleLed( hid, data[15], data[17], data[3]>>3 );
    this.read(this.gotData.bind(this));
};

hid.read(hid.gotData.bind(hid));

/*
 * data is 48-byte Buffer with byte values:
 * index- info
 *  00  - unknown 0x01
 *  01  - unknown 0x00
 *  02  - start, select, dpad digital bitfield (see data[14]-[17] for analog values)
 *  03  - action button, shoulder, triggers digital bitfield (see data[18]-[25] for analog values)
 *  04  - playstation button
 *  05  -
 *  06  - left joystick analog left-right
 *  07  - left joystick analog up-down
 *  08  - right joystick analog left-right
 *  09  - right joystick analog up-down
 *  10,11,12,13 - unknown 0x00
 *  14  - dpad    up analog pressure
 *  15  - dpad right analog pressure
 *  16  - dpad  down analog pressure
 *  17  - dpad  left analog pressure
 *  18  - left  trigger analog pressure
 *  19  - right trigger analog pressure
 *  20  - left shoulder analog pressure
 *  21  - right shoulder analog pressure
 *  22  - triangle action analog pressure
 *  23  - cicle    action analog pressure
 *  24  - X        action analog pressure
 *  25  - square   action analog pressure
 *  26,27,28
 *  29  - charge state
 *  30  - connection type
 *  31,32,33,34,35,36,37,38,39
 *  40,41 - X-axis accelerometer
 *  42,43 - Y-axis accelerometer
 *  44,45 - Z-axis accelerometer
 *  46,47 - Z-axis gyro
 */
// from: https://github.com/ribbotson/USB-Host/blob/master/ps3/PS3USB/ps3_usb.h
// from: https://code.google.com/p/openaxis/
