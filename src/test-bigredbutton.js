/**
 * Interface with the Dream Cheeky Big Red button
 * http://dreamcheeky.com/big-red-button
 *
 * Uses info gleened from:
 * http://ddurdle.blogspot.com/2013/12/using-usb-big-red-button-panic-button.html
 *
 * @author Tod Kurt (https://github.com/todbot)
 */

/*jslint node: true */
"use strict";

var HID = require('..');

var device = new HID.HID(7476, 13);

var buttonGetDataCmd = [ 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02 ];

device.on('data', function(data) {
    var msg = '';
    var buttonState = data[0];
    if( buttonState === 0x17 ) {
        msg = "Lid opened!";
    } else if( buttonState === 0x16 ) {
        msg = "Button pushed!";
    } else if( buttonState === 0x15 ) {
        msg = "waiting...";
    }
    console.log('button data', data, msg);
});

setInterval( function() {
    device.write( buttonGetDataCmd );
}, 100);
