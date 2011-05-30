// Simple interface to Sony Buzz! wireless buzzers

var HID = require('HID');

// buzzer protocol info: http://www.developerfusion.com/article/84338/making-usb-c-friendly/

var buzzers = new HID.HID(0x54c, 0x1000);

buzzers.oldBits = 0;
buzzers.leds = [0, 0, 0, 0, 0, 0];

function handleBuzzer(buzzerNumber, bits)
{
    var mask = 1 << (buzzerNumber * 5);
    for (var buttonNumber = 0; buttonNumber < 5; buttonNumber++) {
        var now = bits & mask;
        var old = buzzers.oldBits & mask;
        if (old ^ now) {
            buzzers.emit('button', buzzerNumber, buttonNumber, now ? true : false);
        }
        mask <<= 1;
    }
}

function buzzerData (error, data) {
    var bits = (data[4] << 16) | (data[3] << 8) | data[2];
    for (var i = 0; i < 4; i++) {
        handleBuzzer(i, bits);
    }
    buzzers.oldBits = bits;
    buzzers.read(arguments.callee);
}

buzzers.led = function(buzzer, state) {
    buzzers.leds[buzzer + 2] = state ? 0xff : 0x00;
    buzzers.write(buzzers.leds);
}

// Initialize buzzers
buzzers.write([0x00, 0x00, 0x00, 0x00, 0x00, 0x00]);
buzzers.read(buzzerData);

exports.buzzers = buzzers;
