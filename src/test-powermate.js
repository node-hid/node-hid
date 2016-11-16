
var PowerMate = require('./powermate');

var powerMate;
for (var i = 0; i < PowerMate.deviceCount(); i++) {

    console.log('opening powermate', i);

    powerMate = new PowerMate.PowerMate(i);

    powerMate.on('buttonDown', function () {
        console.log('button down');
        this.position = 0;
    });

    powerMate.on('buttonUp', function () {
        console.log('button up');
    });

    powerMate.on('turn', function (delta, position) {
        console.log('delta', delta, 'position', position);
        this.setLed(position % 256);
    });
}
