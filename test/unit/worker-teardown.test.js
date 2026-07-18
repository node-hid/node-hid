const { describe, it } = require("node:test");
const assert = require("node:assert");
const path = require("path");
const { Worker } = require("worker_threads");

const LIB_PATH = path.join(__dirname, "..", "..");

/**
 * Spawn a worker running `body`, then terminate it as soon as it reports ready.
 * An abort in the addon kills the whole test process, which is exactly what
 * these tests exist to catch.
 */
async function runWorkerTerminated(body) {
    const src = `
        const { parentPort } = require("worker_threads");
        const HID = require(${JSON.stringify(LIB_PATH)});
        (async () => {
            ${body}
            parentPort.postMessage("ready");
            setInterval(() => {}, 1000); // keep alive until terminated
        })();`;

    const worker = new Worker(src, { eval: true });
    await new Promise((resolve, reject) => {
        worker.on("message", resolve);
        worker.on("error", reject);
    });
    await worker.terminate();
}

/** Natural exit variant: no keepalive, worker ends when its loop drains */
async function runWorkerNaturalExit(body) {
    const src = `
        const { parentPort } = require("worker_threads");
        const HID = require(${JSON.stringify(LIB_PATH)});
        (async () => {
            ${body}
            parentPort.postMessage("done");
        })();`;

    const worker = new Worker(src, { eval: true });
    const exitCode = await new Promise((resolve, reject) => {
        worker.on("error", reject);
        worker.on("exit", resolve);
    });
    assert.strictEqual(exitCode, 0);
}

describe("worker_threads teardown (no device required)", () => {
    it("load binding, then terminate", async () => {
        for (let i = 0; i < 3; i++) {
            await runWorkerTerminated(`HID.devices();`);
        }
    });

    it("awaited devicesAsync, then terminate", async () => {
        for (let i = 0; i < 3; i++) {
            await runWorkerTerminated(`await HID.devicesAsync();`);
        }
    });

    it("in-flight devicesAsync at terminate does not abort", async () => {
        for (let i = 0; i < 3; i++) {
            await runWorkerTerminated(`HID.devicesAsync(); HID.devicesAsync(); HID.devicesAsync();`);
        }
    });

    it("in-flight devicesAsync at natural exit does not abort", async () => {
        for (let i = 0; i < 3; i++) {
            await runWorkerNaturalExit(`HID.devicesAsync(); HID.devicesAsync(); HID.devicesAsync();`);
        }
    });
});
