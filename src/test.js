var HID = require('HID');

// buzzer protocol info: http://www.developerfusion.com/article/84338/making-usb-c-friendly/

var buzzers = new HID.HID(0x54c,0x1000);
buzzers.oldBits = 0;
buzzers.leds = [0, 0, 0, 0, 0, 0];

function showBuzzer(buzzerNumber, bits)
{
    for (var buttonNumber = 0; buttonNumber < 5; buttonNumber++) {
        var now = bits & (1 << buttonNumber);
        var old = buzzers.oldBits & (1 << buttonNumber);
        if (old ^ now) {
            buzzers.emit('button', buzzerNumber, buttonNumber, (old ^ now) ? true : false);
        }
    }
}

function blinkLed(buzzerNumber)
{
    buzzers.led(buzzerNumber, true);
    setTimeout(function () {
        buzzers.led(buzzerNumber, false);
    }, 500);
}

function buzzerData (error, data) {
    var bits = (data[4] << 16) | (data[3] << 8) | data[2];
    for (var i = 0; i < 4; i++) {
        showBuzzer(i, bits & 0x1f);
        if (bits & 0x1f) {
        }
        bits >>= 5;
    }
    buzzers.read(arguments.callee);
}
buzzers.led = function(buzzer, state) {
    buzzers.leds[buzzer + 2] = state ? 0xff : 0x00;
    buzzers.write(buzzers.leds);
}
// Initialize buzzers
buzzers.write([0x00, 0x00, 0x00, 0x00, 0x00, 0x00]);
buzzers.read(buzzerData);

buzzers.on('button', function (buzzer, button, state) {
    console.log('buzzer', buzzer, 'button', button, 'state', state);
    if (state) {
        blinkLed(buzzer);
    }
});
