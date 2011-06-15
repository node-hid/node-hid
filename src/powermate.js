// Interface to Griffin PowerMate

// Received data from the PowerMate contains the state of the button
// in the first byte and the turning direction, if any, in the second
// byte.  The second byte is encoded as a signed integer.  Data sent
// to the PowerMate contains zero in the first byte and the brightness
// of the LED in the second byte.

var HID = require('HID');

var powerMate = new HID.HID(1917, 1040);

powerMate.setLed = function(brightness) {
    powerMate.write([0, brightness]);
}

powerMate.position = 0;
powerMate.button = 0;

function interpretData(error, data) {
    var button = data[0];
    if (button ^ powerMate.button) {
        powerMate.emit(button ? 'buttonDown' : 'buttonUp');
        powerMate.button = button;
    }
    var delta = data[1];
    if (delta) {
        if (delta & 0x80) {
            delta = -256 + delta;
        }
        powerMate.position += delta;
        powerMate.emit('turn', delta, powerMate.position);
    }
    powerMate.read(interpretData);
}

powerMate.read(interpretData);

exports.powerMate = powerMate;