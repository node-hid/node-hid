const { describe, it } = require("node:test");
const assert = require("node:assert");

const HID = require("../..");

function assertWellFormed(devices) {
    assert.ok(Array.isArray(devices));
    for (const dev of devices) {
        assert.strictEqual(typeof dev.vendorId, "number");
        assert.strictEqual(typeof dev.productId, "number");
        assert.strictEqual(typeof dev.release, "number");
        assert.strictEqual(typeof dev.interface, "number");
        if (dev.path !== undefined) assert.strictEqual(typeof dev.path, "string");
        if (dev.serialNumber !== undefined) assert.strictEqual(typeof dev.serialNumber, "string");
        if (dev.manufacturer !== undefined) assert.strictEqual(typeof dev.manufacturer, "string");
        if (dev.product !== undefined) assert.strictEqual(typeof dev.product, "string");
    }
}

describe("enumeration (no device required)", () => {
    it("devices() returns well-formed records", () => {
        assertWellFormed(HID.devices());
    });

    it("devicesAsync() returns well-formed records", async () => {
        assertWellFormed(await HID.devicesAsync());
    });

    it("sync and async enumeration agree", async () => {
        const sync = HID.devices();
        const async = await HID.devicesAsync();
        // Devices can come and go between the calls; compare loosely
        assert.ok(Math.abs(sync.length - async.length) <= 2);
    });

    it("vid/pid filtering returns a subset", async () => {
        const filtered = HID.devices(0x0001, 0x0001);
        assert.ok(Array.isArray(filtered));
        for (const dev of filtered) {
            assert.strictEqual(dev.vendorId, 0x0001);
            assert.strictEqual(dev.productId, 0x0001);
        }
        const filteredAsync = await HID.devicesAsync(0x0001, 0x0001);
        assert.ok(Array.isArray(filteredAsync));
    });

    it("concurrent devicesAsync calls all settle", async () => {
        const results = await Promise.all([HID.devicesAsync(), HID.devicesAsync(), HID.devicesAsync()]);
        for (const r of results) assert.ok(Array.isArray(r));
    });

    it("getHidapiVersion returns a version string", () => {
        assert.match(HID.getHidapiVersion(), /^\d+\.\d+\.\d+/);
    });
});
