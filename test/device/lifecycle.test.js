/**
 * Lifecycle, teardown-ordering and simulated-unplug tests. Fake-only: they rely
 * on destroying the virtual device on demand.
 */
const { describe, it, afterEach, after } = require("node:test");
const assert = require("node:assert");

const HID = require("../..");
const env = require("../helpers/env");
const blink1 = require("../helpers/blink1-protocol");
const { FakeBlink1 } = require("../helpers/fake-blink1");

describe("device lifecycle", { skip: env.fakeOnlySkipReason }, () => {
    let fake;

    async function setup() {
        fake = await FakeBlink1.create();
        return fake;
    }
    afterEach(async () => {
        if (fake) {
            await fake.close();
            fake = null;
        }
    });
    after(async () => {
        await FakeBlink1.cleanupAll();
    });

    describe("async HIDAsync", () => {
        it("readStart after close throws cleanly", async () => {
            await setup();
            const dev = await HID.HIDAsync.open(fake.hidrawPath);
            await dev.close();

            assert.throws(() => dev._raw.readStart(() => {}), /device has been closed/);
        });

        it("readStart without a callback function throws cleanly", async () => {
            await setup();
            const dev = await HID.HIDAsync.open(fake.hidrawPath);

            assert.throws(() => dev._raw.readStart(), /callback function/);
            assert.throws(() => dev._raw.readStart("not a function"), /callback function/);

            await dev.close();
        });

        it("readStop after close is a harmless no-op", async () => {
            await setup();
            const dev = await HID.HIDAsync.open(fake.hidrawPath);
            await dev.close();

            dev._raw.readStop();
        });

        it("unplug during readStart surfaces an error event, close still resolves", async () => {
            await setup();
            const dev = await HID.HIDAsync.open(fake.hidrawPath);

            const errorSeen = new Promise((resolve) => dev.on("error", resolve));
            dev.on("data", () => {});
            await new Promise((r) => setTimeout(r, 50)); // let resume()/readStart begin

            await fake.destroy(); // simulated unplug

            const err = await errorSeen;
            assert.ok(err, "expected an error event after unplug");
            await dev.close();
        });

        it("feature report against an unplugged device rejects", async () => {
            await setup();
            const dev = await HID.HIDAsync.open(fake.hidrawPath);
            await fake.destroy();

            await assert.rejects(dev.getFeatureReport(blink1.REPORT_ID, 9), /could not get feature report/);
            await dev.close();
        });
    });

    describe("sync HID", () => {
        it("close during a pending read is refused, works after readInterrupt", async () => {
            await setup();
            const dev = new HID.HID(fake.hidrawPath);

            let readSettled = false;
            dev.read(() => {
                readSettled = true;
            });

            assert.throws(() => dev._raw.close(), /read is still running/);

            dev._raw.readInterrupt();
            await waitFor(() => readSettled);
            dev.close();
        });

        it("a second read while one is pending throws", async () => {
            await setup();
            const dev = new HID.HID(fake.hidrawPath);

            let readSettled = false;
            dev.read(() => {
                readSettled = true;
            });
            assert.throws(() => dev.read(() => {}), /read is already running/);

            dev._raw.readInterrupt();
            await waitFor(() => readSettled);
            dev.close();
        });

        it("unplug during a pending read surfaces a read error", async () => {
            await setup();
            const dev = new HID.HID(fake.hidrawPath);

            const readResult = new Promise((resolve) => {
                dev.read((err, data) => resolve({ err, data }));
            });
            await new Promise((r) => setTimeout(r, 50));

            await fake.destroy();

            const { err } = await readResult;
            assert.ok(err, "expected the pending read to fail after unplug");
            dev.close();
        });

        it("double close is safe", async () => {
            await setup();
            const dev = new HID.HID(fake.hidrawPath);
            dev.close();
            dev.close();
        });
    });

    describe("device removal", () => {
        it("a destroyed device disappears from enumeration", async () => {
            await setup();

            // present first
            const before = HID.devices(blink1.VENDOR_ID, blink1.PRODUCT_ID).find((d) => d.serialNumber === fake.serial);
            assert.ok(before);

            await fake.destroy();
            await fake.waitForRemoval();
        });

        it("opening a stale path after removal fails cleanly", async () => {
            await setup();
            const stalePath = fake.hidrawPath;
            await fake.destroy();
            await fake.waitForRemoval();

            assert.throws(() => new HID.HID(stalePath), /cannot open device with path/);
            await assert.rejects(HID.HIDAsync.open(stalePath), /cannot open device with path/);
        });
    });
});

async function waitFor(predicate, timeoutMs = 2000) {
    const deadline = Date.now() + timeoutMs;
    while (!predicate()) {
        if (Date.now() > deadline) throw new Error("timed out waiting for condition");
        await new Promise((resolve) => setTimeout(resolve, 10));
    }
}
