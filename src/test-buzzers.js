
buzzers = require('./buzzers').buzzers;

function blinkLed(buzzerNumber)
{
    buzzers.led(buzzerNumber, true);
    setTimeout(function () {
        buzzers.led(buzzerNumber, false);
    }, 500);
}

buzzers.on('button', function (buzzer, button, state) {
    console.log('buzzer', buzzer, 'button', button, 'state', state);
    if (state) {
        blinkLed(buzzer);
    }
});
