/**
 * Two fake devices open at once: state and IO must be fully isolated, and
 * closing one must not disturb the other. Fake-only.
 */
const { describe, it, before, after } = require("node:test");
const assert = require("node:assert");

const HID = require("../..");
const env = require("../helpers/env");
const blink1 = require("../helpers/blink1-protocol");
const { FakeBlink1 } = require("../helpers/fake-blink1");

describe("multiple devices", { skip: env.fakeOnlySkipReason }, () => {
    let fakeA, fakeB;

    before(async () => {
        fakeA = await FakeBlink1.create();
        fakeB = await FakeBlink1.create();
    });
    after(async () => {
        await FakeBlink1.cleanupAll();
    });

    it("both enumerate side by side", () => {
        const serials = HID.devices(blink1.VENDOR_ID, blink1.PRODUCT_ID).map((d) => d.serialNumber);
        assert.ok(serials.includes(fakeA.serial));
        assert.ok(serials.includes(fakeB.serial));
    });

    it("feature report state is independent", async () => {
        const devA = await HID.HIDAsync.open(fakeA.hidrawPath);
        const devB = await HID.HIDAsync.open(fakeB.hidrawPath);

        await devA.sendFeatureReport(blink1.command("n", 10, 20, 30));
        await devB.sendFeatureReport(blink1.command("n", 40, 50, 60));

        assert.deepStrictEqual((await fakeA.getState()).rgb, { r: 10, g: 20, b: 30 });
        assert.deepStrictEqual((await fakeB.getState()).rgb, { r: 40, g: 50, b: 60 });

        await devA.close();
        await devB.close();
    });

    it("input reports are delivered only to their own device", async () => {
        const devA = await HID.HIDAsync.open(fakeA.hidrawPath);
        const devB = await HID.HIDAsync.open(fakeB.hidrawPath);

        const gotA = [];
        const gotB = [];
        devA.on("data", (d) => gotA.push([...d]));
        devB.on("data", (d) => gotB.push([...d]));
        await new Promise((r) => setTimeout(r, 50));

        await fakeA.sendInput([1, 1, 1, 1, 1, 1, 1, 1]);
        await waitFor(() => gotA.length >= 1);
        // Give a misdelivery a chance to show up before asserting
        await new Promise((r) => setTimeout(r, 100));

        assert.strictEqual(gotA.length, 1);
        assert.strictEqual(gotB.length, 0);

        await devA.close();
        await devB.close();
    });

    it("closing one device leaves the other fully usable", async () => {
        const devA = await HID.HIDAsync.open(fakeA.hidrawPath);
        const devB = await HID.HIDAsync.open(fakeB.hidrawPath);

        await devA.close();

        const info = await devB.getDeviceInfo();
        assert.strictEqual(info.serialNumber, fakeB.serial);
        await devB.sendFeatureReport(blink1.command("v"));
        const response = await devB.getFeatureReport(blink1.REPORT_ID, 9);
        assert.strictEqual(response.length, 9);

        await devB.close();
    });
});

async function waitFor(predicate, timeoutMs = 2000) {
    const deadline = Date.now() + timeoutMs;
    while (!predicate()) {
        if (Date.now() > deadline) throw new Error("timed out waiting for condition");
        await new Promise((resolve) => setTimeout(resolve, 10));
    }
}
