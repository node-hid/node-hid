// Simple interface to Sony Buzz! wireless buzzers

var util = require('util');
var events = require('events');
var HID = require('../');

// buzzer protocol info: http://www.developerfusion.com/article/84338/making-usb-c-friendly/

function BuzzerController(index)
{
    if (!arguments.length) {
        index = 0;
    }

    var controllers = HID.devices(0x54c, 0x1000);

    if (!controllers.length) {
        throw new Error("No Buzzer controllers could be found");
    }

    if (index > controllers.length || index < 0) {
        throw new Error("Index " + index + " out of range, only " + controllers.length + " Buzzer controllers found");
    }

    events.EventEmitter.call(this);

    this.hid = new HID.HID(controllers[index].path);
    this.oldBits = 0;
    this.leds = [0, 0, 0, 0, 0, 0];

    // Initialize buzzers
    this.hid.write([0x00, 0x00, 0x00, 0x00, 0x00, 0x00]);
    this.hid.read(this.buzzerData.bind(this));
}

util.inherits(BuzzerController, events.EventEmitter);

BuzzerController.prototype.handleBuzzer = function (buzzerNumber, bits)
{
    var mask = 1 << (buzzerNumber * 5);
    for (var buttonNumber = 0; buttonNumber < 5; buttonNumber++) {
        var now = bits & mask;
        var old = this.oldBits & mask;
        if (old ^ now) {
            this.emit('button', buzzerNumber, buttonNumber, now ? true : false);
        }
        mask <<= 1;
    }
}

BuzzerController.prototype.buzzerData = function (error, data) {
    console.log('error', error, 'data', data);
    var bits = (data[4] << 16) | (data[3] << 8) | data[2];
    for (var i = 0; i < 4; i++) {
        this.handleBuzzer(i, bits);
    }
    this.oldBits = bits;
    this.hid.read(this.buzzerData.bind(this));
}

BuzzerController.prototype.led = function(buzzer, state) {
    this.leds[buzzer + 2] = state ? 0xff : 0x00;
    this.hid.write(this.leds);
}

exports.BuzzerController = BuzzerController;
