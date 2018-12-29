#!/usr/bin/env node

var HID = require('../');

// choose driverType
// default is 'libusb' for Mac OSX & Windows
// default is 'hidraw', for Linux
var type = null;

if( process.argv[2] ) {
    type = process.argv[2];
}
// disabled until prebuild gets multi-target, see issue node-hid#242
// console.log('driverType:', (type) ? type : 'default');
// HID.setDriverType( type );

console.log('devices:', HID.devices());
