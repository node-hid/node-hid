
powerMate = require('./powermate').powerMate;

powerMate.on('buttonDown', function () {
    console.log('button down');
});

powerMate.on('turn', function (delta, position) {
    console.log('delta', delta, 'position', position);
});