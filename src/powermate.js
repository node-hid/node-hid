// Interface to Griffin PowerMate

// Received data from the PowerMate contains the state of the button
// in the first byte and the turning direction, if any, in the second
// byte.  The second byte is encoded as a signed integer.  Data sent
// to the PowerMate contains zero in the first byte and the brightness
// of the LED in the second byte.

var HID = require('../');
var util = require('util');
var events = require('events');

var allDevices;
function getAllDevices()
{
    if (!allDevices) {
	allDevices = HID.devices(1917, 1040);
    }
    return allDevices;
}

function PowerMate(index)
{
    if (!arguments.length) {
        index = 0;
    }

    var powerMates = getAllDevices();
    if (!powerMates.length) {
        throw new Error("No PowerMates could be found");
    }
    if (index > powerMates.length || index < 0) {
        throw new Error("Index " + index + " out of range, only " + powerMates.length + " PowerMates found");
    }
    this.hid = new HID.HID(powerMates[index].path);
    this.position = 0;
    this.button = 0;
    this.hid.read(this.interpretData.bind(this));
}

util.inherits(PowerMate, events.EventEmitter);

PowerMate.prototype.setLed = function(brightness) {
    this.hid.write([0, brightness]);
}

PowerMate.prototype.interpretData = function(error, data) {
    var button = data[0];
    if (button ^ this.button) {
        this.emit(button ? 'buttonDown' : 'buttonUp');
        this.button = button;
    }
    var delta = data[1];
    if (delta) {
        if (delta & 0x80) {
            delta = -256 + delta;
        }
        this.position += delta;
        this.emit('turn', delta, this.position);
    }
    this.hid.read(this.interpretData.bind(this));
}

exports.PowerMate = PowerMate;
exports.deviceCount = function () { return getAllDevices().length; }
