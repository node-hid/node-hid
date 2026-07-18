/**
 * Adapter exposing a physically attached blink(1) with the same core surface as
 * FakeBlink1 (no sendInput/getState/onOutput/destroy - those are fake superpowers).
 */
const HID = require("../..");
const blink1 = require("./blink1-protocol");

class RealBlink1 {
    static async create() {
        const info = HID.devices(blink1.VENDOR_ID, blink1.PRODUCT_ID)[0];
        if (!info) {
            throw new Error("NODE_HID_TEST_REAL_BLINK1=1 but no blink(1) found - is it plugged in?");
        }
        const real = new RealBlink1();
        real.hidrawPath = info.path;
        real.serial = info.serialNumber;
        return real;
    }

    async close() {}
}

module.exports = { RealBlink1 };
