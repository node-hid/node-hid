/*
 This example connect to hid device type msp430 active bsl mode. mass erase and unlock device.
 */

"use strict";

var HID = require("node-hid");
var sleep = require("sleep");

var Commands = {
    BSL_RX_DATA_BLOCK: 0x10,
    BSL_RX_DATA_BLOCK_FAST: 0x1b,
    BSL_RX_PASSWORD: 0x11,
    BSL_ERASE_SEGMENT: 0x12,
    BSL_LOCK_INFO: 0x13,
    BSL_MASS_ERASE: 0x15,
    BSL_CRC_CHECK: 0x16,
    BSL_LOAD_PC: 0x17,
    BSL_TX_DATA_BLOCK: 0x18,
    BSL_VERSION: 0x19,
    BSL_BUFFER_SIZE: 0x1a
};

function GetBSL5ErrorCodeInfo(error_code) {
    switch (error_code) {
        case 0x00:
            return "Operation successful";
        case 0x01:
            return 'Flash write check failed';
        case 0x02:
            return 'Flash fail bit set';
        case 0x03:
            return 'Voltage change during program';
        case 0x04:
            return 'BSL locked';
        case 0x05:
            return 'BSL password error';
        case 0x06:
            return 'Byte write forbidden';
        case 0x07:
            return 'Unknown command';
        case 0x08:
            return 'Packet length exceeds buffer size';
    }

    return "Unknown error code information";
}

function CheckAnswer(data) {
    if (data[0] == 0x3b) {
        if (data[1] == 0x00) {
            return;
        } else {
            throw new Error("CheckAnswer error case 1, error info: " + GetBSL5ErrorCodeInfo(data[1]));
        }
    } else if (data[0] != 0x3a) {
        throw new Error("CheckAnswer error case 2");
    }
}

function Bsl(device, cmd, message, receive_response) {
    var txdata = [0x3f, 1 + message.length, cmd].concat(message);
    var padding_size = 64 - txdata.length;

    for (var i = 0; i < padding_size; i++) {
        txdata.push(0xac);
    }

    device.write(txdata);

    if (receive_response) {
        var report = device.readSync();
        var pi = report[0];

        if (pi = 0x3f) {
            var length = report[1];
            var data = report.slice(2, 2 + length);

            return data;
        } else {
            throw new Error("received bad PI, expected 0x3f");
        }
    }
}

function BslRxPassword(device, password) {
    var answer = Bsl(device, Commands.BSL_RX_PASSWORD, password, true);
    CheckAnswer(answer);
}

// open a device type mps430 in bsl mode
var device = new HID.HID(0x2047, 0x0200);

// only for test readTimeout function
var data_read = device.readTimeout(1000);
console.log("data_read: " + JSON.stringify(data_read));


// init for mass erase password
var package_erase_password = [];

for (var i = 0; i < 30; i++) {
    package_erase_password.push(0xff);
}

package_erase_password.push(0x00);
package_erase_password.push(0x00);

// Mass erase initiated due to incorrect password
try {
    BslRxPassword(device, package_erase_password);
} catch (err) {
    console.log("Trace exception BslRxPassword, " + err);
}

sleep.sleep(1);

var package_erase_password_2 = [];

for (var i = 0; i < 32; i++) {
    package_erase_password_2.push(0xff);
}

// after erase, unlock device
BslRxPassword(device, package_erase_password_2);
