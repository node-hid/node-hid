var HID = require('../');
var REPL = require('repl');

var repl = REPL.start('node-hid> ');
var hid = new HID.HID(1356, 616);

console.log('features', hid.getFeatureReport(0xf2, 17));

hid.on('data', (data) => {
    console.log('got ps3 data', data);
})

repl.context.hid = hid;
