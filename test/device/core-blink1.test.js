/**
 * Core behaviour tests, exercising only what a real blink(1) supports (feature
 * reports), so they run against the uhid fake OR real hardware
 * (NODE_HID_TEST_REAL_BLINK1=1). Fake-specific assertions are guarded on
 * `device.fake`.
 */
const { describe, it, before, after } = require("node:test");
const assert = require("node:assert");

const HID = require("../..");
const env = require("../helpers/env");
const blink1 = require("../helpers/blink1-protocol");
const { acquire } = require("../helpers/device-under-test");
const { FakeBlink1 } = require("../helpers/fake-blink1");

describe("blink(1) core behaviour", { skip: env.skipReason }, () => {
    let device; // { path, serial, fake|null, release }
    let firmwareVersion;

    before(async () => {
        device = await acquire({ version: 301 }); // 301 = mk3-like, enables report-2 tests on the fake
    });
    after(async () => {
        if (device) await device.release();
        await FakeBlink1.cleanupAll();
    });

    describe("enumeration", () => {
        it("is listed by vid/pid with a serial number", () => {
            const match = HID.devices(blink1.VENDOR_ID, blink1.PRODUCT_ID).find((d) => d.serialNumber === device.serial);
            assert.ok(match, "device not found in devices(vid, pid)");
            assert.strictEqual(match.vendorId, blink1.VENDOR_ID);
            assert.strictEqual(match.productId, blink1.PRODUCT_ID);
            assert.strictEqual(match.path, device.path);
        });

        it("is listed by devicesAsync too", async () => {
            const match = (await HID.devicesAsync(blink1.VENDOR_ID, blink1.PRODUCT_ID)).find(
                (d) => d.serialNumber === device.serial
            );
            assert.ok(match);
        });
    });

    describe("opening", () => {
        it("opens by path (sync)", () => {
            const dev = new HID.HID(device.path);
            dev.close();
        });

        it("opens by vid/pid (sync)", () => {
            const dev = new HID.HID(blink1.VENDOR_ID, blink1.PRODUCT_ID);
            dev.close();
        });

        it("opens by vid/pid/serial (sync)", () => {
            const dev = new HID.HID(blink1.VENDOR_ID, blink1.PRODUCT_ID, device.serial);
            dev.close();
        });

        it("open by vid/pid with a wrong serial fails", () => {
            assert.throws(() => new HID.HID(blink1.VENDOR_ID, blink1.PRODUCT_ID, "NO-SUCH-SERIAL"), /cannot open device/);
        });

        it("opens by path (async)", async () => {
            const dev = await HID.HIDAsync.open(device.path);
            await dev.close();
        });

        it("opens by vid/pid/serial (async)", async () => {
            const dev = await HID.HIDAsync.open(blink1.VENDOR_ID, blink1.PRODUCT_ID, device.serial);
            await dev.close();
        });

        it("getDeviceInfo matches enumeration (sync + async)", async () => {
            const sdev = new HID.HID(device.path);
            const sinfo = sdev.getDeviceInfo();
            sdev.close();

            const adev = await HID.HIDAsync.open(device.path);
            const ainfo = await adev.getDeviceInfo();
            await adev.close();

            for (const info of [sinfo, ainfo]) {
                assert.strictEqual(info.vendorId, blink1.VENDOR_ID);
                assert.strictEqual(info.productId, blink1.PRODUCT_ID);
                assert.strictEqual(info.serialNumber, device.serial);
            }
        });
    });

    describe("feature reports (sync)", () => {
        let dev;
        before(() => {
            dev = new HID.HID(device.path);
        });
        after(() => {
            dev.close();
        });

        it("'v' version round-trip", () => {
            const sent = dev.sendFeatureReport(blink1.command("v"));
            assert.strictEqual(sent, blink1.REPORT_LENGTH + 1);

            const response = dev.getFeatureReport(blink1.REPORT_ID, blink1.REPORT_LENGTH + 1);
            assert.strictEqual(response.length, blink1.REPORT_LENGTH + 1);
            assert.strictEqual(response[0], blink1.REPORT_ID);

            firmwareVersion = blink1.parseVersionResponse(response);
            assert.ok(firmwareVersion > 100, `implausible firmware version ${firmwareVersion}`);
        });

        it("'c' fade-to-color echoes through getFeatureReport", async () => {
            dev.sendFeatureReport(blink1.fadeToColor(300, 0x11, 0x22, 0x33, 1));

            const response = dev.getFeatureReport(blink1.REPORT_ID, blink1.REPORT_LENGTH + 1);
            assert.strictEqual(response[1], "c".charCodeAt(0));
            assert.deepStrictEqual(response.slice(2, 5), [0x11, 0x22, 0x33]);

            if (device.fake) {
                const state = await device.fake.getState();
                assert.deepStrictEqual(state.rgb, { r: 0x11, g: 0x22, b: 0x33 });
                assert.strictEqual(state.fadeMillis, 300);
                assert.strictEqual(state.ledn, 1);
            }
        });

        it("accepts a Buffer as well as an array", async () => {
            const sent = dev.sendFeatureReport(Buffer.from(blink1.command("n", 0x44, 0x55, 0x66)));
            assert.strictEqual(sent, blink1.REPORT_LENGTH + 1);

            if (device.fake) {
                const state = await device.fake.getState();
                assert.deepStrictEqual(state.rgb, { r: 0x44, g: 0x55, b: 0x66 });
            }
        });

        it("report 2 round-trip (mk3 only)", (t) => {
            if (!(firmwareVersion >= 300)) {
                t.skip(`firmware ${firmwareVersion} has no report 2`);
                return;
            }

            dev.sendFeatureReport(blink1.command2("U"));
            const response = dev.getFeatureReport(blink1.REPORT2_ID, blink1.REPORT2_LENGTH + 1);
            assert.strictEqual(response.length, blink1.REPORT2_LENGTH + 1);
            assert.strictEqual(response[0], blink1.REPORT2_ID);
            assert.strictEqual(response[1], "U".charCodeAt(0));
        });

        it("getFeatureReport returns the device's reply length, not the requested length", (t) => {
            if (!device.fake) {
                t.skip("reply sizes are only predictable on the fake device");
                return;
            }

            dev.sendFeatureReport(blink1.command("v"));
            // Ask for far more than the 9-byte reply; the result must be 9 bytes
            const response = dev.getFeatureReport(blink1.REPORT_ID, blink1.REPORT2_LENGTH + 1);
            assert.strictEqual(response.length, blink1.REPORT_LENGTH + 1);
        });

        it("getFeatureReport with an unknown report id throws", () => {
            assert.throws(() => dev.getFeatureReport(0x42, 9), /could not get feature report/);
        });

        it("getFeatureReport with zero length throws", () => {
            assert.throws(() => dev.getFeatureReport(blink1.REPORT_ID, 0), /Length parameter/);
        });

        it("getFeatureReport with negative length throws (not aborts)", () => {
            assert.throws(() => dev.getFeatureReport(blink1.REPORT_ID, -1), /Length parameter/);
        });

        it("sendFeatureReport with bad payload types throws", () => {
            assert.throws(() => dev.sendFeatureReport("nope"), /unexpected data to send/);
            assert.throws(() => dev.sendFeatureReport([1, "x", {}]), /unexpected array element/);
        });
    });

    describe("feature reports (async)", () => {
        let dev;
        before(async () => {
            dev = await HID.HIDAsync.open(device.path);
        });
        after(async () => {
            await dev.close();
        });

        it("'v' version round-trip", async () => {
            const sent = await dev.sendFeatureReport(blink1.command("v"));
            assert.strictEqual(sent, blink1.REPORT_LENGTH + 1);

            const response = await dev.getFeatureReport(blink1.REPORT_ID, blink1.REPORT_LENGTH + 1);
            assert.ok(Buffer.isBuffer(response));
            assert.strictEqual(response.length, blink1.REPORT_LENGTH + 1);
            assert.strictEqual(blink1.parseVersionResponse(response), firmwareVersion);
        });

        it("queued operations complete in order", async () => {
            // Fire without awaiting; the per-device queue must serialize them
            const sends = [
                dev.sendFeatureReport(blink1.command("n", 1, 2, 3)),
                dev.sendFeatureReport(blink1.command("n", 4, 5, 6)),
                dev.sendFeatureReport(blink1.command("n", 7, 8, 9)),
            ];
            const results = await Promise.all(sends);
            assert.deepStrictEqual(results, [9, 9, 9]);

            if (device.fake) {
                const state = await device.fake.getState();
                assert.deepStrictEqual(state.rgb, { r: 7, g: 8, b: 9 });
            }
        });

        it("getFeatureReport with negative length rejects (not aborts)", async () => {
            await assert.rejects(dev.getFeatureReport(blink1.REPORT_ID, -1), /Length parameter/);
        });

        it("getFeatureReport with an unknown report id rejects", async () => {
            await assert.rejects(dev.getFeatureReport(0x42, 9), /could not get feature report/);
        });
    });

    describe("close semantics", () => {
        it("sync methods throw after close", () => {
            const dev = new HID.HID(device.path);
            dev.close();

            assert.throws(() => dev.getFeatureReport(1, 9), /closed device/);
            assert.throws(() => dev.sendFeatureReport(blink1.command("v")), /closed device/);
            assert.throws(() => dev.write([1, 0]), /closed device/);
            assert.throws(() => dev.readTimeout(10), /closed device/);
            assert.throws(() => dev.getDeviceInfo(), /closed device/);
            assert.throws(() => dev.setNonBlocking(1), /closed device/);
        });

        it("async methods reject after close, double close rejects", async () => {
            const dev = await HID.HIDAsync.open(device.path);
            await dev.close();

            await assert.rejects(dev.getFeatureReport(1, 9), /device has been closed/);
            await assert.rejects(dev.sendFeatureReport(blink1.command("v")), /device has been closed/);
            await assert.rejects(dev.write([1, 0]), /device has been closed/);
            await assert.rejects(dev.read(10), /device has been closed/);
            await assert.rejects(dev.getDeviceInfo(), /device has been closed/);
            await assert.rejects(dev.setNonBlocking(1), /device has been closed/);
            await assert.rejects(dev.close(), /device is already closed/);
        });

        it("operations queued before close still settle", async () => {
            const dev = await HID.HIDAsync.open(device.path);
            // FIFO drain: everything queued before close() completes first
            const pending = [
                dev.sendFeatureReport(blink1.command("n", 9, 9, 9)),
                dev.getFeatureReport(blink1.REPORT_ID, 9),
                dev.getDeviceInfo(),
            ];
            const closed = dev.close();
            const results = await Promise.all(pending);
            await closed;

            assert.strictEqual(results[0], 9);
            assert.strictEqual(results[1].length, 9);
            assert.strictEqual(results[2].serialNumber, device.serial);
        });
    });
});
