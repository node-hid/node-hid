/**
 * Input-report and read-path tests. Fake-only: a real blink(1) never emits
 * input reports, the fake does so on demand via uhid.
 *
 * Reports are numbered (id 1), so every read surfaces 9 bytes with buf[0] === 1.
 */
const { describe, it, afterEach, after } = require("node:test");
const assert = require("node:assert");

const HID = require("../..");
const env = require("../helpers/env");
const blink1 = require("../helpers/blink1-protocol");
const { FakeBlink1 } = require("../helpers/fake-blink1");

const PAYLOAD_A = [0xaa, 1, 2, 3, 4, 5, 6, 7];
const PAYLOAD_B = [0xbb, 9, 8, 7, 6, 5, 4, 3];

describe("read paths", { skip: env.fakeOnlySkipReason }, () => {
    let fake;
    let dev; // whichever device handle the current test opened

    // A fresh fake per test: input reports queue per open handle, and a fresh
    // device guarantees no cross-test leakage
    async function setup() {
        fake = await FakeBlink1.create();
        return fake;
    }
    afterEach(async () => {
        if (dev) {
            try {
                await dev.close?.();
            } catch (e) {
                /* already closed */
            }
            dev = null;
        }
        if (fake) {
            await fake.close();
            fake = null;
        }
    });
    after(async () => {
        await FakeBlink1.cleanupAll();
    });

    describe("sync HID", () => {
        it("read(callback) delivers an id-prefixed 9-byte buffer", async () => {
            await setup();
            dev = new HID.HID(fake.hidrawPath);

            const result = new Promise((resolve, reject) => {
                dev.read((err, data) => (err ? reject(err) : resolve(data)));
            });
            await fake.sendInput(PAYLOAD_A);

            const data = await result;
            assert.ok(Buffer.isBuffer(data));
            assert.deepStrictEqual([...data], [blink1.REPORT_ID, ...PAYLOAD_A]);
            dev.close();
        });

        it("readTimeout returns data when queued, empty when not", async () => {
            await setup();
            dev = new HID.HID(fake.hidrawPath);

            await fake.sendInput(PAYLOAD_A); // queued into the open handle's ring
            const data = dev.readTimeout(500);
            assert.deepStrictEqual(data, [blink1.REPORT_ID, ...PAYLOAD_A]);

            const empty = dev.readTimeout(50);
            assert.deepStrictEqual(empty, []);
            dev.close();
        });

        it("readSync returns already-queued data", async () => {
            await setup();
            dev = new HID.HID(fake.hidrawPath);

            // The input MUST be acked as sent before readSync: it blocks the event loop
            await fake.sendInput(PAYLOAD_B);
            const data = dev.readSync();
            assert.deepStrictEqual(data, [blink1.REPORT_ID, ...PAYLOAD_B]);
            dev.close();
        });

        it("setNonBlocking(1) makes readSync return empty immediately", async () => {
            await setup();
            dev = new HID.HID(fake.hidrawPath);

            dev.setNonBlocking(1);
            assert.deepStrictEqual(dev.readSync(), []);

            await fake.sendInput(PAYLOAD_A);
            // Non-blocking read still sees queued data (allow the kernel a moment)
            const deadline = Date.now() + 500;
            let data = [];
            while (data.length === 0 && Date.now() < deadline) {
                data = dev.readSync();
                if (data.length === 0) await new Promise((r) => setTimeout(r, 10));
            }
            assert.deepStrictEqual(data, [blink1.REPORT_ID, ...PAYLOAD_A]);
            dev.close();
        });

        it("close() while streaming 'data' events shuts down cleanly", async () => {
            await setup();
            dev = new HID.HID(fake.hidrawPath);

            const errors = [];
            const received = [];
            dev.on("error", (e) => errors.push(e));
            dev.on("data", (d) => received.push([...d]));

            await new Promise((r) => setTimeout(r, 50));
            await fake.sendInput(PAYLOAD_A);
            await waitFor(() => received.length >= 1);

            // Not paused: close() must interrupt the in-flight read and finish
            // closing from the read callback
            dev.close();
            await waitFor(() => {
                try {
                    dev.read(() => {});
                    return false;
                } catch (e) {
                    return /closed/.test(e.message);
                }
            });
            assert.deepStrictEqual(errors, []);
            dev = null;
        });

        it("'data' events flow via the polling read loop, pause() stops them", async () => {
            await setup();
            dev = new HID.HID(fake.hidrawPath);

            const received = [];
            dev.on("error", () => {}); // pause() interrupt surfaces as a read error; ignore
            dev.on("data", (data) => received.push([...data]));

            await new Promise((r) => setTimeout(r, 50)); // let resume() start the loop
            await fake.sendInput(PAYLOAD_A);
            await waitFor(() => received.length >= 1);
            assert.deepStrictEqual(received[0], [blink1.REPORT_ID, ...PAYLOAD_A]);

            dev.pause();
            await new Promise((r) => setTimeout(r, 100));
            dev.close();
        });
    });

    describe("async HIDAsync", () => {
        it("read(timeout) resolves with queued data, empty buffer on timeout", async () => {
            await setup();
            dev = await HID.HIDAsync.open(fake.hidrawPath);

            await fake.sendInput(PAYLOAD_A);
            const data = await dev.read(500);
            assert.ok(Buffer.isBuffer(data));
            assert.deepStrictEqual([...data], [blink1.REPORT_ID, ...PAYLOAD_A]);

            const empty = await dev.read(50);
            assert.strictEqual(empty.length, 0);
        });

        it("'data' events flow, pause() leaves the device fully usable, resume() restarts", async () => {
            await setup();
            dev = await HID.HIDAsync.open(fake.hidrawPath);

            const received = [];
            dev.on("error", (err) => {
                throw err;
            });
            dev.on("data", (data) => received.push([...data]));

            await new Promise((r) => setTimeout(r, 50)); // resume() is queued on nextTick
            await fake.sendInput(PAYLOAD_A);
            await waitFor(() => received.length >= 1);
            assert.deepStrictEqual(received[0], [blink1.REPORT_ID, ...PAYLOAD_A]);

            dev.pause();

            // The device must remain fully usable after pause() (regression: readStop
            // used to strand the device handle)
            const info = await dev.getDeviceInfo();
            assert.strictEqual(info.serialNumber, fake.serial);
            const sent = await dev.sendFeatureReport(blink1.command("n", 1, 2, 3));
            assert.strictEqual(sent, 9);

            dev.resume();
            await new Promise((r) => setTimeout(r, 50));
            await fake.sendInput(PAYLOAD_B);
            await waitFor(() => received.length >= 2);
            assert.deepStrictEqual(received[received.length - 1], [blink1.REPORT_ID, ...PAYLOAD_B]);
        });

        it("survives rapid pause/resume cycles", async () => {
            await setup();
            dev = await HID.HIDAsync.open(fake.hidrawPath);

            dev.on("data", () => {});
            await new Promise((r) => setTimeout(r, 50));

            for (let i = 0; i < 25; i++) {
                dev.pause();
                dev.resume();
            }
            await new Promise((r) => setTimeout(r, 250));

            // Still alive and usable
            const info = await dev.getDeviceInfo();
            assert.strictEqual(info.serialNumber, fake.serial);
        });

        it("read() is refused while the read thread runs, works again after listeners are removed", async () => {
            await setup();
            dev = await HID.HIDAsync.open(fake.hidrawPath);

            const listener = () => {};
            dev.on("data", listener);
            await new Promise((r) => setTimeout(r, 50)); // readStart running

            await assert.rejects(dev.read(50), /Cannot use read while async read is running/);

            // Removing the last 'data' listener auto-pauses (readStop); once the
            // read thread has wound down, one-shot read() must work again
            dev.removeListener("data", listener);
            await new Promise((r) => setTimeout(r, 200));

            const empty = await dev.read(50);
            assert.strictEqual(empty.length, 0);
        });

        it("close() after pause() works", async () => {
            await setup();
            dev = await HID.HIDAsync.open(fake.hidrawPath);

            dev.on("data", () => {});
            await new Promise((r) => setTimeout(r, 50));
            dev.pause();
            await dev.close();
            dev = null;
        });
    });

    describe("write path", () => {
        it("write() reaches the device as an output report (sync + async)", async () => {
            await setup();

            const outputs = [];
            fake.onOutput((data) => outputs.push([...data]));

            dev = new HID.HID(fake.hidrawPath);
            const wroteSync = dev.write([blink1.REPORT_ID, 1, 2, 3, 4, 5, 6, 7, 8]);
            assert.strictEqual(wroteSync, 9);
            dev.close();

            dev = await HID.HIDAsync.open(fake.hidrawPath);
            const wroteAsync = await dev.write(Buffer.from([blink1.REPORT_ID, 8, 7, 6, 5, 4, 3, 2, 1]));
            assert.strictEqual(wroteAsync, 9);

            await waitFor(() => outputs.length >= 2);
            assert.deepStrictEqual(outputs[0], [blink1.REPORT_ID, 1, 2, 3, 4, 5, 6, 7, 8]);
            assert.deepStrictEqual(outputs[1], [blink1.REPORT_ID, 8, 7, 6, 5, 4, 3, 2, 1]);
        });

        it("write with bad payload types throws", async () => {
            await setup();
            dev = new HID.HID(fake.hidrawPath);
            assert.throws(() => dev.write("nope"), /unexpected data to send/);
            assert.throws(() => dev.write([1, null]), /unexpected array element/);
            dev.close();
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
