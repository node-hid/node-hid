/**
 * worker_threads teardown with a real open device: queued-but-unstarted jobs and
 * in-flight operations must not crash, abort, or leak the device on either
 * natural exit or terminate(). Fake-only.
 */
const { describe, it, before, after } = require("node:test");
const assert = require("node:assert");
const path = require("path");
const { Worker } = require("worker_threads");

const env = require("../helpers/env");
const { FakeBlink1 } = require("../helpers/fake-blink1");

const LIB_PATH = path.join(__dirname, "..", "..");

describe("worker_threads with an open device", { skip: env.fakeOnlySkipReason }, () => {
    let fake;

    before(async () => {
        fake = await FakeBlink1.create();
    });
    after(async () => {
        await FakeBlink1.cleanupAll();
    });

    function workerSource() {
        return `
            const { parentPort, workerData } = require("worker_threads");
            const HID = require(${JSON.stringify(LIB_PATH)});
            (async () => {
                const dev = await HID.HIDAsync.open(workerData.path);
                // Queue several jobs; deliberately do not await them
                dev.getDeviceInfo();
                dev.getDeviceInfo();
                dev.getDeviceInfo();
                HID.devicesAsync();
                parentPort.postMessage("queued");
                if (workerData.keepAlive) setInterval(() => {}, 1000);
            })().catch((e) => {
                parentPort.postMessage("error: " + e.message);
            });`;
    }

    it("natural exit with queued device jobs", async () => {
        for (let i = 0; i < 3; i++) {
            const worker = new Worker(workerSource(), {
                eval: true,
                workerData: { path: fake.hidrawPath, keepAlive: false },
            });
            const msg = await new Promise((resolve, reject) => {
                worker.on("message", resolve);
                worker.on("error", reject);
            });
            assert.strictEqual(msg, "queued");
            const exitCode = await new Promise((resolve) => worker.on("exit", resolve));
            assert.strictEqual(exitCode, 0);
        }
    });

    it("terminate() with queued device jobs", async () => {
        for (let i = 0; i < 3; i++) {
            const worker = new Worker(workerSource(), {
                eval: true,
                workerData: { path: fake.hidrawPath, keepAlive: true },
            });
            const msg = await new Promise((resolve, reject) => {
                worker.on("message", resolve);
                worker.on("error", reject);
            });
            assert.strictEqual(msg, "queued");
            await worker.terminate();
        }
    });

    it("device is still usable from the main thread afterwards", async () => {
        const HID = require("../..");
        const dev = await HID.HIDAsync.open(fake.hidrawPath);
        const info = await dev.getDeviceInfo();
        assert.strictEqual(info.serialNumber, fake.serial);
        await dev.close();
    });
});
