/**
 * Test-process handle for a fake blink(1) hosted in a uhid-host.js child process.
 * See uhid-host.js for why the device must live in a separate process.
 */
const { fork } = require("child_process");
const fs = require("fs");
const path = require("path");

const HID = require("../..");
const blink1 = require("./blink1-protocol");

const registry = new Set();
let serialCounter = 0;

class FakeBlink1 {
    static async create({ serial, version = 204, name = "blink(1) mk2 fake" } = {}) {
        const fake = new FakeBlink1(serial ?? `FAKE-${process.pid}-${serialCounter++}`);
        registry.add(fake);
        try {
            const { hidrawPath } = await fake._rpc("create", { serial: fake.serial, version, name }, 10000);
            fake.hidrawPath = hidrawPath;
            await fake._waitForEnumeration();
            await fake._waitForNodeAccess();
        } catch (e) {
            await fake.close();
            throw e;
        }
        return fake;
    }

    static async cleanupAll() {
        for (const fake of [...registry]) {
            await fake.close();
        }
    }

    constructor(serial) {
        this.serial = serial;
        this.hidrawPath = null;
        this.outputCallbacks = [];
        this.pending = new Map();
        this.msgCounter = 0;

        this.child = fork(path.join(__dirname, "uhid-host.js"), [], { stdio: "inherit" });
        this.child.on("message", (msg) => {
            if (msg.event === "output") {
                for (const cb of this.outputCallbacks) cb(Buffer.from(msg.data), msg.rtype);
                return;
            }
            if (msg.event === "error") return; // surfaced through rpc timeouts

            const pending = this.pending.get(msg.id);
            if (!pending) return;
            this.pending.delete(msg.id);
            if (msg.ok) pending.resolve(msg.result);
            else pending.reject(new Error(`uhid-host ${pending.cmd} failed: ${msg.error}`));
        });
        this.child.on("exit", () => {
            for (const [, pending] of this.pending) {
                pending.reject(new Error(`uhid-host exited during ${pending.cmd}`));
            }
            this.pending.clear();
        });
    }

    _rpc(cmd, args = {}, timeoutMs = 2000) {
        const id = this.msgCounter++;
        return new Promise((resolve, reject) => {
            const timer = setTimeout(() => {
                this.pending.delete(id);
                reject(new Error(`uhid-host ${cmd} timed out`));
            }, timeoutMs);
            this.pending.set(id, {
                cmd,
                resolve: (v) => {
                    clearTimeout(timer);
                    resolve(v);
                },
                reject: (e) => {
                    clearTimeout(timer);
                    reject(e);
                },
            });
            this.child.send({ id, cmd, args });
        });
    }

    /** The device node exists immediately, but hidapi enumeration goes via udev - wait for it */
    async _waitForEnumeration(timeoutMs = 5000) {
        const deadline = Date.now() + timeoutMs;
        for (;;) {
            const match = HID.devices(blink1.VENDOR_ID, blink1.PRODUCT_ID).find((d) => d.serialNumber === this.serial);
            if (match) return match;
            if (Date.now() > deadline) throw new Error(`fake blink1 ${this.serial} did not appear in HID.devices()`);
            await new Promise((resolve) => setTimeout(resolve, 50));
        }
    }

    /**
     * Wait until this user can actually open the created node. Non-root users
     * need the udev rules from test/60-node-hid-test.rules, which grant an ACL
     * during udev processing - allow it a moment to land
     */
    async _waitForNodeAccess(timeoutMs = 1000) {
        const deadline = Date.now() + timeoutMs;
        for (;;) {
            try {
                fs.accessSync(this.hidrawPath, fs.constants.R_OK | fs.constants.W_OK);
                return;
            } catch (e) {
                if (Date.now() > deadline) {
                    throw new Error(
                        `created ${this.hidrawPath} but this user cannot open it - ` +
                            "install the udev rules from test/60-node-hid-test.rules, " +
                            'or run the suite as root: sudo env "PATH=$PATH" npm run test:device'
                    );
                }
                await new Promise((resolve) => setTimeout(resolve, 50));
            }
        }
    }

    /** Wait for the device to disappear from enumeration after destroy() */
    async waitForRemoval(timeoutMs = 5000) {
        const deadline = Date.now() + timeoutMs;
        for (;;) {
            const match = HID.devices(blink1.VENDOR_ID, blink1.PRODUCT_ID).find((d) => d.serialNumber === this.serial);
            if (!match) return;
            if (Date.now() > deadline) throw new Error(`fake blink1 ${this.serial} still enumerated after destroy`);
            await new Promise((resolve) => setTimeout(resolve, 50));
        }
    }

    /** Emit an input report. An 8-byte payload gets the report id prefixed automatically */
    async sendInput(bytes) {
        let data = [...bytes];
        if (data.length === blink1.REPORT_LENGTH) data = [blink1.REPORT_ID, ...data];
        await this._rpc("sendInput", { data });
    }

    async getState() {
        return this._rpc("getState");
    }

    onOutput(cb) {
        this.outputCallbacks.push(cb);
    }

    /** Simulate an unplug while the child stays alive for further assertions */
    async destroy() {
        await this._rpc("destroy");
    }

    async close() {
        registry.delete(this);
        try {
            await this._rpc("close", {}, 1000);
        } catch (e) {
            // child may already be dead
        }
        if (this.child.connected) this.child.disconnect();
        const child = this.child;
        setTimeout(() => {
            if (child.exitCode === null) child.kill("SIGKILL");
        }, 1000).unref();
    }
}

module.exports = { FakeBlink1 };
