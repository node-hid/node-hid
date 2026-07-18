/**
 * Non-ASCII serial numbers must round-trip: kernel (UTF-8 uevent) -> hidapi
 * (wchar_t) -> the addon's wide/u16 conversions -> JS string, and back down
 * again when opening by serial. Includes an astral-plane character to exercise
 * the surrogate-pair paths. Fake-only: real blink(1) serials are ASCII.
 */
const { describe, it, before, after } = require("node:test");
const assert = require("node:assert");

const HID = require("../..");
const env = require("../helpers/env");
const blink1 = require("../helpers/blink1-protocol");
const { FakeBlink1 } = require("../helpers/fake-blink1");

// Keep this short: the uhid uniq field is truncated at 63 bytes of UTF-8
const UNICODE_SERIAL = `FÄKE-嗨-😀-${process.pid % 10000}`;
describe("unicode serial numbers", { skip: env.fakeOnlySkipReason }, () => {
  let fake;

  before(async () => {
    fake = await FakeBlink1.create({ serial: UNICODE_SERIAL });
  });
  after(async () => {
    await FakeBlink1.cleanupAll();
  });

  it("survives enumeration exactly", () => {
    const match = HID.devices(blink1.VENDOR_ID, blink1.PRODUCT_ID).find(
      (d) => d.serialNumber === UNICODE_SERIAL,
    );
    assert.ok(match, "device with unicode serial not found in enumeration");
    assert.strictEqual(match.serialNumber, UNICODE_SERIAL);
  });

  it("getDeviceInfo returns the exact serial (sync + async)", async () => {
    const sdev = new HID.HID(fake.hidrawPath);
    assert.strictEqual(sdev.getDeviceInfo().serialNumber, UNICODE_SERIAL);
    sdev.close();

    const adev = await HID.HIDAsync.open(fake.hidrawPath);
    assert.strictEqual(
      (await adev.getDeviceInfo()).serialNumber,
      UNICODE_SERIAL,
    );
    await adev.close();
  });

  it("opens by unicode serial (sync)", () => {
    const dev = new HID.HID(
      blink1.VENDOR_ID,
      blink1.PRODUCT_ID,
      UNICODE_SERIAL,
    );
    dev.close();
  });

  it("opens by unicode serial (async)", async () => {
    const dev = await HID.HIDAsync.open(
      blink1.VENDOR_ID,
      blink1.PRODUCT_ID,
      UNICODE_SERIAL,
    );
    await dev.close();
  });

  it("a lone-surrogate serial argument does not crash and does not match", () => {
    // JS strings may contain lone surrogates; the conversion replaces them,
    // so the open must fail cleanly rather than crash or false-match
    assert.throws(
      () => new HID.HID(blink1.VENDOR_ID, blink1.PRODUCT_ID, "FAKE-\uD800-x"),
      /cannot open device/,
    );
  });
});
