/**
 * Gives the core suite a blink(1) to test against - fake or real - without the
 * suite caring which. `device.fake` is null in real mode; fake-mode-only
 * assertions must be guarded on it.
 */
const env = require("./env");
const { FakeBlink1 } = require("./fake-blink1");
const { RealBlink1 } = require("./real-blink1");

async function acquire(options = {}) {
    if (env.mode === "real") {
        const real = await RealBlink1.create();
        return { path: real.hidrawPath, serial: real.serial, fake: null, release: () => real.close() };
    }

    const fake = await FakeBlink1.create(options);
    return { path: fake.hidrawPath, serial: fake.serial, fake, release: () => fake.close() };
}

module.exports = { acquire };
