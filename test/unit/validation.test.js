const { describe, it } = require("node:test");
const assert = require("node:assert");

const HID = require("../..");

describe("input validation (no device required)", () => {
    it("HID() without new throws", () => {
        assert.throws(() => HID.HID(), /must be called with 'new'/);
    });

    it("HID constructor with no arguments throws", () => {
        assert.throws(() => new HID.HID(), /constructor requires at least one argument/);
    });

    it("HID constructor with a bad path throws", () => {
        assert.throws(() => new HID.HID("/dev/hidraw-does-not-exist"), /cannot open device with path/);
    });

    it("HID constructor with a non-string path throws", () => {
        assert.throws(() => new HID.HID(Symbol("nope")));
    });

    it("HID constructor with non-numeric vid/pid throws cleanly", () => {
        assert.throws(() => new HID.HID(null, undefined), /VendorId and ProductId must be integers/);
    });

    it("HID constructor with non-string serial throws cleanly", () => {
        assert.throws(() => new HID.HID(1, 2, 42), /Serial must be a string/);
    });

    it("devices() with wrong argument count throws", () => {
        assert.throws(() => HID.devices(123), /unexpected number of arguments/);
    });

    it("devices() with non-numeric arguments throws", () => {
        assert.throws(() => HID.devices(null, {}), /unexpected number of arguments/);
    });

    it("devicesAsync() with wrong arguments rejects", async () => {
        await assert.rejects(HID.devicesAsync(123), /unexpected number of arguments/);
    });

    it("HIDAsync cannot be constructed directly", () => {
        assert.throws(() => new HID.HIDAsync({}), /cannot be constructed directly/);
    });

    it("HIDAsync.open with a bad path rejects", async () => {
        await assert.rejects(HID.HIDAsync.open("/dev/hidraw-does-not-exist"), /cannot open device with path/);
    });

    it("HIDAsync.open with bad vid/pid rejects", async () => {
        await assert.rejects(HID.HIDAsync.open(0x0000, 0x0000), /cannot open device/);
    });

    it("failed constructions do not crash at GC", async (t) => {
        if (!global.gc) {
            t.skip("run with --expose-gc");
            return;
        }

        for (let i = 0; i < 50; i++) {
            try {
                new HID.HID();
            } catch (e) {
                // expected
            }
            try {
                new HID.HID("/dev/hidraw-does-not-exist");
            } catch (e) {
                // expected
            }
        }
        global.gc();
        await new Promise((resolve) => setImmediate(resolve));
        global.gc();
    });
});
